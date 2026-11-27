#include "pspnet_inet_internal.h"

typedef struct InetIfaMsghdr {
    u16 ifam_msglen;
    u8 ifam_version;
    u8 ifam_type;
    s32 ifam_addrs;
    s32 ifam_flags;
    u16 ifam_index;
    u16 ifam_pad;
    s32 ifam_metric;
} InetIfaMsghdr;
INET_STATIC_ASSERT(ifamsghdr_size, sizeof(InetIfaMsghdr) == 20);

typedef struct InetIfMsghdr {
    u16 ifm_msglen;
    u8 ifm_version;
    u8 ifm_type;
    s32 ifm_addrs;
    s32 ifm_flags;
    u16 ifm_index;
    u16 ifm_pad;
    InetIfData ifm_data;
} InetIfMsghdr;
INET_STATIC_ASSERT(ifmsghdr_size, sizeof(InetIfMsghdr) == 136);

typedef struct InetIfAnnounceMsghdr {
    u16 ifan_msglen;
    u8 ifan_version;
    u8 ifan_type;
    u16 ifan_index;
    char ifan_name[16];
    u16 ifan_what;
} InetIfAnnounceMsghdr;
INET_STATIC_ASSERT(ifannouncemsghdr_size, sizeof(InetIfAnnounceMsghdr) == 24);

typedef struct InetWalkArg {
    s32 w_op;
    s32 w_arg;
    s32 w_given;
    s32 w_needed;
    u8 *w_where;
    s32 w_tmemsize;
    s32 w_tmemneeded;
    void *w_tmem;
} InetWalkArg;

typedef struct InetSockproto {
    u16 sp_family;
    u16 sp_protocol;
} InetSockproto;

static InetSockaddr g_route_dst = { 2, INET_AF_ROUTE, { 0 } };
static InetSockaddr g_route_src = { 2, INET_AF_ROUTE, { 0 } };
static InetSockproto g_route_proto = { INET_AF_ROUTE, 0 };

static u32 g_raw_sendspace = 8192;
static u32 g_raw_recvspace = 8192;

static InetRawcb *g_rawcb_head;
InetIfnet g_lo0if;

/* Forward declarations for domain/protosw tables */
static int inetRouteUsrreq(InetSocket *so, int req, InetMbuf *m, InetMbuf *nam, InetMbuf *control, void *proc);
static int inetRouteOutput(InetMbuf *m, ...);
static int inetSysctlRtable(int *name, u32 namelen, void *oldp, u32 *oldlenp, void *newp, u32 newlen);
static int inetRawAttach(InetSocket *so, int proto);
static void inetRawDetach(InetRawcb *rp);
static void inetRawDisconnect(InetRawcb *rp);
static void inetRawInput(InetMbuf *mbuf, ...);
static int inetRawUsrreq(InetSocket *so, int req, InetMbuf *m, InetMbuf *nam, InetMbuf *control, void *proc);
static void inetRawInit(void);
static void *inetRawCtlinput(int cmd, InetSockaddr *sa, void *arg);

static InetProtosw g_routesw[1] = {
    {
        INET_SOCK_RAW,
        &g_routedomain,
        0,
        INET_PR_ATOMIC | INET_PR_ADDR,
        inetRawInput,
        inetRouteOutput,
        inetRawCtlinput,
        NULL,
        inetRouteUsrreq,
        inetRawInit,
        NULL,
        NULL,
        NULL,
        inetSysctlRtable
    }
};

InetDomain g_routedomain = {
    INET_AF_ROUTE,
    "route",
    inetRouteInit,
    NULL,
    NULL,
    g_routesw,
    &g_routesw[1],
    NULL,
    NULL,
    0,
    0
};

static u32 inetRoundupSa(const InetSockaddr *sa)
{
    if (sa->sa_len == 0) {
        return 4;
    }
    return ((u32)(sa->sa_len - 1) | 3U) + 1U;
}

static void inetRouteCbAdjust(u16 proto, int delta)
{
    g_route_cb.any_count += delta;
    if (proto == INET_AF_INET) {
        g_route_cb.ip_count += delta;
    } else if (proto == 6) {
        g_route_cb.ns_count += delta;
    } else if (proto == 7) {
        g_route_cb.iso_count += delta;
    } else if (proto == 23) {
        g_route_cb.ipx_count += delta;
    }
}

/* [195] 0x00E6B0 */
static INET_NOINLINE int inetRouteUsrreq(InetSocket *so, int req, InetMbuf *m,
                                         InetMbuf *nam, InetMbuf *control, void *proc)
{
    InetRawcb *rp = (InetRawcb *)so->so_pcb;
    int spl;
    int err;

    if (req == INET_PRU_ATTACH) {
        rp = (InetRawcb *)sceNetIfhandle_lib_15CFE3C0(sizeof(InetRawcb));
        so->so_pcb = rp;
        if (rp != NULL) {
            sceNet_lib_8B615F52(rp, sizeof(InetRawcb));
        }
    }
    if (req == INET_PRU_DETACH && rp != NULL) {
        inetRouteCbAdjust(rp->rcb_protocol, -1);
    }

    spl = sceNet_lib_94B44F26();
    if (req == INET_PRU_ATTACH) {
        err = inetRawAttach(so, (int)(uintptr_t)nam);
    } else {
        err = inetRawUsrreq(so, req, m, nam, control, proc);
    }

    rp = (InetRawcb *)so->so_pcb;
    if (req == INET_PRU_ATTACH && rp != NULL) {
        if (err != 0) {
            sceNetIfhandle_lib_76BAD213(rp);
        } else {
            inetRouteCbAdjust(rp->rcb_protocol, 1);
            rp->rcb_faddr = &g_route_dst;
            rp->rcb_laddr = &g_route_src;
            inetSoIsConnected(so);
            so->so_options |= INET_SO_USELOOPBACK;
        }
    }
    sceNet_lib_515B2F33(spl);
    return err;
}

