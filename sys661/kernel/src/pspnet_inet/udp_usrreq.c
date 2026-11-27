/*
 * PSP 6.61 SceNetInet_Library (pspnet_inet.prx)
 * UDP protocol and Sony PSP P2P UDP/TCP encapsulation (udp_usrreq.c / udpp2p)
 */

#include "pspnet_inet_internal.h"

/* UDP header wire layout */
typedef struct InetUdpHdr {
    u16 uh_sport;
    u16 uh_dport;
    u16 uh_ulen;
    u16 uh_sum;
} InetUdpHdr;

/* UDP statistics (72 bytes = 9 x u64 at 0x366B8) */
typedef struct InetUdpStat {
    u64 udps_ipackets;
    u64 udps_hdrops;
    u64 udps_badsum;
    u64 udps_badlen;
    u64 udps_noport;
    u64 udps_noportbcast;
    u64 udps_fullsock;
    u64 udpps_pcbhashmiss;
    u64 udps_opackets;
} InetUdpStat;

/* Sony P2P statistics (56 bytes = 7 x u64 at 0x36740) */
typedef struct InetP2pStat {
    u64 p2ps_udp_ipackets;
    u64 p2ps_bad_vport;
    u64 p2ps_bad_len;
    u64 p2ps_fullsock;
    u64 p2ps_udp_opackets;
    u64 p2ps_unused;
    u64 p2ps_tcp_packets;
} InetP2pStat;

/* Sony P2P UDP PCB (120 bytes: 92-byte InetInpcbReal + P2P extension) */
typedef struct InetP2pInpcb {
    InetInpcbReal base;
    struct InetP2pInpcb *p2p_next;
    struct InetP2pInpcb **p2p_prev;
    u16 p2p_fport;
    u16 p2p_lport;
    u32 p2p_hdrlen;
    u32 p2p_extra_a;
    u32 p2p_extra_b;
    u32 p2p_extra_c;
} InetP2pInpcb;

/* Sony P2P ioctl parameter struct (36 bytes, 2-byte aligned with packed u32 addr) */
typedef struct InetP2pKeyIoctl {
    u16 family;
    u16 port;
    u32 addr __attribute__((packed));
    u8 zero[8];
    u8 key[16];
    u16 lport;
    u16 pad_align;
} __attribute__((aligned(2))) InetP2pKeyIoctl;

int g_udpcksum = 1;
static u32 g_udbhashsize = 128;
u32 g_udp_sendspace = 9216;
u32 g_udp_recvspace = 41600;
u32 g_udp_p2p_sendspace = 9216;
u32 g_udp_p2p_recvspace = 124800;

static InetUdpStat g_udpstat;
InetInpcbTable g_udbtable;

static InetP2pInpcb *g_p2p_pcb_head = NULL;
static InetP2pKeyEntry *g_p2p_key_head = NULL;
u16 g_p2p_default_port = 0;
static u16 g_p2p_ephemeral_port = 0;
static int g_p2p_local_key_set = 0;
static u8 g_p2p_local_key[16];
static InetP2pStat g_p2p_stat;
static u8 g_p2p_reserved_buf[16];

static INET_NOINLINE int inetUdp4RealInput(InetSockaddrIn *src, InetSockaddrIn *dst, InetMbuf *m, int iphlen);
static INET_NOINLINE int inetUdpOutput(InetMbuf *m, InetInpcbReal *inp);
static INET_NOINLINE void inetUdp4Sendup(InetMbuf *m, int off, InetSockaddrIn *src, InetSocket *so);
static INET_NOINLINE void inetUdpNotify(InetInpcbReal *inp, int err);
static INET_NOINLINE int inetUdpP2pInput(InetSockaddrIn *src, InetSockaddrIn *dst, InetMbuf *m, int iphlen);
static INET_NOINLINE int inetTcpP2pInput(InetSockaddrIn *src, InetSockaddrIn *dst, InetMbuf *m, int iphlen);
static INET_NOINLINE int inetUdpP2pOutput(InetMbuf *m, InetP2pInpcb *p2p);
static INET_NOINLINE int inetP2pIoctl(InetSocket *so, u32 cmd, void *data, InetIfnet *ifp);
static INET_NOINLINE void inetP2pFlushKeys(void);
INET_NOINLINE InetP2pKeyEntry *inetP2pFindKey(u32 addr, u16 port);
static INET_NOINLINE int inetP2pAddKey(const InetP2pKeyIoctl *req);
static INET_NOINLINE int inetP2pDelKey(const InetP2pKeyIoctl *req);
static INET_NOINLINE int inetP2pEncrypt(InetMbuf **mp, int doEnc, int doAuth, InetP2pKeyEntry *keyEnt);
INET_NOINLINE InetMbuf *inetP2pDecrypt(InetMbuf *m, s32 off, u32 len, u8 protoByte);
static INET_NOINLINE int inetP2pIsLocalAddr(u32 addr);
static INET_NOINLINE int inetUdpP2pBind(InetP2pInpcb *p2p, InetMbuf *nam);
static INET_NOINLINE int inetUdpP2pConnect(InetP2pInpcb *p2p, InetMbuf *nam);
static INET_NOINLINE void inetUdpP2pDeliver(InetMbuf *m, s32 off, u32 len, InetSockaddrIn *src,
                                            InetSocket *so, u8 protoByte);
static INET_NOINLINE InetP2pInpcb *inetUdpP2pLookup(u32 laddr, u16 lport, u16 lvport, int wild);

static void p2pDetachPcb(InetP2pInpcb *p2p)
{
    InetSocket *so = p2p->base.inp_socket;
    int spl;

    so->so_pcb = NULL;
    inetSoFree(so);
    if (p2p->base.inp_options != NULL) {
        sceNetIfhandle_lib_F8825DC4(p2p->base.inp_options);
    }
    if (p2p->base.inp_route.ro_rt != NULL) {
        inetRtFree(p2p->base.inp_route.ro_rt);
    }
    inetIpFreeMoptions(p2p->base.inp_moptions);

    spl = sceNet_lib_94B44F26();
    p2p->base.inp_state = 0;
    if (p2p->p2p_next != NULL) {
        p2p->p2p_next->p2p_prev = p2p->p2p_prev;
    }
    *p2p->p2p_prev = p2p->p2p_next;
    sceNet_lib_515B2F33(spl);
    sceNetIfhandle_lib_76BAD213(p2p);
}

static InetMbuf *p2pPrependMbuf(InetMbuf *m, s32 len)
{
    if ((m->m_flags & INET_M_EXT) == 0) {
        u32 lead = ((m->m_flags & INET_M_PKTHDR) != 0)
                       ? (u32)(m->m_data - M_PKTDAT(m))
                       : (u32)(m->m_data - M_DAT(m));
        if (lead >= (u32)len) {
            m->m_data -= len;
            m->m_len += len;
        } else {
            m = sceNetIfhandle_lib_C3325FDC(m, len, INET_M_DONTWAIT);
        }
    } else {
        m = sceNetIfhandle_lib_C3325FDC(m, len, INET_M_DONTWAIT);
    }
    if (m != NULL && (m->m_flags & INET_M_PKTHDR) != 0) {
        M_PKTLEN(m) += len;
    }
    return m;
}

static int p2pAllocCluster(InetMbuf *m)
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

