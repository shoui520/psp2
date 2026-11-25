/*
 * SceNetInet initialization, termination, errno/error queries, and PCB stats.
 */
#include "pspnet_inet_internal.h"

InetGlobals g_inet;

void inetSetErrno(int err)
{
    g_inet.lastErrno = err;
}

int inetGetErrno(void)
{
    return g_inet.lastErrno;
}

int module_start(SceSize args, void *argp)
{
    (void)args;
    (void)argp;
    return 0;
}

int module_stop(SceSize args, void *argp)
{
    (void)args;
    (void)argp;
    return 0;
}

int sceNetInetInit(void)
{
    if (_sceKernelCheckThreadStack() < 4048) {
        return SCE_NET_INET_ERROR_INSUFFICIENT_STACK;
    }
    if (g_inet.initialized == 1) {
        return SCE_NET_INET_ERROR_ALREADY_INITIALIZED;
    }

    sceKernelMemset(&g_inet, 0, sizeof(g_inet));
    g_inet.nextEphemeralPort = 49152;
    g_inet.ifConfig.mtu = 1500;
    g_inet.initialized = 1;
    return 0;
}

int sceNetInetTerm(void)
{
    if (_sceKernelCheckThreadStack() < 4080) {
        return SCE_NET_INET_ERROR_INSUFFICIENT_STACK;
    }
    if (g_inet.initialized == 0) {
        return 0;
    }

    sceKernelMemset(g_inet.sockets, 0, sizeof(g_inet.sockets));
    sceKernelMemset(g_inet.routes, 0, sizeof(g_inet.routes));
    g_inet.initialized = 0;
    g_inet.lastErrno = 0;
    return 0;
}

int sceNetInetGetErrno(void)
{
    if (_sceKernelCheckThreadStack() < 4080) {
        return SCE_NET_INET_ERROR_INSUFFICIENT_STACK;
    }
    return g_inet.lastErrno;
}

u32 sceNetInetGetPspError(void)
{
    int err;

    if (_sceKernelCheckThreadStack() < 4080) {
        return (u32)SCE_NET_INET_ERROR_INSUFFICIENT_STACK;
    }
    err = g_inet.lastErrno;
    if (err != 0) {
        return ((u32)(u16)err) | 0x80010000U;
    }
    return 0;
}

int sceNetInetGetTcpcbstat(int *bufLen, SceNetInetTcpcbstat *buf)
{
    int idx;
    int count = 0;
    int maxEntries;

    if (bufLen == NULL) {
        inetSetErrno(INET_EINVAL);
        return -1;
    }
    if (buf == NULL) {
        for (idx = 0; idx < INET_MAX_SOCKETS; idx++) {
            if (g_inet.sockets[idx].inUse && g_inet.sockets[idx].type == INET_SOCK_STREAM) {
                count++;
            }
        }
        *bufLen = count * (int)sizeof(SceNetInetTcpcbstat);
        return 0;
    }

    maxEntries = *bufLen / (int)sizeof(SceNetInetTcpcbstat);
    for (idx = 0; idx < INET_MAX_SOCKETS && count < maxEntries; idx++) {
        InetSocket *sock = &g_inet.sockets[idx];
        if (sock->inUse && sock->type == INET_SOCK_STREAM) {
            SceNetInetTcpcbstat *st = &buf[count];
            sceKernelMemset(st, 0, sizeof(*st));
            st->state = sock->state;
            st->localAddr = sock->localAddr.sinAddr;
            st->localPort = sock->localAddr.sinPort;
            st->peerAddr = sock->peerAddr.sinAddr;
            st->peerPort = sock->peerAddr.sinPort;
            st->recvQueueLen = sock->rxCount;
            if (count > 0) {
                buf[count - 1].next = st;
            }
            count++;
        }
    }
    *bufLen = count * (int)sizeof(SceNetInetTcpcbstat);
    return 0;
}

int sceNetInetGetUdpcbstat(int *bufLen, SceNetInetUdpcbstat *buf)
{
    int idx;
    int count = 0;
    int maxEntries;

    if (bufLen == NULL) {
        inetSetErrno(INET_EINVAL);
        return -1;
    }
    if (buf == NULL) {
        for (idx = 0; idx < INET_MAX_SOCKETS; idx++) {
            if (g_inet.sockets[idx].inUse && g_inet.sockets[idx].type == INET_SOCK_DGRAM) {
                count++;
            }
        }
        *bufLen = count * (int)sizeof(SceNetInetUdpcbstat);
        return 0;
    }

    maxEntries = *bufLen / (int)sizeof(SceNetInetUdpcbstat);
    for (idx = 0; idx < INET_MAX_SOCKETS && count < maxEntries; idx++) {
        InetSocket *sock = &g_inet.sockets[idx];
        if (sock->inUse && sock->type == INET_SOCK_DGRAM) {
            SceNetInetUdpcbstat *st = &buf[count];
            sceKernelMemset(st, 0, sizeof(*st));
            st->localAddr = sock->localAddr.sinAddr;
            st->localPort = sock->localAddr.sinPort;
            st->recvQueueLen = sock->rxCount;
            if (count > 0) {
                buf[count - 1].next = st;
            }
            count++;
        }
    }
    *bufLen = count * (int)sizeof(SceNetInetUdpcbstat);
    return 0;
}

/* Forwarders to sceNetInet_lib routing/interface functions */
extern int sceNetInet_lib_59561561(void);
extern int sceNetInet_lib_DC38FEE9(int ifIdx, int *outInfo);
extern int sceNetInet_lib_824CBC6D(int ifIdx, int param);

int sceNetInet_CCC18C45(void)
{
    return sceNetInet_lib_59561561();
}

int sceNetInet_2D5868C0(int ifIdx, int *outInfo)
{
    return sceNetInet_lib_DC38FEE9(ifIdx, outInfo);
}

int sceNetInet_1D023504(int ifIdx, int param)
{
    return sceNetInet_lib_824CBC6D(ifIdx, param);
}
