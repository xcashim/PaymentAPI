#include "LogicInterface.h"

#include "ListenThread.h"
#include "ReConnEvent.h"
#include "MsgQueue.h"
#include "MsgInterface.h"
#include "NetConnection.h"

logic_session::logic_session()
    : m_id(invalid_session_oid)
    , m_state(ss_unknown)
    , m_ip(0)
{
    m_hd.m_threadOid = invalid_thread_oid;
    m_hd.m_connOid = invalid_conn_oid;
}

logic_session::~logic_session()
{

}

void logic_session::accpeted()
{
    m_state = ss_conn_reg_ok;  
    on_accept();
}

void logic_session::connected()
{
    m_state = ss_conn_reg_ok;
    on_connect();
}

void logic_session::disconnected()
{
    m_state = ss_conn_disconn;
    on_disconnect();
}

bool logic_session::is_connected()
{
    return m_state == ss_conn_reg_ok;
}

void logic_session::on_accept()
{
    
}

void logic_session::on_connect()
{
    
}

void logic_session::on_disconnect()
{
    
}

//////////////////////////////////////////////////////////////////////////
reconn_session::reconn_session()
    : m_pReconn(NULL)
{

}

reconn_session::~reconn_session()
{

}

void reconn_session::disconnected()
{
    m_pReconn->on_disconnect();
    logic_session::disconnected();
}

void reconn_session::add_reconn_event( reconn_event* pReconn )
{
    pReconn->set_owner(this);
    m_pReconn = pReconn;
}

//////////////////////////////////////////////////////////////////////////
logic_interface::logic_interface()
    : m_pDispatcher(NULL)
{

}

logic_interface::~logic_interface()
{
       
}

void logic_interface::initialize( thread_dispatcher* pDispatcher, thread_oid_t toid, conn_oid_t coid )
{
    m_pDispatcher = pDispatcher;
    m_sessionVecs.resize(toid);
    for (int i = 0; i < (int)m_sessionVecs.size(); ++i) {
        m_sessionVecs[i].resize(coid);
    }
}

void logic_interface::handle_conn_msg(const conn_msg* pConnMsg)
{
    const conn_msg_body& body = pConnMsg->m_body;
    const host_msg_hd& hd = pConnMsg->m_hd;

    if (body.m_state == cs_net_connected) {
        logic_session* pSession = get_pre_session(body.m_id);
        if (pSession) {
            add_session(pSession, hd, 0);
            pSession->connected();
        }
    } else if (body.m_state == cs_net_disconn) {
        logic_session* pSession = get_session(hd);
        if (pSession) {
            remove_session(pSession); 
            send_io_conn_msg(hd, cs_layer_disconn, pSession->get_id(), 0);
            pSession->disconnected();
        }
    } 
	else if (body.m_state == cs_net_norecv){
		logic_session* pSession = get_session(hd);
		if (pSession) {
			remove_session(pSession); 
			//send_io_conn_msg(hd, cs_layer_disconn, pSession->get_id(), 0);
			pSession->disconnected();
		}
    }
}

void logic_interface::handle_logic_msg( const net_msg* pNetMsg )
{
    logic_session* pSession = get_session(pNetMsg->m_hd);
    if (pSession) {
        pSession->handle_msg((const void*)(pNetMsg->m_body));
    }
}

session_oid_t logic_interface::add_pre_session( logic_session* pSession )
{
    session_oid_t id = m_preSessionVec.size();
    pSession->set_id(id);
    m_preSessionVec.push_back(pSession);
    return id;
}

logic_session* logic_interface::get_pre_session(session_oid_t id)
{
    if (id >= 0 && id < (int)m_preSessionVec.size()) {
        return m_preSessionVec[id];
    }
    return NULL;
}

int logic_interface::add_session( logic_session* pSession, const host_msg_hd& hd, uint32 maxBuffer)
{
	if (has_session(hd)) 
	{
		//m_sessionVecs[hd.m_threadOid][hd.m_connOid] = NULL;
    }

    pSession->set_hd(hd);
    m_sessionVecs[hd.m_threadOid][hd.m_connOid] = pSession;

    if (maxBuffer > 0) {
        send_io_conn_msg(hd, cs_layer_connected, pSession->get_id(), maxBuffer);
    }

    io_thread* ioThread = get_io_thread(m_pDispatcher, hd.m_threadOid);
    if (ioThread) {
        net_conn_pool* connPool = io_thread_conn_pool(ioThread);
        if (connPool) {
            net_conn* conn = net_conn_find(connPool, hd.m_connOid);
            if (conn) {
                pSession->set_ip(net_conn_ip(conn));
            }
        }
    }
    return 0;
}

int logic_interface::remove_session( logic_session* pSession )
{   
    const host_msg_hd& hd = pSession->get_hd();
    if (has_session(hd)) {
        if (pSession == m_sessionVecs[hd.m_threadOid][hd.m_connOid]) {
            m_sessionVecs[hd.m_threadOid][hd.m_connOid] = NULL;
            return 0;
        }
    }
    return -1;
}

logic_session* logic_interface::get_session( const host_msg_hd& hd )
{
    if (hd.m_threadOid >= 0 && hd.m_threadOid < (int)m_sessionVecs.size()) {
        session_vec& sessionVec = m_sessionVecs[hd.m_threadOid];
        if (hd.m_connOid >= 0 && hd.m_connOid < (int)sessionVec.size()) {
            return sessionVec[hd.m_connOid];
        }
    }
    return NULL;
}

bool logic_interface::has_session( const host_msg_hd& hd )
{
    return get_session(hd) != NULL;
}

int logic_interface::send_io_conn_msg( const host_msg_hd& hd, conn_state state, session_oid_t soid, uint32 maxBuffer )
{
    if (m_pDispatcher) {
        logic_notify_io_conn_msg(m_pDispatcher, hd, state, soid, maxBuffer);
        return 0;
    }
    return -1;
}
