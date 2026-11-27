#include "pspnet_inet_internal.h"

InetRadixNodeHead *g_rt_tables[32];
InetRouteCb g_route_cb;

static s32 g_rttrash;
static InetRttimerQueue *g_rttimer_queue_head;
static s32 g_rttimer_initialized;
static InetCallout g_rttimer_callout;

typedef struct InetRtStat {
    u16 rts_badredirect;
    u16 rts_dynamic;
    u16 rts_newgateway;
    u16 rts_unreach;
    u16 rts_wildcard;
} InetRtStat;

static InetRtStat g_rtstat;

static void inetRtDestroy(InetRtentry *rt)
{
    inetRtTimerRemoveAll(rt);
    {
        InetIfaddr *ifa = rt->rt_ifa;
        ifa->ifa_refcnt--;
        if (ifa->ifa_refcnt == 0) {
            sceNet_lib_EDCC871E(ifa);
        }
    }
    sceNetIfhandle_lib_76BAD213(rt->rt_nodes[0].rn_u.rn_leaf.rn_key);
    sceNetIfhandle_lib_76BAD213(rt);
}

static void inetRtReleaseRef(InetRtentry *rt)
{
    rt->rt_refcnt--;
    if (rt->rt_refcnt <= 0 && (rt->rt_flags & INET_RTF_UP) == 0) {
        g_rttrash--;
        if (rt->rt_refcnt >= 0) {
            inetRtDestroy(rt);
        }
    }
}

/* [176] 0x00CCF8 */
INET_NOINLINE InetRtentry *inetRtAlloc1(InetSockaddr *dst, int report)
{
    InetRadixNodeHead *rnh = g_rt_tables[dst->sa_family];
    InetRtentry *newRt = NULL;
    int msgType = INET_RTM_MISS;
    int err = 0;
    int spl = sceNet_lib_94B44F26();

    if (rnh != NULL) {
        InetRadixNode *rn = rnh->rnh_matchaddr(dst, rnh);
        if (rn != NULL && (rn->rn_flags & 2) == 0) {
            InetRtentry *rt = (InetRtentry *)rn;
            newRt = rt;
            if (report == 0 || (rt->rt_flags & INET_RTF_CLONING) == 0) {
                rt->rt_refcnt++;
                sceNet_lib_515B2F33(spl);
                return newRt;
            }
            err = inetRtRequest(INET_RTM_RESOLVE, dst, NULL, NULL, 0, &newRt);
            if (err != 0) {
                newRt = rt;
                rt->rt_refcnt++;
            } else if (newRt == NULL || (newRt->rt_flags & INET_RTF_XRESOLVE) == 0) {
                sceNet_lib_515B2F33(spl);
                return newRt;
            } else {
                msgType = INET_RTM_RESOLVE;
            }
        } else {
            g_rtstat.rts_unreach++;
        }
    } else {
        g_rtstat.rts_unreach++;
    }

    if (report != 0) {
        InetRtAddrinfo addrInfo;
        sceNet_lib_8B615F52(&addrInfo, 36);
        addrInfo.rti_info[0] = dst;
        inetRtMissMsg(msgType, &addrInfo, 0, err);
    }
    sceNet_lib_515B2F33(spl);
    return newRt;
}

