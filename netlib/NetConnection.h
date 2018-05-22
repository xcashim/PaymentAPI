#ifndef NET_CONNECTION_H
#define NET_CONNECTION_H

#include <event2/event.h>
#include "TypeDefine.h"


#include <errno.h>
#include <event2/bufferevent.h>
#include <event2/buffer.h>
#include "ListenThread.h"
#include "IdAlloctor.h"
#include "MsgInterface.h"

#define	CHECK_NET_INTERVAL 5 * 60

//struct net_conn;
//struct net_conn_pool;


struct net_conn
{
	conn_oid_t m_iConnId;
	bufferevent* m_pBuffer;
	uint32 m_nMaxBufferSize;
	net_conn_pool* m_pConnPool;

	uint32 m_ip;
	uint32 m_revSec;
};

struct net_conn_pool
{
	net_conn** m_pConns;
	//conn_oid_t m_nMaxConns;
	uint16	m_nMaxConns;
	id_alloctor* m_pIdAlloctor;
	io_thread* m_pIoThread;
};


struct io_thread;

int net_conn_init(net_conn* pConn, evutil_socket_t fd, event_base* pEvBase, uint32 ip);
conn_oid_t net_conn_get_conn_id(net_conn* pConn);
thread_oid_t net_conn_get_thread_id(net_conn* pConn);
net_conn_pool* net_conn_get_pool(net_conn* pConn);
io_thread* net_conn_get_thread(net_conn* pConn);
uint32 net_conn_ip(net_conn* pConn);

net_conn_pool* net_conn_pool_new(io_thread* pThread, uint16 nMaxConn);
int net_conn_pool_del(net_conn_pool* pConnPool);
net_conn* net_conn_find(net_conn_pool* pConnPool, conn_oid_t id);
net_conn* net_conn_new(net_conn_pool* pConnPool, uint32 maxBuffer);
int net_conn_del(net_conn_pool* pConnPool, conn_oid_t id);
void net_conn_set_max_buffer(net_conn* pConn, uint32 nMaxBuffer);

int net_conn_write(net_conn_pool* pConnPool, conn_oid_t id, const void* pData, size_t size);
int net_conn_disconn(net_conn_pool* pConnPool, conn_oid_t id);

void net_conn_recv_sec(net_conn* pConn);
void net_conn_detect(evutil_socket_t fd, short event, void *arg);

#endif // NET_CONNECTION_H