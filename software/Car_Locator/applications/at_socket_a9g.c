
#include <stdio.h>
#include <string.h>

#include <rtthread.h>
#include <rtdevice.h>
#include <sys/socket.h>

#include <at.h>
#include <at_socket.h>

#if !defined(RT_USING_NETDEV)
#error "This RT-Thread version is older, please check and updata laster RT-Thread!"
#else
#include <arpa/inet.h>
#include <netdev.h>
#endif /* RT_USING_NETDEV */

#if !defined(AT_SW_VERSION_NUM) || AT_SW_VERSION_NUM < 0x10200
#error "This AT Client version is older, please check and update latest AT Client!"
#endif

#define LOG_TAG              "at.a9g"
#include <at_log.h>

#ifdef BSP_USING_A9G

#define A9G_NETDEV_NAME										"a9g"

#define A9G_MODULE_SEND_MAX_SIZE					1000
#define A9G_WAIT_CONNECT_TIME							5000
#define	A9G_THREAD_STACK_SIZE							1024
#define A9G_THREAD_PRIORITY								(RT_THREAD_PRIORITY_MAX/2)

/* set real event by current socket and current state */
#define SET_EVENT(socket, event)       (((socket + 1) << 16) | (event))

/* AT socket event type */
#define A9G_EVENT_CONN_OK              (1L << 0)
#define A9G_EVENT_SEND_OK              (1L << 1)
#define A9G_EVENT_RECV_OK              (1L << 2)
#define A9G_EVNET_CLOSE_OK             (1L << 3)
#define A9G_EVENT_CONN_FAIL            (1L << 4)
#define A9G_EVENT_SEND_FAIL            (1L << 5)

static int cur_socket;
static rt_event_t at_socket_event;
static rt_mutex_t at_event_lock;
static at_evt_cb_t at_evt_cb_set[] = {
        [AT_SOCKET_EVT_RECV] = NULL,
        [AT_SOCKET_EVT_CLOSED] = NULL,
};

static int at_socket_event_send(uint32_t event)
{
    return (int) rt_event_send(at_socket_event, event);
}

static int at_socket_event_recv(uint32_t event, uint32_t timeout, rt_uint8_t option)
{
    int result = 0;
    rt_uint32_t recved;

    result = rt_event_recv(at_socket_event, event, option | RT_EVENT_FLAG_CLEAR, timeout, &recved);
    if (result != RT_EOK)
    {
        return -RT_ETIMEOUT;
    }

    return recved;
}

/**
 * close socket by AT commands.
 *
 * @param current socket
 *
 * @return  0: close socket success
 *         -1: send AT commands error
 *         -2: wait socket event timeout
 *         -5: no memory
 */
static int a9g_socket_close(int socket)
{
    int result = 0;
    at_response_t resp = RT_NULL;

    resp = at_create_resp(128, 0, RT_TICK_PER_SECOND);
    if (!resp)
    {
        LOG_E("No memory for response structure!");
        return -RT_ENOMEM;
    }

    rt_mutex_take(at_event_lock, RT_WAITING_FOREVER);
    cur_socket = socket;

    /* Clear socket close event */
    at_socket_event_recv(SET_EVENT(socket, A9G_EVNET_CLOSE_OK), 0, RT_EVENT_FLAG_OR);

    if (at_exec_cmd(resp, "AT+CIPCLOSE=%d", socket) < 0)
    {
        result = -RT_ERROR;
        goto __exit;
    }

    if (at_socket_event_recv(SET_EVENT(socket, A9G_EVNET_CLOSE_OK), rt_tick_from_millisecond(300*3), RT_EVENT_FLAG_AND) < 0)
    {
        LOG_E("socket (%d) close failed, wait close OK timeout.", socket);
        result = -RT_ETIMEOUT;
        goto __exit;
    }

__exit:
    rt_mutex_release(at_event_lock);

    if (resp)
    {
        at_delete_resp(resp);
    }

    return result;
}

/**
 * create TCP/UDP client or server connect by AT commands.
 *
 * @param socket current socket
 * @param ip server or client IP address
 * @param port server or client port
 * @param type connect socket type(tcp, udp)
 * @param is_client connection is client
 *
 * @return   0: connect success
 *          -1: connect failed, send commands error or type error
 *          -2: wait socket event timeout
 *          -5: no memory
 */
static int a9g_socket_connect(int socket, char *ip, int32_t port, enum at_socket_type type, rt_bool_t is_client)
{
    int result = 0, event_result = 0;
    rt_bool_t retryed = RT_FALSE;
    at_response_t resp = RT_NULL;

    RT_ASSERT(ip);
    RT_ASSERT(port >= 0);

    resp = at_create_resp(128, 0, 5 * RT_TICK_PER_SECOND);
    if (!resp)
    {
        LOG_E("No memory for response structure!");
        return -RT_ENOMEM;
    }

    /* lock AT socket connect */
    rt_mutex_take(at_event_lock, RT_WAITING_FOREVER);

__retry:

    /* Clear socket connect event */
    at_socket_event_recv(SET_EVENT(socket, A9G_EVENT_CONN_OK | A9G_EVENT_CONN_FAIL), 0, RT_EVENT_FLAG_OR);

    if (is_client)
    {
        switch (type)
        {
        case AT_SOCKET_TCP:
            /* send AT commands(eg: AT+CIPSTART=0,"TCP","x.x.x.x", 1234) to connect TCP server */
            if (at_exec_cmd(resp, "AT+CIPSTART=%d,\"TCP\",\"%s\",%d", socket, ip, port) < 0)
            {
                result = -RT_ERROR;
                goto __exit;
            }
            break;

        case AT_SOCKET_UDP:
            if (at_exec_cmd(resp, "AT+CIPSTART=%d,\"UDP\",\"%s\",%d", socket, ip, port) < 0)
            {
                result = -RT_ERROR;
                goto __exit;
            }
            break;

        default:
            LOG_E("Not supported connect type : %d.", type);
            result = -RT_ERROR;
            goto __exit;
        }
    }

    /* waiting result event from AT URC, the device default connection timeout is 75 seconds, but it set to 10 seconds is convenient to use.*/
    if (at_socket_event_recv(SET_EVENT(socket, 0), 10 * RT_TICK_PER_SECOND, RT_EVENT_FLAG_OR) < 0)
    {
        LOG_E("socket (%d) connect failed, wait connect result timeout.", socket);
        result = -RT_ETIMEOUT;
        goto __exit;
    }
    /* waiting OK or failed result */
    if ((event_result = at_socket_event_recv(A9G_EVENT_CONN_OK | A9G_EVENT_CONN_FAIL, 1 * RT_TICK_PER_SECOND,
            RT_EVENT_FLAG_OR)) < 0)
    {
        LOG_E("socket (%d) connect failed, wait connect OK|FAIL timeout.", socket);
        result = -RT_ETIMEOUT;
        goto __exit;
    }
    /* check result */
    if (event_result & A9G_EVENT_CONN_FAIL)
    {
        if (!retryed)
        {
            LOG_E("socket (%d) connect failed, maybe the socket was not be closed at the last time and now will retry.", socket);
            if (a9g_socket_close(socket) < 0)
            {
                goto __exit;
            }
            retryed = RT_TRUE;
            goto __retry;
        }
        LOG_E("socket (%d) connect failed, failed to establish a connection.", socket);
        result = -RT_ERROR;
        goto __exit;
    }

__exit:
    /* unlock AT socket connect */
    rt_mutex_release(at_event_lock);

    if (resp)
    {
        at_delete_resp(resp);
    }

    return result;
}