/* [196] 0x00E8FC */
static INET_NOINLINE int inetRouteOutput(InetMbuf *m, ...)
{
    va_list ap;
    InetSocket *so;
    InetRtMsghdr *rtm = NULL;
    InetRtentry *rt = NULL;
    InetRtentry *savedRt = NULL;
    InetRtAddrinfo addrInfo;
    int len = 0;
    int err = 0;
    InetRawcb *rp = NULL;

    va_start(ap, m);
    so = va_arg(ap, InetSocket *);
    va_end(ap);

    sceNet_lib_8B615F52(&addrInfo, 36);
    if (m == NULL) {
        return INET_ENOBUFS;
    }
    if (m->m_len < 4) {
        m = sceNetIfhandle_lib_49EDBB18(m, 4);
        if (m == NULL) {
            return INET_ENOBUFS;
        }
    }

    len = M_PKTLEN(m);
    if (len < (int)sizeof(InetRtMsghdr) || len != (int)(*(u16 *)m->m_data)) {
        err = INET_EINVAL;
    } else {
        rtm = (InetRtMsghdr *)sceNetIfhandle_lib_15CFE3C0((u32)len);
        if (rtm == NULL) {
            err = INET_ENOBUFS;
        } else {
            sceNetIfhandle_lib_9A6261EC(m, 0, len, rtm);
            if (rtm->rtm_version != INET_RTM_VERSION) {
                err = INET_EPROTONOSUPPORT;
            } else {
                rtm->rtm_pid = sceKernelGetThreadId();
                addrInfo.rti_addrs = rtm->rtm_addrs;
                inetRtXaddrs((u8 *)(rtm + 1), (u8 *)rtm + len, &addrInfo);

                if (addrInfo.rti_info[0] == NULL ||
                    addrInfo.rti_info[0]->sa_family >= 31 ||
                    (addrInfo.rti_info[1] != NULL && addrInfo.rti_info[1]->sa_family >= 31)) {
                    err = INET_EINVAL;
                } else {
                    if (addrInfo.rti_info[3] != NULL) {
                        InetRadixNode *maskRn = inetRnAddmask((const u8 *)addrInfo.rti_info[3], 0, 1);
                        if (maskRn == NULL ||
                            sceNet_lib_4BF83CF4(addrInfo.rti_info[3], maskRn->rn_u.rn_leaf.rn_key,
                                                addrInfo.rti_info[3]->sa_len) != 0) {
                            err = INET_ENOBUFS;
                        } else {
                            addrInfo.rti_info[3] = (InetSockaddr *)maskRn->rn_u.rn_leaf.rn_key;
                        }
                    }
                }

                if (err == 0) {
                    switch (rtm->rtm_type) {
                    case INET_RTM_ADD:
                        if (addrInfo.rti_info[1] == NULL) {
                            err = INET_EINVAL;
                        } else {
                            err = inetRtRequest(INET_RTM_ADD,
                                                addrInfo.rti_info[0],
                                                addrInfo.rti_info[1],
                                                addrInfo.rti_info[2],
                                                (u32)rtm->rtm_flags,
                                                &savedRt);
                            if (err == 0 && savedRt != NULL) {
                                inetRtSetMetrics(rtm->rtm_inits, &rtm->rtm_rmx, &savedRt->rt_rmx);
                                savedRt->rt_genmask = addrInfo.rti_info[3];
                                savedRt->rt_refcnt--;
                            }
                        }
                        break;

                    case INET_RTM_DELETE:
                        err = inetRtRequest(INET_RTM_DELETE,
                                            addrInfo.rti_info[0],
                                            addrInfo.rti_info[1],
                                            addrInfo.rti_info[2],
                                            (u32)rtm->rtm_flags,
                                            &savedRt);
                        if (err == 0) {
                            rt = savedRt;
                            rt->rt_refcnt++;
                            /* fall into report generation */
                        }
                        break;

                    case INET_RTM_GET:
                    case INET_RTM_CHANGE:
                    case INET_RTM_LOCK: {
                        InetRadixNodeHead *rnh = g_rt_tables[addrInfo.rti_info[0]->sa_family];
                        if (rnh == NULL) {
                            err = INET_EAFNOSUPPORT;
                        } else {
                            InetRadixNode *rn = rnh->rnh_lookup(addrInfo.rti_info[0],
                                                                addrInfo.rti_info[2], rnh);
                            if (rn == NULL || (rn->rn_flags & 2) != 0) {
                                err = INET_ESRCH;
                            } else {
                                rt = (InetRtentry *)rn;
                                rt->rt_refcnt++;
                            }
                        }
                        if (err == 0 && rtm->rtm_type == INET_RTM_CHANGE) {
                            InetIfnet *newIfp = NULL;
                            InetIfaddr *newIfa = NULL;

                            if (addrInfo.rti_info[1] != NULL) {
                                if (inetRtSetgate(rt, (InetSockaddr *)rt->rt_nodes[0].rn_u.rn_leaf.rn_key,
                                                  addrInfo.rti_info[1]) != 0) {
                                    err = 132;
                                }
                            }
                            if (err == 0) {
                                if (addrInfo.rti_info[4] != NULL) {
                                    newIfa = inetIfaWithNet(addrInfo.rti_info[4]);
                                    if (newIfa != NULL) {
                                        newIfp = newIfa->ifa_ifp;
                                    }
                                }
                                if (newIfp != NULL && (addrInfo.rti_info[5] != NULL || addrInfo.rti_info[1] != NULL)) {
                                    InetSockaddr *sa = (addrInfo.rti_info[5] != NULL) ?
                                        addrInfo.rti_info[5] : addrInfo.rti_info[1];
                                    newIfa = inetIfaOfIfpForAddr(sa, newIfp);
                                } else {
                                    if (addrInfo.rti_info[5] != NULL) {
                                        newIfa = inetIfaWithAddr(addrInfo.rti_info[5]);
                                    }
                                    if (newIfa == NULL && addrInfo.rti_info[1] != NULL) {
                                        newIfa = inetIfaIfWithRoute(rt->rt_flags,
                                                                    (InetSockaddr *)rt->rt_nodes[0].rn_u.rn_leaf.rn_key,
                                                                    addrInfo.rti_info[1]);
                                    }
                                    if (newIfa != NULL) {
                                        newIfp = newIfa->ifa_ifp;
                                    }
                                }

                                if (newIfa != NULL && newIfa != rt->rt_ifa) {
                                    InetIfaddr *oldIfa = rt->rt_ifa;
                                    if (oldIfa != NULL) {
                                        if (oldIfa->ifa_rtrequest != NULL) {
                                            oldIfa->ifa_rtrequest(INET_RTM_DELETE, rt, addrInfo.rti_info[1]);
                                            oldIfa = rt->rt_ifa;
                                        }
                                        oldIfa->ifa_refcnt--;
                                        if (oldIfa->ifa_refcnt == 0) {
                                            sceNet_lib_EDCC871E(oldIfa);
                                        }
                                    }
                                    rt->rt_ifp = newIfp;
                                    rt->rt_ifa = newIfa;
                                    newIfa->ifa_refcnt++;
                                }

                                inetRtSetMetrics(rtm->rtm_inits, &rtm->rtm_rmx, &rt->rt_rmx);
                                if (rt->rt_ifa != NULL && rt->rt_ifa->ifa_rtrequest != NULL) {
                                    rt->rt_ifa->ifa_rtrequest(INET_RTM_ADD, rt, addrInfo.rti_info[1]);
                                }
                                if (addrInfo.rti_info[3] != NULL) {
                                    rt->rt_genmask = addrInfo.rti_info[3];
                                }
                                rt->rt_rmx.rmx_locks &= ~rtm->rtm_inits;
                                rt->rt_rmx.rmx_locks |= (rtm->rtm_inits & rtm->rtm_rmx.rmx_locks);
                            }
                        } else if (err == 0 && rtm->rtm_type == INET_RTM_LOCK) {
                            rt->rt_rmx.rmx_locks &= ~rtm->rtm_inits;
                            rt->rt_rmx.rmx_locks |= (rtm->rtm_inits & rtm->rtm_rmx.rmx_locks);
                        }
                        break;
                    }

                    default:
                        err = INET_EOPNOTSUPP;
                        break;
                    }

                    /* Build reply for RTM_DELETE / RTM_GET */
                    if (err == 0 && rt != NULL &&
                        (rtm->rtm_type == INET_RTM_DELETE || rtm->rtm_type == INET_RTM_GET)) {
                        int newLen = 0;
                        addrInfo.rti_info[0] = (InetSockaddr *)rt->rt_nodes[0].rn_u.rn_leaf.rn_key;
                        addrInfo.rti_info[1] = rt->rt_gateway;
                        addrInfo.rti_info[2] = (InetSockaddr *)rt->rt_nodes[0].rn_u.rn_leaf.rn_mask;
                        addrInfo.rti_info[3] = rt->rt_genmask;
                        if ((rtm->rtm_addrs & (INET_RTA_IFP | INET_RTA_IFA)) != 0) {
                            InetIfnet *rtIfp = rt->rt_ifp;
                            if (rtIfp != NULL) {
                                addrInfo.rti_info[4] = rtIfp->if_addrhead->ifa_addr;
                                addrInfo.rti_info[5] = rt->rt_ifa->ifa_addr;
                                addrInfo.rti_info[7] = ((rtIfp->if_flags & INET_IFF_POINTOPOINT) != 0) ?
                                    rt->rt_ifa->ifa_dstaddr : NULL;
                                rtm->rtm_index = rtIfp->if_index;
                            } else {
                                addrInfo.rti_info[4] = NULL;
                                addrInfo.rti_info[5] = NULL;
                            }
                        }
                        inetRtMsg2(rtm->rtm_type, &addrInfo, NULL, NULL, &newLen);
                        if ((int)rtm->rtm_msglen < newLen) {
                            InetRtMsghdr *grown = (InetRtMsghdr *)sceNetIfhandle_lib_15CFE3C0((u32)newLen);
                            if (grown == NULL) {
                                err = INET_ENOBUFS;
                            } else {
                                sceNet_lib_2F305274(rtm, grown, rtm->rtm_msglen);
                                sceNetIfhandle_lib_76BAD213(rtm);
                                rtm = grown;
                            }
                        }
                        if (err == 0) {
                            inetRtMsg2(rtm->rtm_type, &addrInfo, (u8 *)rtm, NULL, NULL);
                            rtm->rtm_flags = rt->rt_flags;
                            rtm->rtm_rmx = rt->rt_rmx;
                            rtm->rtm_addrs = addrInfo.rti_addrs;
                        }
                    }
                }
            }
        }
    }

    if (rtm != NULL) {
        if (err != 0) {
            rtm->rtm_errno = err;
        } else {
            rtm->rtm_flags |= INET_RTF_DONE;
        }
    }
    if (rt != NULL) {
        inetRtFree(rt);
    }

    if ((so->so_options & INET_SO_USELOOPBACK) == 0) {
        if (g_route_cb.any_count < 2) {
            if (rtm != NULL) {
                sceNetIfhandle_lib_76BAD213(rtm);
            }
            sceNetIfhandle_lib_F56FAC82(m);
            return err;
        }
        rp = (InetRawcb *)so->so_pcb;
    }

    if (rtm != NULL) {
        sceNetIfhandle_lib_1560F143(m, 0, rtm->rtm_msglen, rtm);
        sceNetIfhandle_lib_76BAD213(rtm);
    }
    if (rp != NULL) {
        rp->rcb_family = 0;
    }
    if (addrInfo.rti_info[0] != NULL) {
        g_route_proto.sp_protocol = addrInfo.rti_info[0]->sa_family;
    }
    inetRawInput(m, &g_route_proto, &g_route_src, &g_route_dst);
    if (rp != NULL) {
        rp->rcb_family = INET_AF_ROUTE;
    }
    return err;
}

