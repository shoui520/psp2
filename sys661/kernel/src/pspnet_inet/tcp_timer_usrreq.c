/*
 * PSP 6.61 SceNetInet_Library (pspnet_inet.prx)
 * TCP timers, usrreq, ctloutput, and sysctl (tcp_timer.c / tcp_usrreq.c)
 */

#include "pspnet_inet_internal.h"

typedef struct InetTcpSysctlEntry {
    u32 flags;
    void *var_ptr;
    int const_val;
} InetTcpSysctlEntry;

u32 g_tcp_keepidle = 14400;
u32 g_tcp_keepintvl = 150;
u32 g_tcp_keepcnt = 8;
s32 g_tcp_maxpersistidle = 14400;
const int g_tcp_backoff[13] = { 1, 2, 4, 8, 16, 32, 64, 64, 64, 64, 64, 64, 64 };
int g_tcp_totbackoff = 511;
u32 g_tcp_sendspace = 16384;
u32 g_tcp_recvspace = 16384;
s32 g_tcp_maxidle = 0;
u32 g_tcp_syn_cache_timer_tick = 0;
int g_tcp_cwm_burstsize = 4;
int g_tcp_mssdflt = 512;
int g_tcp_rttdflt = 3;
int g_tcp_do_rfc1323 = 1;
int g_tcp_do_sack = 1;
int g_tcp_do_win_scale = 1;
int g_tcp_do_timestamps = 1;
int g_tcp_init_win = 1;
u32 g_tcbhashsize = 128;
int g_tcp_cwm = 0;
int g_tcp_do_newreno = 0;
int g_tcp_ack_on_push = 0;
int g_tcp_mss_ifmtu = 0;
int g_tcp_compat_42 = 0;
int g_tcp_log_refused = 0;
int g_tcp_delack_ticks = 0;

static const InetTcpSysctlEntry g_tcp_sysctl_table[26] = {
    { 0, NULL, 0 },
    { 1, &g_tcp_do_rfc1323, 0 },
    { 1, &g_tcp_sendspace, 0 },
    { 1, &g_tcp_recvspace, 0 },
    { 1, &g_tcp_mssdflt, 0 },
    { 1, &g_tcp_syn_cache_limit, 0 },
    { 1, &g_tcp_syn_bucket_limit, 0 },
    { 1, &g_tcp_syn_cache_interval, 0 },
    { 1, &g_tcp_init_win, 0 },
    { 1, &g_tcp_mss_ifmtu, 0 },
    { 1, &g_tcp_do_sack, 0 },
    { 1, &g_tcp_do_win_scale, 0 },
    { 1, &g_tcp_do_timestamps, 0 },
    { 1, &g_tcp_compat_42, 0 },
    { 1, &g_tcp_cwm, 0 },
    { 1, &g_tcp_cwm_burstsize, 0 },
    { 1, &g_tcp_ack_on_push, 0 },
    { 1, &g_tcp_keepidle, 0 },
    { 1, &g_tcp_keepintvl, 0 },
    { 1, &g_tcp_keepcnt, 0 },
    { 3, NULL, 2 },
    { 1, &g_tcp_do_newreno, 0 },
    { 1, &g_tcp_log_refused, 0 },
    { 0, NULL, 0 },
    { 1, &g_tcp_rstppslim, 0 },
    { 0, NULL, 0 }
};

static INET_NOINLINE InetTcpcb *inetTcpTimers(InetTcpcb *tp, int timerIdx);
static INET_NOINLINE int inetTcpAttach(InetSocket *so);
static INET_NOINLINE InetTcpcb *inetTcpDisconnect(InetTcpcb *tp);
static INET_NOINLINE InetTcpcb *inetTcpUsrclosed(InetTcpcb *tp);

