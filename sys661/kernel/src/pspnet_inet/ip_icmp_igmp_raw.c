/*
 * PSP 6.61 SceNetInet_Library (pspnet_inet.prx)
 * IPv4 ICMP, IGMP, and Raw IP (ip_icmp.c / igmp.c / raw_ip.c)
 */

#include "pspnet_inet_internal.h"

/* IGMP wire header */
typedef struct InetIgmpHdr {
    u8 igmp_type;
    u8 igmp_code;
    u16 igmp_cksum;
    u32 igmp_group;
} InetIgmpHdr;

/* Per-interface IGMP router version info */
typedef struct InetIgmpRouterInfo {
    InetIfnet *rti_ifp;
    u32 rti_type;
    u32 rti_time;
    struct InetIgmpRouterInfo *rti_next;
} InetIgmpRouterInfo;

/* Path MTU discovery callback list node */
typedef struct InetMtudiscCallback {
    struct InetMtudiscCallback *mc_next;
    struct InetMtudiscCallback **mc_prev;
    void (*mc_func)(u32);
} InetMtudiscCallback;

/* ICMP statistics (368 bytes = 46 x u64 at 0x364D8) */
typedef struct InetIcmpStat {
    u64 icps_error;
    u64 icps_oldshort;
    u64 icps_oldicmp;
    u64 icps_outhist[19];
    u64 icps_badcode;
    u64 icps_tooshort;
    u64 icps_checksum;
    u64 icps_badlen;
    u64 icps_reflect;
    u64 icps_inhist[19];
} InetIcmpStat;

/* IGMP statistics (72 bytes = 9 x u64 at 0x36650) */
typedef struct InetIgmpStat {
    u64 igps_rcv_total;
    u64 igps_rcv_tooshort;
    u64 igps_rcv_badsum;
    u64 igps_rcv_queries;
    u64 igps_rcv_badqueries;
    u64 igps_rcv_reports;
    u64 igps_rcv_badreports;
    u64 igps_rcv_ourreports;
    u64 igps_snd_reports;
} InetIgmpStat;

static u32 g_icmp_returndatabytes = 8;
static InetSockaddrIn g_icmpdst = { 16, INET_AF_INET, 0, 0, { 0 } };
static InetSockaddrIn g_icmpsrc = { 16, INET_AF_INET, 0, 0, { 0 } };
static InetSockaddrIn g_icmpgw = { 16, INET_AF_INET, 0, 0, { 0 } };
int g_icmp_errppslim = 100;
static struct {
    int maskrepl;
    InetMtudiscCallback *mtudisc_callbacks;
} g_icmp_ctl;
static InetIcmpStat g_icmpstat;

static const u32 g_mtudisc_plateaus[14] = {
    65535, 65280, 32000, 17914, 9180, 8166, 4352,
    2002,  1492,  1006,  508,   296,  68,   0
};

static int g_igmp_timers_are_running = 0;
static InetIgmpRouterInfo *g_igmp_rti_head = NULL;
static InetIgmpStat g_igmpstat;

InetInpcbTable g_rawcb_table;
u32 g_rip_sendspace = 8192;
u32 g_rip_recvspace = 8192;

static INET_NOINLINE void inetIcmpReflect(InetMbuf *m);
static INET_NOINLINE int inetIcmpSend(InetMbuf *m, InetMbuf *opts);
INET_NOINLINE u32 inetIptime(void);
static INET_NOINLINE int inetIcmpRatecheck(u32 *srcAddr, int type, int code);
static INET_NOINLINE void inetIcmpMtudiscTimeout(InetRtentry *rt, InetRttimer *timer);
static INET_NOINLINE void inetIgmpSendpkt(InetInMulti *inm, u8 type);

static InetIgmpRouterInfo *igmpFindOrCreateRti(InetIfnet *ifp)
{
    InetIgmpRouterInfo *rti = g_igmp_rti_head;
    while (rti != NULL) {
        if (rti->rti_ifp == ifp) {
            return rti;
        }
        rti = rti->rti_next;
    }
    rti = (InetIgmpRouterInfo *)sceNetIfhandle_lib_15CFE3C0(sizeof(InetIgmpRouterInfo));
    if (rti != NULL) {
        rti->rti_ifp = ifp;
        rti->rti_type = 2;
        rti->rti_time = 0;
        rti->rti_next = g_igmp_rti_head;
        g_igmp_rti_head = rti;
    }
    return rti;
}

static InetInIfaddr *findFirstInetIfa(InetIfnet *ifp)
{
    InetIfaddr *ifa = ifp->if_addrhead;
    while (ifa != NULL) {
        if (ifa->ifa_addr->sa_family == INET_AF_INET) {
            return (InetInIfaddr *)ifa;
        }
        ifa = ifa->ifa_next;
    }
    return NULL;
}

