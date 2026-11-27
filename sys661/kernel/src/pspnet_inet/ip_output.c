#include "pspnet_inet_internal.h"

static InetMbuf *inetIpInsertOptions(InetMbuf *m, InetMbuf *opt, s32 *phlen);
static int inetIpOptcopy(const InetIpHdr *ip, InetIpHdr *jp);
static int inetIpPcbopts(InetMbuf **pcbopt, InetMbuf *m);
static int inetIpSetMoptions(int optname, InetIpMoptions **imop, InetMbuf *m);
static int inetIpGetMoptions(int optname, InetIpMoptions *imo, InetMbuf **mp);
static void inetIpMloopback(InetIfnet *ifp, InetMbuf *m, InetSockaddrIn *dst);

static InetIfnet *inetLookupUpIfpByAddr(u32 ipAddr)
{
    InetInIfaddr *ia = g_in_ifaddr_hashtbl[ipAddr % 509U];
    while (ia != NULL) {
        if (ia->ia_addr.sin_addr == ipAddr && (ia->ia_ifa.ifa_ifp->if_flags & INET_IFF_UP) != 0) {
            return ia->ia_ifa.ifa_ifp;
        }
        ia = ia->ia_hash_next;
    }
    return NULL;
}

/* [286] 0x018528 */
INET_NOINLINE int inetIpOutput(InetMbuf *m, InetMbuf *opt, InetRoute *ro, int flags, InetIpMoptions *imo, ...)
{
    s32 *mtuOut = NULL;
    InetRoute localRoute;
    InetSockaddrIn *dst;
    InetIpHdr *ip;
    InetIfnet *ifp = NULL;
    InetInIfaddr *ia = NULL;
    s32 hlen = sizeof(InetIpHdr);
    s32 mtu = 0;
    int err = 0;
    int doneEarly = 0;

    if ((flags & 0x04) != 0) {
        va_list ap;
        va_start(ap, imo);
        mtuOut = va_arg(ap, s32 *);
        va_end(ap);
    }

    if (opt != NULL) {
        m = inetIpInsertOptions(m, opt, &hlen);
    }

    ip = (InetIpHdr *)m->m_data;
    if ((flags & 0x03) != 0) {
        hlen = (s32)(ip->ip_vhl & 0x0F) * 4;
    } else {
        ip->ip_vhl = (u8)((0x40) | ((hlen >> 2) & 0x0F));
        ip->ip_off &= 0x4000;
        ip->ip_id = inetBswap16(g_ip_id++);
        g_ipstat.ips_localout++;
    }

    if (ro == NULL) {
        ro = &localRoute;
        sceNet_lib_8B615F52(ro, sizeof(localRoute));
    }

    dst = (InetSockaddrIn *)&ro->ro_dst;
    if (ro->ro_rt != NULL && ((ro->ro_rt->rt_flags & INET_RTF_UP) == 0 || dst->sin_addr != ip->ip_dst)) {
        if (ro->ro_rt->rt_refcnt >= 2) {
            ro->ro_rt->rt_refcnt--;
        } else {
            inetRtFree(ro->ro_rt);
        }
        ro->ro_rt = NULL;
    }
    if (ro->ro_rt == NULL) {
        dst->sin_family = INET_AF_INET;
        dst->sin_len = sizeof(InetSockaddrIn);
        dst->sin_addr = ip->ip_dst;
    }

    if ((flags & 0x10) != 0) {
        ia = (InetInIfaddr *)inetIfaWithAnyAddr((InetSockaddr *)dst);
        if (ia == NULL) {
            g_ipstat.ips_noroute++;
            sceNetIfhandle_lib_F56FAC82(m);
            return INET_ENETUNREACH;
        }
        ifp = ia->ia_ifa.ifa_ifp;
        mtu = (s32)ifp->if_data.ifi_mtu;
        ip->ip_ttl = 1;
    } else {
        if (ro->ro_rt == NULL) {
            inetRtAlloc(ro);
            if (ro->ro_rt == NULL) {
                g_ipstat.ips_noroute++;
                sceNetIfhandle_lib_F56FAC82(m);
                return INET_EHOSTUNREACH;
            }
        }
        ia = (InetInIfaddr *)ro->ro_rt->rt_ifa;
        ifp = ro->ro_rt->rt_ifp;
        mtu = (s32)ro->ro_rt->rt_rmx.rmx_mtu;
        if (mtu == 0) {
            mtu = (s32)ifp->if_data.ifi_mtu;
        }
        ro->ro_rt->rt_use++;
        if ((ro->ro_rt->rt_flags & INET_RTF_GATEWAY) != 0) {
            dst = (InetSockaddrIn *)ro->ro_rt->rt_gateway;
        }
    }

    if ((ip->ip_dst & inetBswap32(0xF0000000U)) == inetBswap32(0xE0000000U) ||
        ip->ip_dst == 0xFFFFFFFFU) {
        m->m_flags |= (ip->ip_dst == 0xFFFFFFFFU) ? INET_M_BCAST : INET_M_MCAST;
        dst = (InetSockaddrIn *)&ro->ro_dst;
        if (imo != NULL) {
            ip->ip_ttl = imo->imo_multicast_ttl;
            if (imo->imo_multicast_ifp != NULL) {
                ifp = imo->imo_multicast_ifp;
                mtu = (s32)ifp->if_data.ifi_mtu;
            }
        } else {
            ip->ip_ttl = 1;
        }

        if (((m->m_flags & INET_M_MCAST) != 0 && (ifp->if_flags & INET_IFF_MULTICAST) == 0) ||
            ((m->m_flags & INET_M_BCAST) != 0 && (ifp->if_flags & INET_IFF_BROADCAST) == 0)) {
            g_ipstat.ips_noroute++;
            err = INET_ENETUNREACH;
            sceNetIfhandle_lib_F56FAC82(m);
        } else {
            InetIfaddr *ifa = ifp->if_addrhead;
            while (ifa != NULL && ifa->ifa_addr->sa_family != INET_AF_INET) {
                ifa = ifa->ifa_next;
            }
            if (ip->ip_src == 0) {
                if (ifa == NULL) {
                    err = INET_EADDRNOTAVAIL;
                    sceNetIfhandle_lib_F56FAC82(m);
                } else {
                    ip->ip_src = ((InetInIfaddr *)ifa)->ia_addr.sin_addr;
                }
            }
            if (err == 0) {
                if (ifa != NULL) {
                    InetInMulti *inm = ((InetInIfaddr *)ifa)->ia_multiaddrs;
                    while (inm != NULL) {
                        if (inm->inm_addr == ip->ip_dst) {
                            if (imo == NULL || imo->imo_multicast_loop != 0) {
                                inetIpMloopback(ifp, m, (InetSockaddrIn *)&ro->ro_dst);
                            }
                            break;
                        }
                        inm = inm->inm_next;
                    }
                }
                if (ip->ip_ttl == 0 || (ifp->if_flags & INET_IFF_LOOPBACK) != 0) {
                    sceNetIfhandle_lib_F56FAC82(m);
                    err = 0;
                    doneEarly = 1;
                }
            }
        }
    } else {
        if (ip->ip_src == 0) {
            ip->ip_src = ia->ia_addr.sin_addr;
        }
        if ((ip->ip_src & inetBswap32(0xF0000000U)) == inetBswap32(0xE0000000U)) {
            g_ipstat.ips_odropped++;
            err = INET_EADDRNOTAVAIL;
            sceNetIfhandle_lib_F56FAC82(m);
        } else if (inetInBroadcast(dst->sin_addr, ifp)) {
            if ((ifp->if_flags & INET_IFF_BROADCAST) == 0) {
                err = INET_EADDRNOTAVAIL;
                sceNetIfhandle_lib_F56FAC82(m);
            } else if ((flags & 0x20) == 0) {
                err = INET_EACCES;
                sceNetIfhandle_lib_F56FAC82(m);
            } else if (ifp->if_data.ifi_mtu < (u64)ip->ip_len) {
                err = INET_EMSGSIZE;
                sceNetIfhandle_lib_F56FAC82(m);
            } else {
                m->m_flags |= INET_M_BCAST;
            }
        } else {
            m->m_flags &= (u16)~INET_M_BCAST;
        }
    }

    if (err == 0 && !doneEarly) {
        if ((flags & 0x400) != 0 && ro->ro_rt != NULL && (ro->ro_rt->rt_rmx.rmx_locks & 1) == 0) {
            ip->ip_off |= 0x4000;
        }

        if ((s32)ip->ip_len <= mtu) {
            ip->ip_len = inetBswap16(ip->ip_len);
            ip->ip_off = inetBswap16(ip->ip_off);
            ip->ip_sum = 0;
            ip->ip_sum = inetInCksum(m, hlen);
            err = ifp->if_output(ifp, m, (InetSockaddr *)dst, ro->ro_rt);
        } else if ((ip->ip_off & 0x4000) != 0) {
            if (mtuOut != NULL) {
                *mtuOut = mtu;
            }
            g_ipstat.ips_cantfrag++;
            err = INET_EMSGSIZE;
            sceNetIfhandle_lib_F56FAC82(m);
        } else {
            s32 fragPayload = (mtu - hlen) & ~7;
            if (fragPayload < 8) {
                err = INET_EMSGSIZE;
                sceNetIfhandle_lib_F56FAC82(m);
            } else {
                InetMbuf **mNextPkt = &m->m_nextpkt;
                s32 firstLen = hlen + fragPayload;
                s32 off = firstLen;
                s32 nFrags = 0;
                s32 fragHlen = sizeof(InetIpHdr);
                u16 origIpLen = ip->ip_len;

                while (off < (s32)origIpLen) {
                    InetMbuf *fragM = (InetMbuf *)sceNetIfhandle_lib_15CFE3C0(sizeof(InetMbuf));
                    u32 maxLinkHdr = 0;
                    InetIpHdr *fragIp;
                    s32 curLen = fragPayload;

                    if (fragM == NULL) {
                        g_ipstat.ips_odropped++;
                        err = INET_ENOBUFS;
                        break;
                    }
                    fragM->m_next = NULL;
                    fragM->m_nextpkt = NULL;
                    fragM->m_data = M_PKTDAT(fragM);
                    fragM->m_type = INET_MT_HEADER;
                    fragM->m_flags = INET_M_PKTHDR;
                    M_CSUM(fragM) = 0;
                    *mNextPkt = fragM;
                    mNextPkt = &fragM->m_nextpkt;

                    sceNetIfhandle_lib_5FB31C72(&maxLinkHdr, NULL, NULL, NULL);
                    fragM->m_data += maxLinkHdr;
                    fragIp = (InetIpHdr *)fragM->m_data;
                    *fragIp = *ip;
                    fragM->m_flags |= (u16)(m->m_flags & (INET_M_BCAST | INET_M_MCAST));

                    if (hlen > (s32)sizeof(InetIpHdr)) {
                        fragHlen = inetIpOptcopy(ip, fragIp) + (s32)sizeof(InetIpHdr);
                        fragIp->ip_vhl = (u8)((fragIp->ip_vhl & 0xF0) | ((fragHlen >> 2) & 0x0F));
                    }
                    fragM->m_len = fragHlen;
                    fragIp->ip_off = (u16)(((off - hlen) >> 3) + (ip->ip_off & ~0x2000));
                    if ((ip->ip_off & 0x2000) != 0) {
                        fragIp->ip_off |= 0x2000;
                    }
                    if (off + curLen >= (s32)origIpLen) {
                        curLen = (s32)origIpLen - off;
                    } else {
                        fragIp->ip_off |= 0x2000;
                    }
                    fragIp->ip_len = inetBswap16((u16)(curLen + fragHlen));
                    fragM->m_next = sceNetIfhandle_lib_456E3146(m, off, curLen, INET_M_DONTWAIT);
                    if (fragM->m_next == NULL) {
                        g_ipstat.ips_odropped++;
                        err = INET_ENOBUFS;
                        break;
                    }
                    M_RCVIF(fragM) = NULL;
                    M_PKTLEN(fragM) = fragHlen + curLen;
                    nFrags++;
                    fragIp->ip_off = inetBswap16(fragIp->ip_off);
                    fragIp->ip_sum = 0;
                    fragIp->ip_sum = inetInCksum(fragM, fragHlen);
                    g_ipstat.ips_ofragments++;
                    off += curLen;
                }

                if (err == 0) {
                    sceNetIfhandle_lib_BFF3CEA5(m, firstLen - (s32)origIpLen);
                    M_PKTLEN(m) = firstLen;
                    ip->ip_len = inetBswap16((u16)firstLen);
                    ip->ip_off = inetBswap16((u16)(ip->ip_off | 0x2000));
                    ip->ip_sum = 0;
                    ip->ip_sum = inetInCksum(m, hlen);
                }

                {
                    int spl = sceNet_lib_94B44F26();
                    if (ifp->if_snd.ifq_maxlen - ifp->if_snd.ifq_len < nFrags) {
                        err = INET_ENOBUFS;
                    }
                    sceNet_lib_515B2F33(spl);
                }

                {
                    InetMbuf *sendM = m;
                    while (sendM != NULL) {
                        InetMbuf *nextPkt = sendM->m_nextpkt;
                        sendM->m_nextpkt = NULL;
                        if (err != 0) {
                            sceNetIfhandle_lib_F56FAC82(sendM);
                        } else {
                            err = ifp->if_output(ifp, sendM, (InetSockaddr *)dst, ro->ro_rt);
                        }
                        sendM = nextPkt;
                    }
                }
                if (err == 0) {
                    g_ipstat.ips_fragmented++;
                }
            }
        }
    }

    if (ro == &localRoute && (flags & 0x10) == 0 && localRoute.ro_rt != NULL) {
        if (localRoute.ro_rt->rt_refcnt >= 2) {
            localRoute.ro_rt->rt_refcnt--;
        } else {
            inetRtFree(localRoute.ro_rt);
        }
        ro->ro_rt = NULL;
    }
    return err;
}

