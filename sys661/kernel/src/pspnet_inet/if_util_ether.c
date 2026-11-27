#include "pspnet_inet_internal.h"

const u8 g_etherbroadcastaddr[6] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
const u8 g_ether_ipmulticast_min[6] = { 0x01, 0x00, 0x5E, 0x00, 0x00, 0x00 };
const u8 g_ether_ipmulticast_max[6] = { 0x01, 0x00, 0x5E, 0x7F, 0xFF, 0xFF };

static int (*g_pppoe_ether_cb)(InetMbuf *, u16, InetIfnet *);
static s32 g_pppoe_ether_active;

static int inetAddrMatchMasked(const u8 *target, const u8 *ifaAddr, const u8 *mask)
{
    u32 maskLen = mask[0];
    const u8 *maskPtr = mask + 2;
    const u8 *maskEnd = mask + maskLen;
    const u8 *addrPtr = ifaAddr + 2;
    const u8 *tgtPtr = target + 2;

    while (maskPtr < maskEnd) {
        if (((*tgtPtr ^ *addrPtr) & *maskPtr) != 0) {
            return 0;
        }
        maskPtr++;
        addrPtr++;
        tgtPtr++;
    }
    return 1;
}

/* [136] 0x00A2A0 */
INET_NOINLINE InetIfaddr *inetIfaWithNet(const InetSockaddr *addr)
{
    InetIfaddr *best = NULL;
    u32 family = addr->sa_family;
    s32 ifIndexCount = 0;
    InetIfnet **ifIndexTbl = NULL;
    InetIfaddr **ifAddrTbl = NULL;
    InetIfnet *ifp = NULL;

    sceNet_lib_C2EC2EEA(&ifIndexCount, &ifIndexTbl, &ifAddrTbl, &ifp);
    if (family == INET_AF_LINK) {
        const InetSockaddrDl *sdl = (const InetSockaddrDl *)addr;
        u32 idx = sdl->sdl_index;
        if (idx != 0 && (s32)idx <= ifIndexCount &&
            ifIndexTbl[idx]->if_output != (int (*)(InetIfnet *, InetMbuf *, InetSockaddr *, void *))sceNet_lib_8C55B410) {
            return ifAddrTbl[idx];
        }
    }

    while (ifp != NULL) {
        if (ifp->if_output != (int (*)(InetIfnet *, InetMbuf *, InetSockaddr *, void *))sceNet_lib_8C55B410) {
            InetIfaddr *ifa = ifp->if_addrhead;
            while (ifa != NULL) {
                if (ifa->ifa_addr->sa_family == family && ifa->ifa_netmask != NULL) {
                    if (inetAddrMatchMasked((const u8 *)addr,
                                            (const u8 *)ifa->ifa_addr,
                                            (const u8 *)ifa->ifa_netmask)) {
                        if (best == NULL ||
                            inetRnRefines((const u8 *)ifa->ifa_netmask,
                                          (const u8 *)best->ifa_netmask)) {
                            best = ifa;
                        }
                    }
                }
                ifa = ifa->ifa_next;
            }
        }
        ifp = ifp->if_next;
    }
    return best;
}

/* [137] 0x00A428 */
INET_NOINLINE InetIfaddr *inetIfaOfIfpForAddr(const InetSockaddr *addr, InetIfnet *ifp)
{
    InetIfaddr *fallback = NULL;
    u32 family = addr->sa_family;

    if (ifp->if_output == (int (*)(InetIfnet *, InetMbuf *, InetSockaddr *, void *))sceNet_lib_8C55B410 ||
        family >= 31) {
        return NULL;
    }

    InetIfaddr *ifa = ifp->if_addrhead;
    while (ifa != NULL) {
        if (ifa->ifa_addr->sa_family == family) {
            fallback = ifa;
            if (ifa->ifa_netmask == NULL) {
                if (sceNet_lib_4BF83CF4(addr, ifa->ifa_addr, addr->sa_len) == 0) {
                    return ifa;
                }
                if (ifa->ifa_dstaddr != NULL &&
                    sceNet_lib_4BF83CF4(addr, ifa->ifa_dstaddr, addr->sa_len) == 0) {
                    return ifa;
                }
            } else {
                if (inetAddrMatchMasked((const u8 *)addr,
                                        (const u8 *)ifa->ifa_addr,
                                        (const u8 *)ifa->ifa_netmask)) {
                    return ifa;
                }
            }
        }
        ifa = ifa->ifa_next;
    }
    return fallback;
}