/* [319] 0x01D164 */
INET_NOINLINE void inetUdpInput(InetMbuf *m, int iphlen, int proto)
{
    InetIpHdr *ip;
    InetUdpHdr *uh;
    u16 ulen;

    (void)proto;
    g_udpstat.udps_ipackets++;

    if (iphlen > (s32)sizeof(InetIpHdr)) {
        inetIpStripOptions(m);
        iphlen = (s32)sizeof(InetIpHdr);
    }

    if (m->m_len < iphlen + (s32)sizeof(InetUdpHdr)) {
        m = sceNetIfhandle_lib_49EDBB18(m, iphlen + (s32)sizeof(InetUdpHdr));
        if (m == NULL) {
            g_udpstat.udps_hdrops++;
            return;
        }
    }

    ip = (InetIpHdr *)m->m_data;
    uh = (InetUdpHdr *)(m->m_data + iphlen);
    if (uh->uh_dport == 0) {
        sceNetIfhandle_lib_F56FAC82(m);
        return;
    }

    ulen = inetBswap16(uh->uh_ulen);
    if ((s32)ip->ip_len != iphlen + (s32)ulen) {
        if ((s32)ip->ip_len < iphlen + (s32)ulen || ulen < sizeof(InetUdpHdr)) {
            g_udpstat.udps_badlen++;
            sceNetIfhandle_lib_F56FAC82(m);
            return;
        }
        sceNetIfhandle_lib_BFF3CEA5(m, (iphlen + (s32)ulen) - (s32)ip->ip_len);
    }

    if (uh->uh_sum != 0 && inetIn4Cksum(m, INET_IPPROTO_UDP, iphlen, ulen) != 0) {
        g_udpstat.udps_badsum++;
        sceNetIfhandle_lib_F56FAC82(m);
        return;
    }

    {
        InetSockaddrIn src;
        InetSockaddrIn dst;

        sceNet_lib_8B615F52(&src, sizeof(src));
        src.sin_len = sizeof(InetSockaddrIn);
        src.sin_family = INET_AF_INET;
        sceNet_lib_2F305274(&ip->ip_src, &src.sin_addr, 4);
        src.sin_port = uh->uh_sport;

        sceNet_lib_8B615F52(&dst, sizeof(dst));
        dst.sin_len = sizeof(InetSockaddrIn);
        dst.sin_family = INET_AF_INET;
        sceNet_lib_2F305274(&ip->ip_dst, &dst.sin_addr, 4);
        dst.sin_port = uh->uh_dport;

        if (inetUdpP2pInput(&src, &dst, m, iphlen) == 0 &&
            inetTcpP2pInput(&src, &dst, m, iphlen) == 0 &&
            inetUdp4RealInput(&src, &dst, m, iphlen) == 0) {
            if ((m->m_flags & (INET_M_BCAST | INET_M_MCAST)) != 0) {
                g_udpstat.udps_noportbcast++;
            } else {
                g_udpstat.udps_noport++;
                inetIcmpError(m, 3, 3, 0, NULL);
                m = NULL;
            }
        }
    }

    if (m != NULL) {
        sceNetIfhandle_lib_F56FAC82(m);
    }
}

/* [320] 0x01D498 */
static INET_NOINLINE int inetUdp4RealInput(InetSockaddrIn *src, InetSockaddrIn *dst, InetMbuf *m, int iphlen)
{
    int delivered = 0;
    int off = iphlen + (s32)sizeof(InetUdpHdr);

    if (src->sin_family != INET_AF_INET || dst->sin_family != INET_AF_INET) {
        return 0;
    }

    if ((dst->sin_addr & inetBswap32(0xF0000000U)) != inetBswap32(0xE0000000U) &&
        !inetInBroadcast(dst->sin_addr, M_RCVIF(m))) {
        InetInpcbReal *inp = inetInPcbLookupConnect(&g_udbtable, src->sin_addr, src->sin_port,
                                                    dst->sin_addr, dst->sin_port);
        if (inp == NULL) {
            g_udpstat.udpps_pcbhashmiss++;
            inp = inetInPcbLookupBind(&g_udbtable, dst->sin_addr, dst->sin_port);
            if (inp == NULL) {
                return 0;
            }
        }
        inetUdp4Sendup(m, off, src, inp->inp_socket);
        return 1;
    }

    {
        InetInpcbReal *inp = g_udbtable.inpt_queue_first;
        while (inp != (InetInpcbReal *)&g_udbtable) {
            if (inp->inp_lport == dst->sin_port &&
                (inp->inp_laddr == 0 || inp->inp_laddr == dst->sin_addr) &&
                (inp->inp_faddr == 0 || (inp->inp_faddr == src->sin_addr && inp->inp_fport == src->sin_port))) {
                delivered++;
                inetUdp4Sendup(m, off, src, inp->inp_socket);
                if ((inp->inp_socket->so_options & (INET_SO_REUSEADDR | INET_SO_REUSEPORT)) == 0) {
                    return delivered;
                }
            }
            inp = inp->inp_queue_next;
        }
    }
    return delivered;
}

/* [321] 0x01D6BC */
static INET_NOINLINE int inetUdpOutput(InetMbuf *m, InetInpcbReal *inp)
{
    s32 payloadLen = M_PKTLEN(m);
    u32 totalLen;
    InetIpHdr *ip;
    InetUdpHdr *uh;

    m = p2pPrependMbuf(m, (s32)(sizeof(InetIpHdr) + sizeof(InetUdpHdr)));
    if (m == NULL) {
        sceNetIfhandle_lib_F56FAC82(m);
        return INET_ENOBUFS;
    }

    totalLen = (u32)(payloadLen + 28);
    if (totalLen > 0xFFFFU) {
        sceNetIfhandle_lib_F56FAC82(m);
        return INET_EMSGSIZE;
    }

    ip = (InetIpHdr *)m->m_data;
    uh = (InetUdpHdr *)(m->m_data + sizeof(InetIpHdr));
    sceNet_lib_8B615F52(ip, 9);
    ip->ip_p = INET_IPPROTO_UDP;
    ip->ip_sum = inetBswap16((u16)(payloadLen + 8));
    ip->ip_src = inp->inp_laddr;
    ip->ip_dst = inp->inp_faddr;
    uh->uh_sport = inp->inp_lport;
    uh->uh_dport = inp->inp_fport;
    uh->uh_ulen = ip->ip_sum;
    uh->uh_sum = 0;

    if (g_udpcksum != 0) {
        u16 sum = inetInCksum(m, (s32)totalLen);
        uh->uh_sum = (sum == 0) ? 0xFFFFU : sum;
    }

    ip->ip_len = (u16)totalLen;
    ip->ip_ttl = inp->inp_ip.ip_ttl;
    ip->ip_tos = inp->inp_ip.ip_tos;
    g_udpstat.udps_opackets++;

    return inetIpOutput(m, inp->inp_options, &inp->inp_route,
                        inp->inp_socket->so_options & (INET_SO_DONTROUTE | INET_SO_BROADCAST),
                        inp->inp_moptions, NULL);
}

