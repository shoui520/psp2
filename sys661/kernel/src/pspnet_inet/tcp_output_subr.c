/*
 * PSP 6.61 SceNetInet_Library (pspnet_inet.prx)
 * TCP output and subroutines (tcp_output.c / tcp_subr.c)
 */

#include "pspnet_inet_internal.h"

typedef InetTcpHdr InetTcpHdrOut;
typedef InetTcpqent InetTcpqentSubr;

static const u8 g_tcp_outflags[11] = {
    0x14, 0x00, 0x02, 0x12,
    0x10, 0x10, 0x11, 0x11,
    0x11, 0x10, 0x10
};

u32 g_tcp_iss_seq = 0;

static INET_NOINLINE void inetTcpSegsz(InetTcpcb *tp, u32 *txsegp, s32 *rxsegp);
static INET_NOINLINE int inetTcpFreeq(InetTcpcb *tp);
static INET_NOINLINE int inetTcpHdrOptLen(InetTcpcb *tp);
static INET_NOINLINE void inetTcpNotify(InetInpcbReal *inp, int err);
static INET_NOINLINE void inetTcpQuench(InetInpcbReal *inp, int err);
static INET_NOINLINE void inetTcpMtudiscCallback(u32 faddr);
static INET_NOINLINE void inetTcpMtudisc(InetInpcbReal *inp, int err);

static int tcpSeqLtOut(u32 lhs, u32 rhs) { return (s32)(lhs - rhs) < 0; }
static int tcpSeqGtOut(u32 lhs, u32 rhs) { return (s32)(lhs - rhs) > 0; }

static int tcpAllocCluster(InetMbuf *m)
{
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
    return (m->m_flags & INET_M_EXT) != 0;
}

