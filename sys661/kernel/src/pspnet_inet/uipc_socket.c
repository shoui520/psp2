/*
 * BSD socket core operations (uipc_socket.c) with PSP/Vita Kermit fast-path hooks.
 */
#include "pspnet_inet_internal.h"

InetSocket *g_socket_list_head;
InetSocket **g_socket_list_tail = &g_socket_list_head;
u32 g_somaxconn = 128U;
s32 g_last_sock_id;

static inline s32 inetSbSpace(const InetSockBuf *sb)
{
    s32 ccSpace = (s32)(sb->sb_hiwat - sb->sb_cc);
    s32 mbSpace = (s32)(sb->sb_mbmax - sb->sb_mbcnt);
    return (ccSpace < mbSpace) ? ccSpace : mbSpace;
}

static inline void inetSbUnlock(InetSockBuf *sb)
{
    sb->sb_flags &= (u16)~INET_SB_LOCK;
    if ((sb->sb_flags & INET_SB_WANT) != 0) {
        sb->sb_flags &= (u16)~INET_SB_WANT;
        sceNet_lib_7574FDA1(&sb->sb_flags);
    }
}

int INET_NOINLINE inetSoCreate(int dom, InetSocket **aso, int type, int proto)
{
    InetProtosw *prp;
    InetSocket *so;
    int spl;
    int error;
    s32 kermitId = -1;

    sceKernelGetThreadId();
    if (proto != 0) {
        prp = inetFindProto(dom, proto, type);
    } else {
        prp = inetFindType(dom, type);
    }
    if (prp == NULL || prp->pr_usrreq == NULL) {
        return INET_EPROTONOSUPPORT;
    }
    if (prp->pr_type != type) {
        return INET_EPROTOTYPE;
    }

    spl = sceNet_lib_94B44F26();
    so = (InetSocket *)sceNetIfhandle_lib_15CFE3C0(sizeof(InetSocket));
    if (so == NULL) {
        sceNet_lib_515B2F33(spl);
        return INET_ENOMEM;
    }

    sceKernelMemset(so, 0, sizeof(InetSocket));
    so->so_q0.tqh_first = NULL;
    so->so_q0.tqh_last = &so->so_q0.tqh_first;
    so->so_q.tqh_first = NULL;
    so->so_q.tqh_last = &so->so_q.tqh_first;
    so->so_send = inetSoSend;
    so->so_receive = inetSoReceive;
    so->so_type = (s16)type;
    so->so_proto = prp;
    so->so_sock_id = -1;

    so->so_list_next = NULL;
    so->so_list_prev = g_socket_list_tail;
    *g_socket_list_tail = so;
    g_socket_list_tail = &so->so_list_next;

    error = sceWlanDrv_lib_19602A86(dom, type, proto, &kermitId);
    so->so_kermit_id = kermitId;
    if (kermitId == -1) {
        error = prp->pr_usrreq(so, INET_PRU_ATTACH, NULL, NULL, (InetMbuf *)(intptr_t)proto, NULL);
    }
    if (error != 0) {
        so->so_state |= INET_SS_NOFDREF;
        inetSoFree(so);
        sceNet_lib_515B2F33(spl);
        return error;
    }

    sceNet_lib_515B2F33(spl);
    *aso = so;
    return 0;
}

void INET_NOINLINE inetSoFree(InetSocket *so)
{
    if (so->so_pcb != NULL || (so->so_state & INET_SS_NOFDREF) == 0) {
        return;
    }
    if (so->so_head != NULL && inetSoQremque(so, 0) == 0) {
        return;
    }
    inetSbRelease(&so->so_snd);
    inetSorFlush(so);

    if (so->so_list_next != NULL) {
        so->so_list_next->so_list_prev = so->so_list_prev;
    } else {
        g_socket_list_tail = so->so_list_prev;
    }
    *so->so_list_prev = so->so_list_next;
    sceNetIfhandle_lib_76BAD213(so);
}

int INET_NOINLINE inetSoClose(InetSocket *so)
{
    int error = 0;
    int spl = sceNet_lib_94B44F26();

    if (so->so_kermit_id != -1) {
        sceWlanDrv_lib_7EED1723(so->so_kermit_id);
        so->so_state &= (u16)~(INET_SS_ISCONNECTED | INET_SS_ISDISCONNECTING);
    }

    if ((so->so_options & INET_SO_ACCEPTCONN) != 0) {
        while (so->so_q0.tqh_first != NULL) {
            InetSocket *sp = so->so_q0.tqh_first;
            inetSoQremque(sp, 0);
            inetSoAbort(sp);
        }
        while (so->so_q.tqh_first != NULL) {
            InetSocket *sp = so->so_q.tqh_first;
            inetSoQremque(sp, 1);
            inetSoAbort(sp);
        }
    }

    if (so->so_pcb != NULL) {
        if ((so->so_state & INET_SS_ISCONNECTED) != 0) {
            if ((so->so_state & INET_SS_ISDISCONNECTING) == 0) {
                error = inetSoDisconnect(so);
            }
            if (error == 0 && (so->so_options & INET_SO_LINGER) != 0) {
                if ((so->so_state & (INET_SS_ISDISCONNECTING | INET_SS_NBIO)) !=
                    (INET_SS_ISDISCONNECTING | INET_SS_NBIO)) {
                    while ((so->so_state & INET_SS_ISCONNECTED) != 0) {
                        error = sceNet_lib_205E8D17(&so->so_timeo, (u32)so->so_linger);
                        if (error != 0) {
                            break;
                        }
                    }
                }
            }
        }
        if (so->so_pcb != NULL) {
            int detachErr = so->so_proto->pr_usrreq(so, INET_PRU_DETACH, NULL, NULL, NULL, NULL);
            if (error == 0) {
                error = detachErr;
            }
        }
    }

    so->so_sock_id = -1;
    so->so_state |= INET_SS_NOFDREF;
    inetSoFree(so);
    sceNet_lib_515B2F33(spl);
    return error;
}

