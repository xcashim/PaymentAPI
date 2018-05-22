#ifndef TIME_EVENT_H
#define TIME_EVENT_H

#include <event2/event.h>
#include <event2/event_struct.h>

struct libevent_thread;

class time_event
{
public:
    time_event(const libevent_thread* pThread, bool bPersist, int iMilliSecs);
    virtual ~time_event();
    virtual void trigger() = 0;
private:
    friend bool schedule_event(time_event* pTimeEvent);
    friend void remove_event(time_event* pTimeEvent);
private:
    event m_ev;
    bool m_bPersist;
    timeval m_tv;
};

bool schedule_event(time_event* pTimeEvent);
void remove_event(time_event* pTimeEvent);

template<class C, class R>
class time_event0 : public time_event
{
public:
    typedef R (C::*Fn)();

    time_event0(C* pClass, Fn fn, const libevent_thread* pThread, bool bPersist, int iMilliSecs)
        : time_event(pThread, bPersist, iMilliSecs)
        , m_pClass(pClass)
        , m_fn(fn)
    {
    }
    virtual void trigger()
    {
        (m_pClass->*m_fn)();
    }
private:
    C* m_pClass;
    Fn m_fn;
};

template<class C, class R, class P1>
class time_event1 : public time_event
{
public:
    typedef R (C::*Fn)(P1 p1);

    time_event1(C* pClass, Fn fn, P1 p1, const libevent_thread* pThread, bool bPersist, int iMilliSecs)
        : time_event(pThread, bPersist, iMilliSecs)
        , m_pClass(pClass)
        , m_fn(fn)
        , m_p1(p1)
    {
    }
    virtual void trigger()
    {
        (m_pClass->*m_fn)(m_p1);
    }
private:
    C* m_pClass;
    Fn m_fn;
    P1 m_p1;
};

#endif // TIME_EVENT_H