/* [362] 0x025D04 */
INET_NOINLINE int inetTcpOutput(InetTcpcb *tp)
{
    InetInpcbReal *inp = tp->t_inpcb;
    InetSocket *so = NULL;
    InetRoute *ro = NULL;
    u32 txsegsz = 0;
    s32 rxsegsz = 0;
    int idle;
    int burstLimit = 4;

    if (inp != NULL) {
        so = inp->inp_socket;
        ro = &inp->inp_route;
    }
    if (tp->t_family != INET_AF_INET) {
        return INET_EAFNOSUPPORT;
    }
    if (inp == NULL) {
        return INET_EINVAL;
    }

    inetTcpSegsz(tp, &txsegsz, &rxsegsz);
    idle = (tp->snd_max == tp->snd_una);

    if (g_tcp_cwm != 0) {
        u32 burstCap = (u32)g_tcp_cwm_burstsize * txsegsz + (tp->snd_nxt - tp->snd_una);
        if (tp->snd_cwnd > burstCap) {
            tp->snd_cwnd = burstCap;
        }
    } else if (idle && tp->t_idle >= tp->t_rxtcur) {
        u32 initWin;
        if (g_tcp_init_win != 0) {
            initWin = txsegsz * (u32)g_tcp_init_win;
        } else {
            u32 cwLo = (2U * txsegsz > 4380U) ? (2U * txsegsz) : 4380U;
            u32 cwHi = 4U * txsegsz;
            initWin = (cwHi < cwLo) ? cwHi : cwLo;
        }
        if (tp->snd_cwnd > initWin) {
            tp->snd_cwnd = initWin;
        }
    }

    for (;;) {
        u32 win = (tp->snd_wnd < tp->snd_cwnd) ? tp->snd_wnd : tp->snd_cwnd;
        u32 off = tp->snd_nxt - tp->snd_una;
        u8 flags = g_tcp_outflags[tp->t_state];
        int sendalot = 0;
        s32 len;
        s32 rcvWin;
        int sendNow = 0;

        if (tp->t_force != 0) {
            if (win == 0) {
                if (off < so->so_snd.sb_cc) {
                    flags &= (u8)~1;
                }
                win = 1;
            } else {
                tp->t_timer[1] = 0;
                tp->t_rxtshift = 0;
            }
        }

        if (win < so->so_snd.sb_cc) {
            len = (s32)(win - off);
            flags &= (u8)~1;
        } else {
            len = (s32)(so->so_snd.sb_cc - off);
        }

        if (len < 0) {
            len = 0;
            flags &= (u8)~1;
            if (win == 0) {
                tp->t_timer[0] = 0;
                tp->t_rxtshift = 0;
                tp->snd_nxt = tp->snd_una;
                if (tp->t_timer[1] == 0) {
                    inetTcpSetPersist(tp);
                }
            }
        }

        if (len > (s32)txsegsz) {
            len = (s32)txsegsz;
            flags &= (u8)~1;
            sendalot = 1;
        }

        {
            s32 spaceA = (s32)(so->so_rcv.sb_mbmax - so->so_rcv.sb_mbcnt);
            s32 spaceB = (s32)(so->so_rcv.sb_hiwat - so->so_rcv.sb_cc);
            rcvWin = (spaceA < spaceB) ? spaceA : spaceB;
        }

        if (len != 0) {
            if (len == (s32)txsegsz) {
                sendNow = 1;
            } else if ((so->so_state & INET_SS_MORETOCOME) == 0 &&
                       (idle || (tp->t_flags & 4) != 0) &&
                       (u32)len + off >= so->so_snd.sb_cc) {
                sendNow = 1;
            } else if (tp->t_force != 0 ||
                       (u32)len >= (tp->max_sndwnd >> 1) ||
                       tcpSeqLtOut(tp->snd_nxt, tp->snd_max)) {
                sendNow = 1;
            }
        }

        if (!sendNow && rcvWin > 0) {
            u32 maxWin = 0xFFFFU << tp->rcv_scale;
            u32 effWin = ((u32)rcvWin < maxWin) ? (u32)rcvWin : maxWin;
            s32 adv = (s32)(effWin - (tp->rcv_adv - tp->rcv_nxt));
            if (adv >= 2 * rxsegsz || 2 * adv >= (s32)so->so_rcv.sb_hiwat) {
                sendNow = 1;
            }
        }

        if (!sendNow) {
            if ((tp->t_flags & 1) != 0 ||
                (flags & 7) != 0 ||
                tcpSeqGtOut(tp->snd_up, tp->snd_una)) {
                sendNow = 1;
            }
        }

        if (!sendNow) {
            if (so->so_snd.sb_cc != 0 && tp->t_timer[0] == 0 && tp->t_timer[1] == 0) {
                tp->t_rxtshift = 0;
                inetTcpSetPersist(tp);
            }
            return 0;
        }

        /* Build TCP options */
        {
            u8 optBuf[32];
            int optlen = 0;
            u32 hdrlen;
            InetMbuf *m;
            u32 maxLinkHdr = 0;
            InetIpHdr *ip;
            InetTcpHdrOut *th;
            s32 advWin;
            int outErr;

            if ((flags & 2) != 0) {
                InetRtentry *rt = (inp != NULL) ? inetInPcbRtEntry(inp) : NULL;
                InetIfnet *ifp = (rt != NULL) ? rt->rt_ifp : NULL;
                u16 mss;
                tp->snd_nxt = tp->iss;
                mss = (u16)inetTcpMssToAdvertise(ifp, INET_AF_INET, tp);
                tp->t_ourmss = mss;
                if ((tp->t_flags & 8) == 0) {
                    optBuf[0] = 2;
                    optBuf[1] = 4;
                    optBuf[2] = (u8)(mss >> 8);
                    optBuf[3] = (u8)(mss & 0xFF);
                    optlen = 4;
                    if ((tp->t_flags & 0x20) != 0 &&
                        ((flags & 0x10) == 0 || (tp->t_flags & 0x40) != 0)) {
                        inetWriteU32At(optBuf, (s32)optlen, inetBswap32(0x01030300U | tp->request_r_scale));
                        optlen += 4;
                    }
                }
            }

            if ((tp->t_flags & 0x88) == 0x80 && (flags & 4) == 0 &&
                ((flags & 0x12) == 2 || (tp->t_flags & 0x100) != 0)) {
                inetWriteU32At(optBuf, (s32)optlen, inetBswap32(0x0101080AU));
                inetWriteU32At(optBuf, (s32)optlen + 4, inetBswap32(g_tcp_now));
                inetWriteU32At(optBuf, (s32)optlen + 8, inetBswap32(tp->ts_recent));
                optlen += 12;
            }

            hdrlen = 40U + (u32)optlen;
            sceNetIfhandle_lib_5FB31C72(&maxLinkHdr, NULL, NULL, NULL);

            if (len != 0) {
                if (tp->t_force != 0 && len == 1) {
                    g_tcpstat.tcps_sndprobe++;
                } else if (tcpSeqLtOut(tp->snd_nxt, tp->snd_max)) {
                    g_tcpstat.tcps_sndrexmitpack++;
                    g_tcpstat.tcps_sndrexmitbyte += (u32)len;
                } else {
                    g_tcpstat.tcps_sndpack++;
                    g_tcpstat.tcps_sndbyte += (u32)len;
                }
            } else {
                if ((tp->t_flags & 1) != 0) {
                    g_tcpstat.tcps_sndacks++;
                } else if ((flags & 7) != 0) {
                    g_tcpstat.tcps_sndctrl++;
                } else if (tcpSeqGtOut(tp->snd_up, tp->snd_una)) {
                    g_tcpstat.tcps_sndurg++;
                } else {
                    g_tcpstat.tcps_sndwinup++;
                }
            }

            m = (InetMbuf *)sceNetIfhandle_lib_15CFE3C0(sizeof(InetMbuf));
            if (m != NULL) {
                m->m_next = NULL;
                m->m_nextpkt = NULL;
                m->m_data = M_PKTDAT(m);
                m->m_type = INET_MT_HEADER;
                m->m_flags = INET_M_PKTHDR;
                M_CSUM(m) = 0;

                if (maxLinkHdr + hdrlen > 208U || (len != 0 && maxLinkHdr + hdrlen + (u32)len <= 2048U)) {
                    if (!tcpAllocCluster(m)) {
                        sceNetIfhandle_lib_F56FAC82(m);
                        m = NULL;
                    }
                }
            }

            if (m == NULL) {
                outErr = INET_ENOBUFS;
            } else {
                m->m_len = (s32)hdrlen;
                m->m_data += maxLinkHdr;
                M_RCVIF(m) = NULL;
                outErr = 0;

                if (len != 0) {
                    s32 trail = ((m->m_flags & INET_M_EXT) != 0)
                                    ? (s32)((m->m_ext.ext_buf + m->m_ext.ext_size) - (m->m_data + hdrlen))
                                    : (s32)(((u8 *)m + sizeof(InetMbuf)) - (m->m_data + hdrlen));
                    if (trail >= len) {
                        sceNetIfhandle_lib_9A6261EC(so->so_snd.sb_mb, (s32)off, len, m->m_data + hdrlen);
                        m->m_len += len;
                    } else {
                        m->m_next = sceNetIfhandle_lib_456E3146(so->so_snd.sb_mb, (s32)off, len, INET_M_DONTWAIT);
                        if (m->m_next == NULL) {
                            sceNetIfhandle_lib_F56FAC82(m);
                            outErr = INET_ENOBUFS;
                        }
                    }
                    if (outErr == 0 && off + (u32)len == so->so_snd.sb_cc) {
                        flags |= 8;
                    }
                }
            }

            if (outErr == 0) {
                u32 seqToSend;
                u32 pktTotal = hdrlen + (u32)len;

                ip = (InetIpHdr *)m->m_data;
                th = (InetTcpHdrOut *)(m->m_data + sizeof(InetIpHdr));
                sceNet_lib_2F305274(tp->t_template->m_data, m->m_data, 40);

                if (len != 0 || (flags & 3) != 0 || tp->t_timer[1] != 0) {
                    seqToSend = tp->snd_nxt;
                } else {
                    seqToSend = tp->snd_max;
                }
                th->th_seq = inetBswap32(seqToSend);
                th->th_ack = inetBswap32(tp->rcv_nxt);

                if (optlen != 0) {
                    sceNet_lib_2F305274(optBuf, (u8 *)th + sizeof(InetTcpHdrOut), optlen);
                    th->th_offx2 = (u8)((th->th_offx2 & 0x0F) | (((20U + (u32)optlen) >> 2) << 4));
                }
                th->th_flags = flags;

                if (rcvWin < (s32)(so->so_rcv.sb_hiwat >> 2) && rcvWin < rxsegsz) {
                    rcvWin = 0;
                }
                {
                    s32 maxW = (s32)(0xFFFFU << tp->rcv_scale);
                    s32 capped = (rcvWin < maxW) ? rcvWin : maxW;
                    s32 curAdv = (s32)(tp->rcv_adv - tp->rcv_nxt);
                    advWin = (capped > curAdv) ? capped : curAdv;
                }
                th->th_win = inetBswap16((u16)((u32)advWin >> tp->rcv_scale));

                if (tcpSeqGtOut(tp->snd_up, tp->snd_nxt)) {
                    u32 urp = tp->snd_up - tp->snd_nxt;
                    if (urp > 0xFFFFU) {
                        urp = 0xFFFFU;
                    }
                    th->th_urp = inetBswap16((u16)urp);
                    th->th_flags = (u8)(flags | 0x20);
                } else {
                    tp->snd_up = tp->snd_una;
                }

                if ((tp->t_flags & 0x10000) == 0) {
                    if (len + optlen != 0) {
                        ip->ip_sum = inetBswap16((u16)(len + optlen + 20));
                    }
                    sceNet_lib_8B615F52(ip, 9);
                    th->th_sum = 0;
                    th->th_sum = inetInCksum(m, (s32)pktTotal);
                }

                if (tp->t_force == 0 || tp->t_timer[1] == 0) {
                    u32 startSeq = tp->snd_nxt;
                    if ((flags & 3) != 0) {
                        tp->snd_nxt++;
                    }
                    tp->snd_nxt += (u32)len;
                    if (tcpSeqGtOut(tp->snd_nxt, tp->snd_max)) {
                        tp->snd_max = tp->snd_nxt;
                        if (tp->t_rtt == 0) {
                            tp->t_rtt = 1;
                            tp->t_rtseq = startSeq;
                            g_tcpstat.tcps_segstimed++;
                        }
                    }
                    if (tp->t_timer[0] == 0 && tp->snd_nxt != tp->snd_una) {
                        tp->t_timer[0] = g_inet_ticks + (u32)tp->t_rxtcur;
                        if (tp->t_timer[1] != 0) {
                            tp->t_timer[1] = 0;
                            tp->t_rxtshift = 0;
                        }
                    }
                } else if (tcpSeqGtOut(tp->snd_nxt + (u32)len, tp->snd_max)) {
                    tp->snd_max = tp->snd_nxt + (u32)len;
                }

                if ((so->so_options & INET_SO_DEBUG) != 0) {
                    m->m_data[0] = (u8)((m->m_data[0] & 0x0F) | 0x40);
                }
                M_PKTLEN(m) = (s32)pktTotal;
                ip->ip_len = (u16)pktTotal;
                if (tp->t_inpcb != NULL) {
                    ip->ip_ttl = tp->t_inpcb->inp_ip.ip_ttl;
                    ip->ip_tos = tp->t_inpcb->inp_ip.ip_tos;
                }

                if ((tp->t_flags & 0x10000) != 0) {
                    InetSockaddrIn srcSin;
                    InetSockaddrIn dstSin;
                    srcSin.sin_addr = ip->ip_src;
                    srcSin.sin_port = tp->t_p2p_lport;
                    dstSin.sin_addr = ip->ip_dst;
                    dstSin.sin_port = tp->t_p2p_fport;
                    sceNetIfhandle_lib_BFF3CEA5(m, (s32)sizeof(InetIpHdr));
                    outErr = inetTcpP2pOutput(&srcSin, &dstSin, m, ro, so,
                                              tp->t_flags & 0x20000, tp->t_flags & 0x40000);
                } else {
                    InetMbuf *ipOpts = (tp->t_inpcb != NULL) ? tp->t_inpcb->inp_options : NULL;
                    int ipFlags = ((tp->t_mtudisc != 0) ? 0x400 : 0) | (so->so_options & INET_SO_DONTROUTE);
                    outErr = inetIpOutput(m, ipOpts, ro, ipFlags, NULL, NULL);
                }
            }

            if (outErr != 0) {
                if (outErr == INET_ENOBUFS) {
                    if (tp->t_inpcb != NULL) {
                        inetTcpQuench(tp->t_inpcb, 0);
                    }
                    return 0;
                }
                if ((outErr == INET_EHOSTUNREACH || outErr == INET_ENETDOWN) && tp->t_state >= 3) {
                    tp->t_softerror = (s16)outErr;
                    return 0;
                }
                return outErr;
            }

            g_tcpstat.tcps_sndtotal++;
            if ((tp->t_flags & 2) != 0) {
                g_tcpstat.tcps_delack++;
            }
            if (advWin > 0 && tcpSeqGtOut(tp->rcv_nxt + (u32)advWin, tp->rcv_adv)) {
                tp->rcv_adv = tp->rcv_nxt + (u32)advWin;
            }
            tp->last_ack_sent = tp->rcv_nxt;
            tp->t_flags &= ~1U;
            if ((tp->t_flags & 2) != 0) {
                tp->t_flags &= ~2U;
                if (tp->t_delack_next != NULL) {
                    tp->t_delack_next->t_delack_prev = tp->t_delack_prev;
                }
                *tp->t_delack_prev = tp->t_delack_next;
            }

            if (!sendalot) {
                return 0;
            }
            if (g_tcp_do_newreno != 0) {
                burstLimit--;
                if (burstLimit == 0) {
                    return 0;
                }
            }
        }
    }
}

