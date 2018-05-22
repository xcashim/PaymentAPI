#include "MsgParser.h"

#include "NetInclude.h"
#include "eloger_usefull.h"

static net_msg* my_msg_new( thread_oid_t toid, const conn_oid_t* pCoids, uint16 nCount, CNetMsgHead* pNetMsg )
{
    if (nCount == 0) {
        return NULL;
    }
    if (pNetMsg->wLen > my_msg_max_len) {
        return NULL;
    }

    net_msg* pHostMsg = NULL;
    my_net_msg_hd* pMyMsg = NULL;

    uint16 nSize = host_msg_hd_size + pNetMsg->wLen + my_net_chk_size;
    if (nCount == 1) {
        pHostMsg = (net_msg*)msg_alloc(nSize);
        pHostMsg->m_hd.m_t = host_mt_net;
        pHostMsg->m_hd.m_threadOid = toid;
        pHostMsg->m_hd.m_connOid = *pCoids;
        pMyMsg = (my_net_msg_hd*)pHostMsg->m_body;
        
    } else {
        pHostMsg = (net_msg*)msg_alloc(nSize + sizeof(conn_oid_t) * nCount);
        pHostMsg->m_hd.m_t = host_mt_muli_net;
        pHostMsg->m_hd.m_threadOid = toid;
        pHostMsg->m_hd.m_connOid = nCount;

        conn_oid_t* pDstCoids = (conn_oid_t*)pHostMsg->m_body;
        memcpy(pDstCoids, pCoids, sizeof(conn_oid_t) * nCount);

        pMyMsg = (my_net_msg_hd*)(pDstCoids+nCount);
    }

    pMyMsg->m_nLenChk = pNetMsg->wLen;
    memcpy(&pMyMsg->m_head, pNetMsg, pNetMsg->wLen);

	//printf("msg m_nLenChk=%d,wLen=%d\n",pMyMsg->m_nLenChk,pNetMsg->wLen);

    return pHostMsg;
}


int my_send_listen_conn_msg( logic_thread* pLogic, thread_oid_t toid, conn_oid_t coid, CNetMsgHead* pMsg )
{
    if (toid == invalid_thread_oid || coid == invalid_conn_oid) {
        return -1;
    }
    net_msg* pNetMsg = my_msg_new(toid, &coid, 1, pMsg);
    if (pNetMsg == NULL) {
        return -1;
    }
    return send_listen_conn_msg(pLogic, pNetMsg);
}

int my_send_reconn_conn_msg( logic_thread* pLogic, thread_oid_t toid, conn_oid_t coid, CNetMsgHead* pMsg )
{
    if (toid == invalid_thread_oid || coid == invalid_conn_oid) {
        return -1;
    }
    net_msg* pNetMsg = my_msg_new(toid, &coid, 1, pMsg);
    if (pNetMsg == NULL) {
        return -1;
    }
    return send_reconn_conn_msg(pLogic, pNetMsg);
}

int my_send_reconn_conn_msg_json( logic_thread* pLogic, thread_oid_t toid, conn_oid_t coid, CJsonMsg* pMsg )
{
	if (toid == invalid_thread_oid || coid == invalid_conn_oid) {
		return -1;
	}
	net_msg* pNetMsg = (net_msg*)msg_alloc(sizeof(host_msg_hd) + 1024);
	if (pNetMsg == NULL) 
	{
		return -1;
	}
	else
	{
		pNetMsg->m_hd.m_t = host_mt_net;
		pNetMsg->m_hd.m_threadOid = toid;
		pNetMsg->m_hd.m_connOid = coid;

		memcpy(pNetMsg->m_body, pMsg->szContext, 1024);
	}

	return send_reconn_conn_msg(pLogic, pNetMsg);
}