/* [287] 0x019120 */
static INET_NOINLINE InetMbuf *inetIpInsertOptions(InetMbuf *m, InetMbuf *opt, s32 *phlen)
{
    InetIpHdr *ip = (InetIpHdr *)m->m_data;
    s32 optLen = opt->m_len - 4;
    u32 *optData = (u32 *)opt->m_data;

    if ((u32)(optLen + ip->ip_len) > 0xFFFFU) {
        return m;
    }
    if (optData[0] != 0) {
        ip->ip_dst = optData[0];
    }

    if ((m->m_flags & INET_M_EXT) == 0 && (u32)(m->m_data - M_PKTDAT(m)) >= (u32)optLen) {
        m->m_data -= optLen;
        m->m_len += optLen;
        M_PKTLEN(m) += optLen;
        sceNet_lib_4753D878(m->m_data, ip, sizeof(InetIpHdr));
    } else {
        InetMbuf *hdrM = (InetMbuf *)sceNetIfhandle_lib_15CFE3C0(sizeof(InetMbuf));
        u32 maxLinkHdr = 0;
        if (hdrM == NULL) {
            return m;
        }
        hdrM->m_data = M_PKTDAT(hdrM);
        hdrM->m_type = INET_MT_HEADER;
        hdrM->m_flags = INET_M_PKTHDR;
        hdrM->m_nextpkt = NULL;
        M_CSUM(hdrM) = 0;
        M_PKTLEN(hdrM) = M_PKTLEN(m) + optLen;
        hdrM->m_len = (s32)sizeof(InetIpHdr) + optLen;
        m->m_data += sizeof(InetIpHdr);
        m->m_len -= sizeof(InetIpHdr);
        hdrM->m_next = m;
        m = hdrM;

        sceNetIfhandle_lib_5FB31C72(&maxLinkHdr, NULL, NULL, NULL);
        m->m_data += maxLinkHdr;
        sceNet_lib_2F305274(ip, m->m_data, sizeof(InetIpHdr));
    }

    ip = (InetIpHdr *)m->m_data;
    sceNet_lib_2F305274(optData + 1, (u8 *)ip + sizeof(InetIpHdr), optLen);
    *phlen = optLen + (s32)sizeof(InetIpHdr);
    ip->ip_len = (u16)(ip->ip_len + optLen);
    return m;
}