/* [138] 0x00A55C */
INET_NOINLINE int inetRtPurgeIfWalk(InetRadixNode *rn, void *ctx)
{
    InetRtentry *rt = (InetRtentry *)rn;
    InetIfnet *ifp = (InetIfnet *)ctx;
    if (rt->rt_ifp == ifp) {
        inetRtRequest(INET_RTM_DELETE,
                      (InetSockaddr *)rt->rt_nodes[0].rn_u.rn_leaf.rn_key,
                      rt->rt_gateway,
                      (InetSockaddr *)rt->rt_nodes[0].rn_u.rn_leaf.rn_mask,
                      (u32)rt->rt_flags,
                      NULL);
    }
    return 0;
}

/* [139] 0x00A5A4 */
INET_NOINLINE InetIfaddr *inetIfaWithAddr(const InetSockaddr *addr)
{
    InetIfnet *ifp = NULL;
    sceNet_lib_C2EC2EEA(NULL, NULL, NULL, &ifp);

    while (ifp != NULL) {
        if (ifp->if_output != (int (*)(InetIfnet *, InetMbuf *, InetSockaddr *, void *))sceNet_lib_8C55B410) {
            InetIfaddr *ifa = ifp->if_addrhead;
            while (ifa != NULL) {
                if (ifa->ifa_addr->sa_family == addr->sa_family) {
                    if (sceNet_lib_4BF83CF4(addr, ifa->ifa_addr, addr->sa_len) == 0) {
                        return ifa;
                    }
                    if ((ifp->if_flags & INET_IFF_BROADCAST) != 0 &&
                        ifa->ifa_dstaddr != NULL &&
                        ifa->ifa_dstaddr->sa_len != 0 &&
                        sceNet_lib_4BF83CF4(ifa->ifa_dstaddr, addr, addr->sa_len) == 0) {
                        return ifa;
                    }
                }
                ifa = ifa->ifa_next;
            }
        }
        ifp = ifp->if_next;
    }
    return NULL;
}

/* [140] 0x00A6A0 */
INET_NOINLINE InetIfaddr *inetIfaWithDstAddr(const InetSockaddr *addr)
{
    InetIfnet *ifp = NULL;
    sceNet_lib_C2EC2EEA(NULL, NULL, NULL, &ifp);

    while (ifp != NULL) {
        if (ifp->if_output != (int (*)(InetIfnet *, InetMbuf *, InetSockaddr *, void *))sceNet_lib_8C55B410 &&
            (ifp->if_flags & INET_IFF_POINTOPOINT) != 0) {
            InetIfaddr *ifa = ifp->if_addrhead;
            while (ifa != NULL) {
                if (ifa->ifa_addr->sa_family == addr->sa_family &&
                    ifa->ifa_dstaddr != NULL &&
                    sceNet_lib_4BF83CF4(addr, ifa->ifa_dstaddr, addr->sa_len) == 0) {
                    return ifa;
                }
                ifa = ifa->ifa_next;
            }
        }
        ifp = ifp->if_next;
    }
    return NULL;
}

/* [141] 0x00A77C */
INET_NOINLINE InetIfaddr *inetIfaWithAnyAddr(const InetSockaddr *addr)
{
    InetIfaddr *ifa = inetIfaWithAddr(addr);
    if (ifa != NULL) {
        return ifa;
    }
    ifa = inetIfaWithDstAddr(addr);
    if (ifa != NULL) {
        return ifa;
    }
    return inetIfaWithNet(addr);
}

/* [142] 0x00A944 */
INET_NOINLINE void inetIfHookAttach(InetIfnet *ifp)
{
    inetRtIfAnnounceMsg(ifp, 0);
}

/* [143] 0x00A960 */
INET_NOINLINE int inetIfHookIfaDown(InetIfaddr *ifa)
{
    return inetRtInit(ifa, INET_RTM_DELETE, 0);
}