/* [322] 0x01D900 */
INET_NOINLINE int inetUdpUsrreq(InetSocket *so, int req, InetMbuf *m, InetMbuf *nam, InetMbuf *control)
{
    int err = 0;
    int spl;
    InetInpcbReal *inp;

    if (req == INET_PRU_CONTROL) {
        return inetInControl(so, (u32)m, nam, (InetIfnet *)control);
    }
    if (req == INET_PRU_PURGEIF) {
        inetInPurgeIf((InetIfnet *)control);
        inetInPcbPurgeIf(&g_udbtable, (InetIfnet *)control);
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
                   (err = inetSoReserve(so, g_udp_sendspace, g_udp_recvspace)) == 0) {
            err = inetInPcbAlloc(so, &g_udbtable);
            if (err == 0) {
                ((InetInpcbReal *)so->so_pcb)->inp_ip.ip_ttl = (u8)g_ip_defttl;
            }
        }
        break;

    case INET_PRU_DETACH:
        inetInPcbDetach(inp);
        break;

    case INET_PRU_BIND:
        err = inetInPcbBind(inp, nam);
        break;

    case INET_PRU_LISTEN:
    case INET_PRU_RCVD:
    case INET_PRU_RCVOOB:
    case INET_PRU_PROTORCV:
        err = INET_EOPNOTSUPP;
        break;

    case INET_PRU_CONNECT:
        err = inetInPcbConnect(inp, nam);
        if (err == 0) {
            inetSoIsConnected(so);
        }
        break;

    case INET_PRU_DISCONNECT:
        so->so_state &= (u16)~INET_SS_ISCONNECTED;
        inetInPcbDisconnect(inp);
        inp->inp_laddr = g_zeroin_addr;
        inetInPcbState(inp, 1);
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
            u32 savedLaddr = inp->inp_laddr;
            int ok = 1;
            if (nam != NULL) {
                if ((so->so_state & INET_SS_ISCONNECTED) != 0) {
                    err = INET_EISCONN;
                    ok = 0;
                } else {
                    err = inetInPcbConnect(inp, nam);
                    if (err != 0) {
                        ok = 0;
                    }
                }
            } else if ((so->so_state & INET_SS_ISCONNECTED) == 0) {
                err = INET_ENOTCONN;
                ok = 0;
            }
            if (!ok) {
                sceNetIfhandle_lib_F56FAC82(m);
            } else {
                err = inetUdpOutput(m, inp);
                if (nam != NULL) {
                    inetInPcbDisconnect(inp);
                    inp->inp_laddr = savedLaddr;
                    inetInPcbState(inp, 1);
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

/* [323] 0x01DC2C */
INET_NOINLINE void inetUdpInit(void)
{
    g_udp_sendspace = 9216;
    g_udp_recvspace = 41600;
    g_udpcksum = 1;
    sceKernelMemset(&g_udpstat, 0, sizeof(g_udpstat));
    inetInPcbInit(&g_udbtable, (int)g_udbhashsize, (int)g_udbhashsize);
}

/* [324] 0x01DC90 */
INET_NOINLINE void inetUdpCtlinput(int cmd, InetSockaddr *sa, void *d)
{
    InetIpHdr *ip = (InetIpHdr *)d;
    InetSockaddrIn *sin = (InetSockaddrIn *)sa;

    if (sin->sin_family != INET_AF_INET || sin->sin_len != sizeof(InetSockaddrIn) || (u32)cmd >= 21U) {
        return;
    }

    if ((u32)(cmd - 14) < 4U || cmd == 6) {
        void (*notify)(InetInpcbReal *, int) = ((u32)(cmd - 14) < 4U) ? inetInRtChange : inetUdpNotify;
        inetInPcbNotifyAll(&g_udbtable, sin->sin_addr, g_inetctlerrmap[cmd], notify);
        return;
    }

    if (g_inetctlerrmap[cmd] != 0) {
        if (ip != NULL) {
            InetUdpHdr *uh = (InetUdpHdr *)((u8 *)ip + (ip->ip_vhl & 0x0F) * 4);
            inetInPcbNotify(&g_udbtable, sin->sin_addr, uh->uh_dport,
                            ip->ip_src, uh->uh_sport, g_inetctlerrmap[cmd], inetUdpNotify);
        } else {
            inetInPcbNotifyAll(&g_udbtable, sin->sin_addr, g_inetctlerrmap[cmd], inetUdpNotify);
        }
    }
}

/* [325] 0x01DD98 */
INET_NOINLINE int inetUdpSysctl(int *name, u32 namelen, void *oldp, u32 *oldlenp, void *newp, u32 newlen)
{
    if (namelen != 1) {
        return INET_ENOTDIR;
    }
    switch (name[0]) {
    case 1:
        return inetSysctlInt(oldp, oldlenp, newp, newlen, &g_udpcksum);
    case 2:
        return inetSysctlInt(oldp, oldlenp, newp, newlen, (int *)&g_udp_sendspace);
    case 3:
        return inetSysctlInt(oldp, oldlenp, newp, newlen, (int *)&g_udp_recvspace);
    case 4:
        return inetSysctlRdStruct(oldp, oldlenp, newp, &g_udpstat, sizeof(g_udpstat));
    default:
        return INET_ENOPROTOOPT;
    }
}

/* [326] 0x01DE6C */
static INET_NOINLINE void inetUdp4Sendup(InetMbuf *m, int off, InetSockaddrIn *src, InetSocket *so)
{
    InetMbuf *opts = NULL;
    InetMbuf *copyM;
    InetInpcbReal *inp;

    if (so == NULL || so->so_proto->pr_domain->dom_family != INET_AF_INET) {
        return;
    }
    inp = (InetInpcbReal *)so->so_pcb;
    copyM = sceNetIfhandle_lib_456E3146(m, 0, 1000000000, INET_M_DONTWAIT);
    if (copyM == NULL) {
        return;
    }

    if (inp != NULL && ((inp->inp_flags & 0x87) != 0 || (so->so_options & 0x400) != 0)) {
        inetIpSaveControl(inp, &opts, (InetIpHdr *)copyM->m_data, copyM);
    }

    sceNetIfhandle_lib_BFF3CEA5(copyM, off);
    if (inetSbAppendAddr(&so->so_rcv, (InetSockaddr *)src, copyM, opts) == 0) {
        sceNetIfhandle_lib_F56FAC82(copyM);
        if (opts != NULL) {
            sceNetIfhandle_lib_F56FAC82(opts);
        }
    } else if ((so->so_rcv.sb_flags & 0x3C) != 0) {
        inetSoWakeup(so, &so->so_rcv);
    }
}

/* [327] 0x01DFA0 */
static INET_NOINLINE void inetUdpNotify(InetInpcbReal *inp, int err)
{
    InetSocket *so = inp->inp_socket;
    u16 rcvFlags = so->so_rcv.sb_flags;
    so->so_error = (u16)err;
    if ((rcvFlags & 0x3C) != 0) {
        inetSoWakeup(so, &so->so_rcv);
        so = inp->inp_socket;
    }
    if ((so->so_snd.sb_flags & 0x3C) != 0) {
        inetSoWakeup(so, &so->so_snd);
    }
}

/* [328] 0x01E014 */
static INET_NOINLINE int inetUdpP2pInput(InetSockaddrIn *src, InetSockaddrIn *dst, InetMbuf *m, int iphlen)
{
    s32 off = iphlen + (s32)sizeof(InetUdpHdr);
    InetP2pKeyEntry *keyEnt;
    u16 dstPort = dst->sin_port;
    InetP2pInpcb *scan;
    u8 magic = 0;
    u8 protoByte = 0;
    u16 payloadLen = 0;
    u16 srcVport = 0;
    u16 dstVport = 0;
    u32 protoKind;

    if (src->sin_family != INET_AF_INET || dst->sin_family != INET_AF_INET) {
        return 0;
    }

    keyEnt = inetP2pFindKey(src->sin_addr, src->sin_port);
    if (dstPort == g_p2p_default_port || (keyEnt != NULL && keyEnt->lport == dstPort)) {
        dstPort = inetBswap16(0x0E4A);
    }

    scan = g_p2p_pcb_head;
    while (scan != NULL) {
        if (scan->base.inp_lport == dstPort &&
            (scan->base.inp_laddr == dst->sin_addr || scan->base.inp_laddr == g_zeroin_addr)) {
            break;
        }
        scan = scan->p2p_next;
    }
    if (scan == NULL) {
        return 0;
    }

    if ((u32)M_PKTLEN(m) < (u32)(off + 2)) {
        return 0;
    }
    sceNetIfhandle_lib_9A6261EC(m, off, 1, &magic);
    off++;
    if (magic != 0xFF) {
        return 0;
    }
    sceNetIfhandle_lib_9A6261EC(m, off, 1, &protoByte);
    off++;
    if ((protoByte & 0x80) != 0) {
        payloadLen = (u16)(M_PKTLEN(m) - off);
    } else {
        if ((u32)M_PKTLEN(m) < (u32)(off + 2)) {
            return 0;
        }
        sceNetIfhandle_lib_9A6261EC(m, off, 2, &payloadLen);
        off += 2;
        payloadLen = inetBswap16(payloadLen);
        if (M_PKTLEN(m) < off + (s32)payloadLen) {
            g_p2p_stat.p2ps_bad_len++;
            return 0;
        }
    }

    protoKind = (u32)(protoByte & 0x0F);
    if ((u32)(protoKind - 3) >= 4U || (protoKind != 3 && keyEnt == NULL)) {
        return 0;
    }

    if ((protoByte & 0x40) != 0) {
        u8 shortPort = 0;
        if (payloadLen < 2) {
            g_p2p_stat.p2ps_bad_vport++;
            return 0;
        }
        sceNetIfhandle_lib_9A6261EC(m, off, 1, &shortPort);
        off++;
        srcVport = inetBswap16((u16)shortPort);
        sceNetIfhandle_lib_9A6261EC(m, off, 1, &shortPort);
        off++;
        dstVport = inetBswap16((u16)shortPort);
        payloadLen -= 2;
    } else {
        if (payloadLen < 4) {
            g_p2p_stat.p2ps_bad_vport++;
            return 0;
        }
        sceNetIfhandle_lib_9A6261EC(m, off, 2, &srcVport);
        sceNetIfhandle_lib_9A6261EC(m, off + 2, 2, &dstVport);
        off += 4;
        payloadLen -= 4;
    }

    /* Exact match first */
    scan = g_p2p_pcb_head;
    while (scan != NULL) {
        if (scan->base.inp_faddr == src->sin_addr &&
            scan->base.inp_fport == src->sin_port &&
            scan->base.inp_lport == dstPort &&
            scan->base.inp_laddr == dst->sin_addr &&
            scan->p2p_fport == srcVport &&
            scan->p2p_lport == dstVport) {
            break;
        }
        scan = scan->p2p_next;
    }

    if (scan == NULL) {
        /* Fallback match by local address + local port + virtual port */
        scan = g_p2p_pcb_head;
        while (scan != NULL) {
            if (scan->base.inp_lport == dstPort &&
                (scan->base.inp_laddr == dst->sin_addr || scan->base.inp_laddr == g_zeroin_addr) &&
                (scan->p2p_lport == dstVport || dstVport == 0)) {
                break;
            }
            scan = scan->p2p_next;
        }
    }

    if (scan == NULL) {
        return 0;
    }

    /* Store source virtual port in sin_zero[0..1] */
    src->sin_zero[0] = (u8)(srcVport & 0xFF);
    src->sin_zero[1] = (u8)(srcVport >> 8);
    inetUdpP2pDeliver(m, off, payloadLen, src, scan->base.inp_socket, protoByte);
    g_p2p_stat.p2ps_udp_ipackets++;
    return 1;
}

/* [329] 0x01E4D0 */
static INET_NOINLINE int inetTcpP2pInput(InetSockaddrIn *src, InetSockaddrIn *dst, InetMbuf *m, int iphlen)
{
    s32 off = iphlen + (s32)sizeof(InetUdpHdr);
    u8 magic = 0;
    u8 protoByte = 0;
    u16 payloadLen = 0;

    if ((u32)M_PKTLEN(m) < (u32)(off + 2)) {
        return 0;
    }
    sceNetIfhandle_lib_9A6261EC(m, off, 1, &magic);
    if (magic != 0xFF) {
        return 0;
    }
    sceNetIfhandle_lib_9A6261EC(m, off + 1, 1, &protoByte);
    off += 2;
    if ((protoByte & 0x80) != 0) {
        payloadLen = (u16)(M_PKTLEN(m) - off);
    } else {
        if ((u32)M_PKTLEN(m) < (u32)(off + 2)) {
            return 0;
        }
        sceNetIfhandle_lib_9A6261EC(m, off, 2, &payloadLen);
        off += 2;
        payloadLen = inetBswap16(payloadLen);
        if (M_PKTLEN(m) < off + (s32)payloadLen) {
            g_p2p_stat.p2ps_bad_len++;
            return 0;
        }
    }

    if ((u32)((protoByte & 0x0F) - 7) < 4U) {
        int handled = inetTcpP2pDeliver(src, dst, m, off, payloadLen, protoByte);
        if (handled != 0) {
            g_p2p_stat.p2ps_tcp_packets++;
            return handled;
        }
    }
    return 0;
}

/* [330] 0x01E674 */
static INET_NOINLINE int inetUdpP2pOutput(InetMbuf *m, InetP2pInpcb *p2p)
{
    InetMbuf *curM = m;
    InetSocket *so = p2p->base.inp_socket;
    InetP2pKeyEntry *keyEnt;
    int err = 0;
    s32 encPayloadLen;
    u8 *p2pHdr;
    u8 protoByte;
    s32 udpPayloadLen;
    u32 totalLen;
    InetIpHdr *ip;
    InetUdpHdr *uh;

    if (so == NULL) {
        sceNetIfhandle_lib_F56FAC82(curM);
        return INET_EBADF;
    }

    keyEnt = inetP2pFindKey(p2p->base.inp_faddr, p2p->base.inp_fport);
    if ((so->so_options & 0x3000) != 0 && keyEnt == NULL) {
        sceNetIfhandle_lib_F56FAC82(curM);
        return 0;
    }

    err = inetP2pEncrypt(&curM, so->so_options & 0x1000, so->so_options & 0x2000, keyEnt);
    if (err != 0) {
        sceNetIfhandle_lib_F56FAC82(curM);
        return err;
    }

    encPayloadLen = M_PKTLEN(curM);
    curM = p2pPrependMbuf(curM, 8);
    if (curM == NULL) {
        return INET_ENOBUFS;
    }

    if ((u32)(M_PKTLEN(curM) + (s32)p2p->p2p_hdrlen + 28) > 0xFFFFU) {
        sceNetIfhandle_lib_F56FAC82(curM);
        return INET_EMSGSIZE;
    }

    p2pHdr = curM->m_data;
    p2pHdr[0] = 0xFF;
    if ((so->so_options & 0x3000) == 0x3000) {
        protoByte = 6;
    } else if ((so->so_options & 0x1000) != 0) {
        protoByte = 4;
    } else if ((so->so_options & 0x2000) != 0) {
        protoByte = 5;
    } else {
        protoByte = 3;
    }
    p2pHdr[1] = protoByte;
    {
        u16 netLen = inetBswap16((u16)(encPayloadLen + 4));
        p2pHdr[2] = (u8)(netLen & 0xFF);
        p2pHdr[3] = (u8)(netLen >> 8);
    }
    p2pHdr[4] = (u8)(p2p->p2p_lport & 0xFF);
    p2pHdr[5] = (u8)(p2p->p2p_lport >> 8);
    p2pHdr[6] = (u8)(p2p->p2p_fport & 0xFF);
    p2pHdr[7] = (u8)(p2p->p2p_fport >> 8);

    udpPayloadLen = M_PKTLEN(curM);
    curM = p2pPrependMbuf(curM, 28);
    if (curM == NULL) {
        return INET_ENOBUFS;
    }

    ip = (InetIpHdr *)curM->m_data;
    uh = (InetUdpHdr *)(curM->m_data + sizeof(InetIpHdr));
    sceNet_lib_8B615F52(ip, 9);
    ip->ip_p = INET_IPPROTO_UDP;
    ip->ip_src = p2p->base.inp_laddr;
    ip->ip_dst = p2p->base.inp_faddr;
    uh->uh_sport = p2p->base.inp_lport;
    uh->uh_dport = p2p->base.inp_fport;
    if (uh->uh_sport == inetBswap16(0x0E4A)) {
        if (keyEnt != NULL && keyEnt->lport != 0) {
            uh->uh_sport = keyEnt->lport;
        } else {
            uh->uh_sport = g_p2p_default_port;
        }
    }

    totalLen = (u32)(udpPayloadLen + 28);
    ip->ip_sum = inetBswap16((u16)(udpPayloadLen + 8));
    uh->uh_ulen = ip->ip_sum;
    uh->uh_sum = 0;

    if (g_udpcksum != 0) {
        u16 sum = inetInCksum(curM, (s32)totalLen);
        uh->uh_sum = (sum == 0) ? 0xFFFFU : sum;
    }

    ip->ip_len = (u16)totalLen;
    ip->ip_ttl = p2p->base.inp_ip.ip_ttl;
    ip->ip_tos = p2p->base.inp_ip.ip_tos;
    g_p2p_stat.p2ps_udp_opackets++;

    return inetIpOutput(curM, p2p->base.inp_options, &p2p->base.inp_route,
                        so->so_options & (INET_SO_DONTROUTE | INET_SO_BROADCAST),
                        p2p->base.inp_moptions, NULL);
}

/* [331] 0x01EACC */
INET_NOINLINE int inetTcpP2pOutput(InetSockaddrIn *src, InetSockaddrIn *dst, InetMbuf *m,
                                   InetRoute *ro, InetSocket *so, int doEnc, int doAuth)
{
    InetMbuf *curM = m;
    InetP2pKeyEntry *keyEnt;
    int err;
    s32 encPayloadLen;
    u8 *hdr;
    u8 protoByte;
    s32 udpPayloadLen;
    u32 totalLen;
    InetIpHdr *ip;
    InetUdpHdr *uh;

    (void)so;
    keyEnt = inetP2pFindKey(dst->sin_addr, dst->sin_port);
    if ((doEnc | doAuth) != 0 && keyEnt == NULL) {
        sceNetIfhandle_lib_F56FAC82(curM);
        return 0;
    }

    err = inetP2pEncrypt(&curM, doEnc, doAuth, keyEnt);
    if (err != 0) {
        sceNetIfhandle_lib_F56FAC82(curM);
        return err;
    }

    encPayloadLen = M_PKTLEN(curM);
    curM = p2pPrependMbuf(curM, 32);
    if (curM == NULL) {
        return INET_ENOBUFS;
    }

    hdr = curM->m_data;
    hdr[28] = 0xFF;
    if (doEnc != 0 && doAuth != 0) {
        protoByte = 10;
    } else if (doEnc != 0) {
        protoByte = 8;
    } else if (doAuth != 0) {
        protoByte = 9;
    } else {
        protoByte = 7;
    }
    hdr[29] = protoByte;
    {
        u16 netLen = inetBswap16((u16)encPayloadLen);
        hdr[30] = (u8)(netLen & 0xFF);
        hdr[31] = (u8)(netLen >> 8);
    }

    udpPayloadLen = encPayloadLen + 4;
    ip = (InetIpHdr *)curM->m_data;
    uh = (InetUdpHdr *)(curM->m_data + sizeof(InetIpHdr));
    sceNet_lib_8B615F52(ip, 9);
    ip->ip_p = INET_IPPROTO_UDP;
    ip->ip_src = src->sin_addr;
    ip->ip_dst = dst->sin_addr;
    uh->uh_sport = src->sin_port;
    uh->uh_dport = dst->sin_port;
    if (uh->uh_sport == inetBswap16(0x0E4A)) {
        if (keyEnt != NULL && keyEnt->lport != 0) {
            uh->uh_sport = keyEnt->lport;
        } else {
            uh->uh_sport = g_p2p_default_port;
        }
    }

    totalLen = (u32)(udpPayloadLen + 28);
    ip->ip_sum = inetBswap16((u16)(udpPayloadLen + 8));
    uh->uh_ulen = ip->ip_sum;
    uh->uh_sum = 0;

    if (g_udpcksum != 0) {
        u16 sum = inetInCksum(curM, (s32)totalLen);
        uh->uh_sum = (sum == 0) ? 0xFFFFU : sum;
    }

    ip->ip_len = (u16)totalLen;
    ip->ip_tos = 0;
    ip->ip_ttl = (u8)g_ip_defttl;
    g_p2p_stat.p2ps_tcp_packets++;

    return inetIpOutput(curM, NULL, ro, 0, NULL, NULL);
}

/* [332] 0x01EE2C */
INET_NOINLINE int inetUdpP2pUsrreq(InetSocket *so, int req, InetMbuf *m, InetMbuf *nam, InetMbuf *control)
{
    int err = 0;
    int spl;
    InetP2pInpcb *p2p;

    if (req == INET_PRU_CONTROL) {
        u32 cmd = (u32)m;
        if (cmd == 0x200050C8U || cmd == 0x802450C9U || cmd == 0xC02450CAU ||
            cmd == 0x802450CBU || cmd == 0x802450CCU || cmd == 0x800250CFU ||
            cmd == 0xC00250D0U) {
            return inetP2pIoctl(so, cmd, nam, (InetIfnet *)control);
        }
        return inetInControl(so, cmd, nam, (InetIfnet *)control);
    }

    spl = sceNet_lib_94B44F26();
    p2p = (InetP2pInpcb *)so->so_pcb;
    if (p2p == NULL && req != INET_PRU_ATTACH) {
        sceNet_lib_515B2F33(spl);
        return INET_EINVAL;
    }

    switch (req) {
    case INET_PRU_ATTACH:
        if (p2p != NULL) {
            err = INET_EISCONN;
        } else if ((so->so_snd.sb_hiwat != 0 && so->so_rcv.sb_hiwat != 0) ||
                   (err = inetSoReserve(so, g_udp_p2p_sendspace, g_udp_p2p_recvspace)) == 0) {
            p2p = (InetP2pInpcb *)sceNetIfhandle_lib_15CFE3C0(sizeof(InetP2pInpcb));
            if (p2p == NULL) {
                err = INET_ENOBUFS;
            } else {
                int listSpl;
                sceKernelMemset(p2p, 0, sizeof(InetP2pInpcb));
                p2p->base.inp_errormtu = -1;
                p2p->base.inp_socket = so;
                so->so_pcb = p2p;

                listSpl = sceNet_lib_94B44F26();
                p2p->p2p_next = g_p2p_pcb_head;
                if (g_p2p_pcb_head != NULL) {
                    g_p2p_pcb_head->p2p_prev = &p2p->p2p_next;
                }
                p2p->p2p_prev = &g_p2p_pcb_head;
                p2p->base.inp_state = 0;
                g_p2p_pcb_head = p2p;
                sceNet_lib_515B2F33(listSpl);

                p2p->base.inp_ip.ip_ttl = (u8)g_ip_defttl;
                err = 0;
            }
        }
        break;

    case INET_PRU_DETACH:
        p2pDetachPcb(p2p);
        break;

    case INET_PRU_BIND:
        err = inetUdpP2pBind(p2p, nam);
        break;

    case INET_PRU_LISTEN:
    case INET_PRU_RCVD:
    case INET_PRU_RCVOOB:
    case INET_PRU_PROTORCV:
        err = INET_EOPNOTSUPP;
        break;

    case INET_PRU_CONNECT:
        err = inetUdpP2pConnect(p2p, nam);
        if (err == 0) {
            inetSoIsConnected(so);
        }
        break;

    case INET_PRU_DISCONNECT:
        so->so_state &= (u16)~INET_SS_ISCONNECTED;
        p2p->base.inp_fport = 0;
        p2p->p2p_fport = 0;
        p2p->base.inp_faddr = g_zeroin_addr;
        p2p->base.inp_state = 1;
        if ((p2p->base.inp_socket->so_state & INET_SS_NOFDREF) != 0) {
            p2pDetachPcb(p2p);
        } else {
            p2p->base.inp_laddr = g_zeroin_addr;
            p2p->base.inp_state = 1;
        }
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
            u32 savedLaddr = p2p->base.inp_laddr;
            int ok = 1;
            if (nam != NULL) {
                if ((so->so_state & INET_SS_ISCONNECTED) != 0) {
                    err = INET_EISCONN;
                    ok = 0;
                } else {
                    err = inetUdpP2pConnect(p2p, nam);
                    if (err != 0) {
                        ok = 0;
                    }
                }
            } else if ((so->so_state & INET_SS_ISCONNECTED) == 0) {
                err = INET_ENOTCONN;
                ok = 0;
            }
            if (!ok) {
                sceNetIfhandle_lib_F56FAC82(m);
            } else {
                err = inetUdpP2pOutput(m, p2p);
                if (nam != NULL) {
                    p2p->base.inp_fport = 0;
                    p2p->p2p_fport = 0;
                    p2p->base.inp_faddr = g_zeroin_addr;
                    p2p->base.inp_state = 1;
                    if ((p2p->base.inp_socket->so_state & INET_SS_NOFDREF) != 0) {
                        p2pDetachPcb(p2p);
                    } else {
                        p2p->base.inp_state = 1;
                        p2p->base.inp_laddr = savedLaddr;
                    }
                }
            }
        }
        break;

    case INET_PRU_SENSE:
        sceNet_lib_515B2F33(spl);
        return 0;

    case INET_PRU_SENDOOB:
        sceNetIfhandle_lib_F56FAC82(control);
        sceNetIfhandle_lib_F56FAC82(m);
        err = INET_EOPNOTSUPP;
        break;

    case INET_PRU_SOCKADDR: {
        InetSockaddrIn *sin = (InetSockaddrIn *)nam->m_data;
        nam->m_len = sizeof(InetSockaddrIn);
        sceKernelMemset(sin, 0, sizeof(InetSockaddrIn));
        sin->sin_len = sizeof(InetSockaddrIn);
        sin->sin_family = INET_AF_INET;
        sin->sin_port = p2p->base.inp_lport;
        sin->sin_addr = p2p->base.inp_laddr;
        sin->sin_zero[0] = (u8)(p2p->p2p_lport & 0xFF);
        sin->sin_zero[1] = (u8)(p2p->p2p_lport >> 8);
        break;
    }

    case INET_PRU_PEERADDR: {
        InetSockaddrIn *sin = (InetSockaddrIn *)nam->m_data;
        nam->m_len = sizeof(InetSockaddrIn);
        sceKernelMemset(sin, 0, sizeof(InetSockaddrIn));
        sin->sin_len = sizeof(InetSockaddrIn);
        sin->sin_family = INET_AF_INET;
        sin->sin_port = p2p->base.inp_fport;
        sin->sin_addr = p2p->base.inp_faddr;
        sin->sin_zero[0] = (u8)(p2p->p2p_fport & 0xFF);
        sin->sin_zero[1] = (u8)(p2p->p2p_fport >> 8);
        break;
    }

    default:
        break;
    }

    sceNet_lib_515B2F33(spl);
    return err;
}

/* [333] 0x01F448 */
static INET_NOINLINE int inetP2pIoctl(InetSocket *so, u32 cmd, void *data, InetIfnet *ifp)
{
    int err = 0;
    int spl = sceNet_lib_94B44F26();

    (void)so;
    (void)ifp;
    switch (cmd) {
    case 0x200050C8U:
        inetP2pFlushKeys();
        break;
    case 0x802450C9U: {
        InetP2pKeyIoctl *req = (InetP2pKeyIoctl *)data;
        InetP2pKeyIoctl loopbackReq;
        sceKernelMemcpy(g_p2p_local_key, req->key, 16);
        sceKernelMemset(&loopbackReq, 0, sizeof(loopbackReq));
        loopbackReq.addr = inetBswap32(0x7F000001U);
        loopbackReq.port = inetBswap16(0x0E4A);
        sceKernelMemcpy(loopbackReq.key, req->key, 16);
        inetP2pAddKey(&loopbackReq);
        g_p2p_local_key_set = 1;
        break;
    }
    case 0xC02450CAU: {
        InetP2pKeyIoctl *req = (InetP2pKeyIoctl *)data;
        sceKernelMemcpy(req->key, g_p2p_local_key, 16);
        break;
    }
    case 0x802450CBU:
        err = inetP2pAddKey((const InetP2pKeyIoctl *)data);
        break;
    case 0x802450CCU:
        err = inetP2pDelKey((const InetP2pKeyIoctl *)data);
        break;
    case 0x800250CFU:
        sceKernelMemcpy(&g_p2p_default_port, data, 2);
        break;
    case 0xC00250D0U:
        sceKernelMemcpy(data, &g_p2p_default_port, 2);
        break;
    default:
        break;
    }

    sceNet_lib_515B2F33(spl);
    return err;
}

/* [334] 0x01F5F0 */
static INET_NOINLINE void inetP2pFlushKeys(void)
{
    g_p2p_local_key_set = 0;
    sceKernelMemset(g_p2p_local_key, 0, sizeof(g_p2p_local_key));
    while (g_p2p_key_head != NULL) {
        InetP2pKeyEntry *ent = g_p2p_key_head;
        InetP2pKeyEntry *next = ent->next;
        if (next != NULL) {
            next->prev = ent->prev;
        }
        *ent->prev = next;
        inetTcpP2pDropPeer((InetSockaddrIn *)&ent->sin_len_family);
        sceNetIfhandle_lib_76BAD213(ent);
    }
    g_p2p_key_head = NULL;
}

/* [335] 0x01F678 */
INET_NOINLINE InetP2pKeyEntry *inetP2pFindKey(u32 addr, u16 port)
{
    InetP2pKeyEntry *ent;

    if (inetP2pIsLocalAddr(addr)) {
        u32 loopback = inetBswap32(0x7F000001U);
        ent = g_p2p_key_head;
        while (ent != NULL) {
            if (ent->peer_addr == loopback && ent->peer_port == port) {
                return ent;
            }
            ent = ent->next;
        }
    }

    ent = g_p2p_key_head;
    while (ent != NULL) {
        if (ent->peer_addr == addr && ent->peer_port == port) {
            return ent;
        }
        ent = ent->next;
    }
    return NULL;
}

/* [336] 0x01F730 */
static INET_NOINLINE int inetP2pAddKey(const InetP2pKeyIoctl *req)
{
    InetP2pKeyEntry *ent = inetP2pFindKey(req->addr, req->port);
    if (ent != NULL) {
        if (sceNet_lib_E0A81C7C(ent->aes_key, req->key, 16) != 0) {
            sceKernelMemcpy(ent->aes_key, req->key, 16);
            ent->refcnt = 1;
        } else {
            ent->refcnt++;
        }
        return 0;
    }

    ent = (InetP2pKeyEntry *)sceNetIfhandle_lib_15CFE3C0(sizeof(InetP2pKeyEntry));
    if (ent == NULL) {
        return INET_ENOMEM;
    }
    sceKernelMemset(ent, 0, sizeof(InetP2pKeyEntry));
    ent->peer_port = req->port;
    ent->peer_addr = req->addr;
    ent->lport = req->lport;
    sceKernelMemcpy(ent->aes_key, req->key, 16);
    ent->refcnt = 1;

    ent->next = g_p2p_key_head;
    if (g_p2p_key_head != NULL) {
        g_p2p_key_head->prev = &ent->next;
    }
    ent->prev = &g_p2p_key_head;
    g_p2p_key_head = ent;
    return 0;
}

/* [337] 0x01F854 */
static INET_NOINLINE int inetP2pDelKey(const InetP2pKeyIoctl *req)
{
    InetP2pKeyEntry *ent = inetP2pFindKey(req->addr, req->port);
    if (ent == NULL) {
        return INET_ENOENT;
    }
    if (sceNet_lib_E0A81C7C(ent->aes_key, req->key, 16) == 0) {
        ent->refcnt--;
        if (ent->refcnt <= 0) {
            if (ent->next != NULL) {
                ent->next->prev = ent->prev;
            }
            *ent->prev = ent->next;
            inetTcpP2pDropPeer((InetSockaddrIn *)&ent->sin_len_family);
            sceNetIfhandle_lib_76BAD213(ent);
        }
    }
    return 0;
}

/* [338] 0x01F900 */
static INET_NOINLINE int inetP2pEncrypt(InetMbuf **mp, int doEnc, int doAuth, InetP2pKeyEntry *keyEnt)
{
    InetMbuf *m = *mp;
    u32 prefixLen = 0;
    u32 iv[4];
    u8 hmacCtx[176];
    u8 hmacDigest[32];
    int err = 0;

    if (doEnc != 0) {
        s32 pktLen = M_PKTLEN(m);
        if (pktLen != m->m_len || pktLen > 1600) {
            InetMbuf *newHead = NULL;
            InetMbuf *newTail = NULL;
            s32 copied = 0;
            while (copied < pktLen) {
                s32 chunk = pktLen - copied;
                InetMbuf *nm;
                if (chunk > 1600) {
                    chunk = 1600;
                }
                nm = (InetMbuf *)sceNetIfhandle_lib_15CFE3C0(sizeof(InetMbuf));
                if (nm == NULL) {
                    if (newHead != NULL) {
                        sceNetIfhandle_lib_F56FAC82(newHead);
                    }
                    *mp = m;
                    return INET_ENOBUFS;
                }
                if (newHead == NULL) {
                    nm->m_next = NULL;
                    nm->m_nextpkt = NULL;
                    nm->m_data = M_PKTDAT(nm);
                    nm->m_type = INET_MT_DATA;
                    nm->m_flags = (u16)(m->m_flags & ~(INET_M_EXT | 8));
                    nm->m_pkthdr = m->m_pkthdr;
                    if (chunk > 208 && !p2pAllocCluster(nm)) {
                        sceNetIfhandle_lib_F8825DC4(nm);
                        *mp = m;
                        return INET_ENOBUFS;
                    }
                } else {
                    nm->m_next = NULL;
                    nm->m_nextpkt = NULL;
                    nm->m_data = M_DAT(nm);
                    nm->m_type = INET_MT_DATA;
                    nm->m_flags = 0;
                    if (chunk > 236 && !p2pAllocCluster(nm)) {
                        sceNetIfhandle_lib_F8825DC4(nm);
                        sceNetIfhandle_lib_F56FAC82(newHead);
                        *mp = m;
                        return INET_ENOBUFS;
                    }
                }
                sceNetIfhandle_lib_9A6261EC(m, copied, chunk, nm->m_data);
                nm->m_len = chunk;
                if (newHead == NULL) {
                    newHead = nm;
                } else {
                    newTail->m_next = nm;
                }
                newTail = nm;
                copied += chunk;
            }
            sceNetIfhandle_lib_F56FAC82(m);
            m = newHead;
        }

        prefixLen = 8;
        iv[0] = sceNet_lib_1858883D();
        iv[1] = sceNet_lib_1858883D();
        iv[2] = iv[0];
        iv[3] = iv[1];
        {
            InetMbuf *seg = m;
            while (seg != NULL) {
                err = rijndaelCbcEncrypt(seg->m_data, seg->m_data, (u32)seg->m_len,
                                         keyEnt->aes_key, 128, (const u8 *)iv);
                if (err != 0) {
                    *mp = m;
                    return err;
                }
                seg = seg->m_next;
            }
        }
    }

    if (doAuth != 0) {
        InetMbuf *seg = m;
        sceKernelMemset(hmacDigest, 0, 20);
        sceNet_lib_8D34E686(hmacCtx, keyEnt->aes_key, 16);
        while (seg != NULL) {
            sceNet_lib_3A0668D5(hmacCtx, seg->m_data, (u32)seg->m_len);
            seg = seg->m_next;
        }
        sceNet_lib_4A3E75C1(hmacDigest, hmacCtx);
        prefixLen += 8;
    }

    if (prefixLen != 0) {
        m = p2pPrependMbuf(m, (s32)prefixLen);
        if (m == NULL) {
            *mp = NULL;
            return INET_ENOBUFS;
        }
        if (doEnc != 0 && doAuth != 0) {
            sceKernelMemcpy(m->m_data, iv, 8);
            sceKernelMemcpy(m->m_data + 8, hmacDigest, 8);
        } else if (doEnc != 0) {
            sceKernelMemcpy(m->m_data, iv, 8);
        } else {
            sceKernelMemcpy(m->m_data, hmacDigest, 8);
        }
    }

    *mp = m;
    return 0;
}

/* [339] 0x01FD7C */
INET_NOINLINE InetMbuf *inetP2pDecrypt(InetMbuf *m, s32 off, u32 len, u8 protoByte)
{
    u32 kind = (u32)(protoByte & 0x0F);
    int hasEnc = (kind == 4 || kind == 6 || kind == 8 || kind == 10);
    int hasAuth = (kind == 5 || kind == 6 || kind == 9 || kind == 10);
    u32 tagLen = ((protoByte & 0x80) != 0) ? 4U : 8U;
    u8 iv[16];
    u8 hmacCtx[176];
    u8 recvMac[16];
    u8 calcMac[32];

    if (hasEnc) {
        u32 pos;
        if (len < tagLen) {
            return NULL;
        }
        for (pos = 0; pos < 16; pos += tagLen) {
            sceNetIfhandle_lib_9A6261EC(m, off, (s32)tagLen, iv + pos);
        }
        off += (s32)tagLen;
        len -= tagLen;
    }

    if (hasAuth) {
        if (len < tagLen) {
            return NULL;
        }
        sceKernelMemset(recvMac, 0, 8);
        sceNetIfhandle_lib_9A6261EC(m, off, (s32)tagLen, recvMac);
        off += (s32)tagLen;
        len -= tagLen;
        if (!g_p2p_local_key_set) {
            return NULL;
        }
        sceKernelMemset(calcMac, 0, 20);
        sceNet_lib_8D34E686(hmacCtx, g_p2p_local_key, 16);
        {
            InetMbuf *seg = m;
            s32 curOff = off;
            s32 rem = (s32)len;
            while (seg != NULL && rem > 0) {
                if (curOff < seg->m_len) {
                    s32 take = seg->m_len - curOff;
                    if (take > rem) {
                        take = rem;
                    }
                    sceNet_lib_3A0668D5(hmacCtx, seg->m_data + curOff, (u32)take);
                    rem -= take;
                    curOff = 0;
                } else {
                    curOff -= seg->m_len;
                }
                seg = seg->m_next;
            }
        }
        sceNet_lib_4A3E75C1(calcMac, hmacCtx);
        if (sceNet_lib_E0A81C7C(calcMac, recvMac, tagLen) != 0) {
            return NULL;
        }
    }

    if (!hasEnc) {
        return sceNetIfhandle_lib_E9BF5332(m, off, (s32)len, INET_M_DONTWAIT);
    }

    if (!g_p2p_local_key_set) {
        return NULL;
    }

    {
        InetMbuf *outHead = NULL;
        InetMbuf *outTail = NULL;
        s32 copied = 0;
        while (copied < (s32)len) {
            s32 chunk = (s32)len - copied;
            InetMbuf *nm;
            if (chunk > 1600) {
                chunk = 1600;
            }
            nm = (InetMbuf *)sceNetIfhandle_lib_15CFE3C0(sizeof(InetMbuf));
            if (nm == NULL) {
                if (outHead != NULL) {
                    sceNetIfhandle_lib_F56FAC82(outHead);
                }
                return NULL;
            }
            if (outHead == NULL) {
                nm->m_next = NULL;
                nm->m_nextpkt = NULL;
                nm->m_data = M_PKTDAT(nm);
                nm->m_type = INET_MT_DATA;
                nm->m_flags = (u16)(m->m_flags & ~(INET_M_EXT | 8));
                nm->m_pkthdr = m->m_pkthdr;
                if (chunk > 208 && !p2pAllocCluster(nm)) {
                    sceNetIfhandle_lib_F8825DC4(nm);
                    return NULL;
                }
                M_PKTLEN(nm) = (s32)len;
            } else {
                nm->m_next = NULL;
                nm->m_nextpkt = NULL;
                nm->m_data = M_DAT(nm);
                nm->m_type = INET_MT_DATA;
                nm->m_flags = 0;
                if (chunk > 236 && !p2pAllocCluster(nm)) {
                    sceNetIfhandle_lib_F8825DC4(nm);
                    sceNetIfhandle_lib_F56FAC82(outHead);
                    return NULL;
                }
            }
            sceNetIfhandle_lib_9A6261EC(m, off + copied, chunk, nm->m_data);
            nm->m_len = chunk;
            if (rijndaelCbcDecrypt(nm->m_data, nm->m_data, (u32)chunk,
                                   g_p2p_local_key, 128, iv) != 0) {
                sceNetIfhandle_lib_F8825DC4(nm);
                if (outHead != NULL) {
                    sceNetIfhandle_lib_F56FAC82(outHead);
                }
                return NULL;
            }
            if (outHead == NULL) {
                outHead = nm;
            } else {
                outTail->m_next = nm;
            }
            outTail = nm;
            copied += chunk;
        }
        return outHead;
    }
}

/* [340] 0x02022C */
static INET_NOINLINE int inetP2pIsLocalAddr(u32 addr)
{
    InetInIfaddr *ia = g_in_ifaddr_head;
    while (ia != NULL) {
        if (ia->ia_addr.sin_addr == addr) {
            return 1;
        }
        ia = ia->ia_next;
    }
    return 0;
}

/* [341] 0x02026C */
INET_NOINLINE void inetUdpP2pInit(void)
{
    g_p2p_default_port = inetBswap16(0x0E4A);
    g_p2p_pcb_head = NULL;
    g_p2p_ephemeral_port = (u16)((sceNet_lib_1858883D() % 0x1C40U) + 0x8000U);
    sceKernelMemset(&g_p2p_stat, 0, sizeof(g_p2p_stat));
    g_p2p_key_head = NULL;
    sceKernelMemset(g_p2p_reserved_buf, 0, sizeof(g_p2p_reserved_buf));
}

/* [342] 0x020300 */
static INET_NOINLINE int inetUdpP2pBind(InetP2pInpcb *p2p, InetMbuf *nam)
{
    u16 soOpts = p2p->base.inp_socket->so_options;
    int reusePort = soOpts & INET_SO_REUSEPORT;
    int wildCheck = (soOpts & (INET_SO_REUSEADDR | INET_SO_REUSEPORT)) == 0;
    InetSockaddrIn *sin;
    u16 lport;
    u16 lvport;
    u32 laddr;

    if (p2p->base.inp_lport != 0 || p2p->base.inp_laddr != 0 || p2p->p2p_lport != 0) {
        return INET_EINVAL;
    }
    if (nam == NULL || nam->m_len != sizeof(InetSockaddrIn)) {
        return INET_EINVAL;
    }
    sin = (InetSockaddrIn *)nam->m_data;
    if (sin->sin_family != INET_AF_INET) {
        return INET_EAFNOSUPPORT;
    }
    lport = sin->sin_port;
    if (lport == 0) {
        return INET_EINVAL;
    }
    laddr = sin->sin_addr;
    lvport = (u16)sin->sin_zero[0] | ((u16)sin->sin_zero[1] << 8);
    if ((laddr & inetBswap32(0xF0000000U)) == inetBswap32(0xE0000000U)) {
        return INET_EADDRNOTAVAIL;
    }

    if (laddr != 0) {
        InetInIfaddr *ia = g_in_ifaddr_hashtbl[laddr % 509U];
        int found = 0;
        sin->sin_port = 0;
        sin->sin_zero[0] = 0;
        sin->sin_zero[1] = 0;
        while (ia != NULL) {
            if (ia->ia_addr.sin_addr == laddr && (ia->ia_ifa.ifa_ifp->if_flags & INET_IFF_UP) != 0) {
                found = 1;
                break;
            }
            ia = ia->ia_hash_next;
        }
        if (!found && inetIfaWithAddr((InetSockaddr *)sin) == NULL) {
            return INET_EADDRNOTAVAIL;
        }
    }

    if (lvport == 0) {
        u16 cand = (u16)(g_p2p_ephemeral_port - 1);
        int tries = 7232;
        u16 netPort = 0;
        while (tries > 0) {
            if (cand < 0x8000 || cand > 0x9C3F) {
                cand = 0x9C3F;
            }
            netPort = inetBswap16(cand);
            tries--;
            if (inetUdpP2pLookup(laddr, lport, netPort, 1) == NULL) {
                g_p2p_ephemeral_port = cand;
                lvport = netPort;
                p2p->base.inp_flags |= 0x40;
                break;
            }
            cand--;
            if (tries == 0) {
                return INET_EAGAIN;
            }
        }
    } else {
        InetP2pInpcb *existing = inetUdpP2pLookup(laddr, lport, lvport, wildCheck);
        if (existing != NULL && (reusePort & existing->base.inp_socket->so_options) == 0) {
            return INET_EADDRINUSE;
        }
    }

    p2p->base.inp_state = 1;
    p2p->base.inp_laddr = laddr;
    p2p->base.inp_lport = lport;
    p2p->p2p_lport = lvport;
    return 0;
}

/* [343] 0x020590 */
static INET_NOINLINE int inetUdpP2pConnect(InetP2pInpcb *p2p, InetMbuf *nam)
{
    InetSockaddrIn *sin = (InetSockaddrIn *)nam->m_data;
    InetSockaddrIn *ifaddrSin = NULL;
    u16 fport;
    u16 fvport;
    u32 effLaddr;
    InetP2pInpcb *scan;

    if (nam->m_len != sizeof(InetSockaddrIn)) {
        return INET_EINVAL;
    }
    if (sin->sin_family != INET_AF_INET) {
        return INET_EAFNOSUPPORT;
    }
    if (sin->sin_addr == 0 ||
        (sin->sin_addr & inetBswap32(0xF0000000U)) == inetBswap32(0xE0000000U)) {
        return INET_EADDRNOTAVAIL;
    }
    fport = sin->sin_port;
    fvport = (u16)sin->sin_zero[0] | ((u16)sin->sin_zero[1] << 8);
    if (fport == 0 || fvport == 0 || p2p->base.inp_lport == 0 || p2p->p2p_lport == 0) {
        return INET_EADDRNOTAVAIL;
    }

    if (p2p->base.inp_laddr == 0) {
        int selErr = 0;
        InetInIfaddr *ia;
        int found = 0;
        ifaddrSin = inetInSelectSrc(sin, &p2p->base.inp_route,
                                    p2p->base.inp_socket->so_options,
                                    p2p->base.inp_moptions, &selErr);
        if (ifaddrSin == NULL) {
            return (selErr != 0) ? selErr : INET_EADDRNOTAVAIL;
        }
        ia = g_in_ifaddr_hashtbl[ifaddrSin->sin_addr % 509U];
        while (ia != NULL) {
            if (ia->ia_addr.sin_addr == ifaddrSin->sin_addr &&
                (ia->ia_ifa.ifa_ifp->if_flags & INET_IFF_UP) != 0) {
                found = 1;
                break;
            }
            ia = ia->ia_hash_next;
        }
        if (!found) {
            return INET_EADDRNOTAVAIL;
        }
        effLaddr = ifaddrSin->sin_addr;
    } else {
        effLaddr = p2p->base.inp_laddr;
    }

    scan = g_p2p_pcb_head;
    while (scan != NULL) {
        if (scan->base.inp_faddr == sin->sin_addr &&
            scan->base.inp_fport == fport &&
            scan->base.inp_lport == p2p->base.inp_lport &&
            scan->base.inp_laddr == effLaddr &&
            scan->p2p_fport == fvport &&
            scan->p2p_lport == p2p->p2p_lport) {
            return INET_EADDRINUSE;
        }
        scan = scan->p2p_next;
    }

    if (p2p->base.inp_laddr == 0) {
        p2p->base.inp_laddr = ifaddrSin->sin_addr;
    }
    p2p->base.inp_fport = fport;
    p2p->base.inp_faddr = sin->sin_addr;
    p2p->base.inp_state = 2;
    p2p->p2p_fport = fvport;
    return 0;
}

/* [344] 0x02082C */
static INET_NOINLINE void inetUdpP2pDeliver(InetMbuf *m, s32 off, u32 len, InetSockaddrIn *src,
                                            InetSocket *so, u8 protoByte)
{
    InetMbuf *decM = inetP2pDecrypt(m, off, len, protoByte);
    u32 kind = (u32)(protoByte & 0x0F);

    if (decM == NULL) {
        return;
    }
    if (so == NULL ||
        ((so->so_options & 0x1000) != 0 && kind != 6 && kind != 4) ||
        ((so->so_options & 0x2000) != 0 && kind != 6 && kind != 5)) {
        sceNetIfhandle_lib_F56FAC82(decM);
        return;
    }

    if (inetSbAppendAddr(&so->so_rcv, (InetSockaddr *)src, decM, NULL) == 0) {
        g_p2p_stat.p2ps_fullsock++;
        sceNetIfhandle_lib_F56FAC82(decM);
    } else if ((so->so_rcv.sb_flags & 0x3C) != 0) {
        inetSoWakeup(so, &so->so_rcv);
    }
}

/* [345] 0x020944 */
static INET_NOINLINE InetP2pInpcb *inetUdpP2pLookup(u32 laddr, u16 lport, u16 lvport, int wild)
{
    InetP2pInpcb *cur = g_p2p_pcb_head;
    InetP2pInpcb *best = NULL;
    int bestWild = 3;

    while (cur != NULL) {
        if (cur->base.inp_lport == lport && cur->p2p_lport == lvport) {
            int curWild = (cur->base.inp_faddr != 0) ? 1 : 0;
            int addrMismatch = 0;

            if (cur->base.inp_laddr != 0) {
                if (laddr == 0) {
                    curWild++;
                } else if (cur->base.inp_laddr != laddr) {
                    addrMismatch = 1;
                }
            } else if (laddr != 0) {
                curWild++;
            }

            if (!addrMismatch && (curWild == 0 || wild != 0) && curWild < bestWild) {
                best = cur;
                bestWild = curWild;
                if (curWild == 0) {
                    return cur;
                }
            }
        }
        cur = cur->p2p_next;
    }
    return best;
}