int INET_NOINLINE inetSoAbort(InetSocket *so)
{
    if (so->so_kermit_id != -1) {
        return 0;
    }
    return so->so_proto->pr_usrreq(so, INET_PRU_ABORT, NULL, NULL, NULL, NULL);
}

int INET_NOINLINE inetSoDisconnect(InetSocket *so)
{
    int error = INET_ENOTCONN;
    int spl = sceNet_lib_94B44F26();

    if ((so->so_state & INET_SS_ISCONNECTED) != 0) {
        error = INET_EALREADY;
        if ((so->so_state & INET_SS_ISDISCONNECTING) == 0) {
            error = 0;
            if (so->so_kermit_id == -1) {
                error = so->so_proto->pr_usrreq(so, INET_PRU_DISCONNECT, NULL, NULL, NULL, NULL);
            }
        }
    }
    sceNet_lib_515B2F33(spl);
    return error;
}

int INET_NOINLINE inetSoSend(InetSocket *so, InetMbuf *addr, InetUio *uio,
                             InetMbuf *top, InetMbuf *control, int flags)
{
    InetProtosw *pr = so->so_proto;
    int atomic = ((pr->pr_flags & INET_PR_ATOMIC) != 0) || (top != NULL);
    s32 resid = (uio != NULL) ? uio->uio_resid : M_PKTLEN(top);
    s32 clen = (control != NULL) ? control->m_len : 0;
    int dontroute = ((flags & 0x04) != 0) && ((so->so_options & INET_SO_DONTROUTE) == 0) &&
                    ((pr->pr_flags & INET_PR_ATOMIC) != 0);
    int sendOptP2pEnc = ((flags & 0x400) != 0) && ((so->so_options & 0x1000) == 0) &&
                        ((pr->pr_flags & INET_PR_ATOMIC) != 0);
    int sendOptP2pAuth = ((flags & 0x800) != 0) && ((so->so_options & 0x2000) == 0) &&
                         ((pr->pr_flags & INET_PR_ATOMIC) != 0);
    int error = 0;
    int locked = 0;

    if (resid < 0) {
        error = INET_EINVAL;
    } else {
        for (;;) {
            int spl;
            s32 space;
            InetMbuf **mp;

            if (!locked) {
                if ((so->so_snd.sb_flags & INET_SB_LOCK) != 0) {
                    if ((flags & 0x80) == 0) {
                        inetSbLock(&so->so_snd);
                    }
                } else {
                    so->so_snd.sb_flags |= INET_SB_LOCK;
                }
                locked = 1;
            }

            spl = sceNet_lib_94B44F26();
            if ((so->so_state & INET_SS_CANTSENDMORE) != 0) {
                sceNet_lib_515B2F33(spl);
                error = INET_EPIPE;
                break;
            }
            if (so->so_error != 0) {
                error = so->so_error;
                so->so_error = 0;
                sceNet_lib_515B2F33(spl);
                break;
            }
            if ((so->so_state & INET_SS_ISCONNECTED) == 0) {
                if ((pr->pr_flags & INET_PR_CONNREQUIRED) != 0) {
                    if ((so->so_state & INET_SS_ISDISCONNECTED) == 0 && (resid != 0 || clen == 0)) {
                        sceNet_lib_515B2F33(spl);
                        error = INET_ENOTCONN;
                        break;
                    }
                } else if (addr == NULL) {
                    sceNet_lib_515B2F33(spl);
                    error = INET_EDESTADDRREQ;
                    break;
                }
            }

            space = inetSbSpace(&so->so_snd);
            if ((flags & 0x01) != 0) {
                space += 1024;
            }
            if ((atomic && (u32)resid > so->so_snd.sb_hiwat) || (u32)clen > so->so_snd.sb_hiwat) {
                sceNet_lib_515B2F33(spl);
                error = INET_EMSGSIZE;
                break;
            }
            if (space < resid + clen && uio != NULL &&
                (atomic || space < so->so_snd.sb_lowat || space < clen)) {
                if ((so->so_state & INET_SS_NBIO) != 0) {
                    sceNet_lib_515B2F33(spl);
                    error = INET_EAGAIN;
                    break;
                }
                inetSbUnlock(&so->so_snd);
                locked = 0;
                error = inetSbWait(&so->so_snd);
                sceNet_lib_515B2F33(spl);
                if (error != 0) {
                    break;
                }
                continue;
            }

            sceNet_lib_515B2F33(spl);
            space -= clen;
            mp = &top;

            do {
                if (uio != NULL) {
                    for (;;) {
                        InetMbuf *m = (InetMbuf *)sceNetIfhandle_lib_15CFE3C0(sizeof(InetMbuf));
                        u32 mlen;
                        s32 len;
                        int copyErr;

                        if (m == NULL) {
                            error = INET_ENOBUFS;
                            break;
                        }
                        m->m_next = NULL;
                        m->m_nextpkt = NULL;
                        m->m_type = INET_MT_DATA;
                        if (top != NULL) {
                            m->m_data = M_DAT(m);
                            m->m_flags = 0;
                            mlen = 236U;
                        } else {
                            m->m_data = M_PKTDAT(m);
                            m->m_flags = INET_M_PKTHDR;
                            M_RCVIF(m) = NULL;
                            M_PKTLEN(m) = 0;
                            M_CSUM(m) = 0;
                            mlen = 208U;
                        }

                        if (resid >= 445 && space >= 2048) {
                            u8 *cl = (u8 *)sceNetIfhandle_lib_15CFE3C0(2048);
                            if (cl == NULL) {
                                cl = (u8 *)sceNetIfhandle_lib_15CFE3C0(2048);
                            }
                            M_EXT(m).ext_buf = cl;
                            if (cl != NULL) {
                                m->m_data = cl;
                                m->m_flags |= (INET_M_EXT | INET_M_CLUSTER);
                                M_EXT(m).ext_size = 2048;
                                M_EXT(m).ext_free = NULL;
                                M_EXT(m).ext_arg = NULL;
                                M_EXT(m).ext_nextref = m;
                                M_EXT(m).ext_prevref = m;
                            }
                        }

                        if ((m->m_flags & INET_M_EXT) != 0) {
                            if (atomic && top == NULL) {
                                u32 maxHdr = 0;
                                sceNetIfhandle_lib_5FB31C72(NULL, NULL, &maxHdr, NULL);
                                len = ((u32)resid <= (2048U - maxHdr)) ? resid : (s32)(2048U - maxHdr);
                                m->m_data += maxHdr;
                            } else {
                                len = ((u32)resid <= 2048U) ? resid : 2048;
                            }
                            space -= len;
                        } else {
                            len = ((u32)resid < mlen) ? resid : (s32)mlen;
                            if (len > space) {
                                len = space;
                            }
                            space -= len;
                            if (atomic && top == NULL && len < (s32)mlen) {
                                m->m_data += (208 - len) & ~3;
                            }
                        }

                        copyErr = sceNet_lib_2B42872F(m->m_data, len, uio);
                        *mp = m;
                        error = copyErr;
                        resid = uio->uio_resid;
                        m->m_len = len;
                        M_PKTLEN(top) += len;
                        if (copyErr != 0) {
                            break;
                        }
                        mp = &m->m_next;
                        if (resid <= 0) {
                            if ((flags & 0x08) != 0) {
                                top->m_flags |= INET_M_EOR;
                            }
                            break;
                        }
                        if (!atomic || space <= 0) {
                            break;
                        }
                    }
                    if (error != 0) {
                        break;
                    }
                } else {
                    resid = 0;
                    if ((flags & 0x08) != 0 && top != NULL) {
                        top->m_flags |= INET_M_EOR;
                    }
                }

                spl = sceNet_lib_94B44F26();
                if ((so->so_state & INET_SS_CANTSENDMORE) != 0) {
                    sceNet_lib_515B2F33(spl);
                    error = INET_EPIPE;
                    break;
                }
                if (dontroute) {
                    so->so_options |= INET_SO_DONTROUTE;
                }
                if (resid > 0) {
                    so->so_state |= INET_SS_MORETOCOME;
                }
                if (sendOptP2pEnc) {
                    so->so_options |= 0x1000;
                }
                if (sendOptP2pAuth) {
                    so->so_options |= 0x2000;
                }

                error = pr->pr_usrreq(so, ((flags & 0x01) != 0) ? INET_PRU_SENDOOB : INET_PRU_SEND,
                                      top, addr, control, NULL);

                if (dontroute) {
                    so->so_options &= (u16)~INET_SO_DONTROUTE;
                }
                if (resid > 0) {
                    so->so_state &= (u16)~INET_SS_MORETOCOME;
                }
                if (sendOptP2pEnc) {
                    so->so_options &= (u16)~0x1000;
                }
                if (sendOptP2pAuth) {
                    so->so_options &= (u16)~0x2000;
                }
                sceNet_lib_515B2F33(spl);

                clen = 0;
                top = NULL;
                mp = &top;
                control = NULL;
                if (error != 0) {
                    break;
                }
            } while (resid != 0 && space > 0);

            if (error != 0 || resid == 0) {
                break;
            }
        }
    }

    if (locked) {
        inetSbUnlock(&so->so_snd);
    }
    if (top != NULL) {
        sceNetIfhandle_lib_F56FAC82(top);
    }
    if (control != NULL) {
        sceNetIfhandle_lib_F56FAC82(control);
    }
    return error;
}