/**
 * send data to server or client by AT commands.
 *
 * @param socket current socket
 * @param buff send buffer
 * @param bfsz send buffer size
 * @param type connect socket type(tcp, udp)
 *
 * @return >=0: the size of send success
 *          -1: send AT commands error or send data error
 *          -2: waited socket event timeout
 *          -5: no memory
 */
static int a9g_socket_send(int socket, const char *buff, size_t bfsz, enum at_socket_type type)
{
    int result = 0, event_result = 0;
    at_response_t resp = RT_NULL;
    size_t cur_pkt_size = 0, sent_size = 0;

    RT_ASSERT(buff);

    resp = at_create_resp(128, 2, 5 * RT_TICK_PER_SECOND);
    if (!resp)
    {
        LOG_E("No memory for response structure!");
        return -RT_ENOMEM;
    }

    rt_mutex_take(at_event_lock, RT_WAITING_FOREVER);

    /* Clear socket connect event */
    at_socket_event_recv(SET_EVENT(socket, A9G_EVENT_SEND_OK | A9G_EVENT_SEND_FAIL), 0, RT_EVENT_FLAG_OR);

    /* set current socket for send URC event */
    cur_socket = socket;
    /* set AT client end sign to deal with '>' sign.*/
    at_set_end_sign('>');

    while (sent_size < bfsz)
    {
        if (bfsz - sent_size < A9G_MODULE_SEND_MAX_SIZE)
        {
            cur_pkt_size = bfsz - sent_size;
        }
        else
        {
            cur_pkt_size = A9G_MODULE_SEND_MAX_SIZE;
        }

        /* send the "AT+CISEND" commands to AT server than receive the '>' response on the first line. */
        if (at_exec_cmd(resp, "AT+CIPSEND=%d,%d", socket, cur_pkt_size) < 0)
        {
            result = -RT_ERROR;
            goto __exit;
        }

        /* send the real data to server or client */
        result = (int) at_client_send(buff + sent_size, cur_pkt_size);
        if (result == 0)
        {
            result = -RT_ERROR;
            goto __exit;
        }

        /* waiting result event from AT URC */
        if (at_socket_event_recv(SET_EVENT(socket, 0), 10 * RT_TICK_PER_SECOND, RT_EVENT_FLAG_OR) < 0)
        {
            LOG_E("socket (%d) send failed, wait connect result timeout.", socket);
            result = -RT_ETIMEOUT;
            goto __exit;
        }
        /* waiting OK or failed result */
        if ((event_result = at_socket_event_recv(A9G_EVENT_SEND_OK | A9G_EVENT_SEND_FAIL, 5 * RT_TICK_PER_SECOND,
                RT_EVENT_FLAG_OR)) < 0)
        {
            LOG_E("socket (%d) send failed, wait connect OK|FAIL timeout.", socket);
            result = -RT_ETIMEOUT;
            goto __exit;
        }
        /* check result */
        if (event_result & A9G_EVENT_SEND_FAIL)
        {
            LOG_E("socket (%d) send failed, return failed.", socket);
            result = -RT_ERROR;
            goto __exit;
        }

        if (type == AT_SOCKET_TCP)
        {
            //cur_pkt_size = cur_send_bfsz;
        }

        sent_size += cur_pkt_size;
    }


__exit:
    /* reset the end sign for data conflict */
    at_set_end_sign(0);

    rt_mutex_release(at_event_lock);

    if (resp)
    {
        at_delete_resp(resp);
    }

    return result;
}

/**
 * domain resolve by AT commands.
 *
 * @param name domain name
 * @param ip parsed IP address, it's length must be 16
 *
 * @return  0: domain resolve success
 *         -1: send AT commands error or response error
 *         -2: wait socket event timeout
 *         -5: no memory
 */
static int a9g_domain_resolve(const char *name, char ip[16])
{
#define RESOLVE_RETRY                  5

    int i, result = RT_EOK;
    char recv_ip[16] = { 0 };
    at_response_t resp = RT_NULL;

    RT_ASSERT(name);
    RT_ASSERT(ip);

    /* The maximum response time is 14 seconds, affected by network status */
    resp = at_create_resp(128, 4, 14 * RT_TICK_PER_SECOND);
    if (!resp)
    {
        LOG_E("No memory for response structure!");
        return -RT_ENOMEM;
    }

    rt_mutex_take(at_event_lock, RT_WAITING_FOREVER);

    for (i = 0; i < RESOLVE_RETRY; i++)
    {
        int err_code = 0;

        if (at_exec_cmd(resp, "AT+CDNSGIP=\"%s\"", name) < 0)
        {
            result = -RT_ERROR;
            goto __exit;
        }

        /* domain name prase error options */
        if (at_resp_parse_line_args_by_kw(resp, "+CDNSGIP: 0", "+CDNSGIP: 0,%d", &err_code) > 0)
        {
            /* 3 - network error, 8 - dns common error */
            if (err_code == 3 || err_code == 8)
            {
                result = -RT_ERROR;
                goto __exit;
            }
        }

        /* parse the third line of response data, get the IP address */
        if (at_resp_parse_line_args_by_kw(resp, "+CDNSGIP:", "%*[^,],%*[^,],\"%[^\"]", recv_ip) < 0)
        {
            rt_thread_mdelay(100);
            /* resolve failed, maybe receive an URC CRLF */
            continue;
        }

        if (strlen(recv_ip) < 8)
        {
            rt_thread_mdelay(100);
            /* resolve failed, maybe receive an URC CRLF */
            continue;
        }
        else
        {
            strncpy(ip, recv_ip, 15);
            ip[15] = '\0';
            //LOG_I("DNS IP:%s",ip);
            break;
        }
    }

__exit:
    rt_mutex_release(at_event_lock);

    if (resp)
    {
        at_delete_resp(resp);
    }

    return result;

}

/**
 * set AT socket event notice callback
 *
 * @param event notice event
 * @param cb notice callback
 */
