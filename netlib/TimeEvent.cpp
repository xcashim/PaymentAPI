#include "TimeEvent.h"
#include "ListenThread.h"

static void timeout_cb(evutil_socket_t fd, short event, void *arg)
{
    time_event* pTimeEvent = (time_event*)arg;
    pTimeEvent->trigger();
}

time_event::time_event(const libevent_thread* pThread, bool bPersist, int iMilliSecs)
    : m_bPersist(bPersist)
{
    short nFlags = 0;
    if (bPersist) {
        nFlags |= EV_PERSIST;
    }
    m_tv.tv_sec = iMilliSecs/1000;
    m_tv.tv_usec = (iMilliSecs%1000)*1000;
    event_assign(&m_ev, pThread->m_pEvBase, -1, nFlags, timeout_cb, (void*)this); 
}

time_event::~time_event()
{
    event_del(&this->m_ev);
}

bool schedule_event(time_event* pTimeEvent)
{
    if (pTimeEvent == NULL) {
        return false;
    }
    short nFlags = 0;
    if (pTimeEvent->m_bPersist) {
        nFlags |= EV_PERSIST;
    }
    return event_add(&pTimeEvent->m_ev, &pTimeEvent->m_tv) == 0;
}

void remove_event(time_event* pTimeEvent)
{
    event_del(&pTimeEvent->m_ev);
}