/* [296] 0x01A190 */
INET_NOINLINE void inetIcmpError(InetMbuf *n, int type, int code, u32 dest, InetIfnet *destifp)
{
    InetIpHdr *oip = (InetIpHdr *)n->m_data;
    u32 oiplen = (u32)(oip->ip_vhl & 0x0F) * 4;
    int canSend = 1;

    if (type != 5) {
        g_icmpstat.icps_error++;
    }

    if ((n->m_flags & 0x20) != 0 || (oip->ip_off & 0x9FFF) != 0) {
        canSend = 0;
    }

    if (canSend && oip->ip_p == INET_IPPROTO_ICMP && type != 5) {
        if (n->m_len >= (s32)(oiplen + 8)) {
            u8 otype = ((u8 *)oip)[oiplen];
            if (otype != 0 && otype != 8 && otype != 9 && otype != 10 &&
                otype != 13 && otype != 14 && otype != 15 && otype != 16 &&
                otype != 17 && otype != 18) {
                g_icmpstat.icps_oldicmp++;
                canSend = 0;
            }
        }
    }

    if (canSend && (n->m_flags & (INET_M_BCAST | INET_M_MCAST)) != 0) {
        canSend = 0;
    }

    if (canSend && inetIcmpRatecheck(&oip->ip_src, type, code) == 0) {
        u32 icmplen = (u32)oip->ip_len - oiplen;
        u32 totalData;
        u32 avail = 0;
        InetMbuf *scan = n;
        InetMbuf *m;

        if (icmplen > g_icmp_returndatabytes) {
            icmplen = g_icmp_returndatabytes;
        }
        totalData = oiplen + icmplen;
        while (scan != NULL && avail < totalData) {
            avail += (u32)scan->m_len;
            scan = scan->m_next;
        }
        if (avail < totalData) {
            totalData = avail;
        }
        if (totalData + 8 > 2048) {
            totalData = 2040;
        }

        m = sceNetIfhandle_lib_4CF15C43(INET_M_DONTWAIT, INET_MT_HEADER);
        if (m != NULL) {
            u32 icmpTotal = totalData + 8;
            if (icmpTotal > 208) {
                u8 *cl = (u8 *)sceNetIfhandle_lib_15CFE3C0(2048);
                if (cl == NULL) {
                    cl = (u8 *)sceNetIfhandle_lib_15CFE3C0(2048);
                }
                if (cl != NULL) {
                    m->m_ext.ext_buf = cl;
                    m->m_data = cl;
                    m->m_flags |= (INET_M_EXT | 8);
                    m->m_ext.ext_size = 2048;
                    m->m_ext.ext_free = NULL;
                    m->m_ext.ext_arg = NULL;
                    m->m_ext.ext_nextref = m;
                    m->m_ext.ext_prevref = m;
                }
                if ((m->m_flags & INET_M_EXT) == 0) {
                    sceNetIfhandle_lib_F56FAC82(m);
                    m = NULL;
                }
            }
            if (m != NULL) {
                InetIcmpHdr *icp;
                InetIpHdr *nip;

                m->m_len = (s32)icmpTotal;
                if ((m->m_flags & INET_M_EXT) == 0) {
                    m->m_data += (208 - icmpTotal) & ~3U;
                }
                g_icmpstat.icps_outhist[type]++;
                icp = (InetIcmpHdr *)m->m_data;
                icp->icmp_type = (u8)type;
                if (type == 5) {
                    icp->icmp_hun.ih_gwaddr = dest;
                } else {
                    icp->icmp_hun.ih_void = 0;
                    if (type == 12) {
                        icp->icmp_hun.ih_pptr = (u8)code;
                        code = 0;
                    } else if (type == 3 && code == 4 && destifp != NULL) {
                        icp->icmp_hun.ih_pmtu.ipm_nextmtu = inetBswap16((u16)destifp->if_data.ifi_mtu);
                    }
                }
                oip->ip_off = inetBswap16(oip->ip_off);
                oip->ip_len = inetBswap16(oip->ip_len);
                icp->icmp_code = (u8)code;
                sceNetIfhandle_lib_9A6261EC(n, 0, (s32)totalData, (u8 *)&icp->icmp_dun.id_ip);

                m->m_data -= sizeof(InetIpHdr);
                m->m_len += (s32)sizeof(InetIpHdr);
                M_PKTLEN(m) = m->m_len;
                M_RCVIF(m) = M_RCVIF(n);
                nip = (InetIpHdr *)m->m_data;
                nip->ip_vhl = (u8)((nip->ip_vhl & 0xF0) | 5);
                nip->ip_tos = 0;
                nip->ip_len = (u16)m->m_len;
                nip->ip_off = 0;
                nip->ip_p = INET_IPPROTO_ICMP;
                nip->ip_src = oip->ip_dst;
                nip->ip_dst = oip->ip_src;
                inetIcmpReflect(m);
            }
        }
    }

    sceNetIfhandle_lib_F56FAC82(n);
}

/* [297] 0x01A5F8 */
INET_NOINLINE void inetIcmpInput(InetMbuf *m, int hlen, int proto)
{
    InetIpHdr *ip = (InetIpHdr *)m->m_data;
    s32 icmplen = (s32)ip->ip_len - hlen;
    s32 pullLen;
    InetIcmpHdr *icp;
    int code;
    int deliverRaw = 1;
    int reflect = 0;

    if (icmplen < 8) {
        g_icmpstat.icps_tooshort++;
        sceNetIfhandle_lib_F56FAC82(m);
        return;
    }

    pullLen = (icmplen < 36) ? icmplen : 36;
    if (m->m_len < hlen + pullLen) {
        m = sceNetIfhandle_lib_49EDBB18(m, hlen + pullLen);
        if (m == NULL) {
            g_icmpstat.icps_tooshort++;
            return;
        }
        ip = (InetIpHdr *)m->m_data;
    }

    icp = (InetIcmpHdr *)(m->m_data + hlen);
    m->m_len -= hlen;
    m->m_data += hlen;
    if (inetInCksum(m, icmplen) != 0) {
        g_icmpstat.icps_checksum++;
        sceNetIfhandle_lib_F56FAC82(m);
        return;
    }
    m->m_len += hlen;
    m->m_data -= hlen;

    if (icp->icmp_type < 19) {
        int prcCode = -1;

        g_icmpstat.icps_inhist[icp->icmp_type]++;
        code = icp->icmp_code;
        switch (icp->icmp_type) {
        case 3: /* UNREACH */
            switch (code) {
            case 0:
            case 1:
            case 2:
            case 3:
            case 5:
                prcCode = code + 8;
                break;
            case 4:
                prcCode = 5;
                break;
            case 6:
            case 9:
            case 11:
                prcCode = 8;
                break;
            case 7:
            case 8:
            case 10:
            case 12:
                prcCode = 9;
                break;
            default:
                g_icmpstat.icps_badcode++;
                break;
            }
            break;

        case 4: /* SOURCEQUENCH */
            if (code == 0) {
                prcCode = 4;
            } else {
                g_icmpstat.icps_badcode++;
            }
            break;

        case 5: /* REDIRECT */
            if (code >= 4) {
                g_icmpstat.icps_badcode++;
            } else if (icmplen < 36 ||
                       icmplen < (s32)((icp->icmp_dun.id_ip.ip_vhl & 0x0F) * 4 + 16) ||
                       (icp->icmp_dun.id_ip.ip_vhl & 0x0F) < 5) {
                g_icmpstat.icps_badlen++;
            } else {
                g_icmpgw.sin_addr = ip->ip_src;
                g_icmpsrc.sin_addr = icp->icmp_hun.ih_gwaddr;
                g_icmpdst.sin_addr = icp->icmp_dun.id_ip.ip_dst;
                inetRtRedirect((InetSockaddr *)&g_icmpdst, (InetSockaddr *)&g_icmpsrc,
                               NULL, 6, (InetSockaddr *)&g_icmpgw, NULL);
                inetPfCtlinput(15, (InetSockaddr *)&g_icmpdst);
            }
            break;

        case 8: /* ECHO */
            icp->icmp_type = 0;
            reflect = 1;
            break;

        case 11: /* TIMXCEED */
            if (code < 2) {
                prcCode = code + 18;
            } else {
                g_icmpstat.icps_badcode++;
            }
            break;

        case 12: /* PARAMPROB */
            if (code < 2) {
                prcCode = 20;
            } else {
                g_icmpstat.icps_badcode++;
            }
            break;

        case 13: /* TSTAMP */
            if (icmplen < 20) {
                g_icmpstat.icps_badlen++;
            } else {
                u32 ts = inetIptime();
                icp->icmp_type = 14;
                icp->icmp_dun.id_ts.its_rtime = ts;
                icp->icmp_dun.id_ts.its_ttime = ts;
                reflect = 1;
            }
            break;

        case 17: /* MASKREQ */
            if (g_icmp_ctl.maskrepl != 0) {
                if (icmplen < 12) {
                    g_icmpstat.icps_badlen++;
                } else {
                    InetInIfaddr *ia;
                    if (ip->ip_dst == inetBswap32(0xFFFFFFFFU) || ip->ip_dst == 0) {
                        g_icmpsrc.sin_addr = ip->ip_src;
                    } else {
                        g_icmpsrc.sin_addr = ip->ip_dst;
                    }
                    ia = (InetInIfaddr *)inetIfaOfIfpForAddr((InetSockaddr *)&g_icmpsrc, M_RCVIF(m));
                    if (ia != NULL) {
                        icp->icmp_type = 18;
                        icp->icmp_dun.id_mask = ia->ia_sockmask.sin_addr;
                        if (ip->ip_src == 0) {
                            if ((ia->ia_ifa.ifa_ifp->if_flags & (INET_IFF_BROADCAST | INET_IFF_POINTOPOINT)) != 0) {
                                ip->ip_src = ia->ia_broadaddr.sin_addr;
                            }
                        }
                        reflect = 1;
                    }
                }
            }
            break;

        default:
            break;
        }

        if (prcCode >= 0) {
            u32 innerHlen = (u32)(icp->icmp_dun.id_ip.ip_vhl & 0x0F);
            if (icmplen < 36 || icmplen < (s32)(innerHlen * 4 + 16) || innerHlen < 5) {
                g_icmpstat.icps_badlen++;
                sceNetIfhandle_lib_F56FAC82(m);
                return;
            }
            if ((icp->icmp_dun.id_ip.ip_dst & inetBswap32(0xF0000000U)) == inetBswap32(0xE0000000U)) {
                g_icmpstat.icps_badcode++;
            } else {
                icp->icmp_dun.id_ip.ip_len = inetBswap16(icp->icmp_dun.id_ip.ip_len);
                g_icmpdst.sin_addr = icp->icmp_dun.id_ip.ip_dst;
                {
                    InetProtosw *pr = &g_inetsw[g_ip_protox[icp->icmp_dun.id_ip.ip_p]];
                    if (pr->pr_ctlinput != NULL) {
                        pr->pr_ctlinput(prcCode, (InetSockaddr *)&g_icmpdst, &icp->icmp_dun.id_ip);
                    }
                }
            }
        }

        if (reflect) {
            g_icmpstat.icps_reflect++;
            g_icmpstat.icps_outhist[icp->icmp_type]++;
            inetIcmpReflect(m);
            deliverRaw = 0;
        }
    }

    if (deliverRaw) {
        inetRipInput(m, hlen, proto);
    }
}