static void a9g_socket_set_event_cb(at_socket_evt_t event, at_evt_cb_t cb)
{
    if (event < sizeof(at_evt_cb_set) / sizeof(at_evt_cb_set[1]))
    {
        at_evt_cb_set[event] = cb;
    }
}

static void urc_connect_func(const char *data, rt_size_t size)
{
    int socket = 0;

    RT_ASSERT(data && size);

    sscanf(data, "%d,%*", &socket);

    if (strstr(data, "CONNECT OK"))
    {
        at_socket_event_send(SET_EVENT(socket, A9G_EVENT_CONN_OK));
    }
    else if (strstr(data, "CONNECT FAIL"))
    {
        at_socket_event_send(SET_EVENT(socket, A9G_EVENT_CONN_FAIL));
    }
}

static void urc_send_func(const char *data, rt_size_t size)
{
    RT_ASSERT(data && size);

    if (strstr(data, "SEND OK"))
    {
        at_socket_event_send(SET_EVENT(cur_socket, A9G_EVENT_SEND_OK));
    }
    else if (strstr(data, "SEND FAIL"))
    {
        at_socket_event_send(SET_EVENT(cur_socket, A9G_EVENT_SEND_FAIL));
    }
}

static void urc_close_func(const char *data, rt_size_t size)
{
    int socket = 0;

    RT_ASSERT(data && size);

    if (strstr(data, "CLOSE OK"))
    {
        at_socket_event_send(SET_EVENT(cur_socket, A9G_EVNET_CLOSE_OK));
    }
    else if (strstr(data, "CLOSED"))
    {
        sscanf(data, "%d, CLOSED", &socket);
        /* notice the socket is disconnect by remote */
        if (at_evt_cb_set[AT_SOCKET_EVT_CLOSED])
        {
            at_evt_cb_set[AT_SOCKET_EVT_CLOSED](socket, AT_SOCKET_EVT_CLOSED, NULL, 0);
        }
    }
}

static void urc_recv_func(const char *data, rt_size_t size)
{
    int socket = 0;
    rt_size_t bfsz = 0, temp_size = 0;
    rt_int32_t timeout;
    char *recv_buf = RT_NULL, temp[8];

    RT_ASSERT(data && size);

    /* get the current socket and receive buffer size by receive data */
    sscanf(data, "+RECEIVE,%d,%d:", &socket, (int *) &bfsz);
    /* get receive timeout by receive buffer length */
    timeout = bfsz;

    if (socket < 0 || bfsz == 0)
        return;

    recv_buf = rt_calloc(1, bfsz);
    if (!recv_buf)
    {
        LOG_E("no memory for URC receive buffer (%d)!", bfsz);
        /* read and clean the coming data */
        while (temp_size < bfsz)
        {
            if (bfsz - temp_size > sizeof(temp))
            {
                at_client_recv(temp, sizeof(temp), timeout);
            }
            else
            {
                at_client_recv(temp, bfsz - temp_size, timeout);
            }
            temp_size += sizeof(temp);
        }
        return;
    }

    /* sync receive data */
    if (at_client_recv(recv_buf, bfsz, timeout) != bfsz)
    {
        LOG_E("receive size(%d) data failed!", bfsz);
        rt_free(recv_buf);
        return;
    }

    /* notice the receive buffer and buffer size */
    if (at_evt_cb_set[AT_SOCKET_EVT_RECV])
    {
        at_evt_cb_set[AT_SOCKET_EVT_RECV](socket, AT_SOCKET_EVT_RECV, recv_buf, bfsz);
    }
}

static void urc_func(const char *data, rt_size_t size)
{
    RT_ASSERT(data);

    LOG_I("URC data : %.*s", size, data);
}

static const struct at_urc urc_table[] = {
        {"RDY",         "\r\n",                 urc_func},
        {"",            ", CONNECT OK\r\n",     urc_connect_func},
        {"",            ", CONNECT FAIL\r\n",   urc_connect_func},
        {"",            ", SEND OK\r\n",        urc_send_func},
        {"",            ", SEND FAIL\r\n",      urc_send_func},
        {"",            ", CLOSE OK\r\n",       urc_close_func},
        {"",            ", CLOSED\r\n",         urc_close_func},
        {"+RECEIVE,",   "\r\n",                 urc_recv_func},
};

#ifdef AT_USING_A9G_GPS
#include "gps.h"
#include "minmea.h"

