/*
 * SceNetInet interface/routing/ARP control helpers and TCP/UDP PCB statistics.
 */
#include "pspnet_inet_internal.h"

/* SIOCAIFADDR / SIOCDIFADDR ioctl codes */
#define INET_SIOCAIFADDR  0x8040691AU
#define INET_SIOCDIFADDR  0x80206919U

/* Routing message stack packet buffer (588 bytes) */
typedef struct InetRtPacket {
    InetRtMsghdr hdr;
    u8 body[512];
} InetRtPacket;
INET_STATIC_ASSERT(rt_packet_size, sizeof(InetRtPacket) == 588);

static s32 g_rt_seq;
static s32 g_arp_seq;
static u32 g_dns_main;
static u32 g_dns_backup;

static __inline__ __attribute__((always_inline)) u32 inetRoundupSa(u8 saLen)
{
    u32 step = (((u32)saLen - 1U) | 3U) + 1U;
    if (saLen == 0) {
        step = 4U;
    }
    return step;
}

static int INET_NOINLINE inetSendRouteMsg(u8 msgType, const InetSockaddrIn *dst,
                                          const InetSockaddrIn *gw, const InetSockaddrIn *mask,
                                          s32 flags);
static int INET_NOINLINE inetFetchRouteTable(u8 **outBuf, u32 *outLen);
static int INET_NOINLINE inetSendRecvRouteMsg(int sock, InetRtPacket *pkt, int msgType,
                                              const InetSockaddr *dst, const InetSockaddr *gw,
                                              const InetSockaddr *mask, s32 flags, s32 seq);

int sceNetInet_lib_4F68DB0E(const char *ifname, const u32 *addr,
                            const u32 *dstAddr, const u32 *mask)
{
    int sock;
    int ret = SCE_NET_INET_ERROR_IF_ADDR_SET;
    InetIfAliasReq req;

    sceNetInet_lib_6A046357(ifname);
    sock = inetSockCreateInternal(INET_AF_INET, INET_SOCK_DGRAM, 0);
    if (sock < 0) {
        return SCE_NET_INET_ERROR_SOCKET_CREATE;
    }

    sceKernelMemset(&req, 0, sizeof(req));
    sceNet_lib_B5CE388A(req.ifra_name, ifname, 16);
    if (addr != NULL) {
        req.ifra_addr.sin_len = sizeof(InetSockaddrIn);
        req.ifra_addr.sin_family = INET_AF_INET;
        req.ifra_addr.sin_addr = *addr;
    }
    if (mask != NULL) {
        req.ifra_mask.sin_len = sizeof(InetSockaddrIn);
        req.ifra_mask.sin_addr = *mask;
    }
    if (dstAddr != NULL) {
        req.ifra_broadaddr.sin_len = sizeof(InetSockaddrIn);
        req.ifra_broadaddr.sin_family = INET_AF_INET;
        req.ifra_broadaddr.sin_addr = *dstAddr;
    }

    if (sceNetInet_lib_AEE60F84(sock, INET_SIOCAIFADDR, &req) >= 0) {
        ret = 0;
    }
    inetCloseInternal(sock);
    return ret;
}

int sceNetInet_lib_6A046357(const char *ifname)
{
    int sock;
    int ret;
    InetIfreq req;

    sock = inetSockCreateInternal(INET_AF_INET, INET_SOCK_DGRAM, 0);
    if (sock < 0) {
        return SCE_NET_INET_ERROR_SOCKET_CREATE;
    }

    sceKernelMemset(&req, 0, sizeof(req));
    sceNet_lib_B5CE388A(req.ifr_name, ifname, 16);
    ret = sceNetInet_lib_AEE60F84(sock, INET_SIOCDIFADDR, &req);
    if (ret < 0) {
        ret = 0;
        if (inetGetErrnoRaw() != (int)0x80410183) {
            ret = SCE_NET_INET_ERROR_IF_ADDR_DEL;
        }
    }
    inetCloseInternal(sock);
    return ret;
}

int sceNetInet_lib_5155EC8A(const char *ifname, u32 *addr, u32 *dstAddr, u32 *mask)
{
    u32 zeroAddr = inetBswap32(0);
    int ret;

    if (addr != NULL) {
        *addr = zeroAddr;
    }
    if (dstAddr != NULL) {
        *dstAddr = zeroAddr;
    }
    if (mask != NULL) {
        *mask = zeroAddr;
    }

    ret = inetInGetIfaddrInfo(ifname, addr, dstAddr, mask);
    if (ret < 0) {
        if (inetGetErrnoRaw() == (int)0x80410183) {
            return 0;
        }
        return SCE_NET_INET_ERROR_IF_ADDR_GET;
    }
    return ret;
}

