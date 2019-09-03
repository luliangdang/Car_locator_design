#ifndef __A9G_DEVICE_H__
#define __A9G_DEVICE_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <board.h>

#include <stdlib.h>

#include <at_deviece.h>

/* The maximum number of sockets supported by the a9g device */
#define AT_DEVICE_A9G_SOCKETS_NUM		8

struct at_device_a9g
{
	char *device_name;
	char *client_name;
	
	size_t reve_line_num;
	struct at_device device;
	
	void *user_data;
};

#ifdef AT_USING_SOCKET

/* a9g device socket initialize */
int a9g_socket_init(struct at_device *device);

/* a9g device class socket register */
int a9g_socket_class_register(struct at_device_class *class);

#endif /* AT_USING_SOCKET */

#ifdef __cplusplus
}
#endif


#endif	/*	__AT_DEVICE_A9G_H__*/


