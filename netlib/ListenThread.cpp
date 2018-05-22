#include "ListenThread.h"

#include <event2/listener.h>
#include <event2/event.h>
#include <event2/thread.h>//for IOCP
#include "log-internal.h"
#include "MsgQueue.h"
#include "NetConnection.h"
#include "MsgInterface.h" 
#include "LogicInterface.h"
//#include "eloger_usefull.h"// 2015-07-02
#include "TimeEvent.h"
//////////////////////////////////////////////////////////////////////////

struct listen_thread
{
    libevent_thread m_evThread;
    evconnlistener* m_pListener;
    uint16 m_port;
    thread_dispatcher* m_pDispatcher;
};

struct io_thread
{
    libevent_thread m_evThread;
    event_msgqueue* m_pAcceptConnQueue;
    net_conn_pool* m_pConnPool;
    thread_dispatcher* m_pParent;

    event_msgqueue* m_pInputQueue;
    event_msgqueue* m_pOutputQueue;

    uint32 m_put_out_count;
    uint32 m_get_out_count;

	struct event m_evDetect;
};

#define get_evbase(pThread) (pThread->m_evThread.m_pEvBase)

struct thread_dispatcher
{
    io_thread* m_pIoThreads;
    thread_oid_t m_iThreads;
    thread_oid_t m_iCurThread;
    fn_dispatch_thread m_fnDispatch;

    conn_oid_t m_nMaxConnOfThread;

    logic_interface* m_pLogicIF;

    thread_oid_t m_iCurInitCount;
    sp_thread_mutex_t m_lockInit;
    sp_thread_cond_t m_condInit;
};

struct accept_conn
{
    evutil_socket_t m_fd;
    conn_state m_connState;
    session_oid_t m_id;

    uint32 m_maxBuffer;
    uint32 m_ip;
};

//////////////////////////////////////////////////////////////////////////
libevent_thread* libevent_thread_new(thread_oid_t iOid)
{
    libevent_thread* pThread = new libevent_thread;
    if (libevent_thread_init(pThread, iOid) != 0) {
        libevent_thread_del(pThread);
        return NULL;
    }
    return pThread;
}

void libevent_thread_del(libevent_thread* pThread)
{
    delete pThread;
}

int libevent_thread_init( libevent_thread* pThread, thread_oid_t iOid )
{
#ifdef IOCP
	//2014-11-20 11:31:03  for IOCP-----------------
	printf_s("Init IOCP.\n");

	evthread_use_windows_threads();
	//evthread_use_pthreads();
	event_config *cfg = event_config_new();
	event_config_set_num_cpus_hint(cfg, 4);
	event_config_set_flag(cfg, EVENT_BASE_FLAG_STARTUP_IOCP);

	pThread->m_pEvBase = event_base_new_with_config(cfg);

	event_config_free(cfg);

	printf_s("IOCP starts.\n");
	//----------------------------------------------------
#else
    pThread->m_pEvBase = event_base_new();
#endif

    if (pThread->m_pEvBase == NULL) {
        return -1;
    }
    pThread->m_id = iOid;
    return 0;
}

int libevent_thread_fini( libevent_thread* pThread )
{
    event_base_free(pThread->m_pEvBase); 
    pThread->m_id = invalid_thread_oid;
    return 0;
}

thread_oid_t libevent_thread_id( libevent_thread* pThread )
{
    return pThread->m_id;
}

int libevent_thread_start( sp_thread_func_t fnThread, void* args )
{
    sp_thread_t threadId = 0;
    sp_thread_attr_t attr;
    sp_thread_attr_init(&attr);
    sp_thread_attr_setdetachstate(&attr, SP_THREAD_CREATE_DETACHED);

    if(sp_thread_create(&threadId, &attr, fnThread, args) != 0) {
        return -1;
    } 
    return 0;
}

int libevent_thread_stop( libevent_thread* pThread )
{
    event_base_loopexit(pThread->m_pEvBase, 0);
    return 0;
}

