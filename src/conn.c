#include "header.h"
#include "conn.h"
#include "network.h"

typedef struct send_req_s {
	uv_udp_send_t req;
	uv_buf_t buf;
}send_req_t;

/*@ckcpuv*/
static int on_kcp_output(const char *buf, int len, struct IKCPCB *kcp, void *user)
{
	conn_t * conn = (conn_t*)user;
	return conn_send_udp(conn, buf, len);
}

/*@ckcpuv*/
static void on_send_done(uv_udp_send_t* req, int status)
{
	send_req_t * send_req = (send_req_t *)req;
	free(send_req->buf.base); /* TODO: ensure free*/
	free(send_req);	/** TODO: ensure free */
}


/*@ckcpuv*/
conn_t * conn_new(network_t * network)
{
	conn_t * thiz = malloc(sizeof(conn_t));
    ASSERT_RET(NULL != thiz, "malloc failed", NULL);

	thiz->_network = network;
	thiz->_conv = 0;
	thiz->_udp = NULL;
	thiz->_kcp = NULL;

	return thiz;
}


/*@ckcpuv*/
void conn_del(conn_t * thiz)
{
	free(thiz);
}

/*@ckcpuv*/
int conn_init(conn_t * thiz, kcpuv_conv_t conv, const struct sockaddr* addr, uv_udp_t* handle) {
	thiz->_conv = conv;
	thiz->_addr = *addr;
	thiz->_udp = handle;

	int r = -1;
	thiz->_kcp = ikcp_create(conv, (void*)thiz);
	CHK_COND(thiz->_kcp);

	thiz->_kcp->output = on_kcp_output;

	r = ikcp_nodelay(thiz->_kcp, 1, 10, 2, 1);
	PROC_ERR(r);

	return 0;
Exit0:
	return -1;
}



/*@ckcpuv*/
void conn_shutdown(conn_t * thiz) {
	ikcp_release(thiz->_kcp);
}


/*@ckcpuv*/
void conn_on_recv_udp(conn_t * thiz, const char* buf, ssize_t size, const struct sockaddr* addr)
{
	thiz->_addr = *addr;
	ikcp_input(thiz->_kcp, buf, (long)size);
}

/*@ckcpuv*/
/** TODO: check buf and size */
int conn_recv_kcp(conn_t * thiz, char** buf, uint32_t * size)
{
	int len = ikcp_peeksize(thiz->_kcp);
	if (len < 0) {
		return -1;
	}

	char * data = malloc(sizeof(char) * len);
	int r = ikcp_recv(thiz->_kcp, data, len);
	if (r < 0) {
		free(data);
		return r;
	}

	*buf = data;
	*size = (uint32_t)len;
	return 0;
}


/*@ckcpuv*/
int conn_send_udp(conn_t * thiz, const char* buf, uint32_t len)
{
	int r = -1;
	send_req_t * req = malloc(sizeof(send_req_t));
	CHK_COND(req);

	req->buf.base = malloc(sizeof(char) * len);
	req->buf.len = len;

	memcpy(req->buf.base, buf, len);

	r = uv_udp_send((uv_udp_send_t*)req, thiz->_udp, &req->buf, 1, &thiz->_addr, on_send_done);
	if (r < 0) {
		free(req->buf.base); /* TODO: ensure free */
		free(req); /* TODO: ensure free ? */
		return -1;
	}

	return 0;
Exit0:
	return r;
}


/*@ckcpuv*/
int conn_send_kcp(conn_t * thiz, const char* buf, uint32_t len)
{
	return ikcp_send(thiz->_kcp, buf, len);
}


/*@ckcpuv*/
/** TODO: change msg to malloc */
int conn_run(conn_t * thiz, uint64_t tick)
{
	ikcp_update(thiz->_kcp, (uint32_t)tick);

	char* buf;
	uint32_t size;
	int r = conn_recv_kcp(thiz, &buf, &size);
	if (r < 0)
		return r;

	kcpuv_msg_t * msg = malloc(sizeof(kcpuv_msg_t));
	msg->conv = thiz->_conv;
	msg->data = (uint8_t*)buf;
	msg->size = size;
	network_push_msg(thiz->_network, msg);
	return 0;
}