int INET_NOINLINE inetSoReceive(InetSocket *so, InetMbuf **paddr, InetUio *uio,
                                InetMbuf **mp0, InetMbuf **controlp, u32 *flagsp)
{
    InetProtosw *pr = so->so_proto;
    s32 origResid = uio->uio_resid;
    u32 flags = (flagsp != NULL) ? (*flagsp & ~8U) : 0U;
    int error = 0;
    int needDrop = 0;
    int spl = 0;
    InetMbuf *m = NULL;
    InetMbuf *nextrecord = NULL;
    InetMbuf **mp = mp0;

    if (paddr != NULL) {
        *paddr = NULL;
    }
    if (controlp != NULL) {
        *controlp = NULL;
    }

    /* MSG_OOB fast path */
    if ((flags & 0x01U) != 0) {
        InetMbuf *oob = sceNetIfhandle_lib_A493AA5F(INET_M_WAIT, INET_MT_DATA);
        if (oob == NULL) {
            return INET_ENOBUFS;
        }
        error = pr->pr_usrreq(so, INET_PRU_RCVOOB, oob, (InetMbuf *)(uintptr_t)(flags & 2U), NULL, NULL);
        if (error == 0) {
            while (oob != NULL && uio->uio_resid > 0 && error == 0) {
                s32 copyLen = (uio->uio_resid < oob->m_len) ? uio->uio_resid : oob->m_len;
                error = sceNet_lib_2B42872F(oob->m_data, copyLen, uio);
                oob = sceNetIfhandle_lib_F8825DC4(oob);
            }
        }
        if (oob != NULL) {
            sceNetIfhandle_lib_F56FAC82(oob);
        }
        return error;
    }

    if (mp != NULL) {
        *mp = NULL;
    }
    if ((so->so_state & INET_SS_ISDISCONNECTED) != 0 && origResid != 0) {
        pr->pr_usrreq(so, INET_PRU_RCVD, NULL, NULL, NULL, NULL);
    }

    for (;;) {
        int moff;
        int offset;
        s16 mtype = 0;

        if ((so->so_rcv.sb_flags & INET_SB_LOCK) != 0) {
            if ((flags & 0x80U) == 0) {
                inetSbLock(&so->so_rcv);
            }
        } else {
            so->so_rcv.sb_flags |= INET_SB_LOCK;
        }

        error = 0;
        spl = sceNet_lib_94B44F26();
        m = so->so_rcv.sb_mb;

        if (m == NULL ||
            (((flags & 0x80U) == 0) &&
             so->so_rcv.sb_cc < (u32)uio->uio_resid &&
             (so->so_rcv.sb_cc < (u32)so->so_rcv.sb_lowat ||
              ((flags & 0x40U) != 0 && (u32)uio->uio_resid <= so->so_rcv.sb_hiwat)) &&
             m->m_nextpkt == NULL &&
             (pr->pr_flags & INET_PR_ATOMIC) == 0)) {
            int hasSpecial = 0;
            if (m != NULL) {
                InetMbuf *scan = m;
                while (scan != NULL) {
                    if (scan->m_type == INET_MT_OOBDATA || (scan->m_flags & INET_M_EOR) != 0) {
                        hasSpecial = 1;
                        break;
                    }
                    scan = scan->m_next;
                }
            }
            if (!hasSpecial) {
                if (so->so_error != 0) {
                    if (m == NULL) {
                        error = so->so_error;
                        if ((flags & 2U) == 0) {
                            so->so_error = 0;
                        }
                        break;
                    }
                } else if ((so->so_state & INET_SS_CANTRCVMORE) != 0) {
                    if (m == NULL) {
                        break;
                    }
                } else {
                    if ((so->so_state & (INET_SS_ISCONNECTED | INET_SS_ISCONNECTING)) == 0 &&
                        (pr->pr_flags & INET_PR_CONNREQUIRED) != 0) {
                        error = INET_ENOTCONN;
                        break;
                    }
                    if (uio->uio_resid == 0) {
                        break;
                    }
                    if ((so->so_state & INET_SS_NBIO) != 0 || (flags & 0x80U) != 0) {
                        error = INET_EAGAIN;
                        break;
                    }
                    inetSbUnlock(&so->so_rcv);
                    error = inetSbWait(&so->so_rcv);
                    sceNet_lib_515B2F33(spl);
                    if (error != 0) {
                        return error;
                    }
                    continue;
                }
            }
        }

        nextrecord = m->m_nextpkt;

        /* Extract source address mbuf if PR_ADDR */
        if ((pr->pr_flags & INET_PR_ADDR) != 0) {
            origResid = 0;
            if ((flags & 2U) != 0) {
                if (paddr != NULL) {
                    *paddr = sceNetIfhandle_lib_456E3146(m, 0, m->m_len, INET_M_DONTWAIT);
                }
                m = m->m_next;
            } else {
                so->so_rcv.sb_cc -= (u32)m->m_len;
                so->so_rcv.sb_mbcnt -= 256U;
                if ((m->m_flags & INET_M_EXT) != 0) {
                    so->so_rcv.sb_mbcnt -= M_EXT(m).ext_size;
                }
                needDrop = 1;
                if (paddr != NULL) {
                    *paddr = m;
                    so->so_rcv.sb_mb = m->m_next;
                    m->m_next = NULL;
                    m = so->so_rcv.sb_mb;
                } else {
                    so->so_rcv.sb_mb = inetMfreeOne(m);
                    m = so->so_rcv.sb_mb;
                }
            }
        }

        /* Extract control mbufs */
        while (m != NULL && m->m_type == INET_MT_CONTROL && error == 0) {
            if ((flags & 2U) != 0) {
                if (controlp != NULL) {
                    *controlp = sceNetIfhandle_lib_456E3146(m, 0, m->m_len, INET_M_DONTWAIT);
                }
                m = m->m_next;
            } else {
                so->so_rcv.sb_cc -= (u32)m->m_len;
                so->so_rcv.sb_mbcnt -= 256U;
                if ((m->m_flags & INET_M_EXT) != 0) {
                    so->so_rcv.sb_mbcnt -= M_EXT(m).ext_size;
                }
                needDrop = 1;
                if (controlp != NULL) {
                    if (pr->pr_domain->dom_externalize != NULL) {
                        const InetCmsghdr *cm = (const InetCmsghdr *)m->m_data;
                        if (cm->cmsg_type == 1) {
                            error = pr->pr_domain->dom_externalize(m);
                        }
                    }
                    *controlp = m;
                    so->so_rcv.sb_mb = m->m_next;
                    m->m_next = NULL;
                    m = so->so_rcv.sb_mb;
                } else {
                    so->so_rcv.sb_mb = inetMfreeOne(m);
                    m = so->so_rcv.sb_mb;
                }
            }
            if (controlp != NULL) {
                controlp = &(*controlp)->m_next;
                origResid = 0;
            }
        }

        if (m != NULL) {
            if ((flags & 2U) == 0) {
                m->m_nextpkt = nextrecord;
            }
            mtype = m->m_type;
            if (mtype == INET_MT_OOBDATA) {
                flags |= 0x01U;
            }
        }

        moff = 0;
        offset = 0;

        /* Data copy loop */
        while (m != NULL && uio->uio_resid > 0 && error == 0) {
            s32 len;

            if (m->m_type == INET_MT_OOBDATA) {
                if (mtype != INET_MT_OOBDATA) {
                    break;
                }
            } else if (mtype == INET_MT_OOBDATA) {
                break;
            }

            so->so_state &= (u16)~INET_SS_RCVATMARK;
            len = uio->uio_resid;
            if (so->so_oobmark != 0 && len > (s32)(so->so_oobmark - (u32)offset)) {
                len = (s32)(so->so_oobmark - (u32)offset);
            }
            if (len > m->m_len - moff) {
                len = m->m_len - moff;
            }

            if (mp == NULL) {
                sceNet_lib_515B2F33(spl);
                error = sceNet_lib_2B42872F(m->m_data + moff, len, uio);
                spl = sceNet_lib_94B44F26();
                if (error != 0) {
                    if (m != NULL && needDrop && (pr->pr_flags & INET_PR_ATOMIC) != 0) {
                        inetSbDropRecord(&so->so_rcv);
                    }
                    inetSbUnlock(&so->so_rcv);
                    sceNet_lib_515B2F33(spl);
                    return error;
                }
            } else {
                uio->uio_resid -= len;
            }

            if (len == m->m_len - moff) {
                if ((m->m_flags & INET_M_EOR) != 0) {
                    flags |= 0x08U;
                }
                if ((flags & 2U) != 0) {
                    m = m->m_next;
                    moff = 0;
                } else {
                    nextrecord = m->m_nextpkt;
                    so->so_rcv.sb_cc -= (u32)m->m_len;
                    so->so_rcv.sb_mbcnt -= 256U;
                    if ((m->m_flags & INET_M_EXT) != 0) {
                        so->so_rcv.sb_mbcnt -= M_EXT(m).ext_size;
                    }
                    if (mp != NULL) {
                        *mp = m;
                        mp = &m->m_next;
                        so->so_rcv.sb_mb = m->m_next;
                        m = m->m_next;
                        *mp = NULL;
                    } else {
                        so->so_rcv.sb_mb = inetMfreeOne(m);
                        m = so->so_rcv.sb_mb;
                    }
                    if (m != NULL) {
                        m->m_nextpkt = nextrecord;
                    }
                }
            } else {
                if ((flags & 2U) != 0) {
                    moff += len;
                } else {
                    if (mp != NULL) {
                        *mp = sceNetIfhandle_lib_456E3146(m, 0, len, INET_M_WAIT);
                    }
                    m->m_data += len;
                    m->m_len -= len;
                    so->so_rcv.sb_cc -= (u32)len;
                }
            }

            if (so->so_oobmark != 0) {
                if ((flags & 2U) != 0) {
                    offset += len;
                    if ((u32)offset == so->so_oobmark) {
                        break;
                    }
                } else {
                    so->so_oobmark -= (u32)len;
                    if (so->so_oobmark == 0) {
                        so->so_state |= INET_SS_RCVATMARK;
                        break;
                    }
                }
            }
            if ((flags & 0x08U) != 0) {
                break;
            }
            while ((flags & 0x40U) != 0 && m == NULL && uio->uio_resid > 0 &&
                   (pr->pr_flags & INET_PR_ATOMIC) == 0 && nextrecord == NULL) {
                if (so->so_error != 0 || (so->so_state & INET_SS_CANTRCVMORE) != 0) {
                    break;
                }
                error = inetSbWait(&so->so_rcv);
                if (error != 0) {
                    inetSbUnlock(&so->so_rcv);
                    sceNet_lib_515B2F33(spl);
                    return 0;
                }
                m = so->so_rcv.sb_mb;
                if (m != NULL) {
                    nextrecord = m->m_nextpkt;
                }
            }
        }

        if (m != NULL && (pr->pr_flags & INET_PR_ATOMIC) != 0) {
            flags |= 0x10U;
            if ((flags & 2U) == 0) {
                inetSbDropRecord(&so->so_rcv);
            }
        }
        if ((flags & 2U) == 0) {
            if (m == NULL) {
                so->so_rcv.sb_mb = nextrecord;
            }
            if ((pr->pr_flags & INET_PR_WANTRCVD) != 0 && so->so_pcb != NULL) {
                pr->pr_usrreq(so, INET_PRU_RCVD, NULL, (InetMbuf *)(uintptr_t)flags, NULL, NULL);
            }
        }

        if (origResid == uio->uio_resid && origResid != 0 &&
            (flags & 0x08U) == 0 && (so->so_state & INET_SS_CANTRCVMORE) == 0) {
            inetSbUnlock(&so->so_rcv);
            sceNet_lib_515B2F33(spl);
            continue;
        }
        if (flagsp != NULL) {
            *flagsp |= flags;
        }
        break;
    }

    inetSbUnlock(&so->so_rcv);
    sceNet_lib_515B2F33(spl);
    return error;
}