/* [363] 0x026B98 */
INET_NOINLINE void inetTcpSetPersist(InetTcpcb *tp)
{
    s16 shift = tp->t_rxtshift;
    s32 base = ((tp->t_srtt >> 2) + tp->t_rttvar) >> 3;
    s32 tval;

    if (base < tp->t_rttmin) {
        base = tp->t_rttmin;
    }
    tval = base * g_tcp_backoff[shift];
    if (tval < 10) {
        tval = 10;
    } else if (tval > 120) {
        tval = 120;
    }
    tp->t_timer[1] = g_inet_ticks + (u32)tval;
    if (shift < 12) {
        tp->t_rxtshift = (s16)(shift + 1);
    }
}

/* [364] 0x026C1C */
static INET_NOINLINE void inetTcpSegsz(InetTcpcb *tp, u32 *txsegp, s32 *rxsegp)
{
    InetInpcbReal *inp = tp->t_inpcb;
    u32 maxMss = (u32)g_tcp_mssdflt;
    u32 optlen;
    u32 tx;
    u32 rx;

    if (tp->t_family == INET_AF_INET && inp != NULL) {
        InetRtentry *rt = inetInPcbRtEntry(inp);
        if (rt != NULL) {
            InetIfnet *ifp = rt->rt_ifp;
            if (tp->t_mtudisc != 0) {
                if (rt->rt_rmx.rmx_mtu != 0) {
                    maxMss = rt->rt_rmx.rmx_mtu - 40U;
                } else {
                    maxMss = (u32)ifp->if_data.ifi_mtu - 40U;
                }
            } else if ((ifp->if_flags & INET_IFF_LOOPBACK) != 0 ||
                       inetInLocalAddr(inp->inp_faddr)) {
                maxMss = (u32)ifp->if_data.ifi_mtu - 40U;
            }
        }
    }

    optlen = (u32)inetTcpHdrOptLen(tp);
    if (inp != NULL) {
        optlen += (u32)inetIpOptlen(inp);
    }
    if ((tp->t_flags & 0x10000) != 0) {
        optlen += 12U;
        if ((tp->t_flags & 0x20000) != 0) {
            optlen += 8U;
        }
        if ((tp->t_flags & 0x40000) != 0) {
            optlen += 8U;
        }
    }

    tx = ((u32)tp->t_peermss - optlen < maxMss - optlen) ? ((u32)tp->t_peermss - optlen) : (maxMss - optlen);
    rx = ((u32)tp->t_ourmss - optlen < maxMss - optlen) ? ((u32)tp->t_ourmss - optlen) : (maxMss - optlen);
    *txsegp = tx;
    *rxsegp = (s32)rx;

    if (tx != tp->t_segsz) {
        if ((s32)tx < (s32)tp->t_segsz) {
            u32 oldSeg = tp->t_segsz;
            u32 newCwnd = (tp->snd_cwnd / oldSeg) * tx;
            u32 newSsthresh = (tp->snd_ssthresh / oldSeg) * tx;
            tp->snd_cwnd = (newCwnd < tx) ? tx : newCwnd;
            tp->snd_ssthresh = (newSsthresh < tx) ? tx : newSsthresh;
        }
        tp->t_segsz = (u16)tx;
    }
}