/* [197] 0x00EFF8 */
INET_NOINLINE void inetRtSetMetrics(u32 which, const InetRtMetrics *inMetrics, InetRtMetrics *outMetrics)
{
    if ((which & 0x08) != 0) outMetrics->rmx_recvpipe = inMetrics->rmx_recvpipe;
    if ((which & 0x10) != 0) outMetrics->rmx_sendpipe = inMetrics->rmx_sendpipe;
    if ((which & 0x20) != 0) outMetrics->rmx_ssthresh = inMetrics->rmx_ssthresh;
    if ((which & 0x40) != 0) outMetrics->rmx_rtt      = inMetrics->rmx_rtt;
    if ((which & 0x80) != 0) outMetrics->rmx_rttvar   = inMetrics->rmx_rttvar;
    if ((which & 0x02) != 0) outMetrics->rmx_hopcount = inMetrics->rmx_hopcount;
    if ((which & 0x01) != 0) outMetrics->rmx_mtu      = inMetrics->rmx_mtu;
    if ((which & 0x04) != 0) outMetrics->rmx_expire   = inMetrics->rmx_expire;
}

/* [198] 0x00F084 */
INET_NOINLINE void inetRtXaddrs(u8 *cp, const u8 *cplim, InetRtAddrinfo *rtInfo)
{
    int idx;
    sceNet_lib_8B615F52(rtInfo->rti_info, sizeof(rtInfo->rti_info));
    for (idx = 0; idx < 8 && cp < cplim; idx++) {
        if (((rtInfo->rti_addrs >> idx) & 1) != 0) {
            InetSockaddr *sa = (InetSockaddr *)cp;
            rtInfo->rti_info[idx] = sa;
            cp += inetRoundupSa(sa);
        }
    }
}

