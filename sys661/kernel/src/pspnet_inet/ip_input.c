#include "pspnet_inet_internal.h"

typedef struct InetIpqEnt {
    struct InetIpqEnt *ipqe_next;       /* 0x00 */
    struct InetIpqEnt **ipqe_prev;      /* 0x04 */
    InetIpHdr *ipqe_ip;                 /* 0x08 */
    InetMbuf *ipqe_m;                   /* 0x0C */
    u8 ipqe_mff;                        /* 0x10 */
    u8 ipqe_pad[3];
    u32 ipqe_spare[5];
} InetIpqEnt;
INET_STATIC_ASSERT(ipqent_size, sizeof(InetIpqEnt) == 40);

typedef struct InetIpq {
    struct InetIpq *ipq_next;           /* 0x00 */
    struct InetIpq **ipq_prev;          /* 0x04 */
    u8 ipq_ttl;                         /* 0x08 */
    u8 ipq_p;                           /* 0x09 */
    u16 ipq_id;                         /* 0x0A */
    InetIpqEnt *ipq_frags;              /* 0x0C */
    u32 ipq_src;                        /* 0x10 */
    u32 ipq_dst;                        /* 0x14 */
} InetIpq;
INET_STATIC_ASSERT(ipq_size, sizeof(InetIpq) == 24);

typedef struct InetIpSrcrt {
    u32 dst;                            /* 0x00 */
    u8 nop;                             /* 0x04 */
    u8 srcopt[3];                       /* 0x05..0x07 */
    u32 route[10];                      /* 0x08..0x2F */
} InetIpSrcrt;
INET_STATIC_ASSERT(ip_srcrt_size, sizeof(InetIpSrcrt) == 48);

s32 g_ipforwarding;
s32 g_ipsendredirects = 1;
s32 g_ip_defttl = 64;
s32 g_ip_forwsrcrt = 1;
s32 g_ip_directedbcast;
s32 g_ip_allowsrcrt = 1;
s32 g_ip_mtudisc = 1;
s32 g_ip_mtudisc_timeout = 600;
s32 g_ipqmaxlen = 50;
s32 g_ip_maxfragpackets = 200;

u8 g_ip_protox[256];
u16 g_ip_id;
InetIfQueue g_ipintrq;
InetRoute g_ipforward_rt;
InetIpStat g_ipstat;

static s32 g_ipq_locked;
static InetIpq *g_ipq_head;
static s32 g_ip_nfragpackets;
static s32 g_ip_nhops;
static InetIpSrcrt g_ip_srcrt;
InetRttimerQueue *g_ip_mtudisc_timeout_q;
static InetSockaddrIn g_ipaddr = { 16, INET_AF_INET, 0, 0, { 0 } };

static InetMbuf *inetIpReass(InetIpqEnt *ipqe, InetIpq *fp);
static void inetIpFreef(InetIpq *fp);
static int inetIpDooptions(InetMbuf *m);
static InetInIfaddr *inetIpRtaddr(u32 dstAddr);
static void inetIpSaveRt(const u8 *opt, u32 dstAddr);
static void inetIpForward(InetMbuf *m, int srcrt);

/* [270] 0x0160BC */
INET_NOINLINE void inetTcpSynCacheInit(void)
{
    g_tcp_syn_cache_size = 293;
    g_tcp_syn_cache_limit = 10255;
    g_tcp_syn_bucket_limit = 105;
    sceKernelMemset(g_tcp_syn_cache, 0, sizeof(g_tcp_syn_cache));
    g_tcp_syn_cache_interval = 1;
    g_icmp_errppslim = 100;
    g_tcp_rstppslim = 100;
}

/* [271] 0x016128 */
INET_NOINLINE void inetIpInit(void)
{
    InetProtosw *rawPr;
    InetProtosw *pr;
    u8 rawIdx;
    int idx;

    inetInInit();
    g_ip_defttl = 64;
    g_anonportmin = 49167;
    g_anonportmax = 65535;
    g_lowportmax = 1023;
    g_ipforwarding = 0;
    g_ip_forwsrcrt = 1;
    g_ip_directedbcast = 0;
    g_lowportmin = 600;
    g_ipsendredirects = 1;
    g_ip_allowsrcrt = 1;
    g_ip_mtudisc = 1;
    g_ip_mtudisc_timeout = 600;
    g_ip_nfragpackets = 0;
    g_ip_nhops = 0;
    g_ip_maxfragpackets = 200;
    g_ip_mtudisc_timeout_q = NULL;

    sceKernelMemset(&g_ipforward_rt, 0, sizeof(g_ipforward_rt));
    sceKernelMemset(&g_ipintrq, 0, sizeof(g_ipintrq));
    g_ipq_locked = 0;
    g_ipqmaxlen = 50;
    sceKernelMemset(&g_ipstat, 0, sizeof(g_ipstat));
    sceKernelMemset(&g_ip_srcrt, 0, sizeof(g_ip_srcrt));
    inetTcpSynCacheInit();

    rawPr = inetFindProto(INET_AF_INET, INET_IPPROTO_RAW, INET_SOCK_RAW);
    rawIdx = (u8)(rawPr - g_inetsw);
    for (idx = 0; idx < 256; idx++) {
        g_ip_protox[idx] = rawIdx;
    }

    for (pr = g_inetdomain.dom_protosw; pr < g_inetdomain.dom_protosw_end; pr++) {
        if (pr->pr_domain->dom_family == INET_AF_INET &&
            pr->pr_protocol != 0 && pr->pr_protocol != INET_IPPROTO_RAW) {
            g_ip_protox[(u8)pr->pr_protocol] = (u8)(pr - g_inetsw);
        }
    }

    g_ipq_head = NULL;
    g_ip_id = (u16)sceNet_lib_75D9985C();
    g_ipintrq.ifq_maxlen = g_ipqmaxlen;
    g_in_ifaddr_head = NULL;
    g_in_ifaddr_tail = &g_in_ifaddr_head;
    g_in_ifaddr_hashtbl = (InetInIfaddr **)sceNet_lib_C4261339(509, &g_in_ifaddr_hashmask);

    if (g_ip_mtudisc != 0) {
        g_ip_mtudisc_timeout_q = inetRtTimerQueueCreate(g_ip_mtudisc_timeout);
    }
}