int sceNetInet_lib_E6F67D14(const char *ifname)
{
    int ifIdx;
    int ret;
    int sock;
    u8 *tblBuf = NULL;
    u32 tblLen = 0;

    ifIdx = sceNet_lib_710BD467(ifname);
    if (ifIdx < 0) {
        return SCE_NET_INET_ERROR_NO_IF;
    }

    ret = inetFetchRouteTable(&tblBuf, &tblLen);
    if (ret < 0) {
        return ret;
    }

    sock = sceNetInetSocket(INET_AF_ROUTE, INET_SOCK_RAW, 0);
    if (sock < 0) {
        ret = SCE_NET_INET_ERROR_SOCKET_CREATE;
    } else {
        u8 *cur = tblBuf;
        u8 *end = tblBuf + tblLen;
        s32 seq = 0;
        sceNetInetShutdown(sock, 0);

        while (cur < end) {
            InetRtMsghdr *rtm = (InetRtMsghdr *)cur;
            InetSockaddr *sa = (InetSockaddr *)(cur + sizeof(InetRtMsghdr));
            u16 msgLen = rtm->rtm_msglen;

            if ((rtm->rtm_flags & INET_RTF_GATEWAY) != 0 &&
                sa->sa_family == INET_AF_INET &&
                rtm->rtm_index == (u16)ifIdx) {
                rtm->rtm_seq = seq++;
                rtm->rtm_type = INET_RTM_DELETE;
                if (sceNetInetSend(sock, rtm, msgLen, 0) < (int)msgLen) {
                    ret = SCE_NET_INET_ERROR_ROUTE_DEL;
                    break;
                }
            }
            cur += msgLen;
        }
    }

    if (tblBuf != NULL) {
        sceNetIfhandle_lib_76BAD213(tblBuf);
    }
    if (sock >= 0) {
        sceNetInetClose(sock);
    }
    return ret;
}

int sceNetInet_lib_3D905F34(s32 *bufSize, InetRouteEntryInfo *outBuf)
{
    u8 *tblBuf = NULL;
    u32 tblLen = 0;
    int ret;
    s32 count = 0;
    s32 usedBytes = 0;
    InetRouteEntryInfo *prev = NULL;
    u8 *cur;
    u8 *end;

    ret = inetFetchRouteTable(&tblBuf, &tblLen);
    if (ret < 0) {
        return ret;
    }

    cur = tblBuf;
    end = tblBuf + tblLen;
    while (cur < end) {
        InetRtMsghdr *rtm = (InetRtMsghdr *)cur;
        u8 *addrPtr = cur + sizeof(InetRtMsghdr);
        InetSockaddrIn *dstSa = (InetSockaddrIn *)addrPtr;
        u16 msgLen = rtm->rtm_msglen;

        if ((rtm->rtm_flags & INET_RTF_LLINFO) == 0 && dstSa->sin_family == INET_AF_INET) {
            if (outBuf == NULL) {
                count++;
                usedBytes += (s32)sizeof(InetRouteEntryInfo);
            } else {
                if (*bufSize < usedBytes + (s32)sizeof(InetRouteEntryInfo)) {
                    break;
                }
                {
                    InetRouteEntryInfo *entry = (InetRouteEntryInfo *)((u8 *)outBuf + usedBytes);
                    s32 addrs = rtm->rtm_addrs;
                    u8 *saCur;

                    if (prev != NULL) {
                        prev->next = entry;
                    }
                    usedBytes += (s32)sizeof(InetRouteEntryInfo);
                    sceKernelMemset(entry, 0, sizeof(*entry));
                    entry->flags = (u32)(rtm->rtm_flags & 0x837);
                    sceKernelMemcpy(&entry->dst, &dstSa->sin_addr, 4);
                    count++;

                    saCur = addrPtr + inetRoundupSa(dstSa->sin_len);
                    if ((addrs & INET_RTA_GATEWAY) != 0) {
                        InetSockaddrIn *gwSa = (InetSockaddrIn *)saCur;
                        sceKernelMemcpy(&entry->gateway, &gwSa->sin_addr, 4);
                        saCur += inetRoundupSa(gwSa->sin_len);
                    }
                    if ((addrs & INET_RTA_NETMASK) != 0) {
                        InetSockaddrIn *maskSa = (InetSockaddrIn *)saCur;
                        sceKernelMemcpy(&entry->mask, &maskSa->sin_addr, 4);
                        saCur += inetRoundupSa(maskSa->sin_len);
                    }
                    if ((addrs & INET_RTA_GENMASK) != 0) {
                        InetSockaddr *genSa = (InetSockaddr *)saCur;
                        saCur += inetRoundupSa(genSa->sa_len);
                    }
                    if ((addrs & INET_RTA_IFP) != 0) {
                        InetSockaddrDl *ifpSa = (InetSockaddrDl *)saCur;
                        sceKernelMemcpy(entry->ifname, ifpSa->sdl_data, ifpSa->sdl_nlen);
                    }
                    prev = entry;
                }
            }
        }
        cur += msgLen;
    }

    *bufSize = count * (s32)sizeof(InetRouteEntryInfo);
    if (tblBuf != NULL) {
        sceNetIfhandle_lib_76BAD213(tblBuf);
    }
    return ret;
}