int libevent_thread_loop( libevent_thread* pThread )
{
    return event_base_loop(pThread->m_pEvBase, 0);
}

////////////////////////////////////////////////////////////////////////

static void listen_callback(struct evconnlistener *pListener, evutil_socket_t fd, 
    struct sockaddr *pSockAddr, int socklen, void *user_data)
{
    listen_thread* pLThread = (listen_thread*)user_data;
    thread_dispatcher* pDispatcher = pLThread->m_pDispatcher;
    if (pDispatcher == NULL) {
        return ;
    }
    event_base* pEvBase = evconnlistener_get_base(pListener);
    evutil_socket_t fd2 = evconnlistener_get_fd(pListener);
    accept_conn* pAcceptConn = new accept_conn;
    pAcceptConn->m_fd = fd;
    pAcceptConn->m_connState = cs_unknown;
    pAcceptConn->m_id = invalid_session_oid;
    pAcceptConn->m_maxBuffer = 0;
    pAcceptConn->m_ip = ((sockaddr_in *)pSockAddr)->sin_addr.s_addr;

	//inet_ntoa( ((sockaddr_in *)pSockAddr)->sin_addr );
	printf("IP:%s try connect.\n", inet_ntoa( ((sockaddr_in *)pSockAddr)->sin_addr ));

    if (thread_dispatcher_dispatch(pDispatcher, pAcceptConn) != 0) {
        delete pAcceptConn;
    }
}

int libevent_add_listen(listen_thread* pListenThread, const char* szIp, uint16 port)
{
    struct sockaddr_in sin;
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = inet_addr(szIp);
    sin.sin_port = htons(port);

    evconnlistener* pListener = evconnlistener_new_bind(get_evbase(pListenThread), 
        listen_callback, pListenThread, LEV_OPT_REUSEABLE|LEV_OPT_CLOSE_ON_FREE, -1,
        (struct sockaddr*)&sin,
        sizeof(sin));

    if (!pListener) {
        return -1;
    }

    pListenThread->m_pListener = pListener;
    pListenThread->m_port = port;

    return 0;
}

listen_thread* listen_thread_new()
{
    return new listen_thread;
}

void Listen_thread_del(listen_thread* pListenThread)
{
    delete pListenThread;
}

int listen_thread_init( listen_thread* pListenThread, const char* szIp, uint16 port, thread_dispatcher* pDispatcher )
{
    if (libevent_thread_init(&pListenThread->m_evThread, invalid_thread_oid) != 0) {
        return -1;
    }
    if (libevent_add_listen(pListenThread, szIp, port) != 0) {
        return -1;
    }
    pListenThread->m_port = port;
    pListenThread->m_pDispatcher = pDispatcher;
    return 0;
}

int listen_thread_fini( listen_thread* pListenThread )
{
    if (pListenThread->m_pListener != NULL) {
        evconnlistener_free(pListenThread->m_pListener);
        pListenThread->m_pListener = NULL;            
    }
    pListenThread->m_port = 0;
    pListenThread->m_pDispatcher = NULL;
    libevent_thread_fini(&pListenThread->m_evThread);
    return 0;
}
static sp_thread_result_t SP_THREAD_CALL listen_thread_fn(void* args)
{
    listen_thread* pThread = (listen_thread*)args;
    libevent_thread_loop(&pThread->m_evThread);
    return 0;
}
int listen_thread_start( listen_thread* pListenThread )
{
    return libevent_thread_start(&listen_thread_fn, (void*)pListenThread);   
}

//////////////////////////////////////////////////////////////////////////

static fn_logic_handle_disconn g_logic_handle_disconn = &logic_handle_disconn_def;
static fn_logic_handle_msg g_logic_handle_msg_fn = &logic_handle_msg_def;
static fn_io_send_msg g_io_send_msg_fn = &io_send_msg_def;

void regfn_logic_handle_msg(fn_logic_handle_msg fn)
{
    if (fn) g_logic_handle_msg_fn = fn;   
}
void regfn_io_send_msg(fn_io_send_msg fn)
{
    if (fn) g_io_send_msg_fn = fn;   //g_io_send_msg_fn
}