void INET_NOINLINE inetSorFlush(InetSocket *so)
{
    InetSockBuf *sb = &so->so_rcv;
    InetProtosw *pr = so->so_proto;
    InetSockBuf savedSb;
    int spl;

    sb->sb_flags |= INET_SB_NOINTR;
    if ((sb->sb_flags & INET_SB_LOCK) != 0) {
        inetSbLock(sb);
    } else {
        sb->sb_flags |= INET_SB_LOCK;
    }
    spl = sceNet_lib_94B44F26();
    inetSoCantRcvMore(so);
    inetSbUnlock(sb);
    savedSb = *sb;
    sceKernelMemset(sb, 0, sizeof(*sb));
    sceNet_lib_515B2F33(spl);

    if ((pr->pr_flags & INET_PR_RIGHTS) != 0 && pr->pr_domain->dom_dispose != NULL) {
        pr->pr_domain->dom_dispose(savedSb.sb_mb);
    }
    inetSbRelease(&savedSb);
}

int INET_NOINLINE inetSoSetopt(InetSocket *so, int level, int optname, InetMbuf *m)
{
    int error = 0;
    InetMbuf *optMbuf = m;

    if (level != INET_SOL_SOCKET) {
        if (so->so_kermit_id == -1) {
            if (so->so_proto != NULL && so->so_proto->pr_ctloutput != NULL) {
                return so->so_proto->pr_ctloutput(INET_PRCO_SETOPT, so, level, optname, &optMbuf);
            }
            error = INET_ENOPROTOOPT;
        }
    } else {
        switch (optname) {
        case INET_SO_DEBUG:
        case INET_SO_REUSEADDR:
        case INET_SO_KEEPALIVE:
        case INET_SO_DONTROUTE:
        case INET_SO_BROADCAST:
        case INET_SO_USELOOPBACK:
        case INET_SO_OOBINLINE:
        case INET_SO_REUSEPORT:
        case INET_SO_TIMESTAMP:
        case INET_SO_ONESBCAST:
        case 0x1000:
        case 0x2000:
            if (m == NULL || m->m_len < 4) {
                error = INET_EINVAL;
            } else {
                const s32 *val = (const s32 *)m->m_data;
                if (*val != 0) {
                    so->so_options |= (u16)optname;
                } else {
                    so->so_options &= (u16)~optname;
                }
            }
            break;

        case INET_SO_LINGER:
            error = INET_EINVAL;
            break;

        case INET_SO_SNDBUF:
        case INET_SO_RCVBUF:
        case INET_SO_SNDLOWAT:
        case INET_SO_RCVLOWAT:
            if (m == NULL || m->m_len < 4) {
                error = INET_EINVAL;
            } else {
                s32 val = *(const s32 *)m->m_data;
                if (val <= 0) {
                    error = INET_EINVAL;
                } else if (optname == INET_SO_SNDBUF) {
                    if (inetSbReserve(&so->so_snd, (u32)val) == 0) {
                        error = INET_ENOBUFS;
                    }
                } else if (optname == INET_SO_RCVBUF) {
                    if (inetSbReserve(&so->so_rcv, (u32)val) == 0) {
                        error = INET_ENOBUFS;
                    }
                } else if (optname == INET_SO_SNDLOWAT) {
                    so->so_snd.sb_lowat = ((u32)val > so->so_snd.sb_hiwat) ? (s32)so->so_snd.sb_hiwat : val;
                } else {
                    so->so_rcv.sb_lowat = ((u32)val > so->so_rcv.sb_hiwat) ? (s32)so->so_rcv.sb_hiwat : val;
                }
            }
            break;

        case INET_SO_SNDTIMEO:
        case INET_SO_RCVTIMEO:
            if (m == NULL || m->m_len < 4) {
                error = INET_EINVAL;
            } else {
                u32 usec = *(const u32 *)m->m_data;
                if (optname == INET_SO_SNDTIMEO) {
                    so->so_snd.sb_timeo = (s32)(usec / 10000U);
                } else {
                    so->so_rcv.sb_timeo = (s32)(usec / 10000U);
                }
            }
            break;

        case INET_SO_NBIO:
            if (m == NULL || m->m_len < 4) {
                error = INET_EINVAL;
            } else if (*(const s32 *)m->m_data != 0) {
                so->so_state |= INET_SS_NBIO;
            } else {
                so->so_state &= (u16)~INET_SS_NBIO;
            }
            break;

        default:
            error = INET_ENOPROTOOPT;
            break;
        }

        if (error == 0 && so->so_proto != NULL && so->so_proto->pr_ctloutput != NULL &&
            so->so_kermit_id == -1) {
            so->so_proto->pr_ctloutput(INET_PRCO_SETOPT, so, INET_SOL_SOCKET, optname, &optMbuf);
            optMbuf = NULL;
        }
    }

    if (optMbuf != NULL) {
        sceNetIfhandle_lib_F8825DC4(optMbuf);
    }
    return error;
}