/* [199] 0x00F134 */
INET_NOINLINE int inetRtMsg2(int type, InetRtAddrinfo *rtInfo, u8 *cp, void *walkPtr, int *lenp)
{
    InetWalkArg *w = (InetWalkArg *)walkPtr;
    int secondPass = 0;
    u8 *start = cp;
    int totalLen = 0;

    rtInfo->rti_addrs = 0;
    for (;;) {
        int hdrLen;
        int idx;
        if (type < INET_RTM_NEWADDR) {
            hdrLen = (int)sizeof(InetRtMsghdr);
        } else if (type < INET_RTM_IFINFO) {
            hdrLen = (int)sizeof(InetIfaMsghdr);
        } else if (type == 15) {
            hdrLen = (int)sizeof(InetIfMsghdr);
        } else {
            hdrLen = (int)sizeof(InetRtMsghdr);
        }

        start = cp;
        if (cp != NULL) {
            cp += hdrLen;
        }
        totalLen = hdrLen;

        for (idx = 0; idx < 8; idx++) {
            InetSockaddr *sa = rtInfo->rti_info[idx];
            if (sa != NULL) {
                u32 step = inetRoundupSa(sa);
                rtInfo->rti_addrs |= (1 << idx);
                totalLen += (int)step;
                if (cp != NULL) {
                    sceNet_lib_2F305274(sa, cp, (int)step);
                    cp += step;
                }
            }
        }

        if (cp != NULL || w == NULL || secondPass != 0) {
            break;
        }

        w->w_needed += totalLen;
        if (w->w_needed > 0 || w->w_where == NULL) {
            break;
        }
        if (w->w_tmemsize < totalLen) {
            if (w->w_tmem != NULL) {
                sceNetIfhandle_lib_76BAD213(w->w_tmem);
            }
            w->w_tmem = sceNetIfhandle_lib_15CFE3C0((u32)totalLen);
            if (w->w_tmem != NULL) {
                w->w_tmemsize = totalLen;
            }
        }
        cp = (u8 *)w->w_tmem;
        if (cp == NULL) {
            w->w_tmemneeded = totalLen;
            return INET_ENOBUFS;
        }
        secondPass = 1;
    }

    if (cp != NULL) {
        InetRtMsghdr *hdr = (InetRtMsghdr *)start;
        hdr->rtm_version = INET_RTM_VERSION;
        hdr->rtm_type = (u8)type;
        hdr->rtm_msglen = (u16)totalLen;
    }
    if (lenp != NULL) {
        *lenp = totalLen;
    }
    return 0;
}

/* [200] 0x00F318 */
INET_NOINLINE void inetRtNewAddrMsg(int cmd, InetIfaddr *ifa, int err, InetRtentry *rt)
{
    InetIfnet *ifp = ifa->ifa_ifp;
    int pass;

    if (g_route_cb.any_count == 0) {
        return;
    }

    for (pass = 1; pass <= 2; pass++) {
        InetRtAddrinfo addrInfo;
        InetMbuf *m = NULL;
        InetSockaddr *sa = NULL;

        sceNet_lib_8B615F52(&addrInfo, 36);
        if ((cmd == INET_RTM_ADD && pass == 1) || (cmd == INET_RTM_DELETE && pass == 2)) {
            InetIfaMsghdr ifam;
            int ifaCmd = (cmd == INET_RTM_ADD) ? INET_RTM_NEWADDR : INET_RTM_DELADDR;
            sa = ifa->ifa_addr;
            addrInfo.rti_info[2] = ifa->ifa_netmask;
            addrInfo.rti_info[4] = ifp->if_addrhead->ifa_addr;
            addrInfo.rti_info[5] = sa;
            addrInfo.rti_info[7] = ifa->ifa_dstaddr;

            sceNet_lib_8B615F52(&ifam, sizeof(ifam));
            ifam.ifam_index = ifp->if_index;
            ifam.ifam_metric = ifa->ifa_metric;
            ifam.ifam_flags = (s32)ifa->ifa_flags;
            m = inetRtMsg1(ifaCmd, &addrInfo, &ifam, sizeof(ifam));
            if (m == NULL) {
                continue;
            }
            ((InetIfaMsghdr *)m->m_data)->ifam_addrs = addrInfo.rti_addrs;
        }
        if ((cmd == INET_RTM_ADD && pass == 2) || (cmd == INET_RTM_DELETE && pass == 1)) {
            InetRtMsghdr rtm;
            if (rt == NULL) {
                continue;
            }
            sa = (InetSockaddr *)rt->rt_nodes[0].rn_u.rn_leaf.rn_key;
            addrInfo.rti_info[0] = sa;
            addrInfo.rti_info[1] = rt->rt_gateway;
            addrInfo.rti_info[2] = (InetSockaddr *)rt->rt_nodes[0].rn_u.rn_leaf.rn_mask;

            sceNet_lib_8B615F52(&rtm, sizeof(rtm));
            rtm.rtm_index = ifp->if_index;
            rtm.rtm_flags |= rt->rt_flags;
            rtm.rtm_errno = err;
            m = inetRtMsg1(cmd, &addrInfo, &rtm, sizeof(rtm));
            if (m == NULL) {
                continue;
            }
            ((InetRtMsghdr *)m->m_data)->rtm_addrs = addrInfo.rti_addrs;
        }

        if (m != NULL) {
            g_route_proto.sp_protocol = (sa != NULL) ? sa->sa_family : 0;
            inetRawInput(m, &g_route_proto, &g_route_src, &g_route_dst);
        }
    }
}

/* [201] 0x00F544 */
INET_NOINLINE void inetRtMissMsg(int type, InetRtAddrinfo *rtInfo, int flags, int err)
{
    InetRtMsghdr rtm;
    InetMbuf *m;
    InetSockaddr *sa = rtInfo->rti_info[0];

    if (g_route_cb.any_count == 0) {
        return;
    }
    sceNet_lib_8B615F52(&rtm, sizeof(rtm));
    rtm.rtm_flags = flags | INET_RTF_DONE;
    rtm.rtm_errno = err;
    m = inetRtMsg1(type, rtInfo, &rtm, sizeof(rtm));
    if (m != NULL) {
        ((InetRtMsghdr *)m->m_data)->rtm_addrs = rtInfo->rti_addrs;
        g_route_proto.sp_protocol = (sa != NULL) ? sa->sa_family : 0;
        inetRawInput(m, &g_route_proto, &g_route_src, &g_route_dst);
    }
}

