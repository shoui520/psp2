#include "pspnet_inet_internal.h"

static s32 g_arpt_prune = 300;
static s32 g_arpt_keep = 1200;
static s32 g_arpt_down = 20;
InetIfQueue g_arpintrq = { NULL, NULL, 0, 50, 0 };
static s32 g_arp_maxtries = 5;
static s32 g_useloopback = 1;

static s32 g_arp_timer_initialized;
static s32 g_revarp_got_reply;
static s32 g_revarp_in_progress;
static InetIfnet *g_revarp_ifp;
static InetSockaddrIn g_arp_sin;
static u32 g_revarp_myip;
static u32 g_revarp_srvip;

static s32 g_arp_allocated;
static InetLlinfoArp *g_llinfo_arp;
static s32 g_arp_inuse;
static s32 g_arp_count;
static s32 g_arp_reserved_stat;
static InetCallout g_arp_callout;

static InetSockaddrDl g_null_sdl = { 20, INET_AF_LINK, 0, 0, 0, 0, 0, { 0 } };

static void inetArpDrain(void);
static void inetArpInit(void);
static void inetArpTimer(void *arg);

static InetProtosw g_arpsw[1] = {
    {
        0,
        NULL,
        0,
        0,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        inetArpDrain,
        NULL
    }
};

InetDomain g_arpdomain = {
    28, /* AF_ARP */
    "arp",
    inetArpInit,
    NULL,
    NULL,
    g_arpsw,
    &g_arpsw[1],
    NULL,
    NULL,
    0,
    0
};