/* [177] 0x00CE64 */
INET_NOINLINE void inetRtRedirect(InetSockaddr *dst, InetSockaddr *gateway, InetSockaddr *netmask,
                                  int flags, InetSockaddr *src, InetRtentry **outRt)
{
    InetIfaddr *ifa;
    InetRtentry *rt = NULL;
    u16 *statCounter = NULL;
    int err = 0;
    InetRtAddrinfo addrInfo;

    ifa = inetIfaWithNet(gateway);
    if (ifa == NULL) {
        err = INET_ENETUNREACH;
        g_rtstat.rts_badredirect++;
    } else {
        rt = inetRtAlloc1(dst, 0);
        if ((flags & INET_RTF_DONE) == 0 && rt != NULL) {
            if (sceNet_lib_4BF83CF4(src, rt->rt_gateway, src->sa_len) != 0 ||
                rt->rt_ifa != ifa) {
                err = INET_EINVAL;
            }
        }
        if (err == 0 && inetIfaWithAddr(gateway) != NULL) {
            err = INET_EHOSTUNREACH;
        }
        if (err == 0) {
            int createNew = 0;
            if (rt == NULL) {
                createNew = 1;
            } else if (rt->rt_nodes[0].rn_u.rn_leaf.rn_mask != NULL &&
                       rt->rt_nodes[0].rn_u.rn_leaf.rn_mask[0] < 2) {
                createNew = 1;
            } else if ((rt->rt_flags & INET_RTF_GATEWAY) == 0) {
                err = INET_EHOSTUNREACH;
            } else if ((rt->rt_flags & INET_RTF_HOST) == 0 && (flags & INET_RTF_HOST) != 0) {
                createNew = 1;
            }

            if (err == 0) {
                if (createNew) {
                    flags |= (INET_RTF_GATEWAY | INET_RTF_DYNAMIC);
                    err = inetRtRequest(INET_RTM_ADD, dst, gateway, netmask, (u32)flags, NULL);
                    statCounter = &g_rtstat.rts_dynamic;
                } else {
                    rt->rt_flags |= INET_RTF_MODIFIED;
                    flags |= INET_RTF_MODIFIED;
                    statCounter = &g_rtstat.rts_newgateway;
                    inetRtSetgate(rt, (InetSockaddr *)rt->rt_nodes[0].rn_u.rn_leaf.rn_key, gateway);
                }
            }
        }

        if (rt != NULL) {
            if (outRt != NULL && err == 0) {
                *outRt = rt;
            } else {
                inetRtReleaseRef(rt);
            }
        }

        if (err != 0) {
            g_rtstat.rts_badredirect++;
        } else if (statCounter != NULL) {
            (*statCounter)++;
        }
    }

    sceNet_lib_8B615F52(&addrInfo, 36);
    addrInfo.rti_info[0] = dst;
    addrInfo.rti_info[1] = gateway;
    addrInfo.rti_info[2] = netmask;
    addrInfo.rti_info[6] = src;
    inetRtMissMsg(INET_RTM_REDIRECT, &addrInfo, flags, err);
}

/* [178] 0x00D0E8 */
INET_NOINLINE InetIfaddr *inetIfaIfWithRoute(int flags, InetSockaddr *dst, InetSockaddr *gateway)
{
    InetIfaddr *ifa = NULL;

    if ((flags & INET_RTF_GATEWAY) == 0) {
        if ((flags & INET_RTF_HOST) != 0) {
            ifa = inetIfaWithDstAddr(dst);
        }
        if (ifa == NULL) {
            ifa = inetIfaWithAddr(gateway);
        }
    } else {
        ifa = inetIfaWithDstAddr(gateway);
    }

    if (ifa == NULL) {
        ifa = inetIfaWithNet(gateway);
    }
    if (ifa == NULL) {
        InetRtentry *rt = inetRtAlloc1(dst, 0);
        if (rt == NULL) {
            return NULL;
        }
        ifa = rt->rt_ifa;
        rt->rt_refcnt--;
        if (ifa == NULL) {
            return NULL;
        }
    }
    if (ifa->ifa_addr->sa_family != dst->sa_family) {
        InetIfaddr *alt = inetIfaOfIfpForAddr(dst, ifa->ifa_ifp);
        if (alt != NULL) {
            return alt;
        }
    }
    return ifa;
}