/* [298] 0x01AB9C */
static INET_NOINLINE void inetIcmpReflect(InetMbuf *m)
{
    InetIpHdr *ip = (InetIpHdr *)m->m_data;
    s32 optlen = (s32)(ip->ip_vhl & 0x0F) * 4 - (s32)sizeof(InetIpHdr);
    u32 origDst;
    InetSockaddrIn *srcSin = NULL;
    InetMbuf *opts = NULL;

    if (!inetInCanForward(ip->ip_src) &&
        (ip->ip_src & inetBswap32(0xFF000000U)) != inetBswap32(0x7F000000U)) {
        sceNetIfhandle_lib_F56FAC82(m);
        return;
    }

    origDst = ip->ip_dst;
    ip->ip_dst = ip->ip_src;

    {
        InetInIfaddr *ia = g_in_ifaddr_hashtbl[origDst % 509U];
        while (ia != NULL) {
            if (ia->ia_addr.sin_addr == origDst &&
                (ia->ia_ifa.ifa_ifp->if_flags & INET_IFF_UP) != 0) {
                srcSin = &ia->ia_addr;
                break;
            }
            ia = ia->ia_hash_next;
        }
        if (srcSin == NULL) {
            InetIfnet *rcvif = M_RCVIF(m);
            if ((rcvif->if_flags & INET_IFF_BROADCAST) != 0) {
                InetIfaddr *ifa = rcvif->if_addrhead;
                while (ifa != NULL) {
                    if (ifa->ifa_addr->sa_family == INET_AF_INET) {
                        InetInIfaddr *inIfa = (InetInIfaddr *)ifa;
                        if (origDst == inIfa->ia_broadaddr.sin_addr) {
                            srcSin = &inIfa->ia_addr;
                            break;
                        }
                    }
                    ifa = ifa->ifa_next;
                }
            }
        }
    }

    g_icmpsrc.sin_addr = origDst;
    if (srcSin == NULL) {
        InetSockaddrIn dstSin;
        InetRoute ro;
        int selErr = 0;

        sceNet_lib_8B615F52(&dstSin, sizeof(dstSin));
        dstSin.sin_len = sizeof(InetSockaddrIn);
        dstSin.sin_family = INET_AF_INET;
        dstSin.sin_addr = ip->ip_dst;
        sceNet_lib_8B615F52(&ro, sizeof(ro));
        srcSin = inetInSelectSrc(&dstSin, &ro, 0, NULL, &selErr);
        if (ro.ro_rt != NULL) {
            if (ro.ro_rt->rt_refcnt >= 2) {
                ro.ro_rt->rt_refcnt--;
            } else {
                inetRtFree(ro.ro_rt);
            }
        }
        if (srcSin != NULL) {
            u32 selAddr = srcSin->sin_addr;
            InetInIfaddr *ia = g_in_ifaddr_hashtbl[selAddr % 509U];
            srcSin = NULL;
            while (ia != NULL) {
                if (ia->ia_addr.sin_addr == selAddr &&
                    (ia->ia_ifa.ifa_ifp->if_flags & INET_IFF_UP) != 0) {
                    break;
                }
                ia = ia->ia_hash_next;
            }
            while (ia != NULL) {
                if (ia->ia_addr.sin_addr == selAddr && ia->ia_ifa.ifa_ifp == M_RCVIF(m)) {
                    srcSin = &ia->ia_addr;
                    break;
                }
                ia = ia->ia_hash_next;
            }
        }
        if (srcSin == NULL) {
            InetInIfaddr *inIfa = findFirstInetIfa(M_RCVIF(m));
            if (inIfa != NULL) {
                srcSin = &inIfa->ia_addr;
            }
        }
        if (srcSin == NULL) {
            InetInIfaddr *ia = g_in_ifaddr_head;
            while (ia != NULL) {
                if ((ia->ia_ifa.ifa_ifp->if_flags & INET_IFF_LOOPBACK) == 0) {
                    srcSin = &ia->ia_addr;
                    break;
                }
                ia = ia->ia_next;
            }
        }
        if (srcSin == NULL) {
            sceNetIfhandle_lib_F56FAC82(m);
            return;
        }
    }

    ip->ip_src = srcSin->sin_addr;
    ip->ip_ttl = 255;

    if (optlen > 0) {
        u8 *cp = m->m_data + sizeof(InetIpHdr);
        opts = inetIpSrcroute();
        if (opts == NULL) {
            opts = sceNetIfhandle_lib_4CF15C43(INET_M_DONTWAIT, INET_MT_HEADER);
            if (opts != NULL) {
                opts->m_len = 4;
                *(u32 *)opts->m_data = g_zeroin_addr;
            }
        }
        if (opts != NULL) {
            s32 rem = optlen;
            while (rem > 0) {
                u8 opt = cp[0];
                s32 cnt = 1;
                if (opt == 0) {
                    break;
                }
                if (opt != 1) {
                    if (rem < 2) {
                        break;
                    }
                    cnt = cp[1];
                    if (cnt < 2 || cnt > rem) {
                        break;
                    }
                }
                if (opt == 7 || opt == 68 || opt == 130) {
                    sceNet_lib_2F305274(cp, opts->m_data + opts->m_len, cnt);
                    opts->m_len += cnt;
                }
                rem -= cnt;
                cp += cnt;
            }
            while ((opts->m_len % 4) != 0) {
                opts->m_data[opts->m_len++] = 0;
            }
        }
        ip->ip_vhl = (u8)((ip->ip_vhl & 0xF0) | 5);
        ip->ip_len = (u16)(ip->ip_len - optlen);
        m->m_len -= optlen;
        if ((m->m_flags & INET_M_PKTHDR) != 0) {
            M_PKTLEN(m) -= optlen;
        }
        sceNet_lib_2F305274(m->m_data + sizeof(InetIpHdr) + optlen,
                            m->m_data + sizeof(InetIpHdr),
                            m->m_len - (s32)sizeof(InetIpHdr));
    }

    m->m_flags &= (u16)~(INET_M_BCAST | INET_M_MCAST);
    inetIcmpSend(m, opts);
    if (opts != NULL) {
        sceNetIfhandle_lib_F8825DC4(opts);
    }
}