int INET_NOINLINE inetSoGetopt(InetSocket *so, int level, int optname, InetMbuf **mp)
{
    InetMbuf *m;
    s32 *val;

    if (level != INET_SOL_SOCKET) {
        if (so->so_kermit_id == -1) {
            if (so->so_proto != NULL && so->so_proto->pr_ctloutput != NULL) {
                return so->so_proto->pr_ctloutput(INET_PRCO_GETOPT, so, level, optname, mp);
            }
            return INET_ENOPROTOOPT;
        }
        return (int)(intptr_t)so;
    }

    m = sceNetIfhandle_lib_A493AA5F(INET_M_WAIT, INET_MT_SOOPTS);
    if (m == NULL) {
        return INET_ENOBUFS;
    }
    m->m_len = 4;
    val = (s32 *)m->m_data;

    switch (optname) {
    case INET_SO_LINGER:
        m->m_len = 8;
        val[0] = (s32)(so->so_options & INET_SO_LINGER);
        val[1] = so->so_linger;
        break;

    case INET_SO_DEBUG:
    case INET_SO_REUSEADDR:
    case INET_SO_KEEPALIVE:
    case INET_SO_DONTROUTE:
    case INET_SO_BROADCAST:
    case INET_SO_USELOOPBACK:
    case INET_SO_OOBINLINE:
    case INET_SO_REUSEPORT:
    case INET_SO_TIMESTAMP:
    case INET_SO_ONESBCAST:
    case 0x1000:
    case 0x2000:
        *val = (s32)(so->so_options & (u16)optname);
        break;

    case INET_SO_TYPE:
        *val = so->so_type;
        break;

    case INET_SO_ERROR:
        *val = (so->so_error != 0) ? (s32)((u32)so->so_error | 0x80010000U) : 0;
        so->so_error = 0;
        break;

    case INET_SO_SNDBUF:
        *val = (s32)so->so_snd.sb_hiwat;
        break;

    case INET_SO_RCVBUF:
        *val = (s32)so->so_rcv.sb_hiwat;
        break;

    case INET_SO_SNDLOWAT:
        *val = so->so_snd.sb_lowat;
        break;

    case INET_SO_RCVLOWAT:
        *val = so->so_rcv.sb_lowat;
        break;

    case INET_SO_SNDTIMEO:
        *val = so->so_snd.sb_timeo;
        break;

    case INET_SO_RCVTIMEO:
        *val = so->so_rcv.sb_timeo;
        break;

    case INET_SO_NBIO:
        *val = (s32)(so->so_state & INET_SS_NBIO);
        break;

    default:
        sceNetIfhandle_lib_F8825DC4(m);
        return INET_ENOPROTOOPT;
    }

    *mp = m;
    return 0;
}

