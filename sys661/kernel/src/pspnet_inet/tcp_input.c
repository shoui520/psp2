/*
 * PSP 6.61 SceNetInet_Library (pspnet_inet.prx)
 * TCP input, reassembly, options, RTT estimation, NewReno, and SYN cache (tcp_input.c)
 */

#include "pspnet_inet_internal.h"

/* Parsed TCP options */
typedef struct InetTcpOptInfo {
    int ts_present;
    u32 ts_val;
    u32 ts_ecr;
    u16 maxseg;
    u16 pad_align;
} InetTcpOptInfo;

/* SYN cache timer queue (8 bytes) */
typedef struct InetSynCacheTimerQ {
    InetSynCache *tqh_first;
    InetSynCache **tqh_last;
} InetSynCacheTimerQ;

int g_tcp_syn_cache_size = 293;
int g_tcp_syn_cache_limit = 10255;
int g_tcp_syn_bucket_limit = 105;
int g_tcp_syn_cache_interval = 8;
int g_tcp_rstppslim = 100;
int g_tcprexmtthresh = 3;

InetSynCacheHead g_tcp_syn_cache[293];
static InetSynCacheTimerQ g_tcp_syn_cache_timeq[13];
u32 g_tcp_syn_cache_count = 0;
static u32 g_syn_hash_secret_a = 0;
static u32 g_syn_hash_secret_b = 0;

InetInpcbTable g_tcbtable;
u32 g_tcp_now = 0;
InetTcpStat g_tcpstat;
InetTcpcb *g_tcp_delack_head = NULL;

static INET_NOINLINE void inetTcpDooptions(InetTcpcb *tp, const u8 *cp, int cnt,
                                           InetTcpHdr *th, InetTcpOptInfo *oi);
static INET_NOINLINE void inetTcpPulloutofband(InetSocket *so, InetTcpHdr *th, InetMbuf *m, int off);
static INET_NOINLINE void inetTcpXmitTimer(InetTcpcb *tp, s16 rtt);
static INET_NOINLINE int inetTcpNewrenoPartialAck(InetTcpcb *tp, InetTcpHdr *th);
static INET_NOINLINE void inetSynCacheInsert(InetSynCache *sc, InetTcpcb *tp);
static INET_NOINLINE InetSocket *inetSynCacheGet(InetSockaddrIn *src, InetSockaddrIn *dst,
                                                 InetTcpHdr *th, int hlen, int tlen,
                                                 InetSocket *so, InetMbuf *m);
static INET_NOINLINE void inetSynCacheReset(InetSockaddrIn *src, InetSockaddrIn *dst, InetTcpHdr *th);
static INET_NOINLINE int inetSynCacheAdd(InetSockaddrIn *src, InetSockaddrIn *dst,
                                         InetTcpHdr *th, int tlen, InetSocket *so,
                                         InetMbuf *m, const u8 *optp, int optlen,
                                         InetTcpOptInfo *oi, u16 p2pPort);
static INET_NOINLINE int inetSynCacheRespond(InetSynCache *sc, InetMbuf *m);

static int tcpSeqLt(u32 lhs, u32 rhs) { return (s32)(lhs - rhs) < 0; }
static int tcpSeqLeq(u32 lhs, u32 rhs) { return (s32)(lhs - rhs) <= 0; }
static int tcpSeqGt(u32 lhs, u32 rhs) { return (s32)(lhs - rhs) > 0; }
static int tcpSeqGeq(u32 lhs, u32 rhs) { return (s32)(lhs - rhs) >= 0; }

static void tcpDelackSchedule(InetTcpcb *tp)
{
    if ((tp->t_flags & 2) == 0) {
        tp->t_flags |= 2;
        tp->t_delack_next = g_tcp_delack_head;
        if (g_tcp_delack_head != NULL) {
            g_tcp_delack_head->t_delack_prev = &tp->t_delack_next;
        }
        g_tcp_delack_head = tp;
        tp->t_delack_prev = &g_tcp_delack_head;
    }
}

static void synCacheFreeEntry(InetSynCache *sc)
{
    if (sc->sc_ipopts != NULL) {
        sceNetIfhandle_lib_F8825DC4(sc->sc_ipopts);
    }
    if (sc->sc_route.ro_rt != NULL) {
        if (sc->sc_route.ro_rt->rt_refcnt >= 2) {
            sc->sc_route.ro_rt->rt_refcnt--;
        } else {
            inetRtFree(sc->sc_route.ro_rt);
        }
    }
    sceNetIfhandle_lib_76BAD213(sc);
}

static void synCacheUnlink(InetSynCache *sc)
{
    if (sc->sc_bucketq_next != NULL) {
        sc->sc_bucketq_next->sc_bucketq_prev = sc->sc_bucketq_prev;
    }
    *sc->sc_bucketq_prev = sc->sc_bucketq_next;
    sc->sc_tp = NULL;

    if (sc->sc_tpq_next != NULL) {
        sc->sc_tpq_next->sc_tpq_prev = sc->sc_tpq_prev;
    }
    *sc->sc_tpq_prev = sc->sc_tpq_next;

    g_tcp_syn_cache[sc->sc_bucketidx].sch_length--;

    if (sc->sc_timerq_next != NULL) {
        sc->sc_timerq_next->sc_timerq_prev = sc->sc_timerq_prev;
    } else {
        g_tcp_syn_cache_timeq[sc->sc_rxtshift].tqh_last = sc->sc_timerq_prev;
    }
    *sc->sc_timerq_prev = sc->sc_timerq_next;
    g_tcp_syn_cache_count--;
}

static InetSynCache *synCacheLookup(const InetSockaddrIn *src, const InetSockaddrIn *dst, u32 *outHash)
{
    u32 hash = (src->sin_addr ^ g_syn_hash_secret_a) *
               ((((u32)dst->sin_port << 16) + src->sin_port) ^ g_syn_hash_secret_b);
    u32 bucket = hash % (u32)g_tcp_syn_cache_size;
    int spl = sceNet_lib_94B44F26();
    InetSynCache *sc = g_tcp_syn_cache[bucket].sch_first;

    if (outHash != NULL) {
        *outHash = hash;
    }
    while (sc != NULL) {
        if (sc->sc_hash == hash &&
            sceNet_lib_4BF83CF4(&sc->sc_src, src, sizeof(InetSockaddrIn)) == 0 &&
            sceNet_lib_4BF83CF4(&sc->sc_dst, dst, sizeof(InetSockaddrIn)) == 0) {
            break;
        }
        sc = sc->sc_bucketq_next;
    }
    sceNet_lib_515B2F33(spl);
    return sc;
}

/* [346] 0x020A0C */
INET_NOINLINE int inetTcpReass(InetTcpcb *tp, InetTcpHdr *th, InetMbuf *m, u32 *tlenp)
{
    InetSocket *so = (tp->t_inpcb != NULL) ? tp->t_inpcb->inp_socket : NULL;
    InetTcpqent *ti = NULL;
    InetTcpqent *prev = NULL;
    u32 dupBytes = 0;

    if (th != NULL) {
        InetTcpqent *cur = tp->segq_first;
        u32 origLen = *tlenp;
        u32 curLen = origLen;
        u32 curFlags = th->th_flags;
        u32 curSeq = th->th_seq;
        int dropped = 0;

        while (cur != NULL) {
            InetTcpqent *next = cur->tqe_q_next;
            u32 qSeq = cur->tqe_seq;
            u32 qLen = cur->tqe_len;
            u32 qEnd = qSeq + qLen;

            if (qEnd == curSeq) {
                curLen += qLen;
                curFlags |= cur->tqe_flags;
                curSeq = qSeq;
                sceNetIfhandle_lib_3E8DD3F8(cur->tqe_m, m);
                m = cur->tqe_m;
                if (cur->tqe_q_next != NULL) {
                    cur->tqe_q_next->tqe_q_prev = cur->tqe_q_prev;
                }
                *cur->tqe_q_prev = cur->tqe_q_next;
                if (cur->tqe_timeq_next != NULL) {
                    cur->tqe_timeq_next->tqe_timeq_prev = cur->tqe_timeq_prev;
                }
                *cur->tqe_timeq_prev = cur->tqe_timeq_next;
                if (ti != NULL) {
                    sceNetIfhandle_lib_76BAD213(cur);
                } else {
                    ti = cur;
                }
                cur = next;
                continue;
            }

            if (tcpSeqLt(qEnd, curSeq)) {
                prev = cur;
                cur = next;
                continue;
            }

            if (tcpSeqGt(qSeq, curSeq + curLen)) {
                break;
            }

            if (tcpSeqLeq(qSeq, curSeq) && tcpSeqGeq(qEnd, curSeq + curLen)) {
                g_tcpstat.tcps_rcvduppack++;
                g_tcpstat.tcps_rcvdupbyte += curLen;
                sceNetIfhandle_lib_F56FAC82(m);
                if (ti != NULL) {
                    sceNetIfhandle_lib_76BAD213(ti);
                }
                dropped = 1;
                break;
            }

            if (tcpSeqGeq(qSeq, curSeq) && tcpSeqLeq(qEnd, curSeq + curLen)) {
                dupBytes += qLen;
                sceNetIfhandle_lib_F56FAC82(cur->tqe_m);
                if (cur->tqe_q_next != NULL) {
                    cur->tqe_q_next->tqe_q_prev = cur->tqe_q_prev;
                }
                *cur->tqe_q_prev = cur->tqe_q_next;
                if (cur->tqe_timeq_next != NULL) {
                    cur->tqe_timeq_next->tqe_timeq_prev = cur->tqe_timeq_prev;
                }
                *cur->tqe_timeq_prev = cur->tqe_timeq_next;
                if (ti != NULL) {
                    sceNetIfhandle_lib_76BAD213(cur);
                } else {
                    ti = cur;
                }
                cur = next;
                continue;
            }

            if (tcpSeqLt(qSeq, curSeq)) {
                s32 overlap = (s32)(qEnd - curSeq);
                if (overlap > 0) {
                    sceNetIfhandle_lib_BFF3CEA5(m, overlap);
                    origLen -= (u32)overlap;
                    sceNetIfhandle_lib_3E8DD3F8(cur->tqe_m, m);
                    m = cur->tqe_m;
                    curSeq = qSeq;
                    curLen += qLen - (u32)overlap;
                    dupBytes += (u32)overlap;
                    if (cur->tqe_q_next != NULL) {
                        cur->tqe_q_next->tqe_q_prev = cur->tqe_q_prev;
                    }
                    *cur->tqe_q_prev = cur->tqe_q_next;
                    if (cur->tqe_timeq_next != NULL) {
                        cur->tqe_timeq_next->tqe_timeq_prev = cur->tqe_timeq_prev;
                    }
                    *cur->tqe_timeq_prev = cur->tqe_timeq_next;
                    if (ti != NULL) {
                        sceNetIfhandle_lib_76BAD213(cur);
                    } else {
                        ti = cur;
                    }
                    cur = next;
                    continue;
                }
            }

            if (tcpSeqGt(qSeq, curSeq)) {
                s32 overlap = (s32)(curSeq + curLen - qSeq);
                if (overlap > 0) {
                    sceNetIfhandle_lib_BFF3CEA5(m, -overlap);
                    origLen -= (u32)overlap;
                    curLen -= (u32)overlap;
                    dupBytes += (u32)overlap;
                }
            }

            if (qSeq == curSeq + curLen) {
                curLen += qLen;
                curFlags |= cur->tqe_flags;
                sceNetIfhandle_lib_3E8DD3F8(m, cur->tqe_m);
                if (cur->tqe_q_next != NULL) {
                    cur->tqe_q_next->tqe_q_prev = cur->tqe_q_prev;
                }
                *cur->tqe_q_prev = cur->tqe_q_next;
                if (cur->tqe_timeq_next != NULL) {
                    cur->tqe_timeq_next->tqe_timeq_prev = cur->tqe_timeq_prev;
                }
                *cur->tqe_timeq_prev = cur->tqe_timeq_next;
                if (ti != NULL) {
                    sceNetIfhandle_lib_76BAD213(cur);
                } else {
                    ti = cur;
                }
                break;
            }

            if (tcpSeqLeq(qSeq, curSeq)) {
                prev = cur;
            }
            cur = next;
        }

        if (dropped) {
            return 0;
        }

        if (ti == NULL) {
            ti = (InetTcpqent *)sceNetIfhandle_lib_15CFE3C0(sizeof(InetTcpqent));
            if (ti == NULL) {
                g_tcpstat.tcps_rcvmemdrop++;
                sceNetIfhandle_lib_F56FAC82(m);
                return 0;
            }
        }

        g_tcpstat.tcps_rcvoopack++;
        g_tcpstat.tcps_rcvoobyte += origLen;
        if (dupBytes != 0) {
            g_tcpstat.tcps_rcvpartduppack++;
            g_tcpstat.tcps_rcvpartdupbyte += dupBytes;
        }

        ti->tqe_m = m;
        ti->tqe_seq = curSeq;
        ti->tqe_len = curLen;
        ti->tqe_flags = curFlags;
        if (prev != NULL) {
            ti->tqe_q_next = prev->tqe_q_next;
            if (prev->tqe_q_next != NULL) {
                prev->tqe_q_next->tqe_q_prev = &ti->tqe_q_next;
            }
            ti->tqe_q_prev = &prev->tqe_q_next;
            prev->tqe_q_next = ti;
        } else {
            ti->tqe_q_next = tp->segq_first;
            if (tp->segq_first != NULL) {
                tp->segq_first->tqe_q_prev = &ti->tqe_q_next;
            }
            ti->tqe_q_prev = &tp->segq_first;
            tp->segq_first = ti;
        }

        ti->tqe_timeq_next = tp->t_segq_time_first;
        if (tp->t_segq_time_first != NULL) {
            tp->t_segq_time_first->tqe_timeq_prev = &ti->tqe_timeq_next;
        }
        ti->tqe_timeq_prev = &tp->t_segq_time_first;
        tp->t_segq_time_first = ti;
    }

    if (tp->t_state < 4) {
        return 0;
    }

    {
        InetTcpqent *head = tp->segq_first;
        int flags;
        if (head == NULL || head->tqe_seq != tp->rcv_nxt) {
            return 0;
        }
        if (tp->t_state == 3 && head->tqe_len != 0) {
            return 0;
        }
        tp->rcv_nxt += head->tqe_len;
        flags = (int)(head->tqe_flags & 1);

        if (head->tqe_q_next != NULL) {
            head->tqe_q_next->tqe_q_prev = head->tqe_q_prev;
        }
        *head->tqe_q_prev = head->tqe_q_next;
        if (head->tqe_timeq_next != NULL) {
            head->tqe_timeq_next->tqe_timeq_prev = head->tqe_timeq_prev;
        }
        *head->tqe_timeq_prev = head->tqe_timeq_next;

        if ((so->so_state & INET_SS_CANTRCVMORE) != 0) {
            sceNetIfhandle_lib_F56FAC82(head->tqe_m);
        } else {
            inetSbAppend(&so->so_rcv, head->tqe_m);
        }
        sceNetIfhandle_lib_76BAD213(head);
        if ((so->so_rcv.sb_flags & 0x3C) != 0) {
            inetSoWakeup(so, &so->so_rcv);
        }
        return flags;
    }
}

