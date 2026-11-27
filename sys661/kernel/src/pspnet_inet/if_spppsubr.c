/*
 * PSP 6.61 SceNetInet_Library (pspnet_inet.prx)
 * Synchronous PPP (LCP, IPCP, PAP, CHAP, VJ header compression) (if_spppsubr.c)
 */

#include <stdarg.h>
#include "pspnet_inet_internal.h"

/* PPP control protocol descriptor (60 bytes, matching 0x325F0..0x326DF) */
typedef struct InetSpppCp {
    u16 proto;
    u8 idx;
    u8 flags;
    const char *name;
    void (*up)(InetSppp *sp);
    void (*down)(InetSppp *sp);
    void (*open)(InetSppp *sp);
    void (*close)(InetSppp *sp);
    void (*TO)(void *sp);
    int (*RCR)(InetSppp *sp, u8 *hdr, int len);
    void (*RCN_rej)(InetSppp *sp, u8 *hdr, int len);
    void (*RCN_nak)(InetSppp *sp, u8 *hdr, int len);
    void (*tlu)(InetSppp *sp);
    void (*tld)(InetSppp *sp);
    void (*tls)(InetSppp *sp);
    void (*tlf)(InetSppp *sp);
    void (*scr)(InetSppp *sp);
} InetSpppCp;

/* SPPP ioctl parameter structs */
typedef struct InetSpppAuthCfg {
    char ifname[16];
    char name[128];
    char secret[128];
    u32 timeout;
    u32 max_retry;
} InetSpppAuthCfg;

typedef struct InetSpppStatusIoctl {
    char ifname[16];
    u32 val_a;
    u32 val_b;
    u32 val_c;
    u32 val_d;
    u32 val_e;
} InetSpppStatusIoctl;

static InetCallout g_sppp_keepalive_callout[1];

INET_NOINLINE void inetSpppFlush(InetSppp *sp);
static INET_NOINLINE void inetSpppCpSend(InetSppp *sp, u16 proto, u8 type, u8 ident, u16 len, const void *data);
static INET_NOINLINE void inetSpppCpInput(const InetSpppCp *cp, InetSppp *sp, InetMbuf *m);
static INET_NOINLINE void inetSpppCpChangeState(const InetSpppCp *cp, InetSppp *sp, u32 newstate);
static INET_NOINLINE void inetSpppLcpOpen(InetSppp *sp);
static INET_NOINLINE int inetSpppLcpRcr(InetSppp *sp, u8 *h, int len);
static INET_NOINLINE void inetSpppLcpRcnRej(InetSppp *sp, u8 *h, int len);
static INET_NOINLINE void inetSpppLcpRcnNak(InetSppp *sp, u8 *h, int len);
static INET_NOINLINE void inetSpppLcpTlu(InetSppp *sp);
static INET_NOINLINE void inetSpppLcpScr(InetSppp *sp);
static INET_NOINLINE int inetSpppNcpsOpen(InetSppp *sp);
static INET_NOINLINE void inetSpppLcpCheckTermination(InetSppp *sp);
static INET_NOINLINE int inetSpppIpcpRcr(InetSppp *sp, u8 *h, int len);
static INET_NOINLINE void inetSpppIpcpRcnNak(InetSppp *sp, u8 *h, int len);
static INET_NOINLINE void inetSpppIpcpScr(InetSppp *sp);
static INET_NOINLINE void inetSpppChapInput(InetSppp *sp, InetMbuf *m);
static INET_NOINLINE void inetSpppPapInput(InetSppp *sp, InetMbuf *m);
static INET_NOINLINE void inetSpppAuthSend(const InetSpppCp *cp, InetSppp *sp, u8 type, u8 id, ...);
static INET_NOINLINE void inetSpppKeepalive(void *arg);
static INET_NOINLINE int inetSpppParams(InetSppp *sp, u32 cmd, void *data);
static INET_NOINLINE void inetSpppPhaseNetwork(InetSppp *sp);
static INET_NOINLINE void inetSpppVjCompress(InetSppp *sp, InetMbuf *m);
static INET_NOINLINE InetMbuf *inetSpppVjUncompress(InetSppp *sp, InetMbuf *m);
static INET_NOINLINE int inetSpppStrnlen(const char *str, int maxLen);
static INET_NOINLINE void inetSpppNullEvent(InetSppp *sp);
static INET_NOINLINE void inetSpppLcpUp(InetSppp *sp);
static INET_NOINLINE void inetSpppLcpDown(InetSppp *sp);
static INET_NOINLINE void inetSpppLcpClose(InetSppp *sp);
static INET_NOINLINE void inetSpppLcpTo(void *arg);
static INET_NOINLINE void inetSpppLcpTld(InetSppp *sp);
static INET_NOINLINE void inetSpppLcpTls(InetSppp *sp);
static INET_NOINLINE void inetSpppLcpTlf(InetSppp *sp);
static INET_NOINLINE void inetSpppIpcpUp(InetSppp *sp);
static INET_NOINLINE void inetSpppIpcpDown(InetSppp *sp);
static INET_NOINLINE void inetSpppIpcpOpen(InetSppp *sp);
static INET_NOINLINE void inetSpppIpcpClose(InetSppp *sp);
static INET_NOINLINE void inetSpppIpcpTo(void *arg);
static INET_NOINLINE void inetSpppIpcpRcnRej(InetSppp *sp, u8 *h, int len);
static INET_NOINLINE void inetSpppIpcpTlu(InetSppp *sp);
static INET_NOINLINE void inetSpppIpcpTld(InetSppp *sp);
static INET_NOINLINE void inetSpppIpcpTls(InetSppp *sp);
static INET_NOINLINE void inetSpppIpcpTlf(InetSppp *sp);
static INET_NOINLINE void inetSpppPapOpen(InetSppp *sp);
static INET_NOINLINE void inetSpppPapClose(InetSppp *sp);
static INET_NOINLINE void inetSpppPapToNoop(void *arg);
static INET_NOINLINE void inetSpppPapMyTo(void *arg);
static INET_NOINLINE void inetSpppPapTluNoop(InetSppp *sp);
static INET_NOINLINE void inetSpppPapTld(InetSppp *sp);
static INET_NOINLINE void inetSpppPapScr(InetSppp *sp);
static INET_NOINLINE void inetSpppChapOpen(InetSppp *sp);
static INET_NOINLINE void inetSpppChapClose(InetSppp *sp);
static INET_NOINLINE void inetSpppChapToNoop(void *arg);
static INET_NOINLINE void inetSpppChapMyTo(void *arg);
static INET_NOINLINE void inetSpppChapTluNoop(InetSppp *sp);
static INET_NOINLINE void inetSpppChapTld(InetSppp *sp);
static INET_NOINLINE void inetSpppChapScrNoop(InetSppp *sp);
static INET_NOINLINE void inetSpppCpTimeout(const InetSpppCp *cp, InetSppp *sp);

static InetSpppCp g_sppp_lcp = {
    0xC021, 0, 0x01, "lcp",
    inetSpppLcpUp, inetSpppLcpDown, inetSpppLcpOpen, inetSpppLcpClose,
    inetSpppLcpTo, inetSpppLcpRcr, inetSpppLcpRcnRej, inetSpppLcpRcnNak,
    inetSpppLcpTlu, inetSpppLcpTld, inetSpppLcpTls, inetSpppLcpTlf,
    inetSpppLcpScr
};

static InetSpppCp g_sppp_ipcp = {
    0x8021, 1, 0x04, "ipcp",
    inetSpppIpcpUp, inetSpppIpcpDown, inetSpppIpcpOpen, inetSpppIpcpClose,
    inetSpppIpcpTo, inetSpppIpcpRcr, inetSpppIpcpRcnRej, inetSpppIpcpRcnNak,
    inetSpppIpcpTlu, inetSpppIpcpTld, inetSpppIpcpTls, inetSpppIpcpTlf,
    inetSpppIpcpScr
};

static InetSpppCp g_sppp_pap = {
    0xC023, 2, 0x02, "pap",
    inetSpppNullEvent, inetSpppNullEvent, inetSpppPapOpen, inetSpppPapClose,
    inetSpppPapToNoop, NULL, NULL, NULL,
    inetSpppPapTluNoop, inetSpppPapTld, inetSpppNullEvent, inetSpppNullEvent,
    inetSpppPapScr
};

static InetSpppCp g_sppp_chap = {
    0xC223, 3, 0x02, "chap",
    inetSpppNullEvent, inetSpppNullEvent, inetSpppChapOpen, inetSpppChapClose,
    inetSpppChapToNoop, NULL, NULL, NULL,
    inetSpppChapTluNoop, inetSpppChapTld, inetSpppNullEvent, inetSpppNullEvent,
    inetSpppChapScrNoop
};

static InetSpppCp *g_sppp_cps[4] = {
    &g_sppp_lcp, &g_sppp_ipcp, &g_sppp_pap, &g_sppp_chap
};

static __inline__ __attribute__((always_inline)) void spppResetState(InetSppp *sp)
{
    u32 mtu = (u32)sp->pp_if.if_data.ifi_mtu;
    sp->lcp_opts = 0x20;
    sp->lcp_mru = mtu;
    sp->lcp_their_mru = mtu;
    sp->lcp_magic = 0;
    sp->lcp_protos = 0;
    sp->pp_max_phase = 0;
    sp->state[0] = 0;
    sp->state[1] = 0;
    sp->state[2] = 2;
    sp->state[3] = 2;
    sp->fail_counter[0] = 0;
    sp->fail_counter[1] = 0;
    sp->fail_counter[2] = 0;
    sp->fail_counter[3] = 0;
    sp->pp_seq[0] = 0;
    sp->pp_seq[1] = 0;
    sp->pp_seq[2] = 0;
    sp->pp_seq[3] = 0;
    sp->pp_rseq[0] = 0;
    sp->pp_rseq[1] = 0;
    sp->pp_rseq[2] = 0;
    sp->pp_rseq[3] = 0;
    sp->ipcp_opts = 0;
    sp->ipcp_flags = 0;
}