int sceNetInet_lib_59561561(const u8 *macAddr, const u32 *ipAddr)
{
    int sock;
    int ret;
    InetRtPacket pkt;
    InetSockaddrIn targetSa;
    InetSockaddrDl linkSa;

    sock = sceNetInetSocket(INET_AF_ROUTE, INET_SOCK_RAW, 0);
    if (sock < 0) {
        return SCE_NET_INET_ERROR_SOCKET_CREATE;
    }

    sceKernelMemset(&targetSa, 0, sizeof(targetSa));
    targetSa.sin_len = sizeof(targetSa);
    targetSa.sin_family = INET_AF_INET;
    targetSa.sin_addr = *ipAddr;

    ret = inetSendRecvRouteMsg(sock, &pkt, INET_RTM_GET,
                               (const InetSockaddr *)&targetSa, NULL, NULL, 0, ++g_arp_seq);
    if (ret >= 0) {
        InetSockaddrIn *replyDst = (InetSockaddrIn *)pkt.body;
        InetSockaddrDl *replyGw = (InetSockaddrDl *)(pkt.body + inetRoundupSa(replyDst->sin_len));
        int canAdd = 0;

        if (replyDst->sin_addr == targetSa.sin_addr) {
            if (replyGw->sdl_family == INET_AF_LINK &&
                (pkt.hdr.rtm_flags & (INET_RTF_LLINFO | INET_RTF_GATEWAY)) == INET_RTF_LLINFO &&
                replyGw->sdl_type == 6) {
                canAdd = 1;
            }
        } else if (replyGw->sdl_family == INET_AF_LINK) {
            canAdd = 1;
        }

        if (canAdd) {
            sceKernelMemset(&linkSa, 0, sizeof(linkSa));
            linkSa.sdl_len = sizeof(linkSa);
            linkSa.sdl_family = replyGw->sdl_family;
            linkSa.sdl_index = replyGw->sdl_index;
            linkSa.sdl_type = replyGw->sdl_type;
            sceKernelMemcpy(&linkSa.sdl_data[linkSa.sdl_nlen], macAddr, 6);
            linkSa.sdl_alen = 6;
            ret = inetSendRecvRouteMsg(sock, &pkt, INET_RTM_ADD,
                                       (const InetSockaddr *)&targetSa,
                                       (const InetSockaddr *)&linkSa,
                                       NULL, INET_RTF_STATIC | INET_RTF_HOST, ++g_arp_seq);
        } else {
            ret = SCE_NET_INET_ERROR_ARP_ADD;
        }
    }

    sceNetInetClose(sock);
    return ret;
}

int sceNetInet_lib_DC38FEE9(int unusedArg, const u32 *ipAddr)
{
    int sock;
    int ret;
    InetRtPacket pkt;
    InetSockaddrIn targetSa;
    (void)unusedArg;

    sock = sceNetInetSocket(INET_AF_ROUTE, INET_SOCK_RAW, 0);
    if (sock < 0) {
        return SCE_NET_INET_ERROR_SOCKET_CREATE;
    }

    sceKernelMemset(&targetSa, 0, sizeof(targetSa));
    targetSa.sin_len = sizeof(targetSa);
    targetSa.sin_family = INET_AF_INET;
    targetSa.sin_addr = *ipAddr;

    ret = inetSendRecvRouteMsg(sock, &pkt, INET_RTM_GET,
                               (const InetSockaddr *)&targetSa, NULL, NULL, 0, ++g_arp_seq);
    if (ret >= 0) {
        InetSockaddrIn *replyDst = (InetSockaddrIn *)pkt.body;
        InetSockaddrDl *replyGw = (InetSockaddrDl *)(pkt.body + inetRoundupSa(replyDst->sin_len));
        int canDel = 0;

        if (replyDst->sin_addr != targetSa.sin_addr) {
            if (replyGw->sdl_family == INET_AF_LINK) {
                canDel = 1;
            }
        } else if (replyGw->sdl_family == INET_AF_LINK &&
                   (pkt.hdr.rtm_flags & (INET_RTF_LLINFO | INET_RTF_GATEWAY)) == INET_RTF_LLINFO &&
                   replyGw->sdl_type == 6) {
            canDel = 1;
        }

        if (canDel) {
            ret = inetSendRecvRouteMsg(sock, &pkt, INET_RTM_DELETE,
                                       NULL, NULL, NULL, 0, ++g_arp_seq);
        } else {
            ret = SCE_NET_INET_ERROR_ARP_DEL;
        }
    }

    sceNetInetClose(sock);
    return ret;
}