/* [378] 0x02878C */
INET_NOINLINE void inetTcpSlowtimo(void)
{
    int spl = sceNet_lib_94B44F26();
    InetInpcbReal *inp = g_tcbtable.inpt_queue_first;
    int empty = 0;

    g_tcp_maxidle = (s32)(g_tcp_keepcnt * g_tcp_keepintvl);
    if (inp == NULL) {
        empty = 1;
    } else {
        while (inp != (InetInpcbReal *)&g_tcbtable) {
            InetTcpcb *tp = (InetTcpcb *)inp->inp_ppcb;
            InetInpcbReal *nextInp = inp->inp_queue_next;
            if (tp != NULL && tp->t_state != 1) {
                int timerIdx;
                int destroyed = 0;
                for (timerIdx = 0; timerIdx < 4; timerIdx++) {
                    if (tp->t_timer[timerIdx] != 0 && g_inet_ticks >= tp->t_timer[timerIdx]) {
                        InetInpcbReal *ownerInp = tp->t_inpcb;
                        tp->t_timer[timerIdx] = 0;
                        inetTcpUsrreq(ownerInp->inp_socket, INET_PRU_SLOWTIMO, NULL,
                                      (InetMbuf *)timerIdx, NULL);
                        if (nextInp == (InetInpcbReal *)&g_tcbtable) {
                            if (g_tcbtable.inpt_queue_last != inp) {
                                destroyed = 1;
                                break;
                            }
                        }
                        if ((InetInpcbReal *)nextInp->inp_queue_prev != inp) {
                            destroyed = 1;
                            break;
                        }
                    }
                }
                if (!destroyed) {
                    tp->t_idle++;
                    if (tp->t_rtt != 0) {
                        tp->t_rtt++;
                    }
                }
            }
            inp = nextInp;
        }
    }

    if (!empty) {
        g_tcp_iss_seq += 0x01000000U;
        g_tcp_now++;
        g_tcp_syn_cache_timer_tick++;
        if ((s32)g_tcp_syn_cache_timer_tick >= g_tcp_syn_cache_interval) {
            inetSynCacheTimer();
            g_tcp_syn_cache_timer_tick = 0;
        }
    }
    sceNet_lib_515B2F33(spl);
}