/* [365] 0x026DD4 */
INET_NOINLINE void inetTcpInit(void)
{
    u32 maxLinkHdr = 0;
    u32 maxProtoHdr = 0;

    g_tcp_init_win = 1;
    g_tcp_cwm_burstsize = 4;
    g_tcp_mssdflt = 512;
    g_tcp_rttdflt = 3;
    g_tcprexmtthresh = 3;
    g_tcp_log_refused = 0;
    g_tcp_do_rfc1323 = 1;
    g_tcp_do_sack = 1;
    g_tcp_do_win_scale = 1;
    g_tcp_do_timestamps = 1;
    g_tcp_do_newreno = 0;
    g_tcp_ack_on_push = 0;
    g_tcp_mss_ifmtu = 0;
    g_tcp_compat_42 = 0;
    g_tcp_iss_seq = 0;
    g_tcp_now = 0;
    g_tcp_cwm = 0;

    sceKernelMemset(&g_tcpstat, 0, sizeof(g_tcpstat));
    inetTcpTimerInit();
    inetInPcbInit(&g_tcbtable, (int)g_tcbhashsize, (int)g_tcbhashsize);
    g_tcp_delack_head = NULL;

    sceNetIfhandle_lib_5FB31C72(&maxLinkHdr, &maxProtoHdr, NULL, NULL);
    if ((s32)maxProtoHdr < 40) {
        maxProtoHdr = 40;
    }
    sceNetIfhandle_lib_62B20015(maxLinkHdr, maxProtoHdr, 0, 0);
    inetIcmpMtudiscCallbackRegister(inetTcpMtudiscCallback);
    inetSynCacheInit();
}

/* [366] 0x026F0C */
INET_NOINLINE InetMbuf *inetTcpTemplate(InetTcpcb *tp)
{
    InetInpcbReal *inp = tp->t_inpcb;
    InetMbuf *m;
    InetIpHdr *ip;
    InetTcpHdrOut *th;

    if (tp->t_family != INET_AF_INET || inp == NULL) {
        return NULL;
    }

    m = tp->t_template;
    if (m != NULL && m->m_len != 40) {
        sceNetIfhandle_lib_F56FAC82(m);
        m = NULL;
    }
    if (m == NULL) {
        tp->t_template = NULL;
        m = (InetMbuf *)sceNetIfhandle_lib_15CFE3C0(sizeof(InetMbuf));
        if (m == NULL) {
            return NULL;
        }
        m->m_next = NULL;
        m->m_nextpkt = NULL;
        m->m_data = M_PKTDAT(m);
        m->m_type = INET_MT_HEADER;
        m->m_flags = INET_M_PKTHDR;
        M_CSUM(m) = 0;
        m->m_len = 40;
        M_PKTLEN(m) = 40;
    }

    sceNet_lib_8B615F52(m->m_data, m->m_len);
    ip = (InetIpHdr *)m->m_data;
    th = (InetTcpHdrOut *)(m->m_data + sizeof(InetIpHdr));

    ip->ip_vhl = (u8)((ip->ip_vhl & 0x0F) | 0x40);
    ip->ip_p = INET_IPPROTO_TCP;
    ip->ip_sum = inetBswap16(20);
    ip->ip_src = inp->inp_laddr;
    ip->ip_dst = inp->inp_faddr;

    th->th_sport = inp->inp_lport;
    th->th_dport = inp->inp_fport;
    th->th_seq = 0;
    th->th_ack = 0;
    th->th_offx2 = 0x50;
    th->th_flags = 0;
    th->th_win = 0;
    th->th_sum = 0;
    th->th_urp = 0;
    return m;
}