/* [272] 0x0163A0 */
INET_NOINLINE void inetIpInput(InetMbuf *m)
{
    InetIpHdr *ip;
    s32 hlen;
    u16 ipLen;
    u16 ipOff;
    int downMatch = 0;
    int forUs = 0;

    if (g_in_ifaddr_head == NULL) {
        sceNetIfhandle_lib_F56FAC82(m);
        return;
    }

    g_ipstat.ips_total++;
    if (m->m_len < (s32)sizeof(InetIpHdr)) {
        m = sceNetIfhandle_lib_49EDBB18(m, sizeof(InetIpHdr));
        if (m == NULL) {
            g_ipstat.ips_toosmall++;
            return;
        }
    }

    ip = (InetIpHdr *)m->m_data;
    if ((ip->ip_vhl & 0xF0) != 0x40) {
        g_ipstat.ips_badvers++;
        sceNetIfhandle_lib_F56FAC82(m);
        return;
    }

    hlen = (s32)(ip->ip_vhl & 0x0F) * 4;
    if (hlen < (s32)sizeof(InetIpHdr)) {
        g_ipstat.ips_badhlen++;
        sceNetIfhandle_lib_F56FAC82(m);
        return;
    }
    if (m->m_len < hlen) {
        m = sceNetIfhandle_lib_49EDBB18(m, hlen);
        if (m == NULL) {
            g_ipstat.ips_badhlen++;
            return;
        }
        ip = (InetIpHdr *)m->m_data;
    }

    if ((ip->ip_src & inetBswap32(0xF0000000U)) == inetBswap32(0xE0000000U)) {
        sceNetIfhandle_lib_F56FAC82(m);
        return;
    }
    if (inetInCksum(m, hlen) != 0) {
        g_ipstat.ips_badsum++;
        sceNetIfhandle_lib_F56FAC82(m);
        return;
    }

    ipLen = inetBswap16(ip->ip_len);
    ipOff = inetBswap16(ip->ip_off);
    ip->ip_len = ipLen;
    ip->ip_off = ipOff;

    if ((s32)ipLen < hlen) {
        g_ipstat.ips_badlen++;
        sceNetIfhandle_lib_F56FAC82(m);
        return;
    }
    if (M_PKTLEN(m) < (s32)ipLen) {
        g_ipstat.ips_tooshort++;
        sceNetIfhandle_lib_F56FAC82(m);
        return;
    }
    if ((s32)ipLen < M_PKTLEN(m)) {
        if (m->m_len == M_PKTLEN(m)) {
            M_PKTLEN(m) = ipLen;
            m->m_len = ipLen;
        } else {
            sceNetIfhandle_lib_BFF3CEA5(m, (s32)ipLen - M_PKTLEN(m));
        }
    }

    m->m_flags |= 0x0400;
    g_ip_nhops = 0;
    if (hlen > (s32)sizeof(InetIpHdr) && inetIpDooptions(m) != 0) {
        return;
    }

    /* Check whether destination is one of our addresses */
    {
        u32 dst = ip->ip_dst;
        InetInIfaddr *ia = g_in_ifaddr_hashtbl[dst % 509U];
        while (ia != NULL) {
            if (ia->ia_addr.sin_addr == dst) {
                if ((ia->ia_ifa.ifa_ifp->if_flags & INET_IFF_UP) != 0) {
                    forUs = 1;
                    break;
                }
                downMatch++;
            }
            ia = ia->ia_hash_next;
        }

        if (!forUs) {
            InetIfnet *rcvif = M_RCVIF(m);
            if ((rcvif->if_flags & INET_IFF_BROADCAST) != 0) {
                InetIfaddr *ifa = rcvif->if_addrhead;
                while (ifa != NULL) {
                    if (ifa->ifa_addr->sa_family == INET_AF_INET) {
                        InetInIfaddr *bia = (InetInIfaddr *)ifa;
                        if (dst == bia->ia_dstaddr.sin_addr ||
                            dst == bia->ia_netbroadcast ||
                            dst == bia->ia_subnet ||
                            dst == bia->ia_net ||
                            bia->ia_addr.sin_addr == 0) {
                            forUs = 1;
                            break;
                        }
                    }
                    ifa = ifa->ifa_next;
                }
            }
        }

        if (!forUs) {
            if ((dst & inetBswap32(0xF0000000U)) == inetBswap32(0xE0000000U)) {
                InetIfaddr *ifa = M_RCVIF(m)->if_addrhead;
                while (ifa != NULL && ifa->ifa_addr->sa_family != INET_AF_INET) {
                    ifa = ifa->ifa_next;
                }
                if (ifa != NULL) {
                    InetInMulti *inm = ((InetInIfaddr *)ifa)->ia_multiaddrs;
                    while (inm != NULL) {
                        if (inm->inm_addr == dst) {
                            forUs = 1;
                            break;
                        }
                        inm = inm->inm_next;
                    }
                }
            } else if (dst == 0xFFFFFFFFU || dst == 0) {
                forUs = 1;
            } else if (g_ipforwarding != 0) {
                if (downMatch != 0) {
                    inetIcmpError(m, 3, 1, 0, NULL);
                    g_ipstat.ips_cantforward++;
                    return;
                }
                inetIpForward(m, 0);
                return;
            }
        }

        if (!forUs) {
            g_ipstat.ips_cantforward++;
            sceNetIfhandle_lib_F56FAC82(m);
            return;
        }
    }

    /* Reassemble fragmented IP packets */
    if ((ip->ip_off & 0x3FFF) != 0) {
        int spl;
        InetIpq *fp;
        u16 fragLen;
        u8 mff;

        spl = sceNet_lib_94B44F26();
        if (g_ipq_locked == 0) {
            g_ipq_locked = 1;
        }
        sceNet_lib_515B2F33(spl);

        fp = g_ipq_head;
        while (fp != NULL) {
            if (fp->ipq_id == ip->ip_id &&
                fp->ipq_src == ip->ip_src &&
                fp->ipq_dst == ip->ip_dst &&
                fp->ipq_p == ip->ip_p) {
                break;
            }
            fp = fp->ipq_next;
        }

        fragLen = (u16)(ip->ip_len - hlen);
        ip->ip_len = fragLen;
        mff = (u8)((ip->ip_off & 0x2000) != 0);
        if (mff != 0 && (fragLen == 0 || (fragLen & 7) != 0)) {
            g_ipstat.ips_badfrags++;
            spl = sceNet_lib_94B44F26();
            g_ipq_locked = 0;
            sceNet_lib_515B2F33(spl);
            sceNetIfhandle_lib_F56FAC82(m);
            return;
        }
        ip->ip_off = (u16)(ip->ip_off << 3);

        if (mff == 0 && ip->ip_off == 0) {
            if (fp != NULL) {
                inetIpFreef(fp);
            }
            spl = sceNet_lib_94B44F26();
            g_ipq_locked = 0;
            sceNet_lib_515B2F33(spl);
        } else {
            InetIpqEnt *ipqe;
            g_ipstat.ips_fragments++;
            ipqe = (InetIpqEnt *)sceNetIfhandle_lib_15CFE3C0(sizeof(InetIpqEnt));
            if (ipqe == NULL) {
                g_ipstat.ips_fragdropped_nomem++;
                spl = sceNet_lib_94B44F26();
                g_ipq_locked = 0;
                sceNet_lib_515B2F33(spl);
                sceNetIfhandle_lib_F56FAC82(m);
                return;
            }
            ipqe->ipqe_m = m;
            ipqe->ipqe_mff = mff;
            ipqe->ipqe_ip = ip;
            m = inetIpReass(ipqe, fp);
            if (m == NULL) {
                spl = sceNet_lib_94B44F26();
                g_ipq_locked = 0;
                sceNet_lib_515B2F33(spl);
                return;
            }
            ip = (InetIpHdr *)m->m_data;
            g_ipstat.ips_reassembled++;
            hlen = (s32)(ip->ip_vhl & 0x0F) * 4;
            ip->ip_len = (u16)(ip->ip_len + hlen);

            spl = sceNet_lib_94B44F26();
            g_ipq_locked = 0;
            sceNet_lib_515B2F33(spl);
        }
    }

    g_ipstat.ips_delivered++;
    g_inetsw[g_ip_protox[ip->ip_p]].pr_input(m, hlen, ip->ip_p);
}