/* [202] 0x00F61C */
INET_NOINLINE void inetRtIfMsg(InetIfnet *ifp)
{
    InetIfMsghdr ifm;
    InetRtAddrinfo addrInfo;
    InetMbuf *m;

    if (g_route_cb.any_count == 0) {
        return;
    }
    sceNet_lib_8B615F52(&addrInfo, 36);
    sceNet_lib_8B615F52(&ifm, sizeof(ifm));
    ifm.ifm_index = ifp->if_index;
    ifm.ifm_flags = (s16)ifp->if_flags;
    ifm.ifm_data = ifp->if_data;
    ifm.ifm_addrs = 0;

    m = inetRtMsg1(15, &addrInfo, &ifm, sizeof(ifm));
    if (m != NULL) {
        g_route_proto.sp_protocol = 0;
        inetRawInput(m, &g_route_proto, &g_route_src, &g_route_dst);
    }
}

/* [203] 0x00F710 */
INET_NOINLINE void inetRtIfAnnounceMsg(InetIfnet *ifp, u16 what)
{
    InetIfAnnounceMsghdr ifan;
    InetRtAddrinfo addrInfo;
    InetMbuf *m;

    if (g_route_cb.any_count == 0) {
        return;
    }
    sceNet_lib_8B615F52(&addrInfo, 36);
    sceNet_lib_8B615F52(&ifan, sizeof(ifan));
    ifan.ifan_index = ifp->if_index;
    sceNet_lib_80C9F02A(ifan.ifan_name, ifp->if_xname);
    ifan.ifan_what = what;

    m = inetRtMsg1(INET_RTM_IFANNOUNCE, &addrInfo, &ifan, sizeof(ifan));
    if (m != NULL) {
        g_route_proto.sp_protocol = 0;
        inetRawInput(m, &g_route_proto, &g_route_src, &g_route_dst);
    }
}

/* [204] 0x00F7D0 */
INET_NOINLINE InetMbuf *inetRtMsg1(int type, InetRtAddrinfo *rtInfo, const void *hdrData, int hdrLen)
{
    InetMbuf *m = sceNetIfhandle_lib_4CF15C43(INET_M_DONTWAIT, INET_MT_DATA);
    int msgLen;
    int idx;
    InetRtMsghdr *hdr;

    if (m == NULL) {
        return NULL;
    }
    if (type == 15) {
        msgLen = (int)sizeof(InetIfMsghdr);
    } else if (type == INET_RTM_NEWADDR || type == INET_RTM_DELADDR) {
        msgLen = (int)sizeof(InetIfaMsghdr);
    } else if (type == INET_RTM_IFANNOUNCE) {
        msgLen = (int)sizeof(InetIfAnnounceMsghdr);
    } else {
        msgLen = (int)sizeof(InetRtMsghdr);
    }

    m->m_len = msgLen;
    M_PKTLEN(m) = msgLen;
    M_RCVIF(m) = NULL;
    sceNetIfhandle_lib_1560F143(m, 0, hdrLen, hdrData);

    hdr = (InetRtMsghdr *)m->m_data;
    for (idx = 0; idx < 8; idx++) {
        InetSockaddr *sa = rtInfo->rti_info[idx];
        if (sa != NULL) {
            u32 step = inetRoundupSa(sa);
            rtInfo->rti_addrs |= (1 << idx);
            sceNetIfhandle_lib_1560F143(m, msgLen, (int)step, sa);
            msgLen += (int)step;
        }
    }

    hdr->rtm_msglen = (u16)msgLen;
    hdr->rtm_version = INET_RTM_VERSION;
    hdr->rtm_type = (u8)type;
    return m;
}

/* [206] 0x00FB54 */
static INET_NOINLINE int inetSysctlDumpEntry(InetRadixNode *rn, void *walkCtx)
{
    InetRtentry *rt = (InetRtentry *)rn;
    InetWalkArg *w = (InetWalkArg *)walkCtx;
    InetRtAddrinfo addrInfo;
    int size;
    int err;

    if (w->w_op == 2 && (rt->rt_flags & w->w_arg) == 0) {
        return 0;
    }
    sceNet_lib_8B615F52(&addrInfo, 36);
    addrInfo.rti_info[0] = (InetSockaddr *)rt->rt_nodes[0].rn_u.rn_leaf.rn_key;
    addrInfo.rti_info[1] = rt->rt_gateway;
    addrInfo.rti_info[2] = (InetSockaddr *)rt->rt_nodes[0].rn_u.rn_leaf.rn_mask;
    addrInfo.rti_info[3] = rt->rt_genmask;
    if (rt->rt_ifp != NULL) {
        addrInfo.rti_info[4] = rt->rt_ifp->if_addrhead->ifa_addr;
        addrInfo.rti_info[5] = rt->rt_ifa->ifa_addr;
        if ((rt->rt_ifp->if_flags & INET_IFF_POINTOPOINT) != 0) {
            addrInfo.rti_info[7] = rt->rt_ifa->ifa_dstaddr;
        }
    }

    err = inetRtMsg2(INET_RTM_GET, &addrInfo, NULL, w, &size);
    if (err == 0 && w->w_where != NULL && w->w_tmem != NULL && w->w_needed <= 0) {
        InetRtMsghdr *rtm = (InetRtMsghdr *)w->w_tmem;
        rtm->rtm_flags = rt->rt_flags;
        rtm->rtm_use = (s32)rt->rt_use;
        rtm->rtm_rmx = rt->rt_rmx;
        rtm->rtm_index = rt->rt_ifp->if_index;
        rtm->rtm_errno = 0;
        rtm->rtm_pid = 0;
        rtm->rtm_seq = 0;
        rtm->rtm_addrs = addrInfo.rti_addrs;
        sceNet_lib_2F305274(rtm, w->w_where, size);
        w->w_where += size;
    }
    return err;
}