int INET_NOINLINE inetSoPoll(InetSocket *so, int events, InetThreadInfo *ti)
{
    int revents = 0;
    int spl = sceNet_lib_94B44F26();

    if (so->so_kermit_id != -1) {
        if ((events & 0x41) != 0) {
            if (sceWlanDrv_lib_56AE09D2(so->so_kermit_id, 1) != 0) {
                revents = events & 0x41;
            }
        }
        if ((events & 0x05) != 0) {
            if (sceWlanDrv_lib_56AE09D2(so->so_kermit_id, 4) != 0) {
                revents |= (events & 0x04);
                if ((so->so_state & INET_SS_ISCONNECTING) != 0) {
                    so->so_state = (u16)((so->so_state & ~0x20EU) | INET_SS_ISCONNECTED);
                }
            }
        }
    } else {
        if ((events & 0x41) != 0) {
            if ((s32)so->so_rcv.sb_cc >= so->so_rcv.sb_lowat ||
                (so->so_state & INET_SS_CANTRCVMORE) != 0 ||
                so->so_qlen != 0 ||
                so->so_error != 0) {
                revents = events & 0x41;
            }
        }
        if ((events & 0x04) != 0) {
            if ((inetSbSpace(&so->so_snd) >= so->so_snd.sb_lowat &&
                 ((so->so_state & INET_SS_ISCONNECTED) != 0 ||
                  (so->so_proto->pr_flags & INET_PR_CONNREQUIRED) == 0)) ||
                (so->so_state & INET_SS_CANTSENDMORE) != 0 ||
                so->so_error != 0) {
                revents |= (events & 0x04);
            }
        }
    }

    if ((events & 0x82) != 0) {
        if (so->so_oobmark != 0 || (so->so_state & INET_SS_RCVATMARK) != 0) {
            revents |= (events & 0x82);
        }
    }

    if (revents == 0) {
        if ((events & 0xC3) != 0) {
            sceNet_lib_92633D8D(ti, &so->so_rcv.sb_sel_thid);
            so->so_rcv.sb_flags |= INET_SB_SEL;
        }
        if ((events & 0x04) != 0) {
            sceNet_lib_92633D8D(ti, &so->so_snd.sb_sel_thid);
            so->so_snd.sb_flags |= INET_SB_SEL;
        }
    }

    sceNet_lib_515B2F33(spl);
    return revents;
}