/* [179] 0x00D1C8 */
INET_NOINLINE int inetRtRequest(int req, InetSockaddr *dst, InetSockaddr *gateway,
                                InetSockaddr *netmask, u32 flags, InetRtentry **retRt)
{
    int spl = sceNet_lib_94B44F26();
    InetRadixNodeHead *rnh = g_rt_tables[dst->sa_family];
    int err = 0;

    if (rnh == NULL) {
        sceNet_lib_515B2F33(spl);
        return INET_ESRCH;
    }
    if ((flags & INET_RTF_HOST) != 0) {
        netmask = NULL;
    }

    if (req == INET_RTM_DELETE) {
        InetRadixNode *found = rnh->rnh_lookup(dst, netmask, rnh);
        if (found == NULL) {
            sceNet_lib_515B2F33(spl);
            return INET_ESRCH;
        }
        if ((((InetRtentry *)found)->rt_flags & INET_RTF_CLONING) != 0) {
            rnh->rnh_walktree(rnh, inetRtFlushClone1, found);
        }
        {
            InetRtentry *rt = (InetRtentry *)rnh->rnh_deladdr(dst, netmask, rnh);
            if (rt == NULL) {
                sceNet_lib_515B2F33(spl);
                return INET_ESRCH;
            }
            if (rt->rt_gwroute != NULL) {
                InetRtentry *gw = rt->rt_gwroute;
                if (gw->rt_refcnt >= 2) {
                    gw->rt_refcnt--;
                } else {
                    inetRtReleaseRef(gw);
                }
                rt->rt_gwroute = NULL;
            }
            if (rt->rt_parent != NULL) {
                rt->rt_parent->rt_refcnt--;
                rt->rt_parent = NULL;
            }
            rt->rt_flags &= ~INET_RTF_UP;
            if (rt->rt_ifa != NULL && rt->rt_ifa->ifa_rtrequest != NULL) {
                rt->rt_ifa->ifa_rtrequest(INET_RTM_DELETE, rt, NULL);
            }
            g_rttrash++;
            if (retRt != NULL) {
                *retRt = rt;
            } else if (rt->rt_refcnt <= 0 && (rt->rt_flags & INET_RTF_UP) == 0) {
                g_rttrash--;
                if (rt->rt_refcnt >= 0) {
                    inetRtDestroy(rt);
                }
            }
        }
        sceNet_lib_515B2F33(spl);
        return 0;
    }

    if (req == INET_RTM_ADD || req == INET_RTM_RESOLVE) {
        InetIfaddr *ifa;
        InetRtentry *rt;
        InetSockaddr *rtKey;
        InetRadixNode *added;

        if (req == INET_RTM_RESOLVE) {
            InetRtentry *parentRt;
            if (retRt == NULL || *retRt == NULL || ((*retRt)->rt_flags & INET_RTF_CLONING) == 0) {
                sceNet_lib_515B2F33(spl);
                return INET_EINVAL;
            }
            parentRt = *retRt;
            netmask = parentRt->rt_genmask;
            ifa = parentRt->rt_ifa;
            gateway = parentRt->rt_gateway;
            flags = ((u32)parentRt->rt_flags & ~((u32)(INET_RTF_CLONING | INET_RTF_STATIC | INET_RTF_CLONED))) | INET_RTF_CLONED;
            if (netmask == NULL) {
                flags |= INET_RTF_HOST;
            } else {
                flags &= ~((u32)INET_RTF_HOST);
            }
        } else {
            ifa = inetIfaIfWithRoute((int)flags, dst, gateway);
            if (ifa == NULL) {
                sceNet_lib_515B2F33(spl);
                return INET_ENETUNREACH;
            }
        }

        rt = (InetRtentry *)sceNetIfhandle_lib_15CFE3C0(sizeof(InetRtentry));
        if (rt == NULL) {
            sceNet_lib_515B2F33(spl);
            return INET_ENOBUFS;
        }
        sceNet_lib_8B615F52(rt, sizeof(InetRtentry));
        rt->rt_flags = (s32)(flags | INET_RTF_UP);
        rt->rt_timer = NULL;
        if (inetRtSetgate(rt, dst, gateway) != 0) {
            sceNetIfhandle_lib_76BAD213(rt);
            sceNet_lib_515B2F33(spl);
            return INET_ENOBUFS;
        }

        rtKey = (InetSockaddr *)rt->rt_nodes[0].rn_u.rn_leaf.rn_key;
        if (netmask != NULL) {
            inetRtMaskedCopy((const u8 *)dst, (u8 *)rtKey, (const u8 *)netmask);
        } else {
            sceNet_lib_2F305274(dst, rtKey, dst->sa_len);
        }

        ifa->ifa_refcnt++;
        rt->rt_ifp = ifa->ifa_ifp;
        rt->rt_ifa = ifa;

        if (req == INET_RTM_RESOLVE) {
            InetRtentry *parentRt = *retRt;
            rt->rt_rmx = parentRt->rt_rmx;
            rt->rt_parent = parentRt;
            parentRt->rt_refcnt++;
        } else if (rt->rt_rmx.rmx_mtu == 0 && (rt->rt_rmx.rmx_locks & 1) == 0) {
            if (rt->rt_gwroute != NULL) {
                rt->rt_rmx.rmx_mtu = rt->rt_gwroute->rt_rmx.rmx_mtu;
            } else {
                rt->rt_rmx.rmx_mtu = (u32)ifa->ifa_ifp->if_data.ifi_mtu;
            }
        }

        added = rnh->rnh_addaddr(rtKey, netmask, rnh, rt->rt_nodes);
        if (added == NULL) {
            InetRtentry *existing = inetRtAlloc1(rtKey, 0);
            if (existing != NULL) {
                if ((existing->rt_flags & INET_RTF_CLONED) != 0) {
                    InetRtentry *delRt = existing;
                    InetRtAddrinfo info;
                    int delErr;
                    sceNet_lib_8B615F52(&info, 36);
                    info.rti_info[0] = (InetSockaddr *)delRt->rt_nodes[0].rn_u.rn_leaf.rn_key;
                    info.rti_info[1] = delRt->rt_gateway;
                    info.rti_info[2] = (InetSockaddr *)delRt->rt_nodes[0].rn_u.rn_leaf.rn_mask;
                    delErr = inetRtRequest(INET_RTM_DELETE,
                                           info.rti_info[0], info.rti_info[1], info.rti_info[2],
                                           (u32)delRt->rt_flags, &delRt);
                    inetRtMissMsg(INET_RTM_DELETE, &info, delRt->rt_flags, delErr);
                    if (delErr == 0 && delRt->rt_refcnt <= 0 && (delRt->rt_flags & INET_RTF_UP) == 0) {
                        g_rttrash--;
                        if (delRt->rt_refcnt >= 0) {
                            inetRtDestroy(delRt);
                        }
                    }
                    added = rnh->rnh_addaddr(rtKey, netmask, rnh, rt->rt_nodes);
                }
                if (existing->rt_refcnt >= 2) {
                    existing->rt_refcnt--;
                } else {
                    inetRtReleaseRef(existing);
                }
            }
        }

        if (added == NULL) {
            ifa->ifa_refcnt--;
            if (ifa->ifa_refcnt == 0) {
                sceNet_lib_EDCC871E(ifa);
            }
            if ((rt->rt_flags & INET_RTF_CLONED) != 0 && rt->rt_parent != NULL) {
                inetRtReleaseRef(rt->rt_parent);
            }
            if (rt->rt_gwroute != NULL) {
                inetRtReleaseRef(rt->rt_gwroute);
            }
            sceNetIfhandle_lib_76BAD213(rt->rt_nodes[0].rn_u.rn_leaf.rn_key);
            sceNetIfhandle_lib_76BAD213(rt);
            err = INET_EEXIST;
        } else {
            if (ifa->ifa_rtrequest != NULL) {
                ifa->ifa_rtrequest(req, rt, (retRt != NULL) ? *retRt : NULL);
            }
            if (retRt != NULL) {
                *retRt = rt;
                rt->rt_refcnt++;
            }
            if ((rt->rt_flags & INET_RTF_CLONING) != 0) {
                rnh->rnh_walktree(rnh, inetRtFlushClone1, rt);
            }
        }
    }

    sceNet_lib_515B2F33(spl);
    return err;
}