/* [367] 0x0270B4 */
INET_NOINLINE int inetTcpRespond(InetTcpcb *tp, InetMbuf *mtemplate, InetMbuf *m,
                                 InetTcpHdr *th, u32 ack, u32 seq, int flags)
{
    u32 maxLinkHdr = 0;
    s32 win = 0;
    int tlen = 0;
    InetIpHdr *ip;
    InetTcpHdrOut *outTh;
    int isKeepalive = 0;

    sceNetIfhandle_lib_5FB31C72(&maxLinkHdr, NULL, NULL, NULL);
    if (tp != NULL && (flags & 4) == 0 && tp->t_inpcb != NULL) {
        InetSocket *so = tp->t_inpcb->inp_socket;
        s32 spaceA = (s32)(so->so_rcv.sb_mbmax - so->so_rcv.sb_mbcnt);
        s32 spaceB = (s32)(so->so_rcv.sb_hiwat - so->so_rcv.sb_cc);
        win = (spaceA < spaceB) ? spaceA : spaceB;
    }

    if (m != NULL) {
        u32 thlen;
        if ((m->m_flags & INET_M_PKTHDR) == 0) {
            sceNetIfhandle_lib_F56FAC82(m);
            return INET_EINVAL;
        }
        if ((m->m_data[0] >> 4) != 4) {
            sceNetIfhandle_lib_F56FAC82(m);
            return INET_EAFNOSUPPORT;
        }
        thlen = ((flags & 2) != 0 && ((th->th_offx2 >> 4) * 4) >= 20)
                    ? (u32)((th->th_offx2 >> 4) * 4)
                    : 20U;
        if ((m->m_flags & INET_M_EXT) == 0 &&
            m->m_len > (s32)(thlen + 20) &&
            (u8 *)th == m->m_data + 20) {
            m->m_len = (s32)(thlen + 20);
            sceNetIfhandle_lib_F56FAC82(m->m_next);
            m->m_next = NULL;
        } else {
            InetMbuf *nm = (InetMbuf *)sceNetIfhandle_lib_15CFE3C0(sizeof(InetMbuf));
            if (nm != NULL) {
                nm->m_next = NULL;
                nm->m_nextpkt = NULL;
                nm->m_data = M_PKTDAT(nm);
                nm->m_type = INET_MT_HEADER;
                nm->m_flags = INET_M_PKTHDR;
                M_CSUM(nm) = 0;
                if (maxLinkHdr + 20 + thlen > 208U && !tcpAllocCluster(nm)) {
                    sceNetIfhandle_lib_F56FAC82(nm);
                    nm = NULL;
                }
            }
            if (nm == NULL) {
                sceNetIfhandle_lib_F56FAC82(m);
                return INET_ENOBUFS;
            }
            nm->m_data += maxLinkHdr;
            nm->m_len = (s32)(20 + thlen);
            sceNetIfhandle_lib_1560F143(nm, 0, 20, m->m_data);
            sceNetIfhandle_lib_1560F143(nm, 20, (s32)thlen, th);
            sceNetIfhandle_lib_F56FAC82(m);
            m = nm;
        }

        ip = (InetIpHdr *)m->m_data;
        outTh = (InetTcpHdrOut *)(m->m_data + sizeof(InetIpHdr));
        {
            u32 tmpAddr = ip->ip_dst;
            u16 tmpPort = outTh->th_dport;
            ip->ip_dst = ip->ip_src;
            ip->ip_src = tmpAddr;
            ip->ip_p = INET_IPPROTO_TCP;
            outTh->th_dport = outTh->th_sport;
            outTh->th_sport = tmpPort;
        }
    } else {
        if (mtemplate == NULL) {
            return INET_EINVAL;
        }
        if ((mtemplate->m_data[0] >> 4) != 4) {
            return INET_EAFNOSUPPORT;
        }
        m = (InetMbuf *)sceNetIfhandle_lib_15CFE3C0(sizeof(InetMbuf));
        if (m == NULL) {
            return INET_ENOBUFS;
        }
        m->m_next = NULL;
        m->m_nextpkt = NULL;
        m->m_data = M_PKTDAT(m);
        m->m_type = INET_MT_HEADER;
        m->m_flags = INET_M_PKTHDR;
        M_CSUM(m) = 0;
        if (!tcpAllocCluster(m)) {
            sceNetIfhandle_lib_F8825DC4(m);
            return INET_ENOBUFS;
        }
        m->m_data += maxLinkHdr;
        sceNet_lib_2F305274(mtemplate->m_data, m->m_data, mtemplate->m_len);
        ip = (InetIpHdr *)m->m_data;
        outTh = (InetTcpHdrOut *)(m->m_data + sizeof(InetIpHdr));
        isKeepalive = (g_tcp_compat_42 != 0) ? 1 : 0;
        flags = 0x10;
    }

    outTh->th_seq = inetBswap32(seq);
    outTh->th_ack = inetBswap32(ack);
    outTh->th_offx2 = (u8)(outTh->th_offx2 & 0xF0);
    if ((flags & 2) != 0) {
        tlen = isKeepalive + (int)((outTh->th_offx2 >> 4) * 4);
    } else {
        if (tp != NULL) {
            win >>= tp->rcv_scale;
        }
        if (win > 0xFFFF) {
            win = 0xFFFF;
        }
        outTh->th_win = inetBswap16((u16)win);
        outTh->th_offx2 = 0x50;
        tlen = isKeepalive + 20;
    }

    m->m_len = tlen + 20;
    M_PKTLEN(m) = tlen + 20;
    M_RCVIF(m) = NULL;
    outTh->th_flags = (u8)flags;
    outTh->th_urp = 0;

    sceNet_lib_8B615F52(ip, 9);
    ip->ip_sum = inetBswap16((u16)tlen);
    outTh->th_sum = 0;
    if (tp == NULL || (tp->t_flags & 0x10000) == 0) {
        outTh->th_sum = inetInCksum(m, tlen + 20);
    }
    ip->ip_len = (u16)(tlen + 20);
    ip->ip_ttl = (u8)g_ip_defttl;

    {
        InetRoute *ro = (tp != NULL && tp->t_inpcb != NULL) ? &tp->t_inpcb->inp_route : NULL;
        InetSocket *so = (tp != NULL && tp->t_inpcb != NULL) ? tp->t_inpcb->inp_socket : NULL;
        if (tp != NULL && (tp->t_flags & 0x10000) != 0) {
            InetSockaddrIn srcSin;
            InetSockaddrIn dstSin;
            srcSin.sin_addr = ip->ip_src;
            srcSin.sin_port = tp->t_p2p_lport;
            dstSin.sin_addr = ip->ip_dst;
            dstSin.sin_port = tp->t_p2p_fport;
            sceNetIfhandle_lib_BFF3CEA5(m, (s32)sizeof(InetIpHdr));
            return inetTcpP2pOutput(&srcSin, &dstSin, m, ro, so,
                                    tp->t_flags & 0x20000, tp->t_flags & 0x40000);
        }
        return inetIpOutput(m, NULL, ro,
                            (tp != NULL && tp->t_mtudisc != 0) ? 0x400 : 0, NULL, NULL);
    }
}

/* [368] 0x02771C */
INET_NOINLINE InetTcpcb *inetTcpNewTcpcb(int family, InetInpcbReal *inp)
{
    InetTcpcb *tp;
    s32 rto;

    if (family != INET_AF_INET) {
        return NULL;
    }

    tp = (InetTcpcb *)sceNetIfhandle_lib_15CFE3C0(sizeof(InetTcpcb));
    if (tp == NULL) {
        return NULL;
    }
    sceNet_lib_8B615F52(tp, sizeof(InetTcpcb));

    tp->t_family = INET_AF_INET;
    tp->segq_first = NULL;
    tp->t_segq_time_first = NULL;
    tp->t_sc_head = NULL;
    tp->t_segsz = (u16)g_tcp_mssdflt;
    tp->t_peermss = (u16)g_tcp_mssdflt;
    tp->t_ourmss = (u16)g_tcp_mssdflt;
    tp->t_flags = 0;

    if (g_tcp_do_rfc1323 != 0 && g_tcp_do_win_scale != 0) {
        tp->t_flags |= 0x20;
    }
    if (g_tcp_do_rfc1323 != 0 && g_tcp_do_timestamps != 0) {
        tp->t_flags |= 0x80;
    }
    if (g_tcp_do_sack == 2) {
        tp->t_flags |= 0x800;
    } else if (g_tcp_do_sack == 1) {
        tp->t_flags |= 0x2800;
    }
    tp->t_flags |= 0x1000;

    tp->t_inpcb = inp;
    tp->t_mtudisc = g_ip_mtudisc;
    tp->t_p2p_lport = 0;
    tp->t_p2p_fport = 0;

    tp->t_srtt = 0;
    tp->t_rttvar = (s16)(g_tcp_rttdflt * 16);
    tp->t_rttmin = 2;
    rto = tp->t_rttvar >> 2;
    if (rto < 2) {
        rto = 2;
    } else if (rto > 128) {
        rto = 128;
    }
    tp->t_rxtcur = (s16)rto;
    tp->snd_cwnd = 0x3FFFC000U;
    tp->snd_ssthresh = 0x3FFFC000U;

    inp->inp_ppcb = tp;
    inp->inp_ip.ip_ttl = (u8)g_ip_defttl;
    return tp;
}

