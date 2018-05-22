#include "NetConnection.h"

//#include <errno.h>
//#include <event2/bufferevent.h>
//#include <event2/buffer.h>
//#include "ListenThread.h"
//#include "IdAlloctor.h"
//#include "MsgInterface.h"
//#include "eloger_usefull.h"

#define net_conn_def_max_buffer 4*1024*1024

//struct net_conn
//{
//    conn_oid_t m_iConnId;
//    bufferevent* m_pBuffer;
//    uint32 m_nMaxBufferSize;
//    net_conn_pool* m_pConnPool;
//
//    uint32 m_ip;
//	uint32 m_revSec;
//};
//
//struct net_conn_pool
//{
//    net_conn** m_pConns;
//    //conn_oid_t m_nMaxConns;
//	uint16	m_nMaxConns;
//    id_alloctor* m_pIdAlloctor;
//    io_thread* m_pIoThread;
//};

//////////////////////////////////////////////////////////////////////////
static fn_io_recv_msg g_io_recv_msg_fn = &io_recv_msg_def;
void regfn_io_recv_msg(fn_io_recv_msg fn)
{
    if (fn) g_io_recv_msg_fn = fn;  //
}
//////////////////////////////////////////////////////////////////////////

static void conn_readcb(bufferevent* pBuffer, void* args);
static void conn_writecb(bufferevent* pBuffer, void* args);
static void conn_eventcb(bufferevent* pBuffer, short events, void* args);

int net_conn_init( net_conn* pConn, evutil_socket_t fd, event_base* pEvBase, uint32 ip )
{
    bufferevent *pBuffer = bufferevent_socket_new(pEvBase, fd, BEV_OPT_CLOSE_ON_FREE);
    if (pBuffer == NULL) {
        return -1;
    }
    bufferevent_setcb(pBuffer, conn_readcb, conn_writecb, conn_eventcb, pConn);
    bufferevent_enable(pBuffer, EV_READ);
    bufferevent_enable(pBuffer, EV_WRITE);
    bufferevent_setwatermark(pBuffer, EV_READ, 0, 64*1024);

    pConn->m_pBuffer = pBuffer;
    pConn->m_ip = ip;
    return 0;
}

int net_conn_fini( net_conn* pConn )
{
    if (pConn->m_pBuffer) {
        bufferevent_free(pConn->m_pBuffer);  
        pConn->m_pBuffer = NULL;            
    }
    pConn->m_iConnId = invalid_conn_oid;
    pConn->m_pConnPool = NULL;
    return 0;
}

conn_oid_t net_conn_get_conn_id( net_conn* pConn )
{
    return pConn->m_iConnId;
}

thread_oid_t net_conn_get_thread_id( net_conn* pConn )
{
    return libevent_thread_id((libevent_thread*)pConn->m_pConnPool->m_pIoThread);
}

net_conn_pool* net_conn_get_pool( net_conn* pConn )
{
    return pConn->m_pConnPool;
}

io_thread* net_conn_get_thread( net_conn* pConn )
{
    return pConn->m_pConnPool->m_pIoThread;
}
uint32 net_conn_ip(net_conn* pConn)
{
    return pConn->m_ip;
}

static void conn_readcb(bufferevent* pBuffer, void* args)
{
    net_conn* pConn = (net_conn*)args;
    if (pConn == NULL) {
        return ;
    }         
    event_msgqueue* pInputQueue = io_thread_input(pConn->m_pConnPool->m_pIoThread);
    parse_msg_relt relt = (*g_io_recv_msg_fn)(pBuffer, pConn, pInputQueue);
    if (relt != pmr_ok) {
		printf("parser msg failed(no=%d).\n", (int32)relt);
        net_conn_disconn(pConn->m_pConnPool, pConn->m_iConnId);
    }
}

static void conn_writecb(bufferevent* pBuffer, void* args)
{
    //evbuffer *pOutput = bufferevent_get_output(pBuffer);
    //if (evbuffer_get_length(pOutput) == 0) {
    //    PRINTF(LOG_DEBUG, "bufferevent output..\n", 1);
    //}
}

static void conn_eventcb(bufferevent* pBuffer, short events, void* args)
{
    net_conn* pConn = (net_conn*)args;
    if (pConn == NULL) {
        return ;
    } 
	if (events & BEV_EVENT_EOF) {
		//PRINTF(LOG_DEBUG, "Connection closed.\n", 1);
		printf("Connection closed.\n");
		
	} else if (events & BEV_EVENT_ERROR) {
		printf("Got an error on the connection: %s.\n", strerror(errno));
	} else {
        //PRINTF(LOG_DEBUG, "Connection disconnect.\n", 1);
		printf("Connection disconnect.\n");
    }
    net_conn_disconn(pConn->m_pConnPool, pConn->m_iConnId);
}

//////////////////////////////////////////////////////////////////////////