/* [299] 0x01B120 */
static INET_NOINLINE int inetIcmpSend(InetMbuf *m, InetMbuf *opts)
{
    InetIpHdr *ip = (InetIpHdr *)m->m_data;
    s32 hlen = (s32)(ip->ip_vhl & 0x0F) * 4;
    InetIcmpHdr *icp = (InetIcmpHdr *)(m->m_data + hlen);

    m->m_data += hlen;
    m->m_len -= hlen;
    icp->icmp_cksum = 0;
    icp->icmp_cksum = inetInCksum(m, (s32)ip->ip_len - hlen);
    m->m_data -= hlen;
    m->m_len += hlen;

    return inetIpOutput(m, opts, NULL, 0, NULL, NULL);
}

/* [300] 0x01B1D0 */
INET_NOINLINE u32 inetIptime(void)
{
    InetTimeval tv;
    sceNet_lib_4B2B3416(&tv);
    return inetBswap32((tv.tv_sec % 86400U) * 1000U + tv.tv_usec / 1000U);
}

/* [301] 0x01B244 */
INET_NOINLINE int inetIcmpSysctl(int *name, u32 namelen, void *oldp, u32 *oldlenp, void *newp, u32 newlen)
{
    if (namelen != 1) {
        return INET_ENOTDIR;
    }
    switch (name[0]) {
    case 1:
        return inetSysctlInt(oldp, oldlenp, newp, newlen, &g_icmp_ctl.maskrepl);
    case 3: {
        int val = (int)g_icmp_returndatabytes;
        int err = inetSysctlInt(oldp, oldlenp, newp, newlen, &val);
        if (err == 0) {
            g_icmp_returndatabytes = (u32)val;
        }
        return err;
    }
    case 4:
        return inetSysctlInt(oldp, oldlenp, newp, newlen, &g_icmp_errppslim);
    case 5:
        return inetSysctlRdStruct(oldp, oldlenp, newp, &g_icmpstat, sizeof(g_icmpstat));
    default:
        return INET_ENOPROTOOPT;
    }
}

/* [302] 0x01B33C */
INET_NOINLINE void inetIcmpMtudisc(InetIcmpHdr *icp, u32 faddr)
{
    u32 mtu = inetBswap16(icp->icmp_hun.ih_pmtu.ipm_nextmtu);
    InetRtentry *rt = inetRtAlloc1((InetSockaddr *)&g_icmpdst, 1);

    if (rt == NULL) {
        return;
    }

    if ((rt->rt_flags & INET_RTF_HOST) == 0) {
        InetRtentry *cloneRt = NULL;
        if (inetRtRequest(INET_RTM_ADD, (InetSockaddr *)&g_icmpdst, rt->rt_gateway, NULL,
                          INET_RTF_HOST | INET_RTF_GATEWAY | INET_RTF_DYNAMIC, &cloneRt) != 0) {
            inetRtFree(rt);
            inetRtFree(cloneRt);
            return;
        }
        cloneRt->rt_rmx = rt->rt_rmx;
        inetRtFree(rt);
        rt = cloneRt;
    }

    if (inetRtTimerAdd(rt, inetIcmpMtudiscTimeout, g_ip_mtudisc_timeout_q) != 0) {
        inetRtFree(rt);
        return;
    }

    if (mtu == 0) {
        int idx;
        mtu = icp->icmp_dun.id_ip.ip_len;
        if (rt->rt_rmx.rmx_mtu != 0 && rt->rt_rmx.rmx_mtu < mtu) {
            mtu -= (u32)(icp->icmp_dun.id_ip.ip_vhl & 0x0F) * 4;
        }
        if (mtu == 0) {
            mtu = (rt->rt_rmx.rmx_mtu != 0) ? rt->rt_rmx.rmx_mtu : (u32)rt->rt_ifp->if_data.ifi_mtu;
        }
        for (idx = 0; idx < 14; idx++) {
            if (g_mtudisc_plateaus[idx] < mtu) {
                mtu = g_mtudisc_plateaus[idx];
                break;
            }
        }
    }

    if ((rt->rt_rmx.rmx_locks & 1) == 0) {
        if (mtu < 296) {
            rt->rt_rmx.rmx_locks |= 1;
        } else if (rt->rt_ifp->if_data.ifi_mtu < (u64)mtu) {
            rt->rt_rmx.rmx_locks |= 1;
        } else if (rt->rt_rmx.rmx_mtu == 0 || mtu < rt->rt_rmx.rmx_mtu) {
            rt->rt_rmx.rmx_mtu = mtu;
        }
    }

    inetRtFree(rt);

    {
        InetMtudiscCallback *mc = g_icmp_ctl.mtudisc_callbacks;
        while (mc != NULL) {
            mc->mc_func(faddr);
            mc = mc->mc_next;
        }
    }
}

/* [303] 0x01B588 */
static INET_NOINLINE int inetIcmpRatecheck(u32 *srcAddr, int type, int code)
{
    (void)srcAddr;
    (void)type;
    (void)code;
    return 0;
}

/* [304] 0x01B590 */
INET_NOINLINE void inetIcmpMtudiscCallbackRegister(void (*func)(u32))
{
    InetMtudiscCallback *mc = g_icmp_ctl.mtudisc_callbacks;
    while (mc != NULL) {
        if (mc->mc_func == func) {
            return;
        }
        mc = mc->mc_next;
    }
    mc = (InetMtudiscCallback *)sceNetIfhandle_lib_15CFE3C0(sizeof(InetMtudiscCallback));
    if (mc != NULL) {
        mc->mc_func = func;
        mc->mc_next = g_icmp_ctl.mtudisc_callbacks;
        if (g_icmp_ctl.mtudisc_callbacks != NULL) {
            g_icmp_ctl.mtudisc_callbacks->mc_prev = &mc->mc_next;
        }
        mc->mc_prev = &g_icmp_ctl.mtudisc_callbacks;
        g_icmp_ctl.mtudisc_callbacks = mc;
    }
}