/* [347] 0x020FE8 */
INET_NOINLINE int inetTcpP2pDeliver(InetSockaddrIn *src, InetSockaddrIn *dst, InetMbuf *m,
                                    s32 off, u32 len, u8 protoByte)
{
    u32 kind = (u32)(protoByte & 0x0F);
    InetP2pKeyEntry *keyEnt = inetP2pFindKey(src->sin_addr, src->sin_port);
    InetMbuf *decM;
    InetTcpHdr *th;
    InetInpcbReal *inp;
    InetTcpcb *tp;
    u16 dstPort;
    InetIpHdr *ip;

    if (kind != 7 && keyEnt == NULL) {
        return 0;
    }

    decM = inetP2pDecrypt(m, off, len, protoByte);
    if (decM == NULL) {
        return 0;
    }

    decM = sceNetIfhandle_lib_49EDBB18(decM, (s32)sizeof(InetTcpHdr));
    if (decM == NULL) {
        return 0;
    }

    th = (InetTcpHdr *)decM->m_data;
    inp = inetInPcbLookupConnect(&g_tcbtable, src->sin_addr, th->th_sport,
                                 dst->sin_addr, th->th_dport);
    if (inp == NULL) {
        inp = inetInPcbLookupBind(&g_tcbtable, dst->sin_addr, th->th_dport);
    }
    if (inp == NULL || inp->inp_ppcb == NULL) {
        sceNetIfhandle_lib_F56FAC82(decM);
        return 0;
    }

    tp = (InetTcpcb *)inp->inp_ppcb;
    dstPort = dst->sin_port;
    if ((keyEnt != NULL && keyEnt->lport == dstPort) || dstPort == g_p2p_default_port) {
        dstPort = inetBswap16(0x0E4A);
    }

    if ((tp->t_flags & 0x10000) == 0 || tp->t_p2p_lport != dstPort ||
        ((tp->t_flags & 0x20000) != 0 && kind != 8 && kind != 10) ||
        ((tp->t_flags & 0x40000) != 0 && kind != 9 && kind != 10)) {
        sceNetIfhandle_lib_F56FAC82(decM);
        return 0;
    }

    if ((decM->m_flags & INET_M_EXT) == 0) {
        u32 lead = ((decM->m_flags & INET_M_PKTHDR) != 0)
                       ? (u32)(decM->m_data - M_PKTDAT(decM))
                       : (u32)(decM->m_data - M_DAT(decM));
        if (lead >= sizeof(InetIpHdr)) {
            decM->m_data -= sizeof(InetIpHdr);
            decM->m_len += (s32)sizeof(InetIpHdr);
        } else {
            decM = sceNetIfhandle_lib_C3325FDC(decM, (s32)sizeof(InetIpHdr), INET_M_DONTWAIT);
        }
    } else {
        decM = sceNetIfhandle_lib_C3325FDC(decM, (s32)sizeof(InetIpHdr), INET_M_DONTWAIT);
    }

    if (decM == NULL) {
        return 0;
    }
    if ((decM->m_flags & INET_M_PKTHDR) != 0) {
        M_PKTLEN(decM) += (s32)sizeof(InetIpHdr);
    }

    ip = (InetIpHdr *)decM->m_data;
    sceKernelMemset(ip, 0, sizeof(InetIpHdr));
    ip->ip_vhl = 0x45;
    ip->ip_p = INET_IPPROTO_TCP;
    ip->ip_len = (u16)M_PKTLEN(decM);
    ip->ip_src = src->sin_addr;
    ip->ip_dst = dst->sin_addr;

    inetTcpInput(decM, (int)sizeof(InetIpHdr), src->sin_port);
    return 1;
}