int my_send_reconn_conn_msg_block_chain( logic_thread* pLogic, thread_oid_t toid, conn_oid_t coid, CBlockChainMsg* pMsg )
{
	if (toid == invalid_thread_oid || coid == invalid_conn_oid) {
		return -1;
	}
	net_msg* pNetMsg = (net_msg*)msg_alloc( sizeof(host_msg_hd) +  sizeof(TCP_Head) + pMsg->sHead.TCPInfo.wPacketSize );
	if (pNetMsg == NULL) 
	{
		return -1;
	}
	else
	{
		pNetMsg->m_hd.m_t = host_mt_net;
		pNetMsg->m_hd.m_threadOid = toid;
		pNetMsg->m_hd.m_connOid = coid;

		memcpy(pNetMsg->m_body, pMsg,  sizeof(TCP_Head) + pMsg->sHead.TCPInfo.wPacketSize);
	}

	return send_reconn_conn_msg(pLogic, pNetMsg);
}

int my_multicast_listen_conn_msg( logic_thread* pLogic, thread_oid_t toid, const conn_oid_t* pCoids, uint16 count, CNetMsgHead* pMsg )
{
    if (toid == invalid_thread_oid || count == 0) {
        return -1;
    }
    net_msg* pNetMsg = my_msg_new(toid, pCoids, count, pMsg);
    if (pNetMsg == NULL) {
        return -1;
    }
    return send_listen_conn_msg(pLogic, pNetMsg);
}

parse_msg_relt my_io_recv_msg(bufferevent* pBuffer, net_conn* pConn, event_msgqueue* pInputQueue)
{
    for (;;) {
        evbuffer* pInputBuf = bufferevent_get_input(pBuffer);
		size_t L = evbuffer_get_length(pInputBuf);
        if (L < my_net_msg_hd_size) {
            return pmr_ok;
        }
        my_net_msg_hd msgHd;
        if (evbuffer_copyout(pInputBuf, &msgHd, my_net_msg_hd_size) != my_net_msg_hd_size) {
            return pmr_buffer_error;                
        }

        if (msgHd.m_nLenChk != msgHd.m_head.wLen || msgHd.m_head.byVar != GAME_PROTOCOL_VER) {
            return pmr_verify_failed;
        }
        if (msgHd.m_nLenChk < my_net_msg_hd_size-my_net_chk_size || 
            msgHd.m_nLenChk > my_max_msg_size) {
                return pmr_invalid_size;
        }

        size_t msgSize = msgHd.m_nLenChk + my_net_chk_size;
        if (evbuffer_get_length(pInputBuf) < msgSize) {
            return pmr_ok;
        }

        uint8* pMsg = msg_alloc(host_msg_hd_size + msgSize);

        net_msg* pHostMsg = (net_msg*)pMsg; 
        pHostMsg->m_hd.m_t = host_mt_net;
        pHostMsg->m_hd.m_threadOid = net_conn_get_thread_id(pConn);
        pHostMsg->m_hd.m_connOid = net_conn_get_conn_id(pConn);

        size_t readRm = bufferevent_read(pBuffer, &pHostMsg->m_body, msgSize);
        if (readRm != msgSize) {
            msg_free(pMsg);
            return pmr_buffer_error;
        }

		net_conn_recv_sec(pConn);

        my_net_msg_hd* pNetMsg = (my_net_msg_hd*)pHostMsg->m_body;
        //if(pNetMsg->m_head.wAssistantCmd == 100)
            //printf("racv msg byHostCmd = [ %d] wAssistantCmd=[%d] \n",pNetMsg->m_head.byHostCmd,pNetMsg->m_head.wAssistantCmd);
        if (event_msgqueue_push(pInputQueue, pHostMsg) != 0) {
            msg_free(pMsg);
            return pmr_push_failed;
        }
    }

    return pmr_ok;
}

void my_logic_handle_msg(const net_msg* pMsg, void* args)
{
    io_thread* pIoThread = (io_thread*)args;
    event_msgqueue* pOutputQueue = io_thread_output(pIoThread);
    if (event_msgqueue_push(pOutputQueue, (event_msg_t)pMsg) != 0) {
		printf("send msg to io_thread failed.\n");
        return ;
    }
}

