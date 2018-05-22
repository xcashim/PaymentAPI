#ifndef TYPE_DEFINE_H
#define TYPE_DEFINE_H

#include <stdio.h>
#include <string.h>
//#ifdef WIN32
//typedef unsigned long  DWORD;
//#else
//typedef unsigned int DWORD;
//#endif
//
//typedef unsigned long  UL_PTR;
//typedef int            BOOL;
//typedef unsigned char  BYTE;
//typedef unsigned short WORD;
//
//typedef signed char         int8;
//typedef unsigned char       uint8;
//typedef signed short int    int16;
//typedef unsigned short int  uint16;
//typedef int                 int32;
//typedef unsigned int        uint32;
//
//#if defined(WIN32) && !defined(DEV_WIN32)
//typedef unsigned __int64	uint64;
//typedef signed __int64		int64;
//#else
//typedef unsigned long long	uint64;
//typedef signed long long	int64;
//#endif // defined(WIN32) && !defined(DEV_WIN32)
#define SIZEOF_POINTER sizeof(void*)
#include "../../include/GlobalType.h"
typedef uint16 conn_oid_t;
const conn_oid_t invalid_conn_oid = 0xffff;
typedef int8 thread_oid_t;
const thread_oid_t invalid_thread_oid = -1;
typedef int32 session_oid_t;
const session_oid_t invalid_session_oid = -1;

#ifndef WIN32
#include <netinet/in.h>
# ifdef _XOPEN_SOURCE_EXTENDED
#  include <arpa/inet.h>
# endif
#include <sys/socket.h>
#endif

//#ifdef WIN32
//#include <hash_map>
//#include <hash_set>
//#define hash_map stdext::hash_map
//#define hash_set stdext::hash_set
//#else
//#include <ext/hash_map>
//#include <ext/hash_set>
//#define hash_map __gnu_cxx::hash_map 
//#define hash_set __gnu_cxx::hash_set 
//namespace __gnu_cxx
//{
//    template<> struct hash<std::string>
//    {
//        size_t operator()(std::string __s) const
//        { return __stl_hash_string(__s.c_str()); }
//    };
//
//    template<> struct hash<UINT64>
//    { size_t operator()(UINT64 __x) const { return __x; } };
//};
//#endif

struct io_buffer_setting
{
    size_t m_acceptMaxSize;
    size_t m_inMaxSize;
    size_t m_outMaxSize;
};

struct net_setting
{
    std::string m_strListenIp;
    uint16 m_nListenPort;

    thread_oid_t m_nIoThreads;
    conn_oid_t m_maxConnsOfThread;
    io_buffer_setting m_ioBuf;

    thread_oid_t m_nReConnIoThreads;
    conn_oid_t m_maxReConnOfThread;
    io_buffer_setting m_reconnIoBuf;
};

#endif // TYPE_DEFINE_H