/* [348] 0x0212E0 */
INET_NOINLINE void inetTcpInput(InetMbuf *m, int iphlen, int p2pSrcPort)
{
    InetIpHdr *ip;
    InetTcpHdr *th;
    InetTcpOptInfo opti;
    InetMbuf *saveM = NULL;
    u32 tlen;
    u32 ipTotalLen;
    int off;
    const u8 *optp = NULL;
    int optlen = 0;
    u8 tiflags;
    u16 p2pPort = 0;
    int needOutput = 0;
    int findPcb = 1;
    InetInpcbReal *inp = NULL;
    InetTcpcb *tp = NULL;
    InetSocket *so = NULL;
    u32 tiwin = 0;
    int dropWithReset = 0;
    int dropAfterAck = 0;
    int dropOnly = 0;

    g_tcpstat.tcps_rcvtotal++;
    sceNet_lib_8B615F52(&opti, sizeof(opti));

    if ((m->m_flags & (INET_M_BCAST | INET_M_MCAST)) != 0 ||
        (((InetIpHdr *)m->m_data)->ip_vhl >> 4) != 4) {
        sceNetIfhandle_lib_F56FAC82(m);
        return;
    }

    if (iphlen > (int)sizeof(InetIpHdr)) {
        inetIpStripOptions(m);
        iphlen = (int)sizeof(InetIpHdr);
    }

    if (m->m_len < iphlen + (s32)sizeof(InetTcpHdr)) {
        m = sceNetIfhandle_lib_49EDBB18(m, iphlen + (s32)sizeof(InetTcpHdr));
        if (m == NULL) {
            g_tcpstat.tcps_rcvshort++;
            return;
        }
    }

    ip = (InetIpHdr *)m->m_data;
    th = (InetTcpHdr *)(m->m_data + iphlen);
    if ((ip->ip_dst & inetBswap32(0xF0000000U)) == inetBswap32(0xE0000000U)) {
        sceNetIfhandle_lib_F56FAC82(m);
        return;
    }

    ipTotalLen = ip->ip_len;
    tlen = ipTotalLen - (u32)iphlen;
    off = (int)((th->th_offx2 >> 4) * 4);
    if (off < (int)sizeof(InetTcpHdr) || (u32)off > tlen) {
        g_tcpstat.tcps_rcvbadoff++;
        sceNetIfhandle_lib_F56FAC82(m);
        return;
    }
    tlen -= (u32)off;

    if (off > (int)sizeof(InetTcpHdr)) {
        if (m->m_len < iphlen + off) {
            m = sceNetIfhandle_lib_49EDBB18(m, iphlen + off);
            if (m == NULL) {
                g_tcpstat.tcps_rcvshort++;
                return;
            }
            ip = (InetIpHdr *)m->m_data;
            th = (InetTcpHdr *)(m->m_data + iphlen);
        }
        optlen = off - (int)sizeof(InetTcpHdr);
        optp = (const u8 *)th + sizeof(InetTcpHdr);
        if ((optlen == 12 || (optlen > 12 && optp[12] == 0)) &&
            inetReadU32At(optp, 0) == inetBswap32(0x0101080AU)) {
            if ((th->th_flags & 2) == 0) {
                opti.ts_present = 1;
                opti.ts_val = inetBswap32(inetReadU32At(optp, 4));
                opti.ts_ecr = inetBswap32(inetReadU32At(optp, 8));
                optp = NULL;
            }
        }
    }
    tiflags = th->th_flags;

    while (findPcb) {
        int hdrDrop;
        int ourfinisacked = 0;

        findPcb = 0;
        inp = inetInPcbLookupConnect(&g_tcbtable, ip->ip_src, th->th_sport, ip->ip_dst, th->th_dport);
        if (inp == NULL) {
            g_tcpstat.tcps_pcbhashmiss++;
            inp = inetInPcbLookupBind(&g_tcbtable, ip->ip_dst, th->th_dport);
            if (inp == NULL) {
                g_tcpstat.tcps_noport++;
                if (g_tcp_log_refused != 0 && (tiflags & 2) != 0) {
                    char srcBuf[16];
                    char dstBuf[16];
                    if (ip != NULL) {
                        sceNet_lib_80C9F02A(srcBuf, inetNtoaStatic(ip->ip_src));
                        sceNet_lib_80C9F02A(dstBuf, inetNtoaStatic(ip->ip_dst));
                    } else {
                        sceNet_lib_80C9F02A(srcBuf, "(unknown)");
                        sceNet_lib_80C9F02A(dstBuf, "(unknown)");
                    }
                }
                th->th_seq = inetBswap32(th->th_seq);
                th->th_ack = inetBswap32(th->th_ack);
                th->th_win = inetBswap16(th->th_win);
                th->th_urp = inetBswap16(th->th_urp);
                dropWithReset = 1;
                break;
            }
        }

        tp = (InetTcpcb *)inp->inp_ppcb;
        so = inp->inp_socket;
        if (tp == NULL) {
            th->th_seq = inetBswap32(th->th_seq);
            th->th_ack = inetBswap32(th->th_ack);
            th->th_win = inetBswap16(th->th_win);
            th->th_urp = inetBswap16(th->th_urp);
            dropWithReset = 1;
            break;
        }
        if (tp->t_state == 0) {
            dropOnly = 1;
            break;
        }

        if ((tp->t_flags & 0x10000) != 0) {
            p2pPort = (u16)p2pSrcPort;
        } else {
            sceNet_lib_8B615F52(ip, 9);
            ip->ip_sum = inetBswap16((u16)(tlen + (u32)off));
            if (inetInCksum(m, (s32)ipTotalLen) != 0) {
                g_tcpstat.tcps_rcvbadsum++;
                dropOnly = 1;
                break;
            }
        }

        th->th_seq = inetBswap32(th->th_seq);
        th->th_ack = inetBswap32(th->th_ack);
        th->th_win = inetBswap16(th->th_win);
        th->th_urp = inetBswap16(th->th_urp);

        if ((tiflags & 2) != 0) {
            tiwin = th->th_win;
        } else {
            tiwin = (u32)th->th_win << tp->snd_scale;
        }

        if ((so->so_options & (INET_SO_DEBUG | INET_SO_ACCEPTCONN)) != 0) {
            InetSockaddrIn srcSin;
            InetSockaddrIn dstSin;

            sceNet_lib_8B615F52(&srcSin, sizeof(srcSin));
            sceNet_lib_8B615F52(&dstSin, sizeof(dstSin));
            srcSin.sin_len = sizeof(InetSockaddrIn);
            srcSin.sin_family = INET_AF_INET;
            srcSin.sin_addr = ip->ip_src;
            srcSin.sin_port = th->th_sport;
            dstSin.sin_len = sizeof(InetSockaddrIn);
            dstSin.sin_family = INET_AF_INET;
            dstSin.sin_addr = ip->ip_dst;
            dstSin.sin_port = th->th_dport;

            if ((so->so_options & INET_SO_DEBUG) != 0) {
                if (m->m_len <= 20 || (m->m_flags & INET_M_EXT) != 0) {
                    saveM = (InetMbuf *)sceNetIfhandle_lib_15CFE3C0(sizeof(InetMbuf));
                    if (saveM != NULL) {
                        saveM->m_next = NULL;
                        saveM->m_nextpkt = NULL;
                        saveM->m_data = M_PKTDAT(saveM);
                        saveM->m_type = INET_MT_HEADER;
                        saveM->m_flags = INET_M_PKTHDR;
                        M_CSUM(saveM) = 0;
                        saveM->m_len = 20;
                        sceNetIfhandle_lib_9A6261EC(m, 0, 20, saveM->m_data);
                    }
                } else {
                    saveM = sceNetIfhandle_lib_456E3146(m, 0, 20, INET_M_DONTWAIT);
                }
                if (saveM != NULL) {
                    s32 trail = ((saveM->m_flags & INET_M_EXT) != 0)
                                    ? (s32)((saveM->m_ext.ext_buf + saveM->m_ext.ext_size) - (saveM->m_data + saveM->m_len))
                                    : (s32)(((u8 *)saveM + sizeof(InetMbuf)) - (saveM->m_data + saveM->m_len));
                    if (trail >= 20) {
                        sceNet_lib_2F305274(th, saveM->m_data + 20, 20);
                        saveM->m_len += 20;
                        saveM->m_data[0] = (u8)((saveM->m_data[0] & 0x0F) | 0x40);
                    } else {
                        sceNetIfhandle_lib_F56FAC82(saveM);
                        saveM = NULL;
                    }
                }
            }

            if ((so->so_options & INET_SO_ACCEPTCONN) != 0) {
                if ((tiflags & 0x16) == 2) {
                    if ((th->th_sport == th->th_dport && ip->ip_src == ip->ip_dst) ||
                        so->so_qlen > so->so_qlimit) {
                        if (th->th_sport == th->th_dport && ip->ip_src == ip->ip_dst) {
                            g_tcpstat.tcps_badsyn++;
                        }
                        dropOnly = 1;
                        break;
                    }
                    if (inetSynCacheAdd(&srcSin, &dstSin, th, (int)tlen, so, m, optp, optlen, &opti, p2pPort) != 0) {
                        m = NULL;
                    }
                    dropOnly = 1;
                    break;
                }
                if ((tiflags & 4) != 0) {
                    inetSynCacheReset(&srcSin, &dstSin, th);
                    dropOnly = 1;
                    break;
                }
                if ((tiflags & 0x12) == 0x12 || (tiflags & 0x10) == 0) {
                    g_tcpstat.tcps_badsyn++;
                    tp = NULL;
                    dropWithReset = 1;
                    break;
                }
                so = inetSynCacheGet(&srcSin, &dstSin, th, iphlen, (int)tlen, so, m);
                if (so == NULL) {
                    g_tcpstat.tcps_badsyn++;
                    tp = NULL;
                    dropWithReset = 1;
                    break;
                }
                if (so == (InetSocket *)-1) {
                    m = NULL;
                    dropOnly = 1;
                    break;
                }
                if (so->so_proto->pr_domain->dom_family != INET_AF_INET) {
                    g_tcpstat.tcps_badsyn++;
                    tp = NULL;
                    dropWithReset = 1;
                    break;
                }
                tp = (InetTcpcb *)((InetInpcbReal *)so->so_pcb)->inp_ppcb;
                if (tp == NULL) {
                    g_tcpstat.tcps_badsyn++;
                    dropWithReset = 1;
                    break;
                }
                tiwin <<= tp->snd_scale;
            }
        }

        tp->t_idle = 0;
        if (tp->t_state >= 4) {
            tp->t_timer[2] = g_inet_ticks + g_tcp_keepidle;
        }

        if (optp != NULL) {
            inetTcpDooptions(tp, optp, optlen, th, &opti);
        }

        /* Header prediction fast path */
        if (tp->t_state == 4 &&
            (tiflags & 0x37) == 0x10 &&
            (!opti.ts_present || tcpSeqGeq(opti.ts_val, tp->ts_recent)) &&
            th->th_seq == tp->rcv_nxt &&
            tiwin != 0 && tiwin == tp->snd_wnd &&
            tp->snd_nxt == tp->snd_max) {
            if (opti.ts_present &&
                tcpSeqLeq(th->th_seq, tp->last_ack_sent) &&
                tcpSeqLt(tp->last_ack_sent, th->th_seq + tlen)) {
                tp->ts_recent_age = g_tcp_now;
                tp->ts_recent = opti.ts_val;
            }
            if (tlen == 0) {
                if (tcpSeqGt(th->th_ack, tp->snd_una) &&
                    tcpSeqLeq(th->th_ack, tp->snd_max) &&
                    tp->snd_cwnd >= tp->snd_wnd &&
                    tp->t_dupacks < g_tcprexmtthresh) {
                    u32 acked;
                    g_tcpstat.tcps_predack++;
                    if (opti.ts_present && opti.ts_ecr != 0) {
                        inetTcpXmitTimer(tp, (s16)(g_tcp_now - opti.ts_ecr + 1));
                    } else if (tp->t_rtt != 0 && tcpSeqGt(th->th_ack, tp->t_rtseq)) {
                        inetTcpXmitTimer(tp, tp->t_rtt);
                    }
                    acked = th->th_ack - tp->snd_una;
                    g_tcpstat.tcps_rcvackpack++;
                    g_tcpstat.tcps_rcvackbyte += acked;
                    inetSbDrop(&so->so_snd, (s32)acked);
                    tp->snd_una = th->th_ack;
                    tp->snd_recover = th->th_ack;
                    sceNetIfhandle_lib_F56FAC82(m);
                    if (tp->snd_una == tp->snd_max) {
                        tp->t_timer[0] = 0;
                    } else if (tp->t_timer[1] == 0) {
                        tp->t_timer[0] = g_inet_ticks + (u32)tp->t_rxtcur;
                    }
                    if ((so->so_snd.sb_flags & 0x3C) != 0) {
                        inetSoWakeup(so, &so->so_snd);
                    }
                    if (so->so_snd.sb_cc != 0) {
                        inetTcpOutput(tp);
                    }
                    if (saveM != NULL) {
                        sceNetIfhandle_lib_F56FAC82(saveM);
                    }
                    return;
                }
            } else if (th->th_ack == tp->snd_una && tp->segq_first == NULL) {
                s32 spaceA = (s32)(so->so_rcv.sb_mbmax - so->so_rcv.sb_mbcnt);
                s32 spaceB = (s32)(so->so_rcv.sb_hiwat - so->so_rcv.sb_cc);
                s32 rcvSpace = (spaceA < spaceB) ? spaceA : spaceB;
                if (rcvSpace >= (s32)tlen) {
                    g_tcpstat.tcps_preddat++;
                    tp->rcv_nxt += tlen;
                    g_tcpstat.tcps_rcvpack++;
                    g_tcpstat.tcps_rcvbyte += tlen;
                    if ((so->so_state & INET_SS_CANTRCVMORE) != 0) {
                        sceNetIfhandle_lib_F56FAC82(m);
                    } else {
                        sceNetIfhandle_lib_BFF3CEA5(m, iphlen + off);
                        inetSbAppend(&so->so_rcv, m);
                    }
                    if ((so->so_rcv.sb_flags & 0x3C) != 0) {
                        inetSoWakeup(so, &so->so_rcv);
                    }
                    if ((tp->t_flags & 2) != 0 || (g_tcp_ack_on_push != 0 && (th->th_flags & 8) != 0)) {
                        tp->t_flags |= 1;
                    } else {
                        tcpDelackSchedule(tp);
                    }
                    if ((tp->t_flags & 1) != 0) {
                        inetTcpOutput(tp);
                    }
                    if (saveM != NULL) {
                        sceNetIfhandle_lib_F56FAC82(saveM);
                    }
                    return;
                }
            }
        }

        /* Compute receive window */
        {
            s32 spaceA = (s32)(so->so_rcv.sb_mbmax - so->so_rcv.sb_mbcnt);
            s32 spaceB = (s32)(so->so_rcv.sb_hiwat - so->so_rcv.sb_cc);
            s32 win = (spaceA < spaceB) ? spaceA : spaceB;
            s32 adv;
            if (win < 0) {
                win = 0;
            }
            adv = (s32)(tp->rcv_adv - tp->rcv_nxt);
            tp->rcv_wnd = (u32)((adv > win) ? adv : win);
        }

        hdrDrop = iphlen + off;

        /* SYN_SENT state handling */
        if (tp->t_state == 2) {
            if ((tiflags & 0x10) != 0 && (tcpSeqLeq(th->th_ack, tp->iss) || tcpSeqGt(th->th_ack, tp->snd_max))) {
                dropWithReset = 1;
                break;
            }
            if ((tiflags & 4) != 0) {
                if ((tiflags & 0x10) != 0) {
                    inetTcpDrop(tp, INET_ECONNREFUSED);
                }
                dropOnly = 1;
                break;
            }
            if ((tiflags & 2) == 0) {
                dropOnly = 1;
                break;
            }
            if ((tiflags & 0x10) != 0) {
                tp->snd_una = th->th_ack;
                tp->snd_recover = th->th_ack;
                if (tcpSeqLt(tp->snd_nxt, tp->snd_una)) {
                    tp->snd_nxt = tp->snd_una;
                }
                tp->t_timer[0] = 0;
            }
            tp->irs = th->th_seq;
            tp->rcv_nxt = th->th_seq + 1;
            tp->rcv_adv = tp->rcv_nxt;
            tp->t_flags |= 1;
            inetTcpMssFromPeer(tp, opti.maxseg);

            if ((tp->t_flags & 0x400) != 0) {
                tp->snd_cwnd = tp->t_peermss;
            } else if (g_tcp_init_win != 0) {
                tp->snd_cwnd = (u32)tp->t_peermss * (u32)g_tcp_init_win;
            } else {
                u32 cwndLo = (2U * tp->t_peermss > 4380U) ? (2U * tp->t_peermss) : 4380U;
                u32 cwndHi = 4U * tp->t_peermss;
                tp->snd_cwnd = (cwndHi < cwndLo) ? cwndHi : cwndLo;
            }
            inetTcpRttFromRt(tp);

            if ((tiflags & 0x10) != 0) {
                int spl;
                g_tcpstat.tcps_connects++;
                inetSoIsConnected(so);
                inetTcpEstablished(tp);
                if ((tp->t_flags & 0x60) == 0x60) {
                    tp->snd_scale = tp->requested_s_scale;
                    tp->rcv_scale = tp->request_r_scale;
                }
                spl = sceNet_lib_94B44F26();
                tp->t_flags |= 0x4000;
                sceNet_lib_515B2F33(spl);
                inetTcpReass(tp, NULL, NULL, &tlen);
                spl = sceNet_lib_94B44F26();
                tp->t_flags &= ~0x4000U;
                sceNet_lib_515B2F33(spl);
                if (tp->t_rtt != 0) {
                    inetTcpXmitTimer(tp, tp->t_rtt);
                }
            } else {
                tp->t_state = 3;
            }

            th->th_seq++;
            if (tlen > tp->rcv_wnd) {
                u32 excess = tlen - tp->rcv_wnd;
                sceNetIfhandle_lib_BFF3CEA5(m, -(s32)excess);
                tlen = tp->rcv_wnd;
                tiflags &= (u8)~1;
                g_tcpstat.tcps_rcvpackafterwin++;
                g_tcpstat.tcps_rcvbyteafterwin += excess;
            }
            tp->snd_wl_seq = th->th_seq - 1;
            tp->rcv_up = th->th_seq;
        } else {
            /* Non-SYN_SENT states */
            if (tp->t_state == 1) {
                if ((m->m_flags & (INET_M_BCAST | INET_M_MCAST)) != 0 ||
                    (ip->ip_dst & inetBswap32(0xF0000000U)) == inetBswap32(0xE0000000U) ||
                    inetInBroadcast(ip->ip_dst, M_RCVIF(m))) {
                    dropOnly = 1;
                    break;
                }
            } else if (tp->t_state == 3 && (tiflags & 0x10) != 0) {
                if (tcpSeqLeq(th->th_ack, tp->iss) || tcpSeqGt(th->th_ack, tp->snd_max)) {
                    dropWithReset = 1;
                    break;
                }
            }

            /* PAWS check */
            if (opti.ts_present && (tiflags & 4) == 0 && tp->ts_recent != 0 &&
                tcpSeqLt(opti.ts_val, tp->ts_recent)) {
                if ((s32)(g_tcp_now - tp->ts_recent_age) > 4147200) {
                    tp->ts_recent = 0;
                } else {
                    g_tcpstat.tcps_rcvduppack++;
                    g_tcpstat.tcps_rcvdupbyte += tlen;
                    g_tcpstat.tcps_pawsdrop++;
                    dropAfterAck = 1;
                    break;
                }
            }

            /* Trim bytes before rcv_nxt */
            {
                s32 toDrop = (s32)(tp->rcv_nxt - th->th_seq);
                if (toDrop > 0) {
                    if ((tiflags & 2) != 0) {
                        tiflags &= (u8)~2;
                        th->th_seq++;
                        if (th->th_urp > 1) {
                            th->th_urp--;
                        } else {
                            tiflags &= (u8)~0x20;
                            th->th_urp = 0;
                        }
                        toDrop--;
                    }
                    if (toDrop > (s32)tlen || (toDrop == (s32)tlen && (tiflags & 1) == 0)) {
                        g_tcpstat.tcps_rcvduppack++;
                        g_tcpstat.tcps_rcvdupbyte += tlen;
                        toDrop = (s32)tlen;
                        tiflags &= (u8)~1;
                        tp->t_flags |= 1;
                    } else {
                        g_tcpstat.tcps_rcvpartduppack++;
                        g_tcpstat.tcps_rcvpartdupbyte += (u32)toDrop;
                    }
                    hdrDrop += toDrop;
                    th->th_seq += (u32)toDrop;
                    tlen -= (u32)toDrop;
                    if (th->th_urp > (u16)toDrop) {
                        th->th_urp -= (u16)toDrop;
                    } else {
                        tiflags &= (u8)~0x20;
                        th->th_urp = 0;
                    }
                }
            }

            if ((so->so_state & INET_SS_NOFDREF) != 0 && tp->t_state > 5 && tlen != 0) {
                tp = inetTcpClose(tp);
                g_tcpstat.tcps_rcvafterclose++;
                dropWithReset = 1;
                break;
            }

            /* Trim bytes past rcv_nxt + rcv_wnd */
            {
                s32 toDrop = (s32)((th->th_seq + tlen) - (tp->rcv_nxt + tp->rcv_wnd));
                if (toDrop > 0) {
                    g_tcpstat.tcps_rcvpackafterwin++;
                    if (toDrop >= (s32)tlen) {
                        g_tcpstat.tcps_rcvbyteafterwin += tlen;
                        if ((tiflags & 2) != 0 && tp->t_state == 10 && tcpSeqGt(th->th_seq, tp->rcv_nxt)) {
                            inetTcpNewIss(tp, sizeof(InetTcpcb), tp->snd_nxt);
                            tp = inetTcpClose(tp);
                            findPcb = 1;
                            continue;
                        }
                        if (tp->rcv_wnd == 0 && th->th_seq == tp->rcv_nxt) {
                            tp->t_flags |= 1;
                            g_tcpstat.tcps_rcvwinprobe++;
                        } else {
                            dropAfterAck = 1;
                            break;
                        }
                    } else {
                        g_tcpstat.tcps_rcvbyteafterwin += (u32)toDrop;
                    }
                    sceNetIfhandle_lib_BFF3CEA5(m, -toDrop);
                    tlen -= (u32)toDrop;
                    tiflags &= (u8)~9;
                }
            }

            if (opti.ts_present &&
                tcpSeqGeq(opti.ts_val, tp->ts_recent) &&
                tcpSeqLeq(th->th_seq, tp->last_ack_sent) &&
                tcpSeqLt(tp->last_ack_sent, th->th_seq + tlen + (((tiflags & 3) != 0) ? 1U : 0U))) {
                tp->ts_recent_age = g_tcp_now;
                tp->ts_recent = opti.ts_val;
            }

            if ((tiflags & 4) != 0) {
                switch (tp->t_state) {
                case 3:
                    so->so_error = INET_ECONNREFUSED;
                    tp->t_state = 0;
                    g_tcpstat.tcps_drops++;
                    inetTcpClose(tp);
                    dropOnly = 1;
                    break;
                case 4:
                case 5:
                case 6:
                case 9:
                    so->so_error = INET_ECONNRESET;
                    tp->t_state = 0;
                    g_tcpstat.tcps_drops++;
                    inetTcpClose(tp);
                    dropOnly = 1;
                    break;
                case 7:
                case 8:
                case 10:
                    inetTcpClose(tp);
                    dropOnly = 1;
                    break;
                default:
                    break;
                }
                if (dropOnly) {
                    break;
                }
            }

            if ((tiflags & 2) != 0) {
                tp = inetTcpDrop(tp, INET_ECONNRESET);
                dropWithReset = 1;
                break;
            }

            if ((tiflags & 0x10) == 0) {
                if ((tp->t_flags & 1) != 0) {
                    dropAfterAck = 1;
                } else {
                    dropOnly = 1;
                }
                break;
            }

            /* ACK processing */
            if (tp->t_state == 3) {
                int spl;
                if (tcpSeqGt(tp->snd_una, th->th_ack) || tcpSeqGt(th->th_ack, tp->snd_max)) {
                    dropWithReset = 1;
                    break;
                }
                g_tcpstat.tcps_connects++;
                inetSoIsConnected(so);
                inetTcpEstablished(tp);
                if ((tp->t_flags & 0x60) == 0x60) {
                    tp->snd_scale = tp->requested_s_scale;
                    tp->rcv_scale = tp->request_r_scale;
                }
                spl = sceNet_lib_94B44F26();
                tp->t_flags |= 0x4000;
                sceNet_lib_515B2F33(spl);
                inetTcpReass(tp, NULL, NULL, &tlen);
                spl = sceNet_lib_94B44F26();
                tp->t_flags &= ~0x4000U;
                sceNet_lib_515B2F33(spl);
                tp->snd_wl_seq = th->th_seq - 1;
            }

            if (tp->t_state >= 3 && tp->t_state <= 10) {
                if (tcpSeqLeq(th->th_ack, tp->snd_una)) {
                    if (tlen == 0 && tiwin == tp->snd_wnd) {
                        g_tcpstat.tcps_rcvdupack++;
                        if (tp->t_timer[0] == 0 || th->th_ack != tp->snd_una) {
                            tp->t_dupacks = 0;
                        } else {
                            tp->t_dupacks++;
                            if (tp->t_dupacks == g_tcprexmtthresh) {
                                u32 onxt = tp->snd_nxt;
                                u32 win = (tiwin < tp->snd_cwnd) ? tiwin : tp->snd_cwnd;
                                u32 segsz = tp->t_segsz;
                                if (g_tcp_do_newreno != 0 && tcpSeqLt(th->th_ack, tp->snd_recover)) {
                                    tp->snd_cwnd += segsz;
                                    tp->t_dupacks = 0;
                                    inetTcpOutput(tp);
                                    dropOnly = 1;
                                    break;
                                }
                                {
                                    u32 half = (win >> 1) / segsz;
                                    if (half < 2) {
                                        half = 2;
                                    }
                                    tp->snd_ssthresh = half * segsz;
                                    tp->t_timer[0] = 0;
                                    tp->t_rtt = 0;
                                    tp->snd_recover = tp->snd_max;
                                    tp->snd_nxt = th->th_ack;
                                    tp->snd_cwnd = segsz;
                                    inetTcpOutput(tp);
                                    tp->snd_cwnd = tp->snd_ssthresh + segsz * (u32)tp->t_dupacks;
                                    if (tcpSeqGt(onxt, tp->snd_nxt)) {
                                        tp->snd_nxt = onxt;
                                    }
                                    dropOnly = 1;
                                    break;
                                }
                            } else if (tp->t_dupacks > g_tcprexmtthresh) {
                                tp->snd_cwnd += tp->t_segsz;
                                inetTcpOutput(tp);
                                dropOnly = 1;
                                break;
                            }
                        }
                    } else {
                        tp->t_dupacks = 0;
                    }
                } else {
                    /* New data acknowledged */
                    u32 acked;
                    if (g_tcp_do_newreno != 0 && tp->t_dupacks >= g_tcprexmtthresh) {
                        if (inetTcpNewrenoPartialAck(tp, th) == 0) {
                            u32 ssthresh = tp->snd_ssthresh;
                            u32 flight = tp->snd_max - th->th_ack;
                            tp->snd_cwnd = (flight < ssthresh) ? (flight + tp->t_segsz) : ssthresh;
                            tp->t_dupacks = 0;
                        }
                    } else if (tp->t_dupacks >= g_tcprexmtthresh) {
                        if (tp->snd_ssthresh < tp->snd_cwnd) {
                            tp->snd_cwnd = tp->snd_ssthresh;
                        }
                        tp->t_dupacks = 0;
                    }

                    if (tcpSeqGt(th->th_ack, tp->snd_max)) {
                        g_tcpstat.tcps_rcvacktoomuch++;
                        dropAfterAck = 1;
                        break;
                    }

                    acked = th->th_ack - tp->snd_una;
                    g_tcpstat.tcps_rcvackpack++;
                    g_tcpstat.tcps_rcvackbyte += acked;

                    if (opti.ts_present && opti.ts_ecr != 0) {
                        inetTcpXmitTimer(tp, (s16)(g_tcp_now - opti.ts_ecr + 1));
                    } else if (tp->t_rtt != 0 && tcpSeqGt(th->th_ack, tp->t_rtseq)) {
                        inetTcpXmitTimer(tp, tp->t_rtt);
                    }

                    if (th->th_ack == tp->snd_max) {
                        tp->t_timer[0] = 0;
                        needOutput = 1;
                    } else if (tp->t_timer[1] == 0) {
                        tp->t_timer[0] = g_inet_ticks + (u32)tp->t_rxtcur;
                    }

                    if (g_tcp_do_newreno == 0 || tcpSeqGeq(th->th_ack, tp->snd_recover)) {
                        u32 cw = tp->snd_cwnd;
                        u32 incr = tp->t_segsz;
                        u32 maxCw = 0xFFFFU << tp->snd_scale;
                        if (cw > tp->snd_ssthresh) {
                            incr = (incr * incr) / cw;
                        }
                        tp->snd_cwnd = (cw + incr < maxCw) ? (cw + incr) : maxCw;
                    }

                    if (acked > so->so_snd.sb_cc) {
                        tp->snd_wnd -= so->so_snd.sb_cc;
                        inetSbDrop(&so->so_snd, (s32)so->so_snd.sb_cc);
                        ourfinisacked = 1;
                    } else {
                        inetSbDrop(&so->so_snd, (s32)acked);
                        tp->snd_wnd -= acked;
                        ourfinisacked = 0;
                    }

                    if ((so->so_snd.sb_flags & 0x3C) != 0) {
                        inetSoWakeup(so, &so->so_snd);
                    }

                    tp->snd_una = th->th_ack;
                    tp->snd_recover = th->th_ack;
                    if (tcpSeqLt(tp->snd_nxt, tp->snd_una)) {
                        tp->snd_nxt = tp->snd_una;
                    }

                    if (tp->t_state == 6) {
                        if (ourfinisacked) {
                            if ((so->so_state & INET_SS_CANTRCVMORE) != 0) {
                                inetSoIsAborted(so);
                                if (g_tcp_maxidle > 0) {
                                    tp->t_timer[3] = g_inet_ticks + (u32)g_tcp_maxidle;
                                }
                            }
                            tp->t_state = 9;
                        }
                    } else if (tp->t_state == 7) {
                        if (ourfinisacked) {
                            tp->t_state = 10;
                            inetTcpCancelTimers(tp);
                            tp->t_timer[3] = g_inet_ticks + 120;
                            inetSoIsAborted(so);
                        }
                    } else if (tp->t_state == 8) {
                        if (ourfinisacked) {
                            inetTcpClose(tp);
                            dropOnly = 1;
                            break;
                        }
                    } else if (tp->t_state == 10) {
                        tp->t_timer[3] = g_inet_ticks + 120;
                        dropAfterAck = 1;
                        break;
                    }
                }
            }
        }

        /* Window update check */
        if ((tiflags & 0x10) != 0 &&
            (tcpSeqLt(tp->snd_wl_seq, th->th_seq) ||
             (tp->snd_wl_seq == th->th_seq &&
              (tcpSeqLt(tp->snd_wl_ack, th->th_ack) ||
               (tp->snd_wl_ack == th->th_ack && tiwin > tp->snd_wnd))))) {
            if (tlen == 0 && tp->snd_wl_ack == th->th_ack && tiwin > tp->snd_wnd) {
                g_tcpstat.tcps_rcvwinupd++;
            }
            tp->snd_wnd = tiwin;
            tp->snd_wl_seq = th->th_seq;
            tp->snd_wl_ack = th->th_ack;
            if (tp->snd_wnd > tp->max_sndwnd) {
                tp->max_sndwnd = tp->snd_wnd;
            }
            needOutput = 1;
        }

        /* Urgent data processing */
        if ((tiflags & 0x20) != 0 && th->th_urp != 0 &&
            tp->t_state != 5 && (tp->t_state < 7 || tp->t_state == 9)) {
            if (th->th_urp + so->so_rcv.sb_cc > 0x40000U) {
                th->th_urp = 0;
                tiflags &= (u8)~0x20;
            } else {
                if (tcpSeqGt(th->th_seq + th->th_urp, tp->rcv_up)) {
                    s32 oobMark;
                    tp->rcv_up = th->th_seq + th->th_urp;
                    oobMark = (s32)(so->so_rcv.sb_cc + (tp->rcv_up - tp->rcv_nxt) - 1);
                    so->so_oobmark = oobMark;
                    if (oobMark == 0) {
                        so->so_state |= INET_SS_RCVATMARK;
                    }
                    inetSoSelWakeup(so);
                    tp->t_oobflags &= (u8)~3;
                }
                if (th->th_urp <= tlen && (so->so_options & INET_SO_OOBINLINE) == 0) {
                    inetTcpPulloutofband(so, th, m, hdrDrop);
                }
            }
        } else if (tcpSeqGt(tp->rcv_nxt, tp->rcv_up)) {
            tp->rcv_up = tp->rcv_nxt;
        }

        /* Payload & FIN delivery */
        {
            int gotFin = 0;
            if ((tlen != 0 || (tiflags & 1) != 0) &&
                tp->t_state != 5 && (tp->t_state < 7 || tp->t_state == 9)) {
                int spl = sceNet_lib_94B44F26();
                tp->t_flags |= 0x4000;
                sceNet_lib_515B2F33(spl);

                if (th->th_seq == tp->rcv_nxt && tp->segq_first == NULL && tp->t_state == 4) {
                    if ((tp->t_flags & 2) != 0 || (g_tcp_ack_on_push != 0 && (th->th_flags & 8) != 0)) {
                        tp->t_flags |= 1;
                    } else {
                        tcpDelackSchedule(tp);
                    }
                    tp->rcv_nxt += tlen;
                    gotFin = (int)(th->th_flags & 1);
                    g_tcpstat.tcps_rcvpack++;
                    g_tcpstat.tcps_rcvbyte += tlen;
                    if ((so->so_state & INET_SS_CANTRCVMORE) != 0) {
                        sceNetIfhandle_lib_F56FAC82(m);
                    } else {
                        sceNetIfhandle_lib_BFF3CEA5(m, hdrDrop);
                        inetSbAppend(&so->so_rcv, m);
                    }
                    if ((so->so_rcv.sb_flags & 0x3C) != 0) {
                        inetSoWakeup(so, &so->so_rcv);
                    }
                } else {
                    sceNetIfhandle_lib_BFF3CEA5(m, hdrDrop);
                    gotFin = inetTcpReass(tp, th, m, &tlen) & 1;
                    tp->t_flags |= 1;
                }

                spl = sceNet_lib_94B44F26();
                tp->t_flags &= ~0x4000U;
                sceNet_lib_515B2F33(spl);
            } else {
                sceNetIfhandle_lib_F56FAC82(m);
            }

            if (gotFin && tp->t_state >= 4) {
                if (tp->t_state != 5 && (tp->t_state < 7 || tp->t_state == 9)) {
                    inetSoCantRcvMore(so);
                    tp->t_flags |= 1;
                    tp->rcv_nxt++;
                }
                switch (tp->t_state) {
                case 4:
                    tp->t_state = 5;
                    break;
                case 6:
                    tp->t_state = 7;
                    break;
                case 9:
                    tp->t_state = 10;
                    inetTcpCancelTimers(tp);
                    tp->t_timer[3] = g_inet_ticks + 120;
                    inetSoIsAborted(so);
                    break;
                case 10:
                    tp->t_timer[3] = g_inet_ticks + 120;
                    break;
                default:
                    break;
                }
            }
        }

        if (needOutput || (tp->t_flags & 1) != 0) {
            inetTcpOutput(tp);
        }
        if (saveM != NULL) {
            sceNetIfhandle_lib_F56FAC82(saveM);
        }
        return;
    }

    if (dropAfterAck) {
        if ((tiflags & 4) == 0) {
            sceNetIfhandle_lib_F56FAC82(m);
            tp->t_flags |= 1;
            inetTcpOutput(tp);
            if (saveM != NULL) {
                sceNetIfhandle_lib_F56FAC82(saveM);
            }
            return;
        }
        dropOnly = 1;
    }

    if (dropWithReset) {
        if ((tiflags & 4) == 0 &&
            (ip->ip_dst & inetBswap32(0xF0000000U)) != inetBswap32(0xE0000000U) &&
            !inetInBroadcast(ip->ip_dst, M_RCVIF(m))) {
            m->m_data[0] = (u8)((m->m_data[0] & 0x0F) | 0x40);
            if ((tiflags & 0x10) != 0) {
                inetTcpRespond(tp, m, m, th, 0, th->th_ack, 4);
            } else {
                if ((tiflags & 2) != 0) {
                    tlen++;
                }
                inetTcpRespond(tp, m, m, th, th->th_seq + tlen, 0, 20);
            }
            if (saveM != NULL) {
                sceNetIfhandle_lib_F56FAC82(saveM);
            }
            return;
        }
        dropOnly = 1;
    }

    if (dropOnly) {
        if (saveM != NULL) {
            sceNetIfhandle_lib_F56FAC82(saveM);
        }
        if (m != NULL) {
            sceNetIfhandle_lib_F56FAC82(m);
        }
    }
}