//从buf里面得到第cx个逗号所在的位置
//返回值:0~0XFE,代表逗号所在位置的偏移.
//       0XFF,代表不存在第cx个逗号
rt_uint8_t NMEA_Comma_Pos(rt_uint8_t *buf,rt_uint8_t cx)
{
    rt_uint8_t *p=buf;
    while(cx)
    {
        if(*buf=='*'||*buf<' '||*buf>'z')return 0XFF;//遇到'*'或者非法字符,则不存在第cx个逗号
        if(*buf==',')cx--;
        buf++;
    }
    return buf-p;
}
//m^n函数
//返回值:m^n次方.
rt_uint32_t NMEA_Pow(rt_uint8_t m,rt_uint8_t n)
{
    rt_uint32_t result=1;
    while(n--)result*=m;
    return result;
}
//str转换为数字,以','或者'*'结束
//buf:数字存储区
//dx:小数点位数,返回给调用函数
//返回值:转换后的数值
int NMEA_Str2num(rt_uint8_t *buf,rt_uint8_t*dx)
{
    rt_uint8_t *p=buf;
    rt_uint32_t ires=0,fres=0;
    rt_uint8_t ilen=0,flen=0,i;
    rt_uint8_t mask=0;
    int res;
    while(1) //得到整数和小数的长度
    {
        if(*p=='-') {
            mask|=0X02;    //是负数
            p++;
        }
        if(*p==','||(*p=='*'))break;//遇到结束了
        if(*p=='.') {
            mask|=0X01;    //遇到小数点了
            p++;
        }
        else if(*p>'9'||(*p<'0'))	//有非法字符
        {
            ilen=0;
            flen=0;
            break;
        }
        if(mask&0X01)flen++;
        else ilen++;
        p++;
    }
    if(mask&0X02)buf++;	//去掉负号
    for(i=0; i<ilen; i++)	//得到整数部分数据
    {
        ires+=NMEA_Pow(10,ilen-1-i)*(buf[i]-'0');
    }
    if(flen>5)flen=5;	//最多取5位小数
    *dx=flen;	 		//小数点位数
    for(i=0; i<flen; i++)	//得到小数部分数据
    {
        fres+=NMEA_Pow(10,flen-1-i)*(buf[ilen+1+i]-'0');
    }
    res=ires*NMEA_Pow(10,flen)+fres;
    if(mask&0X02)res=-res;
    return res;
}
//分析GPGSV信息
//gpsx:nmea信息结构体
//buf:接收到的GPS数据缓冲区首地址
void NMEA_GPGSV_Analysis(nmea_msg *gpsx,rt_uint8_t *buf)
{
    rt_uint8_t *p,*p1,dx;
    rt_uint8_t len,i,j,slx=0;
    rt_uint8_t posx;
    p=buf;
    p1=(rt_uint8_t*)strstr((const char *)p,"$GPGSV");
    len=p1[7]-'0';								//得到GPGSV的条数
    posx=NMEA_Comma_Pos(p1,3); 					//得到可见卫星总数
    if(posx!=0XFF)gpsx->svnum=NMEA_Str2num(p1+posx,&dx);
    for(i=0; i<len; i++)
    {
        p1=(rt_uint8_t*)strstr((const char *)p,"$GPGSV");
        for(j=0; j<4; j++)
        {
            posx=NMEA_Comma_Pos(p1,4+j*4);
            if(posx!=0XFF)gpsx->slmsg[slx].num=NMEA_Str2num(p1+posx,&dx);	//得到卫星编号
            else break;
            posx=NMEA_Comma_Pos(p1,5+j*4);
            if(posx!=0XFF)gpsx->slmsg[slx].eledeg=NMEA_Str2num(p1+posx,&dx);//得到卫星仰角
            else break;
            posx=NMEA_Comma_Pos(p1,6+j*4);
            if(posx!=0XFF)gpsx->slmsg[slx].azideg=NMEA_Str2num(p1+posx,&dx);//得到卫星方位角
            else break;
            posx=NMEA_Comma_Pos(p1,7+j*4);
            if(posx!=0XFF)gpsx->slmsg[slx].sn=NMEA_Str2num(p1+posx,&dx);	//得到卫星信噪比
            else break;
            slx++;
        }
        p=p1+1;//切换到下一个GPGSV信息
    }
}
//分析GNGGA信息
//gpsx:nmea信息结构体
//buf:接收到的GPS数据缓冲区首地址
void NMEA_GNGGA_Analysis(nmea_msg *gpsx,rt_uint8_t *buf)
{
    rt_uint8_t *p1,dx;
    rt_uint8_t posx;
    p1=(rt_uint8_t*)strstr((const char *)buf,"$GNGGA");
		
    posx=NMEA_Comma_Pos(p1,6);								//得到GPS状态
    if(posx!=0XFF)gpsx->gpssta=NMEA_Str2num(p1+posx,&dx);
    posx=NMEA_Comma_Pos(p1,7);								//得到用于定位的卫星数
    if(posx!=0XFF)gpsx->posslnum=NMEA_Str2num(p1+posx,&dx);
    posx=NMEA_Comma_Pos(p1,9);								//得到海拔高度
    if(posx!=0XFF)gpsx->altitude=NMEA_Str2num(p1+posx,&dx);
}
//分析GPGSA信息
//gpsx:nmea信息结构体
//buf:接收到的GPS数据缓冲区首地址
void NMEA_GPGSA_Analysis(nmea_msg *gpsx,rt_uint8_t *buf)
{
    rt_uint8_t *p1,dx;
    rt_uint8_t posx;
    rt_uint8_t i;
    p1=(rt_uint8_t*)strstr((const char *)buf,"$GPGSA");
    posx=NMEA_Comma_Pos(p1,2);								//得到定位类型
    if(posx!=0XFF)gpsx->fixmode=NMEA_Str2num(p1+posx,&dx);
    for(i=0; i<12; i++)												//得到定位卫星编号
    {
        posx=NMEA_Comma_Pos(p1,3+i);
        if(posx!=0XFF)gpsx->possl[i]=NMEA_Str2num(p1+posx,&dx);
        else break;
    }
    posx=NMEA_Comma_Pos(p1,15);								//得到PDOP位置精度因子
    if(posx!=0XFF)gpsx->pdop=NMEA_Str2num(p1+posx,&dx);
    posx=NMEA_Comma_Pos(p1,16);								//得到HDOP位置精度因子
    if(posx!=0XFF)gpsx->hdop=NMEA_Str2num(p1+posx,&dx);
    posx=NMEA_Comma_Pos(p1,17);								//得到VDOP位置精度因子
    if(posx!=0XFF)gpsx->vdop=NMEA_Str2num(p1+posx,&dx);
}
//分析GNRMC信息
//gpsx:nmea信息结构体
//buf:接收到的GPS数据缓冲区首地址
void NMEA_GNRMC_Analysis(nmea_msg *gpsx,rt_uint8_t *buf)
{
    rt_uint8_t *p1,dx;
    rt_uint8_t posx;
    rt_uint32_t temp;
    float rs;
    p1=(rt_uint8_t*)strstr((const char *)buf,"GNRMC");//"$GNRMC",经常有&和GNRMC分开的情况,故只判断GNRMC.
    posx=NMEA_Comma_Pos(p1,1);								//得到UTC时间
    if(posx!=0XFF)
    {
        temp=NMEA_Str2num(p1+posx,&dx)/NMEA_Pow(10,dx);	 	//得到UTC时间,去掉ms
        gpsx->utc.hour=temp/10000;
        gpsx->utc.min=(temp/100)%100;
        gpsx->utc.sec=temp%100;
    }
    posx=NMEA_Comma_Pos(p1,3);								//得到纬度
    if(posx!=0XFF)
    {
        temp=NMEA_Str2num(p1+posx,&dx);
        gpsx->latitude=temp/NMEA_Pow(10,dx+2);	//得到°
        rs=temp%NMEA_Pow(10,dx+2);				//得到'
        gpsx->latitude=gpsx->latitude*NMEA_Pow(10,5)+(rs*NMEA_Pow(10,5-dx))/60;//转换为°
    }
    posx=NMEA_Comma_Pos(p1,4);								//南纬还是北纬
    if(posx!=0XFF)gpsx->nshemi=*(p1+posx);
    posx=NMEA_Comma_Pos(p1,5);								//得到经度
    if(posx!=0XFF)
    {
        temp=NMEA_Str2num(p1+posx,&dx);
        gpsx->longitude=temp/NMEA_Pow(10,dx+2);	//得到°
        rs=temp%NMEA_Pow(10,dx+2);				//得到'
        gpsx->longitude=gpsx->longitude*NMEA_Pow(10,5)+(rs*NMEA_Pow(10,5-dx))/60;//转换为°
    }
    posx=NMEA_Comma_Pos(p1,6);								//东经还是西经
    if(posx!=0XFF)gpsx->ewhemi=*(p1+posx);
    posx=NMEA_Comma_Pos(p1,9);								//得到UTC日期
    if(posx!=0XFF)
    {
        temp=NMEA_Str2num(p1+posx,&dx);		 				//得到UTC日期
        gpsx->utc.date=temp/10000;
        gpsx->utc.month=(temp/100)%100;
        gpsx->utc.year=2000+temp%100;
    }
}
//分析GNVTG信息
//gpsx:nmea信息结构体
//buf:接收到的GPS数据缓冲区首地址
void NMEA_GNVTG_Analysis(nmea_msg *gpsx,rt_uint8_t *buf)
{
    rt_uint8_t *p1,dx;
    rt_uint8_t posx;
    p1=(rt_uint8_t*)strstr((const char *)buf,"$GNVTG");
    posx=NMEA_Comma_Pos(p1,7);								//得到地面速率
    if(posx!=0XFF)
    {
        gpsx->speed=NMEA_Str2num(p1+posx,&dx);
        if(dx<3)gpsx->speed*=NMEA_Pow(10,3-dx);	 	 		//确保扩大1000倍
    }
}
//提取NMEA-0183信息
//gpsx:nmea信息结构体
//buf:接收到的GPS数据缓冲区首地址
void GPS_Analysis(nmea_msg *gpsx,rt_uint8_t *buf)
{
    NMEA_GPGSV_Analysis(gpsx,buf);	//GPGSV解析
    NMEA_GNGGA_Analysis(gpsx,buf);	//GNGGA解析
    NMEA_GPGSA_Analysis(gpsx,buf);	//GPGSA解析
    NMEA_GNRMC_Analysis(gpsx,buf);	//GPRMC解析
    NMEA_GNVTG_Analysis(gpsx,buf);	//GPVTG解析
}

