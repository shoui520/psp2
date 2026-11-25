/*
 * SceNetInet / sceNetInet_Library (ePSP 6.61) internal definitions.
 *
 * Implements the PSP BSD-derived IPv4 TCP/UDP socket layer, address
 * conversion utilities, routing/interface configuration, and PCB stat queries.
 */
#ifndef PSPNET_INET_INTERNAL_H
#define PSPNET_INET_INTERNAL_H

#include "common_header.h"

/* PSP error codes */
#define SCE_NET_INET_ERROR_INSUFFICIENT_STACK   ((int)0x80410005)
#define SCE_NET_INET_ERROR_NOT_INITIALIZED      ((int)0x80410200)
#define SCE_NET_INET_ERROR_ALREADY_INITIALIZED  ((int)0x80410201)
#define SCE_NET_INET_ERROR_OUT_OF_MEMORY        ((int)0x80410280)

/* BSD errno values used by sceNetInetGetErrno / sceNetInetGetPspError */
#define INET_EBADF              9
#define INET_EAGAIN             11
#define INET_ENOMEM             12
#define INET_EFAULT             14
#define INET_EINVAL             22
#define INET_EMFILE             24
#define INET_ENOTSOCK           88
#define INET_EMSGSIZE           90
#define INET_EPROTONOSUPPORT    93
#define INET_EOPNOTSUPP         95
#define INET_EAFNOSUPPORT       97
#define INET_EADDRINUSE         98
#define INET_ECONNABORTED       103
#define INET_ECONNRESET         104
#define INET_EISCONN            106
#define INET_ENOTCONN           107
#define INET_ESHUTDOWN          108
#define INET_ENOSPC             28

/* Address families and socket types */
#define INET_AF_INET            2
#define INET_SOCK_STREAM        1
#define INET_SOCK_DGRAM         2
#define INET_SOCK_RAW           3
#define INET_IPPROTO_IP         0
#define INET_IPPROTO_ICMP       1
#define INET_IPPROTO_TCP        6
#define INET_IPPROTO_UDP        17

/* Socket option levels and options */
#define INET_SOL_SOCKET         0xFFFF
#define INET_SO_REUSEADDR       0x0004
#define INET_SO_KEEPALIVE       0x0008
#define INET_SO_BROADCAST       0x0020
#define INET_SO_LINGER          0x0080
#define INET_SO_SNDBUF          0x1001
#define INET_SO_RCVBUF          0x1002
#define INET_SO_SNDTIMEO        0x1005
#define INET_SO_RCVTIMEO        0x1006
#define INET_SO_ERROR           0x1007
#define INET_SO_TYPE            0x1008
#define INET_SO_NBIO            0x1009

/* Poll flags */
#define INET_POLLIN             0x0001
#define INET_POLLOUT            0x0004
#define INET_POLLERR            0x0008
#define INET_POLLHUP            0x0010
#define INET_POLLNVAL           0x0020

/* Limits */
#define INET_MAX_SOCKETS        64
#define INET_MAX_ROUTES         16
#define INET_RING_BUF_SIZE      2048

/* Socket states */
#define INET_SOCK_STATE_FREE        0
#define INET_SOCK_STATE_CREATED     1
#define INET_SOCK_STATE_BOUND       2
#define INET_SOCK_STATE_LISTENING   3
#define INET_SOCK_STATE_CONNECTED   4
#define INET_SOCK_STATE_CLOSED      5

/* Standard BSD/PSP network structures */
typedef struct SceNetInetInAddr {
    u32 sAddr;
} SceNetInetInAddr;

typedef struct SceNetInetSockaddr {
    u8 saLen;
    u8 saFamily;
    u8 saData[14];
} SceNetInetSockaddr;

typedef struct SceNetInetSockaddrIn {
    u8 sinLen;
    u8 sinFamily;
    u16 sinPort;
    SceNetInetInAddr sinAddr;
    u8 sinZero[8];
} SceNetInetSockaddrIn;