/* [305] 0x01B614 */
INET_NOINLINE void inetIcmpInit(void)
{
    InetSockaddrIn *dst = &g_icmpdst;
    InetSockaddrIn *src = &g_icmpsrc;
    InetSockaddrIn *gw = &g_icmpgw;

    g_icmp_returndatabytes = 8;
    g_icmp_ctl.maskrepl = 0;
    sceKernelMemset(&g_icmpstat, 0, sizeof(g_icmpstat));
    sceKernelMemset(dst, 0, sizeof(InetSockaddrIn));
    sceKernelMemset(src, 0, sizeof(InetSockaddrIn));
    sceKernelMemset(gw, 0, sizeof(InetSockaddrIn));
    dst->sin_len = sizeof(InetSockaddrIn);
    dst->sin_family = INET_AF_INET;
    gw->sin_len = sizeof(InetSockaddrIn);
    src->sin_len = sizeof(InetSockaddrIn);
    gw->sin_family = INET_AF_INET;
    src->sin_family = INET_AF_INET;
    g_icmp_ctl.mtudisc_callbacks = NULL;
}

/* [306] 0x01B6F0 */
static INET_NOINLINE void inetIcmpMtudiscTimeout(InetRtentry *rt, InetRttimer *timer)
{
    (void)timer;
    if ((rt->rt_flags & (INET_RTF_DYNAMIC | INET_RTF_HOST)) == (INET_RTF_DYNAMIC | INET_RTF_HOST)) {
        inetRtRequest(INET_RTM_DELETE, (InetSockaddr *)rt->rt_nodes[0].rn_u.rn_leaf.rn_key,
                      rt->rt_gateway, (InetSockaddr *)rt->rt_nodes[0].rn_u.rn_leaf.rn_mask,
                      rt->rt_flags, NULL);
    } else if ((rt->rt_rmx.rmx_locks & 1) == 0) {
        rt->rt_rmx.rmx_mtu = 0;
    }
}

/* [307] 0x01B74C */
INET_NOINLINE void inetIgmpInput(InetMbuf *m, int hlen, int proto)
{
    InetIpHdr *ip = (InetIpHdr *)m->m_data;
    InetIfnet *ifp = M_RCVIF(m);
    InetIgmpHdr *igmp;
    s32 minlen = hlen + (s32)sizeof(InetIgmpHdr);

    g_igmpstat.igps_rcv_total++;

    if ((s32)ip->ip_len < minlen) {
        g_igmpstat.igps_rcv_tooshort++;
        sceNetIfhandle_lib_F56FAC82(m);
        return;
    }

    if (((m->m_flags & INET_M_EXT) != 0 && (ip->ip_src & inetBswap32(0xFF000000U)) == 0) ||
        m->m_len < minlen) {
        m = sceNetIfhandle_lib_49EDBB18(m, minlen);
        if (m == NULL) {
            g_igmpstat.igps_rcv_tooshort++;
            return;
        }
        ip = (InetIpHdr *)m->m_data;
    }

    igmp = (InetIgmpHdr *)(m->m_data + hlen);
    m->m_data += hlen;
    m->m_len -= hlen;
    if (inetInCksum(m, (s32)ip->ip_len - hlen) != 0) {
        g_igmpstat.igps_rcv_badsum++;
        sceNetIfhandle_lib_F56FAC82(m);
        return;
    }
    m->m_data -= hlen;
    m->m_len += hlen;

    switch (igmp->igmp_type) {
    case 0x11: /* IGMP_HOST_MEMBERSHIP_QUERY */
        g_igmpstat.igps_rcv_queries++;
        if ((ifp->if_flags & INET_IFF_LOOPBACK) != 0) {
            break;
        }
        if (igmp->igmp_code == 0) {
            InetIgmpRouterInfo *rti = igmpFindOrCreateRti(ifp);
            if (rti == NULL) {
                sceNetIfhandle_lib_F56FAC82(m);
                return;
            }
            rti->rti_type = 1;
            rti->rti_time = 0;
            if (ip->ip_dst != inetBswap32(0xE0000001U)) {
                g_igmpstat.igps_rcv_badqueries++;
                sceNetIfhandle_lib_F56FAC82(m);
                return;
            }
            {
                InetInIfaddr *ia = g_in_ifaddr_head;
                while (ia != NULL) {
                    InetInMulti *inm = ia->ia_multiaddrs;
                    while (inm != NULL) {
                        if (sceNet_lib_94DCA9F0(inm->inm_ifname, ifp->if_xname, 16) == 0 &&
                            inm->inm_timer == 0 &&
                            (inm->inm_addr & inetBswap32(0xFFFFFF00U)) != inetBswap32(0xE0000000U)) {
                            inm->inm_state = 1;
                            inm->inm_timer = (sceNet_lib_1858883D() % 50U) + 1;
                            g_igmp_timers_are_running = 1;
                        }
                        inm = inm->inm_next;
                    }
                    ia = ia->ia_next;
                }
            }
        } else {
            u32 timer;
            if ((ip->ip_dst & inetBswap32(0xF0000000U)) != inetBswap32(0xE0000000U)) {
                g_igmpstat.igps_rcv_badqueries++;
                sceNetIfhandle_lib_F56FAC82(m);
                return;
            }
            timer = (u32)igmp->igmp_code >> 1;
            if (timer == 0) {
                timer = 1;
            }
            {
                InetInIfaddr *ia = g_in_ifaddr_head;
                while (ia != NULL) {
                    InetInMulti *inm = ia->ia_multiaddrs;
                    while (inm != NULL) {
                        if (sceNet_lib_94DCA9F0(inm->inm_ifname, ifp->if_xname, 16) == 0 &&
                            (inm->inm_addr & inetBswap32(0xFFFFFF00U)) != inetBswap32(0xE0000000U) &&
                            (ip->ip_dst == inetBswap32(0xE0000001U) || ip->ip_dst == inm->inm_addr)) {
                            switch (inm->inm_state) {
                            case 1:
                                if (timer < inm->inm_timer) {
                                    inm->inm_state = 1;
                                    inm->inm_timer = (sceNet_lib_1858883D() % timer) + 1;
                                    g_igmp_timers_are_running = 1;
                                }
                                break;
                            case 2:
                            case 3:
                            case 5:
                                inm->inm_state = 1;
                                inm->inm_timer = (sceNet_lib_1858883D() % timer) + 1;
                                g_igmp_timers_are_running = 1;
                                break;
                            case 4:
                                inm->inm_state = 5;
                                break;
                            default:
                                break;
                            }
                        }
                        inm = inm->inm_next;
                    }
                    ia = ia->ia_next;
                }
            }
        }
        break;

    case 0x12: /* IGMP_v1_HOST_MEMBERSHIP_REPORT */
    case 0x16: /* IGMP_v2_HOST_MEMBERSHIP_REPORT */
        g_igmpstat.igps_rcv_reports++;
        if ((ifp->if_flags & INET_IFF_LOOPBACK) != 0) {
            break;
        }
        if ((igmp->igmp_group & inetBswap32(0xF0000000U)) != inetBswap32(0xE0000000U) ||
            igmp->igmp_group != ip->ip_dst) {
            g_igmpstat.igps_rcv_badreports++;
            sceNetIfhandle_lib_F56FAC82(m);
            return;
        }
        {
            InetInIfaddr *inIfa = findFirstInetIfa(ifp);
            if ((ip->ip_src & inetBswap32(0xFF000000U)) == 0 && inIfa != NULL) {
                ip->ip_src = inIfa->ia_subnet;
            }
            if (inIfa != NULL) {
                InetInMulti *inm = inIfa->ia_multiaddrs;
                while (inm != NULL) {
                    if (inm->inm_addr == igmp->igmp_group) {
                        g_igmpstat.igps_rcv_ourreports++;
                        inm->inm_timer = 0;
                        if (igmp->igmp_type == 0x16) {
                            if (inm->inm_state == 1 || inm->inm_state == 2 || inm->inm_state == 5) {
                                inm->inm_state = 3;
                            }
                        } else {
                            if (inm->inm_state == 1) {
                                InetIgmpRouterInfo *rti = (InetIgmpRouterInfo *)inm->inm_rti;
                                inm->inm_state = (rti != NULL && rti->rti_type == 1) ? 3 : 4;
                            } else if (inm->inm_state >= 2 && inm->inm_state < 6) {
                                inm->inm_state = 4;
                            }
                        }
                        break;
                    }
                    inm = inm->inm_next;
                }
            }
        }
        break;

    default:
        break;
    }

    inetRipInput(m, hlen, proto);
}