/* 串口接收事件标志 */
#define UART_RX_EVENT (1 << 0)

/* 事件控制块 */
static struct rt_event event;
/* 串口设备句柄 */
static rt_device_t uart_device = RT_NULL;

/* 回调函数 */
static rt_err_t uart_intput(rt_device_t dev, rt_size_t size)
{
    /* 发送事件 */
    rt_event_send(&event, UART_RX_EVENT);
    
    return RT_EOK;
}

static rt_uint8_t uart_getchar(void)
{
    rt_uint32_t e;
    rt_uint8_t ch;
    
    /* 读取1字节数据 */
    while (rt_device_read(uart_device, 0, &ch, 1) != 1)
    {
         /* 接收事件 */
        rt_event_recv(&event, UART_RX_EVENT,RT_EVENT_FLAG_AND | RT_EVENT_FLAG_CLEAR,RT_WAITING_FOREVER, &e);
    }

    return ch;
}

static void uart_putchar(const rt_uint8_t c)
{
    rt_size_t len = 0;
    rt_uint32_t timeout = 0;
    do
    {
        len = rt_device_write(uart_device, 0, &c, 1);
        timeout++;
    }
    while (len != 1 && timeout < 500);
}

static void uart_putstring(const rt_uint8_t *s)
{
    while(*s)
    {
        uart_putchar(*s++);
    }
}

static rt_err_t uart_open(rt_device_t device)
{
    rt_err_t res;

    if (device != RT_NULL)
    {      
        res = rt_device_set_rx_indicate(device, uart_intput);
        /* 检查返回值 */
        if (res != RT_EOK)
        {
            rt_kprintf("set %s rx indicate error.%d\n",device->parent.name,res);
            return -RT_ERROR;
        }

        /* 打开设备，以可读写、中断方式 */
        res = rt_device_open(device, RT_DEVICE_OFLAG_RDWR | RT_DEVICE_FLAG_INT_RX );       
        /* 检查返回值 */
        if (res != RT_EOK)
        {
            rt_kprintf("open %s device error.%d\n",device->parent.name,res);
            return -RT_ERROR;
        }
        
        /* 初始化事件对象 */
        rt_event_init(&event, "event", RT_IPC_FLAG_FIFO); 
        
        return RT_EOK;
    }
    else
    {
        rt_kprintf("can't open %s device.\n",device->parent.name);
        return -RT_ERROR;
    }
}

static void data_read(uint8_t* buff, int len)
{
    rt_uint8_t uart_rx_byte;
    rt_uint8_t count = 0;
    
    do
		{
        uart_rx_byte = uart_getchar();
        buff[count] = uart_rx_byte;
        count ++;       
    }while((uart_rx_byte != '\n') && (count <= len));
    
    buff[count] = 0;
}

nmea_msg gpsx;
void gps_thread_entry(void *parameter)
{
    uint8_t line[MINMEA_MAX_LENGTH];

    while(1)
    {
        data_read(line, MINMEA_MAX_LENGTH);
			
				rt_kprintf("%s", line);
				
				GPS_Analysis(&gpsx, line);
				
//				rt_kprintf("gps latitude: %d %1c\n", gpsx.latitude /= 100000, gpsx.nshemi);
//				rt_kprintf("gps longitude: %d %1c\n", gpsx.longitude /= 100000, gpsx.ewhemi);
    }
}

rt_err_t gps_init(void)
{ 
    rt_thread_t tid;
    
    /* 查找系统中的串口设备 */
    uart_device = rt_device_find(GPS_UART_NAME);
    
    if (RT_NULL == uart_device)
    {
        rt_kprintf(" find device %s failed!\n",GPS_UART_NAME);  
        return RT_EINVAL;
    }
    struct serial_configure gps_use_config = 
    {
        BAUD_RATE_9600,   /* 9600 bits/s */
        DATA_BITS_8,      /* 8 databits */
        STOP_BITS_1,      /* 1 stopbit */
        PARITY_NONE,      /* No parity  */ 
        BIT_ORDER_LSB,    /* LSB first sent */
        NRZ_NORMAL,       /* Normal mode */
        1024,             /* Buffer size */
        0   
    };

    if (RT_EOK != rt_device_control(uart_device, RT_DEVICE_CTRL_CONFIG,(void *)&gps_use_config))
    {
        rt_kprintf("uart config failed.\n");
        return RT_ERROR;
    }

    if (uart_open(uart_device) != RT_EOK)
    {
        rt_kprintf("uart open error.\n");
        return RT_ERROR;
    }
     if (RT_EOK != rt_device_control(uart_device, RT_DEVICE_CTRL_CONFIG,(void *)&gps_use_config))
    {
        rt_kprintf("uart config failed.\n");
        return RT_ERROR;
    }

    tid = rt_thread_create("gps", 
                            gps_thread_entry,
                            RT_NULL,
                            2048,
                            8,
                            200);
    if (tid == RT_NULL)
    {
        return RT_ERROR;
    }

    rt_thread_startup(tid);

    return RT_EOK;
}
MSH_CMD_EXPORT(gps_init,APP INIT);


#endif

#define AT_SEND_CMD(resp, resp_line, timeout, cmd)                                                              \
    do                                                                                                          \
    {                                                                                                           \
        if (at_exec_cmd(at_resp_set_info(resp, 128, resp_line, rt_tick_from_millisecond(timeout)), cmd) < 0)    \
        {                                                                                                       \
            result = -RT_ERROR;                                                                                 \
            goto __exit;                                                                                        \
        }                                                                                                       \
    } while(0);                                                                                                 \