/* [379] 0x028964 */
static INET_NOINLINE InetTcpcb *inetTcpTimers(InetTcpcb *tp, int timerIdx)
{
    switch (timerIdx) {
    case 0: { /* TCPT_REXMT */
        s16 rxtShift = (s16)(tp->t_rxtshift + 1);
        s32 rexmt;
        u32 win;
        u32 segsz;
        u32 ssthreshSegs;

        tp->t_rxtshift = rxtShift;
        if (rxtShift > 12) {
            int dropErr = (tp->t_softerror != 0) ? tp->t_softerror : INET_ETIMEDOUT;
            tp->t_rxtshift = 12;
            g_tcpstat.tcps_timeoutdrop++;
            return inetTcpDrop(tp, dropErr);
        }

        rexmt = ((tp->t_srtt >> 3) + tp->t_rttvar) >> 2;
        if (rexmt < tp->t_rttmin) {
            rexmt = tp->t_rttmin;
        }
        rexmt *= g_tcp_backoff[rxtShift];
        g_tcpstat.tcps_rexmttimeo++;
        if (rexmt < tp->t_rttmin) {
            rexmt = tp->t_rttmin;
        } else if (rexmt > 128) {
            rexmt = 128;
        }
        tp->t_rxtcur = (s16)rexmt;
        tp->t_timer[0] = g_inet_ticks + (u32)tp->t_rxtcur;

        if (g_ip_mtudisc != 0 && tp->t_rxtshift >= 3) {
            g_tcpstat.tcps_pmtublackhole++;
            if (tp->t_inpcb != NULL) {
                tp->t_mtudisc = 0;
            }
        }

        if (tp->t_rxtshift >= 4) {
            if (tp->t_inpcb != NULL) {
                inetInLosing(tp->t_inpcb);
            }
            tp->t_rttvar = (s16)((tp->t_srtt >> 3) + tp->t_rttvar);
            tp->t_srtt = 0;
        }

        tp->snd_nxt = tp->snd_una;
        tp->t_rtt = 0;
        if (tp->t_state == 2) {
            tp->t_flags |= 0x400;
        }

        win = (tp->snd_wnd < tp->snd_cwnd) ? tp->snd_wnd : tp->snd_cwnd;
        segsz = tp->t_segsz;
        tp->snd_cwnd = segsz;
        tp->t_dupacks = 0;
        ssthreshSegs = (win >> 1) / segsz;
        if (ssthreshSegs < 2) {
            ssthreshSegs = 2;
        }
        tp->snd_ssthresh = ssthreshSegs * segsz;
        inetTcpOutput(tp);
        return tp;
    }

    case 1: { /* TCPT_PERSIST */
        s32 rto = ((tp->t_srtt >> 3) + tp->t_rttvar) >> 2;
        if (rto < tp->t_rttmin) {
            rto = tp->t_rttmin;
        }
        if (tp->t_rxtshift == 12 &&
            (tp->t_idle >= g_tcp_maxpersistidle || tp->t_idle >= rto * g_tcp_totbackoff)) {
            g_tcpstat.tcps_persistdrop++;
            return inetTcpDrop(tp, INET_ETIMEDOUT);
        }
        g_tcpstat.tcps_persisttimeo++;
        inetTcpSetPersist(tp);
        tp->t_force = 1;
        inetTcpOutput(tp);
        tp->t_force = 0;
        return tp;
    }

    case 2: { /* TCPT_KEEP */
        g_tcpstat.tcps_keeptimeo++;
        if (tp->t_state >= 4) {
            InetSocket *so = (tp->t_inpcb != NULL) ? tp->t_inpcb->inp_socket : NULL;
            if ((so->so_options & INET_SO_KEEPALIVE) == 0 || tp->t_state >= 6) {
                tp->t_timer[2] = g_inet_ticks + g_tcp_keepidle;
                return tp;
            }
            if (g_tcp_maxidle <= 0 || tp->t_idle < (s32)g_tcp_keepidle + g_tcp_maxidle) {
                u32 ackSeq = (g_tcp_compat_42 != 0) ? (tp->rcv_nxt - 1) : tp->rcv_nxt;
                g_tcpstat.tcps_keepprobe++;
                inetTcpRespond(tp, tp->t_template, NULL, NULL, ackSeq, tp->snd_una - 1, 0);
                tp->t_timer[2] = g_inet_ticks + g_tcp_keepintvl;
                return tp;
            }
        }
        g_tcpstat.tcps_keepdrops++;
        return inetTcpDrop(tp, INET_ETIMEDOUT);
    }

    case 3: /* TCPT_2MSL */
        if (tp->t_state == 10 || (g_tcp_maxidle != 0 && tp->t_idle > g_tcp_maxidle)) {
            return inetTcpClose(tp);
        }
        tp->t_timer[3] = g_inet_ticks + g_tcp_keepintvl;
        return tp;

    default:
        return tp;
    }
}

/* [380] 0x028E24 */
INET_NOINLINE void inetTcpFasttimo(void)
{
    int spl = sceNet_lib_94B44F26();
    InetTcpcb *tp = g_tcp_delack_head;

    while (tp != NULL) {
        InetTcpcb *nextTp = tp->t_delack_next;
        tp->t_flags |= 1;
        inetTcpOutput(tp);
        tp = nextTp;
    }
    sceNet_lib_515B2F33(spl);
}

/* [381] 0x028E84 */
INET_NOINLINE void inetTcpCancelTimers(InetTcpcb *tp)
{
    int idx;
    for (idx = 0; idx < 4; idx++) {
        tp->t_timer[idx] = 0;
    }
}

/* [382] 0x028EA4 */
INET_NOINLINE void inetTcpTimerInit(void)
{
    g_tcp_keepidle = 14400;
    g_tcp_totbackoff = 511;
    g_tcp_keepintvl = 150;
    g_tcp_keepcnt = 8;
    g_tcp_maxpersistidle = 14400;
    g_tcp_maxidle = 0;
    g_tcp_syn_cache_timer_tick = 0;
}

