#ifndef LOGIC_THREAD_H
#define LOGIC_THREAD_H

#include <string>
#include "TypeDefine.h"

struct logic_thread;

logic_thread* logic_thread_new(const net_setting& setting);
void logic_thread_del(logic_thread* pThread);
int logic_thread_run(logic_thread* pThread);
int logic_thread_stop(logic_thread* pThread);

struct libevent_thread;
libevent_thread* logic_thread_getbase(logic_thread* pThread);

class reconn_event;
class reconn_session;
int logic_thread_add_reconn(logic_thread* pThread, reconn_session* pSession, 
    const std::string& strIp, uint16 nPort, int iSecs, uint32 maxBuffer);

struct net_msg;
int send_listen_conn_msg(logic_thread* pThread, net_msg* pNetMsg);
int send_reconn_conn_msg(logic_thread* pThread, net_msg* pNetMsg);

class logic_interface;
void reg_logic_interface_listen(logic_thread* pThread, logic_interface* pLogicIF);
void reg_logic_interface_reconn(logic_thread* pThread, logic_interface* pLogicIF);

#endif // LOGIC_THREAD_H