/* [273] 0x016BFC */
static INET_NOINLINE InetMbuf *inetIpReass(InetIpqEnt *ipqe, InetIpq *fp)
{
    InetMbuf *m = ipqe->ipqe_m;
    s32 hlen = (s32)(ipqe->ipqe_ip->ip_vhl & 0x0F) * 4;
    InetIpqEnt *prev = NULL;
    InetIpqEnt *cur;

    m->m_data += hlen;
    m->m_len -= hlen;

    if (fp == NULL) {
        if ((g_ip_maxfragpackets >= 0 && g_ip_nfragpackets >= g_ip_maxfragpackets) ||
            (g_ip_nfragpackets++, fp = (InetIpq *)sceNetIfhandle_lib_15CFE3C0(sizeof(InetIpq)), fp == NULL)) {
            g_ipstat.ips_fragdropped++;
            sceNetIfhandle_lib_F56FAC82(m);
            sceNetIfhandle_lib_76BAD213(ipqe);
            return NULL;
        }
        fp->ipq_next = g_ipq_head;
        if (g_ipq_head != NULL) {
            g_ipq_head->ipq_prev = &fp->ipq_next;
        }
        fp->ipq_prev = &g_ipq_head;
        g_ipq_head = fp;

        fp->ipq_ttl = 60;
        fp->ipq_p = ipqe->ipqe_ip->ip_p;
        fp->ipq_id = ipqe->ipqe_ip->ip_id;
        fp->ipq_frags = NULL;
        fp->ipq_src = ipqe->ipqe_ip->ip_src;
        fp->ipq_dst = ipqe->ipqe_ip->ip_dst;

        ipqe->ipqe_next = fp->ipq_frags;
        ipqe->ipqe_prev = &fp->ipq_frags;
        fp->ipq_frags = ipqe;
    } else {
        cur = fp->ipq_frags;
        while (cur != NULL && ipqe->ipqe_ip->ip_off >= cur->ipqe_ip->ip_off) {
            prev = cur;
            cur = cur->ipqe_next;
        }

        if (prev != NULL) {
            s32 overlap = (s32)(prev->ipqe_ip->ip_off + prev->ipqe_ip->ip_len) - (s32)ipqe->ipqe_ip->ip_off;
            if (overlap > 0) {
                if (overlap >= (s32)ipqe->ipqe_ip->ip_len) {
                    g_ipstat.ips_fragdropped++;
                    sceNetIfhandle_lib_F56FAC82(m);
                    sceNetIfhandle_lib_76BAD213(ipqe);
                    return NULL;
                }
                sceNetIfhandle_lib_BFF3CEA5(ipqe->ipqe_m, overlap);
                ipqe->ipqe_ip->ip_off = (u16)(ipqe->ipqe_ip->ip_off + overlap);
                ipqe->ipqe_ip->ip_len = (u16)(ipqe->ipqe_ip->ip_len - overlap);
            }
        }

        while (cur != NULL) {
            s32 overlap = (s32)(ipqe->ipqe_ip->ip_off + ipqe->ipqe_ip->ip_len) - (s32)cur->ipqe_ip->ip_off;
            if (overlap <= 0) {
                break;
            }
            if (overlap < (s32)cur->ipqe_ip->ip_len) {
                cur->ipqe_ip->ip_len = (u16)(cur->ipqe_ip->ip_len - overlap);
                cur->ipqe_ip->ip_off = (u16)(cur->ipqe_ip->ip_off + overlap);
                sceNetIfhandle_lib_BFF3CEA5(cur->ipqe_m, overlap);
                break;
            }
            {
                InetIpqEnt *next = cur->ipqe_next;
                sceNetIfhandle_lib_F56FAC82(cur->ipqe_m);
                if (cur->ipqe_next != NULL) {
                    cur->ipqe_next->ipqe_prev = cur->ipqe_prev;
                }
                *cur->ipqe_prev = cur->ipqe_next;
                sceNetIfhandle_lib_76BAD213(cur);
                cur = next;
            }
        }

        if (prev == NULL) {
            ipqe->ipqe_next = fp->ipq_frags;
            if (fp->ipq_frags != NULL) {
                fp->ipq_frags->ipqe_prev = &ipqe->ipqe_next;
            }
            ipqe->ipqe_prev = &fp->ipq_frags;
            fp->ipq_frags = ipqe;
        } else {
            ipqe->ipqe_next = prev->ipqe_next;
            if (prev->ipqe_next != NULL) {
                prev->ipqe_next->ipqe_prev = &ipqe->ipqe_next;
            }
            ipqe->ipqe_prev = &prev->ipqe_next;
            prev->ipqe_next = ipqe;
        }
    }

    {
        s32 nextOff = 0;
        InetIpqEnt *last = NULL;
        for (cur = fp->ipq_frags; cur != NULL; cur = cur->ipqe_next) {
            last = cur;
            if (cur->ipqe_ip->ip_off != nextOff) {
                return NULL;
            }
            nextOff += cur->ipqe_ip->ip_len;
        }
        if (last == NULL || last->ipqe_mff != 0) {
            return NULL;
        }

        {
            InetIpqEnt *first = fp->ipq_frags;
            InetIpHdr *firstIp = first->ipqe_ip;
            s32 firstHlen = (s32)(firstIp->ip_vhl & 0x0F) * 4;
            InetMbuf *head;
            InetIpqEnt *rest;

            if (nextOff + firstHlen > 0xFFFF) {
                g_ipstat.ips_toolong++;
                inetIpFreef(fp);
                return NULL;
            }

            head = first->ipqe_m;
            {
                InetMbuf *tail = head->m_next;
                head->m_next = NULL;
                sceNetIfhandle_lib_3E8DD3F8(head, tail);
            }
            rest = first->ipqe_next;
            sceNetIfhandle_lib_76BAD213(first);

            while (rest != NULL) {
                InetMbuf *fragM = rest->ipqe_m;
                InetIpqEnt *next = rest->ipqe_next;
                sceNetIfhandle_lib_76BAD213(rest);
                sceNetIfhandle_lib_3E8DD3F8(head, fragM);
                rest = next;
            }

            firstIp->ip_len = (u16)nextOff;
            firstIp->ip_src = fp->ipq_src;
            firstIp->ip_dst = fp->ipq_dst;

            if (fp->ipq_next != NULL) {
                fp->ipq_next->ipq_prev = fp->ipq_prev;
            }
            *fp->ipq_prev = fp->ipq_next;
            sceNetIfhandle_lib_76BAD213(fp);
            g_ip_nfragpackets--;

            head->m_len += firstHlen;
            head->m_data -= firstHlen;
            if ((head->m_flags & INET_M_PKTHDR) != 0) {
                s32 total = 0;
                InetMbuf *scan = head;
                while (scan != NULL) {
                    total += scan->m_len;
                    scan = scan->m_next;
                }
                M_PKTLEN(head) = total;
            }
            return head;
        }
    }
}

