/*
 * PSP 6.61 SceNetInet_Library (pspnet_inet.prx)
 * PPP over Ethernet (PPPoE) client interface (if_pppoe.c)
 */

#include "pspnet_inet_internal.h"

typedef struct InetPppoeStateIoctl {
    char ifname[16];
    s32 state;
    u32 session;
    u32 padi_retried;
    u32 padr_retried;
} InetPppoeStateIoctl;

typedef struct InetPppoeErrIoctl {
    char ifname[16];
    u32 error_code;
    char errmsg[256];
} InetPppoeErrIoctl;

static InetIfQueue g_pppoe_disc_inq;
static InetIfQueue g_pppoe_sess_inq;
static InetPppoeSoftc g_pppoe_sc;

static INET_NOINLINE int inetPppoeAttach(const char *ethName);
static INET_NOINLINE void inetPppoeDetach(void);
static INET_NOINLINE void inetPppoeSoftintr(void);
static INET_NOINLINE void inetPppoeDiscInput(InetMbuf *m);
static INET_NOINLINE void inetPppoeDataInput(InetMbuf *m);
static INET_NOINLINE int inetPppoeIoctl(InetIfnet *ifp, u32 cmd, void *data);
static INET_NOINLINE void inetPppoeTimeout(void *arg);
static INET_NOINLINE int inetPppoeDisconnect(InetPppoeSoftc *sc);
static INET_NOINLINE int inetPppoeSendPadr(InetPppoeSoftc *sc);
static INET_NOINLINE int inetPppoeSendPadt(InetIfnet *ethIfp, u16 session, const u8 *destMac);
static INET_NOINLINE void inetPppoeStart(InetIfnet *ifp);
static INET_NOINLINE int inetPppoeEtherInput(InetMbuf *m, u16 etype, int *outConsumed);
static INET_NOINLINE void inetPppoeDiscInputNoit(InetMbuf *m, int off);
static INET_NOINLINE void inetPppoeLinkUp(InetSppp *sp);
static INET_NOINLINE void inetPppoeLinkDown(InetSppp *sp);
static INET_NOINLINE int inetPppoeSendPadi(InetPppoeSoftc *sc);
static INET_NOINLINE InetMbuf *inetPppoeGetMbuf(u32 len);

/* [455] 0x02E478 */
static INET_NOINLINE int inetPppoeAttach(const char *ethName)
{
    InetIfnet *ethIfp;

    sceKernelMemset(&g_pppoe_disc_inq, 0, sizeof(g_pppoe_disc_inq));
    g_pppoe_disc_inq.ifq_maxlen = 50;
    sceKernelMemset(&g_pppoe_sess_inq, 0, sizeof(g_pppoe_sess_inq));
    g_pppoe_sess_inq.ifq_maxlen = 50;
    sceKernelMemset(&g_pppoe_sc, 0, sizeof(g_pppoe_sc));

    ethIfp = sceNet_lib_1D10419C(ethName);
    g_pppoe_sc.sc_eth_if = ethIfp;
    if (ethIfp == NULL) {
        return -1;
    }

    sceNet_lib_B5CE388A(g_pppoe_sc.sc_sppp.pp_if.if_xname, "pppoe0", 6);
    g_pppoe_sc.sc_sppp.pp_if.if_softc = &g_pppoe_sc;
    g_pppoe_sc.sc_sppp.pp_if.if_data.ifi_mtu = 1492;
    g_pppoe_sc.sc_sppp.pp_if.if_output = (void *)inetSpppOutput;
    g_pppoe_sc.sc_sppp.pp_if.if_ioctl = (void *)inetPppoeIoctl;
    g_pppoe_sc.sc_sppp.pp_if.if_start = (void *)inetPppoeStart;
    g_pppoe_sc.sc_sppp.pp_if.if_flags = (u16)(INET_IFF_POINTOPOINT | INET_IFF_MULTICAST | 0x0800);
    g_pppoe_sc.sc_sppp.pp_if.if_data.ifi_type = 23;
    g_pppoe_sc.sc_sppp.pp_if.if_data.ifi_hdrlen = 20;
    g_pppoe_sc.sc_sppp.pp_if.if_snd.ifq_maxlen = 50;
    g_pppoe_sc.sc_sppp.pp_flags |= 0x21;
    g_pppoe_sc.sc_sppp.pp_framebytes = 6;
    sceKernelMemset(g_pppoe_sc.sc_dest, 0xFF, 6);
    sceNet_lib_25CC373A(g_pppoe_sc.sc_timeout);
    g_pppoe_sc.sc_sppp.pp_tls = (void *)inetPppoeLinkUp;
    g_pppoe_sc.sc_sppp.pp_tlf = (void *)inetPppoeLinkDown;

    sceNet_lib_701DDDC3(&g_pppoe_sc.sc_sppp.pp_if);
    inetSpppAttach(&g_pppoe_sc.sc_sppp.pp_if);
    return 0;
}