/* [144] 0x00A980 */
INET_NOINLINE void inetIfHookPurgeProtos(InetIfnet *ifp, int family)
{
    InetSocket tempSock;
    InetDomain *dp;
    InetProtosw *pr;

    sceKernelMemset(&tempSock, 0, sizeof(tempSock));
    dp = inetFindDomain(family);
    for (pr = dp->dom_protosw; pr < dp->dom_protosw_end; pr++) {
        tempSock.so_proto = pr;
        if (pr->pr_usrreq != NULL) {
            pr->pr_usrreq(&tempSock, INET_PRU_PURGEIF, NULL, NULL, (InetMbuf *)ifp, NULL);
        }
    }
}

/* [145] 0x00AA24 */
INET_NOINLINE void inetIfHookPurgeRoutes(InetIfnet *ifp)
{
    int idx;
    for (idx = 0; idx < 32; idx++) {
        InetRadixNodeHead *rnh = g_rt_tables[idx];
        if (rnh != NULL) {
            rnh->rnh_walktree(rnh, inetRtPurgeIfWalk, ifp);
        }
    }
}

/* [146] 0x00AA9C */
INET_NOINLINE void inetIfHookDetach(InetIfnet *ifp)
{
    inetRtIfAnnounceMsg(ifp, 1);
}

/* [147] 0x00AAB8 */
INET_NOINLINE void inetIfHookCtlinputDown(InetIfnet *ifp)
{
    InetIfaddr *ifa = ifp->if_addrhead;
    while (ifa != NULL) {
        inetPfCtlinput(0, ifa->ifa_addr);
        ifa = ifa->ifa_next;
    }
}

/* [148] 0x00AAF8 */
INET_NOINLINE void inetIfHookUpMsg(InetIfnet *ifp)
{
    inetRtIfMsg(ifp);
}

/* [149] 0x00AB14 */
INET_NOINLINE void inetIfHookDownMsg(InetIfnet *ifp)
{
    inetRtIfMsg(ifp);
}

/* [150] 0x00AB30 */
INET_NOINLINE int inetIfHookNoop(void)
{
    return 0;
}

/* [151] 0x00AB38 */
INET_NOINLINE int inetIfHookCheckProto(InetSocket *so, int *outUnsupported)
{
    if (so->so_proto == NULL) {
        *outUnsupported = 1;
        return INET_EOPNOTSUPP;
    }
    return 0;
}

/* [152] 0x00AB5C */
INET_NOINLINE int inetIfHookControl(InetSocket *so, u32 cmd, void *data, InetIfnet *ifp)
{
    return so->so_proto->pr_usrreq(so, INET_PRU_CONTROL,
                                   (InetMbuf *)(uintptr_t)cmd,
                                   (InetMbuf *)data,
                                   (InetMbuf *)ifp,
                                   NULL);
}

/* [153] 0x00AB90 */
INET_NOINLINE int inetIfHookInit(void)
{
    return sceNet_lib_976AB1E9(
        inetLinkRtRequest,
        inetIfHookEmpty,
        inetIfHookAttach,
        inetIfHookIfaDown,
        inetIfHookPurgeProtos,
        inetIfHookPurgeRoutes,
        inetIfHookDetach,
        inetIfHookCtlinputDown,
        inetIfHookUpMsg,
        inetIfHookDownMsg,
        inetIfHookNoop,
        inetIfHookCheckProto,
        inetIfHookControl);
}

/* [154] 0x00AC24 */
INET_NOINLINE int inetIfHookTerm(void)
{
    return sceNet_lib_4C8FD452();
}

