/*
 * SceNetInet internal library exports (sceNetInet_lib):
 * IPv4 routing table, network interface configuration, ARP/DHCP/PPPoE helpers,
 * and internal socket ioctl wrappers.
 */
#include "pspnet_inet_internal.h"

static int inetRouteAdd(u32 dest, u32 mask, u32 gateway, u32 flags)
{
    int idx;

    for (idx = 0; idx < INET_MAX_ROUTES; idx++) {
        if (!g_inet.routes[idx].inUse) {
            g_inet.routes[idx].inUse = 1;
            g_inet.routes[idx].dest = dest;
            g_inet.routes[idx].mask = mask;
            g_inet.routes[idx].gateway = gateway;
            g_inet.routes[idx].flags = flags;
            return 0;
        }
    }
    return -1;
}

static int inetRouteDelete(u32 dest, u32 mask)
{
    int idx;

    for (idx = 0; idx < INET_MAX_ROUTES; idx++) {
        if (g_inet.routes[idx].inUse &&
            g_inet.routes[idx].dest == dest &&
            (mask == 0 || g_inet.routes[idx].mask == mask)) {
            g_inet.routes[idx].inUse = 0;
            return 0;
        }
    }
    return -1;
}

/* Interface address / netmask / broadcast configuration */
int sceNetInet_lib_4F68DB0E(int ifIdx, u32 *ipAddr, u32 *netmask, u32 *broadcast)
{
    (void)ifIdx;
    if (ipAddr != NULL) {
        g_inet.ifConfig.ipAddr = *ipAddr;
    }
    if (netmask != NULL) {
        g_inet.ifConfig.netmask = *netmask;
    }
    if (broadcast != NULL) {
        g_inet.ifConfig.broadcastAddr = *broadcast;
    }
    return 0;
}

int sceNetInet_lib_6A046357(int ifIdx)
{
    (void)ifIdx;
    g_inet.ifConfig.ipAddr = 0;
    g_inet.ifConfig.netmask = 0;
    g_inet.ifConfig.broadcastAddr = 0;
    return 0;
}

int sceNetInet_lib_5155EC8A(int ifIdx, u32 *outIp, u32 *outMask, u32 *outBcast)
{
    (void)ifIdx;
    if (outIp != NULL) {
        *outIp = g_inet.ifConfig.ipAddr;
    }
    if (outMask != NULL) {
        *outMask = g_inet.ifConfig.netmask;
    }
    if (outBcast != NULL) {
        *outBcast = g_inet.ifConfig.broadcastAddr;
    }
    return 0;
}

/* Routing table queries and modifications */
int sceNetInet_lib_E6F67D14(void)
{
    int idx;
    int count = 0;

    for (idx = 0; idx < INET_MAX_ROUTES; idx++) {
        if (g_inet.routes[idx].inUse) {
            count++;
        }
    }
    return count;
}

int sceNetInet_lib_3D905F34(u32 *outEntries, int maxCount)
{
    int idx;
    int written = 0;

    if (outEntries == NULL || maxCount <= 0) {
        return 0;
    }
    for (idx = 0; idx < INET_MAX_ROUTES && written < maxCount; idx++) {
        if (g_inet.routes[idx].inUse) {
            outEntries[written * 3 + 0] = g_inet.routes[idx].dest;
            outEntries[written * 3 + 1] = g_inet.routes[idx].mask;
            outEntries[written * 3 + 2] = g_inet.routes[idx].gateway;
            written++;
        }
    }
    return written;
}

int sceNetInet_lib_DC38FEE9(int ifIdx, int *outInfo)
{
    (void)ifIdx;
    if (outInfo == NULL) {
        return -1;
    }
    outInfo[0] = (int)g_inet.ifConfig.ipAddr;
    outInfo[1] = (int)g_inet.ifConfig.netmask;
    outInfo[2] = (int)g_inet.ifConfig.gateway;
    return 0;
}

int sceNetInet_lib_DE9E5174(void)
{
    return (g_inet.ifConfig.ipAddr != 0) ? 1 : 0;
}

int sceNetInet_lib_824CBC6D(int ifIdx, int param)
{
    (void)ifIdx;
    g_inet.ifConfig.flags = (u32)param;
    return 0;
}

int sceNetInet_lib_4459EAAF(const u32 *dest, const u32 *mask, const u32 *gateway)
{
    if (dest == NULL || mask == NULL || gateway == NULL) {
        return -1;
    }
    return inetRouteAdd(*dest, *mask, *gateway, 1);
}

int sceNetInet_lib_A94A75E7(const u32 *dest)
{
    if (dest == NULL) {
        return -1;
    }
    return inetRouteDelete(*dest, 0);
}

int sceNetInet_lib_EB7A7E4F(const u32 *dest, const u32 *mask, const u32 *gateway)
{
    if (dest == NULL || mask == NULL || gateway == NULL) {
        return -1;
    }
    g_inet.ifConfig.gateway = *gateway;
    return inetRouteAdd(*dest, *mask, *gateway, 2);
}

int sceNetInet_lib_FDE938A7(const u32 *dest)
{
    if (dest == NULL) {
        return -1;
    }
    return inetRouteDelete(*dest, 0);
}

int sceNetInet_lib_3FDDB0BA(void)
{
    return (int)g_inet.ifConfig.mtu;
}

int sceNetInet_lib_8FE19FC4(u8 *outHwAddr)
{
    if (outHwAddr == NULL) {
        return -1;
    }
    sceKernelMemcpy(outHwAddr, g_inet.ifConfig.hwAddr, sizeof(g_inet.ifConfig.hwAddr));
    return 0;
}

int sceNetInet_lib_AC9D90A5(int ifIdx, u32 mtu)
{
    (void)ifIdx;
    g_inet.ifConfig.mtu = mtu;
    return 0;
}

int sceNetInet_lib_AAF4895A(int ifIdx, u32 *outMtu)
{
    (void)ifIdx;
    if (outMtu != NULL) {
        *outMtu = g_inet.ifConfig.mtu;
    }
    return 0;
}

int sceNetInet_lib_AEE60F84(int sockFd, u32 cmd, void *data)
{
    InetSocket *sock = inetGetSocket(sockFd);

    if (sock == NULL) {
        inetSetErrno(INET_EBADF);
        return -1;
    }
    (void)cmd;
    (void)data;
    return 0;
}

int sceNetInet_lib_B3FF98EB(int sockFd, int level, int optname, void *optval)
{
    (void)level;
    (void)optname;
    (void)optval;
    return (inetGetSocket(sockFd) != NULL) ? 0 : -1;
}

int sceNetInet_lib_0238B6DF(void)
{
    return g_inet.initialized;
}

void sceNetInet_lib_D6598787(u32 key, u32 *val)
{
    (void)key;
    if (val != NULL) {
        g_inet.ifConfig.flags = *val;
    }
}

int sceNetInet_lib_D609AD36(u32 key, u32 *val)
{
    (void)key;
    if (val != NULL) {
        *val = g_inet.ifConfig.flags;
    }
    return 0;
}

int *sceNetInet_lib_D7928A6D(void)
{
    return &g_inet.lastErrno;
}

int sceNetInet_lib_7CB1D9E3(int param)
{
    (void)param;
    return 0;
}

int sceNetInet_lib_4C13BE10(int param)
{
    (void)param;
    return 0;
}

int sceNetInet_lib_59561561(void)
{
    return (g_inet.ifConfig.ipAddr != 0) ? 1 : 0;
}

int sceNetInet_lib_7776A492(int param)
{
    (void)param;
    return 0;
}