static void accept_conn_fn(const event_msg_t msg, void* args);
static void msg_input_fn(const event_msg_t msg, void* args);
static void msg_output_fn(const event_msg_t msg, void* args);

int io_thread_init( io_thread* pThread, thread_oid_t iOid, conn_oid_t maxConn, const io_buffer_setting& buf, 
    thread_dispatcher* pDispatcher, libevent_thread* pLogicThread )
{
    if (iOid == invalid_thread_oid || maxConn <= 0) {
        return -1;
    }

    pThread->m_put_out_count = 0;
    pThread->m_get_out_count = 0;

    if (libevent_thread_init(&pThread->m_evThread, iOid) != 0) {
        return -1;
    }

    pThread->m_pConnPool = net_conn_pool_new(pThread, maxConn);
    if (pThread->m_pConnPool == NULL) {
        return -1;
    }

    // listen_thread to io_thread
    pThread->m_pAcceptConnQueue = event_msgqueue_new();
    if (pThread->m_pAcceptConnQueue == NULL) {
        return -1;
    }
    if (event_msgqueue_init(pThread->m_pAcceptConnQueue, buf.m_acceptMaxSize, get_evbase(pThread), 
        &accept_conn_fn, (void*)pThread) != 0) {
        return -1;
    }

    // io_thread to logic_thread
    pThread->m_pInputQueue = event_msgqueue_new();
    if (pThread->m_pInputQueue == NULL) {
        return -1;
    }
    if (event_msgqueue_init(pThread->m_pInputQueue, buf.m_inMaxSize, pLogicThread->m_pEvBase, 
        &msg_input_fn, (void*)pThread) != 0) {
            return -1;
    }

    // logic_thread to io_thread
    pThread->m_pOutputQueue = event_msgqueue_new();
    if (pThread->m_pOutputQueue == NULL) {
        return -1;
    }
    if (event_msgqueue_init(pThread->m_pOutputQueue, buf.m_outMaxSize, get_evbase(pThread), 
        &msg_output_fn, (void*)pThread) != 0) {
            return -1;
    }

	struct timeval tv={CHECK_NET_INTERVAL, 0};
	event_assign(&pThread->m_evDetect, pThread->m_evThread.m_pEvBase, -1, EV_PERSIST, net_conn_detect, (void*) pThread->m_pConnPool);
	event_add(&pThread->m_evDetect, &tv);
	    
    pThread->m_pParent = pDispatcher;
    return 0;
}

int io_thread_fini( io_thread* pThread )
{
    pThread->m_pParent = NULL;
    if (pThread->m_pConnPool != NULL) {
        net_conn_pool_del(pThread->m_pConnPool);
        pThread->m_pConnPool = NULL;
    }
    event_msgqueue_destory(pThread->m_pAcceptConnQueue);
    pThread->m_pAcceptConnQueue = NULL;
    
    event_msgqueue_destory(pThread->m_pInputQueue);
    pThread->m_pInputQueue = NULL;
    
    event_msgqueue_destory(pThread->m_pOutputQueue);
    pThread->m_pOutputQueue = NULL;

    libevent_thread_fini(&pThread->m_evThread);

    return 0;
}

