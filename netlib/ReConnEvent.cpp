#include "ReConnEvent.h"

#include <event2/event.h>
#include <event2/bufferevent.h>

#include "ListenThread.h"
#include "eloger_usefull.h"
#include "LogicInterface.h"

void connect_cb(struct bufferevent *bev, short events, void *arg)
{
    reconn_event* pReConn = (reconn_event*)arg;
    pReConn->on_connect(events);
}

reconn_event::reconn_event(const libevent_thread* pThread, 
    thread_dispatcher* pDispatcher, int iSecs, uint32 maxBuffer)
    : time_event(pThread, true, iSecs*1000)
    , m_pOwner(NULL)
    , m_pDispatcher(pDispatcher)
    , m_pThread(pThread)
    , m_pBuffer(NULL)
    , m_nPort(0)
    , m_bConnected(false)
    , m_bConnecting(false)
    , m_maxBuffer(maxBuffer)
{
}

reconn_event::~reconn_event()
{
    free_bufferevent();
}

void reconn_event::connect(const std::string& strIp, uint16 nPort)
{
	printf_s("connect to (%s:%u)...\n", strIp.c_str(), (uint32)nPort);

    m_strIp = strIp;
    m_nPort = nPort;

    sockaddr_in sin;
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = inet_addr(m_strIp.c_str());
    sin.sin_port = htons(nPort);

    if (m_pBuffer == NULL) {
        m_pBuffer = bufferevent_socket_new(m_pThread->m_pEvBase, -1, BEV_OPT_CLOSE_ON_FREE);
    }
    if (m_pBuffer == NULL) {
        return;
    }

    bufferevent_setcb(m_pBuffer, NULL, NULL, connect_cb, (void*)this);

    if (bufferevent_socket_connect(m_pBuffer, (sockaddr *)&sin, sizeof(sin)) < 0) {
        free_bufferevent();
        return;
    }

    m_bConnecting = true;
}

void reconn_event::on_connect(short events)
{
    m_bConnecting = false;
    m_bConnected = (events & BEV_EVENT_CONNECTED) != 0;;

    if (m_bConnected) {
		printf_s("connect to (%s:%u)(no=%d) ok\n", m_strIp.c_str(), (uint32)m_nPort, events);
    } else {
		printf_s("connect to (%s:%u)(no=%d) failed\n", m_strIp.c_str(), (uint32)m_nPort, events);
    }

    if (m_bConnected) {
        evutil_socket_t fd = bufferevent_getfd(m_pBuffer);
        if (fd != -1) {
            thread_dispatcher_dispatch(m_pDispatcher, fd, m_pOwner->get_id(), m_pOwner->get_ip(), m_maxBuffer);
        }
    } else {
        free_bufferevent();
    }
}

void reconn_event::on_disconnect()
{
    m_bConnected = false;  
    free_bufferevent();
}

void reconn_event::trigger()
{
    if (!m_bConnected && !m_bConnecting) {
        connect(m_strIp, m_nPort);
    }
}

void reconn_event::free_bufferevent()
{
    if (m_pBuffer) {
        bufferevent_free(m_pBuffer);
        m_pBuffer = NULL;
    }
}