/* [456] 0x02E5EC */
static INET_NOINLINE void inetPppoeDetach(void)
{
    sceNet_lib_A93A93E9(g_pppoe_sc.sc_timeout);

    while (g_pppoe_disc_inq.ifq_head != NULL) {
        InetMbuf *m = g_pppoe_disc_inq.ifq_head;
        g_pppoe_disc_inq.ifq_head = m->m_nextpkt;
        if (g_pppoe_disc_inq.ifq_head == NULL) {
            g_pppoe_disc_inq.ifq_tail = NULL;
        }
        m->m_nextpkt = NULL;
        g_pppoe_disc_inq.ifq_len--;
        sceNetIfhandle_lib_F56FAC82(m);
    }

    while (g_pppoe_sess_inq.ifq_head != NULL) {
        InetMbuf *m = g_pppoe_sess_inq.ifq_head;
        g_pppoe_sess_inq.ifq_head = m->m_nextpkt;
        if (g_pppoe_sess_inq.ifq_head == NULL) {
            g_pppoe_sess_inq.ifq_tail = NULL;
        }
        m->m_nextpkt = NULL;
        g_pppoe_sess_inq.ifq_len--;
        sceNetIfhandle_lib_F56FAC82(m);
    }

    inetSpppDetach(&g_pppoe_sc.sc_sppp.pp_if);
    sceNet_lib_D5A03BC0(&g_pppoe_sc.sc_sppp.pp_if);

    if (g_pppoe_sc.sc_concentrator_name != NULL) {
        sceNetIfhandle_lib_76BAD213(g_pppoe_sc.sc_concentrator_name);
    }
    if (g_pppoe_sc.sc_service_name != NULL) {
        sceNetIfhandle_lib_76BAD213(g_pppoe_sc.sc_service_name);
    }
    if (g_pppoe_sc.sc_ac_cookie != NULL) {
        sceNetIfhandle_lib_76BAD213(g_pppoe_sc.sc_ac_cookie);
    }
    if (g_pppoe_sc.sc_relay_sid != NULL) {
        sceNetIfhandle_lib_76BAD213(g_pppoe_sc.sc_relay_sid);
    }
}

/* [457] 0x02E738 */
static INET_NOINLINE void inetPppoeSoftintr(void)
{
    int hadDisc;
    int hadSess;

    do {
        hadDisc = 0;
        hadSess = 0;
        for (;;) {
            int spl = sceNet_lib_94B44F26();
            InetMbuf *m = g_pppoe_disc_inq.ifq_head;
            if (m != NULL) {
                g_pppoe_disc_inq.ifq_head = m->m_nextpkt;
                if (g_pppoe_disc_inq.ifq_head == NULL) {
                    g_pppoe_disc_inq.ifq_tail = NULL;
                }
                m->m_nextpkt = NULL;
                g_pppoe_disc_inq.ifq_len--;
            }
            sceNet_lib_515B2F33(spl);
            if (m == NULL) {
                break;
            }
            hadDisc = 1;
            inetPppoeDiscInput(m);
        }
        for (;;) {
            int spl = sceNet_lib_94B44F26();
            InetMbuf *m = g_pppoe_sess_inq.ifq_head;
            if (m != NULL) {
                g_pppoe_sess_inq.ifq_head = m->m_nextpkt;
                if (g_pppoe_sess_inq.ifq_head == NULL) {
                    g_pppoe_sess_inq.ifq_tail = NULL;
                }
                m->m_nextpkt = NULL;
                g_pppoe_sess_inq.ifq_len--;
            }
            sceNet_lib_515B2F33(spl);
            if (m == NULL) {
                break;
            }
            hadSess = 1;
            inetPppoeDataInput(m);
        }
    } while (hadDisc || hadSess);
}

/* [458] 0x02E858 */
static INET_NOINLINE void inetPppoeDiscInput(InetMbuf *m)
{
    inetPppoeDiscInputNoit(m, 0);
}

