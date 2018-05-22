#include "LogicThread.h"

#include <signal.h>

#include "ListenThread.h"
#include "ReConnEvent.h"
#include "LogicInterface.h"
#include "eloger_usefull.h"

struct logic_thread
{
    libevent_thread* m_pEvThread;
    thread_dispatcher* m_pDispatcher;
    listen_thread* m_pListenThread;  
    thread_dispatcher* m_pReConnDispatcher;
};

static listen_thread* create_listener(thread_dispatcher* m_pDispatcher, const char* szIp, uint16 nPort);
static thread_dispatcher* create_dispatcher(logic_thread* pThread, 
    thread_oid_t ioThreads, conn_oid_t maxConnsOfThread, const io_buffer_setting& buf);

static bool has_listen(const net_setting& setting)
{
    return (!setting.m_strListenIp.empty()) && (setting.m_nListenPort > 0);
}

logic_thread* logic_thread_new( const net_setting& setting )
{
    ELOG_INIT("");//上层调用程序会用到

    logic_thread* pLogicThread = new logic_thread;
    if (pLogicThread == NULL) {
        return NULL;
    }
    memset(pLogicThread, 0, sizeof(logic_thread));

#ifdef WIN32
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif

#ifndef WIN32
    struct sigaction sa;
    sa.sa_handler = SIG_IGN;
    sigaction(SIGPIPE, &sa, 0);
#endif

    // main libevent_thread
    pLogicThread->m_pEvThread = libevent_thread_new(0);
    if (pLogicThread->m_pEvThread == NULL) {
        goto err;
    }

    if (has_listen(setting)) {
        if (setting.m_nIoThreads <= 0 || setting.m_maxConnsOfThread <= 0) {
            goto err;
        }
        // io thread dispatcher
        pLogicThread->m_pDispatcher = create_dispatcher(pLogicThread, 
            setting.m_nIoThreads, setting.m_maxConnsOfThread, setting.m_ioBuf);
        if (pLogicThread->m_pDispatcher == NULL) {
            goto err;
        }

        pLogicThread->m_pListenThread = create_listener(
            pLogicThread->m_pDispatcher, setting.m_strListenIp.c_str(), setting.m_nListenPort);
        if (pLogicThread->m_pListenThread == NULL) {
            goto err;
        }
    }

    // reconn io thread dispatcher
    if (setting.m_nReConnIoThreads > 0 && setting.m_maxReConnOfThread > 0) {
        pLogicThread->m_pReConnDispatcher = create_dispatcher(pLogicThread, 
            setting.m_nReConnIoThreads, setting.m_maxReConnOfThread, setting.m_reconnIoBuf);
        if (pLogicThread->m_pReConnDispatcher == NULL) {
            goto err;
        }
    }
    
    return pLogicThread;
err:
    logic_thread_del(pLogicThread);
    return NULL;
}

void logic_thread_del( logic_thread* pThread )
{
    if (pThread->m_pListenThread) {
        listen_thread_fini(pThread->m_pListenThread);
        Listen_thread_del(pThread->m_pListenThread);
        pThread->m_pListenThread = NULL;
    }
    if (pThread->m_pDispatcher) {
        thread_dispatcher_fini(pThread->m_pDispatcher);
        thread_dispatcher_del(pThread->m_pDispatcher);
        pThread->m_pDispatcher = NULL;
    }
	//此处丢失释放 pThread->m_pReConnDispatcher 的逻辑? 2016-7-20
	if (pThread->m_pReConnDispatcher) {
		thread_dispatcher_fini(pThread->m_pReConnDispatcher);
		thread_dispatcher_del(pThread->m_pReConnDispatcher);
		pThread->m_pReConnDispatcher = NULL;
	}
	/////////////////////////////////////////////////
    if (pThread->m_pEvThread) {
        libevent_thread_fini(pThread->m_pEvThread);
        libevent_thread_del(pThread->m_pEvThread);
        pThread->m_pEvThread = NULL;
    }
    delete pThread;

#ifdef WIN32
    WSACleanup();
#endif

    ELOG_FINI();//上层调用程序会用到
}

int logic_thread_run( logic_thread* pThread )
{
    return libevent_thread_loop(pThread->m_pEvThread);   
}

int logic_thread_stop( logic_thread* pThread )
{
    libevent_thread_stop(pThread->m_pEvThread);
    return 0;
}

libevent_thread* logic_thread_getbase( logic_thread* pThread )
{
    return pThread->m_pEvThread;
}

static listen_thread* create_listener(thread_dispatcher* pDispatcher, const char* szIp, uint16 nPort)
{
    listen_thread* pLThread = listen_thread_new();
    if (pLThread == NULL) {
        return NULL;
    }
    if (listen_thread_init(pLThread, szIp, nPort, pDispatcher) != 0) {
		printf("listen_thread_init failed(%u)\n", (uint32)nPort);
        return NULL;
    }

    if (listen_thread_start(pLThread) != 0) {
		printf("start_libevent_thread failed\n");
        return NULL;
    }
	printf("listen (%u) ... \n", (uint32)nPort);
    return pLThread;
}

static thread_dispatcher* create_dispatcher(logic_thread* pThread, 
    thread_oid_t ioThreads, conn_oid_t maxConnsOfThread, const io_buffer_setting& buf)
{
    thread_dispatcher* pDispatcher = thread_dispatcher_new();
    if (pDispatcher == NULL) {
        return NULL;
    }
    if (thread_dispatcher_init(pDispatcher, pThread->m_pEvThread, 
        ioThreads, maxConnsOfThread, buf) != 0) {
			printf("thread_dispatcher_init failed\n");
            return NULL;
    }
    if (thread_dispatcher_start(pDispatcher) != 0) {
		printf("thread_dispatcher_start failed\n");
        return NULL;
    }      
    return pDispatcher;
}

//////////////////////////////////////////////////////////////////////////

int logic_thread_add_reconn( logic_thread* pThread, reconn_session* pSession, 
    const std::string& strIp, uint16 nPort, int iSecs, uint32 maxBuffer )
{
    logic_interface* pLIF = get_logic_interface(pThread->m_pReConnDispatcher);
    if (pLIF) {
        pLIF->add_pre_session(pSession);
        reconn_event* pReconn = new reconn_event(pThread->m_pEvThread, 
            pThread->m_pReConnDispatcher, iSecs, maxBuffer);
        if (pReconn) {
            pSession->add_reconn_event(pReconn);
            pReconn->connect(strIp, nPort);
            schedule_event(pReconn);
            return 0;
        }
    }
    return -1;
}

int send_listen_conn_msg( logic_thread* pThread, net_msg* pNetMsg )
{
    return send_msg_hton(pThread->m_pDispatcher, pNetMsg);
}

int send_reconn_conn_msg( logic_thread* pThread, net_msg* pNetMsg )
{
    return send_msg_hton(pThread->m_pReConnDispatcher, pNetMsg);
}

void reg_logic_interface_listen(logic_thread* pThread, logic_interface* pLogicOps)
{
    reg_logic_interface(pThread->m_pDispatcher, pLogicOps);
}

void reg_logic_interface_reconn(logic_thread* pThread, logic_interface* pLogicOps)
{
    reg_logic_interface(pThread->m_pReConnDispatcher, pLogicOps);
}