static void accept_conn_fn(const event_msg_t msg, void* args)
{
    accept_conn* pAccept = (accept_conn*)msg;
    if (pAccept == NULL) {
        return;
    }

    io_thread* pMeThread = (io_thread*)args;
    if (pMeThread == NULL) {
        delete pAccept;
        return;
    }
    net_conn* pNewConn = net_conn_new(pMeThread->m_pConnPool, pAccept->m_maxBuffer);
    if (pNewConn == NULL) {
        delete pAccept;
		printf("the conn count is over limit.\n");
        return ;
    }
	else//cooker 2017-3-29 08:41:31
	{
		/*
		struct net_conn
		{
		conn_oid_t m_iConnId;
		bufferevent* m_pBuffer;
		uint32 m_nMaxBufferSize;
		net_conn_pool* m_pConnPool;

		uint32 m_ip;
		uint32 m_revSec;
		};
		*/
		printf("m_iConnId:%d m_nMaxBufferSize:%d m_ip:%u m_revSec:%u.\n", pNewConn->m_iConnId, pNewConn->m_nMaxBufferSize, pNewConn->m_ip, pNewConn->m_revSec);
	}

    if (net_conn_init(pNewConn, pAccept->m_fd, pMeThread->m_evThread.m_pEvBase, pAccept->m_ip) != 0) {
        delete pAccept;
        net_conn_del(pMeThread->m_pConnPool, net_conn_get_conn_id(pNewConn));
		printf("net_conn_init failed.\n");
        return ;
    }

	net_conn_recv_sec(pNewConn);

    // notify applayer TODO error notify
    if (pAccept->m_connState == cs_net_connected) {
        io_notify_logic_conn_msg(pNewConn, cs_net_connected, pAccept->m_id);
    }

    delete pAccept;
    pAccept = NULL;
}

static void logic_handle_conn_msg(conn_msg* pConnMsg, void* args)
{
    if (pConnMsg->m_body.m_state == cs_net_disconn) {
        (*g_logic_handle_disconn)(
            pConnMsg->m_hd.m_threadOid, pConnMsg->m_body.m_connOid);
    }
}

static void io_handle_conn_msg(conn_msg* pConnMsg, io_thread* pThread)
{
    if (pConnMsg->m_body.m_state == cs_layer_disconn) {
        net_conn_del(pThread->m_pConnPool, pConnMsg->m_body.m_connOid);
    } else if (pConnMsg->m_body.m_state == cs_layer_connected) {
        net_conn* pConn = net_conn_find(pThread->m_pConnPool, pConnMsg->m_body.m_connOid);
        if (pConn) {
            net_conn_set_max_buffer(pConn, pConnMsg->m_body.m_maxBuffer);
        }
    }
}

static void msg_input_fn(const event_msg_t msg, void* args)
{
    io_thread* pThread = (io_thread*)args;
    logic_interface* pLogicIF = pThread->m_pParent->m_pLogicIF;

    host_msg_hd* pHostMsgHd = (host_msg_hd*)msg;
    if (pHostMsgHd->m_t == host_mt_conn) {
        conn_msg* pConnMsg = (conn_msg*)msg;
        pLogicIF->handle_conn_msg(pConnMsg);
        conn_msg_free(pConnMsg);
        //logic_handle_conn_msg(pConnMsg, args);
    } else {
        net_msg* pNetMsg = (net_msg*)msg;
        pLogicIF->handle_logic_msg(pNetMsg);
        msg_free(msg);
        //(*g_logic_handle_msg_fn)(pNetMsg, args);           
    }
}

static void msg_output_fn(const event_msg_t msg, void* args)
{
    io_thread* pThread = (io_thread*)args;
    host_msg_hd* pHostMsgHd = (host_msg_hd*)msg;
    if (pHostMsgHd->m_t == host_mt_conn) {
        conn_msg* pConnMsg = (conn_msg*)msg;
        io_handle_conn_msg(pConnMsg, pThread);
        conn_msg_free(pConnMsg);
    } else {
        net_msg* pNetMsg = (net_msg*)msg;

        io_thread* pThread = (io_thread*)args;
        ++pThread->m_get_out_count;
		//printf("iosend:tid=%u,cid=%u,c=%u.\n", (uint32)pNetMsg->m_hd.m_threadOid, (uint32)pNetMsg->m_hd.m_connOid,pThread->m_get_out_count);

        (*g_io_send_msg_fn)(pNetMsg, args);   
        msg_free(msg);
    } 
}

event_msgqueue* io_thread_input(io_thread* pThread)
{
    return pThread->m_pInputQueue;
}

