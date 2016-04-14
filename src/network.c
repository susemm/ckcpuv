#include "header.h"
#include "network.h"
#include "tm.h"
#include "conn.h"

/* TODO: change new to malloc */
static void on_alloc_buffer(uv_handle_t* handle, size_t size, uv_buf_t* buf) {
	buf->len = (unsigned long)size;
	buf->base = malloc(sizeof(char) * size);
}

static void on_network_recv_udp(
    uv_udp_t* handle,
    ssize_t nread,
    const uv_buf_t* rcvbuf,
    const struct sockaddr* addr,
    unsigned flags)
{
	network_t * server;
	if (nread <= 0) {
		goto Exit0;
	}

	server = (network_t *)(handle->data);
	network_on_recv_udp(server, rcvbuf->base, nread, addr);

Exit0:
	free(rcvbuf->base);
}

static void on_conn_recv_udp(
    uv_udp_t* handle,
    ssize_t nread,
    const uv_buf_t* rcvbuf,
    const struct sockaddr* addr,
    unsigned flags)
{
	conn_t * conn;
	if (nread <= 0) {
		goto Exit0;
	}

	conn = (conn_t *)(handle->data);
	conn_on_recv_udp(conn, rcvbuf->base, nread, addr);

Exit0:
	free(rcvbuf->base);
}

static void on_close_done(uv_handle_t* handle) {

}

/*@ckcpuv*/
network_t * network_new(void) {
	network_t * thiz = malloc(sizeof(network_t));
	ASSERT_RET(NULL != thiz, "malloc failed", NULL);
	thiz->_loop = NULL;

	thiz->_map_conn = create_map(int, void *);
	thiz->_queue_msg = create_list(void *);

	return thiz;
}

/*@ckcpuv*/
void network_del(network_t * thiz) {
    map_destroy(thiz->_map_conn);
    list_destroy(thiz->_queue_msg);

	free(thiz);
}

/*@ckcpuv*/
int network_init(network_t * thiz) {
    map_init(thiz->_map_conn);
    list_init(thiz->_queue_msg);
    
	thiz->_loop = uv_loop_new();
#ifdef PLATFORM_WINDOWS
	SetErrorMode(0);
#endif
	return 0;
}


/*@ckcpuv*/
void network_shutdown(network_t * thiz) {
	uv_close((uv_handle_t*)&(thiz->_udp), on_close_done);
	uv_run(thiz->_loop, UV_RUN_DEFAULT);
	uv_loop_delete(thiz->_loop);

	for (map_iterator_t it = map_begin(thiz->_map_conn);
	        !iterator_equal(it, map_end(thiz->_map_conn)); it = iterator_next(it)) {
		conn_t * conn = *(conn_t **)pair_second((pair_t*)iterator_get_pointer(it));
		conn_shutdown(conn);
		conn_del(conn); /*  */
	}
}



/*@*/
void network_run(network_t * thiz) {
	uv_run(thiz->_loop, UV_RUN_NOWAIT);

	for (map_iterator_t it = map_begin(thiz->_map_conn);
	        !iterator_equal(it, map_end(thiz->_map_conn)); it = iterator_next(it))
	{
		conn_t * conn = *(conn_t **)pair_second((pair_t*)iterator_get_pointer(it));
		conn_run(conn, get_tick_ms());
	}
}


int network_udp_listen(network_t * thiz, const char* local_addr, int32_t port) {
	int r = -1;
	struct sockaddr_in bind_addr;

	thiz->_udp.data = thiz;
	uv_udp_init(thiz->_loop, &(thiz->_udp));

	r = uv_ip4_addr(local_addr, port, &bind_addr);
	PROC_ERR(r);

	r = uv_udp_bind(&(thiz->_udp), (const struct sockaddr*)&bind_addr, 0);
	PROC_ERR(r);

	r = uv_udp_recv_start(&(thiz->_udp), on_alloc_buffer, on_network_recv_udp);
	PROC_ERR(r);

	log_info("udp listen port: %d", port);

	return 0;
Exit0:
	return r;
}