int sceNetInet_lib_DE9E5174(const char *ifname)
{
    int ifIdx;
    int ret;
    int sock;
    u8 *tblBuf = NULL;
    u32 tblLen = 0;

    ifIdx = sceNet_lib_710BD467(ifname);
    if (ifIdx < 0) {
        return SCE_NET_INET_ERROR_NO_IF;
    }

    ret = inetFetchRouteTable(&tblBuf, &tblLen);
    if (ret < 0) {
        return ret;
    }

    sock = sceNetInetSocket(INET_AF_ROUTE, INET_SOCK_RAW, 0);
    if (sock < 0) {
        ret = SCE_NET_INET_ERROR_SOCKET_CREATE;
    } else {
        u8 *cur = tblBuf;
        u8 *end = tblBuf + tblLen;
        s32 seq = 0;
        sceNetInetShutdown(sock, 0);

        while (cur < end) {
            InetRtMsghdr *rtm = (InetRtMsghdr *)cur;
            InetSockaddr *sa = (InetSockaddr *)(cur + sizeof(InetRtMsghdr));
            u16 msgLen = rtm->rtm_msglen;

            if ((rtm->rtm_flags & INET_RTF_GATEWAY) != 0 &&
                sa->sa_family == INET_AF_INET &&
                rtm->rtm_index == (u16)ifIdx) {
                rtm->rtm_seq = seq++;
                rtm->rtm_type = INET_RTM_DELETE;
                if (sceNetInetSend(sock, rtm, msgLen, 0) < (int)msgLen) {
                    ret = SCE_NET_INET_ERROR_ARP_FLUSH;
                    break;
                }
            }
            cur += msgLen;
        }
    }

    if (tblBuf != NULL) {
        sceNetIfhandle_lib_76BAD213(tblBuf);
    }
    if (sock >= 0) {
        sceNetInetClose(sock);
    }
    return ret;
}

int sceNetInet_lib_824CBC6D(s32 bufSize, InetArpEntryInfo *outBuf)
{
    u8 *tblBuf = NULL;
    u32 tblLen = 0;
    int ret;
    s32 count = 0;
    s32 usedBytes = 0;
    InetArpEntryInfo *prev = NULL;
    u8 *cur;
    u8 *end;

    ret = inetFetchRouteTable(&tblBuf, &tblLen);
    if (ret < 0) {
        return ret;
    }

    cur = tblBuf;
    end = tblBuf + tblLen;
    while (cur < end) {
        InetRtMsghdr *rtm = (InetRtMsghdr *)cur;
        u16 msgLen = rtm->rtm_msglen;

        if ((rtm->rtm_flags & INET_RTF_LLINFO) != 0) {
            u8 *addrPtr = cur + sizeof(InetRtMsghdr);
            InetSockaddrIn *dstSa = (InetSockaddrIn *)addrPtr;
            InetSockaddrDl *gwSa = (InetSockaddrDl *)(addrPtr + inetRoundupSa(dstSa->sin_len));

            if (gwSa->sdl_family == INET_AF_LINK && gwSa->sdl_type == 6) {
                InetIfnet *ifp = sceNet_lib_45945E8D(gwSa->sdl_index);
                if (ifp != NULL) {
                    if (outBuf == NULL) {
                        count++;
                        usedBytes += (s32)sizeof(InetArpEntryInfo);
                    } else {
                        if (bufSize < usedBytes) {
                            break;
                        }
                        {
                            InetArpEntryInfo *entry = (InetArpEntryInfo *)((u8 *)outBuf + usedBytes);
                            if (prev != NULL) {
                                prev->next = entry;
                            }
                            sceKernelMemset(entry, 0, sizeof(*entry));
                            entry->ip_addr = dstSa->sin_addr;
                            usedBytes += (s32)sizeof(InetArpEntryInfo);
                            prev = entry;
                            sceKernelMemcpy(entry->mac_addr, &gwSa->sdl_data[gwSa->sdl_nlen], 6);
                            count++;
                            sceKernelMemcpy(entry->ifname, ifp->if_xname, 16);
                        }
                    }
                }
            }
        }
        cur += msgLen;
    }

    ret = count * (s32)sizeof(InetArpEntryInfo);
    if (tblBuf != NULL) {
        sceNetIfhandle_lib_76BAD213(tblBuf);
    }
    return ret;
}