void INET_NOINLINE inetSoInit(void)
{
    sceWlanDrv_lib_78C84FEC();
    g_somaxconn = 128U;
    g_last_sock_id = 0;
    g_socket_list_head = NULL;
    g_socket_list_tail = &g_socket_list_head;
}

int INET_NOINLINE inetSoBind(InetSocket *so, InetMbuf *nam)
{
    int spl = sceNet_lib_94B44F26();
    int error = so->so_proto->pr_usrreq(so, INET_PRU_BIND, NULL, nam, NULL, NULL);
    sceNet_lib_515B2F33(spl);
    return error;
}

int INET_NOINLINE inetSoListen(InetSocket *so, s32 backlog)
{
    int spl = sceNet_lib_94B44F26();
    int error;

    if (so->so_kermit_id == -1) {
        error = so->so_proto->pr_usrreq(so, INET_PRU_LISTEN, NULL, NULL, NULL, NULL);
    } else {
        error = sceWlanDrv_lib_A2DA9314(so->so_kermit_id, backlog);
    }
    if (error != 0) {
        sceNet_lib_515B2F33(spl);
        return error;
    }

    if (so->so_q.tqh_first == NULL) {
        so->so_options |= INET_SO_ACCEPTCONN;
    }
    if (backlog < 0) {
        backlog = 0;
    }
    so->so_qlimit = ((u32)backlog < g_somaxconn) ? (s16)backlog : (s16)g_somaxconn;
    sceNet_lib_515B2F33(spl);
    return 0;
}