/* [383] 0x028EF0 */
INET_NOINLINE int inetTcpUsrreq(InetSocket *so, int req, InetMbuf *m, InetMbuf *nam, InetMbuf *control)
{
    int err = 0;
    int family = so->so_proto->pr_domain->dom_family;
    int spl;
    InetInpcbReal *inp;
    InetTcpcb *tp = NULL;

    if (req == INET_PRU_CONTROL) {
        if (family != INET_AF_INET) {
            return INET_EAFNOSUPPORT;
        }
        return inetInControl(so, (u32)m, nam, (InetIfnet *)control);
    }
    if (req == INET_PRU_PURGEIF) {
        if (family != INET_AF_INET) {
            return INET_EAFNOSUPPORT;
        }
        inetInPurgeIf((InetIfnet *)control);
        inetInPcbPurgeIf(&g_tcbtable, (InetIfnet *)control);
        return 0;
    }

    spl = sceNet_lib_94B44F26();
    if (family != INET_AF_INET) {
        sceNet_lib_515B2F33(spl);
        return INET_EAFNOSUPPORT;
    }

    inp = (InetInpcbReal *)so->so_pcb;
    if (inp == NULL && req != INET_PRU_ATTACH) {
        sceNet_lib_515B2F33(spl);
        return INET_EINVAL;
    }
    if (inp != NULL) {
        tp = (InetTcpcb *)inp->inp_ppcb;
    }

    switch (req) {
    case INET_PRU_ATTACH:
        if (inp != NULL) {
            err = INET_EISCONN;
        } else {
            err = inetTcpAttach(so);
            if (err == 0 && (so->so_options & INET_SO_LINGER) != 0 && so->so_linger == 0) {
                so->so_linger = 120;
            }
        }
        break;

    case INET_PRU_DETACH:
    case INET_PRU_DISCONNECT:
        inetTcpDisconnect(tp);
        break;

    case INET_PRU_BIND:
        err = inetInPcbBind(inp, nam);
        if (err == 0 && so->so_type == 10) {
            InetSockaddrIn *sin = (InetSockaddrIn *)nam->m_data;
            u16 vport = (u16)sin->sin_zero[0] | ((u16)sin->sin_zero[1] << 8);
            tp->t_p2p_lport = (vport != 0) ? vport : inetBswap16(0x0E4A);
        }
        break;

    case INET_PRU_LISTEN:
        if (inp != NULL) {
            if (inp->inp_lport == 0) {
                err = inetInPcbBind(inp, NULL);
            }
            if (err == 0 && so->so_type == 10) {
                if (tp->t_p2p_lport == 0) {
                    tp->t_p2p_lport = inetBswap16(0x0E4A);
                }
                tp->t_flags |= 0x10000;
                if ((so->so_options & 0x1000) != 0) {
                    tp->t_flags |= 0x20000;
                }
                if ((so->so_options & 0x2000) != 0) {
                    tp->t_flags |= 0x40000;
                }
            }
        }
        if (err == 0) {
            tp->t_state = 1;
        }
        break;

    case INET_PRU_CONNECT:
        if (inp != NULL) {
            if (inp->inp_lport == 0) {
                err = inetInPcbBind(inp, NULL);
            }
            if (err == 0) {
                err = inetInPcbConnect(inp, nam);
            }
            if (err == 0 && so->so_type == 10) {
                InetSockaddrIn *sin = (InetSockaddrIn *)nam->m_data;
                u16 fvport = (u16)sin->sin_zero[0] | ((u16)sin->sin_zero[1] << 8);
                if (tp->t_p2p_lport == 0) {
                    tp->t_p2p_lport = inetBswap16(0x0E4A);
                }
                tp->t_p2p_fport = (fvport != 0) ? fvport : inetBswap16(0x0E4A);
                tp->t_flags |= 0x10000;
                if ((so->so_options & 0x1000) != 0) {
                    tp->t_flags |= 0x20000;
                }
                if ((so->so_options & 0x2000) != 0) {
                    tp->t_flags |= 0x40000;
                }
            }
        }
        if (err == 0) {
            tp->t_template = inetTcpTemplate(tp);
            if (tp->t_template == NULL) {
                if (inp != NULL) {
                    inetInPcbDisconnect(inp);
                }
                err = INET_ENOBUFS;
            } else {
                u32 iss;
                while (tp->request_r_scale < 14 &&
                       (0xFFFFU << tp->request_r_scale) < so->so_rcv.sb_hiwat) {
                    tp->request_r_scale++;
                }
                inetSoIsConnecting(so);
                tp->t_state = 2;
                tp->t_timer[2] = g_inet_ticks + 150;
                g_tcpstat.tcps_connattempt++;
                iss = inetTcpNewIss(tp, sizeof(InetTcpcb), 0);
                tp->iss = iss;
                tp->snd_una = iss;
                tp->snd_nxt = iss;
                tp->snd_max = iss;
                tp->snd_up = iss;
                tp->snd_recover = iss;
                err = inetTcpOutput(tp);
            }
        }
        break;

    case INET_PRU_ACCEPT:
    case INET_PRU_PEERADDR:
        if (inp != NULL) {
            inetInSetPeeraddr(inp, nam);
            if (so->so_type == 10) {
                InetSockaddrIn *sin = (InetSockaddrIn *)nam->m_data;
                sin->sin_zero[0] = (u8)(tp->t_p2p_fport & 0xFF);
                sin->sin_zero[1] = (u8)(tp->t_p2p_fport >> 8);
            }
        }
        break;

    case INET_PRU_SHUTDOWN:
        inetSoCantSendMore(so);
        tp = inetTcpUsrclosed(tp);
        if (tp != NULL) {
            err = inetTcpOutput(tp);
        }
        break;

    case INET_PRU_RCVD:
        inetTcpOutput(tp);
        break;

    case INET_PRU_SEND:
        if (control != NULL && control->m_len != 0) {
            sceNetIfhandle_lib_F56FAC82(control);
            sceNetIfhandle_lib_F56FAC82(m);
            err = INET_EINVAL;
        } else {
            inetSbAppend(&so->so_snd, m);
            err = inetTcpOutput(tp);
        }
        break;

    case INET_PRU_ABORT:
        inetTcpDrop(tp, INET_ECONNABORTED);
        break;

    case INET_PRU_SENSE:
        sceNet_lib_515B2F33(spl);
        return 0;

    case INET_PRU_RCVOOB:
        if (control != NULL && control->m_len != 0) {
            sceNetIfhandle_lib_F56FAC82(control);
            sceNetIfhandle_lib_F56FAC82(m);
            err = INET_EINVAL;
        } else if ((so->so_oobmark == 0 && (so->so_state & INET_SS_RCVATMARK) == 0) ||
                   (so->so_options & INET_SO_OOBINLINE) != 0 ||
                   (tp->t_oobflags & 2) != 0) {
            err = INET_EINVAL;
        } else if ((tp->t_oobflags & 1) == 0) {
            err = INET_EAGAIN;
        } else {
            m->m_len = 1;
            m->m_data[0] = tp->t_iobc;
            if (((u32)nam & 2) == 0) {
                tp->t_oobflags ^= 3;
            }
        }
        break;

    case INET_PRU_SENDOOB: {
        s32 spaceA = (s32)(so->so_snd.sb_mbmax - so->so_snd.sb_mbcnt);
        s32 spaceB = (s32)(so->so_snd.sb_hiwat - so->so_snd.sb_cc);
        s32 minSpace = (spaceA < spaceB) ? spaceA : spaceB;
        if (minSpace < -512) {
            sceNetIfhandle_lib_F56FAC82(m);
            err = INET_ENOBUFS;
        } else {
            inetSbAppend(&so->so_snd, m);
            tp->t_force = 1;
            tp->snd_up = tp->snd_una + so->so_snd.sb_cc;
            err = inetTcpOutput(tp);
            tp->t_force = 0;
        }
        break;
    }

    case INET_PRU_SOCKADDR:
        if (inp != NULL) {
            inetInSetSockaddr(inp, nam);
            if (so->so_type == 10) {
                InetSockaddrIn *sin = (InetSockaddrIn *)nam->m_data;
                sin->sin_zero[0] = (u8)(tp->t_p2p_lport & 0xFF);
                sin->sin_zero[1] = (u8)(tp->t_p2p_lport >> 8);
            }
        }
        break;

    case INET_PRU_PROTORCV:
        err = INET_EOPNOTSUPP;
        break;

    case INET_PRU_SLOWTIMO:
        inetTcpTimers(tp, (int)nam);
        break;

    default:
        break;
    }

    sceNet_lib_515B2F33(spl);
    return err;
}