/* [349] 0x0237C4 */
static INET_NOINLINE void inetTcpDooptions(InetTcpcb *tp, const u8 *cp, int cnt,
                                           InetTcpHdr *th, InetTcpOptInfo *oi)
{
    while (cnt > 0) {
        u8 opt = cp[0];
        int optlen = 1;

        if (opt == 0) {
            break;
        }
        if (opt != 1) {
            if (cnt < 2) {
                break;
            }
            optlen = cp[1];
            if (optlen < 2 || optlen > cnt) {
                break;
            }
        }

        switch (opt) {
        case 2: /* MAXSEG */
            if (optlen == 4 && (th->th_flags & 2) != 0) {
                u16 mss;
                sceNet_lib_2F305274(cp + 2, &mss, 2);
                oi->maxseg = inetBswap16(mss);
            }
            break;
        case 3: /* WINDOW */
            if (optlen == 3 && (th->th_flags & 2) != 0) {
                u8 scale = cp[2];
                tp->t_flags |= 0x40;
                tp->requested_s_scale = (scale > 14) ? 14 : scale;
            }
            break;
        case 4: /* SACK_PERMITTED */
            if (optlen == 2 && (th->th_flags & 2) != 0) {
                tp->t_flags &= ~0x1000U;
            }
            break;
        case 5: /* SACK */
            if ((tp->t_flags & 0x2000) == 0 && (optlen % 8) == 2 && optlen >= 10) {
                const u8 *sp = cp + 2;
                int rem = optlen - 2;
                while (rem > 0) {
                    u32 leftEdge;
                    u32 rightEdge;
                    sceNet_lib_2F305274(sp, &leftEdge, 4);
                    leftEdge = inetBswap32(leftEdge);
                    sceNet_lib_2F305274(sp, &rightEdge, 4);
                    rightEdge = inetBswap32(rightEdge);
                    (void)leftEdge;
                    (void)rightEdge;
                    rem -= 8;
                    sp -= 8;
                }
                optlen = rem;
            }
            break;
        case 8: /* TIMESTAMP */
            if (optlen == 10) {
                oi->ts_present = 1;
                sceNet_lib_2F305274(cp + 2, &oi->ts_val, 4);
                oi->ts_val = inetBswap32(oi->ts_val);
                sceNet_lib_2F305274(cp + 6, &oi->ts_ecr, 4);
                oi->ts_ecr = inetBswap32(oi->ts_ecr);
                if ((th->th_flags & 2) != 0) {
                    tp->t_flags |= 0x100;
                    tp->ts_recent = oi->ts_val;
                    tp->ts_recent_age = g_tcp_now;
                }
            }
            break;
        default:
            break;
        }
        cnt -= optlen;
        cp += optlen;
    }
}