/* [274] 0x017188 */
static INET_NOINLINE void inetIpFreef(InetIpq *fp)
{
    InetIpqEnt *cur = fp->ipq_frags;
    while (cur != NULL) {
        InetIpqEnt *next = cur->ipqe_next;
        sceNetIfhandle_lib_F56FAC82(cur->ipqe_m);
        if (cur->ipqe_next != NULL) {
            cur->ipqe_next->ipqe_prev = cur->ipqe_prev;
        }
        *cur->ipqe_prev = cur->ipqe_next;
        sceNetIfhandle_lib_76BAD213(cur);
        cur = next;
    }
    if (fp->ipq_next != NULL) {
        fp->ipq_next->ipq_prev = fp->ipq_prev;
    }
    *fp->ipq_prev = fp->ipq_next;
    sceNetIfhandle_lib_76BAD213(fp);
    g_ip_nfragpackets--;
}

/* [275] 0x01722C */
static INET_NOINLINE int inetIpDooptions(InetMbuf *m)
{
    InetIpHdr *ip = (InetIpHdr *)m->m_data;
    u8 *base = (u8 *)ip;
    u8 *cp = base + sizeof(InetIpHdr);
    s32 cnt = (s32)(ip->ip_vhl & 0x0F) * 4 - (s32)sizeof(InetIpHdr);
    u32 origDst = ip->ip_dst;
    int forward = 0;
    int icmpType = 12;
    int icmpCode = 0;
    int badOpt = 0;

    while (cnt > 0) {
        u8 opt = cp[0];
        s32 optLen = 1;

        if (opt == 0) {
            break;
        }
        if (opt != 1) {
            if (cnt < 2) {
                icmpCode = (int)(cp - base) + 1;
                badOpt = 1;
                break;
            }
            optLen = cp[1];
            if (optLen < 2 || optLen > cnt) {
                icmpCode = (int)(cp - base) + 1;
                badOpt = 1;
                break;
            }
        }

        if (opt == 131 || opt == 137) { /* LSRR / SSRR */
            if (g_ip_allowsrcrt == 0) {
                icmpType = 3;
                icmpCode = 9;
                badOpt = 1;
                break;
            }
            if (optLen < 3) {
                icmpCode = (int)(cp - base) + 1;
                badOpt = 1;
                break;
            }
            {
                u32 off = cp[2];
                if (off < 4) {
                    icmpCode = (int)(cp - base) + 2;
                    badOpt = 1;
                    break;
                }
                g_ipaddr.sin_addr = ip->ip_dst;
                if (inetIfaWithAddr((InetSockaddr *)&g_ipaddr) == NULL) {
                    if (opt == 137) {
                        icmpType = 3;
                        icmpCode = 5;
                        badOpt = 1;
                        break;
                    }
                } else {
                    off--;
                    if ((s32)(off + 4) > optLen) {
                        inetIpSaveRt(cp, ip->ip_src);
                    } else {
                        InetInIfaddr *ia;
                        sceNet_lib_2F305274(cp + off, &g_ipaddr.sin_addr, 4);
                        if (opt == 137) {
                            ia = (InetInIfaddr *)inetIfaWithAddr((InetSockaddr *)&g_ipaddr);
                        } else {
                            ia = inetIpRtaddr(g_ipaddr.sin_addr);
                        }
                        if (ia == NULL) {
                            icmpType = 3;
                            icmpCode = 5;
                            badOpt = 1;
                            break;
                        }
                        ip->ip_dst = g_ipaddr.sin_addr;
                        sceNet_lib_2F305274(&ia->ia_addr.sin_addr, cp + off, 4);
                        cp[2] = (u8)(cp[2] + 4);
                        forward = ((ip->ip_dst & inetBswap32(0xF0000000U)) != inetBswap32(0xE0000000U));
                    }
                }
            }
        } else if (opt == 7) { /* RR */
            if (optLen < 3) {
                icmpCode = (int)(cp - base) + 1;
                badOpt = 1;
                break;
            }
            {
                u32 off = cp[2];
                if (off < 4) {
                    icmpCode = (int)(cp - base) + 2;
                    badOpt = 1;
                    break;
                }
                if ((s32)(off + 3) <= optLen) {
                    InetInIfaddr *ia;
                    sceNet_lib_2F305274(&ip->ip_dst, &g_ipaddr.sin_addr, 4);
                    ia = (InetInIfaddr *)inetIfaWithAddr((InetSockaddr *)&g_ipaddr);
                    if (ia == NULL) {
                        ia = inetIpRtaddr(g_ipaddr.sin_addr);
                        if (ia == NULL) {
                            icmpType = 3;
                            icmpCode = 1;
                            badOpt = 1;
                            break;
                        }
                    }
                    sceNet_lib_2F305274(&ia->ia_addr.sin_addr, cp + off - 1, 4);
                    cp[2] = (u8)(cp[2] + 4);
                }
            }
        } else if (opt == 68) { /* TS */
            if (optLen < 4 || optLen > 40) {
                icmpCode = (int)(cp - base) + 1;
                badOpt = 1;
                break;
            }
            {
                u32 off = cp[2];
                if (off < 5) {
                    icmpCode = (int)(cp - base) + 2;
                    badOpt = 1;
                    break;
                }
                if ((s32)(off + 4) > optLen) {
                    u8 oflw = (u8)((cp[3] >> 4) + 1);
                    cp[3] = (u8)((cp[3] & 0x0F) | ((oflw & 0x0F) << 4));
                    if ((oflw & 0x0F) == 0) {
                        icmpCode = (int)(cp - base) + 2;
                        badOpt = 1;
                        break;
                    }
                } else {
                    u8 tsFlags = (u8)(cp[3] & 0x0F);
                    u32 tsVal;
                    int storeTs = 1;
                    if (tsFlags == 1) {
                        if ((s32)(off + 7) > optLen) {
                            icmpCode = (int)(cp - base) + 2;
                            badOpt = 1;
                            break;
                        }
                        g_ipaddr.sin_addr = origDst;
                        {
                            InetInIfaddr *ia = (InetInIfaddr *)inetIfaOfIfpForAddr((InetSockaddr *)&g_ipaddr, M_RCVIF(m));
                            if (ia != NULL) {
                                sceNet_lib_2F305274(&ia->ia_addr.sin_addr, cp + off - 1, 4);
                                cp[2] = (u8)(cp[2] + 4);
                            } else {
                                storeTs = 0;
                            }
                        }
                    } else if (tsFlags == 3) {
                        if ((s32)(off + 7) > optLen) {
                            icmpCode = (int)(cp - base) + 2;
                            badOpt = 1;
                            break;
                        }
                        sceNet_lib_2F305274(cp + off - 1, &g_ipaddr.sin_addr, 4);
                        if (inetIfaWithAddr((InetSockaddr *)&g_ipaddr) == NULL) {
                            storeTs = 0;
                        } else {
                            cp[2] = (u8)(cp[2] + 4);
                        }
                    } else if (tsFlags != 0) {
                        icmpCode = (int)(cp - base) + 3;
                        badOpt = 1;
                        break;
                    }

                    if (storeTs) {
                        tsVal = inetIptime();
                        sceNet_lib_2F305274(&tsVal, cp + cp[2] - 1, 4);
                        cp[2] = (u8)(cp[2] + 4);
                    }
                }
            }
        }

        cnt -= optLen;
        cp += optLen;
    }

    if (!badOpt) {
        if (!forward) {
            return 0;
        }
        if (g_ip_forwsrcrt != 0) {
            inetIpForward(m, 1);
            return 1;
        }
        icmpType = 3;
        icmpCode = 5;
    }

    inetIcmpError(m, icmpType, icmpCode, 0, NULL);
    g_ipstat.ips_badoptions++;
    return 1;
}