/* [389] 0x029AE4 */
INET_NOINLINE void inetSpppInput(InetIfnet *ifp, InetMbuf *m)
{
    InetSppp *sp = (InetSppp *)ifp;
    u16 proto = 0;
    InetIfQueue *inq = NULL;

    if ((ifp->if_flags & INET_IFF_UP) != 0) {
        ifp->if_data.ifi_ibytes += (u32)(M_PKTLEN(m) + (s32)sp->pp_framebytes);
    }

    if (M_PKTLEN(m) < 5) {
        ifp->if_data.ifi_ierrors++;
        ifp->if_data.ifi_iqdrops++;
        sceNetIfhandle_lib_F56FAC82(m);
        return;
    }

    if (sp->ipcp_his_vj != 0) {
        m = inetSpppVjUncompress(sp, m);
        if (m == NULL) {
            ifp->if_data.ifi_ierrors++;
            ifp->if_data.ifi_iqdrops++;
            sceNetIfhandle_lib_F56FAC82(m);
            return;
        }
    }

    if ((sp->pp_flags & 0x20) != 0) {
        sceKernelMemcpy(&proto, m->m_data, 2);
        proto = inetBswap16(proto);
        sceNetIfhandle_lib_BFF3CEA5(m, 2);
    } else {
        u8 *hdr = m->m_data;
        sceNetIfhandle_lib_BFF3CEA5(m, 4);
        if (hdr[0] != 0xFF || hdr[1] != 0x03) {
            ifp->if_data.ifi_ierrors++;
            ifp->if_data.ifi_iqdrops++;
            sceNetIfhandle_lib_F56FAC82(m);
            return;
        }
        proto = (u16)(((u16)hdr[2] << 8) | hdr[3]);
    }

    switch (proto) {
    case 0xC021:
        inetSpppCpInput(&g_sppp_lcp, sp, m);
        sceNetIfhandle_lib_F56FAC82(m);
        return;
    case 0x8021:
        if (sp->pp_phase == 4) {
            inetSpppCpInput(&g_sppp_ipcp, sp, m);
        }
        sceNetIfhandle_lib_F56FAC82(m);
        return;
    case 0xC023:
        if (sp->pp_phase >= 3) {
            inetSpppPapInput(sp, m);
        }
        sceNetIfhandle_lib_F56FAC82(m);
        return;
    case 0xC223:
        if (sp->pp_phase >= 3) {
            inetSpppChapInput(sp, m);
        }
        sceNetIfhandle_lib_F56FAC82(m);
        return;
    case 0x0021:
        if (sp->state[1] == 9) {
            sceNet_lib_7C3B86C5(2);
            inq = sceNetInet_lib_D7928A6D();
        }
        if ((ifp->if_flags & INET_IFF_UP) != 0 && inq != NULL) {
            int spl = sceNet_lib_94B44F26();
            if (inq->ifq_len >= inq->ifq_maxlen) {
                inq->ifq_drops++;
                sceNet_lib_515B2F33(spl);
            } else {
                m->m_nextpkt = NULL;
                if (inq->ifq_tail != NULL) {
                    inq->ifq_tail->m_nextpkt = m;
                } else {
                    inq->ifq_head = m;
                }
                inq->ifq_tail = m;
                inq->ifq_len++;
                sceNet_lib_515B2F33(spl);
                return;
            }
        }
        break;
    default:
        if (sp->state[0] == 9) {
            u16 netProto = inetBswap16(proto);
            sp->pp_seq[0]++;
            inetSpppCpSend(sp, 0xC021, 8, (u8)sp->pp_seq[0], (u16)(M_PKTLEN(m) + 2), &netProto);
        }
        ifp->if_data.ifi_noproto++;
        break;
    }

    ifp->if_data.ifi_ierrors++;
    ifp->if_data.ifi_iqdrops++;
    sceNetIfhandle_lib_F56FAC82(m);
}

/* [390] 0x029E20 */
INET_NOINLINE int inetSpppOutput(InetIfnet *ifp, InetMbuf *m, InetSockaddr *dst, InetRtentry *rt)
{
    InetSppp *sp = (InetSppp *)ifp;
    int spl = sceNet_lib_94B44F26();
    int err = 0;
    u8 *hdrPtr = NULL;
    u16 protoNet;

    (void)rt;
    if ((ifp->if_flags & (INET_IFF_UP | INET_IFF_RUNNING)) != (INET_IFF_UP | INET_IFF_RUNNING)) {
        sceNetIfhandle_lib_F56FAC82(m);
        sceNet_lib_515B2F33(spl);
        return INET_ENETDOWN;
    }

    if (dst->sa_family == INET_AF_INET && m->m_len >= 20 && m->m_data != NULL) {
        InetIpHdr *ip = (InetIpHdr *)m->m_data;
        if (ip->ip_src == 0) {
            u8 ipProto = ip->ip_p;
            sceNetIfhandle_lib_F56FAC82(m);
            sceNet_lib_515B2F33(spl);
            return (ipProto == INET_IPPROTO_TCP) ? INET_EADDRNOTAVAIL : 0;
        }
    }

    if ((sp->pp_flags & 0x20) == 0) {
        if ((m->m_flags & INET_M_EXT) == 0) {
            u32 lead = ((m->m_flags & INET_M_PKTHDR) != 0)
                           ? (u32)(m->m_data - M_PKTDAT(m))
                           : (u32)(m->m_data - M_DAT(m));
            if (lead >= 4) {
                m->m_data -= 4;
                m->m_len += 4;
            } else {
                m = sceNetIfhandle_lib_C3325FDC(m, 4, INET_M_DONTWAIT);
            }
        } else {
            m = sceNetIfhandle_lib_C3325FDC(m, 4, INET_M_DONTWAIT);
        }
        if (m == NULL) {
            ifp->if_data.ifi_oerrors++;
            sceNet_lib_515B2F33(spl);
            return INET_ENOBUFS;
        }
        if ((m->m_flags & INET_M_PKTHDR) != 0) {
            M_PKTLEN(m) += 4;
        }
        hdrPtr = m->m_data;
        hdrPtr[0] = 0xFF;
        hdrPtr[1] = 0x03;
    }

    if (dst->sa_family != INET_AF_INET) {
        sceNetIfhandle_lib_F56FAC82(m);
        ifp->if_data.ifi_oerrors++;
        sceNet_lib_515B2F33(spl);
        return INET_EAFNOSUPPORT;
    }

    protoNet = inetBswap16(0x0021);
    if (sp->state[1] != 9) {
        err = INET_ENETDOWN;
    }
    if (sp->ipcp_my_vj != 0) {
        inetSpppVjCompress(sp, m);
    }

    if ((sp->pp_flags & 0x20) != 0) {
        if ((m->m_flags & INET_M_EXT) == 0) {
            u32 lead = ((m->m_flags & INET_M_PKTHDR) != 0)
                           ? (u32)(m->m_data - M_PKTDAT(m))
                           : (u32)(m->m_data - M_DAT(m));
            if (lead >= 2) {
                m->m_data -= 2;
                m->m_len += 2;
            } else {
                m = sceNetIfhandle_lib_C3325FDC(m, 2, INET_M_DONTWAIT);
            }
        } else {
            m = sceNetIfhandle_lib_C3325FDC(m, 2, INET_M_DONTWAIT);
        }
        if (m == NULL) {
            ifp->if_data.ifi_oerrors++;
            sceNet_lib_515B2F33(spl);
            return INET_ENOBUFS;
        }
        if ((m->m_flags & INET_M_PKTHDR) != 0) {
            M_PKTLEN(m) += 2;
        }
        sceKernelMemcpy(m->m_data, &protoNet, 2);
    } else {
        sceKernelMemcpy(hdrPtr + 2, &protoNet, 2);
    }

    {
        s32 pktLen = M_PKTLEN(m);
        if (ifp->if_snd.ifq_len >= ifp->if_snd.ifq_maxlen) {
            ifp->if_snd.ifq_drops++;
            sceNetIfhandle_lib_F56FAC82(m);
            if (err == 0) {
                err = INET_ENOBUFS;
            }
        } else {
            m->m_nextpkt = NULL;
            if (ifp->if_snd.ifq_tail != NULL) {
                ifp->if_snd.ifq_tail->m_nextpkt = m;
            } else {
                ifp->if_snd.ifq_head = m;
            }
            ifp->if_snd.ifq_tail = m;
            ifp->if_snd.ifq_len++;
        }

        if (err != 0) {
            ifp->if_data.ifi_oerrors++;
            sceNet_lib_515B2F33(spl);
            return err;
        }

        if ((ifp->if_flags & INET_IFF_OACTIVE) == 0 && ifp->if_start != NULL) {
            ifp->if_start(ifp);
        }
        ifp->if_data.ifi_obytes += (u32)(pktLen + (s32)sp->pp_framebytes);
    }
    sceNet_lib_515B2F33(spl);
    return 0;
}

/* [391] 0x02A1E4 */
INET_NOINLINE void inetSpppFlush(InetSppp *sp)
{
    InetIfnet *ifp = &sp->pp_if;
    while (ifp->if_snd.ifq_head != NULL) {
        InetMbuf *m = ifp->if_snd.ifq_head;
        ifp->if_snd.ifq_head = m->m_nextpkt;
        if (ifp->if_snd.ifq_head == NULL) {
            ifp->if_snd.ifq_tail = NULL;
        }
        m->m_nextpkt = NULL;
        ifp->if_snd.ifq_len--;
        sceNetIfhandle_lib_F56FAC82(m);
    }
    while (sp->pp_fastq.ifq_head != NULL) {
        InetMbuf *m = sp->pp_fastq.ifq_head;
        sp->pp_fastq.ifq_head = m->m_nextpkt;
        if (sp->pp_fastq.ifq_head == NULL) {
            sp->pp_fastq.ifq_tail = NULL;
        }
        m->m_nextpkt = NULL;
        sp->pp_fastq.ifq_len--;
        sceNetIfhandle_lib_F56FAC82(m);
    }
}

/* [392] 0x02A274 */
INET_NOINLINE InetMbuf *inetSpppDequeue(InetIfnet *ifp)
{
    InetSppp *sp = (InetSppp *)ifp;
    int spl = sceNet_lib_94B44F26();
    InetMbuf *m = sp->pp_fastq.ifq_head;

    if (m != NULL) {
        sp->pp_fastq.ifq_head = m->m_nextpkt;
        if (sp->pp_fastq.ifq_head == NULL) {
            sp->pp_fastq.ifq_tail = NULL;
        }
        m->m_nextpkt = NULL;
        sp->pp_fastq.ifq_len--;
    } else if (inetSpppNcpsOpen(sp) != 0) {
        m = ifp->if_snd.ifq_head;
        if (m != NULL) {
            ifp->if_snd.ifq_head = m->m_nextpkt;
            if (ifp->if_snd.ifq_head == NULL) {
                ifp->if_snd.ifq_tail = NULL;
            }
            m->m_nextpkt = NULL;
            ifp->if_snd.ifq_len--;
        }
    }
    sceNet_lib_515B2F33(spl);
    return m;
}

/* [393] 0x02A324 */
INET_NOINLINE int inetSpppIoctl(InetIfnet *ifp, u32 cmd, void *data)
{
    InetSppp *sp = (InetSppp *)ifp;
    int err = 0;
    int spl = sceNet_lib_94B44F26();
    u32 *uarg = (u32 *)data;

    switch (cmd) {
    case 0x8020690CU: /* SIOCSIFADDR */
        sceNet_lib_FA6DE6A6(ifp, 0);
        /* fall through to flags handling */
    case 0x80206910U: { /* SIOCSIFFLAGS */
        u16 f = ifp->if_flags;
        int goingUp = (f & (INET_IFF_UP | INET_IFF_RUNNING)) == INET_IFF_UP;
        int goingDown = (f & (INET_IFF_UP | INET_IFF_RUNNING)) == INET_IFF_RUNNING;
        if (goingUp || goingDown) {
            inetSpppLcpClose(sp);
        }
        if (goingUp) {
            ifp->if_flags |= INET_IFF_RUNNING;
            inetSpppLcpOpen(sp);
        } else if (goingDown) {
            inetSpppFlush(sp);
            ifp->if_flags &= (u16)~INET_IFF_RUNNING;
        }
        break;
    }
    case 0x8020690EU: /* SIOCSIFDSTADDR */
    case 0x80206931U: /* SIOCADDMULTI */
    case 0x80206932U: /* SIOCDELMULTI */
    case 0x8040691AU: /* SIOCAIFADDR */
        break;
    case 0x8020697FU: { /* SIOCSIFMTU */
        u32 mtu = uarg[4];
        if (mtu < 128 || mtu > sp->lcp_their_mru) {
            err = INET_EINVAL;
        } else {
            ifp->if_data.ifi_mtu = mtu;
        }
        break;
    }
    case 0xC020697EU: /* SIOCGIFMTU */
        uarg[4] = (u32)ifp->if_data.ifi_mtu;
        break;
    case 0x8014697CU:
    case 0x8024697AU:
    case 0x81186979U:
    case 0xC014697BU:
    case 0xC014697DU:
    case 0xC0246978U:
        err = inetSpppParams(sp, cmd, data);
        break;
    default:
        err = INET_ENOTTY;
        break;
    }

    sceNet_lib_515B2F33(spl);
    return err;
}

