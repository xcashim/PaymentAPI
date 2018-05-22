#include "MsgQueue.h"

#include <stdlib.h>

#include <event2/event_struct.h>
#include <event2/event.h>
#include <event2/bufferevent.h>
#include <event2/buffer.h>
#include <event2/listener.h>
#include <event2/util.h>
#include <event2/event.h>

#include "eloger_usefull.h"
#include "spthread.h"

//////////////////////////////////////////////////////////////////////////

struct socket_pair_wrapper
{
    event m_event;
    evutil_socket_t m_fdWrite;
    evutil_socket_t m_fdRead;
};

inline int socket_pair_write(socket_pair_wrapper* pSockPair)
{
	//printf("m_fdWrite=%d\n", pSockPair->m_fdWrite);
    return send(pSockPair->m_fdWrite, "", 1, 0);      
	
}

inline void socket_pair_read(socket_pair_wrapper* pSockPair)
{
    unsigned char buf[1024];
    while (recv(pSockPair->m_fdRead, (char*)buf, sizeof(buf), 0) > 0) {
    }
}

static int socket_pair_init(socket_pair_wrapper* pSockPair, 
    event_base* pEvBase, event_callback_fn fn, void* args)
{
    evutil_socket_t fds[2];
    if (evutil_socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == -1) {
        return -1;
    }

    pSockPair->m_fdWrite = fds[0];
    pSockPair->m_fdRead = fds[1];

    evutil_make_socket_nonblocking(pSockPair->m_fdWrite);
    evutil_make_socket_nonblocking(pSockPair->m_fdRead);

    event_assign(&pSockPair->m_event, pEvBase, pSockPair->m_fdRead,
        EV_READ|EV_PERSIST, fn, args);

    event_add(&pSockPair->m_event, NULL);

    return 0;
}

static int socket_pair_fini(socket_pair_wrapper* pSockPair)
{
    event_del(&pSockPair->m_event);

    if (pSockPair->m_fdRead != -1) {
        evutil_closesocket(pSockPair->m_fdRead);
        pSockPair->m_fdRead = -1;
    }
    if (pSockPair->m_fdWrite != -1) {
        evutil_closesocket(pSockPair->m_fdWrite);
        pSockPair->m_fdWrite = -1;
    }
    
    return 0;
}

//////////////////////////////////////////////////////////////////////////

/************************************************************************/
typedef void* cq_elem_t;

/************************************************************************/
struct circle_queue {
    uint32 m_head;
    uint32 m_tail;
    uint32 m_cur_count;
    uint32 m_max_count;
    cq_elem_t m_elems[1];
};

/************************************************************************/
static circle_queue* circle_queue_new(uint32 max_count)
{
    if (max_count == 0) {
        return NULL; 
    }
    circle_queue* cq = (circle_queue*)calloc(1, 
        sizeof(circle_queue) + sizeof(cq_elem_t) * (max_count - 1) );//2016-10-10
    if (cq == NULL) {
        return NULL;
    }

    cq->m_head = 0;
    cq->m_tail = 0;
    cq->m_cur_count = 0;
    cq->m_max_count = max_count;

    return cq;
}

/************************************************************************/
static int circle_queue_del(circle_queue* cq)
{
    free(cq);
    return 0;
}

/************************************************************************/
static int circle_queue_push(circle_queue* cq, const cq_elem_t elem)
{
    if (cq->m_cur_count >= cq->m_max_count) {
        return -1;
    }

    cq->m_elems[cq->m_tail++] = elem;
    if (cq->m_tail >= cq->m_max_count) {
        cq->m_tail = 0;
    }
    ++cq->m_cur_count;
    return 0;
}

/************************************************************************/
static cq_elem_t circle_queue_pop(circle_queue* cq)
{
    if (cq->m_cur_count == 0) {
        return NULL;
    }
    cq_elem_t elem = cq->m_elems[cq->m_head++];
    if (cq->m_head >= cq->m_max_count) {
        cq->m_head = 0;
    }
    --cq->m_cur_count;
    return elem;
}

/************************************************************************/
static inline uint32 circle_queue_size(circle_queue* cq)
{
    return cq->m_cur_count;
}

/************************************************************************/
static inline bool circle_queue_is_empty( circle_queue* cq )
{
    return circle_queue_size(cq) == 0;
}

/************************************************************************/
static inline bool circle_queue_is_full( circle_queue* cq )
{
    return circle_queue_size(cq) == cq->m_max_count;
}

static inline int circle_queue_clear(circle_queue* cq)
{
    cq->m_head = 0;
    cq->m_tail = 0;
    cq->m_cur_count = 0;

	return 0;
}

//////////////////////////////////////////////////////////////////////////