/* [276] 0x017714 */
static INET_NOINLINE InetInIfaddr *inetIpRtaddr(u32 dstAddr)
{
    InetSockaddrIn *sin = (InetSockaddrIn *)&g_ipforward_rt.ro_dst;

    if (g_ipforward_rt.ro_rt != NULL && sin->sin_addr != dstAddr) {
        if (g_ipforward_rt.ro_rt->rt_refcnt >= 2) {
            g_ipforward_rt.ro_rt->rt_refcnt--;
        } else {
            inetRtFree(g_ipforward_rt.ro_rt);
        }
        g_ipforward_rt.ro_rt = NULL;
    }
    if (g_ipforward_rt.ro_rt == NULL) {
        sin->sin_family = INET_AF_INET;
        sin->sin_len = sizeof(InetSockaddrIn);
        sin->sin_addr = dstAddr;
        inetRtAlloc(&g_ipforward_rt);
    }
    if (g_ipforward_rt.ro_rt == NULL) {
        return NULL;
    }
    return (InetInIfaddr *)g_ipforward_rt.ro_rt->rt_ifa;
}

/* [277] 0x0177C8 */
static INET_NOINLINE void inetIpSaveRt(const u8 *opt, u32 dstAddr)
{
    u32 optLen = opt[1];
    if (optLen < 44) {
        sceNet_lib_2F305274(opt, g_ip_srcrt.srcopt, (int)optLen);
        g_ip_srcrt.dst = dstAddr;
        g_ip_nhops = (s32)((optLen - 3) >> 2);
    }
}