/* [459] 0x02E874 */
static INET_NOINLINE void inetPppoeDataInput(InetMbuf *m)
{
    u8 *ph;
    u16 session;
    u16 payloadLen;

    sceNetIfhandle_lib_BFF3CEA5(m, 14);
    if (M_PKTLEN(m) <= 6) {
        sceNetIfhandle_lib_F56FAC82(m);
        return;
    }
    if (m->m_len < 6) {
        m = sceNetIfhandle_lib_49EDBB18(m, 6);
        if (m == NULL) {
            return;
        }
    }

    ph = m->m_data;
    if (ph[0] != 0x11 || ph[1] != 0) {
        sceNetIfhandle_lib_F56FAC82(m);
        return;
    }
    session = (u16)(((u16)ph[2] << 8) | ph[3]);
    if (session == 0 || g_pppoe_sc.sc_state != 3 ||
        g_pppoe_sc.sc_session != session ||
        g_pppoe_sc.sc_eth_if != M_RCVIF(m)) {
        sceNetIfhandle_lib_F56FAC82(m);
        return;
    }

    payloadLen = (u16)(((u16)ph[4] << 8) | ph[5]);
    sceNetIfhandle_lib_BFF3CEA5(m, 6);
    if (M_PKTLEN(m) < (s32)payloadLen) {
        sceNetIfhandle_lib_F56FAC82(m);
        return;
    }

    M_RCVIF(m) = &g_pppoe_sc.sc_sppp.pp_if;
    g_pppoe_sc.sc_sppp.pp_if.if_data.ifi_ipackets++;
    inetSpppInput(&g_pppoe_sc.sc_sppp.pp_if, m);
}

/* [460] 0x02E9C0 */
static INET_NOINLINE int inetPppoeIoctl(InetIfnet *ifp, u32 cmd, void *data)
{
    InetPppoeSoftc *sc = (InetPppoeSoftc *)ifp;
    u32 *uarg = (u32 *)data;

    switch (cmd) {
    case 0x80206910U: /* SIOCSIFFLAGS */
        if ((((InetIfreq *)data)->ifr_u.ifru_flags & INET_IFF_UP) == 0 &&
            sc->sc_state > 0 && sc->sc_state < 3) {
            sceNet_lib_A93A93E9(sc->sc_timeout);
            sc->sc_state = 0;
            sc->sc_padi_retried = 0;
            sc->sc_padr_retried = 0;
            sceKernelMemset(sc->sc_dest, 0xFF, 6);
        }
        break;
    case 0xC0206970U: {
        InetPppoeStateIoctl *st = (InetPppoeStateIoctl *)data;
        st->state = sc->sc_state;
        st->session = sc->sc_session;
        st->padi_retried = sc->sc_padi_retried;
        st->padr_retried = sc->sc_padr_retried;
        return 0;
    }
    case 0xC1146974U: {
        InetPppoeErrIoctl *er = (InetPppoeErrIoctl *)data;
        er->error_code = sc->sc_last_error;
        sceKernelMemcpy(er->errmsg, sc->sc_last_errmsg, 256);
        return 0;
    }
    case 0x8020697FU: /* SIOCSIFMTU */
        if ((s32)uarg[4] > 1492) {
            return INET_EINVAL;
        }
        break;
    case 0x80146971U:
        sc->sc_last_stage = 0;
        return 0;
    case 0x80146972U:
        uarg[4] = (u32)sc->sc_last_stage;
        return 0;
    default:
        break;
    }

    return inetSpppIoctl(ifp, cmd, data);
}

/* [461] 0x02EB38 */
static INET_NOINLINE void inetPppoeTimeout(void *arg)
{
    InetPppoeSoftc *sc = (InetPppoeSoftc *)arg;

    switch (sc->sc_state) {
    case 1: {
        int spl = sceNet_lib_94B44F26();
        sc->sc_padi_retried++;
        if (inetPppoeSendPadi(sc) != 0) {
            sc->sc_padi_retried--;
        }
        sceNet_lib_7C86FBA4(sc->sc_timeout, 500, inetPppoeTimeout, sc);
        sceNet_lib_515B2F33(spl);
        break;
    }
    case 2: {
        int spl = sceNet_lib_94B44F26();
        sc->sc_padr_retried++;
        if (sc->sc_padr_retried < 2) {
            if (inetPppoeSendPadr(sc) != 0) {
                sc->sc_padr_retried--;
            }
        } else {
            sceKernelMemset(sc->sc_dest, 0xFF, 6);
            sc->sc_state = 1;
            sc->sc_padr_retried = 0;
            inetPppoeSendPadi(sc);
        }
        sceNet_lib_7C86FBA4(sc->sc_timeout, 500, inetPppoeTimeout, sc);
        sceNet_lib_515B2F33(spl);
        break;
    }
    case 4:
        inetPppoeDisconnect(sc);
        break;
    default:
        break;
    }
}

