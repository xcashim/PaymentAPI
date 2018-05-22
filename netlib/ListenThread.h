#ifndef LISTEN_THREAD_H
#define LISTEN_THREAD_H

#include <vector>
#include <event2/util.h>

#include "TypeDefine.h"
#include "spthread.h"


//////////////////////////////////////////////////////////////////////////
// base thread
struct event_base;
struct libevent_thread
{
    thread_oid_t m_id;
    event_base* m_pEvBase;
};
struct libevent_thread;
libevent_thread* libevent_thread_new(thread_oid_t iOid);
void libevent_thread_del(libevent_thread* pThread);
int libevent_thread_init(libevent_thread* pThread, thread_oid_t iOid);
int libevent_thread_fini(libevent_thread* pThread);

thread_oid_t libevent_thread_id(libevent_thread* pThread);

int libevent_thread_start(sp_thread_func_t fnThread, void* args);
int libevent_thread_stop(libevent_thread* pThread);
int libevent_thread_loop(libevent_thread* pThread);

//////////////////////////////////////////////////////////////////////////
// listen thread
struct listen_thread;
struct thread_dispatcher;
listen_thread* listen_thread_new();
void Listen_thread_del(listen_thread* pListenThread);
int listen_thread_init(listen_thread* pListenThread, const char* szIp, uint16 port, thread_dispatcher* pDispatcher);
int listen_thread_fini(listen_thread* pListenThread);
int libevent_add_listen(listen_thread* pListenThread, const char* szIp, uint16 port);

int listen_thread_start(listen_thread* pListenThread);

//////////////////////////////////////////////////////////////////////////
// io thread
struct io_thread;
struct thread_dispatcher;
struct event_msgqueue;
struct net_conn_pool;
struct thread_dispatcher;
int io_thread_init(io_thread* pThread, thread_oid_t iOid, conn_oid_t maxConn, const io_buffer_setting& buf, 
    thread_dispatcher* pDispatcher, libevent_thread* pLogicThread);
int io_thread_fini(io_thread* pThread);

event_msgqueue* io_thread_input(io_thread* pThread);
event_msgqueue* io_thread_output(io_thread* pThread);
net_conn_pool* io_thread_conn_pool(io_thread* pThread);
thread_dispatcher* io_thread_get_parent(io_thread* pThread);

//////////////////////////////////////////////////////////////////////////
// thread dispatcher
struct thread_dispatcher;
thread_dispatcher* thread_dispatcher_new();
void thread_dispatcher_del(thread_dispatcher* pDispatcher);
int thread_dispatcher_init(thread_dispatcher* pDispatcher, libevent_thread* pLogicThread, 
    thread_oid_t iThreads, conn_oid_t maxConn, const io_buffer_setting& buf);
int thread_dispatcher_fini(thread_dispatcher* pDispatcher);

typedef thread_oid_t (*fn_dispatch_thread)(thread_dispatcher* pDispatcher);
void thread_dispatcher_install_dispatch(thread_dispatcher* pDispatcher, fn_dispatch_thread fn);
void thread_dispatcher_restore_dispatch(thread_dispatcher* pDispatcher);

thread_oid_t get_threads(thread_dispatcher* pDispatcher);
conn_oid_t get_thread_max_conn(thread_dispatcher* pDispatcher);

struct accept_conn;
int thread_dispatcher_start(thread_dispatcher* pDispatcher);
int thread_dispatcher_dispatch(thread_dispatcher* pDispatcher, accept_conn* pConn);
int thread_dispatcher_dispatch(thread_dispatcher* pDispatcher, evutil_socket_t fd, session_oid_t soid, uint32 ip, uint32 maxBuffer);

class logic_interface;
void reg_logic_interface(thread_dispatcher* pDispatcher, logic_interface* pLogicIF);
logic_interface* get_logic_interface(thread_dispatcher* pDispatcher);

io_thread* get_io_thread(thread_dispatcher* pDispatcher, thread_oid_t toid);

struct net_msg;
int send_msg_hton(thread_dispatcher* pDispatcher, net_msg* pNetMsg);

#endif // LISTEN_THREAD_H