/* [288] 0x0192E8 */
static INET_NOINLINE int inetIpOptcopy(const InetIpHdr *ip, InetIpHdr *jp)
{
    const u8 *cp = (const u8 *)ip + sizeof(InetIpHdr);
    s32 cnt = (s32)(ip->ip_vhl & 0x0F) * 4 - (s32)sizeof(InetIpHdr);
    u8 *dp = (u8 *)jp + sizeof(InetIpHdr);
    int outLen;

    while (cnt > 0) {
        u8 opt = cp[0];
        s32 optLen = 1;
        if (opt == 0) {
            break;
        }
        if (opt == 1) {
            *dp++ = 1;
        } else {
            optLen = (cp[1] < cnt) ? cp[1] : cnt;
            if ((opt & 0x80) != 0) {
                sceNet_lib_2F305274(cp, dp, optLen);
                dp += optLen;
            }
        }
        cnt -= optLen;
        cp += optLen;
    }

    outLen = (int)(dp - ((u8 *)jp + sizeof(InetIpHdr)));
    while ((outLen & 3) != 0) {
        *dp++ = 0;
        outLen++;
    }
    return outLen;
}

/* [289] 0x0193D0 */
INET_NOINLINE int inetIpCtloutput(int op, InetSocket *so, int level, int optname, InetMbuf **mp)
{
    InetInpcbReal *inp = (InetInpcbReal *)so->so_pcb;
    InetMbuf *m = *mp;
    int err = 0;

    if (level != INET_IPPROTO_IP) {
        if (op == INET_PRCO_SETOPT && m != NULL) {
            sceNetIfhandle_lib_F8825DC4(m);
        }
        return INET_EINVAL;
    }

    if (op == INET_PRCO_GETOPT) {
        switch (optname) {
        case 1: /* IP_OPTIONS */
        case 8: /* IP_RETOPTS */
            m = sceNetIfhandle_lib_A493AA5F(INET_M_WAIT, INET_MT_SOOPTS);
            *mp = m;
            if (m == NULL) {
                return INET_ENOBUFS;
            }
            if (inp->inp_options != NULL) {
                m->m_len = inp->inp_options->m_len;
                sceNet_lib_2F305274(inp->inp_options->m_data, m->m_data, m->m_len);
            } else {
                m->m_len = 0;
            }
            return 0;

        case 3:  /* IP_TOS */
        case 4:  /* IP_TTL */
        case 5:  /* IP_RECVOPTS */
        case 6:  /* IP_RECVRETOPTS */
        case 7:  /* IP_RECVDSTADDR */
        case 19: /* IP_PORTRANGE */
        case 20: /* IP_RECVIF */
        case 21: /* IP_ERRORMTU */ {
            s32 val = 0;
            m = sceNetIfhandle_lib_A493AA5F(INET_M_WAIT, INET_MT_SOOPTS);
            *mp = m;
            if (m == NULL) {
                return INET_ENOBUFS;
            }
            m->m_len = 4;
            switch (optname) {
            case 3:  val = inp->inp_ip.ip_tos; break;
            case 4:  val = inp->inp_ip.ip_ttl; break;
            case 5:  val = (inp->inp_flags & 0x01); break;
            case 6:  val = (inp->inp_flags & 0x02) != 0; break;
            case 7:  val = (inp->inp_flags & 0x04) != 0; break;
            case 19: val = (inp->inp_flags >> 4) & 2; break;
            case 20: val = (inp->inp_flags & 0x80) != 0; break;
            case 21: val = inp->inp_errormtu; break;
            default: break;
            }
            *(s32 *)m->m_data = val;
            return 0;
        }

        case 9:
        case 10:
        case 11:
        case 12:
        case 13:
            return inetIpGetMoptions(optname, inp->inp_moptions, mp);

        default:
            return INET_ENOPROTOOPT;
        }
    }

    if (op == INET_PRCO_SETOPT) {
        switch (optname) {
        case 1:
            return inetIpPcbopts(&inp->inp_options, m);

        case 3:
        case 4:
        case 5:
        case 6:
        case 7:
        case 20:
            if (m == NULL || m->m_len != 4) {
                err = INET_EINVAL;
            } else {
                s32 val = *(s32 *)m->m_data;
                switch (optname) {
                case 3:  inp->inp_ip.ip_tos = (u8)val; break;
                case 4:  inp->inp_ip.ip_ttl = (u8)val; break;
                case 5:  inp->inp_flags = val ? (inp->inp_flags | 0x01) : (inp->inp_flags & ~0x01); break;
                case 6:  inp->inp_flags = val ? (inp->inp_flags | 0x02) : (inp->inp_flags & ~0x02); break;
                case 7:  inp->inp_flags = val ? (inp->inp_flags | 0x04) : (inp->inp_flags & ~0x04); break;
                case 20: inp->inp_flags = val ? (inp->inp_flags | 0x80) : (inp->inp_flags & ~0x80); break;
                default: break;
                }
            }
            break;

        case 9:
        case 10:
        case 11:
        case 12:
        case 13:
            err = inetIpSetMoptions(optname, &inp->inp_moptions, m);
            break;

        case 19:
            if (m == NULL || m->m_len != 4) {
                err = INET_EINVAL;
            } else {
                s32 val = *(s32 *)m->m_data;
                if (val == 0 || val == 1) {
                    inp->inp_flags &= ~0x20;
                } else if (val == 2) {
                    inp->inp_flags |= 0x20;
                } else {
                    err = INET_EINVAL;
                }
            }
            break;

        default:
            err = INET_ENOPROTOOPT;
            break;
        }
        if (m != NULL) {
            sceNetIfhandle_lib_F8825DC4(m);
        }
        return err;
    }

    return 0;
}

