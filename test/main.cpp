#include <iostream>

#include "NetInclude.h"
#include "MsgParser.h"

#pragma pack(1)
struct TestMsg
{
    host_msg_hd m_hd;
    WORD wLenW;
    BYTE byVar;
    WORD wLen;
    BYTE byHostCmd;
    WORD wAssistantCmd;
    char msg[6];
    TestMsg()
    {
        wLenW = 12;
        byVar = 8;
        wLen = 12;
        byHostCmd = 9;
        wAssistantCmd = 10;
        strncpy(msg, "hello", 6);
    }
};
#pragma pack()

class send_event : public time_event
{
public:
    send_event(logic_thread* pThread, bool bPersist, int iMilliSecs, reconn_event* pReconn)
        : time_event(logic_thread_getbase(pThread), bPersist, iMilliSecs)
        , m_pLogic(pThread)
        , m_pReconn(pReconn)
        , m_iCount(0)
    {
    }
    virtual void trigger()
    {
        if (m_iCount >= 100000) {
            return ;
        }
        if (m_pReconn->is_connected()) {
            for (int i = 0; i < 5000; ++i) {
                TestMsg* pMsg = new TestMsg;
                send_reconn_conn_msg(m_pLogic, 0, 0, (net_msg*)pMsg);      
            }
            m_iCount += 5000;
        }
    }
private:
    logic_thread* m_pLogic;
    reconn_event* m_pReconn;
    int m_iCount;
};

void id_dump(id_alloctor* pAlloc)
{
    std::stringstream stream;
    id_alloctor_dump(pAlloc, &stream);
    std::cout<<stream.str().c_str()<<std::endl;
}

void test_id_alloc()
{
    id_alloctor* pAlloc = id_alloctor_new(5);
    id_dump(pAlloc);

    for (uint16 i = 0; i < 6; ++i) {
        uint16 id = id_alloctor_alloc(pAlloc);
        id_dump(pAlloc);            
    }

    id_alloctor_free(pAlloc, 3);
    id_dump(pAlloc);
    
    id_alloctor_free(pAlloc, 0);
    id_dump(pAlloc);

    id_alloctor_free(pAlloc, 1);
    id_dump(pAlloc);

    id_alloctor_free(pAlloc, 4);
    id_dump(pAlloc);

    id_alloctor_free(pAlloc, 2);
    id_dump(pAlloc);

    for (uint16 i = 0; i < 6; ++i) {
        uint16 id = id_alloctor_alloc(pAlloc);
        std::cout<<"alloc:"<<id<<std::endl;
        id_dump(pAlloc);
    }
    id_dump(pAlloc);
}


int main(int argc, char **argv)
{
    net_setting setting;
    setting.m_nListenPort = 0;
    setting.m_nIoThreads = 0;
    setting.m_maxConnsOfThread = 0;
    setting.m_maxReConnOfThread = 1;
    setting.m_nReConnIoThreads = 1;
    logic_thread* pLogic = logic_thread_new(setting);

    reconn_event* pReconn = create_reconn(pLogic, "10.0.1.122", 9990, 3);

    send_event* pSendEv = new send_event(pLogic, true, 1000, pReconn);
    schedule_event(pSendEv);

    regfn_io_recv_msg(my_io_recv_msg);
    regfn_logic_handle_msg(my_logic_handle_msg);
    regfn_io_send_msg(my_io_send_msg);

    logic_thread_run(pLogic);

    delete pSendEv; pSendEv = NULL;
    delete pReconn; pReconn = NULL;
    logic_thread_del(pLogic);

    system("pause");
    return 0;
}