/* [180] 0x00D9BC */
INET_NOINLINE int inetRtSetgate(InetRtentry *rt, InetSockaddr *dst, InetSockaddr *gate)
{
    u32 dstLen = (dst->sa_len != 0) ? (((u32)(dst->sa_len - 1) | 3U) + 1U) : 4U;
    u32 gateLen = (gate->sa_len != 0) ? (((u32)(gate->sa_len - 1) | 3U) + 1U) : 4U;
    u32 totalLen = dstLen + gateLen;
    InetSockaddr *oldGate = rt->rt_gateway;
    u8 *oldBuf = NULL;
    u8 *buf;

    if (oldGate != NULL) {
        u32 curGateLen = (oldGate->sa_len != 0) ? (((u32)(oldGate->sa_len - 1) | 3U) + 1U) : 4U;
        if (curGateLen >= gateLen) {
            buf = rt->rt_nodes[0].rn_u.rn_leaf.rn_key;
        } else {
            oldBuf = rt->rt_nodes[0].rn_u.rn_leaf.rn_key;
            buf = (u8 *)sceNetIfhandle_lib_15CFE3C0(totalLen);
            if (buf == NULL) {
                return 1;
            }
            sceNet_lib_8B615F52(buf, (int)totalLen);
            rt->rt_nodes[0].rn_u.rn_leaf.rn_key = buf;
        }
    } else {
        oldBuf = rt->rt_nodes[0].rn_u.rn_leaf.rn_key;
        buf = (u8 *)sceNetIfhandle_lib_15CFE3C0(totalLen);
        if (buf == NULL) {
            return 1;
        }
        sceNet_lib_8B615F52(buf, (int)totalLen);
        rt->rt_nodes[0].rn_u.rn_leaf.rn_key = buf;
    }

    rt->rt_gateway = (InetSockaddr *)(buf + dstLen);
    sceNet_lib_2F305274(gate, buf + dstLen, (int)gateLen);
    if (oldBuf != NULL) {
        sceNet_lib_2F305274(dst, buf, (int)dstLen);
        sceNetIfhandle_lib_76BAD213(oldBuf);
    }

    if (rt->rt_gwroute != NULL) {
        InetRtentry *gw = rt->rt_gwroute;
        if (gw->rt_refcnt >= 2) {
            gw->rt_refcnt--;
        } else {
            inetRtReleaseRef(gw);
        }
        rt->rt_gwroute = NULL;
    }

    if ((rt->rt_flags & INET_RTF_GATEWAY) != 0) {
        InetRtentry *gw = inetRtAlloc1(gate, 1);
        rt->rt_gwroute = gw;
        if (gw != NULL && (rt->rt_rmx.rmx_locks & 1) == 0) {
            if (rt->rt_rmx.rmx_mtu == 0 || gw->rt_rmx.rmx_mtu < rt->rt_rmx.rmx_mtu) {
                rt->rt_rmx.rmx_mtu = gw->rt_rmx.rmx_mtu;
            }
        }
    }
    return 0;
}