static int a9g_netdev_set_info(struct netdev *netdev);
static int a9g_netdev_check_link_status(struct netdev *netdev); 

/* reset the a9g device */
static int a9g_reset(void)
{
		rt_err_t result = RT_EOK;
		at_response_t resp = RT_NULL;
		
		at_exec_cmd(at_resp_set_info(resp, 128, 0, rt_tick_from_millisecond(300)), "AT+RST=1");
		
		return result;
}

/* init for a9g */
static void a9g_init_thread_entry(void *parameter)
{
#define AT_RETRY											 10
#define CPIN_RETRY                     10
#define CSQ_RETRY                      10
#define CREG_RETRY                     10

    at_response_t resp = RT_NULL;
    int i, qimux;
    char parsed_data[10];
    rt_err_t result = RT_EOK;

    rt_mutex_take(at_event_lock, RT_WAITING_FOREVER);

    while (1)
    {
        result = RT_EOK;
        rt_memset(parsed_data, 0, sizeof(parsed_data));
        rt_thread_mdelay(500);
        rt_thread_mdelay(2000);
        resp = at_create_resp(128, 0, rt_tick_from_millisecond(300));
        if (!resp)
        {
            LOG_E("No memory for response structure!");
            result = -RT_ENOMEM;
            goto __exit;
        }
        LOG_D("Start initializing the A9G module");
        /* wait A9G startup finish */
        if (at_client_wait_connect(A9G_WAIT_CONNECT_TIME))
        {
            result = -RT_ETIMEOUT;
            goto __exit;
        }
				
				for(i = 0; i < AT_RETRY; i++)
				{
						AT_SEND_CMD(resp, 0 , 300, "AT")
						if(at_resp_get_line_by_kw(resp, "OK"))
						{
								LOG_D("A9G automatically initialized successfully");
								break;
						}
						rt_thread_mdelay(1000);
				}
				
        /* disable echo */
        AT_SEND_CMD(resp, 0, 300, "ATE0");
        /* get module version */
        AT_SEND_CMD(resp, 0, 300, "ATI");
        /* show module version */
        for (i = 0; i < (int)resp->line_counts - 1; i++)
        {
            LOG_D("%s", at_resp_get_line(resp, i + 1));
        }
        /* check SIM card */
        for (i = 0; i < CPIN_RETRY; i++)
        {
						/* 查看SIM卡的状态 */
            at_exec_cmd(at_resp_set_info(resp, 128, 2, 5 * RT_TICK_PER_SECOND), "AT+CCID");

            if (at_resp_get_line_by_kw(resp, "+CCID"))
            {
                LOG_D("SIM card detection success");
                break;
            }
            rt_thread_mdelay(1000);
        }
        if (i == CPIN_RETRY)
        {
            LOG_E("SIM card detection failed!");
            result = -RT_ERROR;
            goto __exit;
        }
        /* waiting for dirty data to be digested */
        rt_thread_mdelay(10);

				AT_SEND_CMD(resp, 0, 300, "AT+CREG=1");
				
        /* check the GSM network is registered */
        for (i = 0; i < CREG_RETRY; i++)
        {
            AT_SEND_CMD(resp, 0, 300, "AT+CREG=?");
            at_resp_parse_line_args_by_kw(resp, "+CREG:", "+CREG: %s", &parsed_data);
						rt_kprintf("%s\n",resp);
            if (!strncmp(parsed_data, "(0,1,2)", sizeof(parsed_data)) || !strncmp(parsed_data, "(0,5,2)", sizeof(parsed_data)))
            {
                LOG_D("GSM network is registered (%s)", parsed_data);
                break;
            }
            rt_thread_mdelay(1000);
        }
        if (i == CREG_RETRY)
        {
            LOG_E("The GSM network is register failed (%s)", parsed_data);
            result = -RT_ERROR;
            goto __exit;
        }

        /* check signal strength */
        for (i = 0; i < CSQ_RETRY; i++)
        {
            AT_SEND_CMD(resp, 0, 300, "AT+CSQ");
            at_resp_parse_line_args_by_kw(resp, "+CSQ:", "+CSQ: %s", &parsed_data);
            if (strncmp(parsed_data, ",99", sizeof(parsed_data)))
            {
                LOG_D("Signal strength: %s", parsed_data);
                break;
            }
            rt_thread_mdelay(1000);
        }
        if (i == CSQ_RETRY)
        {
            LOG_E("Signal strength check failed (%s)", parsed_data);
            result = -RT_ERROR;
            goto __exit;
        }
				
				/* Set GPRS attachment */
				AT_SEND_CMD(resp, 2, 300, "AT+CGATT=1");
				rt_thread_mdelay(10);
				
				/* Define the PDP context */
				AT_SEND_CMD(resp, 2, 300, "AT+CGDCONT=1,\"IP\",\"CMNET\"");
				rt_thread_mdelay(10);
				
				/* Set PDP context activation */
				AT_SEND_CMD(resp, 2, 300, "AT+CGACT=1,1");
				rt_thread_mdelay(10);
				
        /* Set to multiple connections */
        AT_SEND_CMD(resp, 0, 300, "AT+CIPMUX?");
        at_resp_parse_line_args_by_kw(resp, "+CIPMUX:", "+CIPMUX: %d", &qimux);
        if (qimux == 0)
        {
            AT_SEND_CMD(resp, 0, 300, "AT+CIPMUX=1");
        }
				
				/* get the local address */
        AT_SEND_CMD(resp, 2, 300, "AT+CIFSR");
        if(at_resp_get_line_by_kw(resp, "ERROR") != RT_NULL)
        {
            LOG_E("Get the local address failed");
            result = -RT_ERROR;
            goto __exit;
        }
				
#ifdef	AT_USING_A9G_GPS
				
				AT_SEND_CMD(resp, 0, 300, "AT+GPS?");
				at_resp_parse_line_args_by_kw(resp, "+GPS:", "+GPS: %d", &qimux);
        if (qimux == 0)
        {
            AT_SEND_CMD(resp, 0, 300, "AT+GPS=1");
        }
				
#endif

    __exit:
        if (resp)
        {
            at_delete_resp(resp);
        }

        if (!result)
        {
            LOG_I("AT network initialize success!");
            rt_mutex_release(at_event_lock);
            break;
        }
        else
        {
            LOG_E("AT network initialize failed (%d)!", result);
						a9g_reset();
        }
        
        rt_thread_mdelay(1000);
    }

    /* set network interface device status and address information */
    a9g_netdev_set_info(netdev_get_by_name(A9G_NETDEV_NAME));
//    a9g_netdev_check_link_status(netdev_get_by_name(A9G_NETDEV_NAME));
}