/* [369] 0x0278D0 */
INET_NOINLINE InetTcpcb *inetTcpClose(InetTcpcb *tp)
{
    InetInpcbReal *inp = tp->t_inpcb;
    InetSocket *so = (inp != NULL) ? inp->inp_socket : NULL;
    InetRoute *ro = (inp != NULL) ? &inp->inp_route : NULL;
    int spl;

    /* Save RTT & ssthresh metrics into route entry if enough data was transferred */
    if (ro != NULL && so != NULL &&
        tcpSeqGtOut(tp->snd_max, tp->iss + 16U * so->so_snd.sb_hiwat)) {
        InetRtentry *rt = ro->ro_rt;
        if (rt != NULL && rt->rt_nodes[0].rn_u.rn_leaf.rn_key != NULL &&
            ((InetSockaddrIn *)rt->rt_nodes[0].rn_u.rn_leaf.rn_key)->sin_addr != 0) {
            u32 locks = rt->rt_rmx.rmx_locks;
            if ((locks & 0x40) == 0) {
                u32 srtt = (u32)(15625 * tp->t_srtt);
                if (rt->rt_rmx.rmx_rtt != 0 && srtt != 0) {
                    rt->rt_rmx.rmx_rtt = (rt->rt_rmx.rmx_rtt + srtt) >> 1;
                } else {
                    rt->rt_rmx.rmx_rtt = srtt;
                }
            }
            if ((locks & 0x80) == 0) {
                u32 rttvar = (u32)(31250 * tp->t_rttvar);
                if (rt->rt_rmx.rmx_rttvar != 0 && rttvar != 0) {
                    rt->rt_rmx.rmx_rttvar = (rt->rt_rmx.rmx_rttvar + rttvar) >> 1;
                } else {
                    rt->rt_rmx.rmx_rttvar = rttvar;
                }
            }
            if ((locks & 0x20) == 0) {
                u32 ssth = tp->snd_ssthresh;
                if ((ssth != 0 && rt->rt_rmx.rmx_ssthresh != 0) ||
                    ssth < (rt->rt_rmx.rmx_sendpipe >> 1)) {
                    u32 segsz = tp->t_segsz;
                    u32 segs = (ssth + (segsz >> 1)) / segsz;
                    u32 pipe;
                    if (segs < 2) {
                        segs = 2;
                    }
                    pipe = segs * (segsz + 40U);
                    if (rt->rt_rmx.rmx_ssthresh != 0) {
                        rt->rt_rmx.rmx_ssthresh = (rt->rt_rmx.rmx_ssthresh + pipe) >> 1;
                    } else {
                        rt->rt_rmx.rmx_ssthresh = pipe;
                    }
                }
            }
        }
    }

    spl = sceNet_lib_94B44F26();
    tp->t_flags |= 0x4000;
    sceNet_lib_515B2F33(spl);
    inetTcpFreeq(tp);
    spl = sceNet_lib_94B44F26();
    tp->t_flags &= ~0x4000U;
    sceNet_lib_515B2F33(spl);

    if ((tp->t_flags & 2) != 0) {
        tp->t_flags &= ~2U;
        if (tp->t_delack_next != NULL) {
            tp->t_delack_next->t_delack_prev = tp->t_delack_prev;
        }
        *tp->t_delack_prev = tp->t_delack_next;
    }

    inetSynCacheCleanup(tp);
    if (tp->t_template != NULL) {
        sceNetIfhandle_lib_F8825DC4(tp->t_template);
        tp->t_template = NULL;
    }
    sceNetIfhandle_lib_76BAD213(tp);

    if (inp != NULL) {
        inp->inp_ppcb = NULL;
        inetSoIsAborted(so);
        inetInPcbDetach(inp);
    }

    g_tcpstat.tcps_closed++;
    return NULL;
}

/* [370] 0x027B5C */
static INET_NOINLINE int inetTcpFreeq(InetTcpcb *tp)
{
    int rv = 0;
    InetTcpqentSubr *qe = (InetTcpqentSubr *)tp->segq_first;

    while (qe != NULL) {
        if (qe->tqe_q_next != NULL) {
            qe->tqe_q_next->tqe_q_prev = qe->tqe_q_prev;
        }
        *qe->tqe_q_prev = qe->tqe_q_next;

        if (qe->tqe_timeq_next != NULL) {
            qe->tqe_timeq_next->tqe_timeq_prev = qe->tqe_timeq_prev;
        }
        *qe->tqe_timeq_prev = qe->tqe_timeq_next;

        sceNetIfhandle_lib_F56FAC82(qe->tqe_m);
        sceNetIfhandle_lib_76BAD213(qe);
        rv = 1;
        qe = (InetTcpqentSubr *)tp->segq_first;
    }
    return rv;
}

