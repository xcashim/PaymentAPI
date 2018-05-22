#include "MsgInterface.h"

#include "ListenThread.h"
#include "NetConnection.h"
#include "MsgQueue.h"

int io_notify_logic_conn_msg( net_conn* pConn, conn_state state, session_oid_t id )
{
    io_thread* pThread = net_conn_get_thread(pConn);
    if (pThread) {
        event_msgqueue* pInputQueue = io_thread_input(pThread);
        if (pInputQueue) {
            conn_msg* pConnMsg = conn_msg_alloc();
            pConnMsg->m_hd.m_t = host_mt_conn;
            pConnMsg->m_hd.m_threadOid = net_conn_get_thread_id(pConn);
            pConnMsg->m_hd.m_connOid = net_conn_get_conn_id(pConn);
            pConnMsg->m_body.m_connOid = net_conn_get_conn_id(pConn);
            pConnMsg->m_body.m_state = state;
            pConnMsg->m_body.m_id = id;
            pConnMsg->m_body.m_maxBuffer = 0;
            return event_msgqueue_push(pInputQueue, pConnMsg);
        }
    }
    return -1;
}

int logic_notify_io_conn_msg( thread_dispatcher* pDispatcher, const host_msg_hd& hd, 
                              conn_state state, session_oid_t soid, uint32 maxBuffer )
{
    io_thread* pThread = get_io_thread(pDispatcher, hd.m_threadOid);
    if (pThread == NULL) {
        return -1;
    }
    event_msgqueue* pOutputQueue = io_thread_output(pThread);
    if (pOutputQueue) {
        conn_msg* pConnMsg = conn_msg_alloc();
        pConnMsg->m_hd.m_t = host_mt_conn;
        pConnMsg->m_hd.m_threadOid = hd.m_threadOid;
        pConnMsg->m_hd.m_connOid = hd.m_connOid;
        pConnMsg->m_body.m_connOid = hd.m_connOid;
        pConnMsg->m_body.m_state = state;
        pConnMsg->m_body.m_id = soid;
        pConnMsg->m_body.m_maxBuffer = maxBuffer;
        return event_msgqueue_push(pOutputQueue, pConnMsg);
    }
    return -1;   
}

conn_msg* conn_msg_alloc()
{
    return new conn_msg;
}
void conn_msg_free(conn_msg* pMsg)
{
    delete pMsg;
}

uint8* msg_alloc( uint16 nSize )
{
    return new uint8[nSize];
}

void msg_free( void* pMsg )
{
    delete[] pMsg;
}