int INET_NOINLINE inetSoAccept(InetSocket *so, InetMbuf *nam)
{
    int spl = sceNet_lib_94B44F26();
    int error = INET_ECONNABORTED;
    int aborted = (so->so_state & INET_SS_ABORTED) != 0;

    so->so_state &= (u16)~INET_SS_NOFDREF;
    if (!aborted) {
        error = so->so_proto->pr_usrreq(so, INET_PRU_ACCEPT, NULL, nam, NULL, NULL);
    }
    sceNet_lib_515B2F33(spl);
    return error;
}

int INET_NOINLINE inetSoConnect(InetSocket *so, InetMbuf *nam)
{
    int spl;
    int error;

    if ((so->so_options & INET_SO_ACCEPTCONN) != 0) {
        return INET_EOPNOTSUPP;
    }
    spl = sceNet_lib_94B44F26();
    if ((so->so_state & (INET_SS_ISCONNECTED | INET_SS_ISCONNECTING)) != 0) {
        if ((so->so_proto->pr_flags & INET_PR_CONNREQUIRED) != 0 || inetSoDisconnect(so) != 0) {
            sceNet_lib_515B2F33(spl);
            return INET_EISCONN;
        }
    }
    error = so->so_proto->pr_usrreq(so, INET_PRU_CONNECT, NULL, nam, NULL, NULL);
    sceNet_lib_515B2F33(spl);
    return error;
}

int INET_NOINLINE inetSoShutdown(InetSocket *so, u32 how)
{
    if (how >= 3U) {
        return INET_EINVAL;
    }
    if (so->so_kermit_id != -1) {
        return sceWlanDrv_lib_91E04A18(so->so_kermit_id, how);
    }
    if (how == 0U || how == 2U) {
        inetSorFlush(so);
    }
    if (how == 1U || how == 2U) {
        return so->so_proto->pr_usrreq(so, INET_PRU_SHUTDOWN, NULL, NULL, NULL, NULL);
    }
    return 0;
}

void INET_NOINLINE inetSoSelWakeup(InetSocket *so)
{
    sceNet_lib_B9C780C7(&so->so_rcv.sb_sel_thid);
}

int INET_NOINLINE inetSooIoctl(InetSocket *so, u32 cmd, void *data)
{
    u8 group = (u8)((cmd >> 8) & 0xFFU);

    if (so->so_kermit_id == -1) {
        if (group == 'i') {
            return sceNet_lib_13A8B98A(so, cmd, data);
        }
        if (group == 'r') {
            return inetRtIoctl(cmd, data, NULL);
        }
        return so->so_proto->pr_usrreq(so, INET_PRU_CONTROL,
                                       (InetMbuf *)(uintptr_t)cmd,
                                       (InetMbuf *)data, NULL, NULL);
    }
    if (group == 'i' || group == 'r') {
        return 0;
    }
    return sceWlanDrv_lib_51FF0D5F(so->so_kermit_id, cmd, data);
}