/* [290] 0x019734 */
static INET_NOINLINE int inetIpPcbopts(InetMbuf **pcbopt, InetMbuf *m)
{
    s32 cnt;
    u8 *cp;

    if (*pcbopt != NULL) {
        sceNetIfhandle_lib_F8825DC4(*pcbopt);
    }
    *pcbopt = NULL;
    if (m == NULL) {
        return 0;
    }
    cnt = m->m_len;
    if (cnt == 0) {
        sceNetIfhandle_lib_F8825DC4(m);
        return 0;
    }
    if ((cnt & 3) != 0 || (u8 *)m->m_data + cnt + 4 >= (u8 *)m + sizeof(InetMbuf)) {
        sceNetIfhandle_lib_F8825DC4(m);
        return INET_EINVAL;
    }

    m->m_len = cnt + 4;
    sceNet_lib_4753D878(m->m_data + 4, m->m_data, cnt);
    sceNet_lib_8B615F52(m->m_data, 4);

    cp = m->m_data + 4;
    while (cnt > 0) {
        u8 opt = cp[0];
        s32 optLen = 1;
        if (opt == 0) {
            break;
        }
        if (opt != 1) {
            if (cnt < 2) {
                sceNetIfhandle_lib_F8825DC4(m);
                return INET_EINVAL;
            }
            optLen = cp[1];
            if (optLen < 2 || optLen > cnt) {
                sceNetIfhandle_lib_F8825DC4(m);
                return INET_EINVAL;
            }
        }
        if (opt == 131 || opt == 137) {
            if (optLen < 7) {
                sceNetIfhandle_lib_F8825DC4(m);
                return INET_EINVAL;
            }
            optLen -= 4;
            cnt -= 4;
            m->m_len -= 4;
            cp[1] = (u8)optLen;
            sceNet_lib_2F305274(cp + 3, m->m_data, 4);
            sceNet_lib_4753D878(cp + 3, cp + 7, cnt + 4);
        }
        cnt -= optLen;
        cp += optLen;
    }

    if (m->m_len > 44) {
        sceNetIfhandle_lib_F8825DC4(m);
        return INET_EINVAL;
    }
    *pcbopt = m;
    return 0;
}