/* [371] 0x027BE0 */
INET_NOINLINE void inetTcpCtlinput(int cmd, InetSockaddr *sa, void *d)
{
    InetIpHdr *ip = (InetIpHdr *)d;
    InetSockaddrIn *sin = (InetSockaddrIn *)sa;
    void (*notify)(InetInpcbReal *, int) = inetTcpNotify;
    int err;

    if (sin->sin_family != INET_AF_INET || sin->sin_len != sizeof(InetSockaddrIn) || (u32)cmd >= 21U) {
        return;
    }

    err = g_inetctlerrmap[cmd];
    if (cmd == 4) {
        notify = inetTcpQuench;
    } else if ((u32)(cmd - 14) < 4U) {
        notify = inetInRtChange;
        inetInPcbNotifyAll(&g_tcbtable, sin->sin_addr, err, notify);
        return;
    } else if (cmd == 5 && ip != NULL && (ip->ip_vhl & 0xF0) == 0x40) {
        InetTcpHdrOut *th = (InetTcpHdrOut *)((u8 *)ip + (ip->ip_vhl & 0x0F) * 4);
        if (inetInPcbLookupConnect(&g_tcbtable, ip->ip_dst, th->th_dport,
                                   ip->ip_src, th->th_sport) != NULL) {
            inetIcmpMtudisc((void *)((u8 *)ip - 8), ip->ip_dst);
        }
        return;
    } else if (cmd == 6) {
        inetInPcbNotifyAll(&g_tcbtable, sin->sin_addr, err, notify);
        return;
    } else if (err == 0) {
        return;
    }

    if (ip != NULL && (ip->ip_vhl & 0xF0) == 0x40 && sin->sin_family == INET_AF_INET) {
        InetTcpHdrOut *th = (InetTcpHdrOut *)((u8 *)ip + (ip->ip_vhl & 0x0F) * 4);
        if (inetInPcbNotify(&g_tcbtable, sin->sin_addr, th->th_dport,
                            ip->ip_src, th->th_sport, err, notify) == 0 &&
            g_tcp_syn_cache_count != 0) {
            if (err == INET_EHOSTUNREACH || err == INET_ENETUNREACH || err == INET_EHOSTDOWN) {
                InetSockaddrIn srcSin;
                sceNet_lib_8B615F52(&srcSin, sizeof(srcSin));
                srcSin.sin_len = sizeof(InetSockaddrIn);
                srcSin.sin_family = INET_AF_INET;
                srcSin.sin_addr = ip->ip_src;
                srcSin.sin_port = th->th_sport;
                inetSynCacheUnreach(&srcSin, sin, (InetTcpHdr *)th);
            }
        }
    } else {
        inetInPcbNotifyAll(&g_tcbtable, sin->sin_addr, err, notify);
    }
}

/* [372] 0x027E6C */
INET_NOINLINE void inetTcpMssFromPeer(InetTcpcb *tp, u32 offer)
{
    InetSocket *so = NULL;
    InetRtentry *rt = NULL;
    u32 mss;
    u32 segsz;
    u32 bufsize;

    if (tp->t_inpcb != NULL) {
        so = tp->t_inpcb->inp_socket;
        rt = inetInPcbRtEntry(tp->t_inpcb);
    }

    mss = (offer != 0) ? offer : (u32)g_tcp_mssdflt;
    if (mss < 32) {
        mss = 32;
    }
    tp->t_peermss = (u16)mss;

    segsz = mss - (u32)inetTcpHdrOptLen(tp);
    if (tp->t_inpcb != NULL) {
        segsz -= (u32)inetIpOptlen(tp->t_inpcb);
    }

    bufsize = (rt != NULL && rt->rt_rmx.rmx_sendpipe != 0)
                  ? rt->rt_rmx.rmx_sendpipe
                  : so->so_snd.sb_hiwat;
    if (bufsize >= segsz) {
        u32 rounded = ((bufsize + segsz - 1U) / segsz) * segsz;
        if (rounded > 0x40000U) {
            rounded = 0x40000U;
        }
        inetSbReserve(&so->so_snd, rounded);
    } else {
        segsz = bufsize;
    }

    tp->t_segsz = (u16)segsz;
    if (rt != NULL && rt->rt_rmx.rmx_ssthresh != 0) {
        tp->snd_ssthresh = (rt->rt_rmx.rmx_ssthresh >= 2U * segsz)
                               ? rt->rt_rmx.rmx_ssthresh
                               : (2U * segsz);
    }
}

/* [373] 0x027FA0 */
INET_NOINLINE void inetTcpRttFromRt(InetTcpcb *tp)
{
    InetRtentry *rt;
    s32 rtt;
    u32 rttvar;
    s32 rto;

    if (tp->t_inpcb == NULL) {
        return;
    }
    rt = inetInPcbRtEntry(tp->t_inpcb);
    if (rt == NULL) {
        return;
    }
    if (tp->t_srtt != 0) {
        return;
    }
    rtt = (s32)rt->rt_rmx.rmx_rtt;
    if (rtt == 0) {
        return;
    }
    if ((rt->rt_rmx.rmx_locks & 0x40) != 0) {
        s32 minVal = rtt / 500000;
        if (minVal < 2) {
            minVal = 2;
        } else if (minVal > 128) {
            minVal = 128;
        }
        tp->t_rttmin = (s16)minVal;
    }

    tp->t_srtt = (s16)(rtt / 15625);
    rttvar = rt->rt_rmx.rmx_rttvar;
    tp->t_rttvar = (rttvar != 0) ? (s16)(rttvar / 31250U) : (s16)(tp->t_srtt >> 1);

    rto = ((tp->t_srtt >> 2) + tp->t_rttvar) >> 3;
    if (rto < tp->t_rttmin) {
        tp->t_rxtcur = tp->t_rttmin;
    } else if (rto > 128) {
        tp->t_rxtcur = 128;
    } else {
        tp->t_rxtcur = (s16)rto;
    }
}

/* [374] 0x0280C4 */
static INET_NOINLINE int inetTcpHdrOptLen(InetTcpcb *tp)
{
    return ((tp->t_flags & 0x188) == 0x180) ? 12 : 0;
}

/* [375] 0x0280DC */
INET_NOINLINE InetTcpcb *inetTcpDrop(InetTcpcb *tp, int err)
{
    InetInpcbReal *inp = tp->t_inpcb;
    InetSocket *so;

    if (inp == NULL) {
        return NULL;
    }
    so = inp->inp_socket;
    if (tp->t_state >= 3) {
        tp->t_state = 0;
        inetTcpOutput(tp);
        g_tcpstat.tcps_drops++;
    } else {
        g_tcpstat.tcps_conndrops++;
    }
    if (err == INET_ETIMEDOUT && tp->t_softerror != 0) {
        err = tp->t_softerror;
    }
    so->so_error = (u16)err;
    return inetTcpClose(tp);
}

/* [376] 0x0281A4 */
INET_NOINLINE void inetTcpDrain(void)
{
    InetInpcbReal *inp = g_tcbtable.inpt_queue_first;

    if (inp == NULL) {
        return;
    }
    while (inp != (InetInpcbReal *)&g_tcbtable) {
        InetTcpcb *tp = (InetTcpcb *)inp->inp_ppcb;
        if (tp != NULL) {
            int spl = sceNet_lib_94B44F26();
            if ((tp->t_flags & 0x4000) == 0) {
                tp->t_flags |= 0x4000;
                sceNet_lib_515B2F33(spl);
                if (inetTcpFreeq(tp) != 0) {
                    g_tcpstat.tcps_reass_drained++;
                }
                spl = sceNet_lib_94B44F26();
                tp->t_flags &= ~0x4000U;
            }
            sceNet_lib_515B2F33(spl);
        }
        inp = inp->inp_queue_next;
    }
}