/* [394] 0x02A558 */
static INET_NOINLINE void inetSpppCpSend(InetSppp *sp, u16 proto, u8 type, u8 ident, u16 len, const void *data)
{
    InetIfnet *ifp = &sp->pp_if;
    u32 hdrOff = ((sp->pp_flags & 0x20) != 0) ? 2U : 4U;
    InetMbuf *m;
    u16 protoNet;
    u16 cpLenNet;
    u8 *cpHdr;

    if (len > 204U - hdrOff) {
        len = (u16)(204U - hdrOff);
    }

    m = (InetMbuf *)sceNetIfhandle_lib_15CFE3C0(sizeof(InetMbuf));
    if (m == NULL) {
        return;
    }

    m->m_next = NULL;
    m->m_nextpkt = NULL;
    m->m_data = M_PKTDAT(m);
    m->m_type = INET_MT_DATA;
    m->m_flags = INET_M_PKTHDR;
    M_RCVIF(m) = NULL;
    M_CSUM(m) = 0;
    m->m_len = (s32)(hdrOff + len + 4U);
    M_PKTLEN(m) = m->m_len;

    protoNet = inetBswap16(proto);
    if ((sp->pp_flags & 0x20) != 0) {
        u8 *pktBuf = M_PKTDAT(m);
        sceKernelMemcpy(pktBuf, &protoNet, 2);
        cpHdr = pktBuf + 2;
    } else {
        u8 *pktBuf = M_PKTDAT(m);
        pktBuf[0] = 0xFF;
        pktBuf[1] = 0x03;
        sceKernelMemcpy(pktBuf + 2, &protoNet, 2);
        cpHdr = pktBuf + 4;
    }

    cpHdr[0] = type;
    cpHdr[1] = ident;
    cpLenNet = inetBswap16((u16)(len + 4U));
    sceKernelMemcpy(cpHdr + 2, &cpLenNet, 2);
    if (len != 0) {
        sceNet_lib_2F305274(data, cpHdr + 4, len);
    }

    if (sp->pp_fastq.ifq_len >= sp->pp_fastq.ifq_maxlen) {
        ifp->if_snd.ifq_drops++;
        sceNetIfhandle_lib_F56FAC82(m);
        ifp->if_data.ifi_oerrors++;
        return;
    }

    m->m_nextpkt = NULL;
    if (sp->pp_fastq.ifq_tail != NULL) {
        sp->pp_fastq.ifq_tail->m_nextpkt = m;
    } else {
        sp->pp_fastq.ifq_head = m;
    }
    sp->pp_fastq.ifq_tail = m;
    sp->pp_fastq.ifq_len++;

    if ((ifp->if_flags & INET_IFF_OACTIVE) == 0 && ifp->if_start != NULL) {
        ifp->if_start(ifp);
    }
    ifp->if_data.ifi_obytes += (u32)(M_PKTLEN(m) + (s32)sp->pp_framebytes);
}

/* [395] 0x02A790 */
static INET_NOINLINE void inetSpppCpInput(const InetSpppCp *cp, InetSppp *sp, InetMbuf *m)
{
    s32 len = M_PKTLEN(m);
    u8 *h;
    u16 hlen;
    u8 code;
    u8 ident;
    int bad = 0;

    if (len < 4) {
        return;
    }

    h = m->m_data;
    hlen = (u16)(((u16)h[2] << 8) | h[3]);
    if (hlen < len) {
        len = hlen;
    }
    code = h[0];
    ident = h[1];

    switch (code) {
    case 1: /* CONF_REQ */
        if (len < 4) {
            bad = 1;
        } else if (sp->state[cp->idx] == 2) {
            inetSpppCpSend(sp, cp->proto, 6, ident, 0, NULL);
        } else if (sp->state[cp->idx] < 2 || sp->state[cp->idx] > 5) {
            int rcr = cp->RCR(sp, h, len);
            switch (sp->state[cp->idx]) {
            case 3:
                sp->rst_counter[cp->idx] = (s32)sp->lcp_max_configure;
                cp->scr(sp);
                inetSpppCpChangeState(cp, sp, rcr ? 8 : 6);
                break;
            case 6:
            case 8:
                inetSpppCpChangeState(cp, sp, rcr ? 8 : 6);
                break;
            case 7:
                if (rcr) {
                    inetSpppCpChangeState(cp, sp, 9);
                    cp->tlu(sp);
                } else {
                    inetSpppCpChangeState(cp, sp, 7);
                }
                break;
            case 9:
                cp->tld(sp);
                cp->scr(sp);
                inetSpppCpChangeState(cp, sp, rcr ? 8 : 6);
                break;
            default:
                bad = 1;
                break;
            }
        }
        break;

    case 2: /* CONF_ACK */
        if (sp->confid[cp->idx] != ident) {
            bad = 1;
        } else {
            switch (sp->state[cp->idx]) {
            case 2:
            case 3:
                inetSpppCpSend(sp, cp->proto, 6, ident, 0, NULL);
                break;
            case 4:
            case 5:
                break;
            case 6:
                sp->rst_counter[cp->idx] = (s32)sp->lcp_max_configure;
                inetSpppCpChangeState(cp, sp, 7);
                break;
            case 7:
                cp->scr(sp);
                inetSpppCpChangeState(cp, sp, 6);
                break;
            case 8:
                sp->rst_counter[cp->idx] = (s32)sp->lcp_max_configure;
                inetSpppCpChangeState(cp, sp, 9);
                cp->tlu(sp);
                break;
            case 9:
                cp->tld(sp);
                cp->scr(sp);
                inetSpppCpChangeState(cp, sp, 6);
                break;
            default:
                bad = 1;
                break;
            }
        }
        break;

    case 3: /* CONF_NAK */
    case 4: /* CONF_REJ */
        if (sp->confid[cp->idx] != ident) {
            bad = 1;
        } else {
            if (code == 3) {
                cp->RCN_nak(sp, h, len);
            } else {
                cp->RCN_rej(sp, h, len);
            }
            switch (sp->state[cp->idx]) {
            case 2:
            case 3:
                inetSpppCpSend(sp, cp->proto, 6, ident, 0, NULL);
                break;
            case 4:
            case 5:
                break;
            case 6:
            case 8:
                sp->rst_counter[cp->idx] = (s32)sp->lcp_max_configure;
                cp->scr(sp);
                break;
            case 7:
                inetSpppCpChangeState(cp, sp, 8);
                cp->scr(sp);
                break;
            case 9:
                cp->tld(sp);
                inetSpppCpChangeState(cp, sp, 8);
                cp->scr(sp);
                break;
            default:
                bad = 1;
                break;
            }
        }
        break;

    case 5: /* TERM_REQ */
        switch (sp->state[cp->idx]) {
        case 2:
        case 3:
        case 4:
        case 5:
        case 6:
            inetSpppCpSend(sp, cp->proto, 6, ident, 0, NULL);
            break;
        case 7:
        case 8:
            inetSpppCpChangeState(cp, sp, 6);
            inetSpppCpSend(sp, cp->proto, 6, ident, 0, NULL);
            break;
        case 9:
            cp->tld(sp);
            sp->rst_counter[cp->idx] = 0;
            inetSpppCpChangeState(cp, sp, 5);
            inetSpppCpSend(sp, cp->proto, 6, ident, 0, NULL);
            break;
        default:
            bad = 1;
            break;
        }
        break;

    case 6: /* TERM_ACK */
        switch (sp->state[cp->idx]) {
        case 2:
        case 3:
        case 6:
        case 8:
            break;
        case 4:
            cp->tlf(sp);
            inetSpppCpChangeState(cp, sp, 2);
            inetSpppLcpCheckTermination(sp);
            break;
        case 5:
            cp->tlf(sp);
            inetSpppCpChangeState(cp, sp, 3);
            inetSpppLcpCheckTermination(sp);
            break;
        case 7:
            inetSpppCpChangeState(cp, sp, 6);
            break;
        case 9:
            cp->tld(sp);
            cp->scr(sp);
            inetSpppCpChangeState(cp, sp, 7);
            break;
        default:
            bad = 1;
            break;
        }
        break;

    case 7: /* CODE_REJ */
        if (sp->state[cp->idx] <= 1 || sp->state[cp->idx] >= 10) {
            bad = 1;
        } else if (sp->state[cp->idx] == 7) {
            inetSpppCpChangeState(cp, sp, 6);
        }
        break;

    case 8: { /* PROTO_REJ */
        u16 rejProto = (u16)(((u16)h[4] << 8) | h[5]);
        const InetSpppCp *target = NULL;
        int idx;
        for (idx = 0; idx < 4; idx++) {
            if (g_sppp_cps[idx]->proto == rejProto) {
                target = g_sppp_cps[idx];
                break;
            }
        }
        if (target != NULL && sp->state[target->idx] == 6) {
            target->close(sp);
        } else {
            if (sp->state[cp->idx] <= 1 || sp->state[cp->idx] >= 10) {
                bad = 1;
            } else if (sp->state[cp->idx] == 7) {
                inetSpppCpChangeState(cp, sp, 6);
            }
        }
        break;
    }

    case 9: /* ECHO_REQ */
        if (cp->proto != 0xC021) {
            sp->pp_seq[cp->idx]++;
            inetSpppCpSend(sp, cp->proto, 7, (u8)sp->pp_seq[cp->idx], (u16)M_PKTLEN(m), h);
            bad = 1;
        } else if (sp->state[cp->idx] != 9) {
            bad = 1;
        } else if (len >= 8) {
            u32 rxMagic;
            sceKernelMemcpy(&rxMagic, h + 4, 4);
            if (inetBswap32(rxMagic) != sp->lcp_magic) {
                u32 txMagic = inetBswap32(sp->lcp_magic);
                sceKernelMemcpy(h + 4, &txMagic, 4);
                inetSpppCpSend(sp, 0xC021, 10, ident, (u16)(len - 4), h + 4);
            }
        }
        break;

    case 10: /* ECHO_REPLY */
        if (cp->proto != 0xC021) {
            sp->pp_seq[cp->idx]++;
            inetSpppCpSend(sp, cp->proto, 7, (u8)sp->pp_seq[cp->idx], (u16)M_PKTLEN(m), h);
            bad = 1;
        } else if (sp->lcp_echoid != ident) {
            bad = 1;
        } else if (len >= 8) {
            u32 rxMagic;
            sceKernelMemcpy(&rxMagic, h + 4, 4);
            if (inetBswap32(rxMagic) != sp->lcp_magic) {
                sp->pp_alivecnt = 0;
            }
        }
        break;

    case 11: /* DISC_REQ */
        if (cp->proto != 0xC021) {
            sp->pp_seq[cp->idx]++;
            inetSpppCpSend(sp, cp->proto, 7, (u8)sp->pp_seq[cp->idx], (u16)M_PKTLEN(m), h);
            bad = 1;
        }
        break;

    default:
        sp->pp_seq[cp->idx]++;
        inetSpppCpSend(sp, cp->proto, 7, (u8)sp->pp_seq[cp->idx], (u16)M_PKTLEN(m), h);
        bad = 1;
        break;
    }

    if (bad) {
        sp->pp_if.if_data.ifi_ierrors++;
    }
}

/* [396] 0x02AE38 */
static INET_NOINLINE void inetSpppCpChangeState(const InetSpppCp *cp, InetSppp *sp, u32 newstate)
{
    sp->state[cp->idx] = newstate;
    sceNet_lib_A93A93E9(sp->ch[cp->idx]);
    if (newstate >= 4 && newstate <= 8) {
        sceNet_lib_7C86FBA4(sp->ch[cp->idx], sp->lcp_timeout, cp->TO, sp);
    }
}