/* [462] 0x02EC5C */
static INET_NOINLINE int inetPppoeDisconnect(InetPppoeSoftc *sc)
{
    int err = INET_EBUSY;
    int spl = sceNet_lib_94B44F26();

    if (sc->sc_state >= 3) {
        err = inetPppoeSendPadt(sc->sc_eth_if, sc->sc_session, sc->sc_dest);
    }
    sc->sc_state = 0;
    sceKernelMemset(sc->sc_dest, 0xFF, 6);
    if (sc->sc_ac_cookie != NULL) {
        sceNetIfhandle_lib_76BAD213(sc->sc_ac_cookie);
        sc->sc_ac_cookie = NULL;
    }
    sc->sc_ac_cookie_len = 0;
    if (sc->sc_relay_sid != NULL) {
        sceNetIfhandle_lib_76BAD213(sc->sc_relay_sid);
        sc->sc_relay_sid = NULL;
    }
    sc->sc_relay_sid_len = 0;
    sc->sc_session = 0;
    sc->sc_sppp.pp_down(&sc->sc_sppp);
    sceNet_lib_515B2F33(spl);
    return err;
}

/* [463] 0x02ED38 */
static INET_NOINLINE int inetPppoeSendPadr(InetPppoeSoftc *sc)
{
    u32 svcLen = 0;
    u32 payloadLen = 12;
    InetMbuf *m;
    u8 *p;
    InetPppoeSoftc *scSelf = sc;

    if (sc->sc_state != 2) {
        return INET_EIO;
    }

    if (sc->sc_service_name != NULL) {
        svcLen = (u32)sceNet_lib_D8722983(sc->sc_service_name);
        payloadLen += svcLen;
    }
    if (sc->sc_ac_cookie_len != 0) {
        payloadLen += sc->sc_ac_cookie_len + 4;
    }
    if (sc->sc_relay_sid_len != 0) {
        payloadLen += sc->sc_relay_sid_len + 4;
    }

    m = inetPppoeGetMbuf(payloadLen + 6);
    if (m == NULL) {
        return INET_ENOBUFS;
    }

    p = m->m_data;
    p[0] = 0x11;
    p[1] = 0x19; /* PADR */
    p[2] = 0;
    p[3] = 0;
    p[4] = (u8)(payloadLen >> 8);
    p[5] = (u8)payloadLen;
    p[6] = 0x01;
    p[7] = 0x01; /* TAG_SNAME */
    if (sc->sc_service_name != NULL) {
        p[8] = (u8)(svcLen >> 8);
        p[9] = (u8)svcLen;
        sceKernelMemcpy(p + 10, sc->sc_service_name, svcLen);
        p += 10 + svcLen;
    } else {
        p[8] = 0;
        p[9] = 0;
        p += 10;
    }

    if (sc->sc_ac_cookie_len != 0) {
        p[0] = 0x01;
        p[1] = 0x04; /* TAG_ACCOOKIE */
        p[2] = (u8)(sc->sc_ac_cookie_len >> 8);
        p[3] = (u8)sc->sc_ac_cookie_len;
        sceKernelMemcpy(p + 4, sc->sc_ac_cookie, sc->sc_ac_cookie_len);
        p += 4 + sc->sc_ac_cookie_len;
    }
    if (sc->sc_relay_sid_len != 0) {
        p[0] = 0x01;
        p[1] = 0x10; /* TAG_RELAYSID */
        p[2] = (u8)(sc->sc_relay_sid_len >> 8);
        p[3] = (u8)sc->sc_relay_sid_len;
        sceKernelMemcpy(p + 4, sc->sc_relay_sid, sc->sc_relay_sid_len);
        p += 4 + sc->sc_relay_sid_len;
    }

    p[0] = 0x01;
    p[1] = 0x03; /* TAG_HUNIQUE */
    p[2] = 0;
    p[3] = 4;
    sceKernelMemcpy(p + 4, &scSelf, 4);

    if (sc->sc_eth_if == NULL) {
        sceNetIfhandle_lib_F56FAC82(m);
        return INET_EIO;
    }

    {
        InetSockaddr dstSa;
        u16 etype = (sc->sc_state == 3) ? 0x8864 : 0x8863;
        sceKernelMemset(&dstSa, 0, sizeof(dstSa));
        dstSa.sa_family = 0; /* AF_UNSPEC */
        sceKernelMemcpy(dstSa.sa_data, sc->sc_dest, 6);
        dstSa.sa_data[12] = (u8)(etype >> 8);
        dstSa.sa_data[13] = (u8)(etype & 0xFF);
        m->m_flags &= (u16)~(INET_M_BCAST | INET_M_MCAST);
        sc->sc_sppp.pp_if.if_data.ifi_opackets++;
        sc->sc_eth_if->if_output(sc->sc_eth_if, m, &dstSa, NULL);
    }
    return 0;
}