/* [350] 0x023A48 */
static INET_NOINLINE void inetTcpPulloutofband(InetSocket *so, InetTcpHdr *th, InetMbuf *m, int off)
{
    int cnt = off + (int)th->th_urp - 1;

    while (cnt >= 0 && m != NULL) {
        if (m->m_len > cnt) {
            InetTcpcb *tp = (InetTcpcb *)((InetInpcbReal *)so->so_pcb)->inp_ppcb;
            u8 *cp = m->m_data + cnt;
            tp->t_iobc = *cp;
            tp->t_oobflags |= 1;
            sceNet_lib_2F305274(cp + 1, cp, m->m_len - cnt - 1);
            m->m_len--;
            return;
        }
        cnt -= m->m_len;
        m = m->m_next;
    }
}

/* [351] 0x023AEC */
static INET_NOINLINE void inetTcpXmitTimer(InetTcpcb *tp, s16 rtt)
{
    s16 delta;
    s32 rto;
    s32 minRto;

    g_tcpstat.tcps_rttupdated++;
    rtt--;
    if (tp->t_srtt != 0) {
        delta = (s16)(rtt * 4 - (tp->t_srtt >> 3));
        tp->t_srtt = (s16)(tp->t_srtt + delta);
        if (tp->t_srtt <= 0) {
            tp->t_srtt = 4;
        }
        if (delta < 0) {
            delta = (s16)(-delta);
        }
        delta = (s16)(delta - (tp->t_rttvar >> 2));
        tp->t_rttvar = (s16)(tp->t_rttvar + delta);
        if (tp->t_rttvar <= 0) {
            tp->t_rttvar = 4;
        }
    } else {
        tp->t_srtt = (s16)(rtt << 5);
        tp->t_rttvar = (s16)(rtt << 3);
    }

    tp->t_rtt = 0;
    tp->t_rxtshift = 0;
    rto = ((tp->t_srtt >> 3) + tp->t_rttvar) >> 2;
    minRto = (rtt + 2 >= tp->t_rttmin) ? (rtt + 2) : tp->t_rttmin;
    if (rto < minRto) {
        tp->t_rxtcur = (s16)minRto;
    } else if (rto > 128) {
        tp->t_rxtcur = 128;
    } else {
        tp->t_rxtcur = (s16)rto;
    }
    tp->t_softerror = 0;
}