/*@*/
conn_t * network_kcp_conn(network_t * thiz, kcpuv_conv_t conv, const char* local_addr, int port)
{
	int r = -1;
	conn_t * conn = NULL;

	struct sockaddr_in addr;
	r = uv_ip4_addr(local_addr, port, &addr);
	PROC_ERR(r);

	conn = conn_new(thiz);
	conn_init(conn, conv, (const struct sockaddr*)&addr, &(thiz->_udp));

	thiz->_udp.data = conn;
	uv_udp_init(thiz->_loop, &(thiz->_udp));

	r = uv_udp_recv_start(&(thiz->_udp), on_alloc_buffer, on_conn_recv_udp);
	PROC_ERR(r);

	*(conn_t **)map_at(thiz->_map_conn, conv) = conn;
	return conn;

Exit0:
	free(conn);
	return NULL;
}


/*@*/
void network_on_recv_udp(network_t * thiz, const char* buf, ssize_t size, const struct sockaddr* addr)
{
	kcpuv_conv_t conv;
	int ret = ikcp_get_conv(buf, (long)size, (IUINT32 *)&conv);

	if (ret == 0)
		return;

	conn_t * conn = network_get_conn_by_conv(thiz, conv);

	if (!conn) {
		conn = network_add_conn(thiz, conv, addr);
	}

	if (conn) {
		conn_on_recv_udp(conn, buf, size, addr);
	}
}

/*@*/
conn_t * network_get_conn_by_conv(network_t * thiz, kcpuv_conv_t conv) {
	map_iterator_t it = map_find(thiz->_map_conn, conv);

	if (!iterator_equal(it, map_end(thiz->_map_conn)))
		return *(conn_t **)pair_second((pair_t*)iterator_get_pointer(it));;

	return NULL;
}

/*@*/
conn_t * network_add_conn(network_t * thiz, kcpuv_conv_t conv, const struct sockaddr* addr) {
	conn_t* conn = conn_new(thiz);
	conn_init(conn, conv, addr, &(thiz->_udp));
	*(conn_t **)map_at(thiz->_map_conn, conv) = conn;
	return conn;
}

/*@*/
int network_get_msg(network_t * thiz, kcpuv_msg_t** msg) {
	if (list_size(thiz->_queue_msg) == 0)
		return -1;

	*msg = *(kcpuv_msg_t **)list_front(thiz->_queue_msg);
	list_pop_front(thiz->_queue_msg);

	return 0;
}

/*@*/
void network_push_msg(network_t * thiz, kcpuv_msg_t * msg) {
	list_push_back(thiz->_queue_msg, msg);
}

/******************************************************************************/

struct kcpuv_s {
	network_t * network;
};

kcpuv_t kcpuv_create() {
	struct kcpuv_s * kcpuv = malloc(sizeof(struct kcpuv_s));

    kcpuv->network = network_new();
	int r = network_init(kcpuv->network);
	PROC_ERR(r);

	return kcpuv;
Exit0:
	free(kcpuv);
	return NULL;
}

void kcpuv_destroy(kcpuv_t kcpuv) {
	network_shutdown(kcpuv->network);
	network_del(kcpuv->network);
	free(kcpuv);
}

/* for server */
int kcpuv_listen(kcpuv_t kcpuv, const char* addr, uint32_t port) {
	return network_udp_listen(kcpuv->network, addr, port);
}

/* for client */
int kcpuv_connect(kcpuv_t kcpuv, kcpuv_conv_t conv, const char* addr, uint32_t port) {
	conn_t * conn = network_kcp_conn(kcpuv->network, conv, addr, port);
	if (!conn)
		return -1;
	return 0;
}

void kcpuv_run(kcpuv_t kcpuv) {
	network_run(kcpuv->network);
}

int kcpuv_recv(kcpuv_t kcpuv, kcpuv_msg_t** msg) {
	return network_get_msg(kcpuv->network, msg);
}

int kcpuv_send(kcpuv_t kcpuv, kcpuv_conv_t conv, const void* data, uint32_t size) {
	conn_t * conn = network_get_conn_by_conv(kcpuv->network, conv);

	if (!conn)
		return -1;

	return conn_send_kcp(conn, (const char*)data, size);
}

void kcpuv_msg_free(kcpuv_msg_t * msg) {
	free(msg->data);
	free(msg);
}