int sceNetInet_lib_4459EAAF(const u32 *dst, const u32 *gw, const u32 *mask)
{
    InetSockaddrIn dstSa;
    InetSockaddrIn gwSa;
    InetSockaddrIn maskSa;

    sceKernelMemset(&dstSa, 0, sizeof(dstSa));
    sceKernelMemset(&gwSa, 0, sizeof(gwSa));
    sceKernelMemset(&maskSa, 0, sizeof(maskSa));
    dstSa.sin_len = sizeof(dstSa);
    dstSa.sin_family = INET_AF_INET;
    dstSa.sin_addr = *dst;
    gwSa.sin_len = sizeof(gwSa);
    gwSa.sin_family = INET_AF_INET;
    gwSa.sin_addr = *gw;
    maskSa.sin_len = sizeof(maskSa);
    maskSa.sin_family = INET_AF_INET;
    maskSa.sin_addr = *mask;
    return inetSendRouteMsg(INET_RTM_ADD, &dstSa, &gwSa, &maskSa,
                            INET_RTF_STATIC | INET_RTF_GATEWAY | INET_RTF_UP);
}

int sceNetInet_lib_A94A75E7(const u32 *gw)
{
    InetSockaddrIn dstSa;
    InetSockaddrIn gwSa;
    InetSockaddrIn maskSa;

    sceKernelMemset(&dstSa, 0, sizeof(dstSa));
    sceKernelMemset(&gwSa, 0, sizeof(gwSa));
    sceKernelMemset(&maskSa, 0, sizeof(maskSa));
    dstSa.sin_len = sizeof(dstSa);
    dstSa.sin_family = INET_AF_INET;
    dstSa.sin_addr = inetBswap32(0xE0000000U);
    gwSa.sin_len = sizeof(gwSa);
    gwSa.sin_family = INET_AF_INET;
    gwSa.sin_addr = *gw;
    maskSa.sin_len = sizeof(maskSa);
    maskSa.sin_family = INET_AF_INET;
    maskSa.sin_addr = inetBswap32(0xF0000000U);
    return inetSendRouteMsg(INET_RTM_ADD, &dstSa, &gwSa, &maskSa,
                            INET_RTF_STATIC | INET_RTF_UP);
}

int sceNetInet_lib_EB7A7E4F(const u32 *dst, const u32 *gw, const u32 *mask)
{
    InetSockaddrIn dstSa;
    InetSockaddrIn gwSa;
    InetSockaddrIn maskSa;

    sceKernelMemset(&dstSa, 0, sizeof(dstSa));
    sceKernelMemset(&gwSa, 0, sizeof(gwSa));
    sceKernelMemset(&maskSa, 0, sizeof(maskSa));
    dstSa.sin_len = sizeof(dstSa);
    dstSa.sin_family = INET_AF_INET;
    dstSa.sin_addr = *dst;
    gwSa.sin_len = sizeof(gwSa);
    gwSa.sin_family = INET_AF_INET;
    gwSa.sin_addr = *gw;
    maskSa.sin_len = sizeof(maskSa);
    maskSa.sin_family = INET_AF_INET;
    maskSa.sin_addr = *mask;
    return inetSendRouteMsg(INET_RTM_DELETE, &dstSa, &gwSa, &maskSa,
                            INET_RTF_STATIC | INET_RTF_GATEWAY);
}

int sceNetInet_lib_FDE938A7(const u32 *gw)
{
    InetSockaddrIn dstSa;
    InetSockaddrIn gwSa;
    InetSockaddrIn maskSa;

    sceKernelMemset(&dstSa, 0, sizeof(dstSa));
    sceKernelMemset(&gwSa, 0, sizeof(gwSa));
    sceKernelMemset(&maskSa, 0, sizeof(maskSa));
    dstSa.sin_len = sizeof(dstSa);
    dstSa.sin_family = INET_AF_INET;
    dstSa.sin_addr = inetBswap32(0xE0000000U);
    gwSa.sin_len = sizeof(gwSa);
    gwSa.sin_family = INET_AF_INET;
    gwSa.sin_addr = *gw;
    maskSa.sin_len = sizeof(maskSa);
    maskSa.sin_family = INET_AF_INET;
    maskSa.sin_addr = inetBswap32(0xF0000000U);
    return inetSendRouteMsg(INET_RTM_DELETE, &dstSa, &gwSa, &maskSa,
                            INET_RTF_STATIC);
}

