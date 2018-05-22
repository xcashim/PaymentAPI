#ifndef MSG_QUEUE_H
#define MSG_QUEUE_H

#include <event2/event.h>

#include "TypeDefine.h"

typedef void* event_msg_t;

#define event_msg_size sizeof(event_msg_t)

struct event_msgqueue;

event_msgqueue* event_msgqueue_new();
void event_msgqueue_del(event_msgqueue* pMsgQueue);

typedef void (*fn_event_msg_cb)(const event_msg_t msg, void* args);
int event_msgqueue_init(event_msgqueue* pMsgQueue, size_t maxSize, event_base* pEvBase, fn_event_msg_cb cb, void* args);
int event_msgqueue_fini(event_msgqueue* pMsgQueue);
int event_msgqueue_destory(event_msgqueue* pMsgQueue);
int event_msgqueue_push(event_msgqueue* pMsgQueue, const event_msg_t msg);

#endif // MSG_QUEUE_H