/* [308] 0x01C0D0 */
INET_NOINLINE void inetIgmpFasttimo(void)
{
    int spl;
    InetInIfaddr *ia;

    if (g_igmp_timers_are_running == 0) {
        return;
    }

    spl = sceNet_lib_94B44F26();
    g_igmp_timers_are_running = 0;
    ia = g_in_ifaddr_head;
    while (ia != NULL) {
        InetInMulti *inm = ia->ia_multiaddrs;
        while (inm != NULL) {
            if (inm->inm_timer != 0) {
                inm->inm_timer--;
                if (inm->inm_timer == 0) {
                    if (inm->inm_state == 1) {
                        InetIgmpRouterInfo *rti = (InetIgmpRouterInfo *)inm->inm_rti;
                        u8 reportType = (rti != NULL && rti->rti_type == 1) ? 0x12 : 0x16;
                        inetIgmpSendpkt(inm, reportType);
                        inm->inm_state = 2;
                    }
                } else {
                    g_igmp_timers_are_running = 1;
                }
            }
            inm = inm->inm_next;
        }
        ia = ia->ia_next;
    }
    sceNet_lib_515B2F33(spl);
}

/* [309] 0x01C21C */
static INET_NOINLINE void inetIgmpSendpkt(InetInMulti *inm, u8 type)
{
    InetIfnet *ifp = sceNet_lib_1D10419C(inm->inm_ifname);
    InetMbuf *m;
    u32 maxLinkHdr = 0;
    InetIpHdr *ip;
    InetIgmpHdr *igmp;
    InetIpMoptions imo;

    if (ifp == NULL) {
        return;
    }
    m = (InetMbuf *)sceNetIfhandle_lib_15CFE3C0(sizeof(InetMbuf));
    if (m == NULL) {
        return;
    }
    m->m_next = NULL;
    m->m_nextpkt = NULL;
    m->m_data = M_PKTDAT(m);
    m->m_type = INET_MT_HEADER;
    m->m_flags = INET_M_PKTHDR;
    M_CSUM(m) = 0;

    sceNetIfhandle_lib_5FB31C72(&maxLinkHdr, NULL, NULL, NULL);
    m->m_data += maxLinkHdr;
    m->m_len = 28;
    M_PKTLEN(m) = 28;

    ip = (InetIpHdr *)m->m_data;
    ip->ip_tos = 0;
    ip->ip_len = 28;
    ip->ip_off = 0;
    ip->ip_p = INET_IPPROTO_IGMP;
    ip->ip_src = g_zeroin_addr;
    ip->ip_dst = inm->inm_addr;

    m->m_data += sizeof(InetIpHdr);
    m->m_len -= (s32)sizeof(InetIpHdr);
    igmp = (InetIgmpHdr *)m->m_data;
    igmp->igmp_type = type;
    igmp->igmp_code = 0;
    igmp->igmp_group = inm->inm_addr;
    igmp->igmp_cksum = 0;
    igmp->igmp_cksum = inetInCksum(m, sizeof(InetIgmpHdr));
    m->m_data -= sizeof(InetIpHdr);
    m->m_len += (s32)sizeof(InetIpHdr);

    imo.imo_multicast_ifp = ifp;
    imo.imo_multicast_ttl = 1;
    imo.imo_multicast_loop = 0;
    inetIpOutput(m, NULL, NULL, 0, &imo, NULL);
    g_igmpstat.igps_snd_reports++;
}

/* [310] 0x01C3C8 */
INET_NOINLINE void inetIgmpInit(void)
{
    sceKernelMemset(&g_igmpstat, 0, sizeof(g_igmpstat));
    g_igmp_timers_are_running = 0;
    g_igmp_rti_head = NULL;
}

/* [311] 0x01C400 */
INET_NOINLINE void inetIgmpJoingroup(InetInMulti *inm)
{
    int spl = sceNet_lib_94B44F26();
    InetIfnet *ifp = sceNet_lib_1D10419C(inm->inm_ifname);

    if (ifp != NULL) {
        inm->inm_state = 2;
        if ((inm->inm_addr & inetBswap32(0xFFFFFF00U)) == inetBswap32(0xE0000000U) ||
            (ifp->if_flags & INET_IFF_LOOPBACK) != 0) {
            inm->inm_timer = 0;
        } else {
            InetIfnet *ifpLookup = sceNet_lib_1D10419C(inm->inm_ifname);
            u8 reportType = 0x16;
            if (ifpLookup != NULL) {
                InetIgmpRouterInfo *rti = igmpFindOrCreateRti(ifpLookup);
                if (rti != NULL) {
                    inm->inm_rti = rti;
                    if (rti->rti_type == 1) {
                        reportType = 0x12;
                    }
                }
            }
            inetIgmpSendpkt(inm, reportType);
            inm->inm_state = 1;
            inm->inm_timer = (sceNet_lib_1858883D() % 50U) + 1;
            g_igmp_timers_are_running = 1;
        }
    }
    sceNet_lib_515B2F33(spl);
}

/* [312] 0x01C564 */
INET_NOINLINE void inetIgmpLeavegroup(InetInMulti *inm)
{
    InetIfnet *ifp = sceNet_lib_1D10419C(inm->inm_ifname);
    if (ifp != NULL && (inm->inm_state == 1 || inm->inm_state == 2)) {
        InetIgmpRouterInfo *rti = (InetIgmpRouterInfo *)inm->inm_rti;
        if ((inm->inm_addr & inetBswap32(0xFFFFFF00U)) != inetBswap32(0xE0000000U) &&
            (ifp->if_flags & INET_IFF_LOOPBACK) == 0 &&
            (rti == NULL || rti->rti_type != 1)) {
            inetIgmpSendpkt(inm, 0x17);
        }
    }
}