/* [291] 0x0198E4 */
static INET_NOINLINE int inetIpSetMoptions(int optname, InetIpMoptions **imop, InetMbuf *m)
{
    InetIpMoptions *imo = *imop;
    int err = 0;

    if (imo == NULL) {
        imo = (InetIpMoptions *)sceNetIfhandle_lib_15CFE3C0(sizeof(InetIpMoptions));
        if (imo == NULL) {
            return INET_ENOBUFS;
        }
        *imop = imo;
        imo->imo_multicast_ifp = NULL;
        imo->imo_multicast_ttl = 1;
        imo->imo_multicast_loop = 1;
        imo->imo_num_memberships = 0;
    }

    switch (optname) {
    case 9: /* IP_MULTICAST_IF */
        if (m == NULL || m->m_len != 4) {
            err = INET_EINVAL;
        } else {
            u32 addr = *(u32 *)m->m_data;
            if (addr == 0) {
                imo->imo_multicast_ifp = NULL;
            } else {
                InetIfnet *ifp = inetLookupUpIfpByAddr(addr);
                if (ifp == NULL || (ifp->if_flags & INET_IFF_MULTICAST) == 0) {
                    err = INET_EADDRNOTAVAIL;
                } else {
                    imo->imo_multicast_ifp = ifp;
                }
            }
        }
        break;

    case 10: /* IP_MULTICAST_TTL */
        if (m == NULL || m->m_len != 1) {
            err = INET_EINVAL;
        } else {
            imo->imo_multicast_ttl = m->m_data[0];
        }
        break;

    case 11: /* IP_MULTICAST_LOOP */
        if (m == NULL || m->m_len != 1 || m->m_data[0] >= 2) {
            err = INET_EINVAL;
        } else {
            imo->imo_multicast_loop = m->m_data[0];
        }
        break;

    case 12: /* IP_ADD_MEMBERSHIP */
        if (m == NULL || m->m_len != 8) {
            err = INET_EINVAL;
        } else {
            u32 *mreq = (u32 *)m->m_data;
            u32 group = mreq[0];
            u32 ifAddr = mreq[1];
            InetIfnet *ifp = NULL;

            if ((group & inetBswap32(0xF0000000U)) != inetBswap32(0xE0000000U)) {
                err = INET_EINVAL;
            } else if (ifAddr == 0) {
                InetRoute ro;
                InetSockaddrIn *sin = (InetSockaddrIn *)&ro.ro_dst;
                sceNet_lib_8B615F52(&ro, sizeof(ro));
                sin->sin_len = sizeof(InetSockaddrIn);
                sin->sin_family = INET_AF_INET;
                sin->sin_addr = group;
                inetRtAlloc(&ro);
                if (ro.ro_rt != NULL) {
                    ifp = ro.ro_rt->rt_ifp;
                    inetRtFree(ro.ro_rt);
                } else {
                    err = INET_EADDRNOTAVAIL;
                }
            } else {
                ifp = inetLookupUpIfpByAddr(ifAddr);
            }

            if (err == 0) {
                if (ifp == NULL || (ifp->if_flags & INET_IFF_MULTICAST) == 0) {
                    err = INET_EADDRNOTAVAIL;
                } else {
                    int idx;
                    for (idx = 0; idx < (int)imo->imo_num_memberships; idx++) {
                        if (sceNet_lib_94DCA9F0(imo->imo_membership[idx]->inm_ifname, ifp->if_xname, 16) == 0 &&
                            imo->imo_membership[idx]->inm_addr == group) {
                            break;
                        }
                    }
                    if (idx < (int)imo->imo_num_memberships) {
                        err = INET_EADDRINUSE;
                    } else if (idx == 20) {
                        err = INET_ETOOMANYREFS;
                    } else {
                        imo->imo_membership[idx] = inetInAddMulti(&group, ifp);
                        if (imo->imo_membership[idx] == NULL) {
                            err = INET_ENOBUFS;
                        } else {
                            imo->imo_num_memberships++;
                        }
                    }
                }
            }
        }
        break;

    case 13: /* IP_DROP_MEMBERSHIP */
        if (m == NULL || m->m_len != 8) {
            err = INET_EINVAL;
        } else {
            u32 *mreq = (u32 *)m->m_data;
            u32 group = mreq[0];
            u32 ifAddr = mreq[1];
            InetIfnet *ifp = NULL;

            if ((group & inetBswap32(0xF0000000U)) != inetBswap32(0xE0000000U)) {
                err = INET_EINVAL;
            } else {
                if (ifAddr != 0) {
                    ifp = inetLookupUpIfpByAddr(ifAddr);
                    if (ifp == NULL) {
                        err = INET_EADDRNOTAVAIL;
                    }
                }
                if (err == 0) {
                    int idx;
                    for (idx = 0; idx < (int)imo->imo_num_memberships; idx++) {
                        if ((ifp == NULL ||
                             sceNet_lib_94DCA9F0(imo->imo_membership[idx]->inm_ifname, ifp->if_xname, 16) == 0) &&
                            imo->imo_membership[idx]->inm_addr == group) {
                            break;
                        }
                    }
                    if (idx == (int)imo->imo_num_memberships) {
                        err = INET_EADDRNOTAVAIL;
                    } else {
                        int moveIdx;
                        inetInDelMulti(imo->imo_membership[idx]);
                        for (moveIdx = idx + 1; moveIdx < (int)imo->imo_num_memberships; moveIdx++) {
                            imo->imo_membership[moveIdx - 1] = imo->imo_membership[moveIdx];
                        }
                        imo->imo_num_memberships--;
                    }
                }
            }
        }
        break;

    default:
        err = INET_EOPNOTSUPP;
        break;
    }

    if (imo->imo_multicast_ifp == NULL &&
        imo->imo_multicast_ttl == 1 &&
        imo->imo_multicast_loop == 1 &&
        imo->imo_num_memberships == 0) {
        sceNetIfhandle_lib_76BAD213(*imop);
        *imop = NULL;
    }
    return err;
}