/* [222] 0x011618 */
INET_NOINLINE void inetArpRtRequest(int req, void *rtPtr, void *rtInfo)
{
    InetRtentry *rt = (InetRtentry *)rtPtr;
    InetSockaddrDl *sdl = (InetSockaddrDl *)rt->rt_gateway;
    InetLlinfoArp *la = (InetLlinfoArp *)rt->rt_llinfo;
    int spl;
    (void)rtInfo;

    if (g_arp_timer_initialized == 0) {
        g_arp_timer_initialized = 1;
        sceNet_lib_25CC373A(&g_arp_callout);
        sceNet_lib_7C86FBA4(&g_arp_callout, 100, inetArpTimer, NULL);
    }

    if ((rt->rt_flags & INET_RTF_GATEWAY) != 0) {
        return;
    }

    spl = sceNet_lib_94B44F26();
    g_arp_inuse++;
    sceNet_lib_515B2F33(spl);

    switch (req) {
    case INET_RTM_ADD:
        if ((rt->rt_flags & INET_RTF_HOST) == 0 &&
            ((InetSockaddrIn *)rt->rt_nodes[0].rn_u.rn_leaf.rn_mask)->sin_addr != 0xFFFFFFFFU) {
            rt->rt_flags |= INET_RTF_CLONING;
        }
        if ((rt->rt_flags & INET_RTF_CLONING) != 0) {
            inetRtSetgate(rt, (InetSockaddr *)rt->rt_nodes[0].rn_u.rn_leaf.rn_key,
                          (InetSockaddr *)&g_null_sdl);
            sdl = (InetSockaddrDl *)rt->rt_gateway;
            sdl->sdl_type = rt->rt_ifp->if_data.ifi_type;
            sdl->sdl_index = rt->rt_ifp->if_index;
            rt->rt_rmx.rmx_expire = sceNet_lib_75D9985C();
            break;
        }
        if ((rt->rt_flags & INET_RTF_PROTO2) != 0) {
            const u32 *ipPtr = &((InetSockaddrIn *)rt->rt_nodes[0].rn_u.rn_leaf.rn_key)->sin_addr;
            inetArpRequest(rt->rt_ifp, ipPtr, ipPtr, sdl->sdl_data + sdl->sdl_nlen);
        }
        /* fall through to RTM_RESOLVE setup */
    case INET_RTM_RESOLVE:
        if (sdl->sdl_family == INET_AF_LINK && sdl->sdl_len >= 20) {
            sdl->sdl_type = rt->rt_ifp->if_data.ifi_type;
            sdl->sdl_index = rt->rt_ifp->if_index;
            if (la == NULL) {
                la = (InetLlinfoArp *)sceNetIfhandle_lib_15CFE3C0(sizeof(InetLlinfoArp));
                rt->rt_llinfo = la;
                if (la != NULL) {
                    InetInIfaddr *ia = (InetInIfaddr *)rt->rt_ifa;
                    g_arp_count++;
                    g_arp_allocated++;
                    sceNet_lib_8B615F52(la, sizeof(InetLlinfoArp));
                    la->la_rt = rt;
                    rt->rt_flags |= INET_RTF_LLINFO;
                    la->la_next = g_llinfo_arp;
                    if (g_llinfo_arp != NULL) {
                        g_llinfo_arp->la_prev = &la->la_next;
                    }
                    la->la_prev = &g_llinfo_arp;
                    g_llinfo_arp = la;

                    if (((InetSockaddrIn *)rt->rt_nodes[0].rn_u.rn_leaf.rn_key)->sin_addr ==
                        ia->ia_addr.sin_addr) {
                        InetIfnet *ifp = rt->rt_ifp;
                        rt->rt_rmx.rmx_expire = 0;
                        sdl->sdl_alen = ifp->if_data.ifi_addrlen;
                        sceNet_lib_2F305274(ifp->if_sadl->sdl_data + ifp->if_sadl->sdl_nlen,
                                            sdl->sdl_data + sdl->sdl_nlen,
                                            ifp->if_data.ifi_addrlen);
                        if (g_useloopback != 0) {
                            rt->rt_ifp = &g_lo0if;
                        }
                    }
                }
            }
        }
        break;

    case INET_RTM_DELETE:
        if (la != NULL) {
            InetMbuf *held;
            g_arp_count--;
            if (la->la_next != NULL) {
                la->la_next->la_prev = la->la_prev;
            }
            *la->la_prev = la->la_next;
            rt->rt_llinfo = NULL;
            rt->rt_flags &= ~INET_RTF_LLINFO;

            spl = sceNet_lib_94B44F26();
            held = la->la_hold;
            la->la_hold = NULL;
            sceNet_lib_515B2F33(spl);

            if (held != NULL) {
                sceNetIfhandle_lib_F56FAC82(held);
            }
            sceNetIfhandle_lib_76BAD213(la);
        }
        break;

    default:
        break;
    }

    spl = sceNet_lib_94B44F26();
    g_arp_inuse--;
    sceNet_lib_515B2F33(spl);
}

/* [223] 0x011958 */
INET_NOINLINE void inetArpRequest(InetIfnet *ifp, const u32 *sip, const u32 *tip, const u8 *enaddr)
{
    InetMbuf *m = sceNetIfhandle_lib_4CF15C43(INET_M_DONTWAIT, INET_MT_DATA);
    u32 pktLen;
    u8 *arpHdr;
    u16 pro;
    u16 op;
    InetSockaddr sa;

    if (m == NULL) {
        return;
    }
    pktLen = (u32)(2 * ifp->if_data.ifi_addrlen + 16);
    m->m_data += ((208 - pktLen) & ~3U);
    m->m_len = (s32)pktLen;
    M_PKTLEN(m) = (s32)pktLen;
    arpHdr = m->m_data;
    sceNet_lib_8B615F52(arpHdr, (int)pktLen);

    pro = inetBswap16(0x0800);
    op = inetBswap16(1);
    arpHdr[2] = (u8)(pro & 0xFF);
    arpHdr[3] = (u8)(pro >> 8);
    arpHdr[4] = ifp->if_data.ifi_addrlen;
    arpHdr[5] = 4;
    arpHdr[6] = (u8)(op & 0xFF);
    arpHdr[7] = (u8)(op >> 8);

    sceNet_lib_2F305274(enaddr, arpHdr + 8, arpHdr[4]);
    sceNet_lib_2F305274(sip, arpHdr + 8 + arpHdr[4], arpHdr[5]);
    sceNet_lib_2F305274(tip, arpHdr + 8 + 2 * arpHdr[4] + arpHdr[5], arpHdr[5]);

    sa.sa_len = 2;
    sa.sa_family = 28; /* AF_ARP */
    m->m_flags |= INET_M_BCAST;
    ifp->if_output(ifp, m, &sa, NULL);
}