/* [155] 0x00AC40 */
INET_NOINLINE void inetLinkRtRequest(int cmd, InetRtentry *rt, InetRtAddrinfo *rtInfo)
{
    if (cmd != INET_RTM_ADD) {
        return;
    }
    if (rt->rt_ifa == NULL || rt->rt_ifa->ifa_ifp == NULL) {
        return;
    }
    InetSockaddr *dst = (InetSockaddr *)rt->rt_nodes[0].rn_u.rn_leaf.rn_key;
    if (dst == NULL) {
        return;
    }
    InetIfaddr *ifa = inetIfaOfIfpForAddr(dst, rt->rt_ifa->ifa_ifp);
    if (ifa == NULL) {
        return;
    }
    InetIfaddr *oldIfa = rt->rt_ifa;
    oldIfa->ifa_refcnt--;
    if (oldIfa->ifa_refcnt == 0) {
        sceNet_lib_EDCC871E(oldIfa);
    }
    rt->rt_ifa = ifa;
    ifa->ifa_refcnt++;
    if (ifa->ifa_rtrequest != NULL &&
        ifa->ifa_rtrequest != (void (*)(int, void *, void *))sceNet_lib_EB6DE71A) {
        ifa->ifa_rtrequest(INET_RTM_ADD, rt, rtInfo);
    }
}

/* [156] 0x00AD10 */
INET_NOINLINE void inetIfHookEmpty(void)
{
}

/* [157] 0x00AD18 */
INET_NOINLINE int inetEtherMulticastRange(const InetSockaddr *sa, u8 *loAddr, u8 *hiAddr, int *outHandled)
{
    if (sa->sa_family == INET_AF_INET) {
        const InetSockaddrIn *sin = (const InetSockaddrIn *)sa;
        const u8 *ipBytes = (const u8 *)&sin->sin_addr;
        if (sin->sin_addr == 0) {
            sceNet_lib_2F305274(g_ether_ipmulticast_min, loAddr, 6);
            sceNet_lib_2F305274(g_ether_ipmulticast_max, hiAddr, 6);
        } else {
            loAddr[0] = 0x01;
            loAddr[1] = 0x00;
            loAddr[2] = 0x5E;
            loAddr[3] = (u8)(ipBytes[1] & 0x7F);
            loAddr[4] = ipBytes[2];
            loAddr[5] = ipBytes[3];
            sceNet_lib_2F305274(loAddr, hiAddr, 6);
        }
        *outHandled = 1;
    }
    return 0;
}

/* [158] 0x00ADE8 */
INET_NOINLINE int inetEtherPppoeDispatch(InetIfnet *ifp, InetMbuf *m, u16 etype, InetIfnet *srcIfp)
{
    (void)ifp;
    if (g_pppoe_ether_active > 0) {
        return g_pppoe_ether_cb(m, etype, srcIfp);
    }
    return etype;
}

/* [159] 0x00AE2C */
INET_NOINLINE InetIfQueue *inetEtherInputDispatch(InetIfnet *ifp, InetMbuf *m, u16 etype, int unused,
                                                  InetIfQueue **outQueue, int *outIsr, int *outHandled)
{
    (void)ifp;
    (void)unused;
    if (etype == 0x0806) {
        *outIsr = 28;
        *outQueue = &g_arpintrq;
        return &g_arpintrq;
    }
    if (etype == 0x8035) {
        inetRevarpInput(m);
        *outHandled = 1;
        return NULL;
    }
    if (etype == 0x0800) {
        *outIsr = 2;
        *outQueue = &g_ipintrq;
        return &g_ipintrq;
    }
    sceNetIfhandle_lib_F56FAC82(m);
    *outHandled = 1;
    return NULL;
}

/* [160] 0x00AEC4 */
INET_NOINLINE int inetEtherSetPppoeInput(int (*cb)(InetMbuf *, u16, InetIfnet *))
{
    g_pppoe_ether_cb = cb;
    g_pppoe_ether_active = 1;
    return 0;
}

/* [161] 0x00AEE0 */
INET_NOINLINE int inetEtherClearPppoeInput(void)
{
    g_pppoe_ether_active = 0;
    g_pppoe_ether_cb = NULL;
    return 0;
}

/* [162] 0x00AEF8 */
INET_NOINLINE int inetEtherHookInit(void)
{
    return sceNet_lib_5ED457BE(inetEtherOutput, inetEtherPppoeDispatch,
                               inetEtherInputDispatch, inetEtherMulticastRange);
}

/* [163] 0x00AF30 */
INET_NOINLINE int inetEtherHookTerm(void)
{
    return sceNet_lib_31F3CDA1();
}