/* [181] 0x00DBE0 */
INET_NOINLINE void inetRtMaskedCopy(const u8 *src, u8 *dst, const u8 *mask)
{
    u32 srcLen = src[0];
    u32 maskLen = mask[0];
    u32 limit = (srcLen < maskLen) ? srcLen : maskLen;
    u32 pos;

    dst[0] = (u8)srcLen;
    dst[1] = src[1];
    for (pos = 2; pos < limit; pos++) {
        dst[pos] = (u8)(src[pos] & mask[pos]);
    }
    if (pos < srcLen) {
        sceNet_lib_8B615F52(dst + pos, (int)(srcLen - pos));
    }
}

/* [182] 0x00DC80 */
INET_NOINLINE int inetRtInit(InetIfaddr *ifa, int cmd, int flags)
{
    InetSockaddr *dst = ((flags & INET_RTF_HOST) != 0) ? ifa->ifa_dstaddr : ifa->ifa_addr;
    InetRtentry *rt = NULL;
    u8 maskedDst[128];
    int err;

    if (cmd == INET_RTM_DELETE) {
        InetSockaddr *lookupDst = dst;
        if ((flags & INET_RTF_HOST) == 0 && ifa->ifa_netmask != NULL) {
            inetRtMaskedCopy((const u8 *)dst, maskedDst, (const u8 *)ifa->ifa_netmask);
            dst = (InetSockaddr *)maskedDst;
            lookupDst = dst;
        }
        {
            InetRtentry *found = inetRtAlloc1(lookupDst, 0);
            if (found != NULL) {
                InetIfaddr *foundIfa = found->rt_ifa;
                found->rt_refcnt--;
                if (foundIfa != ifa) {
                    return ((flags & INET_RTF_HOST) != 0) ? INET_EHOSTUNREACH : INET_ENETUNREACH;
                }
            }
        }
    }

    err = inetRtRequest(cmd, dst, ifa->ifa_addr, ifa->ifa_netmask,
                        (u32)(flags | (int)ifa->ifa_flags), &rt);

    if (cmd == INET_RTM_DELETE && err == 0 && rt != NULL) {
        inetRtNewAddrMsg(cmd, ifa, err, rt);
        if (rt->rt_refcnt <= 0 && (rt->rt_flags & INET_RTF_UP) == 0) {
            g_rttrash--;
            if (rt->rt_refcnt >= 0) {
                inetRtDestroy(rt);
            }
        }
    } else if (cmd == INET_RTM_ADD && err == 0 && rt != NULL) {
        rt->rt_refcnt--;
        if (rt->rt_ifa != ifa) {
            InetIfaddr *oldIfa = rt->rt_ifa;
            if (oldIfa->ifa_rtrequest != NULL) {
                oldIfa->ifa_rtrequest(INET_RTM_DELETE, rt, NULL);
                oldIfa = rt->rt_ifa;
            }
            oldIfa->ifa_refcnt--;
            if (oldIfa->ifa_refcnt == 0) {
                sceNet_lib_EDCC871E(oldIfa);
            }
            rt->rt_ifp = ifa->ifa_ifp;
            rt->rt_ifa = ifa;
            ifa->ifa_refcnt++;
            rt->rt_rmx.rmx_mtu = (u32)ifa->ifa_ifp->if_data.ifi_mtu;
            if (ifa->ifa_rtrequest != NULL) {
                ifa->ifa_rtrequest(INET_RTM_ADD, rt, NULL);
            }
        }
        inetRtNewAddrMsg(cmd, ifa, err, rt);
    }
    return err;
}