/* [352] 0x023C08 */
static INET_NOINLINE int inetTcpNewrenoPartialAck(InetTcpcb *tp, InetTcpHdr *th)
{
    u32 onxt = tp->snd_nxt;
    u32 ocwnd = tp->snd_cwnd;

    if (tcpSeqLt(th->th_ack, tp->snd_recover)) {
        tp->t_timer[0] = 0;
        tp->t_rtt = 0;
        tp->snd_nxt = th->th_ack;
        tp->snd_cwnd = tp->t_segsz + (th->th_ack - tp->snd_una);
        inetTcpOutput(tp);
        tp->snd_cwnd = ocwnd;
        if (tcpSeqGt(onxt, tp->snd_nxt)) {
            tp->snd_nxt = onxt;
        }
        tp->snd_cwnd -= (th->th_ack - tp->snd_una) - tp->t_segsz;
        return 1;
    }
    return 0;
}

/* [353] 0x023CDC */
static INET_NOINLINE void inetSynCacheInsert(InetSynCache *sc, InetTcpcb *tp)
{
    u32 hash;
    u32 bucket;
    InetSynCacheHead *head;
    int spl;
    u32 rxt;

    if (g_tcp_syn_cache_count == 0) {
        InetTimeval tv;
        sceNet_lib_4B2B3416(&tv);
        g_syn_hash_secret_a = sceNet_lib_1858883D() ^ (u32)(uintptr_t)&sc;
        g_syn_hash_secret_b = sceNet_lib_1858883D() ^ tv.tv_usec;
    }

    hash = (sc->sc_src.sin_addr ^ g_syn_hash_secret_a) *
           ((((u32)sc->sc_dst.sin_port << 16) + sc->sc_src.sin_port) ^ g_syn_hash_secret_b);
    bucket = hash % (u32)g_tcp_syn_cache_size;
    sc->sc_hash = hash;
    sc->sc_bucketidx = bucket;
    head = &g_tcp_syn_cache[bucket];

    spl = sceNet_lib_94B44F26();
    if (head->sch_length >= (u16)g_tcp_syn_bucket_limit) {
        int qIdx;
        InetSynCache *victim = NULL;
        g_tcpstat.tcps_sc_bucketoverflow++;
        for (qIdx = 12; qIdx >= 0 && victim == NULL; qIdx--) {
            InetSynCache *scan = g_tcp_syn_cache_timeq[qIdx].tqh_first;
            while (scan != NULL) {
                if (scan->sc_bucketidx == bucket) {
                    victim = scan;
                    break;
                }
                scan = scan->sc_timerq_next;
            }
        }
        if (victim != NULL) {
            synCacheUnlink(victim);
            synCacheFreeEntry(victim);
        }
    } else if (g_tcp_syn_cache_count >= (u32)g_tcp_syn_cache_limit) {
        int qIdx;
        InetSynCache *victim = NULL;
        g_tcpstat.tcps_sc_overflowed++;
        for (qIdx = 12; qIdx >= 0; qIdx--) {
            if (g_tcp_syn_cache_timeq[qIdx].tqh_first != NULL) {
                victim = g_tcp_syn_cache_timeq[qIdx].tqh_first;
                break;
            }
        }
        if (victim != NULL) {
            synCacheUnlink(victim);
            synCacheFreeEntry(victim);
        }
    }

    sc->sc_rxttot = 0;
    sc->sc_rxtshift = 0;
    rxt = (u32)(6 * g_tcp_backoff[0]);
    if (rxt < 2) {
        rxt = 2;
    } else if (rxt > 128) {
        rxt = 128;
    }
    sc->sc_rxt_delta = rxt;
    sc->sc_rxtcur = g_inet_ticks + rxt;

    sc->sc_timerq_next = NULL;
    sc->sc_timerq_prev = g_tcp_syn_cache_timeq[0].tqh_last;
    *g_tcp_syn_cache_timeq[0].tqh_last = sc;
    g_tcp_syn_cache_timeq[0].tqh_last = &sc->sc_timerq_next;

    sc->sc_tpq_next = tp->t_sc_head;
    if (tp->t_sc_head != NULL) {
        tp->t_sc_head->sc_tpq_prev = &sc->sc_tpq_next;
    }
    tp->t_sc_head = sc;
    sc->sc_tpq_prev = &tp->t_sc_head;

    sc->sc_bucketq_next = head->sch_first;
    if (head->sch_first != NULL) {
        head->sch_first->sc_bucketq_prev = &sc->sc_bucketq_next;
    }
    sc->sc_bucketq_prev = &head->sch_first;
    head->sch_first = sc;
    head->sch_length++;

    g_tcp_syn_cache_count++;
    g_tcpstat.tcps_sc_added++;
    sceNet_lib_515B2F33(spl);
}