/* [164] 0x00AF4C */
INET_NOINLINE InetIfQueue *sceNetInet_lib_D7928A6D(void)
{
    return &g_ipintrq;
}

/* [165] 0x00AF58 */
INET_NOINLINE int inetEtherOutput(InetIfnet *ifp, InetMbuf *m, InetSockaddr *dst, InetRtentry *rt0)
{
    int hasSrcMac = 0;
    InetMbuf *loopCopy = NULL;
    InetRtentry *rt = rt0;
    InetRtentry *origRt = rt0;
    u16 etherType = 0;
    u8 srcMac[6];
    u8 dstMac[6];
    int err = INET_ENETDOWN;
    int spl;

    if ((ifp->if_flags & (INET_IFF_UP | INET_IFF_RUNNING)) != (INET_IFF_UP | INET_IFF_RUNNING)) {
        if (m != NULL) {
            sceNetIfhandle_lib_F56FAC82(m);
        }
        return err;
    }

    if (rt != NULL) {
        if ((rt->rt_flags & INET_RTF_UP) == 0) {
            rt = inetRtAlloc1(dst, 1);
            origRt = rt;
            if (rt == NULL) {
                sceNetIfhandle_lib_F56FAC82(m);
                return INET_EHOSTUNREACH;
            }
            rt->rt_refcnt--;
            if (rt->rt_ifp != ifp) {
                return rt->rt_ifp->if_output(ifp, m, dst, rt);
            }
        }
        if ((rt->rt_flags & INET_RTF_GATEWAY) != 0 && dst->sa_family != 6) {
            InetRtentry *gw = rt->rt_gwroute;
            if (gw != NULL && (gw->rt_flags & INET_RTF_UP) != 0) {
                rt = gw;
            } else {
                if (gw != NULL) {
                    inetRtFree(gw);
                }
                gw = inetRtAlloc1(origRt->rt_gateway, 1);
                origRt->rt_gwroute = gw;
                rt = gw;
                if (gw == NULL) {
                    sceNetIfhandle_lib_F56FAC82(m);
                    return INET_EHOSTUNREACH;
                }
                if ((gw->rt_flags & INET_RTF_GATEWAY) != 0 || gw->rt_ifp != ifp) {
                    origRt->rt_gwroute = NULL;
                    gw->rt_refcnt--;
                    sceNetIfhandle_lib_F56FAC82(m);
                    return INET_EHOSTUNREACH;
                }
            }
        }
        if ((rt->rt_flags & INET_RTF_REJECT) != 0) {
            if (rt->rt_rmx.rmx_expire == 0 || sceNet_lib_75D9985C() < rt->rt_rmx.rmx_expire) {
                sceNetIfhandle_lib_F56FAC82(m);
                return (rt != origRt) ? INET_EHOSTUNREACH : INET_EHOSTDOWN;
            }
        }
    }

    switch (dst->sa_family) {
    case INET_AF_INET:
        if ((m->m_flags & INET_M_BCAST) != 0) {
            sceNet_lib_2F305274(g_etherbroadcastaddr, dstMac, 6);
        } else if ((m->m_flags & INET_M_MCAST) != 0) {
            const u8 *ipBytes = (const u8 *)&((const InetSockaddrIn *)dst)->sin_addr;
            dstMac[0] = 0x01;
            dstMac[1] = 0x00;
            dstMac[2] = 0x5E;
            dstMac[3] = (u8)(ipBytes[1] & 0x7F);
            dstMac[4] = ipBytes[2];
            dstMac[5] = ipBytes[3];
        } else {
            if (!inetArpResolve(ifp, rt, m, dst, dstMac)) {
                return 0;
            }
        }
        if ((m->m_flags & INET_M_BCAST) != 0 && (ifp->if_flags & INET_IFF_SIMPLEX) != 0) {
            loopCopy = sceNetIfhandle_lib_456E3146(m, 0, 1000000000, INET_M_DONTWAIT);
        }
        etherType = inetBswap16(0x0800);
        if (loopCopy != NULL) {
            inetLoOutput(ifp, loopCopy, dst, rt);
        }
        break;

    case 28: { /* AF_ARP */
        u8 *arpHdr = m->m_data;
        const u8 *targetMac = g_etherbroadcastaddr;
        u16 arpOp;
        if ((m->m_flags & INET_M_BCAST) == 0) {
            targetMac = arpHdr + arpHdr[4] + arpHdr[5] + 8;
        }
        sceNet_lib_2F305274(targetMac, dstMac, 6);
        arpOp = inetBswap16((u16)(((u16)arpHdr[7] << 8) | arpHdr[6]));
        {
            u16 hrd = inetBswap16(1);
            arpHdr[0] = (u8)(hrd & 0xFF);
            arpHdr[1] = (u8)(hrd >> 8);
        }
        if (arpOp >= 3 && arpOp < 5) {
            etherType = inetBswap16(0x8035);
        } else {
            etherType = inetBswap16(0x0806);
        }
        break;
    }

    case INET_AF_UNSPEC: {
        const u8 *rawHdr = dst->sa_data;
        sceNet_lib_2F305274(rawHdr, dstMac, 6);
        etherType = (u16)(((u16)rawHdr[13] << 8) | rawHdr[12]);
        break;
    }

    case 30: { /* pseudo_AF_HDRCMPLT */
        const u8 *rawHdr = dst->sa_data;
        hasSrcMac = 1;
        sceNet_lib_2F305274(rawHdr + 6, srcMac, 6);
        sceNet_lib_2F305274(rawHdr, dstMac, 6);
        etherType = (u16)(((u16)rawHdr[13] << 8) | rawHdr[12]);
        break;
    }

    default:
        sceNetIfhandle_lib_F56FAC82(m);
        return INET_EAFNOSUPPORT;
    }

    if (etherType == 0) {
        etherType = inetBswap16((u16)M_PKTLEN(m));
    }

    /* Prepend 14-byte Ethernet header */
    if ((m->m_flags & INET_M_EXT) == 0) {
        u8 *base = ((m->m_flags & INET_M_PKTHDR) != 0) ? M_PKTDAT(m) : M_DAT(m);
        if ((u32)(m->m_data - base) >= 14) {
            m->m_data -= 14;
            m->m_len += 14;
        } else {
            m = sceNetIfhandle_lib_C3325FDC(m, 14, INET_M_DONTWAIT);
        }
    } else {
        m = sceNetIfhandle_lib_C3325FDC(m, 14, INET_M_DONTWAIT);
    }

    if (m == NULL) {
        return INET_ENOBUFS;
    }
    if ((m->m_flags & INET_M_PKTHDR) != 0) {
        M_PKTLEN(m) += 14;
    }

    {
        u8 *ethHdr = m->m_data;
        const u8 *srcPtr = hasSrcMac ? srcMac :
            (ifp->if_sadl->sdl_data + ifp->if_sadl->sdl_nlen);
        sceNet_lib_2F305274(&etherType, ethHdr + 12, 2);
        sceNet_lib_2F305274(dstMac, ethHdr, 6);
        sceNet_lib_2F305274(srcPtr, ethHdr + 6, 6);
    }

    spl = sceNet_lib_94B44F26();
    if (ifp->if_snd.ifq_len >= ifp->if_snd.ifq_maxlen) {
        ifp->if_snd.ifq_drops++;
        sceNet_lib_515B2F33(spl);
        sceNetIfhandle_lib_F56FAC82(m);
        return INET_ENOBUFS;
    }

    ifp->if_data.ifi_obytes += (u64)(s64)M_PKTLEN(m);
    if ((m->m_flags & INET_M_MCAST) != 0) {
        ifp->if_data.ifi_omcasts++;
    }
    m->m_nextpkt = NULL;
    if (ifp->if_snd.ifq_tail != NULL) {
        ifp->if_snd.ifq_tail->m_nextpkt = m;
    } else {
        ifp->if_snd.ifq_head = m;
    }
    ifp->if_snd.ifq_tail = m;
    ifp->if_snd.ifq_len++;
    if ((ifp->if_flags & INET_IFF_OACTIVE) == 0) {
        ifp->if_start(ifp);
    }
    sceNet_lib_515B2F33(spl);
    return 0;
}