/* [207] 0x00FCF8 */
static INET_NOINLINE int inetSysctlIfList(int af, InetWalkArg *w, int type)
{
    InetIfnet *ifp = NULL;
    InetRtAddrinfo addrInfo;
    int len = 0;
    int err = 0;

    sceNet_lib_C2EC2EEA(NULL, NULL, NULL, &ifp);
    sceNet_lib_8B615F52(&addrInfo, 36);

    while (ifp != NULL) {
        if (w->w_arg == 0 || w->w_arg == (s32)ifp->if_index) {
            InetIfaddr *ifa = ifp->if_addrhead;
            addrInfo.rti_info[4] = ifa->ifa_addr;
            if (type == 4) {
                err = inetRtMsg2(15, &addrInfo, NULL, w, &len);
                if (err != 0) {
                    return err;
                }
            }
            addrInfo.rti_info[4] = NULL;

            if (w->w_where != NULL && w->w_tmem != NULL && w->w_needed <= 0 && type == 4) {
                InetIfMsghdr *ifm = (InetIfMsghdr *)w->w_tmem;
                ifm->ifm_index = ifp->if_index;
                ifm->ifm_flags = (s16)ifp->if_flags;
                ifm->ifm_data = ifp->if_data;
                ifm->ifm_addrs = addrInfo.rti_addrs;
                sceNet_lib_2F305274(ifm, w->w_where, len);
                w->w_where += len;
            }

            ifa = ifa->ifa_next;
            while (ifa != NULL) {
                if (af == 0 || af == (int)ifa->ifa_addr->sa_family) {
                    addrInfo.rti_info[5] = ifa->ifa_addr;
                    addrInfo.rti_info[2] = ifa->ifa_netmask;
                    addrInfo.rti_info[7] = ifa->ifa_dstaddr;
                    err = inetRtMsg2(INET_RTM_NEWADDR, &addrInfo, NULL, w, &len);
                    if (err != 0) {
                        return err;
                    }
                    if (w->w_where != NULL && w->w_tmem != NULL && w->w_needed <= 0) {
                        InetIfaMsghdr *ifam = (InetIfaMsghdr *)w->w_tmem;
                        ifam->ifam_index = ifa->ifa_ifp->if_index;
                        ifam->ifam_flags = (s32)ifa->ifa_flags;
                        ifam->ifam_metric = ifa->ifa_metric;
                        ifam->ifam_addrs = addrInfo.rti_addrs;
                        sceNet_lib_2F305274(ifam, w->w_where, len);
                        w->w_where += len;
                    }
                }
                ifa = ifa->ifa_next;
            }
            addrInfo.rti_info[5] = NULL;
            addrInfo.rti_info[2] = NULL;
            addrInfo.rti_info[7] = NULL;
        }
        ifp = ifp->if_next;
    }
    return err;
}

/* [205] 0x00F930 */
static INET_NOINLINE int inetSysctlRtable(int *name, u32 namelen, void *oldp,
                                          u32 *oldlenp, void *newp, u32 newlen)
{
    InetWalkArg walk;
    int af;
    int err = INET_EINVAL;
    (void)newlen;

    if (newp != NULL) {
        return INET_EPERM;
    }
    if (namelen != 3) {
        return INET_EINVAL;
    }

    af = (u8)name[0];
    walk.w_tmemneeded = 0;
    walk.w_tmemsize = 0;
    walk.w_tmem = NULL;

    for (;;) {
        int spl;
        if (walk.w_tmemneeded != 0) {
            walk.w_tmem = sceNetIfhandle_lib_15CFE3C0((u32)walk.w_tmemneeded);
            if (walk.w_tmem == NULL) {
                return INET_ENOMEM;
            }
            walk.w_tmemsize = walk.w_tmemneeded;
            walk.w_tmemneeded = 0;
        }
        walk.w_op = name[1];
        walk.w_arg = name[2];
        walk.w_given = (s32)(*oldlenp);
        walk.w_needed = -walk.w_given;
        walk.w_where = (u8 *)oldp;

        spl = sceNet_lib_94B44F26();
        if (walk.w_op == 1 || walk.w_op == 2) {
            int idx;
            for (idx = 1; idx < 32; idx++) {
                InetRadixNodeHead *rnh = g_rt_tables[idx];
                if (rnh != NULL && (af == 0 || af == idx)) {
                    err = rnh->rnh_walktree(rnh, inetSysctlDumpEntry, &walk);
                    if (err != 0) {
                        break;
                    }
                }
            }
        } else if (walk.w_op == 4) {
            err = inetSysctlIfList(af, &walk, 4);
        }
        sceNet_lib_515B2F33(spl);

        if (err != INET_ENOBUFS || walk.w_tmem != NULL || walk.w_tmemneeded == 0) {
            break;
        }
    }

    if (walk.w_tmem != NULL) {
        sceNetIfhandle_lib_76BAD213(walk.w_tmem);
    }
    walk.w_needed += walk.w_given;
    if (oldp == NULL) {
        *oldlenp = (u32)((11 * walk.w_needed) / 10);
        return err;
    }
    {
        u32 written = (u32)(walk.w_where - (u8 *)oldp);
        *oldlenp = written;
        if (written < (u32)walk.w_needed) {
            return INET_ENOSPC;
        }
    }
    return err;
}

/* [208] 0x00FF44 */
static INET_NOINLINE int inetRawAttach(InetSocket *so, int proto)
{
    InetRawcb *rp = (InetRawcb *)so->so_pcb;
    int err;

    if (rp == NULL) {
        return INET_ENOBUFS;
    }
    err = inetSoReserve(so, g_raw_sendspace, g_raw_recvspace);
    if (err != 0) {
        return err;
    }
    rp->rcb_socket = so;
    rp->rcb_family = (u16)so->so_proto->pr_domain->dom_family;
    rp->rcb_protocol = (u16)proto;
    rp->rcb_next = g_rawcb_head;
    if (g_rawcb_head != NULL) {
        g_rawcb_head->rcb_prev = &rp->rcb_next;
    }
    rp->rcb_prev = &g_rawcb_head;
    g_rawcb_head = rp;
    return 0;
}

/* [209] 0x00FFE4 */
static INET_NOINLINE void inetRawDetach(InetRawcb *rp)
{
    InetSocket *so = rp->rcb_socket;
    so->so_pcb = NULL;
    inetSoFree(so);
    if (rp->rcb_next != NULL) {
        rp->rcb_next->rcb_prev = rp->rcb_prev;
    }
    *rp->rcb_prev = rp->rcb_next;
    sceNetIfhandle_lib_76BAD213(rp);
}

/* [210] 0x010030 */
static INET_NOINLINE void inetRawDisconnect(InetRawcb *rp)
{
    if ((rp->rcb_socket->so_state & INET_SS_NOFDREF) != 0) {
        inetRawDetach(rp);
    }
}

