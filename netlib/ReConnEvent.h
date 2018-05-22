#ifndef RE_CONN_EVENT_H
#define RE_CONN_EVENT_H

#include <string>

#include "TypeDefine.h"
#include "TimeEvent.h"

struct libevent_thread;
struct bufferevent;
struct thread_dispatcher;
class reconn_session;
class reconn_event : public time_event
{
public:
    reconn_event(const libevent_thread* pThread, 
        thread_dispatcher* pDispatcher, 
        int iSecs, uint32 maxBuffer);
    virtual ~reconn_event();

    virtual void trigger();

    void connect(const std::string& strIp, uint16 nPort);
    void on_connect(short events);
    void on_disconnect();
    bool is_connected() const { return m_bConnected; }
    void set_owner(reconn_session* pOwner) { m_pOwner = pOwner; }

private:
    void free_bufferevent();

private:
    reconn_session* m_pOwner;
    bufferevent* m_pBuffer;
    const libevent_thread* m_pThread;
    thread_dispatcher* m_pDispatcher;

    uint16 m_nPort;
    bool m_bConnected;
    bool m_bConnecting;
    uint32 m_maxBuffer;

public:
	std::string m_strIp;//2015-06-25  临时调试使用
};

#endif // RE_CONN_EVENT_H
