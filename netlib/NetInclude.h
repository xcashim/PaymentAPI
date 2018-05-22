#ifndef NET_INCLUDE_H
#define NET_INCLUDE_H

#include <event2/bufferevent.h>
#include <event2/buffer.h>

#include "TypeDefine.h"
//#include "spthread.h"
#include "IdAlloctor.h"

#include "ListenThread.h"
#include "LogicThread.h"

#include "MsgQueue.h"
#include "MsgInterface.h"

#include "NetConnection.h"

#include "TimeEvent.h"
#include "ReConnEvent.h"

#include "LogicInterface.h"
#include "MsgParser.h"

#define GAME_PROTOCOL_VER 8

#endif // NET_INCLUDE_H
