#ifndef MSG_PARSER_H
#define MSG_PARSER_H

#include "MsgInterface.h"
#include "TypeDefine.h"

struct bufferevent;
struct net_conn;
struct event_msgqueue;
struct logic_thread;

//////////////////////////////////////////////////////////////////////////
#pragma pack( push )
#pragma pack( 1 )

class CNetMsgHead
{
public:
    int byVar;
    int wLen;
    int byHostCmd;
    int wAssistantCmd;
    CNetMsgHead()
    {
        byVar = 0x00;
        wLen = 0;
        byHostCmd = 0x00;
        wAssistantCmd = 0x00;
    }
    int	GetHeadSize(){ return sizeof(CNetMsgHead); };
    inline void CleanMsg(){};
    inline void RevertMsg(){};
    uint16 GetMsgLen() { return wLen; }
};

#pragma pack( pop )


//struct for blockchain

//
struct TCP_Info
{
	BYTE							cbDataKind;							//
	BYTE							cbCheckCode;						//
	WORD							wPacketSize;						//
};

//
struct TCP_Command
{
	WORD							wMainCmdID;							//
	WORD							wSubCmdID;							//
};

//
struct TCP_Head
{
	TCP_Info						TCPInfo;							//
	TCP_Command						CommandInfo;						//
};


//
class CJsonMsg
{
public:
	char szContext[1024];
	CJsonMsg()
	{
		memset(szContext, 0, sizeof(szContext));
	}
};

class CBlockChainMsg
{
public:
	TCP_Head sHead;
	char szContext[1024];
	//int nSize;
	CBlockChainMsg()
	{
		memset(this, 0, sizeof(CBlockChainMsg));
	}
};

#define BEGIN_MAKE_MSG_CLASS( HostCmd , x ) class CMsg_##x : public CNetMsgHead \
{\
public:\
    CMsg_##x() {\
    byVar = GAME_PROTOCOL_VER;\
    wLen = sizeof( CMsg_##x );\
    byHostCmd = HostCmd;\
    wAssistantCmd = x;  } \
    CMsg_##x& operator=( const CMsg_##x& v ) {\
    memcpy(this, &v,sizeof(CMsg_##x) );\
    return *this; }\
    void* GetData() {\
    return (void*)this; }
#define END_MAKE_MSG_CLASS( HostCmd, x ) }; 
//////////////////////////////////////////////////////////////////////////

#pragma pack(1)
struct my_net_msg_hd
{
    uint16 m_nLenChk;
    CNetMsgHead m_head;
};
#pragma pack()

#define logic_msg(msg_ptr)     (CNetMsgHead*)&(((my_net_msg_hd*)msg_ptr)->m_head)
#define my_net_msg_hd_size     sizeof(my_net_msg_hd)
#define my_msg_hd_size         host_msg_hd_size + my_net_msg_hd_size
#define my_net_chk_size        2
#define my_max_msg_size        0xffff
#define my_msg_max_len         50000

int my_send_listen_conn_msg(logic_thread* pLogic, thread_oid_t toid, conn_oid_t coid, CNetMsgHead* pMsg);
int my_send_reconn_conn_msg(logic_thread* pLogic, thread_oid_t toid, conn_oid_t coid, CNetMsgHead* pMsg);
int my_multicast_listen_conn_msg(logic_thread* pLogic, thread_oid_t toid, const conn_oid_t* pCoids, uint16 count, CNetMsgHead* pMsg);

parse_msg_relt my_io_recv_msg(bufferevent* pBuffer, net_conn* pConn, event_msgqueue* pInputQueue);
void my_logic_handle_msg(const net_msg* pMsg, void* args);
void my_io_send_msg(const net_msg* pMsg, void* args);


parse_msg_relt my_io_recv_msg_json(bufferevent* pBuffer, net_conn* pConn, event_msgqueue* pInputQueue);
void my_io_send_msg_json(const net_msg* pMsg, void* args);
int my_send_reconn_conn_msg_json(logic_thread* pLogic, thread_oid_t toid, conn_oid_t coid, CJsonMsg* pMsg);

parse_msg_relt my_io_recv_msg_block_chain(bufferevent* pBuffer, net_conn* pConn, event_msgqueue* pInputQueue);
void my_io_send_msg_block_chain(const net_msg* pMsg, void* args);
int my_send_reconn_conn_msg_block_chain(logic_thread* pLogic, thread_oid_t toid, conn_oid_t coid, CBlockChainMsg* pMsg);




#endif // MSG_PARSER_H