/* [224] 0x011AB8 */
INET_NOINLINE int inetArpResolve(InetIfnet *ifp, InetRtentry *rt, InetMbuf *m,
                                 InetSockaddr *dst, u8 *destEnaddr)
{
    InetLlinfoArp *la;
    InetSockaddrDl *sdl;
    InetMbuf *oldHold;
    int spl;

    if (rt != NULL) {
        la = (InetLlinfoArp *)rt->rt_llinfo;
    } else {
        la = inetArpLookup(&((InetSockaddrIn *)dst)->sin_addr, 1, 0);
        if (la != NULL) {
            rt = la->la_rt;
        }
    }

    if (la == NULL || rt == NULL) {
        sceNetIfhandle_lib_F56FAC82(m);
        return 0;
    }

    sdl = (InetSockaddrDl *)rt->rt_gateway;
    if ((rt->rt_rmx.rmx_expire == 0 || sceNet_lib_75D9985C() < rt->rt_rmx.rmx_expire) &&
        sdl->sdl_family == INET_AF_LINK && sdl->sdl_alen != 0) {
        u32 copyLen = ifp->if_data.ifi_addrlen;
        if (sdl->sdl_alen < copyLen) {
            copyLen = sdl->sdl_alen;
        }
        sceNet_lib_2F305274(sdl->sdl_data + sdl->sdl_nlen, destEnaddr, (int)copyLen);
        return 1;
    }

    spl = sceNet_lib_94B44F26();
    oldHold = la->la_hold;
    la->la_hold = m;
    sceNet_lib_515B2F33(spl);
    if (oldHold != NULL) {
        sceNetIfhandle_lib_F56FAC82(oldHold);
    }

    if (rt->rt_rmx.rmx_expire != 0) {
        u32 now;
        rt->rt_flags &= ~INET_RTF_REJECT;
        now = sceNet_lib_75D9985C();
        if (la->la_asked == 0 || rt->rt_rmx.rmx_expire != now) {
            rt->rt_rmx.rmx_expire = now;
            if (la->la_asked++ < g_arp_maxtries) {
                const u32 *srcIp = &((InetSockaddrIn *)rt->rt_ifa->ifa_addr)->sin_addr;
                const u32 *dstIp = &((InetSockaddrIn *)dst)->sin_addr;
                inetArpRequest(ifp, srcIp, dstIp,
                               ifp->if_sadl->sdl_data + ifp->if_sadl->sdl_nlen);
            } else {
                rt->rt_flags |= INET_RTF_REJECT;
                rt->rt_rmx.rmx_expire += (u32)g_arpt_down;
                la->la_asked = 0;
            }
        }
    }
    return 0;
}