/* [384] 0x029548 */
INET_NOINLINE int inetTcpCtloutput(int op, InetSocket *so, int level, int optname, InetMbuf **mp)
{
    int err = 0;
    int family = so->so_proto->pr_domain->dom_family;
    int spl = sceNet_lib_94B44F26();
    InetInpcbReal *inp;
    InetTcpcb *tp;

    if (family != INET_AF_INET) {
        sceNet_lib_515B2F33(spl);
        return INET_EAFNOSUPPORT;
    }

    inp = (InetInpcbReal *)so->so_pcb;
    if (inp == NULL) {
        sceNet_lib_515B2F33(spl);
        if (op == INET_PRCO_SETOPT && *mp != NULL) {
            sceNetIfhandle_lib_F8825DC4(*mp);
        }
        return INET_ECONNRESET;
    }

    if (level != INET_IPPROTO_TCP) {
        err = inetIpCtloutput(op, so, level, optname, mp);
        sceNet_lib_515B2F33(spl);
        return err;
    }

    tp = (InetTcpcb *)inp->inp_ppcb;
    if (op == INET_PRCO_SETOPT) {
        InetMbuf *m = *mp;
        switch (optname) {
        case 1: /* TCP_NODELAY */
            if (m != NULL && m->m_len >= 4) {
                if (*(s32 *)m->m_data != 0) {
                    tp->t_flags |= 4;
                } else {
                    tp->t_flags &= ~4U;
                }
            } else {
                err = INET_EINVAL;
            }
            break;
        case 2: /* TCP_MAXSEG */
            if (m != NULL) {
                s32 val = *(s32 *)m->m_data;
                if (val > 0 && val <= (s32)tp->t_peermss) {
                    tp->t_peermss = (u16)val;
                } else {
                    err = INET_EINVAL;
                }
            } else {
                err = INET_EINVAL;
            }
            break;
        case 3: /* TCP_OURMSS */
            if (m != NULL) {
                tp->t_ourmss = *(u16 *)m->m_data;
            } else {
                err = INET_EINVAL;
            }
            break;
        default:
            err = INET_ENOPROTOOPT;
            break;
        }
        if (m != NULL) {
            sceNetIfhandle_lib_F8825DC4(m);
        }
    } else if (op == INET_PRCO_GETOPT) {
        InetMbuf *m = sceNetIfhandle_lib_A493AA5F(INET_M_WAIT, INET_MT_SOOPTS);
        *mp = m;
        if (m == NULL) {
            err = INET_ENOBUFS;
        } else {
            m->m_len = 4;
            switch (optname) {
            case 1:
                *(u32 *)m->m_data = tp->t_flags & 4;
                break;
            case 2:
                *(u32 *)m->m_data = tp->t_peermss;
                break;
            case 3:
                *(u16 *)m->m_data = tp->t_ourmss;
                break;
            default:
                err = INET_ENOPROTOOPT;
                break;
            }
        }
    }

    sceNet_lib_515B2F33(spl);
    return err;
}