/* [292] 0x019E90 */
static INET_NOINLINE int inetIpGetMoptions(int optname, InetIpMoptions *imo, InetMbuf **mp)
{
    InetMbuf *m = sceNetIfhandle_lib_A493AA5F(INET_M_WAIT, INET_MT_SOOPTS);
    *mp = m;
    if (m == NULL) {
        return INET_ENOBUFS;
    }

    if (optname == 9) {
        u32 *addrOut = (u32 *)m->m_data;
        m->m_len = 4;
        if (imo != NULL && imo->imo_multicast_ifp != NULL) {
            InetIfaddr *ifa = imo->imo_multicast_ifp->if_addrhead;
            while (ifa != NULL) {
                if (ifa->ifa_addr->sa_family == INET_AF_INET) {
                    *addrOut = ((InetInIfaddr *)ifa)->ia_addr.sin_addr;
                    return 0;
                }
                ifa = ifa->ifa_next;
            }
        }
        *addrOut = g_zeroin_addr;
        return 0;
    }
    if (optname == 10) {
        m->m_len = 1;
        m->m_data[0] = (imo != NULL) ? imo->imo_multicast_ttl : 1;
        return 0;
    }
    if (optname == 11) {
        m->m_len = 1;
        m->m_data[0] = (imo != NULL) ? imo->imo_multicast_loop : 1;
        return 0;
    }
    return INET_EOPNOTSUPP;
}

