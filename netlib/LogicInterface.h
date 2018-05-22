#ifndef LOGIC_INTERFACE_H
#define LOGIC_INTERFACE_H

#include <vector>
#include "TypeDefine.h"
#include "MsgInterface.h"


//struct logic_interface
//{
//    void (*fn_on_accept)();
//    void (*fn_on_connect)();
//    void (*fn_on_disconnect)();
//    void (*fn_handle_msg)(const thread_dispatcher* pDispatcher, const net_msg* pNetMsg);
//};

enum session_state
{
    ss_unknown          = 0,
    ss_conn_reg_ok      = 2,
    ss_conn_reg_failed  = 3,
    ss_conn_disconn     = 4,
};

//////////////////////////////////////////////////////////////////////////
class logic_session
{
public:
    logic_session();
    virtual ~logic_session();

    virtual void accpeted();
    virtual void connected();
    virtual void disconnected();
    bool is_connected();

    virtual void on_accept();
    virtual void on_connect();
    virtual void on_disconnect();
    virtual void handle_msg(const void* pMsg) {};

    void set_hd(const host_msg_hd& hd) { m_hd = hd; }
    void set_id(session_oid_t id) { m_id = id; }
    void set_ip(uint32 ip) { m_ip = ip; }

    bool has_valid_hd() const 
    { 
        return m_hd.m_threadOid != invalid_thread_oid && 
            m_hd.m_connOid != invalid_conn_oid; 
    }

    const host_msg_hd& get_hd() const { return m_hd; }
    thread_oid_t get_toid() const { return m_hd.m_threadOid; }
    conn_oid_t get_coid() const { return m_hd.m_connOid; }
    session_oid_t get_id() const { return m_id; }
    uint32 get_ip() const { return m_ip; }

protected:
    host_msg_hd m_hd;

private:
    session_oid_t m_id;
    session_state m_state;
    uint32 m_ip;
};

class reconn_event;
class reconn_session : public logic_session
{
public:
    reconn_session();
    virtual ~reconn_session();

    virtual void add_reconn_event(reconn_event* pReconn);
    
    virtual void disconnected();

protected:
	reconn_event* m_pReconn;//
};

struct thread_dispatcher;

//////////////////////////////////////////////////////////////////////////
class logic_interface
{
public:    
    logic_interface();
    virtual ~logic_interface();
    
    void initialize(thread_dispatcher* pDispatcher, thread_oid_t toid, conn_oid_t coid);

    void handle_conn_msg(const conn_msg* pConnMsg);
    virtual void handle_logic_msg(const net_msg* pNetMsg);

    int add_session(logic_session* pSession, const host_msg_hd& hd, uint32 maxBuffer);
    int remove_session(logic_session* pSession);
    bool has_session(const host_msg_hd& hd);
    logic_session* get_session(const host_msg_hd& hd);

    session_oid_t add_pre_session(logic_session* pSession);
    logic_session* get_pre_session(session_oid_t id);

    int send_io_conn_msg(const host_msg_hd& hd, conn_state state, session_oid_t soid, uint32 maxBuffer);

private:
    thread_dispatcher* m_pDispatcher;
    typedef std::vector<logic_session*> session_vec;
    typedef std::vector<session_vec> session_vecs;
    session_vecs m_sessionVecs;

    session_vec m_preSessionVec;
};

#endif // LOGIC_INTERFACE_H