void my_io_send_msg(const net_msg* pMsg, void* args)
{
    const host_msg_hd& hd = pMsg->m_hd;

    io_thread* pIoThread = (io_thread*)args;
    if (hd.m_threadOid != libevent_thread_id((libevent_thread*)pIoThread)) {
		printf("send_msg_hton failed(thread not match %d).\n", (uint32)hd.m_threadOid);
        return ;
    }
    net_conn_pool* pConnPool = io_thread_conn_pool(pIoThread);
    if (pConnPool == NULL) {
		printf("io_thread_conn_pool return NULL.\n");
        return ;
    }

    if (hd.m_t == host_mt_net) {
        my_net_msg_hd* pNetMsg = (my_net_msg_hd*)pMsg->m_body;
        if (net_conn_write(pConnPool, hd.m_connOid, pNetMsg, pNetMsg->m_nLenChk + my_net_chk_size) != 0) {
            //net_conn_disconn(pConnPool, hd.m_connOid);
            //net_conn_del(pConnPool, hd.m_connOid);
        }
    } else if (pMsg->m_hd.m_t == host_mt_muli_net) {
        uint16 nConnCount = hd.m_connOid;
        conn_oid_t* pCoids = (conn_oid_t*)pMsg->m_body;
        my_net_msg_hd* pNetMsg = (my_net_msg_hd*)(pCoids+nConnCount);

        for (uint16 i=0; i<nConnCount; ++i) {
            conn_oid_t coid = pCoids[i];
            if (net_conn_write(pConnPool, coid, pNetMsg, pNetMsg->m_nLenChk + my_net_chk_size) != 0) {
                //net_conn_disconn(pConnPool, hd.m_connOid);
                //net_conn_del(pConnPool, hd.m_connOid);
            }
        }
    }

}


parse_msg_relt my_io_recv_msg_json(bufferevent* pBuffer, net_conn* pConn, event_msgqueue* pInputQueue)
{
	for (;;) 
	{
		net_conn_recv_sec(pConn);

		evbuffer* pInputBuf = bufferevent_get_input(pBuffer);
		size_t L = evbuffer_get_length(pInputBuf);
		if (L < my_net_msg_hd_size) 
		{
			return pmr_ok;
		}

		my_net_msg_hd msgHd;
		if (evbuffer_copyout(pInputBuf, &msgHd, my_net_msg_hd_size) != my_net_msg_hd_size) 
		{
			return pmr_buffer_error;                
		}


		if (msgHd.m_nLenChk == msgHd.m_head.wLen && msgHd.m_head.byVar == GAME_PROTOCOL_VER) 
		{
			return my_io_recv_msg(pBuffer, pConn, pInputQueue);
		}
		

		size_t msgSize = 1024;
		if (L != msgSize) 
		{
			return pmr_ok;			
		}

		uint8* pMsg = msg_alloc(host_msg_hd_size + 1024);
		if (!pMsg)
		{
			return pmr_alloc_failed;
		}

		net_msg* pHostMsg = (net_msg*)pMsg; 
		pHostMsg->m_hd.m_t = host_mt_net;
		pHostMsg->m_hd.m_threadOid = net_conn_get_thread_id(pConn);
		pHostMsg->m_hd.m_connOid = net_conn_get_conn_id(pConn);

		size_t readRm = bufferevent_read(pBuffer, &pHostMsg->m_body, msgSize);
		if (readRm != msgSize) 
		{
			msg_free(pMsg);
			return pmr_buffer_error;
		}

		if (event_msgqueue_push(pInputQueue, pHostMsg) != 0) 
		{
			msg_free(pMsg);
			return pmr_push_failed;
		}
	}

	return pmr_ok;
}