int sceNetInet_lib_7776A492(const u32 *gw)
{
    InetSockaddrIn delDst;
    InetSockaddrIn delMask;
    InetSockaddrIn addDst;
    InetSockaddrIn addGw;
    InetSockaddrIn addMask;

    sceKernelMemset(&delDst, 0, sizeof(delDst));
    sceKernelMemset(&delMask, 0, sizeof(delMask));
    delDst.sin_len = sizeof(delDst);
    delDst.sin_family = INET_AF_INET;
    delMask.sin_len = sizeof(delMask);
    delMask.sin_family = INET_AF_INET;
    inetSendRouteMsg(INET_RTM_DELETE, &delDst, NULL, &delMask,
                     INET_RTF_STATIC | INET_RTF_GATEWAY);

    sceKernelMemset(&addDst, 0, sizeof(addDst));
    sceKernelMemset(&addGw, 0, sizeof(addGw));
    sceKernelMemset(&addMask, 0, sizeof(addMask));
    addDst.sin_len = sizeof(addDst);
    addDst.sin_family = INET_AF_INET;
    addDst.sin_addr = 0;
    addGw.sin_len = sizeof(addGw);
    addGw.sin_family = INET_AF_INET;
    addGw.sin_addr = *gw;
    addMask.sin_len = sizeof(addMask);
    addMask.sin_family = INET_AF_INET;
    addMask.sin_addr = 0;
    return inetSendRouteMsg(INET_RTM_ADD, &addDst, &addGw, &addMask,
                            INET_RTF_STATIC | INET_RTF_GATEWAY | INET_RTF_UP);
}

int sceNetInet_lib_3FDDB0BA(void)
{
    InetSockaddrIn dstSa;
    InetSockaddrIn maskSa;

    sceKernelMemset(&dstSa, 0, sizeof(dstSa));
    sceKernelMemset(&maskSa, 0, sizeof(maskSa));
    dstSa.sin_len = sizeof(dstSa);
    dstSa.sin_family = INET_AF_INET;
    maskSa.sin_len = sizeof(maskSa);
    maskSa.sin_family = INET_AF_INET;
    return inetSendRouteMsg(INET_RTM_DELETE, &dstSa, NULL, &maskSa,
                            INET_RTF_STATIC | INET_RTF_GATEWAY);
}

int sceNetInet_lib_8FE19FC4(u8 *outGw)
{
    InetRouteEntryInfo entry;
    s32 len = (s32)sizeof(entry);
    int ret;

    ret = sceNetInet_lib_3D905F34(&len, &entry);
    if (ret >= 0) {
        if (entry.dst != 0) {
            return SCE_NET_INET_ERROR_NO_GATEWAY;
        }
        outGw[0] = (u8)(entry.gateway & 0xFFU);
        outGw[1] = (u8)((entry.gateway >> 8) & 0xFFU);
        outGw[2] = (u8)((entry.gateway >> 16) & 0xFFU);
        outGw[3] = (u8)((entry.gateway >> 24) & 0xFFU);
    }
    return ret;
}

void INET_NOINLINE inetIfctrlResetSeq(void)
{
    g_rt_seq = 0;
    g_arp_seq = 0;
}

static int INET_NOINLINE inetSendRouteMsg(u8 msgType, const InetSockaddrIn *dst,
                                          const InetSockaddrIn *gw, const InetSockaddrIn *mask,
                                          s32 flags)
{
    int sock;
    int ret;
    InetRtPacket pkt;
    u8 *cursor = pkt.body;

    sock = sceNetInetSocket(INET_AF_ROUTE, INET_SOCK_RAW, 0);
    if (sock < 0) {
        return SCE_NET_INET_ERROR_SOCKET_CREATE;
    }

    sceKernelMemset(&pkt, 0, sizeof(pkt));
    pkt.hdr.rtm_version = INET_RTM_VERSION;
    pkt.hdr.rtm_type = msgType;
    pkt.hdr.rtm_flags = flags;
    pkt.hdr.rtm_seq = ++g_rt_seq;

    if (dst != NULL) {
        u32 step = inetRoundupSa(dst->sin_len);
        pkt.hdr.rtm_addrs |= INET_RTA_DST;
        sceNet_lib_4753D878(cursor, dst, (int)step);
        cursor += step;
    }
    if (gw != NULL) {
        u32 step = inetRoundupSa(gw->sin_len);
        pkt.hdr.rtm_addrs |= INET_RTA_GATEWAY;
        sceNet_lib_4753D878(cursor, gw, (int)step);
        cursor += step;
    }
    if (mask != NULL) {
        u32 step = inetRoundupSa(mask->sin_len);
        pkt.hdr.rtm_addrs |= INET_RTA_NETMASK;
        sceNet_lib_4753D878(cursor, mask, (int)step);
        cursor += step;
    }

    pkt.hdr.rtm_msglen = (u16)(cursor - (u8 *)&pkt);
    ret = (sceNetInetSend(sock, &pkt, pkt.hdr.rtm_msglen, 0) >= 0) ? 0 : SCE_NET_INET_ERROR_ROUTE_SEND;
    sceNetInetClose(sock);
    return ret;
}