/* [183] 0x00DEE8 */
INET_NOINLINE void inetRtTimerQueueDestroy(InetRttimerQueue *queue, int destroyRoutes)
{
    while (queue->rtq_head != NULL) {
        InetRttimer *timer = queue->rtq_head;
        if (timer->rtt_link_next != NULL) {
            timer->rtt_link_next->rtt_link_prev = timer->rtt_link_prev;
        }
        *timer->rtt_link_prev = timer->rtt_link_next;

        if (timer->rtt_next != NULL) {
            timer->rtt_next->rtt_prev = timer->rtt_prev;
        } else {
            queue->rtq_tail = timer->rtt_prev;
        }
        *timer->rtt_prev = timer->rtt_next;

        if (destroyRoutes != 0) {
            if (timer->rtt_func != NULL) {
                timer->rtt_func(timer->rtt_rt, timer);
            } else {
                inetRtRequest(INET_RTM_DELETE,
                              (InetSockaddr *)timer->rtt_rt->rt_nodes[0].rn_u.rn_leaf.rn_key,
                              NULL, NULL, 0, NULL);
            }
        }
        sceNetIfhandle_lib_76BAD213(timer);
    }

    if (queue->rtq_next != NULL) {
        queue->rtq_next->rtq_prev = queue->rtq_prev;
    }
    *queue->rtq_prev = queue->rtq_next;
}

/* [184] 0x00DFD0 */
INET_NOINLINE void inetRtTimerRemoveAll(InetRtentry *rt)
{
    while (rt->rt_timer != NULL) {
        InetRttimer *timer = rt->rt_timer;
        if (timer->rtt_link_next != NULL) {
            timer->rtt_link_next->rtt_link_prev = timer->rtt_link_prev;
        }
        *timer->rtt_link_prev = timer->rtt_link_next;

        if (timer->rtt_next != NULL) {
            timer->rtt_next->rtt_prev = timer->rtt_prev;
        } else {
            timer->rtt_queue->rtq_tail = timer->rtt_prev;
        }
        *timer->rtt_prev = timer->rtt_next;

        sceNetIfhandle_lib_76BAD213(timer);
    }
}