event_msgqueue* io_thread_output(io_thread* pThread)
{
    return pThread->m_pOutputQueue;
}

net_conn_pool* io_thread_conn_pool(io_thread* pThread)
{
    return pThread->m_pConnPool;
}

thread_dispatcher* io_thread_get_parent( io_thread* pThread )
{
    return pThread->m_pParent;
}

//////////////////////////////////////////////////////////////////////////

#define is_valid_io_thread(dispatcher, oid) (oid >= 0 && oid < dispatcher->m_iThreads)

static thread_oid_t dispatch_thread_default(thread_dispatcher* pDispatcher);

thread_dispatcher* thread_dispatcher_new()
{
    return new thread_dispatcher;
}

void thread_dispatcher_del(thread_dispatcher* pDispatcher)
{
    delete pDispatcher;
}

int thread_dispatcher_init( thread_dispatcher* pDispatcher, libevent_thread* pLogicThread, 
    thread_oid_t iThreads, conn_oid_t maxConnOfThread, const io_buffer_setting& buf )
{
    if (iThreads <= 0 || maxConnOfThread <= 0) {
        return -1;
    }
    pDispatcher->m_pIoThreads = new io_thread[iThreads];
    if (pDispatcher->m_pIoThreads == NULL) {
        return -1;
    }
    for (thread_oid_t i = 0; i < iThreads; ++i) {
        io_thread_init(&pDispatcher->m_pIoThreads[i], i, maxConnOfThread, buf, pDispatcher, pLogicThread);
    }
    pDispatcher->m_iThreads = iThreads;
    pDispatcher->m_iCurThread = 0;
    pDispatcher->m_fnDispatch = &dispatch_thread_default;

    pDispatcher->m_nMaxConnOfThread = maxConnOfThread;
    pDispatcher->m_pLogicIF = NULL;

    pDispatcher->m_iCurInitCount = 0;
    sp_thread_mutex_init(&pDispatcher->m_lockInit, NULL);
    sp_thread_cond_init(&pDispatcher->m_condInit, NULL);
    return 0;
}

int thread_dispatcher_fini( thread_dispatcher* pDispatcher )
{
    if (pDispatcher->m_pIoThreads != NULL) {
        for (thread_oid_t i = 0; i < pDispatcher->m_iThreads; ++i) {
            io_thread_fini(&pDispatcher->m_pIoThreads[i]);
        }
        delete[] pDispatcher->m_pIoThreads;            
        pDispatcher->m_pIoThreads = NULL;
    }
    pDispatcher->m_iThreads = 0;
    pDispatcher->m_iCurThread = 0;
    pDispatcher->m_pLogicIF = NULL;

    pDispatcher->m_iCurInitCount = 0;

    sp_thread_mutex_destroy(&pDispatcher->m_lockInit);
    sp_thread_cond_destroy(&pDispatcher->m_condInit);
    return 0;
}

static thread_oid_t dispatch_thread_default(thread_dispatcher* pDispatcher)
{
    if (pDispatcher->m_iCurThread >= pDispatcher->m_iThreads)
        pDispatcher->m_iCurThread = 0;
    return pDispatcher->m_iCurThread++;
}

void thread_dispatcher_install_dispatch( thread_dispatcher* pDispatcher, fn_dispatch_thread fn )
{
    pDispatcher->m_fnDispatch = fn;
}

void thread_dispatcher_restore_dispatch( thread_dispatcher* pDispatcher )
{
    pDispatcher->m_fnDispatch = &dispatch_thread_default;
}

static sp_thread_result_t SP_THREAD_CALL io_thread_fn(void* args)
{
    io_thread* pThread = (io_thread*)args;

    thread_dispatcher* pDispatcher = pThread->m_pParent;

    sp_thread_mutex_lock(&pDispatcher->m_lockInit);
    ++pDispatcher->m_iCurInitCount;
    sp_thread_cond_signal(&pDispatcher->m_condInit);
    sp_thread_mutex_unlock(&pDispatcher->m_lockInit);

    libevent_thread_loop(&pThread->m_evThread);
    return 0;
}