/* [278] 0x017838 */
static INET_NOINLINE void inetIpForward(InetMbuf *m, int srcrt)
{
    InetIpHdr *ip = (InetIpHdr *)m->m_data;
    InetRtentry *rt;
    InetMbuf *mcopy;
    u32 dest = 0;
    int type = 0;
    int code = 0;
    int err;

    if ((m->m_flags & (INET_M_BCAST | INET_M_MCAST)) != 0 || !inetInCanForward(ip->ip_dst)) {
        g_ipstat.ips_cantforward++;
        sceNetIfhandle_lib_F56FAC82(m);
        return;
    }
    if (ip->ip_ttl < 2) {
        inetIcmpError(m, 11, 0, 0, NULL);
        return;
    }
    ip->ip_ttl--;

    rt = g_ipforward_rt.ro_rt;
    if (rt != NULL && ((InetSockaddrIn *)&g_ipforward_rt.ro_dst)->sin_addr != ip->ip_dst) {
        if (rt->rt_refcnt >= 2) {
            rt->rt_refcnt--;
        } else {
            inetRtFree(rt);
        }
        g_ipforward_rt.ro_rt = NULL;
        rt = NULL;
    }
    if (rt == NULL) {
        InetSockaddrIn *sin = (InetSockaddrIn *)&g_ipforward_rt.ro_dst;
        sin->sin_family = INET_AF_INET;
        sin->sin_len = sizeof(InetSockaddrIn);
        sin->sin_addr = ip->ip_dst;
        inetRtAlloc(&g_ipforward_rt);
        rt = g_ipforward_rt.ro_rt;
        if (rt == NULL) {
            inetIcmpError(m, 3, 1, 0, NULL);
            return;
        }
    }

    {
        s32 copyLen = (ip->ip_len < 68) ? ip->ip_len : 68;
        mcopy = sceNetIfhandle_lib_456E3146(m, 0, copyLen, INET_M_DONTWAIT);
        if (mcopy != NULL) {
            mcopy = sceNetIfhandle_lib_49EDBB18(mcopy, (s32)(ip->ip_vhl & 0x0F) * 4);
        }
    }

    if (rt->rt_ifp == M_RCVIF(m) &&
        (rt->rt_flags & (INET_RTF_DYNAMIC | INET_RTF_MODIFIED)) == 0 &&
        ((InetSockaddrIn *)rt->rt_nodes[0].rn_u.rn_leaf.rn_key)->sin_addr != 0 &&
        g_ipsendredirects != 0 && srcrt == 0) {
        InetInIfaddr *ia = (InetInIfaddr *)rt->rt_ifa;
        if (ia != NULL && (ip->ip_src & ia->ia_subnetmask) == ia->ia_subnet) {
            dest = ((rt->rt_flags & INET_RTF_GATEWAY) != 0) ?
                ((InetSockaddrIn *)rt->rt_gateway)->sin_addr : ip->ip_dst;
            type = 5;
            code = 1;
        }
    }

    err = inetIpOutput(m, NULL, &g_ipforward_rt,
                       (g_ip_directedbcast != 0) ? 0x21 : 0x01, NULL);
    if (err != 0) {
        g_ipstat.ips_cantforward++;
    } else {
        g_ipstat.ips_forward++;
        if (type == 0) {
            if (mcopy != NULL) {
                sceNetIfhandle_lib_F56FAC82(mcopy);
            }
            return;
        }
        g_ipstat.ips_redirectsent++;
    }

    if (mcopy != NULL) {
        InetIfnet *destIfp = NULL;
        switch (err) {
        case 0:
            break;
        case INET_EMSGSIZE:
            type = 3;
            code = 4;
            if (g_ipforward_rt.ro_rt != NULL) {
                destIfp = g_ipforward_rt.ro_rt->rt_ifp;
            }
            g_ipstat.ips_cantfrag++;
            break;
        case INET_ENOBUFS:
            type = 4;
            code = 0;
            break;
        default:
            type = 3;
            code = 1;
            break;
        }
        inetIcmpError(mcopy, type, code, dest, destIfp);
    }
}