/* init for A9G */
int a9g_net_init(void)
{
#ifdef AT_DEVICE_A9G_INIT_ASYN
    rt_thread_t tid;

    tid = rt_thread_create("a9g_net_init", a9g_init_thread_entry, RT_NULL, A9G_THREAD_STACK_SIZE, A9G_THREAD_PRIORITY, 20);
    if (tid)
    {
        rt_thread_startup(tid);
    }
    else
    {
        LOG_E("Create AT initialization thread fail!");
    }
#ifdef AT_USING_A9G_GPS
		
//		gps_init(RT_NULL);
		
#endif
		
#else
		
    a9g_init_thread_entry(RT_NULL);
		
#ifdef AT_USING_A9G_GPS
		
		gps_init(RT_NULL);
		
#endif
		
#endif

    return RT_EOK;
}

#ifdef FINSH_USING_MSH
#include <finsh.h>
MSH_CMD_EXPORT_ALIAS(a9g_net_init, at_net_init, initialize AT network);
#endif

static const struct at_device_ops a9g_socket_ops = {
    a9g_socket_connect,
    a9g_socket_close,
    a9g_socket_send,
    a9g_domain_resolve,
    a9g_socket_set_event_cb,
};


/* set a9g network interface device status and address information */
static int a9g_netdev_set_info(struct netdev *netdev)
{
#define A9G_IEMI_RESP_SIZE      32
#define A9G_IPADDR_RESP_SIZE    32
#define A9G_DNS_RESP_SIZE       96
#define A9G_INFO_RESP_TIMO      rt_tick_from_millisecond(300)

    int result = RT_EOK;
    at_response_t resp = RT_NULL;
    ip_addr_t addr;

    if (netdev == RT_NULL)
    {
        LOG_E("Input network interface device is NULL.\n");
        return -RT_ERROR;
    }

    rt_mutex_take(at_event_lock, RT_WAITING_FOREVER);

    /* set network interface device status */
    netdev_low_level_set_status(netdev, RT_TRUE);
    netdev_low_level_set_link_status(netdev, RT_TRUE);
    netdev_low_level_set_dhcp_status(netdev, RT_TRUE);

    resp = at_create_resp(A9G_IEMI_RESP_SIZE, 0, A9G_INFO_RESP_TIMO);
    if (resp == RT_NULL)
    {
        LOG_E("A9G set IP address failed, no memory for response object.");
        result = -RT_ENOMEM;
        goto __exit;
    }

    /* set network interface device hardware address(IEMI) */
    {
        #define A9G_NETDEV_HWADDR_LEN   8
        #define A9G_IEMI_LEN            15

        char iemi[A9G_IEMI_LEN] = {0};
        int i = 0, j = 0;

        /* send "AT+CGSN" commond to get device IEMI */
        if (at_exec_cmd(resp, "AT+CGSN") < 0)
        {
            result = -RT_ERROR;
            goto __exit;
        }

        if (at_resp_parse_line_args(resp, 2, "%s", iemi) <= 0)
        {
            LOG_E("Prase \"AT+CGSN\" commands resposne data error!");
            result = -RT_ERROR;
            goto __exit;
        }

        LOG_D("A9G IEMI number: %s", iemi);

        netdev->hwaddr_len = A9G_NETDEV_HWADDR_LEN;
        /* get hardware address by IEMI */
        for (i = 0, j = 0; i < A9G_NETDEV_HWADDR_LEN && j < A9G_IEMI_LEN; i++, j+=2)
        {
            if (j != A9G_IEMI_LEN - 1)
            {
                netdev->hwaddr[i] = (iemi[j] - '0') * 10 + (iemi[j + 1] - '0');
            }
            else
            {
                netdev->hwaddr[i] = (iemi[j] - '0');
            }
        }
    }

    /* set network interface device IP address */
    {
        #define IP_ADDR_SIZE_MAX    16
        char ipaddr[IP_ADDR_SIZE_MAX] = {0};
        
        at_resp_set_info(resp, A9G_IPADDR_RESP_SIZE, 2, A9G_INFO_RESP_TIMO);

        /* send "AT+CIFSR" commond to get IP address */
        if (at_exec_cmd(resp, "AT+CIFSR") < 0)
        {
            result = -RT_ERROR;
            goto __exit;
        }

        if (at_resp_parse_line_args_by_kw(resp, ".", "%s", ipaddr) <= 0)
        {
            LOG_E("Prase \"AT+CIFSR\" commands resposne data error!");
            result = -RT_ERROR;
            goto __exit;
        }
        
        LOG_D("A9G IP address: %s", ipaddr);

        /* set network interface address information */
        inet_aton(ipaddr, &addr);
        netdev_low_level_set_ipaddr(netdev, &addr);
    }

__exit:
    if (resp)
    {
        at_delete_resp(resp);
    }
    
    rt_mutex_release(at_event_lock);
    
    return result;
}

static void check_link_status_entry(void *parameter)
{
#define A9G_LINK_STATUS_OK   1
#define A9G_LINK_RESP_SIZE   128
#define A9G_LINK_RESP_TIMO   (3 * RT_TICK_PER_SECOND)
#define A9G_LINK_DELAY_TIME  (30 * RT_TICK_PER_SECOND)

    struct netdev *netdev = (struct netdev *)parameter;
    at_response_t resp = RT_NULL;
    int result_code, link_status;

    resp = at_create_resp(A9G_LINK_RESP_SIZE, 0, A9G_LINK_RESP_TIMO);
    if (resp == RT_NULL)
    {
        LOG_E("a9g set check link status failed, no memory for response object.");
        return;
    }

    while (1)
    {
        rt_mutex_take(at_event_lock, RT_WAITING_FOREVER);

        /* send "AT+CGREG?" commond  to check netweork interface device link status */
        if (at_exec_cmd(resp, "AT+CGREG?") < 0)
        {
            rt_mutex_release(at_event_lock);
            rt_thread_mdelay(A9G_LINK_DELAY_TIME);

            continue;
        }
        
        link_status = -1;
        at_resp_parse_line_args_by_kw(resp, "+CGREG:", "+CGREG: %d,%d", &result_code, &link_status);

        /* check the network interface device link status  */
        if ((A9G_LINK_STATUS_OK == link_status) != netdev_is_link_up(netdev))
        {
            netdev_low_level_set_link_status(netdev, (A9G_LINK_STATUS_OK == link_status));
        }

        rt_mutex_release(at_event_lock);
        rt_thread_mdelay(A9G_LINK_DELAY_TIME);
    }
}