/* [225] 0x011CA8 */
INET_NOINLINE void inetArpIntr(void)
{
    while (g_arpintrq.ifq_head != NULL) {
        int spl = sceNet_lib_94B44F26();
        InetMbuf *m = g_arpintrq.ifq_head;
        if (m != NULL) {
            g_arpintrq.ifq_head = m->m_nextpkt;
            if (g_arpintrq.ifq_head == NULL) {
                g_arpintrq.ifq_tail = NULL;
            }
            m->m_nextpkt = NULL;
            g_arpintrq.ifq_len--;
        }
        sceNet_lib_515B2F33(spl);

        if (m != NULL) {
            u8 *arpHdr = m->m_data;
            if (m->m_len >= 8 && arpHdr != NULL &&
                m->m_len >= (s32)(2 * (arpHdr[4] + arpHdr[5]) + 8)) {
                u16 pro = inetBswap16((u16)(((u16)arpHdr[3] << 8) | arpHdr[2]));
                if (pro == 0x0800 || pro == 0x1000) {
                    inetInArpInput(m);
                    continue;
                }
            }
            sceNetIfhandle_lib_F56FAC82(m);
        }
    }
}

/* [226] 0x011DC0 */
INET_NOINLINE void inetInArpInput(InetMbuf *m)
{
    u8 *arpHdr = m->m_data;
    InetIfnet *ifp = M_RCVIF(m);
    u16 op = inetBswap16((u16)(((u16)arpHdr[7] << 8) | arpHdr[6]));
    u32 spa = 0;
    u32 tpa = 0;
    u32 myAddr = 0;
    int foundIa = 0;
    InetInIfaddr *ia;

    sceNet_lib_2F305274(arpHdr + 8 + arpHdr[4], &spa, 4);
    sceNet_lib_2F305274(arpHdr + 8 + 2 * arpHdr[4] + arpHdr[5], &tpa, 4);

    if (tpa == 0 || spa == 0) {
        sceNetIfhandle_lib_F56FAC82(m);
        return;
    }

    /* Search local address matching tpa, then spa, then first AF_INET on ifp */
    ia = g_in_ifaddr_hashtbl[tpa % 509U];
    while (ia != NULL) {
        if (ia->ia_addr.sin_addr == tpa && (ia->ia_ifa.ifa_ifp->if_flags & INET_IFF_UP) != 0) {
            if (ia->ia_ifa.ifa_ifp != ifp) {
                InetInIfaddr *scan = ia->ia_hash_next;
                while (scan != NULL) {
                    if (scan->ia_addr.sin_addr == tpa && scan->ia_ifa.ifa_ifp == ifp) {
                        ia = scan;
                        break;
                    }
                    scan = scan->ia_hash_next;
                }
                if (scan == NULL) {
                    break;
                }
            }
            myAddr = ia->ia_addr.sin_addr;
            foundIa = 1;
            break;
        }
        ia = ia->ia_hash_next;
    }

    if (!foundIa) {
        ia = g_in_ifaddr_hashtbl[spa % 509U];
        while (ia != NULL) {
            if (ia->ia_addr.sin_addr == spa && (ia->ia_ifa.ifa_ifp->if_flags & INET_IFF_UP) != 0) {
                if (ia->ia_ifa.ifa_ifp != ifp) {
                    InetInIfaddr *scan = ia->ia_hash_next;
                    while (scan != NULL) {
                        if (scan->ia_addr.sin_addr == spa && scan->ia_ifa.ifa_ifp == ifp) {
                            ia = scan;
                            break;
                        }
                        scan = scan->ia_hash_next;
                    }
                    if (scan == NULL) {
                        break;
                    }
                }
                myAddr = ia->ia_addr.sin_addr;
                foundIa = 1;
                break;
            }
            ia = ia->ia_hash_next;
        }
    }

    if (!foundIa) {
        InetIfaddr *ifa = ifp->if_addrhead;
        while (ifa != NULL && ifa->ifa_addr->sa_family != INET_AF_INET) {
            ifa = ifa->ifa_next;
        }
        if (ifa == NULL) {
            sceNetIfhandle_lib_F56FAC82(m);
            return;
        }
        myAddr = ((InetInIfaddr *)ifa)->ia_addr.sin_addr;
    }

    /* Ignore ARP packets sourced from our own hardware address or broadcast */
    if (sceNet_lib_4BF83CF4(arpHdr + 8, ifp->if_sadl->sdl_data + ifp->if_sadl->sdl_nlen, arpHdr[4]) == 0 ||
        sceNet_lib_4BF83CF4(arpHdr + 8, g_etherbroadcastaddr, arpHdr[4]) == 0) {
        sceNetIfhandle_lib_F56FAC82(m);
        return;
    }

    if (spa == myAddr) {
        tpa = myAddr;
    } else {
        InetLlinfoArp *la = inetArpLookup(&spa, tpa == myAddr, 0);
        if (la != NULL && la->la_rt != NULL && la->la_rt->rt_gateway != NULL) {
            InetRtentry *rt = la->la_rt;
            InetSockaddrDl *sdl = (InetSockaddrDl *)rt->rt_gateway;
            int update = 0;

            if (sdl->sdl_alen == 0) {
                update = 1;
            } else if (sceNet_lib_4BF83CF4(arpHdr + 8, sdl->sdl_data + sdl->sdl_nlen, sdl->sdl_alen) == 0) {
                update = 1;
            } else if ((rt->rt_flags & INET_RTF_STATIC) == 0 && rt->rt_ifp == ifp) {
                update = 1;
            } else {
                sceNetIfhandle_lib_F56FAC82(m);
                return;
            }

            if (update && ifp->if_data.ifi_addrlen == arpHdr[4]) {
                int spl;
                InetMbuf *held;
                sdl->sdl_alen = arpHdr[4];
                sceNet_lib_2F305274(arpHdr + 8, sdl->sdl_data + sdl->sdl_nlen, arpHdr[4]);
                if (rt->rt_rmx.rmx_expire != 0) {
                    rt->rt_rmx.rmx_expire = sceNet_lib_75D9985C() + (u32)g_arpt_keep;
                }
                la->la_asked = 0;
                rt->rt_flags &= ~INET_RTF_REJECT;

                spl = sceNet_lib_94B44F26();
                held = la->la_hold;
                la->la_hold = NULL;
                sceNet_lib_515B2F33(spl);

                if (held != NULL) {
                    ifp->if_output(ifp, held,
                                   (InetSockaddr *)rt->rt_nodes[0].rn_u.rn_leaf.rn_key, rt);
                }
            }
        }
    }

    if (op != 1) {
        sceNetIfhandle_lib_F56FAC82(m);
        return;
    }

    {
        const u8 *replyMac;
        if (tpa == myAddr) {
            replyMac = ifp->if_sadl->sdl_data + ifp->if_sadl->sdl_nlen;
        } else {
            InetLlinfoArp *proxyLa = inetArpLookup(&tpa, 0, 1);
            if (proxyLa == NULL) {
                sceNetIfhandle_lib_F56FAC82(m);
                return;
            }
            {
                InetSockaddrDl *proxySdl = (InetSockaddrDl *)proxyLa->la_rt->rt_gateway;
                replyMac = proxySdl->sdl_data + proxySdl->sdl_nlen;
            }
        }

        /* Build ARP reply in-place */
        sceNet_lib_2F305274(arpHdr + 8, arpHdr + 8 + arpHdr[4] + arpHdr[5], arpHdr[4]);
        sceNet_lib_2F305274(replyMac, arpHdr + 8, arpHdr[4]);
        sceNet_lib_2F305274(arpHdr + 8 + arpHdr[4], arpHdr + 8 + 2 * arpHdr[4] + arpHdr[5], arpHdr[5]);
        sceNet_lib_2F305274(&tpa, arpHdr + 8 + arpHdr[4], arpHdr[5]);
        {
            u16 pro = inetBswap16(0x0800);
            u16 replyOp = inetBswap16(2);
            arpHdr[2] = (u8)(pro & 0xFF);
            arpHdr[3] = (u8)(pro >> 8);
            arpHdr[6] = (u8)(replyOp & 0xFF);
            arpHdr[7] = (u8)(replyOp >> 8);
        }
        m->m_flags &= (u16)~(INET_M_BCAST | INET_M_MCAST);
        {
            s32 replyLen = 2 * (arpHdr[4] + arpHdr[5]) + 8;
            InetSockaddr sa;
            M_PKTLEN(m) = replyLen;
            m->m_len = replyLen;
            sa.sa_len = 2;
            sa.sa_family = 28; /* AF_ARP */
            ifp->if_output(ifp, m, &sa, NULL);
        }
    }
}