/* [279] 0x017C20 */
INET_NOINLINE void inetIpSaveControl(InetInpcbReal *inp, InetMbuf **mp, InetIpHdr *ip, InetMbuf *m)
{
    if ((inp->inp_socket->so_options & INET_SO_TIMESTAMP) != 0) {
        InetTimeval tv;
        sceNet_lib_4B2B3416(&tv);
        *mp = inetSbCreateControl(&tv, sizeof(tv), 2, INET_SOL_SOCKET);
        if (*mp != NULL) {
            mp = &(*mp)->m_next;
        }
    }
    if ((inp->inp_flags & 0x04) != 0) {
        *mp = inetSbCreateControl(&ip->ip_dst, sizeof(ip->ip_dst), 7, INET_IPPROTO_IP);
        if (*mp != NULL) {
            mp = &(*mp)->m_next;
        }
    }
    if ((inp->inp_flags & 0x80) != 0) {
        InetSockaddrDl sdl;
        sdl.sdl_len = 8;
        sdl.sdl_family = INET_AF_LINK;
        sdl.sdl_index = (M_RCVIF(m) != NULL) ? M_RCVIF(m)->if_index : 0;
        sdl.sdl_nlen = 0;
        sdl.sdl_alen = 0;
        sdl.sdl_slen = 0;
        *mp = inetSbCreateControl(&sdl, sdl.sdl_len, 20, INET_IPPROTO_IP);
    }
}

/* [280] 0x017D2C */
INET_NOINLINE int inetIpSysctl(int *name, u32 namelen, void *oldp, u32 *oldlenp, void *newp, u32 newlen)
{
    if (namelen != 1) {
        return 20; /* ENOTDIR */
    }
    switch (name[0]) {
    case 1:  return inetSysctlInt(oldp, oldlenp, newp, newlen, &g_ipforwarding);
    case 2:  return inetSysctlInt(oldp, oldlenp, newp, newlen, &g_ipsendredirects);
    case 3:  return inetSysctlInt(oldp, oldlenp, newp, newlen, &g_ip_defttl);
    case 5:  return inetSysctlInt(oldp, oldlenp, newp, newlen, &g_ip_forwsrcrt);
    case 6:  return inetSysctlInt(oldp, oldlenp, newp, newlen, &g_ip_directedbcast);
    case 7:  return inetSysctlInt(oldp, oldlenp, newp, newlen, &g_ip_allowsrcrt);
    case 8:  return inetSysctlInt(oldp, oldlenp, newp, newlen, &g_subnetsarelocal);
    case 9: {
        int err = inetSysctlInt(oldp, oldlenp, newp, newlen, &g_ip_mtudisc);
        if (g_ip_mtudisc != 0 && g_ip_mtudisc_timeout_q == NULL) {
            g_ip_mtudisc_timeout_q = inetRtTimerQueueCreate(g_ip_mtudisc_timeout);
        } else if (g_ip_mtudisc == 0 && g_ip_mtudisc_timeout_q != NULL) {
            inetRtTimerQueueDestroy(g_ip_mtudisc_timeout_q, 1);
            g_ip_mtudisc_timeout_q = NULL;
        }
        return err;
    }
    case 10: {
        s32 oldVal = g_anonportmin;
        int err = inetSysctlInt(oldp, oldlenp, newp, newlen, &g_anonportmin);
        if (g_anonportmin >= g_anonportmax || g_anonportmin < 1024 || g_anonportmin > 65535) {
            g_anonportmin = oldVal;
            return INET_EINVAL;
        }
        return err;
    }
    case 11: {
        s32 oldVal = g_anonportmax;
        int err = inetSysctlInt(oldp, oldlenp, newp, newlen, &g_anonportmax);
        if (g_anonportmin >= g_anonportmax || g_anonportmax < 1024 || g_anonportmax > 65535) {
            g_anonportmax = oldVal;
            return INET_EINVAL;
        }
        return err;
    }
    case 12: {
        int err = inetSysctlInt(oldp, oldlenp, newp, newlen, &g_ip_mtudisc_timeout);
        if (g_ip_mtudisc_timeout_q != NULL) {
            inetRtTimerQueueChange(g_ip_mtudisc_timeout_q, g_ip_mtudisc_timeout);
        }
        return err;
    }
    case 14: return inetSysctlInt(oldp, oldlenp, newp, newlen, &g_hostzeroisbroadcast);
    case 16: {
        s32 oldVal = g_lowportmin;
        int err = inetSysctlInt(oldp, oldlenp, newp, newlen, &g_lowportmin);
        if (g_lowportmin >= g_lowportmax || g_lowportmin < 600 || g_lowportmin >= 1024) {
            g_lowportmin = oldVal;
            return INET_EINVAL;
        }
        return err;
    }
    case 17: {
        s32 oldVal = g_lowportmax;
        int err = inetSysctlInt(oldp, oldlenp, newp, newlen, &g_lowportmax);
        if (g_lowportmin >= g_lowportmax || g_lowportmax < 600 || g_lowportmax >= 1024) {
            g_lowportmax = oldVal;
            return INET_EINVAL;
        }
        return err;
    }
    case 18: return inetSysctlInt(oldp, oldlenp, newp, newlen, &g_ip_maxfragpackets);
    case 19: return inetSysctlRdStruct(oldp, oldlenp, newp, &g_ipstat, sizeof(g_ipstat));
    default: return INET_EOPNOTSUPP;
    }
}

