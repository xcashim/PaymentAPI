#ifndef MSG_INTERFACE_H
#define MSG_INTERFACE_H

#include "TypeDefine.h"

enum parse_msg_relt
{
    pmr_ok             = 0,
    pmr_invalid_size   = 1,
    pmr_buffer_error   = 2,
    pmr_verify_failed  = 3,
    pmr_push_failed    = 4,
	pmr_alloc_failed   = 5,
};

enum conn_state
{
    cs_unknown         = 0,
    cs_net_accepted    = 1,
    cs_net_connected   = 2,
    cs_net_disconn     = 3,
    cs_layer_disconn   = 4,
    cs_layer_connected = 5,
	cs_net_norecv      = 6,
};

typedef int8 host_msg_t;
static host_msg_t 
    host_mt_conn = 0,
    host_mt_net  = 1,
    host_mt_muli_net = 2;

#pragma pack(1)
struct host_msg_hd
{
    host_msg_t m_t;
    thread_oid_t m_threadOid;
    conn_oid_t m_connOid;
};

struct conn_msg_body
{
    conn_oid_t m_connOid;
    int8 m_state;
    session_oid_t m_id;
    uint32 m_maxBuffer;
};
struct conn_msg
{
    host_msg_hd m_hd;
    conn_msg_body m_body;
};
struct net_msg
{
    host_msg_hd m_hd;
    uint8 m_body[1];
    
};
#pragma pack()

#define host_msg_hd_size    sizeof(host_msg_hd)

struct bufferevent;
struct net_conn;
struct event_msgqueue;

typedef parse_msg_relt (*fn_io_recv_msg)(bufferevent* pBuffer, net_conn* pConn, event_msgqueue* pInputQueue);
typedef void (*fn_logic_handle_msg)(const net_msg* pMsg, void* args);
typedef void (*fn_io_send_msg)(const net_msg* pMsg, void* args);
typedef void (*fn_logic_handle_disconn)(thread_oid_t toid, conn_oid_t coid);

void regfn_io_recv_msg(fn_io_recv_msg fn);
void regfn_logic_handle_msg(fn_logic_handle_msg fn);
void regfn_io_send_msg(fn_io_send_msg fn);
void regfn_logic_handle_disconn(fn_logic_handle_disconn fn);

inline parse_msg_relt io_recv_msg_def(bufferevent* pBuffer, net_conn* pConn, event_msgqueue* pInputQueue){ return pmr_ok; }
inline void logic_handle_msg_def(const net_msg* pMsg, void* args){}
inline void io_send_msg_def(const net_msg* pMsg, void* args){}
inline void logic_handle_disconn_def(thread_oid_t toid, conn_oid_t coid){}

struct thread_dispatcher;
int io_notify_logic_conn_msg(net_conn* pConn, conn_state state, session_oid_t id);
int logic_notify_io_conn_msg(thread_dispatcher* pDispatcher, const host_msg_hd& hd, conn_state state, session_oid_t soid, uint32 maxBuffer);

conn_msg* conn_msg_alloc();
void conn_msg_free(conn_msg* pMsg);

uint8* msg_alloc(uint16 nSize);
void msg_free(void* pMsg);

#endif // MSG_INTERFACE_H