/* [385] 0x0297A4 */
static INET_NOINLINE int inetTcpAttach(InetSocket *so)
{
    int family = so->so_proto->pr_domain->dom_family;
    int err = 0;

    if (so->so_snd.sb_hiwat == 0 || so->so_rcv.sb_hiwat == 0) {
        err = inetSoReserve(so, g_tcp_sendspace, g_tcp_recvspace);
        if (err != 0) {
            return err;
        }
    }
    if (family != INET_AF_INET) {
        return INET_EAFNOSUPPORT;
    }

    err = inetInPcbAlloc(so, &g_tcbtable);
    if (err == 0) {
        InetInpcbReal *inp = (InetInpcbReal *)so->so_pcb;
        InetTcpcb *tp = (inp != NULL) ? inetTcpNewTcpcb(INET_AF_INET, inp) : NULL;
        if (tp != NULL) {
            tp->t_state = 0;
        } else {
            u16 nofd = so->so_state & INET_SS_NOFDREF;
            so->so_state &= (u16)~INET_SS_NOFDREF;
            if (inp != NULL) {
                inetInPcbDetach(inp);
            }
            so->so_state |= nofd;
            err = INET_ENOBUFS;
        }
    }
    return err;
}

/* [386] 0x029890 */
static INET_NOINLINE InetTcpcb *inetTcpDisconnect(InetTcpcb *tp)
{
    InetSocket *so = (tp->t_inpcb != NULL) ? tp->t_inpcb->inp_socket : NULL;

    if (tp->t_state < 4) {
        return inetTcpClose(tp);
    }
    if ((so->so_options & INET_SO_LINGER) != 0 && so->so_linger == 0) {
        return inetTcpDrop(tp, 0);
    }
    inetSoIsDisconnected(so);
    inetSbFlush(&so->so_rcv);
    tp = inetTcpUsrclosed(tp);
    if (tp != NULL) {
        inetTcpOutput(tp);
    }
    return tp;
}