/**
 * The function is checking the a9g net link status.
 * @NOTE
 * 
 * @param netdev the network interface device to change
 * 
 * @retrun 0: cheging success
 *        -1: the net device is NULL
*/
static int a9g_netdev_check_link_status(struct netdev *netdev)
{
#define A9G_LINK_THREAD_STACK_SIZE     512
#define A9G_LINK_THREAD_PRIORITY       (RT_THREAD_PRIORITY_MAX - 2)
#define A9G_LINK_THREAD_TICK           20

    rt_thread_t tid;
    char tname[RT_NAME_MAX];

    if (netdev == RT_NULL)
    {
        LOG_E("Input network interface device is NULL.\n");
        return -RT_ERROR;
    }

    rt_memset(tname, 0x00, sizeof(tname));
    rt_snprintf(tname, RT_NAME_MAX, "%s_link", netdev->name);

    tid = rt_thread_create(tname, check_link_status_entry, (void *)netdev, 
            A9G_LINK_THREAD_STACK_SIZE, A9G_LINK_THREAD_PRIORITY, A9G_LINK_THREAD_TICK);
    if (tid)
    {
        rt_thread_startup(tid);
    }

    return RT_EOK;
}

static int a9g_netdev_set_up(struct netdev *netdev)
{
    netdev_low_level_set_status(netdev, RT_TRUE);
    LOG_D("The network interface device(%s) set up status", netdev->name);

    return RT_EOK;
}

static int a9g_netdev_set_down(struct netdev *netdev)
{
    netdev_low_level_set_status(netdev, RT_FALSE);
    LOG_D("The network interface device(%s) set down status", netdev->name);
    return RT_EOK;
}

static int a9g_netdev_ping(struct netdev *netdev, const char *host, size_t data_len, uint32_t timeout, struct netdev_ping_resp *ping_resp)
{
#define A9G_PING_RESP_SIZE         128
#define A9G_PING_IP_SIZE           16
#define A9G_PING_TIMEO             (5 * RT_TICK_PER_SECOND)

#define A9G_PING_ERR_TIME          600
#define A9G_PING_ERR_TTL           255

    int result = RT_EOK;
    at_response_t resp = RT_NULL;
    char ip_addr[A9G_PING_IP_SIZE] = {0};
    int response, time, ttl, i;

    RT_ASSERT(netdev);
    RT_ASSERT(host);
    RT_ASSERT(ping_resp);

    for (i = 0; i < rt_strlen(host) && !isalpha(host[i]); i++);

    if (i < strlen(host))
    {
        /* check domain name is usable */
        if (a9g_domain_resolve(host, ip_addr) < 0)
        {
            return -RT_ERROR;
        }
        rt_memset(ip_addr, 0x00, A9G_PING_IP_SIZE);
    }

    rt_mutex_take(at_event_lock, RT_WAITING_FOREVER);

    resp = at_create_resp(A9G_PING_RESP_SIZE, 0, A9G_PING_TIMEO);
    if (resp == RT_NULL)
    {
        LOG_D("a9g set dns server failed, no memory for response object.");
        result = -RT_ERROR;
        goto __exit;
    }

    /* send "AT+CIPPING=<IP addr>[,<retryNum>[,<dataLen>[,<timeout>[,<ttl>]]]]" commond to send ping request */
    if (at_exec_cmd(resp, "AT+CIPPING=%s,1,%d,%d,64", host, data_len, A9G_PING_TIMEO / (RT_TICK_PER_SECOND / 10)) < 0)
    {
        result = -RT_ERROR;
        goto __exit;
    }

    if (at_resp_parse_line_args_by_kw(resp, "+CIPPING:", "+CIPPING:%d,\"%[^\"]\",%d,%d",
             &response, ip_addr, &time, &ttl) <= 0)
    {
        LOG_D("Prase \"AT+CIPPING\" commands resposne data error!");
        result = -RT_ERROR;
        goto __exit;
    }

    /* the ping request timeout expires, the response time settting to 600 and ttl setting to 255 */
    if (time == A9G_PING_ERR_TIME && ttl == A9G_PING_ERR_TTL)
    {
        result = -RT_ETIMEOUT;
        goto __exit;
    }

    inet_aton(ip_addr, &(ping_resp->ip_addr));
    ping_resp->data_len = data_len;
    /* reply time, in units of 100 ms */
    ping_resp->ticks = time * 100;
    ping_resp->ttl = ttl;

 __exit:
    if (resp)
    {
        at_delete_resp(resp);
    }
    
     rt_mutex_release(at_event_lock);

    return result;
}

void a9g_netdev_netstat(struct netdev *netdev)
{ 
    // TODO netstat support
}

const struct netdev_ops a9g_netdev_ops =
{
    a9g_netdev_set_up,
    a9g_netdev_set_down,

    RT_NULL, /* not support set ip, netmask, gatway address */
    RT_NULL, /* not support set dns server */
    RT_NULL, /* not support set DHCP status */

    a9g_netdev_ping,
    a9g_netdev_netstat,
};

static int a9g_netdev_add(const char *netdev_name)
{
#define A9G_NETDEV_MTU       1500
    struct netdev *netdev = RT_NULL;

    RT_ASSERT(netdev_name);

    netdev = (struct netdev *) rt_calloc(1, sizeof(struct netdev));
    if (netdev == RT_NULL)
    {
        return -RT_ENOMEM;
    }

    netdev->mtu = A9G_NETDEV_MTU;
    netdev->ops = &a9g_netdev_ops;

#ifdef SAL_USING_AT
    extern int sal_at_netdev_set_pf_info(struct netdev *netdev);
    /* set the network interface socket/netdb operations */
    sal_at_netdev_set_pf_info(netdev);
#endif

    return netdev_register(netdev, netdev_name, RT_NULL);
}

static int at_socket_device_init(void)
{
    /* create current AT socket event */
    at_socket_event = rt_event_create("at_se", RT_IPC_FLAG_FIFO);
    if (at_socket_event == RT_NULL)
    {
        LOG_E("AT client port initialize failed! at_sock_event create failed!");
        return -RT_ENOMEM;
    }

    /* create current AT socket event lock */
    at_event_lock = rt_mutex_create("at_se", RT_IPC_FLAG_FIFO);
    if (at_event_lock == RT_NULL)
    {
        LOG_E("AT client port initialize failed! at_sock_lock create failed!");
        rt_event_delete(at_socket_event);
        return -RT_ENOMEM;
    }

    /* initialize AT client */
    at_client_init(AT_DEVICE_NAME, AT_DEVICE_RECV_BUFF_LEN);

    /* register URC data execution function  */
    at_set_urc_table(urc_table, sizeof(urc_table) / sizeof(urc_table[0]));

    /* add network interface device to netdev list */
    if (a9g_netdev_add(A9G_NETDEV_NAME) < 0)
    {
        LOG_E("A9G network interface device(%d) add failed.", A9G_NETDEV_NAME);
        return -RT_ENOMEM;
    }

    /* initialize a9g network */
    a9g_net_init();

    /* set a9g AT Socket options */
    at_socket_device_register(&a9g_socket_ops);

    return RT_EOK;
}
INIT_APP_EXPORT(at_socket_device_init);

#endif /* AT_DEVICE_A9G */