/* [464] 0x02F04C */
static INET_NOINLINE int inetPppoeSendPadt(InetIfnet *ethIfp, u16 session, const u8 *destMac)
{
    InetMbuf *m = inetPppoeGetMbuf(6);
    u8 *p;
    InetSockaddr dstSa;
    u16 etypeNet;

    if (m == NULL) {
        return INET_EIO;
    }

    p = m->m_data;
    p[0] = 0x11;
    p[1] = 0xA7; /* PADT */
    p[2] = (u8)(session >> 8);
    p[3] = (u8)session;
    p[4] = 0;
    p[5] = 0;

    sceKernelMemset(&dstSa, 0, sizeof(dstSa));
    dstSa.sa_family = 0;
    sceKernelMemcpy(dstSa.sa_data, destMac, 6);
    dstSa.sa_data[12] = 0x88;
    dstSa.sa_data[13] = 0x63;
    m->m_flags &= (u16)~(INET_M_BCAST | INET_M_MCAST);
    return ethIfp->if_output(ethIfp, m, &dstSa, NULL);
}

/* [465] 0x02F138 */
static INET_NOINLINE void inetPppoeStart(InetIfnet *ifp)
{
    InetPppoeSoftc *sc = (InetPppoeSoftc *)ifp;

    if (inetSpppIsEmpty(ifp)) {
        return;
    }
    if (sc->sc_state < 3) {
        inetSpppFlush(&sc->sc_sppp);
        return;
    }

    for (;;) {
        InetMbuf *m = inetSpppDequeue(ifp);
        s32 payloadLen;
        u8 *ph;

        if (m == NULL) {
            return;
        }
        payloadLen = M_PKTLEN(m);
        if ((m->m_flags & INET_M_EXT) == 0) {
            u32 lead = ((m->m_flags & INET_M_PKTHDR) != 0)
                           ? (u32)(m->m_data - M_PKTDAT(m))
                           : (u32)(m->m_data - M_DAT(m));
            if (lead >= 6) {
                m->m_data -= 6;
                m->m_len += 6;
            } else {
                m = sceNetIfhandle_lib_C3325FDC(m, 6, INET_M_DONTWAIT);
            }
        } else {
            m = sceNetIfhandle_lib_C3325FDC(m, 6, INET_M_DONTWAIT);
        }

        if (m == NULL) {
            ifp->if_data.ifi_oerrors++;
            continue;
        }
        if ((m->m_flags & INET_M_PKTHDR) != 0) {
            M_PKTLEN(m) += 6;
        }

        ph = m->m_data;
        ph[0] = 0x11;
        ph[1] = 0;
        ph[2] = (u8)(sc->sc_session >> 8);
        ph[3] = (u8)sc->sc_session;
        ph[4] = (u8)(payloadLen >> 8);
        ph[5] = (u8)payloadLen;

        if (sc->sc_eth_if != NULL) {
            InetSockaddr dstSa;
            u16 etypeNet = inetBswap16((sc->sc_state == 3) ? 0x8864 : 0x8863);
            sceKernelMemset(&dstSa, 0, sizeof(dstSa));
            dstSa.sa_family = 0;
            sceKernelMemcpy(dstSa.sa_data, sc->sc_dest, 6);
            sceKernelMemcpy(dstSa.sa_data + 12, &etypeNet, 2);
            m->m_flags &= (u16)~(INET_M_BCAST | INET_M_MCAST);
            ifp->if_data.ifi_opackets++;
            sc->sc_eth_if->if_output(sc->sc_eth_if, m, &dstSa, NULL);
        } else {
            sceNetIfhandle_lib_F56FAC82(m);
        }
    }
}

/* [466] 0x02F338 */
static INET_NOINLINE int inetPppoeEtherInput(InetMbuf *m, u16 etype, int *outConsumed)
{
    if (etype == 0x8863 || etype == 0x8864) {
        InetIfQueue *inq = (etype == 0x8863) ? &g_pppoe_disc_inq : &g_pppoe_sess_inq;
        int spl = sceNet_lib_94B44F26();
        if (inq->ifq_len >= inq->ifq_maxlen) {
            inq->ifq_drops++;
            sceNetIfhandle_lib_F56FAC82(m);
        } else {
            m->m_nextpkt = NULL;
            if (inq->ifq_tail != NULL) {
                inq->ifq_tail->m_nextpkt = m;
            } else {
                inq->ifq_head = m;
            }
            inq->ifq_tail = m;
            inq->ifq_len++;
        }
        sceNet_lib_515B2F33(spl);
        sceNet_lib_05D525E4(30);
        *outConsumed = 1;
    }
    return 0;
}

