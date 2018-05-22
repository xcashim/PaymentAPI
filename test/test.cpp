#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <signal.h>
#ifndef WIN32
#include <netinet/in.h>
# ifdef _XOPEN_SOURCE_EXTENDED
#  include <arpa/inet.h>
# endif
#include <sys/socket.h>
#endif
#include <signal.h>
#include <event2/bufferevent.h>
#include <event2/buffer.h>
#include <event2/listener.h>
#include <event2/util.h>
#include <event2/event.h>
#include "queue.h"
//////////////////////////////////////////////////////////////////////////

#include "spthread.h"
//////////////////////////////////////////////////////////////////////////
#ifdef WIN32
#define LOG_DEBUG 1
#define PRINTF(LEVEL, FORMAT, PARAM) printf(FORMAT, PARAM)
#else
#include <syslog.h>
#define PRINTF(LEVEL, FORMAT, PARAM) syslog(LEVEL, FORMAT, PARAM)
#endif
#ifdef WIN32
#define sleep Sleep
#else
#define sleep usleep
#endif

////////////////////////////////////////////////////////////////////////////
//// global
////////////////////////////////////////////////////////////////////////////
//struct client {
//    int fd;
//    struct bufferevent *buf_ev;
//    TAILQ_ENTRY(client) entries;
//};
//TAILQ_HEAD(, client) client_tailq_head;
//
//evbuffer* g_inputBuffer;
//evbuffer* g_outputBuffer;
//
//unsigned int g_recvCount = 0;
//unsigned int g_sendCount = 0;
//unsigned int g_recvCountH = 0;
//unsigned int g_acceptCount = 0;
//
////////////////////////////////////////////////////////////////////////////
//sp_thread_result_t SP_THREAD_CALL RecvThread( void * arg );
//sp_thread_result_t SP_THREAD_CALL SendThread( void * arg );
//void CreateRecvThread();
//void CreateSendThread();
//
//typedef unsigned char BYTE;
//typedef unsigned short WORD;
//
//#pragma pack(1)
//struct Msg
//{
//    BYTE byVar;
//    WORD wLen;
//    BYTE byHostCmd;
//    WORD wAssistantCmd;
//    char msg[6];
//};
//
//
//#define MSG_LEN sizeof(Msg)
//
//struct MsgEntery
//{
//    client* m_pClient;
//    Msg* m_pMsg;
//};
//#pragma pack()
//static const int PORT = 9995;
//
//static void listener_cb(struct evconnlistener *, evutil_socket_t,
//struct sockaddr *, int socklen, void *);
//static void conn_readcb(struct bufferevent *bev, void *user_data);
//static void conn_writecb(struct bufferevent *, void *);
//static void conn_eventcb(struct bufferevent *, short, void *);
//static void signal_cb(evutil_socket_t, short, void *);
//static void my_event_fatal_cb(int err)
//{
//    PRINTF(LOG_DEBUG, "event_fatal_cb(%d)", err);
//}
////////////////////////////////////////////////////////////////////////////
//
//sp_thread_result_t SP_THREAD_CALL RecvThread( void * arg )
//{
//    struct event_base *base;
//    struct evconnlistener *listener;
//    struct event *signal_event;
//
//    struct sockaddr_in sin;
//
//    base = event_base_new();
//    if (!base) {
//        PRINTF(LOG_DEBUG, "[%s] Could not initialize libevent!\n", __FUNCTION__);
//        return 0;
//    }
//
//    memset(&sin, 0, sizeof(sin));
//    sin.sin_family = AF_INET;
//    sin.sin_port = htons(PORT);
//
//    listener = evconnlistener_new_bind(base, listener_cb, (void *)base,
//        LEV_OPT_REUSEABLE|LEV_OPT_CLOSE_ON_FREE, -1,
//        (struct sockaddr*)&sin,
//        sizeof(sin));
//
//    if (!listener) {
//        PRINTF(LOG_DEBUG, "[%s]Could not create a listener!\n", __FUNCTION__);
//        return 0;
//    }
//
//    signal_event = evsignal_new(base, SIGINT, signal_cb, (void *)base);
//
//    if (!signal_event || event_add(signal_event, NULL)<0) {
//        PRINTF(LOG_DEBUG, "[%s]Could not create/add a signal event!\n", __FUNCTION__);
//        return 0;
//    }
//
//    event_set_fatal_callback(my_event_fatal_cb);
//
//    PRINTF(LOG_DEBUG, "listen....", 1);
//    event_base_dispatch(base);
//
//    evconnlistener_free(listener);
//    event_free(signal_event);
//    event_base_free(base);
//
//    PRINTF(LOG_DEBUG, "[%s]done\n", __FUNCTION__);
//    return 0;
//}
//
//void CreateRecvThread()
//{
//    sp_thread_t threadId = 0;
//    sp_thread_attr_t attr;
//    sp_thread_attr_init( &attr );
//    sp_thread_attr_setdetachstate( &attr, SP_THREAD_CREATE_DETACHED );
//
//    if( 0 == sp_thread_create( &threadId, &attr, RecvThread, NULL ) ) {
//        PRINTF(LOG_DEBUG,  "create recv_thread #%ld\n", threadId );
//    } else {
//        PRINTF(LOG_DEBUG,  "cannot create recv_thread\n", 1 );
//    }
//}
//
//sp_thread_result_t SP_THREAD_CALL SendThread( void * arg )
//{
//    // create event_base
//    struct event_base *base;
//    base = event_base_new();
//
//    // send
//    for (;;)
//    {
//        sleep(10);
//        MsgEntery msgEntry;
//        size_t len = evbuffer_get_length(g_outputBuffer);
//        if (len <= 0) {
//            continue;
//        }
//
//        int iReadLen = evbuffer_remove(g_outputBuffer, &msgEntry, sizeof(msgEntry));
//        if (iReadLen <= 0) {
//            PRINTF(LOG_DEBUG, "[%s] get msg failed!\n", __FUNCTION__);
//        }
//
//        bufferevent* pBuffEvent = msgEntry.m_pClient->buf_ev;
//        if (pBuffEvent == NULL) {
//            continue;
//        }
//
//        //bufferevent_setcb(pBuffEvent, NULL, conn_writecb, NULL, NULL);
//        //bufferevent_enable(bev, EV_READ);
//        //bufferevent_enable(pBuffEvent, EV_WRITE);
//        bufferevent_write(pBuffEvent, msgEntry.m_pMsg, MSG_LEN);
//        delete msgEntry.m_pMsg;
//
//        PRINTF(LOG_DEBUG, "send one packet(%d).\n", ++g_sendCount);
//    }
//
//    // free event_base
//    event_base_free(base);
//}
//void CreateSendThread()
//{
//    sp_thread_t threadId = 0;
//    sp_thread_attr_t attr;
//    sp_thread_attr_init( &attr );
//    sp_thread_attr_setdetachstate( &attr, SP_THREAD_CREATE_DETACHED );
//
//    if( 0 == sp_thread_create( &threadId, &attr, SendThread, NULL ) ) {
//        PRINTF(LOG_DEBUG,  "create send_thread #%ld\n", threadId );
//    } else {
//        PRINTF(LOG_DEBUG,  "cannot create send_thread\n",1 );
//    }
//}
//
////////////////////////////////////////////////////////////////////////////
//// main thread
////////////////////////////////////////////////////////////////////////////
//int MainThread()
//{
//#ifdef WIN32
//    WSADATA wsa_data;
//    WSAStartup(0x0201, &wsa_data);
//#else
//    struct sigaction sa;
//    sa.sa_handler = SIG_IGN;
//    sigaction(SIGPIPE, &sa, 0);
//#endif
//
//    g_inputBuffer = evbuffer_new();
//    g_outputBuffer = evbuffer_new();
//    evbuffer_enable_locking(g_inputBuffer, NULL);
//    evbuffer_enable_locking(g_outputBuffer, NULL);
//
//    TAILQ_INIT(&client_tailq_head);
//
//    CreateRecvThread();
//    //CreateSendThread();
//
//    for(;;)
//    {
//        sleep(10);
//        //size_t len = evbuffer_get_length(g_inputBuffer);
//        //if (len <= 0) {
//        //    continue;
//        //}
//        //MsgEntery msgEntry;
//        //int iReadLen = evbuffer_remove(g_inputBuffer, &msgEntry, sizeof(msgEntry));
//        //if (iReadLen <= 0) {
//        //    PRINTF(LOG_DEBUG, "[%s] get msg failed!\n", __FUNCTION__);
//        //}
//        //// TODO
//        //evbuffer_add(g_outputBuffer, &msgEntry, sizeof(msgEntry));
//    }
//
//    evbuffer_free(g_inputBuffer);
//    evbuffer_free(g_outputBuffer);
//}
//
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
//
//static void
//listener_cb(struct evconnlistener *listener, evutil_socket_t fd,
//    struct sockaddr *sa, int socklen, void *user_data)
//{
//	struct event_base *base = (event_base*)user_data;
//	struct bufferevent *bev;
//
//	bev = bufferevent_socket_new(base, fd, BEV_OPT_CLOSE_ON_FREE);
//	if (!bev) {
//		PRINTF(LOG_DEBUG, "[%s]Error constructing bufferevent!", __FUNCTION__);
//		event_base_loopbreak(base);
//		return;
//	}
//
//    client* pNewClient = new client;
//    pNewClient->fd = fd;
//    pNewClient->buf_ev = bev;
//    //bufferevent_setwatermark(bev, EV_READ, MSG_LEN, 0);
//	bufferevent_setcb(bev, conn_readcb, conn_writecb, conn_eventcb, pNewClient);
//    bufferevent_enable(bev, EV_READ);
//    bufferevent_enable(bev, EV_WRITE);
//
//    // TODO
//    TAILQ_INSERT_TAIL(&client_tailq_head, pNewClient, entries);
//	//bufferevent_enable(bev, EV_WRITE);
//	//bufferevent_disable(bev, EV_READ);
//
//    ++g_acceptCount;
//    //PRINTF(LOG_DEBUG, "accept new client(%u)\n", g_acceptCount);
//}
//
//timeval g_lastTime;
//
//static void
//conn_readcb(struct bufferevent *bev, void *user_data)
//{
//    //PRINTF(LOG_DEBUG, "oneread\n", 1);
//    client* pClient = (client*)user_data;
//    if (pClient == NULL) {
//        return ;
//    }
//    int iCount = 0;
//    while(true)
//    {
//        struct evbuffer *output = bufferevent_get_input(bev);
//        size_t len = evbuffer_get_length(output);
//        if (len >= MSG_LEN) {
//            
//            if (g_recvCount == 0)
//            {
//                evutil_gettimeofday(&g_lastTime,NULL);
//            }
//            ++g_recvCount;
//            if (g_recvCount % 1000 == 0)
//            {
//                ++g_recvCountH;
//                timeval curTime;
//                evutil_gettimeofday(&curTime,NULL);
//
//                timeval diffTime;
//                evutil_timersub(&curTime, &g_lastTime, &diffTime);
//                
//                char szTemp[100] = {0};
//                sprintf(szTemp, "(100)(%u)dt_s(%u)(%u).\n", g_recvCountH, diffTime.tv_sec, diffTime.tv_usec);
//                PRINTF(LOG_DEBUG, szTemp, 1);
//
//                g_lastTime = curTime;
//            }
//            
//            //PRINTF(LOG_DEBUG, "recv one packet(%d).\n", g_recvCount);
//            MsgEntery entry;
//            entry.m_pClient = pClient;
//            entry.m_pMsg = new Msg;
//            bufferevent_read(bev, entry.m_pMsg, MSG_LEN);
//            //for (int i = 0; i < 10; ++i)
//            {   
//                bufferevent_write(bev,entry.m_pMsg, MSG_LEN);
//            }
//            //evbuffer_add(g_inputBuffer, &entry, sizeof(entry));
//            delete entry.m_pMsg;
//            entry.m_pMsg = NULL;
//            ++iCount;
//        }
//        else
//        {
//            //int len = evbuffer_get_length(output);
//            //char *pNew = new char[len];
//            //bufferevent_read(bev, pNew, len);
//            //bufferevent_write(bev, pNew, len);
//            //delete[] pNew;
//            break;
//        }
//    }
//    //PRINTF(LOG_DEBUG, "onereadcount(%d)\n", iCount);
//}
//
//static void
//conn_writecb(struct bufferevent *bev, void *user_data)
//{
//    int i = 0;
//    //TODO:
//    //struct evbuffer *output = bufferevent_get_output(bev);
//    //if (evbuffer_get_length(output) == 0) {
//    //    printf("flushed answer\n");
//    //}
//}
//
//static void
//conn_eventcb(struct bufferevent *bev, short events, void *user_data)
//{
//    PRINTF(LOG_DEBUG, "disconnect\n", 1);
//    client* pClient = (client*)user_data;
//    if (pClient == NULL) {
//        return ;
//    }
//    /* Remove the client from the tailq. */
//    TAILQ_REMOVE(&client_tailq_head, pClient, entries);
//
//	//if (events & BEV_EVENT_EOF) {
//	//	printf("Connection closed.\n");
//	//} else if (events & BEV_EVENT_ERROR) {
//	//	printf("Got an error on the connection: %s\n",
//	//	    strerror(errno));/*XXX win32*/
//	//}
//	///* None of the other events can happen here, since we haven't enabled
//	// * timeouts */
//    bufferevent_free(pClient->buf_ev);
//    evutil_closesocket(pClient->fd);
//    delete pClient;
//}
//
//static void
//signal_cb(evutil_socket_t sig, short events, void *user_data)
//{
//	struct event_base *base = (event_base*)user_data;
//	struct timeval delay = { 2, 0 };
//
//	PRINTF(LOG_DEBUG, "Caught an interrupt signal; exiting cleanly in two seconds.\n", 1);
//
//	event_base_loopexit(base, &delay);
//}
