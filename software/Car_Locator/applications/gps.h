#ifndef __GPS_H
#define __GPS_H	 
#include <board.h>
//////////////////////////////////////////////////////////////////////////////////	 
//本程序只供学习使用，未经作者许可，不得用于其它任何用途
//ALIENTEK STM32F407开发板
//ATK-NEO-6M GPS模块驱动代码	   
//正点原子@ALIENTEK
//技术论坛:www.openedv.com
//修改日期:2014/10/26
//版本：V1.0
//版权所有，盗版必究。
//Copyright(C) 广州市星翼电子科技有限公司 2014-2024
//All rights reserved							  
////////////////////////////////////////////////////////////////////////////////// 	   

//GPS NMEA-0183协议重要参数结构体定义 
//卫星信息
__packed typedef struct  
{
 	rt_uint8_t num;		//卫星编号
	rt_uint8_t eledeg;	//卫星仰角
	rt_uint16_t azideg;	//卫星方位角
	rt_uint8_t sn;		//信噪比		   
}nmea_slmsg;
//UTC时间信息
__packed typedef struct  
{
 	rt_uint16_t year;	//年份
	rt_uint8_t month;	//月份
	rt_uint8_t date;	//日期
	rt_uint8_t hour; 	//小时
	rt_uint8_t min; 	//分钟
	rt_uint8_t sec; 	//秒钟
}nmea_utc_time;
//NMEA 0183 协议解析后数据存放结构体
__packed typedef struct  
{
 	rt_uint8_t svnum;					//可见卫星数
	nmea_slmsg slmsg[12];		//最多12颗卫星
	nmea_utc_time utc;			//UTC时间
	rt_uint32_t latitude;				//纬度 分扩大100000倍,实际要除以100000
	rt_uint8_t nshemi;					//北纬/南纬,N:北纬;S:南纬				  
	rt_uint32_t longitude;			    //经度 分扩大100000倍,实际要除以100000
	rt_uint8_t ewhemi;					//东经/西经,E:东经;W:西经
	rt_uint8_t gpssta;					//GPS状态:0,未定位;1,非差分定位;2,差分定位;6,正在估算.				  
 	rt_uint8_t posslnum;				//用于定位的卫星数,0~12.
 	rt_uint8_t possl[12];				//用于定位的卫星编号
	rt_uint8_t fixmode;					//定位类型:1,没有定位;2,2D定位;3,3D定位
	rt_uint16_t pdop;					//位置精度因子 0~500,对应实际值0~50.0
	rt_uint16_t hdop;					//水平精度因子 0~500,对应实际值0~50.0
	rt_uint16_t vdop;					//垂直精度因子 0~500,对应实际值0~50.0 

	int altitude;			 	//海拔高度,放大了10倍,实际除以10.单位:0.1m	 
	rt_uint16_t speed;					//地面速率,放大了1000倍,实际除以10.单位:0.001公里/小时	 
}nmea_msg;

static nmea_msg gps_data;

int NMEA_Str2num(rt_uint8_t *buf,rt_uint8_t*dx);
void GPS_Analysis(nmea_msg *gpsx,rt_uint8_t *buf);
void NMEA_GPGSV_Analysis(nmea_msg *gpsx,rt_uint8_t *buf);
void NMEA_GPGGA_Analysis(nmea_msg *gpsx,rt_uint8_t *buf);
void NMEA_GPGSA_Analysis(nmea_msg *gpsx,rt_uint8_t *buf);
void NMEA_GPGSA_Analysis(nmea_msg *gpsx,rt_uint8_t *buf);
void NMEA_GPRMC_Analysis(nmea_msg *gpsx,rt_uint8_t *buf);
void NMEA_GPVTG_Analysis(nmea_msg *gpsx,rt_uint8_t *buf);

#endif  

 