/* [397] 0x02AEEC */
static INET_NOINLINE void inetSpppLcpOpen(InetSppp *sp)
{
    u32 cfg = sp->lcp_flags_cfg;
    sp->pp_flags &= ~0x10U;

    if ((cfg & 1) != 0) {
        sp->lcp_my_accm = 0xFFFFFFFFU;
        sp->lcp_opts |= 4;
    } else {
        sp->lcp_my_accm = 0;
        sp->lcp_opts &= ~4U;
    }
    sp->lcp_their_accm = 0xFFFFFFFFU;

    if ((cfg & 2) != 0) {
        sp->lcp_my_pcomp = 1;
        sp->lcp_opts |= 0x80;
    } else {
        sp->lcp_my_pcomp = 0;
        sp->lcp_opts &= ~0x80U;
    }
    sp->lcp_their_pcomp = 0;

    if ((cfg & 4) != 0) {
        sp->lcp_my_accomp = 1;
        sp->lcp_opts |= 0x100;
    } else {
        sp->lcp_my_accomp = 0;
        sp->lcp_opts &= ~0x100U;
    }
    sp->lcp_their_accomp = 0;

    switch (sp->state[0]) {
    case 0:
        inetSpppLcpTls(sp);
        inetSpppCpChangeState(&g_sppp_lcp, sp, 1);
        break;
    case 2:
        sp->rst_counter[0] = (s32)sp->lcp_max_configure;
        inetSpppLcpScr(sp);
        inetSpppCpChangeState(&g_sppp_lcp, sp, 6);
        break;
    case 4:
        inetSpppCpChangeState(&g_sppp_lcp, sp, 5);
        break;
    default:
        break;
    }
}

/* [398] 0x02B05C */
static INET_NOINLINE int inetSpppLcpRcr(InetSppp *sp, u8 *h, int len)
{
    int optBytes = len - 4;
    u8 *buf = (u8 *)sceNetIfhandle_lib_15CFE3C0(optBytes);
    u8 *r;
    u8 *p;
    int rlen = 0;
    int rem;
    int forceRej = 0;

    if (buf == NULL) {
        return 0;
    }

    /* Pass 1: check for rejected options */
    r = buf;
    p = h + 4;
    rem = optBytes;
    while (rem >= 2 && p[1] != 0) {
        int reject = 0;
        u8 opt = p[0];
        u8 olen = p[1];
        switch (opt) {
        case 1:
            if (rem < 4 || olen != 4) {
                reject = 1;
            }
            break;
        case 2:
        case 5:
            if (rem < 6 || olen != 6) {
                reject = 1;
            }
            break;
        case 3:
            if (rem < 4) {
                reject = 1;
            } else {
                u16 authProto = (u16)(((u16)p[2] << 8) | p[3]);
                if ((authProto == 0xC023 && olen != 4) ||
                    (authProto == 0xC223 && olen != 5)) {
                    reject = 1;
                } else {
                    sp->pp_flags |= 0x10;
                }
            }
            break;
        case 7:
        case 8:
            if ((sp->pp_flags & 0x20) != 0 || rem < 2 || olen != 2) {
                reject = 1;
            }
            break;
        default:
            reject = 1;
            break;
        }
        if (reject) {
            sceNet_lib_2F305274(p, r, olen);
            rlen += olen;
            r += olen;
        }
        rem -= olen;
        p += olen;
    }

    if (rlen != 0) {
        forceRej = 1;
    } else {
        /* Pass 2: check for NAKed options and apply accepted ones */
        p = h + 4;
        rem = optBytes;
        while (rem >= 2 && p[1] != 0) {
            u8 opt = p[0];
            u8 olen = p[1];
            int nakLen = 0;
            switch (opt) {
            case 1:
                sp->lcp_their_mru = ((u32)p[2] << 8) + p[3];
                break;
            case 2:
                sp->lcp_their_accm = ((u32)p[2] << 24) | ((u32)p[3] << 16) |
                                     ((u32)p[4] << 8) | (u32)p[5];
                break;
            case 3: {
                u16 authProto = (u16)(((u16)p[2] << 8) | p[3]);
                if (authProto != 0xC023 && authProto != 0xC223) {
                    sp->myauth_proto = 0xC023;
                    r[0] = opt;
                    r[1] = 4;
                    r[2] = 0xC0;
                    r[3] = 0x23;
                    nakLen = 4;
                } else if (authProto == 0xC223 && p[4] != 5) {
                    sp->myauth_proto = 0xC223;
                    r[0] = opt;
                    r[1] = 5;
                    r[2] = 0xC2;
                    r[3] = 0x23;
                    r[4] = 5;
                    nakLen = 5;
                } else {
                    sp->myauth_proto = authProto;
                }
                break;
            }
            case 5: {
                u32 peerMagic = ((u32)p[2] << 24) | ((u32)p[3] << 16) |
                                ((u32)p[4] << 8) | (u32)p[5];
                if (peerMagic == sp->lcp_magic) {
                    u32 inv = ~peerMagic;
                    r[0] = opt;
                    r[1] = 6;
                    r[2] = (u8)(inv >> 24);
                    r[3] = (u8)(inv >> 16);
                    r[4] = (u8)(inv >> 8);
                    r[5] = (u8)inv;
                    nakLen = 6;
                }
                break;
            }
            case 7:
                sp->lcp_their_pcomp = 1;
                break;
            case 8:
                sp->lcp_their_accomp = 1;
                break;
            default:
                break;
            }
            if (nakLen != 0) {
                rlen += nakLen;
                r += nakLen;
            }
            rem -= olen;
            p += olen;
        }
    }

    if (forceRej) {
        inetSpppCpSend(sp, 0xC021, 4, h[1], (u16)rlen, buf);
    } else if (rlen != 0) {
        sp->fail_counter[0]++;
        if (sp->fail_counter[0] >= (s32)sp->lcp_max_failure) {
            inetSpppCpSend(sp, 0xC021, 4, h[1], (u16)rlen, buf);
        } else {
            inetSpppCpSend(sp, 0xC021, 3, h[1], (u16)rlen, buf);
        }
    } else {
        sp->fail_counter[0] = 0;
        inetSpppCpSend(sp, 0xC021, 2, h[1], (u16)optBytes, h + 4);
    }

    sceNetIfhandle_lib_76BAD213(buf);
    return (rlen == 0);
}

/* [399] 0x02B4A0 */
static INET_NOINLINE void inetSpppLcpRcnRej(InetSppp *sp, u8 *h, int len)
{
    int rem = len - 4;
    void *tmp = sceNetIfhandle_lib_15CFE3C0(rem);
    u8 *p = h + 4;

    if (tmp == NULL) {
        return;
    }

    while (rem >= 2 && p[1] != 0) {
        u8 olen = p[1];
        switch (p[0]) {
        case 1:
            sp->lcp_opts &= ~2U;
            break;
        case 2:
            sp->lcp_opts &= ~4U;
            break;
        case 5:
            sp->lcp_magic = 0;
            sp->lcp_opts &= ~0x20U;
            break;
        case 7:
            sp->lcp_my_pcomp = 0;
            sp->lcp_opts &= ~0x80U;
            break;
        case 8:
            sp->lcp_my_accomp = 0;
            sp->lcp_opts &= ~0x100U;
            break;
        default:
            break;
        }
        rem -= olen;
        p += olen;
    }
    sceNetIfhandle_lib_76BAD213(tmp);
}

/* [400] 0x02B5A0 */
static INET_NOINLINE void inetSpppLcpRcnNak(InetSppp *sp, u8 *h, int len)
{
    int rem = len - 4;
    void *tmp = sceNetIfhandle_lib_15CFE3C0(rem);
    u8 *p = h + 4;

    if (tmp == NULL) {
        return;
    }

    while (rem >= 2 && p[1] != 0) {
        u8 olen = p[1];
        switch (p[0]) {
        case 1:
            if (rem >= 4 && olen == 4) {
                sp->lcp_mru = ((u32)p[2] << 8) + p[3];
                sp->lcp_opts |= 2;
            }
            break;
        case 2:
            if (rem >= 6 && olen == 6) {
                sp->lcp_my_accm = ((u32)p[2] << 24) | ((u32)p[3] << 16) |
                                  ((u32)p[4] << 8) | (u32)p[5];
                sp->lcp_opts |= 4;
            }
            break;
        case 5:
            if (rem >= 6 && olen == 6) {
                sp->lcp_magic = ((u32)p[2] << 24) | ((u32)p[3] << 16) |
                                ((u32)p[4] << 8) | (u32)p[5];
                sp->lcp_opts |= 0x20;
            }
            break;
        case 7:
            if (rem >= 2 && olen == 2) {
                sp->lcp_my_pcomp = 0;
                sp->lcp_opts &= ~0x80U;
            }
            break;
        case 8:
            if (rem >= 2 && olen == 2) {
                sp->lcp_my_accomp = 0;
                sp->lcp_opts &= ~0x100U;
            }
            break;
        default:
            break;
        }
        rem -= olen;
        p += olen;
    }
    sceNetIfhandle_lib_76BAD213(tmp);
}

/* [401] 0x02B798 */
static INET_NOINLINE void inetSpppLcpTlu(InetSppp *sp)
{
    int idx;

    for (idx = 0; idx < 4; idx++) {
        if ((g_sppp_cps[idx]->flags & 8) != 0) {
            g_sppp_cps[idx]->open(sp);
        }
    }

    sp->pp_phase = ((sp->lcp_opts & 8) != 0 || (sp->pp_flags & 0x10) != 0) ? 3U : 4U;
    if (sp->pp_phase > sp->pp_max_phase) {
        sp->pp_max_phase = sp->pp_phase;
    }
    if (sp->pp_lcp_up != NULL) {
        sp->pp_lcp_up(sp);
    }

    for (idx = 0; idx < 4; idx++) {
        if ((g_sppp_cps[idx]->flags & 2) != 0) {
            g_sppp_cps[idx]->open(sp);
        }
    }

    if (sp->pp_phase == 4) {
        for (idx = 0; idx < 4; idx++) {
            if ((g_sppp_cps[idx]->flags & 4) != 0) {
                g_sppp_cps[idx]->open(sp);
            }
        }
    }

    for (idx = 0; idx < 4; idx++) {
        if ((sp->lcp_protos & (1U << idx)) != 0 && (g_sppp_cps[idx]->flags & 1) == 0) {
            g_sppp_cps[idx]->up(sp);
        }
    }

    if (sp->pp_chg != NULL) {
        sp->pp_chg(sp, (int)sp->pp_phase);
    }
    if (sp->pp_phase == 4) {
        inetSpppLcpCheckTermination(sp);
    }
}

/* [402] 0x02B994 */
static INET_NOINLINE void inetSpppLcpScr(InetSppp *sp)
{
    u8 opt[16];
    int len = 0;

    if ((sp->lcp_opts & 4) != 0 && (sp->pp_flags & 0x20) == 0) {
        u32 accm = sp->lcp_my_accm;
        opt[len++] = 2;
        opt[len++] = 6;
        opt[len++] = (u8)(accm >> 24);
        opt[len++] = (u8)(accm >> 16);
        opt[len++] = (u8)(accm >> 8);
        opt[len++] = (u8)accm;
    }
    if ((sp->lcp_opts & 0x80) != 0 && (sp->pp_flags & 0x20) == 0) {
        opt[len++] = 7;
        opt[len++] = 2;
    }
    if ((sp->lcp_opts & 0x100) != 0 && (sp->pp_flags & 0x20) == 0) {
        opt[len++] = 8;
        opt[len++] = 2;
    }
    if ((sp->lcp_opts & 0x20) != 0) {
        u32 magic = sp->lcp_magic;
        if (magic == 0) {
            magic = sceNet_lib_1858883D();
            sp->lcp_magic = magic;
        }
        opt[len++] = 5;
        opt[len++] = 6;
        opt[len++] = (u8)(magic >> 24);
        opt[len++] = (u8)(magic >> 16);
        opt[len++] = (u8)(magic >> 8);
        opt[len++] = (u8)magic;
    }
    if ((sp->lcp_opts & 2) != 0) {
        u32 mru = sp->lcp_mru;
        opt[len++] = 1;
        opt[len++] = 4;
        opt[len++] = (u8)(mru >> 8);
        opt[len++] = (u8)mru;
    }

    sp->pp_seq[0]++;
    sp->confid[0] = (u8)sp->pp_seq[0];
    inetSpppCpSend(sp, 0xC021, 1, sp->confid[0], (u16)len, opt);
}