/* [185] 0x00E04C */
INET_NOINLINE int inetRtTimerAdd(InetRtentry *rt,
                                 void (*func)(InetRtentry *, InetRttimer *),
                                 InetRttimerQueue *queue)
{
    u32 now = sceNet_lib_75D9985C();
    InetRttimer *cur = rt->rt_timer;
    InetRttimer *timer;

    while (cur != NULL) {
        if (cur->rtt_func == func) {
            if (cur->rtt_link_next != NULL) {
                cur->rtt_link_next->rtt_link_prev = cur->rtt_link_prev;
            }
            *cur->rtt_link_prev = cur->rtt_link_next;

            if (cur->rtt_next != NULL) {
                cur->rtt_next->rtt_prev = cur->rtt_prev;
            } else {
                cur->rtt_queue->rtq_tail = cur->rtt_prev;
            }
            *cur->rtt_prev = cur->rtt_next;
            sceNetIfhandle_lib_76BAD213(cur);
            break;
        }
        cur = cur->rtt_link_next;
    }

    timer = (InetRttimer *)sceNetIfhandle_lib_15CFE3C0(sizeof(InetRttimer));
    if (timer == NULL) {
        return INET_ENOBUFS;
    }
    sceNet_lib_8B615F52(timer, sizeof(InetRttimer));
    timer->rtt_time = now;
    timer->rtt_func = func;
    timer->rtt_rt = rt;
    timer->rtt_queue = queue;

    timer->rtt_link_next = rt->rt_timer;
    if (rt->rt_timer != NULL) {
        rt->rt_timer->rtt_link_prev = &timer->rtt_link_next;
    }
    timer->rtt_link_prev = &rt->rt_timer;
    rt->rt_timer = timer;

    timer->rtt_next = NULL;
    timer->rtt_prev = queue->rtq_tail;
    *queue->rtq_tail = timer;
    queue->rtq_tail = &timer->rtt_next;
    return 0;
}

/* [186] 0x00E180 */
INET_NOINLINE void inetRtTimerRun(void *unusedArg)
{
    u32 now = sceNet_lib_75D9985C();
    int spl = sceNet_lib_94B44F26();
    InetRttimerQueue *queue = g_rttimer_queue_head;
    (void)unusedArg;

    while (queue != NULL) {
        while (queue->rtq_head != NULL &&
               (s32)(queue->rtq_head->rtt_time + (u32)queue->rtq_timeout) < (s32)now) {
            InetRttimer *timer = queue->rtq_head;
            if (timer->rtt_link_next != NULL) {
                timer->rtt_link_next->rtt_link_prev = timer->rtt_link_prev;
            }
            *timer->rtt_link_prev = timer->rtt_link_next;

            if (timer->rtt_next != NULL) {
                timer->rtt_next->rtt_prev = timer->rtt_prev;
            } else {
                queue->rtq_tail = timer->rtt_prev;
            }
            *timer->rtt_prev = timer->rtt_next;

            if (timer->rtt_func != NULL) {
                timer->rtt_func(timer->rtt_rt, timer);
            } else {
                inetRtRequest(INET_RTM_DELETE,
                              (InetSockaddr *)timer->rtt_rt->rt_nodes[0].rn_u.rn_leaf.rn_key,
                              NULL, NULL, 0, NULL);
            }
            sceNetIfhandle_lib_76BAD213(timer);
        }
        queue = queue->rtq_next;
    }

    sceNet_lib_515B2F33(spl);
    sceNet_lib_7C86FBA4(&g_rttimer_callout, 100, inetRtTimerRun, NULL);
}

/* [187] 0x00E2CC */
INET_NOINLINE void inetRouteInit(void)
{
    InetDomain *dp;

    sceKernelMemset(&g_route_cb, 0, sizeof(g_route_cb));
    sceKernelMemset(g_rt_tables, 0, sizeof(g_rt_tables));
    sceKernelMemset(&g_rtstat, 0, sizeof(g_rtstat));
    g_rttrash = 0;
    g_rttimer_initialized = 0;

    inetRnInit();
    for (dp = g_domains; dp != NULL; dp = dp->dom_next) {
        if (dp->dom_rtattach != NULL) {
            dp->dom_rtattach((void **)&g_rt_tables[dp->dom_family], dp->dom_rtoffset);
        }
    }
}