/* [211] 0x010094 */
static INET_NOINLINE void inetRawInput(InetMbuf *mbuf, ...)
{
    va_list ap;
    InetSockproto *proto;
    InetSockaddr *src;
    InetSockaddr *dst;
    InetRawcb *rp;
    InetSocket *last = NULL;

    va_start(ap, mbuf);
    proto = va_arg(ap, InetSockproto *);
    src = va_arg(ap, InetSockaddr *);
    dst = va_arg(ap, InetSockaddr *);
    va_end(ap);

    for (rp = g_rawcb_head; rp != NULL; rp = rp->rcb_next) {
        if (rp->rcb_family != proto->sp_family) {
            continue;
        }
        if (rp->rcb_protocol != 0 && rp->rcb_protocol != proto->sp_protocol) {
            continue;
        }
        if (rp->rcb_laddr != NULL &&
            sceNet_lib_4BF83CF4(rp->rcb_laddr, dst, rp->rcb_laddr->sa_len) != 0) {
            continue;
        }
        if (rp->rcb_faddr != NULL &&
            sceNet_lib_4BF83CF4(rp->rcb_faddr, src, rp->rcb_faddr->sa_len) != 0) {
            continue;
        }
        if (last != NULL) {
            InetMbuf *copy = sceNetIfhandle_lib_456E3146(mbuf, 0, 1000000000, INET_M_DONTWAIT);
            if (copy != NULL) {
                if (inetSbAppendAddr(&last->so_rcv, src, copy, NULL) == 0) {
                    sceNetIfhandle_lib_F56FAC82(copy);
                } else if ((last->so_rcv.sb_flags & 0x3C) != 0) {
                    inetSoWakeup(last, &last->so_rcv);
                }
            }
        }
        last = rp->rcb_socket;
    }

    if (last == NULL) {
        sceNetIfhandle_lib_F56FAC82(mbuf);
        return;
    }
    if (inetSbAppendAddr(&last->so_rcv, src, mbuf, NULL) == 0) {
        sceNetIfhandle_lib_F56FAC82(mbuf);
    } else if ((last->so_rcv.sb_flags & 0x3C) != 0) {
        inetSoWakeup(last, &last->so_rcv);
    }
}

/* [212] 0x01026C */
static INET_NOINLINE int inetRawUsrreq(InetSocket *so, int req, InetMbuf *m,
                                       InetMbuf *nam, InetMbuf *control, void *proc)
{
    InetRawcb *rp;
    int spl;
    int err = 0;
    (void)proc;

    if (req == INET_PRU_CONTROL) {
        return INET_EOPNOTSUPP;
    }

    spl = sceNet_lib_94B44F26();
    rp = (InetRawcb *)so->so_pcb;
    if (rp == NULL && req != INET_PRU_ATTACH) {
        sceNet_lib_515B2F33(spl);
        return INET_EINVAL;
    }

    switch (req) {
    case INET_PRU_ATTACH:
        err = inetRawAttach(so, (int)(uintptr_t)nam);
        break;
    case INET_PRU_DETACH:
        inetRawDetach(rp);
        break;
    case INET_PRU_BIND:
    case INET_PRU_LISTEN:
    case INET_PRU_CONNECT:
    case INET_PRU_RCVD:
    case INET_PRU_RCVOOB:
    case INET_PRU_CONNECT2:
        err = INET_EOPNOTSUPP;
        break;
    case INET_PRU_DISCONNECT:
        inetSoIsAborted(so);
        inetRawDisconnect(rp);
        break;
    case INET_PRU_SHUTDOWN:
        inetSoCantSendMore(so);
        break;
    case INET_PRU_SEND:
        if (control != NULL && control->m_len != 0) {
            sceNetIfhandle_lib_F56FAC82(control);
            sceNetIfhandle_lib_F56FAC82(m);
            err = INET_EINVAL;
            break;
        }
        if (nam != NULL) {
            if ((so->so_state & INET_SS_ISCONNECTED) != 0) {
                sceNetIfhandle_lib_F56FAC82(m);
                err = INET_EISCONN;
                break;
            }
            err = so->so_proto->pr_usrreq(so, INET_PRU_CONNECT, NULL, nam, NULL, NULL);
            if (err != 0) {
                sceNetIfhandle_lib_F56FAC82(m);
                break;
            }
        } else if ((so->so_state & INET_SS_ISCONNECTED) == 0) {
            sceNetIfhandle_lib_F56FAC82(m);
            err = INET_ENOTCONN;
            break;
        }
        err = so->so_proto->pr_output(m, so);
        if (nam != NULL) {
            inetRawDisconnect(rp);
        }
        break;
    case INET_PRU_SENSE:
        return 0;
    case INET_PRU_SENDOOB:
        sceNetIfhandle_lib_F56FAC82(control);
        sceNetIfhandle_lib_F56FAC82(m);
        err = INET_EOPNOTSUPP;
        break;
    case INET_PRU_SOCKADDR:
        if (rp->rcb_laddr == NULL) {
            err = INET_EINVAL;
        } else {
            u32 len = rp->rcb_laddr->sa_len;
            nam->m_len = (s32)len;
            sceNet_lib_2F305274(rp->rcb_laddr, nam->m_data, (int)len);
        }
        break;
    case INET_PRU_PEERADDR:
        if (rp->rcb_faddr == NULL) {
            err = INET_ENOTCONN;
        } else {
            u32 len = rp->rcb_faddr->sa_len;
            nam->m_len = (s32)len;
            sceNet_lib_2F305274(rp->rcb_faddr, nam->m_data, (int)len);
        }
        break;
    default:
        break;
    }

    sceNet_lib_515B2F33(spl);
    return err;
}

/* [213] 0x010494 */
static INET_NOINLINE void inetRawInit(void)
{
    g_rawcb_head = NULL;
}

/* [214] 0x0104A0 */
static INET_NOINLINE void *inetRawCtlinput(int cmd, InetSockaddr *sa, void *arg)
{
    (void)cmd;
    (void)sa;
    (void)arg;
    return NULL;
}