/* [387] 0x029944 */
static INET_NOINLINE InetTcpcb *inetTcpUsrclosed(InetTcpcb *tp)
{
    switch (tp->t_state) {
    case 0:
    case 1:
    case 2:
        tp->t_state = 0;
        tp = inetTcpClose(tp);
        break;
    case 3:
    case 4:
        tp->t_state = 6;
        break;
    case 5:
        tp->t_state = 8;
        break;
    default:
        break;
    }

    if (tp != NULL && tp->t_state >= 9) {
        InetSocket *so = (tp->t_inpcb != NULL) ? tp->t_inpcb->inp_socket : NULL;
        inetSoIsAborted(so);
        if (tp->t_state == 9 && g_tcp_maxidle > 0) {
            tp->t_timer[3] = g_inet_ticks + (u32)g_tcp_maxidle;
        }
    }
    return tp;
}

/* [388] 0x029A10 */
INET_NOINLINE int inetTcpSysctl(int *name, u32 namelen, void *oldp, u32 *oldlenp, void *newp, u32 newlen)
{
    if (namelen != 1) {
        return INET_ENOTDIR;
    }
    if ((u32)name[0] < 26U && (g_tcp_sysctl_table[name[0]].flags & 1) != 0) {
        if ((g_tcp_sysctl_table[name[0]].flags & 2) != 0) {
            return inetSysctlRdInt(oldp, oldlenp, newp, g_tcp_sysctl_table[name[0]].const_val);
        }
        return inetSysctlInt(oldp, oldlenp, newp, newlen, (int *)g_tcp_sysctl_table[name[0]].var_ptr);
    }
    if (name[0] == 25) {
        return inetSysctlRdStruct(oldp, oldlenp, newp, &g_tcpstat, sizeof(g_tcpstat));
    }
    return INET_ENOPROTOOPT;
}