/* [467] 0x02F414 */
static INET_NOINLINE void inetPppoeDiscInputNoit(InetMbuf *m, int off)
{
    u8 *ethHdr;
    s32 curOff = off + 14;
    s32 tagOff = 0;
    InetMbuf *seg;
    u8 *ph;
    u8 code;
    u16 session;
    u16 payloadLen;
    InetPppoeSoftc *sc = NULL;
    u8 *acCookie = NULL;
    u32 acCookieLen = 0;
    u8 *relaySid = NULL;
    u32 relaySidLen = 0;
    u32 errTag = 0;

    if (m->m_len < 14) {
        m = sceNetIfhandle_lib_49EDBB18(m, 14);
        if (m == NULL) {
            return;
        }
    }
    ethHdr = m->m_data;
    if (M_PKTLEN(m) - curOff < 7) {
        sceNetIfhandle_lib_F56FAC82(m);
        return;
    }

    seg = sceNetIfhandle_lib_E80F00A4(m, curOff, 6, &tagOff);
    if (seg == NULL) {
        return;
    }
    ph = seg->m_data + tagOff;
    if (ph[0] != 0x11) {
        sceNetIfhandle_lib_F56FAC82(m);
        return;
    }
    code = ph[1];
    session = (u16)(((u16)ph[2] << 8) | ph[3]);
    payloadLen = (u16)(((u16)ph[4] << 8) | ph[5]);
    curOff += 6;

    if (M_PKTLEN(m) < curOff + (s32)payloadLen) {
        sceNetIfhandle_lib_F56FAC82(m);
        return;
    }
    sceNetIfhandle_lib_BFF3CEA5(m, (curOff + (s32)payloadLen) - M_PKTLEN(m));

    if (session != 0 && g_pppoe_sc.sc_state == 3 &&
        g_pppoe_sc.sc_session == session &&
        g_pppoe_sc.sc_eth_if == M_RCVIF(m)) {
        sc = &g_pppoe_sc;
    }

    /* Parse PPPoE tags */
    while (M_PKTLEN(m) >= curOff + 4) {
        u8 *tp;
        u16 tagType;
        u16 tagLen;

        seg = sceNetIfhandle_lib_E80F00A4(m, curOff, 4, &tagOff);
        if (seg == NULL) {
            return;
        }
        tp = seg->m_data + tagOff;
        tagType = (u16)(((u16)tp[0] << 8) | tp[1]);
        tagLen = (u16)(((u16)tp[2] << 8) | tp[3]);
        if (M_PKTLEN(m) < curOff + (s32)tagLen) {
            sceNetIfhandle_lib_F56FAC82(m);
            return;
        }

        if (tagType == 0) { /* EOL */
            break;
        } else if (tagType == 0x0103) { /* HUNIQUE */
            if (sc == NULL) {
                InetPppoeSoftc *cand = NULL;
                seg = sceNetIfhandle_lib_E80F00A4(m, curOff + 4, tagLen, &tagOff);
                if (seg == NULL) {
                    return;
                }
                if (tagLen == 4) {
                    sceKernelMemcpy(&cand, seg->m_data + tagOff, 4);
                    if (cand == &g_pppoe_sc &&
                        (g_pppoe_sc.sc_state == 1 || g_pppoe_sc.sc_state == 2) &&
                        g_pppoe_sc.sc_eth_if == M_RCVIF(m)) {
                        sc = &g_pppoe_sc;
                    }
                }
            }
        } else if (tagType == 0x0104) { /* ACCOOKIE */
            if (acCookie == NULL) {
                seg = sceNetIfhandle_lib_E80F00A4(m, curOff + 4, tagLen, &tagOff);
                if (seg == NULL) {
                    return;
                }
                acCookie = seg->m_data + tagOff;
                acCookieLen = tagLen;
            }
        } else if (tagType == 0x0110) { /* RELAYSID */
            if (relaySid == NULL) {
                seg = sceNetIfhandle_lib_E80F00A4(m, curOff + 4, tagLen, &tagOff);
                if (seg == NULL) {
                    return;
                }
                relaySid = seg->m_data + tagOff;
                relaySidLen = tagLen;
            }
        } else if (tagType == 0x0201 || tagType == 0x0202 || tagType == 0x0203) {
            errTag = tagType;
        }

        if (sc != NULL && errTag != 0) {
            sc->sc_last_error = errTag;
            if (code == 0x07 && sc->sc_last_stage < 2) {
                sc->sc_last_stage = 2;
            } else if (code == 0x65 && sc->sc_last_stage < 4) {
                sc->sc_last_stage = 4;
            }
            if (tagLen != 0) {
                seg = sceNetIfhandle_lib_E80F00A4(m, curOff + 4, tagLen, &tagOff);
                if (seg == NULL) {
                    return;
                }
                if (seg->m_data[tagOff] != 0) {
                    u32 copyLen = (tagLen < 256) ? tagLen : 256;
                    sceKernelMemset(sc->sc_last_errmsg, 0, 256);
                    sceKernelMemcpy(sc->sc_last_errmsg, seg->m_data + tagOff, copyLen);
                }
            }
            sceNetIfhandle_lib_F56FAC82(m);
            return;
        }

        curOff += (s32)tagLen + 4;
    }

    if (code == 0x07) { /* PADO */
        if (sc != NULL && sc->sc_state == 1) {
            if (acCookie != NULL) {
                if (sc->sc_ac_cookie != NULL) {
                    sceNetIfhandle_lib_76BAD213(sc->sc_ac_cookie);
                }
                sc->sc_ac_cookie = (u8 *)sceNetIfhandle_lib_15CFE3C0(acCookieLen);
                if (sc->sc_ac_cookie == NULL) {
                    sceNetIfhandle_lib_F56FAC82(m);
                    return;
                }
                sc->sc_ac_cookie_len = acCookieLen;
                sceKernelMemcpy(sc->sc_ac_cookie, acCookie, acCookieLen);
            }
            if (relaySid != NULL) {
                if (sc->sc_relay_sid != NULL) {
                    sceNetIfhandle_lib_76BAD213(sc->sc_relay_sid);
                }
                sc->sc_relay_sid = (u8 *)sceNetIfhandle_lib_15CFE3C0(relaySidLen);
                if (sc->sc_relay_sid == NULL) {
                    sceNetIfhandle_lib_F56FAC82(m);
                    return;
                }
                sc->sc_relay_sid_len = relaySidLen;
                sceKernelMemcpy(sc->sc_relay_sid, relaySid, relaySidLen);
            }
            sceKernelMemcpy(sc->sc_dest, ethHdr + 6, 6);
            sceNet_lib_A93A93E9(sc->sc_timeout);
            if (sc->sc_last_stage < 3) {
                sc->sc_last_stage = 3;
            }
            sc->sc_state = 2;
            sc->sc_padr_retried = 0;
            inetPppoeSendPadr(sc);
            sceNet_lib_7C86FBA4(sc->sc_timeout, 500, inetPppoeTimeout, sc);
        }
    } else if (code == 0x65) { /* PADS */
        if (sc != NULL) {
            sc->sc_session = session;
            sceNet_lib_A93A93E9(sc->sc_timeout);
            if (sc->sc_last_stage < 5) {
                sc->sc_last_stage = 5;
            }
            sc->sc_state = 3;
            sc->sc_sppp.pp_up(&sc->sc_sppp);
        }
    } else if (code == 0xA7) { /* PADT */
        if (sc != NULL) {
            sceNet_lib_A93A93E9(sc->sc_timeout);
            sc->sc_state = 0;
            if (sc->sc_last_stage < 6) {
                sc->sc_last_stage = 6;
            }
            sceKernelMemset(sc->sc_dest, 0xFF, 6);
            if (sc->sc_ac_cookie != NULL) {
                sceNetIfhandle_lib_76BAD213(sc->sc_ac_cookie);
                sc->sc_ac_cookie = NULL;
            }
            sc->sc_ac_cookie_len = 0;
            if (sc->sc_relay_sid != NULL) {
                sceNetIfhandle_lib_76BAD213(sc->sc_relay_sid);
                sc->sc_relay_sid = NULL;
            }
            sc->sc_relay_sid_len = 0;
            sc->sc_session = 0;
            sc->sc_sppp.pp_down(&sc->sc_sppp);
        }
    }

    sceNetIfhandle_lib_F56FAC82(m);
}