/* [215] 0x0104A8 */
INET_NOINLINE int inetLoOutput(InetIfnet *ifp, InetMbuf *m, InetSockaddr *dst, InetRtentry *rt)
{
    int spl;

    M_RCVIF(m) = ifp;
    if (rt != NULL && (rt->rt_flags & (INET_RTF_REJECT | INET_RTF_BLACKHOLE)) != 0) {
        sceNetIfhandle_lib_F56FAC82(m);
        if ((rt->rt_flags & INET_RTF_BLACKHOLE) != 0) {
            return 0;
        }
        return ((rt->rt_flags & INET_RTF_HOST) != 0) ? INET_EHOSTUNREACH : INET_ENETUNREACH;
    }

    /* Pull packet into contiguous leading mbuf if split */
    if (m->m_len != M_PKTLEN(m)) {
        InetMbuf *copy = (InetMbuf *)sceNetIfhandle_lib_15CFE3C0(sizeof(InetMbuf));
        s32 capacity = 208;
        if (copy != NULL) {
            copy->m_next = NULL;
            copy->m_nextpkt = NULL;
            copy->m_data = M_PKTDAT(copy);
            copy->m_type = INET_MT_HEADER;
            copy->m_flags = (u16)(m->m_flags & ~(INET_M_EXT | INET_M_CLUSTER));
            M_RCVIF(copy) = M_RCVIF(m);
            M_PKTLEN(copy) = M_PKTLEN(m);
            M_CSUM(copy) = M_CSUM(m);
            copy->mh.mh_pkt.aux[0] = m->mh.mh_pkt.aux[0];
            copy->mh.mh_pkt.aux[1] = m->mh.mh_pkt.aux[1];
            copy->mh.mh_pkt.aux[2] = m->mh.mh_pkt.aux[2];
            copy->mh.mh_pkt.aux[3] = m->mh.mh_pkt.aux[3];
            if (M_PKTLEN(m) > 208) {
                u8 *cl = (u8 *)sceNetIfhandle_lib_15CFE3C0(2048);
                if (cl == NULL) {
                    cl = (u8 *)sceNetIfhandle_lib_15CFE3C0(2048);
                }
                M_EXT(copy).ext_buf = cl;
                if (cl != NULL) {
                    copy->m_data = cl;
                    copy->m_flags |= (INET_M_EXT | INET_M_CLUSTER);
                    M_EXT(copy).ext_size = 2048;
                    M_EXT(copy).ext_free = NULL;
                    M_EXT(copy).ext_arg = NULL;
                    M_EXT(copy).ext_nextref = copy;
                    M_EXT(copy).ext_prevref = copy;
                }
                capacity = 2048;
                if ((copy->m_flags & INET_M_EXT) == 0) {
                    sceNetIfhandle_lib_F8825DC4(copy);
                    copy = NULL;
                }
            }
        }
        if (copy == NULL) {
            sceNetIfhandle_lib_F56FAC82(m);
            return INET_ENOBUFS;
        }
        if (capacity < M_PKTLEN(m)) {
            sceNetIfhandle_lib_9A6261EC(m, 0, capacity, copy->m_data);
            sceNetIfhandle_lib_BFF3CEA5(m, capacity);
            copy->m_len = capacity;
            m->m_flags &= (u16)~INET_M_PKTHDR;
            copy->m_next = m;
        } else {
            sceNetIfhandle_lib_9A6261EC(m, 0, M_PKTLEN(m), copy->m_data);
            copy->m_len = M_PKTLEN(m);
            copy->m_next = NULL;
            sceNetIfhandle_lib_F56FAC82(m);
        }
        m = copy;
    }

    ifp->if_data.ifi_opackets++;
    ifp->if_data.ifi_obytes += (u64)(s64)M_PKTLEN(m);

    if (dst->sa_family != INET_AF_INET) {
        sceNetIfhandle_lib_F56FAC82(m);
        return INET_EAFNOSUPPORT;
    }

    spl = sceNet_lib_94B44F26();
    if (g_ipintrq.ifq_len >= g_ipintrq.ifq_maxlen) {
        g_ipintrq.ifq_drops++;
        sceNetIfhandle_lib_F56FAC82(m);
        sceNet_lib_515B2F33(spl);
        return INET_ENOBUFS;
    }

    m->m_nextpkt = NULL;
    if (g_ipintrq.ifq_tail != NULL) {
        g_ipintrq.ifq_tail->m_nextpkt = m;
    } else {
        g_ipintrq.ifq_head = m;
    }
    g_ipintrq.ifq_tail = m;
    g_ipintrq.ifq_len++;
    sceNet_lib_05D525E4(2);

    ifp->if_data.ifi_ipackets++;
    ifp->if_data.ifi_ibytes += (u64)(s64)M_PKTLEN(m);
    sceNet_lib_515B2F33(spl);
    return 0;
}

/* [218] 0x0108F0 */
static INET_NOINLINE void inetLoRtRequest(int cmd, void *rtPtr, void *rtInfo)
{
    InetRtentry *rt = (InetRtentry *)rtPtr;
    (void)cmd;
    (void)rtInfo;
    if (rt != NULL) {
        rt->rt_rmx.rmx_mtu = 33212;
    }
}

/* [216] 0x0107F0 */
static INET_NOINLINE int inetLoIoctl(InetIfnet *ifp, u32 cmd, void *data)
{
    if (cmd == 0x8020690CU) { /* SIOCSIFADDR */
        ifp->if_flags |= INET_IFF_UP;
        if (data != NULL) {
            ((InetIfaddr *)data)->ifa_rtrequest = inetLoRtRequest;
        }
        return 0;
    }
    if (cmd == 0x80206931U || cmd == 0x80206932U) { /* SIOCADDMULTI / SIOCDELMULTI */
        InetIfreq *ifr = (InetIfreq *)data;
        if (ifr == NULL || ifr->ifr_u.ifru_addr.sa_family != INET_AF_INET) {
            return INET_EAFNOSUPPORT;
        }
        return 0;
    }
    return INET_EINVAL;
}

/* [217] 0x010864 */
INET_NOINLINE void inetLoAttach(void)
{
    sceNet_lib_CF705E46(g_lo0if.if_xname, "lo%d", 0);
    g_lo0if.if_ioctl = inetLoIoctl;
    g_lo0if.if_data.ifi_mtu = 33212;
    g_lo0if.if_flags = (u16)(INET_IFF_LOOPBACK | INET_IFF_MULTICAST);
    g_lo0if.if_output = (int (*)(InetIfnet *, InetMbuf *, InetSockaddr *, void *))inetLoOutput;
    g_lo0if.if_data.ifi_type = 24; /* IFT_LOOP */
    g_lo0if.if_softc = NULL;
    g_lo0if.if_data.ifi_hdrlen = 0;
    g_lo0if.if_data.ifi_addrlen = 0;
    sceNet_lib_701DDDC3(&g_lo0if);
}