/* [403] 0x02BB90 */
static INET_NOINLINE int inetSpppNcpsOpen(InetSppp *sp)
{
    int idx;
    for (idx = 0; idx < 4; idx++) {
        if ((sp->lcp_protos & (1U << idx)) != 0 && (g_sppp_cps[idx]->flags & 4) != 0) {
            return 1;
        }
    }
    return 0;
}

/* [404] 0x02BBE0 */
static INET_NOINLINE void inetSpppLcpCheckTermination(InetSppp *sp)
{
    if (sp->pp_phase >= 4 && !inetSpppNcpsOpen(sp)) {
        g_sppp_lcp.close(sp);
    }
}

/* [405] 0x02BC34 */
static INET_NOINLINE int inetSpppIpcpRcr(InetSppp *sp, u8 *h, int len)
{
    int optBytes = len - 4;
    int allocLen = (optBytes > 6) ? optBytes : 6;
    u8 *buf = (u8 *)sceNetIfhandle_lib_15CFE3C0(allocLen);
    u8 *r;
    u8 *p;
    int rlen = 0;
    int rem;
    int forceRej = 0;

    if (buf == NULL) {
        return 0;
    }

    /* Pass 1: check rejected options */
    r = buf;
    p = h + 4;
    rem = optBytes;
    while (rem >= 2 && p[1] != 0) {
        u8 opt = p[0];
        u8 olen = p[1];
        int ok = 0;
        if (opt == 3 || opt == 129 || opt == 131 ||
            (opt == 2 && (sp->ipcp_cfg_flags & 1) != 0)) {
            if (rem >= 6 && olen == 6) {
                ok = 1;
            }
        }
        if (!ok) {
            sceNet_lib_2F305274(p, r, olen);
            rlen += olen;
            r += olen;
        }
        rem -= olen;
        p += olen;
    }

    if (rlen != 0) {
        forceRej = 1;
    } else {
        /* Pass 2: validate and record accepted options */
        p = h + 4;
        rem = optBytes;
        while (rem >= 2 && p[1] != 0) {
            u8 opt = p[0];
            u8 olen = p[1];
            u32 addrVal = ((u32)p[2] << 24) | ((u32)p[3] << 16) |
                          ((u32)p[4] << 8) | (u32)p[5];
            if (opt == 3) {
                sp->ipcp_hisaddr = addrVal;
                sp->ipcp_flags |= 1;
            } else if (opt == 129) {
                sp->pp_dns_main = addrVal;
                sp->ipcp_flags |= 4;
            } else if (opt == 131) {
                sp->pp_dns_alt = addrVal;
                sp->ipcp_flags |= 8;
            } else if (opt == 2) {
                u32 vj = sp->ipcp_his_vj;
                u32 proto = ((u32)p[2] << 8) | p[3];
                u32 maxSlot = p[4];
                if (proto != (vj >> 16) || maxSlot < 3 || maxSlot > ((vj >> 8) & 0xFF)) {
                    p[2] = (u8)(vj >> 24);
                    p[3] = (u8)(vj >> 16);
                    p[4] = (u8)(vj >> 8);
                    sceNet_lib_2F305274(p, r, olen);
                    rlen += olen;
                    r += olen;
                } else {
                    sp->ipcp_his_vj = (proto << 16) | (maxSlot << 8) | p[5];
                }
            } else {
                sceNet_lib_2F305274(p, r, olen);
                rlen += olen;
                r += olen;
            }
            rem -= olen;
            p += olen;
        }
    }

    if (forceRej) {
        inetSpppCpSend(sp, 0x8021, 4, h[1], (u16)rlen, buf);
    } else if (rlen != 0) {
        sp->fail_counter[1]++;
        if (sp->fail_counter[1] >= (s32)sp->lcp_max_failure) {
            inetSpppCpSend(sp, 0x8021, 4, h[1], (u16)rlen, buf);
        } else {
            inetSpppCpSend(sp, 0x8021, 3, h[1], (u16)rlen, buf);
        }
    } else {
        inetSpppCpSend(sp, 0x8021, 2, h[1], (u16)optBytes, h + 4);
    }

    sceNetIfhandle_lib_76BAD213(buf);
    return (rlen == 0);
}

/* [406] 0x02BFD8 */
static INET_NOINLINE void inetSpppIpcpRcnNak(InetSppp *sp, u8 *h, int len)
{
    int rem = len - 4;
    u8 *p = h + 4;

    while (rem >= 2 && p[1] != 0) {
        u8 opt = p[0];
        u8 olen = p[1];
        if (rem >= 6 && olen == 6) {
            u32 addrVal = ((u32)p[2] << 24) | ((u32)p[3] << 16) |
                          ((u32)p[4] << 8) | (u32)p[5];
            if (opt == 3) {
                sp->ipcp_myaddr = addrVal;
                sp->ipcp_flags |= 2;
            } else if (opt == 129) {
                sp->pp_dns_main = addrVal;
                sp->ipcp_flags |= 4;
            } else if (opt == 131) {
                sp->pp_dns_alt = addrVal;
                sp->ipcp_flags |= 8;
            } else if (opt == 2) {
                u32 vj = sp->ipcp_my_vj;
                u32 proto = ((u32)p[2] << 8) | p[3];
                u32 maxSlot = p[4];
                if (proto != (vj >> 16) || maxSlot < 3 || maxSlot > ((vj >> 8) & 0xFF)) {
                    sp->ipcp_my_vj = 0;
                    sp->ipcp_opts &= ~4U;
                } else {
                    sp->ipcp_my_vj = (proto << 16) | (maxSlot << 8) | p[5];
                }
            }
        }
        rem -= olen;
        p += olen;
    }
}

/* [407] 0x02C1D0 */
static INET_NOINLINE void inetSpppIpcpScr(InetSppp *sp)
{
    u8 opt[32];
    int len = 0;

    if ((sp->ipcp_opts & 4) != 0) {
        u32 vj = sp->ipcp_my_vj;
        opt[len++] = 2;
        opt[len++] = 6;
        opt[len++] = (u8)(vj >> 24);
        opt[len++] = (u8)(vj >> 16);
        opt[len++] = (u8)(vj >> 8);
        opt[len++] = (u8)vj;
    }
    if ((sp->ipcp_opts & 8) != 0) {
        u32 addr = sp->ipcp_myaddr;
        opt[len++] = 3;
        opt[len++] = 6;
        opt[len++] = (u8)(addr >> 24);
        opt[len++] = (u8)(addr >> 16);
        opt[len++] = (u8)(addr >> 8);
        opt[len++] = (u8)addr;
    }
    if ((sp->ipcp_opts & 0x20000000U) != 0) {
        u32 dnsMain = sp->pp_dns_main;
        opt[len++] = 129;
        opt[len++] = 6;
        opt[len++] = (u8)(dnsMain >> 24);
        opt[len++] = (u8)(dnsMain >> 16);
        opt[len++] = (u8)(dnsMain >> 8);
        opt[len++] = (u8)dnsMain;
        if ((sp->ipcp_opts & 0x80000000U) != 0) {
            u32 dnsAlt = sp->pp_dns_alt;
            opt[len++] = 131;
            opt[len++] = 6;
            opt[len++] = (u8)(dnsAlt >> 24);
            opt[len++] = (u8)(dnsAlt >> 16);
            opt[len++] = (u8)(dnsAlt >> 8);
            opt[len++] = (u8)dnsAlt;
        }
    }

    {
        u8 id = (u8)(++sp->pp_seq[1]);
        sp->confid[1] = id;
        inetSpppCpSend(sp, 0x8021, 1, id, (u16)len, opt);
    }
}

/* [408] 0x02C3A8 */
static INET_NOINLINE void inetSpppChapInput(InetSppp *sp, InetMbuf *m)
{
    s32 pktLen = M_PKTLEN(m);
    u8 *h;
    u16 hlen;
    u8 code;
    u8 ident;

    if (pktLen < 4) {
        return;
    }

    h = m->m_data;
    code = h[0];
    ident = h[1];
    hlen = (u16)(((u16)h[2] << 8) | h[3]);
    if (hlen < pktLen) {
        pktLen = hlen;
    }

    if (code == 1) { /* CHALLENGE */
        u32 valLen = h[4];
        if (pktLen >= (s32)(valLen + 5)) {
            u8 md5Ctx[96];
            u8 digest[16];
            u8 digestLen = 16;
            sceKernelUtilsMd5BlockInit(md5Ctx);
            sceKernelUtilsMd5BlockUpdate(md5Ctx, &ident, 1);
            sceKernelUtilsMd5BlockUpdate(md5Ctx, (u8 *)sp->myauth_secret, sp->myauth_secret_len);
            sceKernelUtilsMd5BlockUpdate(md5Ctx, h + 5, valLen);
            sceKernelUtilsMd5BlockResult(md5Ctx, digest);
            inetSpppAuthSend(&g_sppp_chap, sp, 2, ident,
                             1, &digestLen,
                             16, digest,
                             (int)sp->myauth_name_len, sp->myauth_name,
                             0);
            sceNet_lib_7C86FBA4(sp->ch[3], sp->auth_timeout, inetSpppChapMyTo, sp);
            sp->rst_counter[3] = (s32)sp->auth_max_retry;
            sp->chap_id = ident;
            sceKernelMemcpy(sp->chap_challenge, h + 5, valLen);
            sp->chap_challenge_len = valLen;
        }
    } else if (code == 3) { /* SUCCESS */
        int spl;
        sceNet_lib_A93A93E9(sp->ch[3]);
        spl = sceNet_lib_94B44F26();
        sp->pp_flags &= ~0x10U;
        sceNet_lib_515B2F33(spl);
        inetSpppPhaseNetwork(sp);
    } else if (code == 4) { /* FAILURE */
        int spl;
        sceNet_lib_A93A93E9(sp->ch[3]);
        spl = sceNet_lib_94B44F26();
        sceNet_lib_515B2F33(spl);
    }
}

/* [409] 0x02C584 */
static INET_NOINLINE void inetSpppPapInput(InetSppp *sp, InetMbuf *m)
{
    u8 code;

    if (M_PKTLEN(m) < 5) {
        return;
    }
    code = m->m_data[0];
    if (code == 2) { /* ACK */
        int spl;
        sceNet_lib_A93A93E9(sp->ch[2]);
        spl = sceNet_lib_94B44F26();
        sp->pp_flags &= ~0x10U;
        sceNet_lib_515B2F33(spl);
        inetSpppPhaseNetwork(sp);
    } else if (code == 3) { /* NAK */
        sceNet_lib_A93A93E9(sp->ch[2]);
    }
}

