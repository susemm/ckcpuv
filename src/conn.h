#ifndef __CONN_H
#define __CONN_H

typedef struct network_s network_t;

typedef struct conn_s
{
    network_t * _network;
	kcpuv_conv_t _conv;
	uv_udp_t * _udp;
	struct sockaddr _addr;
	ikcpcb * _kcp;
}conn_t;


conn_t * conn_new(network_t * network);
void conn_del(conn_t * thiz);

int conn_init(conn_t * thiz, kcpuv_conv_t conv, const struct sockaddr * addr, uv_udp_t* handle);
void conn_shutdown(conn_t * thiz);

int conn_send_kcp(conn_t * thiz, const char* buf, uint32_t len);
int conn_recv_kcp(conn_t * thiz, char ** buf, uint32_t * size);
int conn_run(conn_t * thiz, uint64_t tick);

void conn_on_recv_udp(conn_t * thiz, const char* buf, ssize_t size, const struct sockaddr* addr);
int conn_send_udp(conn_t * thiz, const char* buf, uint32_t len);


#endif