/* [354] 0x024174 */
INET_NOINLINE void inetSynCacheTimer(void)
{
    int spl = sceNet_lib_94B44F26();
    int qIdx;

    for (qIdx = 0; qIdx < 12; qIdx++) {
        InetSynCache *sc = g_tcp_syn_cache_timeq[qIdx].tqh_first;
        while (sc != NULL && sc->sc_rxtcur != 0 && g_inet_ticks >= sc->sc_rxtcur) {
            InetSynCache *next = sc->sc_timerq_next;
            sc->sc_rxttot += sc->sc_rxt_delta;
            if (sc->sc_rxttot >= 150) {
                g_tcpstat.tcps_sc_timed_out++;
                synCacheUnlink(sc);
                synCacheFreeEntry(sc);
            } else {
                u32 rxt;
                g_tcpstat.tcps_sc_retransmitted++;
                inetSynCacheRespond(sc, NULL);
                if (sc->sc_timerq_next != NULL) {
                    sc->sc_timerq_next->sc_timerq_prev = sc->sc_timerq_prev;
                } else {
                    g_tcp_syn_cache_timeq[qIdx].tqh_last = sc->sc_timerq_prev;
                }
                *sc->sc_timerq_prev = sc->sc_timerq_next;

                sc->sc_rxtshift = (u16)(qIdx + 1);
                rxt = (u32)(6 * g_tcp_backoff[sc->sc_rxtshift]);
                if (rxt < 2) {
                    rxt = 2;
                } else if (rxt > 128) {
                    rxt = 128;
                }
                sc->sc_rxt_delta = rxt;
                sc->sc_rxtcur = g_inet_ticks + rxt;
                sc->sc_timerq_next = NULL;
                sc->sc_timerq_prev = g_tcp_syn_cache_timeq[sc->sc_rxtshift].tqh_last;
                *g_tcp_syn_cache_timeq[sc->sc_rxtshift].tqh_last = sc;
                g_tcp_syn_cache_timeq[sc->sc_rxtshift].tqh_last = &sc->sc_timerq_next;
            }
            sc = next;
        }
    }

    {
        InetSynCache *sc = g_tcp_syn_cache_timeq[12].tqh_first;
        while (sc != NULL && sc->sc_rxtcur != 0 && g_inet_ticks >= sc->sc_rxtcur) {
            InetSynCache *next = sc->sc_timerq_next;
            g_tcpstat.tcps_sc_timed_out++;
            synCacheUnlink(sc);
            synCacheFreeEntry(sc);
            sc = next;
        }
    }

    sceNet_lib_515B2F33(spl);
}

/* [355] 0x0245C4 */
INET_NOINLINE void inetSynCacheCleanup(InetTcpcb *tp)
{
    int spl = sceNet_lib_94B44F26();
    InetSynCache *sc = tp->t_sc_head;

    while (sc != NULL) {
        InetSynCache *next = sc->sc_tpq_next;
        synCacheUnlink(sc);
        synCacheFreeEntry(sc);
        sc = next;
    }
    tp->t_sc_head = NULL;
    sceNet_lib_515B2F33(spl);
}

/* [356] 0x02472C */
static INET_NOINLINE InetSocket *inetSynCacheGet(InetSockaddrIn *src, InetSockaddrIn *dst,
                                                 InetTcpHdr *th, int hlen, int tlen,
                                                 InetSocket *so, InetMbuf *m)
{
    int spl = sceNet_lib_94B44F26();
    InetSynCache *sc = synCacheLookup(src, dst, NULL);
    InetSocket *newSo;
    InetInpcbReal *inp = NULL;

    (void)hlen;
    if (sc == NULL) {
        sceNet_lib_515B2F33(spl);
        return NULL;
    }

    if (th->th_ack != sc->sc_iss + 1 ||
        tcpSeqLeq(th->th_seq, sc->sc_irs) ||
        tcpSeqGt(th->th_seq, sc->sc_irs + sc->sc_win + 1)) {
        inetSynCacheRespond(sc, m);
        sceNet_lib_515B2F33(spl);
        return (InetSocket *)-1;
    }

    synCacheUnlink(sc);
    sceNet_lib_515B2F33(spl);

    newSo = inetSoNewConn(so, INET_SS_ISCONNECTED);
    if (newSo != NULL) {
        InetMbuf *addrM;
        if (newSo->so_proto->pr_domain->dom_family == INET_AF_INET) {
            inp = (InetInpcbReal *)newSo->so_pcb;
        }
        if (src->sin_family == INET_AF_INET && inp != NULL) {
            inp->inp_laddr = dst->sin_addr;
            inp->inp_lport = dst->sin_port;
            inp->inp_options = inetIpSrcroute();
            inetInPcbState(inp, 1);
            if (inp->inp_options == NULL) {
                inp->inp_options = sc->sc_ipopts;
                sc->sc_ipopts = NULL;
            }
        }
        if (inp != NULL) {
            inp->inp_route = sc->sc_route;
        }
        sc->sc_route.ro_rt = NULL;

        addrM = sceNetIfhandle_lib_A493AA5F(INET_M_DONTWAIT, INET_MT_SONAME);
        if (addrM != NULL) {
            addrM->m_len = src->sin_len;
            sceNet_lib_2F305274(src, addrM->m_data, src->sin_len);
            if (inp != NULL && inetInPcbConnect(inp, addrM) == 0) {
                InetTcpcb *listenTp = (InetTcpcb *)((InetInpcbReal *)so->so_pcb)->inp_ppcb;
                InetTcpcb *tp = (InetTcpcb *)inp->inp_ppcb;

                sceNetIfhandle_lib_F8825DC4(addrM);
                tp->t_p2p_lport = listenTp->t_p2p_lport;
                tp->t_flags |= (listenTp->t_flags & 0x70000U);
                tp->t_p2p_fport = sc->sc_p2p_port;
                if ((sc->sc_scale & 0x0F) != 0x0F) {
                    tp->requested_s_scale = (u8)((sc->sc_scale >> 4) & 0x0F);
                    tp->t_flags |= 0x40U;
                    tp->request_r_scale = (u8)(sc->sc_scale & 0x0F);
                    tp->snd_scale = (u8)((sc->sc_scale >> 4) & 0x0F);
                    tp->rcv_scale = (u8)(sc->sc_scale & 0x0F);
                }
                if ((sc->sc_flags & 2) != 0) {
                    tp->t_flags |= 0x100U;
                }
                tp->t_template = inetTcpTemplate(tp);
                if (tp->t_template == NULL) {
                    inetTcpDrop(tp, INET_ENOBUFS);
                    sceNetIfhandle_lib_F56FAC82(m);
                    synCacheFreeEntry(sc);
                    g_tcpstat.tcps_sc_aborted++;
                    return (InetSocket *)-1;
                }

                tp->t_state = 3;
                tp->t_timer[2] = (s32)(g_inet_ticks + 150U);
                tp->t_ourmss = sc->sc_ourmss;
                g_tcpstat.tcps_accepts++;
                tp->iss = sc->sc_iss;
                tp->snd_una = sc->sc_iss;
                tp->snd_nxt = sc->sc_iss;
                tp->snd_up = sc->sc_iss;
                tp->snd_recover = sc->sc_iss;
                tp->snd_max = sc->sc_iss;
                tp->irs = sc->sc_irs;
                tp->rcv_nxt = sc->sc_irs + 1U;
                tp->rcv_adv = sc->sc_irs + 1U;

                inetTcpMssFromPeer(tp, sc->sc_peermss);
                if (sc->sc_rxtshift != 0) {
                    tp->snd_cwnd = tp->t_peermss;
                } else if (g_tcp_init_win != 0) {
                    tp->snd_cwnd = (u32)tp->t_peermss * (u32)g_tcp_init_win;
                } else {
                    u32 lim = (2U * tp->t_peermss > 4380U) ? (2U * tp->t_peermss) : 4380U;
                    u32 four = 4U * tp->t_peermss;
                    tp->snd_cwnd = (four < lim) ? four : lim;
                }

                inetTcpRttFromRt(tp);
                tp->snd_wl_seq = sc->sc_irs;
                tp->rcv_up = sc->sc_irs + 1U;
                tp->snd_up = tp->snd_una;
                tp->snd_nxt = tp->iss + 1U;
                tp->snd_max = tp->iss + 1U;
                tp->t_timer[0] = (s32)(g_inet_ticks + (u32)(s32)tp->t_rxtcur);
                if (sc->sc_win > 0 && tcpSeqGt(tp->rcv_nxt + (u32)sc->sc_win, tp->rcv_adv)) {
                    tp->rcv_adv = tp->rcv_nxt + (u32)sc->sc_win;
                }
                tp->last_ack_sent = tp->rcv_nxt;
                g_tcpstat.tcps_sc_completed++;
                synCacheFreeEntry(sc);
                return newSo;
            }
            sceNetIfhandle_lib_F8825DC4(addrM);
        }
    }

    inetTcpRespond(NULL, m, m, th, th->th_seq + (u32)tlen, 0, 20);
    if (newSo != NULL) {
        inetSoAbort(newSo);
    }
    synCacheFreeEntry(sc);
    g_tcpstat.tcps_sc_aborted++;
    return (InetSocket *)-1;
}

/* [357] 0x024DFC */
static INET_NOINLINE void inetSynCacheReset(InetSockaddrIn *src, InetSockaddrIn *dst, InetTcpHdr *th)
{
    int spl = sceNet_lib_94B44F26();
    InetSynCache *sc = synCacheLookup(src, dst, NULL);

    if (sc == NULL || (u32)(th->th_seq - sc->sc_irs) >= 2U) {
        sceNet_lib_515B2F33(spl);
        return;
    }

    synCacheUnlink(sc);
    sceNet_lib_515B2F33(spl);
    g_tcpstat.tcps_sc_reset++;
    synCacheFreeEntry(sc);
}