static int INET_NOINLINE inetFetchRouteTable(u8 **outBuf, u32 *outLen)
{
    int mib[6];
    u32 needed = 0;

    for (;;) {
        u8 *buf;
        mib[0] = 4;
        mib[1] = INET_AF_ROUTE;
        mib[2] = 0;
        mib[3] = INET_AF_INET;
        mib[4] = 1;
        mib[5] = 0;
        if (sceNetInet_lib_B3FF98EB(mib, 6, NULL, &needed, NULL, 0) < 0) {
            return SCE_NET_INET_ERROR_ROUTE_SYSCTL;
        }
        if (needed == 0) {
            *outLen = 0;
            return 0;
        }
        buf = (u8 *)sceNetIfhandle_lib_15CFE3C0(needed);
        if (buf == NULL) {
            return SCE_NET_INET_ERROR_ENOMEM;
        }
        sceKernelMemset(buf, 0, needed);
        if (sceNetInet_lib_B3FF98EB(mib, 6, buf, &needed, NULL, 0) >= 0) {
            *outBuf = buf;
            *outLen = needed;
            return 0;
        }
        if (sceNetInetGetErrno() != INET_ENOSPC) {
            sceNetIfhandle_lib_76BAD213(buf);
            return SCE_NET_INET_ERROR_ROUTE_SYSCTL;
        }
        sceNetIfhandle_lib_76BAD213(buf);
    }
}

static int INET_NOINLINE inetSendRecvRouteMsg(int sock, InetRtPacket *pkt, int msgType,
                                              const InetSockaddr *dst, const InetSockaddr *gw,
                                              const InetSockaddr *mask, s32 flags, s32 seq)
{
    SceUID thid;
    int recvLen;

    thid = sceKernelGetThreadId();
    if (thid < 0) {
        return thid;
    }

    if (msgType != INET_RTM_DELETE) {
        u8 *cursor = pkt->body;
        sceKernelMemset(pkt, 0, sizeof(*pkt));
        pkt->hdr.rtm_flags = flags;
        pkt->hdr.rtm_version = INET_RTM_VERSION;
        if (msgType == INET_RTM_ADD) {
            pkt->hdr.rtm_inits = 4;
            pkt->hdr.rtm_rmx.rmx_expire = 0;
        }
        if (dst != NULL) {
            u32 step = inetRoundupSa(dst->sa_len);
            pkt->hdr.rtm_addrs |= INET_RTA_DST;
            sceNet_lib_4753D878(cursor, dst, (int)step);
            cursor += step;
        }
        if (gw != NULL) {
            u32 step = inetRoundupSa(gw->sa_len);
            pkt->hdr.rtm_addrs |= INET_RTA_GATEWAY;
            sceNet_lib_4753D878(cursor, gw, (int)step);
            cursor += step;
        }
        if (mask != NULL) {
            u32 step = inetRoundupSa(mask->sa_len);
            pkt->hdr.rtm_addrs |= INET_RTA_NETMASK;
            sceNet_lib_4753D878(cursor, mask, (int)step);
            cursor += step;
        }
        pkt->hdr.rtm_msglen = (u16)(cursor - (u8 *)pkt);
    }

    pkt->hdr.rtm_type = (u8)msgType;
    pkt->hdr.rtm_seq = seq;
    if (sceNetInetSend(sock, pkt, pkt->hdr.rtm_msglen, 0) < 0) {
        return SCE_NET_INET_ERROR_ARP_SEND;
    }

    do {
        recvLen = sceNetInetRecv(sock, pkt, (s32)sizeof(*pkt), 0);
        if (recvLen <= 0) {
            break;
        }
    } while (pkt->hdr.rtm_seq != seq || pkt->hdr.rtm_pid != thid);

    if (recvLen < 0) {
        return SCE_NET_INET_ERROR_ARP_RECV;
    }
    return 0;
}