/* [410] 0x02C620 */
static INET_NOINLINE void inetSpppAuthSend(const InetSpppCp *cp, InetSppp *sp, u8 type, u8 id, ...)
{
    InetIfnet *ifp = &sp->pp_if;
    InetMbuf *m;
    u32 hdrOff;
    u16 protoNet;
    u16 totalNet;
    u8 *cpHdr;
    u8 *dst;
    int totalLen = 0;
    va_list ap;

    m = (InetMbuf *)sceNetIfhandle_lib_15CFE3C0(sizeof(InetMbuf));
    if (m == NULL) {
        return;
    }

    m->m_next = NULL;
    m->m_nextpkt = NULL;
    m->m_data = M_PKTDAT(m);
    m->m_type = INET_MT_DATA;
    m->m_flags = INET_M_PKTHDR;
    M_RCVIF(m) = NULL;
    M_CSUM(m) = 0;

    protoNet = inetBswap16(cp->proto);
    if ((sp->pp_flags & 0x20) != 0) {
        sceKernelMemcpy(m->m_data, &protoNet, 2);
        hdrOff = 2;
        cpHdr = m->m_data + 2;
    } else {
        m->m_data[0] = 0xFF;
        m->m_data[1] = 0x03;
        sceKernelMemcpy(m->m_data + 2, &protoNet, 2);
        hdrOff = 4;
        cpHdr = m->m_data + 4;
    }

    cpHdr[0] = type;
    cpHdr[1] = id;
    dst = cpHdr + 4;

    va_start(ap, id);
    for (;;) {
        int segLen = va_arg(ap, int);
        const void *segPtr;
        if (segLen == 0) {
            break;
        }
        segPtr = va_arg(ap, const void *);
        totalLen += segLen;
        if (totalLen > (int)(204U - hdrOff)) {
            va_end(ap);
            sceNetIfhandle_lib_F56FAC82(m);
            return;
        }
        sceNet_lib_2F305274(segPtr, dst, segLen);
        dst += segLen;
    }
    va_end(ap);

    m->m_len = (s32)(hdrOff + (u32)totalLen + 4U);
    M_PKTLEN(m) = m->m_len;
    totalNet = inetBswap16((u16)(totalLen + 4));
    sceKernelMemcpy(cpHdr + 2, &totalNet, 2);

    if (sp->pp_fastq.ifq_len >= sp->pp_fastq.ifq_maxlen) {
        ifp->if_snd.ifq_drops++;
        sceNetIfhandle_lib_F56FAC82(m);
        ifp->if_data.ifi_oerrors++;
        return;
    }

    m->m_nextpkt = NULL;
    if (sp->pp_fastq.ifq_tail != NULL) {
        sp->pp_fastq.ifq_tail->m_nextpkt = m;
    } else {
        sp->pp_fastq.ifq_head = m;
    }
    sp->pp_fastq.ifq_tail = m;
    sp->pp_fastq.ifq_len++;

    if ((ifp->if_flags & INET_IFF_OACTIVE) == 0 && ifp->if_start != NULL) {
        ifp->if_start(ifp);
    }
    ifp->if_data.ifi_obytes += (u32)(M_PKTLEN(m) + 3);
}

/* [411] 0x02C88C */
static INET_NOINLINE void inetSpppKeepalive(void *arg)
{
    InetSppp *sp = (InetSppp *)arg;
    InetIfnet *ifp = &sp->pp_if;
    int spl = sceNet_lib_94B44F26();

    if ((sp->pp_flags & 1) != 0 && (ifp->if_flags & INET_IFF_RUNNING) != 0 && sp->pp_phase >= 3) {
        if (sp->pp_alivecnt == 3) {
            sceNet_lib_EDB11CB4(ifp);
            while (sp->pp_fastq.ifq_head != NULL) {
                InetMbuf *m = sp->pp_fastq.ifq_head;
                sp->pp_fastq.ifq_head = m->m_nextpkt;
                if (sp->pp_fastq.ifq_head == NULL) {
                    sp->pp_fastq.ifq_tail = NULL;
                }
                m->m_nextpkt = NULL;
                sp->pp_fastq.ifq_len--;
                sceNetIfhandle_lib_F56FAC82(m);
            }
            sp->pp_alivecnt = 0;
            inetSpppLcpClose(sp);
            inetSpppCpChangeState(&g_sppp_lcp, sp, 3);
            if (sp->pp_tlf != NULL) {
                sp->pp_tlf(sp);
            }
        } else {
            u32 magicNet;
            if (sp->pp_alivecnt < 4) {
                sp->pp_alivecnt++;
            }
            sp->pp_seq[0]++;
            sp->lcp_echoid = (u8)sp->pp_seq[0];
            magicNet = inetBswap32(sp->lcp_magic);
            inetSpppCpSend(sp, 0xC021, 9, sp->lcp_echoid, 4, &magicNet);
        }
    }

    sceNet_lib_515B2F33(spl);
    sceNet_lib_7C86FBA4(g_sppp_keepalive_callout, 1000, inetSpppKeepalive, sp);
}

/* [412] 0x02C9F0 */
static INET_NOINLINE int inetSpppParams(InetSppp *sp, u32 cmd, void *data)
{
    InetSpppStatusIoctl *st = (InetSpppStatusIoctl *)data;

    switch (cmd) {
    case 0x81186979U: {
        InetSpppAuthCfg *cfg = (InetSpppAuthCfg *)data;
        sceKernelMemcpy(sp->myauth_name, cfg->name, 128);
        sp->myauth_name_len = (u8)inetSpppStrnlen(cfg->name, 128);
        sceKernelMemcpy(sp->myauth_secret, cfg->secret, 128);
        sp->myauth_secret_len = (u8)inetSpppStrnlen(cfg->secret, 128);
        sp->auth_timeout = cfg->timeout;
        sp->auth_max_retry = cfg->max_retry;
        return 0;
    }
    case 0xC014697BU:
        st->val_a = sp->pp_phase;
        return 0;
    case 0xC014697DU:
        st->val_a = sp->pp_max_phase;
        return 0;
    case 0xC0246978U:
        st->val_a = sp->ipcp_flags;
        st->val_b = sp->ipcp_myaddr;
        st->val_c = sp->ipcp_hisaddr;
        st->val_d = sp->pp_dns_main;
        st->val_e = sp->pp_dns_alt;
        return 0;
    case 0x8014697CU:
        spppResetState(sp);
        return 0;
    case 0x80146980U:
        sp->ipcp_cfg_flags = st->val_a;
        return 0;
    case 0x8024697AU:
        sp->lcp_timeout = st->val_a;
        sp->lcp_max_terminate = st->val_b;
        sp->lcp_max_configure = st->val_c;
        sp->lcp_max_failure = st->val_d;
        sp->lcp_flags_cfg = st->val_e;
        return 0;
    default:
        return INET_EINVAL;
    }
}

/* [413] 0x02CBE8 */
static INET_NOINLINE void inetSpppPhaseNetwork(InetSppp *sp)
{
    int idx;

    sp->pp_phase = 4;
    if (sp->pp_max_phase < 4) {
        sp->pp_max_phase = 4;
    }

    for (idx = 0; idx < 4; idx++) {
        if ((g_sppp_cps[idx]->flags & 4) != 0) {
            g_sppp_cps[idx]->open(sp);
        }
    }
    for (idx = 0; idx < 4; idx++) {
        if ((sp->lcp_protos & (1U << idx)) != 0 && (g_sppp_cps[idx]->flags & 4) != 0) {
            g_sppp_cps[idx]->up(sp);
        }
    }
    if (sp->pp_chg != NULL) {
        sp->pp_chg(sp, (int)sp->pp_phase);
    }
    inetSpppLcpCheckTermination(sp);
}

/* [414] 0x02CCF4 */
static INET_NOINLINE void inetSpppVjCompress(InetSppp *sp, InetMbuf *m)
{
    u8 *hdr = m->m_data;
    u8 addrByte = hdr[0];
    u8 ctrlByte = hdr[1];
    u16 proto = (u16)(((u16)hdr[2] << 8) | hdr[3]);

    if (proto == 0x0021) {
        InetMbuf *ipM = m;
        InetIpHdr *ip;
        s32 origLen;
        if (m->m_len < 5) {
            ipM = m->m_next;
            if (ipM == NULL) {
                return;
            }
            ip = (InetIpHdr *)ipM->m_data;
        } else {
            ip = (InetIpHdr *)(hdr + 4);
        }
        origLen = ipM->m_len;
        if (ip->ip_p == INET_IPPROTO_TCP) {
            u32 slType = inetSlCompressTcp(ipM, ip, &sp->slcomp, (int)(sp->ipcp_my_vj & 0xFF));
            if (slType == 0x70) {
                proto = 0x002F;
            } else if (slType == 0x80) {
                hdr = m->m_data;
                hdr[0] = addrByte;
                hdr[1] = ctrlByte;
                hdr[2] = 0;
                proto = 0x002D;
                M_PKTLEN(m) -= (origLen - ipM->m_len);
            }
            hdr[3] = (u8)proto;
        }
    }
}

/* [415] 0x02CDF8 */
static INET_NOINLINE InetMbuf *inetSpppVjUncompress(InetSppp *sp, InetMbuf *m)
{
    u32 pktLen = (u32)M_PKTLEN(m);
    u8 *hdr = m->m_data;
    u8 addrByte = hdr[0];
    u8 ctrlByte = hdr[1];
    u16 proto = (u16)(((u16)hdr[2] << 8) | hdr[3]);
    u8 *outHdr = NULL;
    u32 outHdrLen = 0;

    if (proto == 0x002D) { /* VJC_COMP */
        s32 pullLen = (pktLen >= 20U) ? 20 : (s32)pktLen;
        s32 hdrConsumed;
        InetMbuf *nm;
        s32 trail;

        if (m->m_len < pullLen) {
            m = sceNetIfhandle_lib_49EDBB18(m, pullLen);
            if (m == NULL) {
                sp->pp_if.if_data.ifi_ierrors++;
                return NULL;
            }
        }
        hdrConsumed = inetSlUncompressTcp(m->m_data + 4, m->m_len - 4, (s32)(pktLen - 4U),
                                          0x80, &sp->slcomp, &outHdr, &outHdrLen);
        if (hdrConsumed <= 0) {
            sp->pp_if.if_data.ifi_ierrors++;
            sceNetIfhandle_lib_F56FAC82(m);
            return NULL;
        }

        nm = (InetMbuf *)sceNetIfhandle_lib_15CFE3C0(sizeof(InetMbuf));
        if (nm == NULL) {
            sp->pp_if.if_data.ifi_ierrors++;
            sceNetIfhandle_lib_F56FAC82(m);
            return NULL;
        }
        nm->m_next = NULL;
        nm->m_nextpkt = NULL;
        nm->m_data = M_PKTDAT(nm);
        nm->m_len = 0;
        nm->m_type = INET_MT_DATA;
        nm->m_flags = INET_M_PKTHDR;
        M_CSUM(nm) = 0;

        if (outHdrLen + 4U > 208U) {
            u8 *cl = (u8 *)sceNetIfhandle_lib_15CFE3C0(2048);
            if (cl == NULL) {
                cl = (u8 *)sceNetIfhandle_lib_15CFE3C0(2048);
            }
            if (cl != NULL) {
                nm->m_ext.ext_buf = cl;
                nm->m_data = cl;
                nm->m_flags |= (INET_M_EXT | 8);
                nm->m_ext.ext_size = 2048;
                nm->m_ext.ext_free = NULL;
                nm->m_ext.ext_arg = NULL;
                nm->m_ext.ext_nextref = nm;
                nm->m_ext.ext_prevref = nm;
            }
            trail = ((nm->m_flags & INET_M_EXT) != 0)
                        ? (s32)((nm->m_ext.ext_buf + nm->m_ext.ext_size) - (nm->m_data + nm->m_len))
                        : (s32)(((u8 *)nm + sizeof(InetMbuf)) - (nm->m_data + nm->m_len));
            if (trail < (s32)(outHdrLen + 4U)) {
                sceNetIfhandle_lib_F56FAC82(nm);
                sp->pp_if.if_data.ifi_ierrors++;
                sceNetIfhandle_lib_F56FAC82(m);
                return NULL;
            }
        }

        nm->m_data[0] = addrByte;
        nm->m_data[1] = ctrlByte;
        nm->m_data[2] = 0;
        nm->m_data[3] = 0x21;
        sceNet_lib_2F305274(outHdr, nm->m_data + 4, (s32)outHdrLen);
        nm->m_len = (s32)(outHdrLen + 4U);

        m->m_data += hdrConsumed + 4;
        m->m_len -= hdrConsumed + 4;

        trail = ((nm->m_flags & INET_M_EXT) != 0)
                    ? (s32)((nm->m_ext.ext_buf + nm->m_ext.ext_size) - (nm->m_data + nm->m_len) - nm->m_len)
                    : (s32)(((u8 *)nm + sizeof(InetMbuf)) - (nm->m_data + nm->m_len) - nm->m_len);
        if (trail >= m->m_len) {
            sceNet_lib_2F305274(m->m_data, nm->m_data + nm->m_len, m->m_len);
            nm->m_len += m->m_len;
            if ((m->m_flags & INET_M_EXT) != 0) {
                if (m->m_ext.ext_nextref == m) {
                    sceNetIfhandle_lib_76BAD213(m->m_ext.ext_buf);
                } else {
                    m->m_ext.ext_nextref->m_ext.ext_prevref = m->m_ext.ext_prevref;
                    m->m_ext.ext_prevref->m_ext.ext_nextref = m->m_ext.ext_nextref;
                }
                m->m_flags &= (u16)~(INET_M_EXT | 8);
                m->m_ext.ext_size = 0;
            }
            nm->m_next = m->m_next;
            sceNetIfhandle_lib_76BAD213(m);
        } else {
            nm->m_next = m;
        }
        M_RCVIF(nm) = &sp->pp_if;
        M_PKTLEN(nm) = (s32)(pktLen + outHdrLen - (u32)hdrConsumed);
        return nm;
    }

    if (proto == 0x002F) { /* VJC_UNCOMP */
        s32 ipHlen;
        s32 fullHdr;
        if (m->m_len < 24) {
            m = sceNetIfhandle_lib_49EDBB18(m, 24);
            if (m == NULL) {
                sp->pp_if.if_data.ifi_ierrors++;
                return NULL;
            }
            hdr = m->m_data;
        }
        ipHlen = (s32)(hdr[4] & 0x0F) * 4;
        if (m->m_len < ipHlen + 24) {
            m = sceNetIfhandle_lib_49EDBB18(m, ipHlen + 24);
            if (m == NULL) {
                sp->pp_if.if_data.ifi_ierrors++;
                return NULL;
            }
            hdr = m->m_data;
        }
        fullHdr = ipHlen + (s32)((hdr[4 + ipHlen + 12] >> 4) * 4) + 4;
        if (m->m_len < fullHdr) {
            m = sceNetIfhandle_lib_49EDBB18(m, fullHdr);
            if (m == NULL) {
                sp->pp_if.if_data.ifi_ierrors++;
                return NULL;
            }
            hdr = m->m_data;
        }
        if (inetSlUncompressTcp(hdr + 4, m->m_len - 4, (s32)(pktLen - 4U),
                                0x70, &sp->slcomp, &outHdr, &outHdrLen) >= 0) {
            hdr[0] = addrByte;
            hdr[1] = ctrlByte;
            hdr[2] = 0;
            hdr[3] = 0x21;
            return m;
        }
        sp->pp_if.if_data.ifi_ierrors++;
        sceNetIfhandle_lib_F56FAC82(m);
        return NULL;
    }

    return m;
}

