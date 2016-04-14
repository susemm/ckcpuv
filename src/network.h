#ifndef __NETWORK_H
#define __NETWORK_H

#include "cstl/cmap.h"
#include "cstl/clist.h"

#include "uv.h"

typedef struct conn_s conn_t;
typedef int (*recv_msg_callback)(char* buf, uint32_t size);

typedef struct network_s
{
    uv_loop_t* _loop;
	uv_udp_t _udp;

	map_t * _map_conn;
	list_t * _queue_msg;
}network_t;

network_t * network_new(void);
void network_del(network_t * thiz);

int network_init(network_t * thiz);
void network_shutdown(network_t * thiz);

void network_run(network_t * thiz);

int network_udp_listen(network_t * thiz, const char* local_addr, int port);
conn_t * network_kcp_conn(network_t * thiz, kcpuv_conv_t conv, const char* local_addr, int port);

conn_t * network_get_conn_by_conv(network_t * thiz, kcpuv_conv_t conv);

int network_get_msg(network_t * thiz, kcpuv_msg_t ** msg);


void network_on_recv_udp(network_t * thiz, const char* buf, ssize_t size, const struct sockaddr* addr);
conn_t * network_add_conn(network_t * thiz, kcpuv_conv_t conv, const struct sockaddr* addr);
void network_push_msg(network_t * thiz, kcpuv_msg_t * msg);



#endif