typedef struct SceNetInetIovec {
    void *iovBase;
    u32 iovLen;
} SceNetInetIovec;

typedef struct SceNetInetMsghdr {
    void *msgName;
    u32 msgNamelen;
    SceNetInetIovec *msgIov;
    int msgIovlen;
    void *msgControl;
    u32 msgControllen;
    int msgFlags;
} SceNetInetMsghdr;

typedef struct SceNetInetPollfd {
    int fd;
    s16 events;
    s16 revents;
} SceNetInetPollfd;

typedef struct SceNetInetTimeval {
    s32 tvSec;
    s32 tvUsec;
} SceNetInetTimeval;

typedef struct SceNetInetFdSet {
    u32 bits[8];
} SceNetInetFdSet;

typedef struct SceNetInetLinger {
    int onoff;
    int linger;
} SceNetInetLinger;

typedef struct SceNetInetTcpcbstat {
    struct SceNetInetTcpcbstat *next;
    int state;
    SceNetInetInAddr localAddr;
    u16 localPort;
    SceNetInetInAddr peerAddr;
    u16 peerPort;
    u32 sendQueueLen;
    u32 recvQueueLen;
} SceNetInetTcpcbstat;

typedef struct SceNetInetUdpcbstat {
    struct SceNetInetUdpcbstat *next;
    SceNetInetInAddr localAddr;
    u16 localPort;
    u32 recvQueueLen;
} SceNetInetUdpcbstat;

typedef struct InetRouteEntry {
    int inUse;
    u32 dest;
    u32 mask;
    u32 gateway;
    u32 flags;
} InetRouteEntry;

typedef struct InetSocket {
    int inUse;
    int domain;
    int type;
    int protocol;
    int state;
    int backlog;
    int shutRead;
    int shutWrite;
    int nonBlocking;
    int reuseAddr;
    int keepAlive;
    int broadcast;
    int lastError;
    int aborted;
    u32 sndBufSize;
    u32 rcvBufSize;
    u32 sndTimeout;
    u32 rcvTimeout;
    SceNetInetLinger linger;
    SceNetInetSockaddrIn localAddr;
    SceNetInetSockaddrIn peerAddr;
    u8 rxBuf[INET_RING_BUF_SIZE];
    u32 rxHead;
    u32 rxTail;
    u32 rxCount;
} InetSocket;

typedef struct InetIfConfig {
    u32 ipAddr;
    u32 netmask;
    u32 gateway;
    u32 broadcastAddr;
    u32 mtu;
    u32 flags;
    u8 hwAddr[6];
    u8 reserved[2];
} InetIfConfig;

typedef struct InetGlobals {
    int initialized;
    int lastErrno;
    u16 nextEphemeralPort;
    InetSocket sockets[INET_MAX_SOCKETS];
    InetRouteEntry routes[INET_MAX_ROUTES];
    InetIfConfig ifConfig;
    char ntoaBuf[32];
} InetGlobals;

extern InetGlobals g_inet;

/* External imports from module.yml */
extern void *sceKernelMemcpy(void *dst, const void *src, u32 len);
extern void *sceKernelMemset(void *dst, int val, u32 len);
extern int _sceKernelCheckThreadStack(void);
extern int sceKernelGetThreadId(void);
extern int sceKernelSignalSema(int semaId, int count);
extern u64 sceKernelGetSystemTimeWide(void);
extern int sceNet_lib_435843CB(void);
extern int sceNet_lib_757085B0(int ctx, int err);
extern int sceNet_lib_CA3CF5EB(int *outErr);

/* Internal helpers */
void inetSetErrno(int err);
int inetGetErrno(void);
InetSocket *inetGetSocket(int sockFd);
u16 inetAllocEphemeralPort(void);
int inetSocketCreateInternal(int domain, int type, int protocol);

#endif /* PSPNET_INET_INTERNAL_H */