struct event_msgqueue 
{
    circle_queue* m_pBuffer;

    sp_thread_mutex_t m_lock;

    sp_thread_cond_t m_condFull;

    socket_pair_wrapper m_socketPair;
    
    fn_event_msg_cb m_fnMsgCb;

    void* m_pCbArgs;
};

event_msgqueue* event_msgqueue_new()
{
    return new event_msgqueue;
}
void event_msgqueue_del(event_msgqueue* pMsgQueue)
{
    delete pMsgQueue;
}

static void event_msgqueue_pop(evutil_socket_t fd, short events, void* args);

int event_msgqueue_init(event_msgqueue* pMsgQueue, size_t maxSize,
    event_base* pEvBase, fn_event_msg_cb cb, void* args)
{
    pMsgQueue->m_pBuffer = circle_queue_new(maxSize);
    if (pMsgQueue->m_pBuffer == NULL) {
        return -1;
    }
    if (socket_pair_init(&pMsgQueue->m_socketPair, pEvBase, event_msgqueue_pop, (void*)pMsgQueue) != 0) {
        return -1;
    }
    sp_thread_mutex_init(&pMsgQueue->m_lock, NULL);
    sp_thread_cond_init(&pMsgQueue->m_condFull, NULL);

    pMsgQueue->m_fnMsgCb = cb;
    pMsgQueue->m_pCbArgs = args;

    return 0;
}

int event_msgqueue_fini(event_msgqueue* pMsgQueue)
{
    circle_queue_del(pMsgQueue->m_pBuffer);
    pMsgQueue->m_pBuffer = NULL;

    socket_pair_fini(&pMsgQueue->m_socketPair);
    
    sp_thread_mutex_destroy(&pMsgQueue->m_lock);
    sp_thread_cond_destroy(&pMsgQueue->m_condFull);

    pMsgQueue->m_fnMsgCb = NULL;
    pMsgQueue->m_pCbArgs = NULL;

    return 0;
}

int event_msgqueue_destory( event_msgqueue* pMsgQueue )
{
    if (pMsgQueue) {
        event_msgqueue_fini(pMsgQueue);
        event_msgqueue_del(pMsgQueue);
    }
    return 0;
}

int event_msgqueue_push(event_msgqueue* pMsgQueue, const event_msg_t msg)
{
    sp_thread_mutex_lock(&pMsgQueue->m_lock);

    //while (circle_queue_is_full(pMsgQueue->m_pBuffer)) {
    //    uint32 size = circle_queue_size(pMsgQueue->m_pBuffer);
    //    ELOG((ELOG_ERR, "msg_queue_size=%u", size));
    //    sp_thread_cond_wait(&pMsgQueue->m_condFull, &pMsgQueue->m_lock);
    //}

    if (circle_queue_is_full(pMsgQueue->m_pBuffer))
    {
        uint32 size = circle_queue_size(pMsgQueue->m_pBuffer);
        printf("push msg_queue_size=%u\n", size);

        circle_queue_clear(pMsgQueue->m_pBuffer);
    }

    if (circle_queue_push(pMsgQueue->m_pBuffer, msg) != 0) {
        sp_thread_mutex_unlock(&pMsgQueue->m_lock);
        return -1;
    }

    if (circle_queue_size(pMsgQueue->m_pBuffer) == 1) {
        socket_pair_write(&pMsgQueue->m_socketPair);
    }

    sp_thread_mutex_unlock(&pMsgQueue->m_lock);
    return 0;
}

static void event_msgqueue_pop(evutil_socket_t fd, short events, void* args)
{
    event_msgqueue* pMsgQueue = (event_msgqueue*)args;

    socket_pair_read(&pMsgQueue->m_socketPair);

    sp_thread_mutex_lock(&pMsgQueue->m_lock);

    //uint32 size = circle_queue_size(pMsgQueue->m_pBuffer);
    //printf("pop msg_queue_size=%u\n", size);
    
    //int  a = 0;
    while(circle_queue_size(pMsgQueue->m_pBuffer) >= 1) {
        event_msg_t msg = circle_queue_pop(pMsgQueue->m_pBuffer);
        //sp_thread_cond_signal(&pMsgQueue->m_condFull);
        //sp_thread_mutex_unlock(&pMsgQueue->m_lock);

        (*pMsgQueue->m_fnMsgCb)(msg, pMsgQueue->m_pCbArgs);

        //sp_thread_mutex_lock(&pMsgQueue->m_lock);

        //a++;
        //if( a>= 10000)
        //{
        //    sp_sleep(10);
        //    a = 0;
        //}
    }

    //sp_thread_cond_signal(&pMsgQueue->m_condFull);
    sp_thread_mutex_unlock(&pMsgQueue->m_lock);
    
}