/* [358] 0x025080 */
INET_NOINLINE void inetSynCacheUnreach(InetSockaddrIn *src, InetSockaddrIn *dst, InetTcpHdr *th)
{
    int spl = sceNet_lib_94B44F26();
    InetSynCache *sc = synCacheLookup(src, dst, NULL);

    if (sc == NULL || inetBswap32(th->th_seq) != sc->sc_iss) {
        sceNet_lib_515B2F33(spl);
        return;
    }

    if ((sc->sc_flags & 1) == 0 || sc->sc_rxtshift < 3) {
        sc->sc_flags |= 1;
        sceNet_lib_515B2F33(spl);
        return;
    }

    synCacheUnlink(sc);
    sceNet_lib_515B2F33(spl);
    g_tcpstat.tcps_sc_unreach++;
    synCacheFreeEntry(sc);
}

/* [359] 0x025324 */
static INET_NOINLINE int inetSynCacheAdd(InetSockaddrIn *src, InetSockaddrIn *dst,
                                         InetTcpHdr *th, int tlen, InetSocket *so,
                                         InetMbuf *m, const u8 *optp, int optlen,
                                         InetTcpOptInfo *oi, u16 p2pPort)
{
    InetMbuf *ipopts = NULL;
    InetTcpcb *listenTp = (InetTcpcb *)((InetInpcbReal *)so->so_pcb)->inp_ppcb;
    InetTcpcb tmpTp;
    s32 spaceA = (s32)(so->so_rcv.sb_mbmax - so->so_rcv.sb_mbcnt);
    s32 spaceB = (s32)(so->so_rcv.sb_hiwat - so->so_rcv.sb_cc);
    s32 rcvWin = (spaceA < spaceB) ? spaceA : spaceB;
    InetSynCache *sc;

    (void)tlen;
    if (rcvWin > 0xFFFF) {
        rcvWin = 0xFFFF;
    }
    if (src->sin_family == INET_AF_INET) {
        ipopts = inetIpSrcroute();
    }

    tmpTp.t_flags = (g_tcp_do_rfc1323 != 0) ? 0xA0 : 0;
    tmpTp.ts_recent = 0;
    tmpTp.requested_s_scale = 0;
    if (optp != NULL) {
        inetTcpDooptions(&tmpTp, optp, optlen, th, oi);
    }

    sc = synCacheLookup(src, dst, NULL);
    if (sc != NULL) {
        g_tcpstat.tcps_sc_dupesyn++;
        if (ipopts != NULL) {
            if (sc->sc_ipopts != NULL) {
                sceNetIfhandle_lib_F8825DC4(sc->sc_ipopts);
            }
            sc->sc_ipopts = ipopts;
        }
        sc->sc_timestamp = tmpTp.ts_recent;
        if (inetSynCacheRespond(sc, m) == 0) {
            g_tcpstat.tcps_sndacks++;
            g_tcpstat.tcps_sndtotal++;
        }
        return 1;
    }

    sc = (InetSynCache *)sceNetIfhandle_lib_15CFE3C0(sizeof(InetSynCache));
    if (sc == NULL) {
        if (ipopts != NULL) {
            sceNetIfhandle_lib_F8825DC4(ipopts);
        }
        return 0;
    }

    sceNet_lib_8B615F52(sc, sizeof(InetSynCache));
    sceNet_lib_2F305274(src, &sc->sc_src, src->sin_len);
    sceNet_lib_2F305274(dst, &sc->sc_dst, dst->sin_len);
    sc->sc_ipopts = ipopts;
    sc->sc_flags = 0;
    sc->sc_irs = th->th_seq;
    sc->sc_iss = inetTcpNewIss(sc, sizeof(InetSynCache), 0);
    sc->sc_peermss = oi->maxseg;
    sc->sc_ourmss = (u16)inetTcpMssToAdvertise(((m->m_flags & INET_M_PKTHDR) != 0) ? M_RCVIF(m) : NULL,
                                               sc->sc_src.sin_family, listenTp);
    sc->sc_win = (u32)rcvWin;
    sc->sc_timestamp = tmpTp.ts_recent;
    if (g_tcp_do_rfc1323 != 0 && (tmpTp.t_flags & 0x100) != 0) {
        sc->sc_flags |= 2;
    }
    if ((tmpTp.t_flags & 0x60) == 0x60) {
        u32 rscale = 0;
        while (rscale < 14 && (0xFFFFU << rscale) < so->so_rcv.sb_hiwat) {
            rscale++;
        }
        sc->sc_scale = (rscale & 0x0F) | (((u32)tmpTp.requested_s_scale & 0x0F) << 4);
    } else {
        sc->sc_scale = 0xFF;
    }

    sc->sc_p2p_port = p2pPort;
    sc->sc_tp = listenTp;
    if (inetSynCacheRespond(sc, m) == 0) {
        inetSynCacheInsert(sc, listenTp);
        g_tcpstat.tcps_sndacks++;
        g_tcpstat.tcps_sndtotal++;
        return 1;
    }

    synCacheFreeEntry(sc);
    g_tcpstat.tcps_sc_dropped++;
    return 1;
}

/* [360] 0x0257EC */
static INET_NOINLINE int inetSynCacheRespond(InetSynCache *sc, InetMbuf *m)
{
    u32 optlen;
    u32 totalLen;
    InetMbuf *outM;
    u32 maxLinkHdr = 0;
    InetTcpcb *tp = sc->sc_tp;
    InetSocket *so = NULL;
    InetIpHdr *ip;
    InetTcpHdr *th;
    u8 *optp;

    if (sc->sc_src.sin_family != INET_AF_INET) {
        if (m != NULL) {
            sceNetIfhandle_lib_F56FAC82(m);
        }
        return INET_EAFNOSUPPORT;
    }

    optlen = ((sc->sc_scale & 0x0F) == 0x0F) ? 4U : 8U;
    if ((sc->sc_flags & 2) != 0) {
        optlen += 12U;
    }
    totalLen = 40U + optlen;

    if (m != NULL) {
        sceNetIfhandle_lib_F56FAC82(m);
    }

    outM = (InetMbuf *)sceNetIfhandle_lib_15CFE3C0(sizeof(InetMbuf));
    if (outM == NULL) {
        return INET_ENOBUFS;
    }
    outM->m_next = NULL;
    outM->m_nextpkt = NULL;
    outM->m_data = M_PKTDAT(outM);
    outM->m_type = INET_MT_DATA;
    outM->m_flags = INET_M_PKTHDR;
    M_CSUM(outM) = 0;

    if (totalLen > 208U) {
        u8 *cl = (u8 *)sceNetIfhandle_lib_15CFE3C0(2048);
        if (cl == NULL) {
            cl = (u8 *)sceNetIfhandle_lib_15CFE3C0(2048);
        }
        if (cl != NULL) {
            outM->m_ext.ext_buf = cl;
            outM->m_data = cl;
            outM->m_flags |= (INET_M_EXT | 8);
            outM->m_ext.ext_size = 2048;
            outM->m_ext.ext_free = NULL;
            outM->m_ext.ext_arg = NULL;
            outM->m_ext.ext_nextref = outM;
            outM->m_ext.ext_prevref = outM;
        }
        if ((outM->m_flags & INET_M_EXT) == 0) {
            sceNetIfhandle_lib_F56FAC82(outM);
            return INET_ENOBUFS;
        }
    }

    sceNetIfhandle_lib_5FB31C72(&maxLinkHdr, NULL, NULL, NULL);
    outM->m_data += maxLinkHdr;
    outM->m_len = (s32)totalLen;
    M_PKTLEN(outM) = (s32)totalLen;
    M_RCVIF(outM) = NULL;

    if (tp != NULL && tp->t_inpcb != NULL) {
        so = tp->t_inpcb->inp_socket;
    }

    sceKernelMemset(outM->m_data, 0, totalLen);
    ip = (InetIpHdr *)outM->m_data;
    th = (InetTcpHdr *)(outM->m_data + sizeof(InetIpHdr));

    ip->ip_dst = sc->sc_src.sin_addr;
    ip->ip_src = sc->sc_dst.sin_addr;
    ip->ip_p = INET_IPPROTO_TCP;
    th->th_dport = sc->sc_src.sin_port;
    th->th_sport = sc->sc_dst.sin_port;
    th->th_seq = inetBswap32(sc->sc_iss);
    th->th_ack = inetBswap32(sc->sc_irs + 1);
    th->th_offx2 = (u8)(((20U + optlen) >> 2) << 4);
    th->th_flags = 0x12; /* SYN | ACK */
    th->th_win = inetBswap16((u16)sc->sc_win);

    optp = (u8 *)th + sizeof(InetTcpHdr);
    optp[0] = 2;
    optp[1] = 4;
    optp[2] = (u8)(sc->sc_ourmss >> 8);
    optp[3] = (u8)(sc->sc_ourmss & 0xFF);
    optp += 4;

    if ((sc->sc_scale & 0x0F) != 0x0F) {
        inetWriteU32At(optp, 0, inetBswap32(0x01030300U | (sc->sc_scale & 0x0F)));
        optp += 4;
    }
    if ((sc->sc_flags & 2) != 0) {
        inetWriteU32At(optp, 0, inetBswap32(0x0101080AU));
        inetWriteU32At(optp, 4, inetBswap32(g_tcp_now));
        inetWriteU32At(optp, 8, inetBswap32(sc->sc_timestamp));
    }

    ip->ip_len = inetBswap16((u16)(totalLen - sizeof(InetIpHdr)));
    th->th_sum = 0;
    if (tp == NULL || (tp->t_flags & 0x10000) == 0) {
        th->th_sum = inetInCksum(outM, (s32)totalLen);
    }

    ip->ip_len = (u16)totalLen;
    ip->ip_ttl = (u8)g_ip_defttl;

    if (tp != NULL && (tp->t_flags & 0x10000) != 0) {
        InetSockaddrIn srcSin;
        InetSockaddrIn dstSin;
        srcSin.sin_addr = ip->ip_src;
        srcSin.sin_port = tp->t_p2p_lport;
        dstSin.sin_addr = ip->ip_dst;
        dstSin.sin_port = sc->sc_p2p_port;
        sceNetIfhandle_lib_BFF3CEA5(outM, (s32)sizeof(InetIpHdr));
        return inetTcpP2pOutput(&srcSin, &dstSin, outM, &sc->sc_route, so,
                                tp->t_flags & 0x20000, tp->t_flags & 0x40000);
    }

    return inetIpOutput(outM, sc->sc_ipopts, &sc->sc_route,
                        (g_ip_mtudisc != 0) ? 0x400 : 0, NULL, NULL);
}

/* [361] 0x025CAC */
INET_NOINLINE void inetSynCacheInit(void)
{
    int idx;

    g_tcp_syn_cache_count = 0;
    for (idx = 0; idx < g_tcp_syn_cache_size; idx++) {
        g_tcp_syn_cache[idx].sch_first = NULL;
    }
    for (idx = 0; idx < 13; idx++) {
        g_tcp_syn_cache_timeq[idx].tqh_first = NULL;
        g_tcp_syn_cache_timeq[idx].tqh_last = &g_tcp_syn_cache_timeq[idx].tqh_first;
    }
}