int thread_dispatcher_start( thread_dispatcher* pDispatcher )
{
    for (thread_oid_t i = 0; i < pDispatcher->m_iThreads; ++i)
    {
        io_thread* pThread = &pDispatcher->m_pIoThreads[i];
        libevent_thread_start(io_thread_fn, (void*)pThread);  
    }

    sp_thread_mutex_lock(&pDispatcher->m_lockInit);
    while(pDispatcher->m_iCurInitCount < pDispatcher->m_iThreads) {
        sp_thread_cond_wait(&pDispatcher->m_condInit, &pDispatcher->m_lockInit);
    }
    sp_thread_mutex_unlock(&pDispatcher->m_lockInit);

    return 0;
}

int thread_dispatcher_dispatch( thread_dispatcher* pDispatcher, accept_conn* pConn )
{
    thread_oid_t iThreadOid = (*pDispatcher->m_fnDispatch)(pDispatcher);
    if (!is_valid_io_thread(pDispatcher, iThreadOid)) {
        return -1;
    }
    io_thread& ioThread = pDispatcher->m_pIoThreads[iThreadOid];
    event_msgqueue* pMsgQueue = ioThread.m_pAcceptConnQueue;
    if (event_msgqueue_push(pMsgQueue, (void*)pConn) != 0) {
        return -1;
    }
    return 0;
}

int thread_dispatcher_dispatch( thread_dispatcher* pDispatcher, evutil_socket_t fd, session_oid_t soid, uint32 ip, uint32 maxBuffer )
{
    accept_conn* pConn = new accept_conn;
    pConn->m_fd = fd;
    pConn->m_connState = cs_net_connected;
    pConn->m_id = soid;
    pConn->m_maxBuffer = maxBuffer;
    pConn->m_ip = ip;
    if (thread_dispatcher_dispatch(pDispatcher, pConn) != 0) {
        delete pConn;
        return -1;
    }
    return 0;
}

void reg_logic_interface( thread_dispatcher* pDispatcher, logic_interface* pLogicIF )
{
    if (pLogicIF) {
        pLogicIF->initialize(pDispatcher, pDispatcher->m_iThreads, pDispatcher->m_nMaxConnOfThread);
        pDispatcher->m_pLogicIF = pLogicIF ;
    }
}

logic_interface* get_logic_interface( thread_dispatcher* pDispatcher )
{
    return pDispatcher->m_pLogicIF;
}

int send_msg_hton( thread_dispatcher* pDispatcher, net_msg* pNetMsg )
{
    thread_oid_t toid = pNetMsg->m_hd.m_threadOid;

    if (!is_valid_io_thread(pDispatcher, toid)) {
		//printf("send_msg_hton(is_valid_io_thread=%u)\n", (uint32)toid);
        return -1;
    }
    io_thread& ioThread = pDispatcher->m_pIoThreads[toid];
    ++ioThread.m_put_out_count;
	//printf("hton:tid=%u, cid=%u, c=%u", (uint32)toid, (uint32)pNetMsg->m_hd.m_connOid, ioThread.m_put_out_count);

    event_msgqueue* pOutputQueue = io_thread_output(&ioThread);
    if (pOutputQueue == NULL) {
        return -1;
    }    
    if (event_msgqueue_push(pOutputQueue, pNetMsg) != 0) {
		//printf("send msg to io_thread failed\n");
        return -1;
    }
    return 0;   
}

thread_oid_t get_threads( thread_dispatcher* pDispatcher )
{
    return pDispatcher->m_iThreads;
}

io_thread* get_io_thread( thread_dispatcher* pDispatcher, thread_oid_t toid )
{
    if (is_valid_io_thread(pDispatcher, toid)) {
        return &pDispatcher->m_pIoThreads[toid];
    }
    return NULL;
}

conn_oid_t get_thread_max_conn( thread_dispatcher* pDispatcher )
{
    return pDispatcher->m_nMaxConnOfThread;
}