/* [377] 0x028280 */
static INET_NOINLINE void inetTcpNotify(InetInpcbReal *inp, int err)
{
    InetTcpcb *tp = (InetTcpcb *)inp->inp_ppcb;
    InetSocket *so = inp->inp_socket;

    if (tp->t_state == 4 &&
        (((err == INET_EHOSTUNREACH) | (err == INET_ENETUNREACH)) || err == INET_EHOSTDOWN)) {
        return;
    }
    if (tp->t_state < 4 && tp->t_rxtshift > 3 && tp->t_softerror != 0) {
        so->so_error = (u16)err;
    } else {
        tp->t_softerror = (s16)err;
    }
    sceNet_lib_7574FDA1(&so->so_timeo);
    if ((so->so_rcv.sb_flags & 0x3C) != 0) {
        inetSoWakeup(so, &so->so_rcv);
    }
    if ((so->so_snd.sb_flags & 0x3C) != 0) {
        inetSoWakeup(so, &so->so_snd);
    }
}

/* [378-sub] 0x028354 */
static INET_NOINLINE void inetTcpQuench(InetInpcbReal *inp, int err)
{
    InetTcpcb *tp = (InetTcpcb *)inp->inp_ppcb;
    (void)err;
    if (tp != NULL) {
        tp->snd_cwnd = tp->t_segsz;
    }
}

/* [379-sub] 0x028370 */
static INET_NOINLINE void inetTcpMtudiscCallback(u32 faddr)
{
    inetInPcbNotifyAll(&g_tcbtable, faddr, INET_EMSGSIZE, inetTcpMtudisc);
}

/* [380-sub] 0x0283A4 */
static INET_NOINLINE void inetTcpMtudisc(InetInpcbReal *inp, int err)
{
    InetTcpcb *tp = (InetTcpcb *)inp->inp_ppcb;
    InetRtentry *rt = inetInPcbRtEntry(inp);

    (void)err;
    if (tp == NULL) {
        return;
    }
    if (rt != NULL) {
        if ((rt->rt_flags & INET_RTF_HOST) == 0) {
            inetInRtChange(inp, 0);
            rt = inetInPcbRtEntry(inp);
            if (rt == NULL) {
                return;
            }
        }
        if (rt->rt_rmx.rmx_mtu != 0) {
            u32 mtu = rt->rt_rmx.rmx_mtu;
            if (g_tcp_init_win != 0) {
                tp->snd_cwnd = mtu * (u32)g_tcp_init_win;
            } else {
                u32 cwLo = (2U * mtu > 4380U) ? (2U * mtu) : 4380U;
                u32 cwHi = 4U * mtu;
                tp->snd_cwnd = (cwHi < cwLo) ? cwHi : cwLo;
            }
        }
    }
    tp->snd_nxt = tp->snd_una;
    inetTcpOutput(tp);
}

/* [381-sub] 0x028474 */
INET_NOINLINE u32 inetTcpMssToAdvertise(InetIfnet *ifp, int family, InetTcpcb *tp)
{
    u32 hdrlen = (family == INET_AF_INET) ? 40U : 20U;
    u32 mtu = (ifp != NULL) ? (u32)ifp->if_data.ifi_mtu : 0;
    u32 optMss = tp->t_maxseg_opt;

    if (optMss == 0 && g_tcp_mss_ifmtu == 0 && mtu < g_in_maxmtu) {
        mtu = g_in_maxmtu;
    }
    if (mtu > hdrlen) {
        mtu -= hdrlen;
    }
    if (optMss != 0 && optMss < mtu) {
        return optMss;
    }
    return (mtu >= (u32)g_tcp_mssdflt) ? mtu : (u32)g_tcp_mssdflt;
}

/* [382-sub] 0x0284F8 */
INET_NOINLINE void inetTcpEstablished(InetTcpcb *tp)
{
    InetSocket *so = NULL;
    InetRtentry *rt = NULL;
    u32 bufsize;
    u32 ourmss;

    if (tp->t_inpcb != NULL) {
        so = tp->t_inpcb->inp_socket;
        rt = inetInPcbRtEntry(tp->t_inpcb);
    }

    tp->t_state = 4;
    tp->t_timer[2] = g_inet_ticks + g_tcp_keepidle;

    bufsize = (rt != NULL && rt->rt_rmx.rmx_recvpipe != 0)
                  ? rt->rt_rmx.rmx_recvpipe
                  : so->so_rcv.sb_hiwat;
    ourmss = tp->t_ourmss;
    if (bufsize > ourmss) {
        u32 rounded = ((bufsize + ourmss - 1U) / ourmss) * ourmss;
        if (rounded > 0x40000U) {
            rounded = 0x40000U;
        }
        inetSbReserve(&so->so_rcv, rounded);
    }
}

/* [383-sub] 0x0285B8 */
INET_NOINLINE u32 inetTcpNewIss(void *ctx, u32 len, u32 addin)
{
    u32 rnd = sceNet_lib_1858883D();
    u32 iss;

    (void)ctx;
    (void)len;
    if (addin != 0) {
        iss = (rnd & 0x00FFFFFFU) + addin + 0x01000000U;
    } else {
        iss = (rnd & 0x00FFFFFFU) + g_tcp_iss_seq;
        g_tcp_iss_seq += 0x01000000U;
    }
    if (g_tcp_compat_42 != 0 && (s32)iss < 0) {
        iss &= 0x7FFFFFFFU;
    }
    return iss;
}

/* [384-sub] 0x028634 */
INET_NOINLINE void inetTcpP2pDropPeer(const InetSockaddrIn *peer)
{
    InetInpcbReal *inp = g_tcbtable.inpt_queue_first;

    while (inp != (InetInpcbReal *)&g_tcbtable) {
        InetTcpcb *tp = (InetTcpcb *)inp->inp_ppcb;
        InetInpcbReal *nextInp = inp->inp_queue_next;
        if (tp != NULL && tp->t_inpcb != NULL &&
            (tp->t_flags & 0x10000) != 0 &&
            tp->t_inpcb->inp_faddr == peer->sin_addr &&
            tp->t_p2p_fport == peer->sin_port) {
            InetSocket *so = tp->t_inpcb->inp_socket;
            if (tp->t_state >= 3) {
                tp->t_state = 0;
                inetTcpOutput(tp);
                g_tcpstat.tcps_drops++;
            } else {
                g_tcpstat.tcps_conndrops++;
            }
            so->so_error = INET_ECONNRESET;
            inetTcpClose(tp);
        }
        inp = nextInp;
    }
}