/* [313] 0x01C5F8 */
INET_NOINLINE void inetIgmpSlowtimo(void)
{
    int spl = sceNet_lib_94B44F26();
    InetIgmpRouterInfo *rti = g_igmp_rti_head;
    while (rti != NULL) {
        if (rti->rti_type == 1) {
            rti->rti_time++;
            if (rti->rti_time >= 540) {
                rti->rti_type = 2;
            }
        }
        rti = rti->rti_next;
    }
    sceNet_lib_515B2F33(spl);
}

/* [314] 0x01C664 */
INET_NOINLINE void inetRipInput(InetMbuf *m, int hlen, int proto)
{
    InetIpHdr *ip = (InetIpHdr *)m->m_data;
    InetSockaddrIn ripsrc;
    InetMbuf *opts = NULL;
    InetInpcbReal *inp;
    InetInpcbReal *last = NULL;

    (void)hlen;
    ripsrc.sin_len = sizeof(InetSockaddrIn);
    ripsrc.sin_family = INET_AF_INET;
    ripsrc.sin_port = 0;
    ripsrc.sin_addr = ip->ip_src;
    sceNet_lib_8B615F52(ripsrc.sin_zero, sizeof(ripsrc.sin_zero));

    ip->ip_len = (u16)(ip->ip_len - (u16)((ip->ip_vhl & 0x0F) * 4));

    for (inp = g_rawcb_table.inpt_queue_first;
         inp != (InetInpcbReal *)&g_rawcb_table;
         inp = inp->inp_queue_next) {
        if (inp->inp_ip.ip_p != 0 && inp->inp_ip.ip_p != (u8)proto) {
            continue;
        }
        if (inp->inp_laddr != 0 && inp->inp_laddr != ip->ip_dst) {
            continue;
        }
        if (inp->inp_faddr != 0 && inp->inp_faddr != ip->ip_src) {
            continue;
        }
        if (last != NULL) {
            InetMbuf *copyM = sceNetIfhandle_lib_456E3146(m, 0, 1000000000, INET_M_DONTWAIT);
            if (copyM != NULL) {
                InetSocket *so = last->inp_socket;
                if ((last->inp_flags & 0x87) != 0 || (so->so_options & 0x400) != 0) {
                    inetIpSaveControl(last, &opts, ip, copyM);
                }
                if (inetSbAppendAddr(&so->so_rcv, (InetSockaddr *)&ripsrc, copyM, opts) == 0) {
                    sceNetIfhandle_lib_F56FAC82(copyM);
                    if (opts != NULL) {
                        sceNetIfhandle_lib_F56FAC82(opts);
                    }
                } else {
                    if ((so->so_rcv.sb_flags & 0x3C) != 0) {
                        inetSoWakeup(so, &so->so_rcv);
                    }
                }
                opts = NULL;
            }
        }
        last = inp;
    }

    if (last != NULL) {
        InetSocket *so = last->inp_socket;
        if ((last->inp_flags & 0x87) != 0 || (so->so_options & 0x400) != 0) {
            inetIpSaveControl(last, &opts, ip, m);
        }
        if (inetSbAppendAddr(&so->so_rcv, (InetSockaddr *)&ripsrc, m, opts) == 0) {
            sceNetIfhandle_lib_F56FAC82(m);
            if (opts != NULL) {
                sceNetIfhandle_lib_F56FAC82(opts);
            }
        } else if ((so->so_rcv.sb_flags & 0x3C) != 0) {
            inetSoWakeup(so, &so->so_rcv);
        }
    } else {
        if (g_inetsw[g_ip_protox[ip->ip_p]].pr_input == (void *)inetRipInput) {
            inetIcmpError(m, 3, 2, 0, NULL);
            g_ipstat.ips_noproto++;
            g_ipstat.ips_delivered--;
        } else {
            sceNetIfhandle_lib_F56FAC82(m);
        }
    }
}

/* [315] 0x01C9D0 */
INET_NOINLINE int inetRipOutput(InetMbuf *m, InetInpcbReal *inp)
{
    InetSocket *so = inp->inp_socket;
    int flags = (so->so_options & INET_SO_DONTROUTE) | 0x24;
    InetIpHdr *ip;

    if ((inp->inp_flags & 8) == 0) {
        if ((u32)(M_PKTLEN(m) + (s32)sizeof(InetIpHdr)) > 0xFFFFU) {
            sceNetIfhandle_lib_F56FAC82(m);
            return INET_EMSGSIZE;
        }
        if ((m->m_flags & INET_M_EXT) == 0) {
            u32 lead = ((m->m_flags & INET_M_PKTHDR) != 0)
                           ? (u32)(m->m_data - M_PKTDAT(m))
                           : (u32)(m->m_data - M_DAT(m));
            if (lead >= sizeof(InetIpHdr)) {
                m->m_data -= sizeof(InetIpHdr);
                m->m_len += (s32)sizeof(InetIpHdr);
            } else {
                m = sceNetIfhandle_lib_C3325FDC(m, (s32)sizeof(InetIpHdr), INET_M_WAIT);
            }
        } else {
            m = sceNetIfhandle_lib_C3325FDC(m, (s32)sizeof(InetIpHdr), INET_M_WAIT);
        }
        if (m != NULL && (m->m_flags & INET_M_PKTHDR) != 0) {
            M_PKTLEN(m) += (s32)sizeof(InetIpHdr);
        }
        ip = (InetIpHdr *)m->m_data;
        ip->ip_tos = 0;
        ip->ip_off = 0;
        ip->ip_p = inp->inp_ip.ip_p;
        ip->ip_len = (u16)M_PKTLEN(m);
        ip->ip_src = inp->inp_laddr;
        ip->ip_dst = inp->inp_faddr;
        ip->ip_ttl = 255;
        return inetIpOutput(m, inp->inp_options, &inp->inp_route, flags,
                            inp->inp_moptions, &inp->inp_errormtu);
    }

    if ((u32)M_PKTLEN(m) > 0xFFFFU) {
        sceNetIfhandle_lib_F56FAC82(m);
        return INET_EMSGSIZE;
    }
    ip = (InetIpHdr *)m->m_data;
    if (M_PKTLEN(m) != (s32)ip->ip_len) {
        sceNetIfhandle_lib_F56FAC82(m);
        return INET_EINVAL;
    }
    if (ip->ip_id == 0) {
        ip->ip_id = inetBswap16(g_ip_id++);
    }
    g_ipstat.ips_rawout++;
    return inetIpOutput(m, NULL, &inp->inp_route, flags | 2,
                        inp->inp_moptions, &inp->inp_errormtu);
}