net_conn_pool* net_conn_pool_new(io_thread* pThread, uint16 nMaxConn)
{
    if (nMaxConn == 0) {
        return NULL;
    }

    net_conn_pool* pConnPool = new net_conn_pool;
    pConnPool->m_pConns = new net_conn*[nMaxConn];
    memset(pConnPool->m_pConns, 0, sizeof(pConnPool->m_pConns[0])*nMaxConn);
    pConnPool->m_nMaxConns = nMaxConn;

    pConnPool->m_pIdAlloctor = id_alloctor_new(nMaxConn);
    if (pConnPool->m_pIdAlloctor == NULL) {
        return NULL;
    }

    pConnPool->m_pIoThread = pThread;

    return pConnPool;
}

int net_conn_pool_del( net_conn_pool* pConnPool )
{
    if (pConnPool->m_pConns) {
        for (conn_oid_t id = 0; id < pConnPool->m_nMaxConns; ++id) {
            net_conn_del(pConnPool, id);
        }
        delete[] pConnPool->m_pConns;
        pConnPool->m_pConns = NULL;
    }
    if (pConnPool->m_pIdAlloctor) {
        id_alloctor_del(pConnPool->m_pIdAlloctor);            
        pConnPool->m_pIdAlloctor = NULL;
    }
    return 0;
}

inline bool is_valid_net_conn(net_conn_pool* pConnPool, conn_oid_t id)
{
    return id >= 0 && id < pConnPool->m_nMaxConns;  
}

net_conn* net_conn_find( net_conn_pool* pConnPool, conn_oid_t id )
{
    if (is_valid_net_conn(pConnPool, id)) {
        return pConnPool->m_pConns[id];
    }
    return NULL;
}

net_conn* net_conn_new( net_conn_pool* pConnPool, uint32 maxBuffer )
{
    uint16 id = id_alloctor_alloc(pConnPool->m_pIdAlloctor);
    if (!is_valid_net_conn(pConnPool, id)) {
        return NULL;
    }
    net_conn* pConn = new net_conn;
    pConnPool->m_pConns[id] = pConn;
    pConn->m_iConnId = id;
    pConn->m_pConnPool = pConnPool;
    pConn->m_pBuffer = NULL;
    if (maxBuffer > 0) {
        net_conn_set_max_buffer(pConn, maxBuffer);
    } else {
        net_conn_set_max_buffer(pConn, net_conn_def_max_buffer);
    }
    return pConn;
}

int net_conn_del( net_conn_pool* pConnPool, conn_oid_t id )
{
    if (!is_valid_net_conn(pConnPool, id)) {
        return -1;
    }
    if (pConnPool->m_pIdAlloctor) {
        id_alloctor_free(pConnPool->m_pIdAlloctor, id);
    }
    net_conn*& pConn = pConnPool->m_pConns[id];
    if (pConn) {
        net_conn_fini(pConn);
        delete pConn;
        pConn = NULL;
    }
    return 0;
}

void net_conn_set_max_buffer(net_conn* pConn, uint32 nMaxBuffer)
{
    pConn->m_nMaxBufferSize = nMaxBuffer;
}

int net_conn_write( net_conn_pool* pConnPool, conn_oid_t id, const void* pData, size_t size )
{
    net_conn* pConn = net_conn_find(pConnPool, id);
    if (pConn) {
        evbuffer* pOutBuf = bufferevent_get_output(pConn->m_pBuffer);
        size_t outLength = evbuffer_get_length(pOutBuf);
        if (outLength >= pConn->m_nMaxBufferSize) {
			printf("net_conn_write huge(conn_oid=%u).\n", (uint32)id);
           return -1;
        }

#ifdef _DEBUG
#endif
        return bufferevent_write(pConn->m_pBuffer, pData, size);
    } else {
        return -1;
    }
}

int net_conn_disconn( net_conn_pool* pConnPool, conn_oid_t id )
{
    net_conn* pConn = net_conn_find(pConnPool, id);
    if (pConn) {
        return io_notify_logic_conn_msg(pConn, cs_net_disconn, invalid_session_oid);
    }
    return -1;
}

void net_conn_recv_sec(net_conn* pConn)
{
	timeval tmVal;
	evutil_gettimeofday(&tmVal, NULL);
	pConn->m_revSec=tmVal.tv_sec;
}

void net_conn_detect(evutil_socket_t fd, short event, void *arg)
{
	net_conn_pool* pConnPool=(net_conn_pool*)arg;
	if (pConnPool->m_pConns) 
	{
		net_conn* pConn;
		timeval tmVal;
		evutil_gettimeofday(&tmVal, NULL);
		/*for (conn_oid_t id = 8; id < pConnPool->m_nMaxConns; id++)*/ 
		for (conn_oid_t id = 8; id < pConnPool->m_nMaxConns; id++)//
		{
			pConn = pConnPool->m_pConns[id];
			if (pConn&&( tmVal.tv_sec - pConn->m_revSec > CHECK_NET_INTERVAL ))//
			{
				printf("detect---kill net ConnId:%d .\n", pConn->m_iConnId);
				io_notify_logic_conn_msg(pConn, cs_net_norecv, invalid_session_oid);
				net_conn_del(pConnPool, pConn->m_iConnId);
			}
		}
		
	}
}