int sceNetInet_lib_AC9D90A5(u32 *outMainDns, u32 *outAltDns)
{
    if (outMainDns != NULL) {
        sceKernelMemcpy(outMainDns, &g_dns_main, 4);
    }
    if (outAltDns != NULL) {
        sceKernelMemcpy(outAltDns, &g_dns_backup, 4);
    }
    return 0;
}

int sceNetInet_lib_AAF4895A(const u32 *inMainDns, const u32 *inAltDns)
{
    if (inMainDns != NULL) {
        sceKernelMemcpy(&g_dns_main, inMainDns, 4);
    }
    if (inAltDns != NULL) {
        sceKernelMemcpy(&g_dns_backup, inAltDns, 4);
    }
    return 0;
}

int sceNetInetGetTcpcbstat(s32 *bufLen, SceNetInetTcpcbstat *buf)
{
    int spl;
    InetInpcbReal *inp;
    InetInpcbReal *sentinel;
    s32 count = 0;
    SceNetInetTcpcbstat *outCur = buf;
    SceNetInetTcpcbstat *prev = NULL;

    if (sceKernelCheckThreadStack() < 4064) {
        return SCE_NET_INET_ERROR_INSUFFICIENT_STACK;
    }
    if (bufLen == NULL && buf == NULL) {
        return SCE_NET_INET_ERROR_EINVAL_PARAM;
    }

    spl = sceNet_lib_94B44F26();
    sentinel = (InetInpcbReal *)&g_tcbtable.inpt_queue_first;
    inp = g_tcbtable.inpt_queue_first;
    while (inp != sentinel && inp != NULL) {
        count++;
        if (buf != NULL) {
            if (*bufLen < count) {
                count--;
                break;
            }
            if (prev != NULL) {
                prev->next = outCur;
            }
            sceKernelMemset(outCur, 0, sizeof(*outCur));
            outCur->send_queue_len = inp->inp_socket->so_snd.sb_cc;
            outCur->recv_queue_len = inp->inp_socket->so_rcv.sb_cc;
            sceKernelMemcpy(&outCur->laddr, &inp->inp_laddr, 4);
            sceKernelMemcpy(&outCur->peer_addr, &inp->inp_faddr, 4);
            outCur->lport = inp->inp_lport;
            outCur->peer_port = inp->inp_fport;
            {
                const u16 *tcpWords = (const u16 *)inp->inp_ppcb;
                outCur->state = tcpWords[12];
            }
            prev = outCur;
            outCur++;
        }
        inp = inp->inp_queue_next;
    }

    sceNet_lib_515B2F33(spl);
    *bufLen = count * (s32)sizeof(SceNetInetTcpcbstat);
    return 0;
}

int sceNetInetGetUdpcbstat(s32 *bufLen, SceNetInetUdpcbstat *buf)
{
    int spl;
    InetInpcbReal *inp;
    InetInpcbReal *sentinel;
    s32 count = 0;
    SceNetInetUdpcbstat *outCur = buf;
    SceNetInetUdpcbstat *prev = NULL;

    if (sceKernelCheckThreadStack() < 4064) {
        return SCE_NET_INET_ERROR_INSUFFICIENT_STACK;
    }
    if (bufLen == NULL && buf == NULL) {
        return SCE_NET_INET_ERROR_EINVAL_PARAM;
    }

    spl = sceNet_lib_94B44F26();
    sentinel = (InetInpcbReal *)&g_udbtable.inpt_queue_first;
    inp = g_udbtable.inpt_queue_first;
    while (inp != sentinel && inp != NULL) {
        count++;
        if (buf != NULL) {
            if (*bufLen < count) {
                count--;
                break;
            }
            if (prev != NULL) {
                prev->next = outCur;
            }
            sceKernelMemset(outCur, 0, sizeof(*outCur));
            outCur->send_queue_len = inp->inp_socket->so_snd.sb_cc;
            outCur->recv_queue_len = inp->inp_socket->so_rcv.sb_cc;
            sceKernelMemcpy(&outCur->laddr, &inp->inp_laddr, 4);
            sceKernelMemcpy(&outCur->peer_addr, &inp->inp_faddr, 4);
            outCur->lport = inp->inp_lport;
            outCur->peer_port = inp->inp_fport;
            prev = outCur;
            outCur++;
        }
        inp = inp->inp_queue_next;
    }

    sceNet_lib_515B2F33(spl);
    *bufLen = count * (s32)sizeof(SceNetInetUdpcbstat);
    return 0;
}