/* [316] 0x01CBFC */
INET_NOINLINE int inetRipCtloutput(int op, InetSocket *so, int level, int optname, InetMbuf **mp)
{
    InetInpcbReal *inp = (InetInpcbReal *)so->so_pcb;
    int err = 0;

    if (level != INET_IPPROTO_IP) {
        if (op == INET_PRCO_SETOPT && *mp != NULL) {
            sceNetIfhandle_lib_F8825DC4(*mp);
        }
        return INET_ENOPROTOOPT;
    }

    if (op == INET_PRCO_GETOPT) {
        if (optname == 2) { /* IP_HDRINCL */
            InetMbuf *m = sceNetIfhandle_lib_A493AA5F(INET_M_WAIT, 10);
            *mp = m;
            if (m == NULL) {
                return INET_ENOBUFS;
            }
            m->m_len = 4;
            *(s32 *)m->m_data = (inp->inp_flags & 8) != 0;
            return 0;
        }
        return inetIpCtloutput(op, so, level, optname, mp);
    }

    if (op == INET_PRCO_SETOPT) {
        if (optname != 2) {
            return inetIpCtloutput(op, so, level, optname, mp);
        }
        if (*mp != NULL && (*mp)->m_len >= 4) {
            if (*(s32 *)(*mp)->m_data != 0) {
                inp->inp_flags |= 8;
            } else {
                inp->inp_flags &= ~8U;
            }
        } else {
            err = INET_EINVAL;
        }
        if (*mp != NULL) {
            sceNetIfhandle_lib_F8825DC4(*mp);
        }
        return err;
    }

    return 0;
}

/* [317] 0x01CD18 */
INET_NOINLINE int inetRipUsrreq(InetSocket *so, int req, InetMbuf *m, InetMbuf *nam, InetMbuf *control)
{
    int err = 0;
    int spl;
    InetInpcbReal *inp;

    if (req == INET_PRU_CONTROL) {
        return inetInControl(so, (u32)m, nam, (InetIfnet *)control);
    }
    if (req == INET_PRU_PURGEIF) {
        inetInPurgeIf((InetIfnet *)control);
        inetInPcbPurgeIf(&g_rawcb_table, (InetIfnet *)control);
        return 0;
    }

    spl = sceNet_lib_94B44F26();
    inp = (InetInpcbReal *)so->so_pcb;
    if (inp == NULL && req != INET_PRU_ATTACH) {
        sceNet_lib_515B2F33(spl);
        return INET_EINVAL;
    }

    switch (req) {
    case INET_PRU_ATTACH:
        if (inp != NULL) {
            err = INET_EISCONN;
        } else if ((so->so_snd.sb_hiwat != 0 && so->so_rcv.sb_hiwat != 0) ||
                   (err = inetSoReserve(so, g_rip_sendspace, g_rip_recvspace)) == 0) {
            err = inetInPcbAlloc(so, &g_rawcb_table);
            if (err == 0) {
                ((InetInpcbReal *)so->so_pcb)->inp_ip.ip_p = (u8)(u32)nam;
            }
        }
        break;

    case INET_PRU_DETACH:
        inetInPcbDetach(inp);
        break;

    case INET_PRU_BIND: {
        InetSockaddrIn *sin = (InetSockaddrIn *)nam->m_data;
        InetIfnet *ifhead = NULL;
        sceNet_lib_C2EC2EEA(NULL, NULL, NULL, &ifhead);
        if (nam->m_len != sizeof(InetSockaddrIn)) {
            err = INET_EINVAL;
        } else if (ifhead == NULL) {
            err = INET_EADDRNOTAVAIL;
        } else if (sin->sin_family != INET_AF_INET && sin->sin_family != 3) {
            err = INET_EAFNOSUPPORT;
        } else if (sin->sin_addr != 0 && inetIfaWithAddr((InetSockaddr *)sin) == NULL) {
            err = INET_EADDRNOTAVAIL;
        } else {
            inp->inp_laddr = sin->sin_addr;
        }
        break;
    }

    case INET_PRU_LISTEN:
    case INET_PRU_RCVD:
    case INET_PRU_RCVOOB:
    case INET_PRU_PROTORCV:
        err = INET_EOPNOTSUPP;
        break;

    case INET_PRU_CONNECT: {
        InetSockaddrIn *sin = (InetSockaddrIn *)nam->m_data;
        InetIfnet *ifhead = NULL;
        sceNet_lib_C2EC2EEA(NULL, NULL, NULL, &ifhead);
        if (nam->m_len != sizeof(InetSockaddrIn)) {
            err = INET_EINVAL;
        } else if (ifhead == NULL) {
            err = INET_EADDRNOTAVAIL;
        } else if (sin->sin_family != INET_AF_INET && sin->sin_family != 3) {
            err = INET_EAFNOSUPPORT;
        } else {
            inp->inp_faddr = sin->sin_addr;
            inetSoIsConnected(so);
        }
        break;
    }

    case INET_PRU_DISCONNECT:
        inetSoIsAborted(so);
        inp->inp_faddr = g_zeroin_addr;
        break;

    case INET_PRU_SHUTDOWN:
        inetSoCantSendMore(so);
        break;

    case INET_PRU_SEND:
        if (control != NULL && control->m_len != 0) {
            sceNetIfhandle_lib_F56FAC82(control);
            sceNetIfhandle_lib_F56FAC82(m);
            err = INET_EINVAL;
        } else {
            int ok = 1;
            if (nam != NULL) {
                InetSockaddrIn *sin = (InetSockaddrIn *)nam->m_data;
                InetIfnet *ifhead = NULL;
                if ((so->so_state & INET_SS_ISCONNECTED) != 0) {
                    err = INET_EISCONN;
                    ok = 0;
                } else {
                    sceNet_lib_C2EC2EEA(NULL, NULL, NULL, &ifhead);
                    if (nam->m_len != sizeof(InetSockaddrIn)) {
                        err = INET_EINVAL;
                        ok = 0;
                    } else if (ifhead == NULL) {
                        err = INET_EADDRNOTAVAIL;
                        ok = 0;
                    } else if (sin->sin_family != INET_AF_INET && sin->sin_family != 3) {
                        err = INET_EAFNOSUPPORT;
                        ok = 0;
                    } else {
                        inp->inp_faddr = sin->sin_addr;
                    }
                }
            } else if ((so->so_state & INET_SS_ISCONNECTED) == 0) {
                err = INET_ENOTCONN;
                ok = 0;
            }
            if (!ok) {
                sceNetIfhandle_lib_F56FAC82(m);
            } else {
                err = inetRipOutput(m, inp);
                if (nam != NULL) {
                    inp->inp_faddr = g_zeroin_addr;
                }
            }
        }
        break;

    case INET_PRU_ABORT:
        inetSoIsAborted(so);
        inetInPcbDetach(inp);
        break;

    case INET_PRU_SENSE:
        sceNet_lib_515B2F33(spl);
        return 0;

    case INET_PRU_SENDOOB:
        sceNetIfhandle_lib_F56FAC82(control);
        sceNetIfhandle_lib_F56FAC82(m);
        err = INET_EOPNOTSUPP;
        break;

    case INET_PRU_SOCKADDR:
        inetInSetSockaddr(inp, nam);
        break;

    case INET_PRU_PEERADDR:
        inetInSetPeeraddr(inp, nam);
        break;

    default:
        break;
    }

    sceNet_lib_515B2F33(spl);
    return err;
}

/* [318] 0x01D128 */
INET_NOINLINE void inetRipInit(void)
{
    g_rip_sendspace = 8192;
    g_rip_recvspace = 8192;
    inetInPcbInit(&g_rawcb_table, 1, 1);
}