/* [293] 0x019FE0 */
static INET_NOINLINE void inetIpMloopback(InetIfnet *ifp, InetMbuf *m, InetSockaddrIn *dst)
{
    InetMbuf *copy = sceNetIfhandle_lib_456E3146(m, 0, 1000000000, INET_M_DONTWAIT);
    if (copy == NULL) {
        return;
    }
    if ((copy->m_flags & INET_M_EXT) != 0 || copy->m_len < (s32)sizeof(InetIpHdr)) {
        copy = sceNetIfhandle_lib_49EDBB18(copy, sizeof(InetIpHdr));
        if (copy == NULL) {
            return;
        }
    }
    {
        InetIpHdr *ip = (InetIpHdr *)copy->m_data;
        ip->ip_len = inetBswap16(ip->ip_len);
        ip->ip_off = inetBswap16(ip->ip_off);
        ip->ip_sum = 0;
        ip->ip_sum = inetInCksum(copy, (s32)(ip->ip_vhl & 0x0F) * 4);
        inetLoOutput(ifp, copy, (InetSockaddr *)dst, NULL);
    }
}

/* [294] 0x01A0FC */
INET_NOINLINE s32 inetIpOptlen(InetInpcbReal *inp)
{
    if (inp->inp_options != NULL && inp->inp_options->m_len > 0) {
        return inp->inp_options->m_len;
    }
    return 0;
}

/* [295] 0x01A124 */
INET_NOINLINE void inetIpFreeMoptions(InetIpMoptions *imo)
{
    if (imo != NULL) {
        int idx;
        for (idx = 0; idx < (int)imo->imo_num_memberships; idx++) {
            inetInDelMulti(imo->imo_membership[idx]);
        }
        sceNetIfhandle_lib_76BAD213(imo);
    }
}