/* [468] 0x02FA60 */
static INET_NOINLINE void inetPppoeLinkUp(InetSppp *sp)
{
    InetPppoeSoftc *sc = (InetPppoeSoftc *)sp;
    if (sc->sc_state == 0) {
        int spl = sceNet_lib_94B44F26();
        if (sc->sc_last_stage <= 0) {
            sc->sc_last_stage = 1;
        }
        sc->sc_state = 1;
        sc->sc_padr_retried = 0;
        inetPppoeSendPadi(sc);
        sceNet_lib_7C86FBA4(sc->sc_timeout, 500, inetPppoeTimeout, sc);
        sceNet_lib_515B2F33(spl);
    }
}

/* [469] 0x02FAF4 */
static INET_NOINLINE void inetPppoeLinkDown(InetSppp *sp)
{
    InetPppoeSoftc *sc = (InetPppoeSoftc *)sp;
    if (sc->sc_state >= 3) {
        sc->sc_state = 4;
        sceNet_lib_7C86FBA4(sc->sc_timeout, 2, inetPppoeTimeout, sc);
    }
}

/* [470] 0x02FB40 */
static INET_NOINLINE int inetPppoeSendPadi(InetPppoeSoftc *sc)
{
    u32 svcLen = 0;
    u32 acLen = 0;
    u32 payloadLen = 12;
    InetMbuf *m;
    u8 *p;
    InetPppoeSoftc *scSelf = sc;

    if (sc->sc_state >= 2) {
        return 0;
    }

    if (sc->sc_service_name != NULL) {
        svcLen = (u32)sceNet_lib_D8722983(sc->sc_service_name);
        payloadLen += svcLen;
    }
    if (sc->sc_concentrator_name != NULL) {
        acLen = (u32)sceNet_lib_D8722983(sc->sc_concentrator_name);
        payloadLen += acLen + 4;
    }

    m = inetPppoeGetMbuf(payloadLen + 6);
    if (m == NULL) {
        return INET_ENOBUFS;
    }

    p = m->m_data;
    p[0] = 0x11;
    p[1] = 0x09; /* PADI */
    p[2] = 0;
    p[3] = 0;
    p[4] = (u8)(payloadLen / 256U);
    p[5] = (u8)(payloadLen % 256U);
    p[6] = 0x01;
    p[7] = 0x01; /* TAG_SNAME */
    if (sc->sc_service_name != NULL) {
        p[8] = (u8)(svcLen / 256U);
        p[9] = (u8)(svcLen % 256U);
        sceKernelMemcpy(p + 10, sc->sc_service_name, svcLen);
        p += 10 + svcLen;
    } else {
        p[8] = 0;
        p[9] = 0;
        p += 10;
    }

    if (sc->sc_concentrator_name != NULL) {
        p[0] = 0x01;
        p[1] = 0x02; /* TAG_ACNAME */
        p[2] = (u8)(acLen >> 8);
        p[3] = (u8)acLen;
        sceKernelMemcpy(p + 4, sc->sc_concentrator_name, acLen);
        p += 4 + acLen;
    }

    p[0] = 0x01;
    p[1] = 0x03; /* TAG_HUNIQUE */
    p[2] = 0;
    p[3] = 4;
    sceKernelMemcpy(p + 4, &scSelf, 4);

    if (sc->sc_eth_if == NULL) {
        sceNetIfhandle_lib_F56FAC82(m);
        return INET_EIO;
    }

    {
        InetSockaddr dstSa;
        u16 etype = (sc->sc_state == 3) ? 0x8864 : 0x8863;
        sceKernelMemset(&dstSa, 0, sizeof(dstSa));
        dstSa.sa_family = 0;
        sceKernelMemcpy(dstSa.sa_data, sc->sc_dest, 6);
        dstSa.sa_data[12] = (u8)(etype >> 8);
        dstSa.sa_data[13] = (u8)(etype & 0xFF);
        m->m_flags &= (u16)~(INET_M_BCAST | INET_M_MCAST);
        sc->sc_sppp.pp_if.if_data.ifi_opackets++;
        sc->sc_eth_if->if_output(sc->sc_eth_if, m, &dstSa, NULL);
    }
    return 0;
}