void my_io_send_msg_json(const net_msg* pMsg, void* args)
{
	const host_msg_hd& hd = pMsg->m_hd;

	io_thread* pIoThread = (io_thread*)args;
	if (hd.m_threadOid != libevent_thread_id((libevent_thread*)pIoThread)) 
	{
		printf("send_msg_hton failed(thread not match %d).\n", (uint32)hd.m_threadOid);
		return;
	}

	net_conn_pool* pConnPool = io_thread_conn_pool(pIoThread);
	if (pConnPool == NULL) 
	{
		printf("io_thread_conn_pool return NULL.\n");
		return;
	}

	if (hd.m_t == host_mt_net) 
	{
		my_net_msg_hd* pNetMsg = (my_net_msg_hd*)pMsg->m_body;
		if (pNetMsg->m_nLenChk == pNetMsg->m_head.wLen && pNetMsg->m_head.byVar == GAME_PROTOCOL_VER)
		{
			return my_io_send_msg(pMsg, args);
		}
		else
		{
			net_conn_write(pConnPool, hd.m_connOid, (char*)pMsg->m_body, 1024);
		}
	} 
	else
	{
		return my_io_send_msg(pMsg, args);
	}
}





















//for block chain 数据包头 同 loginsvr  cooker 2018-01-15 17:52:20
parse_msg_relt my_io_recv_msg_block_chain(bufferevent* pBuffer, net_conn* pConn, event_msgqueue* pInputQueue)
{
	for (;;) 
	{
		net_conn_recv_sec(pConn);//更新连接的数据接收状态 防止超时被踢掉

		//长度验证
		evbuffer* pInputBuf = bufferevent_get_input(pBuffer);
		size_t L = evbuffer_get_length(pInputBuf);
		if (L < sizeof(TCP_Head)) 
		{
			return pmr_ok;
		}

		//头部拷贝
		TCP_Head msgHd;
		if (evbuffer_copyout(pInputBuf, &msgHd, sizeof(TCP_Head)) != sizeof(TCP_Head)) 
		{
			return pmr_buffer_error;                
		}

		//分配内存
		uint8* pMsg = msg_alloc(sizeof(TCP_Head) + msgHd.TCPInfo.wPacketSize);
		if (!pMsg)
		{
			return pmr_alloc_failed;
		}


		net_msg* pHostMsg = (net_msg*)pMsg; 
		pHostMsg->m_hd.m_t = host_mt_net;
		pHostMsg->m_hd.m_threadOid = net_conn_get_thread_id(pConn);
		pHostMsg->m_hd.m_connOid = net_conn_get_conn_id(pConn);

		size_t readRm = bufferevent_read(pBuffer, &pHostMsg->m_body, sizeof(TCP_Head) + msgHd.TCPInfo.wPacketSize);
		if (readRm != sizeof(TCP_Head) + msgHd.TCPInfo.wPacketSize) 
		{
			msg_free(pMsg);
			return pmr_buffer_error;
		}

		if (event_msgqueue_push(pInputQueue, pHostMsg) != 0) 
		{
			msg_free(pMsg);
			return pmr_push_failed;
		}
	}

	return pmr_ok;
}

//for block chain 数据包头 同 loginsvr  cooker 2018-01-15 17:52:20
void my_io_send_msg_block_chain(const net_msg* pMsg, void* args)
{
	const host_msg_hd& hd = pMsg->m_hd;

	io_thread* pIoThread = (io_thread*)args;
	if (hd.m_threadOid != libevent_thread_id((libevent_thread*)pIoThread)) 
	{
		printf("send_msg_hton failed(thread not match %d).\n", (uint32)hd.m_threadOid);
		return;
	}

	net_conn_pool* pConnPool = io_thread_conn_pool(pIoThread);
	if (pConnPool == NULL) 
	{
		printf("io_thread_conn_pool return NULL.\n");
		return;
	}

	if (hd.m_t == host_mt_net) 
	{
		TCP_Head* pNetMsg = (TCP_Head*)pMsg->m_body;

		net_conn_write(pConnPool, hd.m_connOid, (char*)pMsg->m_body, sizeof(TCP_Head) + pNetMsg->TCPInfo.wPacketSize);

	} 
}