/* [416] 0x02D234 */
static INET_NOINLINE int inetSpppStrnlen(const char *str, int maxLen)
{
    int len = 0;
    while (len < maxLen && str[len] != '\0') {
        len++;
    }
    return len;
}

/* [417] 0x02D26C */
INET_NOINLINE void inetSpppAttach(InetIfnet *ifp)
{
    InetSppp *sp = (InetSppp *)ifp;
    int idx;

    sceNet_lib_25CC373A(g_sppp_keepalive_callout);
    sceNet_lib_7C86FBA4(g_sppp_keepalive_callout, 1000, inetSpppKeepalive, sp);
    sp->pp_fastq.ifq_maxlen = 20;
    sp->pp_alivecnt = 0;
    sceKernelMemset(sp->pp_seq, 0, sizeof(sp->pp_seq));
    sceKernelMemset(sp->pp_rseq, 0, sizeof(sp->pp_rseq));
    sp->pp_phase = 0;
    sp->pp_up = inetSpppLcpUp;
    sp->pp_down = inetSpppLcpDown;
    sceKernelMemset(&sp->myauth_proto, 0, 288);
    spppResetState(sp);
    for (idx = 0; idx < 4; idx++) {
        sceNet_lib_25CC373A(sp->ch[idx]);
    }
}

/* [418] 0x02D3A0 */
INET_NOINLINE void inetSpppDetach(InetIfnet *ifp)
{
    InetSppp *sp = (InetSppp *)ifp;
    int idx;

    sceNet_lib_A93A93E9(g_sppp_keepalive_callout);
    for (idx = 0; idx < 4; idx++) {
        sceNet_lib_A93A93E9(sp->ch[idx]);
    }
}

/* [419] 0x02D3F0 */
INET_NOINLINE int inetSpppIsEmpty(InetIfnet *ifp)
{
    InetSppp *sp = (InetSppp *)ifp;
    int spl = sceNet_lib_94B44F26();
    int empty = (sp->pp_fastq.ifq_len == 0 && ifp->if_snd.ifq_len == 0);
    sceNet_lib_515B2F33(spl);
    return empty;
}

/* [420] 0x02D440 */
INET_NOINLINE int inetSpppHookInit(void)
{
    return sceNet_lib_B51452D2(inetSpppAttach, inetSpppDetach, inetSpppOutput,
                               inetSpppInput, inetSpppIoctl, inetSpppDequeue);
}

/* [421] 0x02D488 */
INET_NOINLINE int inetSpppHookTerm(void)
{
    return sceNet_lib_F6FE6D8E();
}

/* [422] 0x02D4A4 */
static INET_NOINLINE void inetSpppNullEvent(InetSppp *sp)
{
    (void)sp;
}

/* [423] 0x02D4AC */
static INET_NOINLINE void inetSpppLcpUp(InetSppp *sp)
{
    if (sp->state[0] == 0) {
        sp->pp_if.if_flags |= INET_IFF_RUNNING;
        inetSpppLcpOpen(sp);
    }
    if (sp->state[0] == 0) {
        inetSpppCpChangeState(&g_sppp_lcp, sp, 2);
    } else if (sp->state[0] == 1) {
        sp->rst_counter[0] = (s32)sp->lcp_max_configure;
        inetSpppLcpScr(sp);
        inetSpppCpChangeState(&g_sppp_lcp, sp, 6);
    }
}

/* [424] 0x02D55C */
static INET_NOINLINE void inetSpppLcpDown(InetSppp *sp)
{
    switch (sp->state[0]) {
    case 2:
    case 4:
        inetSpppCpChangeState(&g_sppp_lcp, sp, 0);
        break;
    case 3:
        inetSpppLcpTls(sp);
        inetSpppCpChangeState(&g_sppp_lcp, sp, 1);
        break;
    case 5:
    case 6:
    case 7:
    case 8:
        inetSpppCpChangeState(&g_sppp_lcp, sp, 1);
        break;
    case 9:
        inetSpppLcpTld(sp);
        inetSpppCpChangeState(&g_sppp_lcp, sp, 1);
        break;
    default:
        break;
    }

    sceNet_lib_EDB11CB4(&sp->pp_if);
    if (sp->state[0] != 0) {
        inetSpppLcpClose(sp);
    } else {
        inetSpppLcpTlf(sp);
    }
    sp->pp_if.if_flags &= (u16)~INET_IFF_RUNNING;
}

/* [425] 0x02D638 */
static INET_NOINLINE void inetSpppLcpClose(InetSppp *sp)
{
    switch (sp->state[0]) {
    case 1:
        inetSpppLcpTlf(sp);
        inetSpppCpChangeState(&g_sppp_lcp, sp, 0);
        break;
    case 3:
        inetSpppCpChangeState(&g_sppp_lcp, sp, 2);
        break;
    case 5:
        inetSpppCpChangeState(&g_sppp_lcp, sp, 4);
        break;
    case 6:
    case 7:
    case 8:
        sp->rst_counter[0] = (s32)sp->lcp_max_terminate;
        sp->pp_seq[0]++;
        inetSpppCpSend(sp, 0xC021, 5, (u8)sp->pp_seq[0], 0, NULL);
        inetSpppCpChangeState(&g_sppp_lcp, sp, 4);
        break;
    case 9:
        inetSpppLcpTld(sp);
        sp->rst_counter[0] = (s32)sp->lcp_max_terminate;
        sp->pp_seq[0]++;
        inetSpppCpSend(sp, 0xC021, 5, (u8)sp->pp_seq[0], 0, NULL);
        inetSpppCpChangeState(&g_sppp_lcp, sp, 4);
        break;
    default:
        break;
    }
}

/* [426] 0x02D74C */
static INET_NOINLINE void inetSpppLcpTo(void *arg)
{
    inetSpppCpTimeout(&g_sppp_lcp, (InetSppp *)arg);
}

/* [427] 0x02D770 */
static INET_NOINLINE void inetSpppLcpTld(InetSppp *sp)
{
    int idx;
    sp->pp_phase = 2;
    for (idx = 0; idx < 4; idx++) {
        if ((sp->lcp_protos & (1U << idx)) != 0 && (g_sppp_cps[idx]->flags & 1) == 0) {
            g_sppp_cps[idx]->down(sp);
            g_sppp_cps[idx]->close(sp);
        }
    }
}

/* [428] 0x02D814 */
static INET_NOINLINE void inetSpppLcpTls(InetSppp *sp)
{
    sp->pp_phase = 1;
    if (sp->pp_max_phase < 1) {
        sp->pp_max_phase = 1;
    }
    if (sp->pp_tls != NULL) {
        sp->pp_tls(sp);
    }
}

/* [429] 0x02D854 */
static INET_NOINLINE void inetSpppLcpTlf(InetSppp *sp)
{
    sp->pp_phase = 0;
    if (sp->pp_tlf != NULL) {
        sp->pp_tlf(sp);
    }
}

/* [430] 0x02D884 */
static INET_NOINLINE void inetSpppIpcpUp(InetSppp *sp)
{
    if (sp->state[1] == 0) {
        inetSpppCpChangeState(&g_sppp_ipcp, sp, 2);
    } else if (sp->state[1] == 1) {
        sp->rst_counter[1] = (s32)sp->lcp_max_configure;
        inetSpppIpcpScr(sp);
        inetSpppCpChangeState(&g_sppp_ipcp, sp, 6);
    }
}

/* [431] 0x02D90C */
static INET_NOINLINE void inetSpppIpcpDown(InetSppp *sp)
{
    switch (sp->state[1]) {
    case 2:
    case 4:
        inetSpppCpChangeState(&g_sppp_ipcp, sp, 0);
        break;
    case 3:
        inetSpppIpcpTls(sp);
        inetSpppCpChangeState(&g_sppp_ipcp, sp, 1);
        break;
    case 5:
    case 6:
    case 7:
    case 8:
        inetSpppCpChangeState(&g_sppp_ipcp, sp, 1);
        break;
    case 9:
        inetSpppIpcpTld(sp);
        inetSpppCpChangeState(&g_sppp_ipcp, sp, 1);
        break;
    default:
        break;
    }
}