/* [227] 0x012334 */
INET_NOINLINE InetLlinfoArp *inetArpLookup(const u32 *addr, int create, int proxy)
{
    InetRtentry *rt;

    g_arp_sin.sin_len = sizeof(InetSockaddrIn);
    g_arp_sin.sin_family = INET_AF_INET;
    g_arp_sin.sin_addr = *addr;
    *(u16 *)&g_arp_sin.sin_zero[6] = (u16)(proxy != 0);

    rt = inetRtAlloc1((InetSockaddr *)&g_arp_sin, create);
    if (rt == NULL) {
        return NULL;
    }
    rt->rt_refcnt--;
    if ((rt->rt_flags & INET_RTF_GATEWAY) == 0 &&
        (rt->rt_flags & INET_RTF_LLINFO) != 0 &&
        rt->rt_gateway->sa_family == INET_AF_LINK) {
        return (InetLlinfoArp *)rt->rt_llinfo;
    }

    if (create != 0 && rt->rt_refcnt <= 0 && (rt->rt_flags & INET_RTF_CLONED) != 0) {
        inetRtRequest(INET_RTM_DELETE,
                      (InetSockaddr *)rt->rt_nodes[0].rn_u.rn_leaf.rn_key,
                      rt->rt_gateway,
                      (InetSockaddr *)rt->rt_nodes[0].rn_u.rn_leaf.rn_mask,
                      (u32)rt->rt_flags,
                      NULL);
    }
    return NULL;
}