/* [188] 0x00E388 */
INET_NOINLINE void inetRtAlloc(InetRoute *ro)
{
    if (ro->ro_rt == NULL || ro->ro_rt->rt_ifp == NULL ||
        (ro->ro_rt->rt_flags & INET_RTF_UP) == 0) {
        ro->ro_rt = inetRtAlloc1(&ro->ro_dst, 1);
    }
}

/* [189] 0x00E3E0 */
INET_NOINLINE void inetRtFree(InetRtentry *rt)
{
    inetRtReleaseRef(rt);
}

/* [190] 0x00E478 */
INET_NOINLINE int inetRtIoctl(u32 cmd, void *data, InetIfnet *ifp)
{
    (void)cmd;
    (void)data;
    (void)ifp;
    return INET_EOPNOTSUPP;
}

/* [191] 0x00E480 */
INET_NOINLINE InetRttimerQueue *inetRtTimerQueueCreate(s32 timeout)
{
    InetRttimerQueue *queue;

    if (g_rttimer_initialized == 0) {
        g_rttimer_queue_head = NULL;
        sceNet_lib_25CC373A(&g_rttimer_callout);
        sceNet_lib_7C86FBA4(&g_rttimer_callout, 100, inetRtTimerRun, NULL);
        g_rttimer_initialized = 1;
    }

    queue = (InetRttimerQueue *)sceNetIfhandle_lib_15CFE3C0(sizeof(InetRttimerQueue));
    if (queue == NULL) {
        return NULL;
    }
    sceNet_lib_8B615F52(queue, sizeof(InetRttimerQueue));
    queue->rtq_timeout = timeout;
    queue->rtq_head = NULL;
    queue->rtq_tail = &queue->rtq_head;
    queue->rtq_next = g_rttimer_queue_head;
    if (g_rttimer_queue_head != NULL) {
        g_rttimer_queue_head->rtq_prev = &queue->rtq_next;
    }
    queue->rtq_prev = &g_rttimer_queue_head;
    g_rttimer_queue_head = queue;
    return queue;
}

/* [192] 0x00E55C */
INET_NOINLINE void inetRtTimerQueueChange(InetRttimerQueue *queue, s32 timeout)
{
    queue->rtq_timeout = timeout;
}

/* [193] 0x00E564 */
INET_NOINLINE int inetRtFlushClone1(InetRadixNode *rn, void *parentCtx)
{
    InetRtentry *rt = (InetRtentry *)rn;
    if ((rt->rt_flags & INET_RTF_CLONED) != 0 && rt->rt_parent == (InetRtentry *)parentCtx) {
        inetRtFlushClone(rt);
    }
    return 0;
}

/* [194] 0x00E5A4 */
INET_NOINLINE int inetRtFlushClone(InetRtentry *rt)
{
    InetRtentry *delRt = rt;
    InetRtAddrinfo addrInfo;
    int err;

    sceNet_lib_8B615F52(&addrInfo, 36);
    addrInfo.rti_info[0] = (InetSockaddr *)delRt->rt_nodes[0].rn_u.rn_leaf.rn_key;
    addrInfo.rti_info[1] = delRt->rt_gateway;
    addrInfo.rti_info[2] = (InetSockaddr *)delRt->rt_nodes[0].rn_u.rn_leaf.rn_mask;

    err = inetRtRequest(INET_RTM_DELETE,
                        addrInfo.rti_info[0],
                        addrInfo.rti_info[1],
                        addrInfo.rti_info[2],
                        (u32)delRt->rt_flags,
                        &delRt);
    inetRtMissMsg(INET_RTM_DELETE, &addrInfo, delRt->rt_flags, err);
    if (err == 0 && delRt->rt_refcnt <= 0 && (delRt->rt_flags & INET_RTF_UP) == 0) {
        g_rttrash--;
        if (delRt->rt_refcnt >= 0) {
            inetRtDestroy(delRt);
        }
    }
    return err;
}