/* [432] 0x02D9AC */
static INET_NOINLINE void inetSpppIpcpOpen(InetSppp *sp)
{
    sp->ipcp_flags = 0;
    sp->ipcp_myaddr = 0;
    sp->ipcp_hisaddr = 0;
    sp->pp_dns_main = 0;
    sp->pp_dns_alt = 0;
    sp->ipcp_opts |= 0xA0000008U;

    if ((sp->ipcp_cfg_flags & 1) != 0) {
        sp->ipcp_opts |= 4;
        sp->ipcp_my_vj = 0x002D0F01U;
        sp->ipcp_his_vj = 0x002D0F01U;
    } else {
        sp->ipcp_opts &= ~4U;
        sp->ipcp_my_vj = 0;
        sp->ipcp_his_vj = 0;
    }

    switch (sp->state[1]) {
    case 0:
        inetSpppIpcpTls(sp);
        inetSpppCpChangeState(&g_sppp_ipcp, sp, 1);
        break;
    case 2:
        sp->rst_counter[1] = (s32)sp->lcp_max_configure;
        inetSpppIpcpScr(sp);
        inetSpppCpChangeState(&g_sppp_ipcp, sp, 6);
        break;
    case 4:
        inetSpppCpChangeState(&g_sppp_ipcp, sp, 5);
        break;
    default:
        break;
    }
}

/* [433] 0x02DAE4 */
static INET_NOINLINE void inetSpppIpcpClose(InetSppp *sp)
{
    switch (sp->state[1]) {
    case 1:
        inetSpppIpcpTlf(sp);
        inetSpppCpChangeState(&g_sppp_ipcp, sp, 0);
        break;
    case 3:
        inetSpppCpChangeState(&g_sppp_ipcp, sp, 2);
        break;
    case 5:
        inetSpppCpChangeState(&g_sppp_ipcp, sp, 4);
        break;
    case 6:
    case 7:
    case 8:
        sp->rst_counter[1] = (s32)sp->lcp_max_terminate;
        sp->pp_seq[1]++;
        inetSpppCpSend(sp, 0x8021, 5, (u8)sp->pp_seq[1], 0, NULL);
        inetSpppCpChangeState(&g_sppp_ipcp, sp, 4);
        break;
    case 9:
        inetSpppIpcpTld(sp);
        sp->rst_counter[1] = (s32)sp->lcp_max_terminate;
        sp->pp_seq[1]++;
        inetSpppCpSend(sp, 0x8021, 5, (u8)sp->pp_seq[1], 0, NULL);
        inetSpppCpChangeState(&g_sppp_ipcp, sp, 4);
        break;
    default:
        break;
    }

    if (sp->pp_saved_mtu != 0) {
        sp->pp_if.if_data.ifi_mtu = sp->pp_saved_mtu;
        sp->pp_saved_mtu = 0;
    }
}

/* [434] 0x02DC38 */
static INET_NOINLINE void inetSpppIpcpTo(void *arg)
{
    inetSpppCpTimeout(&g_sppp_ipcp, (InetSppp *)arg);
}

/* [435] 0x02DC5C */
static INET_NOINLINE void inetSpppIpcpRcnRej(InetSppp *sp, u8 *h, int len)
{
    int rem = len - 4;
    u8 *p = h + 4;

    while (rem >= 2 && p[1] != 0) {
        u8 olen = p[1];
        switch (p[0]) {
        case 2:
            sp->ipcp_my_vj = 0;
            sp->ipcp_opts &= ~4U;
            break;
        case 3:
            sp->ipcp_opts &= ~8U;
            break;
        case 129:
            sp->ipcp_opts &= ~0x20000000U;
            break;
        case 131:
            sp->ipcp_opts &= ~0x80000000U;
            break;
        default:
            break;
        }
        rem -= olen;
        p += olen;
    }
}

/* [436] 0x02DD10 */
static INET_NOINLINE void inetSpppIpcpTlu(InetSppp *sp)
{
    sp->pp_max_phase = 2;
    sp->ipcp_flags |= 0x10;
    if ((u64)sp->lcp_their_mru < sp->pp_if.if_data.ifi_mtu) {
        sp->pp_saved_mtu = sp->pp_if.if_data.ifi_mtu;
        sp->pp_if.if_data.ifi_mtu = sp->lcp_their_mru;
    }
    if (sp->ipcp_my_vj != 0) {
        inetSlCompressInit(&sp->slcomp, (int)((sp->ipcp_my_vj >> 8) & 0xFF));
    }
    if (sp->pp_con != NULL) {
        sp->pp_con(sp);
    }
}

/* [437] 0x02DDC8 */
static INET_NOINLINE void inetSpppIpcpTld(InetSppp *sp)
{
    sp->ipcp_flags &= ~0x10U;
}

/* [438] 0x02DDD8 */
static INET_NOINLINE void inetSpppIpcpTls(InetSppp *sp)
{
    sp->lcp_protos |= 2;
}

/* [439] 0x02DDE8 */
static INET_NOINLINE void inetSpppIpcpTlf(InetSppp *sp)
{
    sp->lcp_protos &= ~2U;
}

/* [440] 0x02DDF8 */
static INET_NOINLINE void inetSpppPapOpen(InetSppp *sp)
{
    sp->lcp_protos |= 4;
    if (sp->myauth_proto == 0xC023) {
        g_sppp_pap.scr(sp);
        sceNet_lib_7C86FBA4(sp->ch[2], sp->auth_timeout, inetSpppPapMyTo, sp);
        sp->rst_counter[2] = (s32)sp->auth_max_retry;
    }
}

/* [441] 0x02DE64 */
static INET_NOINLINE void inetSpppPapClose(InetSppp *sp)
{
    if (sp->state[2] != 2) {
        inetSpppCpChangeState(&g_sppp_pap, sp, 2);
    }
    sp->lcp_protos &= ~4U;
    sceNet_lib_A93A93E9(sp->ch[2]);
}

/* [442] 0x02DEBC */
static INET_NOINLINE void inetSpppPapToNoop(void *arg)
{
    (void)arg;
}

/* [443] 0x02DEC4 */
static INET_NOINLINE void inetSpppPapMyTo(void *arg)
{
    InetSppp *sp = (InetSppp *)arg;
    sp->rst_counter[2]--;
    if (sp->rst_counter[2] < 0) {
        inetSpppPapTld(sp);
        inetSpppCpChangeState(&g_sppp_pap, sp, 2);
    } else {
        sceNet_lib_7C86FBA4(sp->ch[2], sp->auth_timeout, inetSpppPapMyTo, sp);
        g_sppp_pap.scr(sp);
    }
}

/* [444] 0x02DF54 */
static INET_NOINLINE void inetSpppPapTluNoop(InetSppp *sp)
{
    (void)sp;
}

/* [445] 0x02DF5C */
static INET_NOINLINE void inetSpppPapTld(InetSppp *sp)
{
    sceNet_lib_A93A93E9(sp->ch[2]);
    sp->lcp_protos &= ~4U;
    g_sppp_lcp.close(sp);
}

/* [446] 0x02DFA0 */
static INET_NOINLINE void inetSpppPapScr(InetSppp *sp)
{
    u8 nameLen = sp->myauth_name_len;
    u8 secLen = sp->myauth_secret_len;
    u8 id;

    sp->pp_seq[2]++;
    id = (u8)sp->pp_seq[2];
    sp->confid[2] = id;

    if (nameLen != 0 && secLen != 0) {
        inetSpppAuthSend(&g_sppp_pap, sp, 1, id,
                         1, &nameLen,
                         (int)nameLen, sp->myauth_name,
                         1, &secLen,
                         (int)secLen, sp->myauth_secret,
                         0);
    } else if (secLen != 0) {
        inetSpppAuthSend(&g_sppp_pap, sp, 1, id,
                         1, &nameLen,
                         1, &secLen,
                         (int)secLen, sp->myauth_secret,
                         0);
    } else if (nameLen != 0) {
        inetSpppAuthSend(&g_sppp_pap, sp, 1, id,
                         1, &nameLen,
                         (int)nameLen, sp->myauth_name,
                         1, &secLen,
                         0);
    } else {
        inetSpppAuthSend(&g_sppp_pap, sp, 1, id,
                         1, &nameLen,
                         1, &secLen,
                         0);
    }
}

/* [447] 0x02E0D8 */
static INET_NOINLINE void inetSpppChapOpen(InetSppp *sp)
{
    sp->lcp_protos |= 8;
}

/* [448] 0x02E0E8 */
static INET_NOINLINE void inetSpppChapClose(InetSppp *sp)
{
    if (sp->state[3] != 2) {
        inetSpppCpChangeState(&g_sppp_chap, sp, 2);
    }
    sp->lcp_protos &= ~8U;
    sceNet_lib_A93A93E9(sp->ch[3]);
}

/* [449] 0x02E140 */
static INET_NOINLINE void inetSpppChapToNoop(void *arg)
{
    (void)arg;
}

/* [450] 0x02E148 */
static INET_NOINLINE void inetSpppChapMyTo(void *arg)
{
    InetSppp *sp = (InetSppp *)arg;
    sp->rst_counter[3]--;
    if (sp->rst_counter[3] < 0) {
        g_sppp_chap.tld(sp);
        inetSpppCpChangeState(&g_sppp_chap, sp, 2);
    } else {
        u8 md5Ctx[96];
        u8 digest[16];
        u8 digestLen = 16;
        sceKernelUtilsMd5BlockInit(md5Ctx);
        sceKernelUtilsMd5BlockUpdate(md5Ctx, &sp->chap_id, 1);
        sceKernelUtilsMd5BlockUpdate(md5Ctx, (u8 *)sp->myauth_secret, sp->myauth_secret_len);
        sceKernelUtilsMd5BlockUpdate(md5Ctx, sp->chap_challenge, sp->chap_challenge_len);
        sceKernelUtilsMd5BlockResult(md5Ctx, digest);
        inetSpppAuthSend(&g_sppp_chap, sp, 2, sp->chap_id,
                         1, &digestLen,
                         16, digest,
                         (int)sp->myauth_name_len, sp->myauth_name,
                         0);
        sceNet_lib_7C86FBA4(sp->ch[3], sp->auth_timeout, inetSpppChapMyTo, sp);
    }
}

/* [451] 0x02E258 */
static INET_NOINLINE void inetSpppChapTluNoop(InetSppp *sp)
{
    (void)sp;
}

/* [452] 0x02E260 */
static INET_NOINLINE void inetSpppChapTld(InetSppp *sp)
{
    sceNet_lib_A93A93E9(sp->ch[3]);
    sp->lcp_protos &= ~8U;
    g_sppp_lcp.close(sp);
}

/* [453] 0x02E2A4 */
static INET_NOINLINE void inetSpppChapScrNoop(InetSppp *sp)
{
    (void)sp;
}

/* [454] 0x02E2AC */
static INET_NOINLINE void inetSpppCpTimeout(const InetSpppCp *cp, InetSppp *sp)
{
    int spl = sceNet_lib_94B44F26();
    u32 st = sp->state[cp->idx];

    sp->rst_counter[cp->idx]--;
    if (sp->rst_counter[cp->idx] < 0) {
        if (st == 4) {
            cp->tlf(sp);
            inetSpppCpChangeState(cp, sp, 2);
            inetSpppLcpCheckTermination(sp);
        } else if (st >= 5 && st <= 8) {
            cp->tlf(sp);
            inetSpppCpChangeState(cp, sp, 3);
            inetSpppLcpCheckTermination(sp);
        }
        sceNet_lib_515B2F33(spl);
        return;
    }

    switch (st) {
    case 4:
    case 5:
        sp->pp_seq[cp->idx]++;
        inetSpppCpSend(sp, cp->proto, 5, (u8)sp->pp_seq[cp->idx], 0, NULL);
        sceNet_lib_7C86FBA4(sp->ch[cp->idx], sp->lcp_timeout, cp->TO, sp);
        break;
    case 6:
    case 7:
        cp->scr(sp);
        inetSpppCpChangeState(cp, sp, 6);
        break;
    case 8:
        cp->scr(sp);
        sceNet_lib_7C86FBA4(sp->ch[cp->idx], sp->lcp_timeout, cp->TO, sp);
        break;
    default:
        break;
    }

    sceNet_lib_515B2F33(spl);
}