/* [228] 0x012410 */
INET_NOINLINE void inetInRevarpInput(InetMbuf *m)
{
    u8 *arpHdr = m->m_data;
    u16 op = inetBswap16((u16)(((u16)arpHdr[7] << 8) | arpHdr[6]));

    if (op == 1 || op == 2) {
        inetInArpInput(m);
        return;
    }
    if (op == 4 && g_revarp_in_progress != 0 && M_RCVIF(m) == g_revarp_ifp) {
        if (g_revarp_got_reply == 0) {
            InetIfnet *ifp = M_RCVIF(m);
            if (sceNet_lib_4BF83CF4(arpHdr + 8 + arpHdr[4] + arpHdr[5],
                                    ifp->if_sadl->sdl_data + ifp->if_sadl->sdl_nlen,
                                    arpHdr[4]) != 0) {
                sceNetIfhandle_lib_F56FAC82(m);
                return;
            }
            sceNet_lib_2F305274(arpHdr + 8 + arpHdr[4], &g_revarp_srvip, 4);
            sceNet_lib_2F305274(arpHdr + 8 + 2 * arpHdr[4] + arpHdr[5], &g_revarp_myip, 4);
            g_revarp_got_reply = 1;
        }
        sceNet_lib_7574FDA1(&g_revarp_myip);
    }
    sceNetIfhandle_lib_F56FAC82(m);
}

/* [229] 0x012550 */
static INET_NOINLINE void inetArpDrain(void)
{
    int spl = sceNet_lib_94B44F26();
    if (g_arp_inuse == 0) {
        InetLlinfoArp *la;
        g_arp_inuse = 1;
        sceNet_lib_515B2F33(spl);

        la = g_llinfo_arp;
        while (la != NULL) {
            InetLlinfoArp *next = la->la_next;
            InetMbuf *held = la->la_hold;
            la->la_hold = NULL;
            if (held != NULL) {
                sceNetIfhandle_lib_F56FAC82(held);
            }
            la = next;
        }

        spl = sceNet_lib_94B44F26();
        g_arp_inuse--;
    }
    sceNet_lib_515B2F33(spl);
}