/* [471] 0x02FE20 */
static INET_NOINLINE InetMbuf *inetPppoeGetMbuf(u32 len)
{
    InetMbuf *m = (InetMbuf *)sceNetIfhandle_lib_15CFE3C0(sizeof(InetMbuf));
    if (m == NULL) {
        return NULL;
    }

    m->m_next = NULL;
    m->m_nextpkt = NULL;
    m->m_data = M_PKTDAT(m);
    m->m_type = INET_MT_DATA;
    m->m_flags = INET_M_PKTHDR;
    M_CSUM(m) = 0;

    if (len + 14U > 208U) {
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
            sceNetIfhandle_lib_76BAD213(m);
            return NULL;
        }
    }

    m->m_data += 14;
    m->m_len = (s32)len;
    M_PKTLEN(m) = (s32)len;
    M_RCVIF(m) = NULL;
    return m;
}

/* [472] 0x02FF28 */
int sceNetInet_lib_7CB1D9E3(const char *ethName)
{
    int spl = sceNet_lib_94B44F26();
    int ret = inetPppoeAttach(ethName);

    if (ret >= 0) {
        ret = sceNet_lib_572AD6ED(inetPppoeSoftintr);
        if (ret >= 0) {
            ret = inetEtherSetPppoeInput((void *)inetPppoeEtherInput);
            if (ret >= 0) {
                sceNet_lib_515B2F33(spl);
                return 0;
            }
        }
        {
            int cleanSpl = sceNet_lib_94B44F26();
            inetEtherClearPppoeInput();
            sceNet_lib_87DC7A7E();
            inetPppoeDetach();
            sceNet_lib_515B2F33(cleanSpl);
        }
    }

    sceNet_lib_515B2F33(spl);
    return ret;
}

/* [473] 0x02FFE8 */
int sceNetInet_lib_4C13BE10(void)
{
    int spl = sceNet_lib_94B44F26();
    inetEtherClearPppoeInput();
    sceNet_lib_87DC7A7E();
    inetPppoeDetach();
    sceNet_lib_515B2F33(spl);
    return 0;
}