/* [281] 0x0180F8 */
INET_NOINLINE void inetIpIntr(void)
{
    for (;;) {
        int spl = sceNet_lib_94B44F26();
        InetMbuf *m = g_ipintrq.ifq_head;
        if (m != NULL) {
            g_ipintrq.ifq_head = m->m_nextpkt;
            if (g_ipintrq.ifq_head == NULL) {
                g_ipintrq.ifq_tail = NULL;
            }
            m->m_nextpkt = NULL;
            g_ipintrq.ifq_len--;
        }
        sceNet_lib_515B2F33(spl);
        if (m == NULL) {
            break;
        }
        inetIpInput(m);
    }
}

/* [282] 0x018180 */
INET_NOINLINE void inetIpSlowtimo(void)
{
    int outerSpl = sceNet_lib_94B44F26();
    int innerSpl = sceNet_lib_94B44F26();
    InetIpq *fp;

    if (g_ipq_locked == 0) {
        g_ipq_locked = 1;
    }
    sceNet_lib_515B2F33(innerSpl);

    fp = g_ipq_head;
    while (fp != NULL) {
        InetIpq *next = fp->ipq_next;
        fp->ipq_ttl--;
        if (fp->ipq_ttl == 0) {
            g_ipstat.ips_fragtimeout++;
            inetIpFreef(fp);
        }
        fp = next;
    }

    if (g_ip_maxfragpackets >= 0) {
        while (g_ip_nfragpackets > g_ip_maxfragpackets && g_ipq_head != NULL) {
            inetIpFreef(g_ipq_head);
        }
    }

    innerSpl = sceNet_lib_94B44F26();
    g_ipq_locked = 0;
    sceNet_lib_515B2F33(innerSpl);
    sceNet_lib_515B2F33(outerSpl);
}

/* [283] 0x0182C4 */
INET_NOINLINE void inetIpDrain(void)
{
    int spl = sceNet_lib_94B44F26();
    if (g_ipq_locked == 0) {
        g_ipq_locked = 1;
        sceNet_lib_515B2F33(spl);
        while (g_ipq_head != NULL) {
            g_ipstat.ips_fragdropped++;
            inetIpFreef(g_ipq_head);
        }
        spl = sceNet_lib_94B44F26();
        g_ipq_locked = 0;
    }
    sceNet_lib_515B2F33(spl);
}

/* [284] 0x01836C */
INET_NOINLINE InetMbuf *inetIpSrcroute(void)
{
    InetMbuf *m;
    u32 *outWords;
    int idx;

    if (g_ip_nhops == 0) {
        return NULL;
    }
    m = sceNetIfhandle_lib_A493AA5F(INET_M_DONTWAIT, INET_MT_SOOPTS);
    if (m == NULL) {
        return NULL;
    }
    m->m_len = 4 * g_ip_nhops + 8;
    outWords = (u32 *)m->m_data;
    outWords[0] = g_ip_srcrt.route[g_ip_nhops - 1];
    g_ip_srcrt.nop = 1;
    g_ip_srcrt.srcopt[2] = 4;
    sceNet_lib_2F305274(&g_ip_srcrt.nop, outWords + 1, 4);

    {
        u32 *dstPtr = outWords + 2;
        for (idx = g_ip_nhops - 2; idx >= 0; idx--) {
            *dstPtr++ = g_ip_srcrt.route[idx];
        }
        *dstPtr = g_ip_srcrt.dst;
    }
    return m;
}

/* [285] 0x018474 */
INET_NOINLINE void inetIpStripOptions(InetMbuf *m)
{
    InetIpHdr *ip = (InetIpHdr *)m->m_data;
    s32 hlen = (s32)(ip->ip_vhl & 0x0F) * 4;
    s32 optLen = hlen - (s32)sizeof(InetIpHdr);

    sceNet_lib_2F305274((u8 *)ip + hlen, (u8 *)ip + sizeof(InetIpHdr), m->m_len - hlen);
    ip->ip_vhl = (u8)((ip->ip_vhl & 0xF0) | 5);
    m->m_len -= optLen;
    if ((m->m_flags & INET_M_PKTHDR) != 0) {
        M_PKTLEN(m) -= optLen;
    }
    ip->ip_len = (u16)(ip->ip_len - optLen);
}