/* [230] 0x0125E8 */
INET_NOINLINE void inetArpIfInit(InetIfnet *ifp, InetIfaddr *ifa)
{
    InetInIfaddr *ia = (InetInIfaddr *)ifa;
    if (ia->ia_addr.sin_addr != 0) {
        inetArpRequest(ifp, &ia->ia_addr.sin_addr, &ia->ia_addr.sin_addr,
                       ifp->if_sadl->sdl_data + ifp->if_sadl->sdl_nlen);
    }
    ifa->ifa_flags |= INET_RTF_CLONING;
    ifa->ifa_rtrequest = inetArpRtRequest;
}

/* [231] 0x012650 */
INET_NOINLINE void inetRevarpInput(InetMbuf *m)
{
    if (m->m_len >= 8) {
        u8 *arpHdr = m->m_data;
        if (m->m_len >= (s32)(2 * (arpHdr[4] + arpHdr[5]) + 8)) {
            u16 pro = inetBswap16((u16)(((u16)arpHdr[3] << 8) | arpHdr[2]));
            if (pro == 0x0800 || pro == 0x1000) {
                inetInRevarpInput(m);
                return;
            }
        }
    }
    sceNetIfhandle_lib_F56FAC82(m);
}

/* [232] 0x0126D8 */
static INET_NOINLINE void inetArpTimer(void *unusedArg)
{
    int outerSpl = sceNet_lib_94B44F26();
    int innerSpl = sceNet_lib_94B44F26();
    (void)unusedArg;

    if (g_arp_inuse == 0) {
        InetLlinfoArp *la;
        g_arp_inuse = 1;
        sceNet_lib_515B2F33(innerSpl);

        sceNet_lib_7C86FBA4(&g_arp_callout, (u32)(100 * g_arpt_prune), inetArpTimer, NULL);

        la = g_llinfo_arp;
        while (la != NULL) {
            InetLlinfoArp *next = la->la_next;
            InetRtentry *rt = la->la_rt;
            if (rt->rt_rmx.rmx_expire != 0 && sceNet_lib_75D9985C() >= rt->rt_rmx.rmx_expire) {
                if (rt->rt_refcnt > 0 && rt->rt_gateway != NULL &&
                    rt->rt_gateway->sa_family == INET_AF_LINK) {
                    ((InetSockaddrDl *)rt->rt_gateway)->sdl_alen = 0;
                    la->la_asked = 0;
                    rt->rt_flags &= ~INET_RTF_REJECT;
                } else {
                    inetRtRequest(INET_RTM_DELETE,
                                  (InetSockaddr *)rt->rt_nodes[0].rn_u.rn_leaf.rn_key,
                                  NULL,
                                  (InetSockaddr *)rt->rt_nodes[0].rn_u.rn_leaf.rn_mask,
                                  0,
                                  NULL);
                }
            }
            la = next;
        }

        innerSpl = sceNet_lib_94B44F26();
        g_arp_inuse--;
    }

    sceNet_lib_515B2F33(innerSpl);
    sceNet_lib_515B2F33(outerSpl);
}

/* [233] 0x012834 */
static INET_NOINLINE void inetArpInit(void)
{
    g_arp_reserved_stat = 0;
    g_arp_count = 0;
    g_arp_allocated = 0;
    g_arp_inuse = 0;
    g_arp_timer_initialized = 0;
    sceKernelMemset(&g_arpintrq, 0, sizeof(g_arpintrq));
    g_arpintrq.ifq_maxlen = 50;
    g_revarp_got_reply = 0;
    g_llinfo_arp = NULL;
    g_revarp_in_progress = 0;
    g_revarp_ifp = NULL;
}
