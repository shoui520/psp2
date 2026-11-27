/*
 * BSD socket queue, buffer, and descriptor primitives (uipc_socket2.c).
 */
#include "pspnet_inet_internal.h"

void INET_NOINLINE inetSoIsConnected(InetSocket *so)
{
    InetSocket *head = so->so_head;

    so->so_state = (u16)((so->so_state & ~(INET_SS_ISCONNECTING | INET_SS_ISDISCONNECTING | INET_SS_ISDISCONNECTED)) |
                         INET_SS_ISCONNECTED);
    if (head != NULL && inetSoQremque(so, 0) != 0) {
        inetSoQinsque(head, so, 1);
        if ((head->so_rcv.sb_flags & 0x3CU) != 0) {
            inetSoWakeup(head, &head->so_rcv);
        }
        sceNet_lib_7574FDA1(&head->so_timeo);
    } else {
        sceNet_lib_7574FDA1(&so->so_timeo);
        if ((so->so_rcv.sb_flags & 0x3CU) != 0) {
            inetSoWakeup(so, &so->so_rcv);
        }
        if ((so->so_snd.sb_flags & 0x3CU) != 0) {
            inetSoWakeup(so, &so->so_snd);
        }
    }
}

InetSocket *INET_NOINLINE inetSoNewConn(InetSocket *head, int connstatus)
{
    int qtype = (connstatus != 0) ? 1 : 0;
    InetSocket *so;

    if ((head->so_qlen + head->so_q0len) > (3 * head->so_qlimit) / 2) {
        return NULL;
    }

    so = (InetSocket *)sceNetIfhandle_lib_15CFE3C0(sizeof(InetSocket));
    if (so == NULL) {
        return NULL;
    }

    sceKernelMemset(so, 0, sizeof(InetSocket));
    so->so_type = head->so_type;
    so->so_options = (u16)(head->so_options & ~INET_SO_ACCEPTCONN);
    so->so_linger = head->so_linger;
    so->so_state = (u16)(head->so_state | INET_SS_NOFDREF);
    so->so_proto = head->so_proto;
    so->so_timeo = head->so_timeo;
    so->so_send = head->so_send;
    so->so_receive = head->so_receive;
    so->so_sock_id = -1;

    inetSoReserve(so, head->so_snd.sb_hiwat, head->so_rcv.sb_hiwat);
    inetSoQinsque(head, so, qtype);
    if (so->so_proto->pr_usrreq(so, INET_PRU_ATTACH, NULL, NULL, NULL, NULL) != 0) {
        inetSoQremque(so, qtype);
        sceNetIfhandle_lib_76BAD213(so);
        return NULL;
    }

    if (connstatus != 0) {
        if ((head->so_rcv.sb_flags & 0x3CU) != 0) {
            inetSoWakeup(head, &head->so_rcv);
        }
        sceNet_lib_7574FDA1(&head->so_timeo);
        so->so_state |= (u16)connstatus;
    }

    so->so_list_next = NULL;
    so->so_list_prev = g_socket_list_tail;
    *g_socket_list_tail = so;
    g_socket_list_tail = &so->so_list_next;
    return so;
}

void INET_NOINLINE inetSoQinsque(InetSocket *head, InetSocket *so, int qtype)
{
    InetSocketQueue *queue;

    so->so_head = head;
    if (qtype == 0) {
        head->so_q0len++;
        queue = &head->so_q0;
    } else {
        head->so_qlen++;
        queue = &head->so_q;
    }
    so->so_onq = queue;
    so->so_qe_next = NULL;
    so->so_qe_prev = queue->tqh_last;
    *queue->tqh_last = so;
    queue->tqh_last = &so->so_qe_next;
}

int INET_NOINLINE inetSoQremque(InetSocket *so, int qtype)
{
    InetSocket *head = so->so_head;
    InetSocketQueue *queue = so->so_onq;

    if (qtype == 0) {
        if (queue != &head->so_q0) {
            return 0;
        }
        head->so_q0len--;
    } else {
        if (queue != &head->so_q) {
            return 0;
        }
        head->so_qlen--;
    }

    if (so->so_qe_next != NULL) {
        so->so_qe_next->so_qe_prev = so->so_qe_prev;
    } else {
        queue->tqh_last = so->so_qe_prev;
    }
    *so->so_qe_prev = so->so_qe_next;
    so->so_onq = NULL;
    so->so_head = NULL;
    return 1;
}

void INET_NOINLINE inetSoWakeup(InetSocket *so, InetSockBuf *sb)
{
    u16 flags;
    u16 wait;

    sceNet_lib_B9C780C7(&sb->sb_sel_thid);
    flags = sb->sb_flags;
    wait = (u16)(flags & INET_SB_WAIT);
    flags &= (u16)~INET_SB_SEL;
    sb->sb_flags = flags;
    if (wait != 0) {
        sb->sb_flags = (u16)(flags & ~INET_SB_WAIT);
        sceNet_lib_7574FDA1(sb);
    }
    if ((sb->sb_flags & INET_SB_UPCALL) != 0) {
        so->so_upcall(so, so->so_upcallarg, 1);
    }
}

int INET_NOINLINE inetSoReserve(InetSocket *so, u32 sndcc, u32 rcvcc)
{
    if (inetSbReserve(&so->so_snd, sndcc) == 0) {
        return INET_ENOBUFS;
    }
    if (inetSbReserve(&so->so_rcv, rcvcc) == 0) {
        inetSbRelease(&so->so_snd);
        return INET_ENOBUFS;
    }
    if (so->so_rcv.sb_lowat == 0) {
        so->so_rcv.sb_lowat = 1;
    }
    if (so->so_snd.sb_lowat == 0) {
        so->so_snd.sb_lowat = 2048;
    }
    if ((u32)so->so_snd.sb_lowat > so->so_snd.sb_hiwat) {
        so->so_snd.sb_lowat = (s32)so->so_snd.sb_hiwat;
    }
    return 0;
}

int INET_NOINLINE inetSbReserve(InetSockBuf *sb, u32 cc)
{
    u32 mbmax = (cc * 2U < 0x40000U) ? (cc * 2U) : 0x40000U;
    if (cc == 0U || cc > 0x38E38U) {
        return 0;
    }
    sb->sb_hiwat = cc;
    sb->sb_mbmax = mbmax;
    if ((u32)sb->sb_lowat > cc) {
        sb->sb_lowat = (s32)cc;
    }
    return 1;
}

void INET_NOINLINE inetSbRelease(InetSockBuf *sb)
{
    inetSbFlush(sb);
    sb->sb_hiwat = 0;
    sb->sb_mbmax = 0;
}

int INET_NOINLINE inetSbAppendAddr(InetSockBuf *sb, const InetSockaddr *asa,
                                   InetMbuf *mbuf, InetMbuf *control)
{
    s32 totalLen = (s32)asa->sa_len;
    InetMbuf *n = control;
    InetMbuf *lastCtrl = NULL;
    InetMbuf *addrMbuf;
    InetMbuf *scan;
    s32 ccSpace;
    s32 mbSpace;
    s32 avail;

    if (mbuf != NULL) {
        totalLen += M_PKTLEN(mbuf);
    }
    while (n != NULL) {
        totalLen += n->m_len;
        lastCtrl = n;
        n = n->m_next;
    }

    ccSpace = (s32)(sb->sb_hiwat - sb->sb_cc);
    mbSpace = (s32)(sb->sb_mbmax - sb->sb_mbcnt);
    avail = (ccSpace < mbSpace) ? ccSpace : mbSpace;
    if (totalLen > avail) {
        return 0;
    }

    addrMbuf = (InetMbuf *)sceNetIfhandle_lib_15CFE3C0(sizeof(InetMbuf));
    if (addrMbuf == NULL) {
        return 0;
    }
    addrMbuf->m_type = INET_MT_SONAME;
    addrMbuf->m_data = M_DAT(addrMbuf);
    addrMbuf->m_next = NULL;
    addrMbuf->m_nextpkt = NULL;
    addrMbuf->m_flags = 0;

    if (asa->sa_len >= 237U) {
        u8 *ext = (u8 *)sceNetIfhandle_lib_15CFE3C0(asa->sa_len);
        M_EXT(addrMbuf).ext_buf = ext;
        if (ext != NULL) {
            addrMbuf->m_data = ext;
            addrMbuf->m_flags = (u16)((addrMbuf->m_flags & ~(INET_M_EXT | INET_M_CLUSTER)) | INET_M_EXT);
            M_EXT(addrMbuf).ext_free = NULL;
            M_EXT(addrMbuf).ext_arg = NULL;
            M_EXT(addrMbuf).ext_size = asa->sa_len;
            M_EXT(addrMbuf).ext_type = (u32)addrMbuf->m_type;
            M_EXT(addrMbuf).ext_nextref = addrMbuf;
            M_EXT(addrMbuf).ext_prevref = addrMbuf;
        }
        if ((addrMbuf->m_flags & INET_M_EXT) == 0) {
            sceNetIfhandle_lib_F8825DC4(addrMbuf);
            return 0;
        }
    }

    addrMbuf->m_len = asa->sa_len;
    sceKernelMemcpy(addrMbuf->m_data, asa, asa->sa_len);
    if (lastCtrl != NULL) {
        lastCtrl->m_next = mbuf;
        addrMbuf->m_next = control;
    } else {
        addrMbuf->m_next = mbuf;
    }

    for (scan = addrMbuf; scan != NULL; scan = scan->m_next) {
        sb->sb_cc += (u32)scan->m_len;
        sb->sb_mbcnt += 256U;
        if ((scan->m_flags & INET_M_EXT) != 0) {
            sb->sb_mbcnt += M_EXT(scan).ext_size;
        }
    }

    if (sb->sb_mb == NULL) {
        sb->sb_mb = addrMbuf;
    } else {
        InetMbuf *tail = sb->sb_mb;
        while (tail->m_nextpkt != NULL) {
            tail = tail->m_nextpkt;
        }
        tail->m_nextpkt = addrMbuf;
    }
    return 1;
}

void INET_NOINLINE inetSbCompress(InetSockBuf *sb, InetMbuf *m, InetMbuf *n)
{
    u16 eor = 0;

    while (m != NULL) {
        eor |= (u16)(m->m_flags & INET_M_EOR);
        if (m->m_len == 0 && (eor == 0 ||
            (((m->m_next != NULL) ? m->m_next : n) != NULL &&
             (((m->m_next != NULL) ? m->m_next : n)->m_type == m->m_type)))) {
            m = sceNetIfhandle_lib_F8825DC4(m);
            continue;
        }
        if (n != NULL && (n->m_flags & INET_M_EOR) == 0 && n->m_type == m->m_type) {
            int canMerge = 0;
            if ((n->m_flags & INET_M_EXT) == 0) {
                if ((uintptr_t)(n->m_data + n->m_len + m->m_len) <= (uintptr_t)((u8 *)n + sizeof(InetMbuf))) {
                    canMerge = 1;
                }
            } else if ((n->m_flags & (INET_M_EXT | INET_M_CLUSTER)) == (INET_M_EXT | INET_M_CLUSTER) &&
                       M_EXT(n).ext_nextref == n &&
                       (uintptr_t)(n->m_data + n->m_len + m->m_len) <= (uintptr_t)(M_EXT(n).ext_buf + 2048U)) {
                canMerge = 1;
            }
            if (canMerge) {
                sceKernelMemcpy(n->m_data + n->m_len, m->m_data, (SceSize)m->m_len);
                n->m_len += m->m_len;
                sb->sb_cc += (u32)m->m_len;
                m = sceNetIfhandle_lib_F8825DC4(m);
                continue;
            }
        }
        if (n != NULL) {
            n->m_next = m;
        } else {
            sb->sb_mb = m;
        }
        sb->sb_cc += (u32)m->m_len;
        sb->sb_mbcnt += 256U;
        if ((m->m_flags & INET_M_EXT) != 0) {
            sb->sb_mbcnt += M_EXT(m).ext_size;
        }
        n = m;
        m->m_flags &= (u16)~INET_M_EOR;
        m = m->m_next;
        n->m_next = NULL;
    }

    if (eor != 0 && n != NULL) {
        n->m_flags |= eor;
    }
}

void INET_NOINLINE inetSbFlush(InetSockBuf *sb)
{
    while (sb->sb_mbcnt != 0) {
        inetSbDrop(sb, (int)sb->sb_cc);
    }
}

void INET_NOINLINE inetSbDrop(InetSockBuf *sb, int len)
{
    InetMbuf *m = sb->sb_mb;
    InetMbuf *next = (m != NULL) ? m->m_nextpkt : NULL;

    while (len > 0) {
        if (m == NULL) {
            if (next == NULL) {
                break;
            }
            m = next;
            next = m->m_nextpkt;
            continue;
        }
        if (m->m_len > len) {
            m->m_len -= len;
            m->m_data += len;
            sb->sb_cc -= (u32)len;
            break;
        }
        len -= m->m_len;
        sb->sb_cc -= (u32)m->m_len;
        sb->sb_mbcnt -= 256U;
        if ((m->m_flags & INET_M_EXT) != 0) {
            sb->sb_mbcnt -= M_EXT(m).ext_size;
        }
        m = inetMfreeOne(m);
    }

    while (m != NULL && m->m_len == 0) {
        sb->sb_mbcnt -= 256U;
        if ((m->m_flags & INET_M_EXT) != 0) {
            sb->sb_mbcnt -= M_EXT(m).ext_size;
        }
        m = inetMfreeOne(m);
    }

    if (m != NULL) {
        sb->sb_mb = m;
        m->m_nextpkt = next;
    } else {
        sb->sb_mb = next;
    }
}

void INET_NOINLINE inetSbDropRecord(InetSockBuf *sb)
{
    InetMbuf *m = sb->sb_mb;

    if (m != NULL) {
        sb->sb_mb = m->m_nextpkt;
        do {
            sb->sb_cc -= (u32)m->m_len;
            sb->sb_mbcnt -= 256U;
            if ((m->m_flags & INET_M_EXT) != 0) {
                sb->sb_mbcnt -= M_EXT(m).ext_size;
            }
            m = inetMfreeOne(m);
        } while (m != NULL);
    }
}

InetMbuf *INET_NOINLINE inetSbCreateControl(const void *p, int size, int type, int level)
{
    u32 mask = inetCmsgAlignMask();
    u32 hdrAligned = (12U + mask) & ~mask;
    u32 dataAligned = ((u32)size + mask) & ~mask;
    InetMbuf *m;
    InetCmsghdr *cp;

    if (hdrAligned + dataAligned > 2048U) {
        return NULL;
    }

    m = sceNetIfhandle_lib_A493AA5F(INET_M_DONTWAIT, INET_MT_CONTROL);
    if (m == NULL) {
        return NULL;
    }

    if (hdrAligned + dataAligned >= 237U) {
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
        if ((m->m_flags & INET_M_EXT) == 0) {
            sceNetIfhandle_lib_F8825DC4(m);
            return NULL;
        }
    }

    cp = (InetCmsghdr *)m->m_data;
    sceKernelMemcpy(m->m_data + hdrAligned, p, (SceSize)size);
    m->m_len = (s32)(hdrAligned + dataAligned);
    cp->cmsg_len = hdrAligned + (u32)size;
    cp->cmsg_level = level;
    cp->cmsg_type = type;
    return m;
}

int INET_NOINLINE inetSockAllocId(void)
{
    s32 cand = (g_last_sock_id == 255) ? 1 : (g_last_sock_id + 1);

    if (cand == g_last_sock_id) {
        return -1;
    }

    for (;;) {
        int spl;
        InetSocket *so;

        if ((u32)(cand - 1) >= 255U) {
            break;
        }
        spl = sceNet_lib_94B44F26();
        so = g_socket_list_head;
        while (so != NULL) {
            if (so->so_sock_id == cand) {
                break;
            }
            so = so->so_list_next;
        }
        sceNet_lib_515B2F33(spl);
        if (so == NULL) {
            break;
        }
        cand = (cand == 255) ? 1 : (cand + 1);
        if (cand == g_last_sock_id) {
            return -1;
        }
    }

    g_last_sock_id = cand;
    return cand;
}

InetSocket *INET_NOINLINE inetSockFind(int sockId)
{
    int spl;
    InetSocket *so;

    if ((u32)(sockId - 1) >= 255U) {
        return NULL;
    }
    spl = sceNet_lib_94B44F26();
    so = g_socket_list_head;
    while (so != NULL) {
        if (so->so_sock_id == sockId) {
            break;
        }
        so = so->so_list_next;
    }
    sceNet_lib_515B2F33(spl);
    return so;
}

void INET_NOINLINE inetSoIsConnecting(InetSocket *so)
{
    so->so_state = (u16)((so->so_state & ~(INET_SS_ISCONNECTED | INET_SS_ISDISCONNECTING)) |
                         INET_SS_ISCONNECTING);
}

void INET_NOINLINE inetSoIsDisconnected(InetSocket *so)
{
    so->so_state = (u16)((so->so_state & ~(INET_SS_ISCONNECTING | INET_SS_ISCONNECTED | INET_SS_ISDISCONNECTING)) |
                         (INET_SS_CANTRCVMORE | INET_SS_CANTSENDMORE | INET_SS_ISDISCONNECTING));
    sceNet_lib_7574FDA1(&so->so_timeo);
    if ((so->so_snd.sb_flags & 0x3CU) != 0) {
        inetSoWakeup(so, &so->so_snd);
    }
    if ((so->so_rcv.sb_flags & 0x3CU) != 0) {
        inetSoWakeup(so, &so->so_rcv);
    }
}

void INET_NOINLINE inetSoIsAborted(InetSocket *so)
{
    so->so_state = (u16)((so->so_state & ~(INET_SS_ISCONNECTED | INET_SS_ISCONNECTING | INET_SS_ISDISCONNECTING)) |
                         (INET_SS_ABORTED | INET_SS_CANTRCVMORE | INET_SS_CANTSENDMORE));
    sceNet_lib_7574FDA1(&so->so_timeo);
    if ((so->so_snd.sb_flags & 0x3CU) != 0) {
        inetSoWakeup(so, &so->so_snd);
    }
    if ((so->so_rcv.sb_flags & 0x3CU) != 0) {
        inetSoWakeup(so, &so->so_rcv);
    }
}

void INET_NOINLINE inetSoCantSendMore(InetSocket *so)
{
    so->so_state |= INET_SS_CANTSENDMORE;
    if ((so->so_snd.sb_flags & 0x3CU) != 0) {
        inetSoWakeup(so, &so->so_snd);
    }
}

void INET_NOINLINE inetSoCantRcvMore(InetSocket *so)
{
    so->so_state |= INET_SS_CANTRCVMORE;
    if ((so->so_rcv.sb_flags & 0x3CU) != 0) {
        inetSoWakeup(so, &so->so_rcv);
    }
}

int INET_NOINLINE inetSbWait(InetSockBuf *sb)
{
    sb->sb_flags |= INET_SB_WAIT;
    return sceNet_lib_205E8D17(sb, (u32)sb->sb_timeo);
}

int INET_NOINLINE inetSbLock(InetSockBuf *sb)
{
    while ((sb->sb_flags & INET_SB_LOCK) != 0) {
        int error;
        sb->sb_flags |= INET_SB_WANT;
        error = sceNet_lib_205E8D17(&sb->sb_flags, 0);
        if (error != 0) {
            return error;
        }
    }
    sb->sb_flags |= INET_SB_LOCK;
    return 0;
}

void INET_NOINLINE inetSbAppend(InetSockBuf *sb, InetMbuf *m)
{
    InetMbuf *n;

    if (m == NULL) {
        return;
    }

    n = sb->sb_mb;
    if (n != NULL) {
        while (n->m_nextpkt != NULL) {
            n = n->m_nextpkt;
        }
        while ((n->m_flags & INET_M_EOR) == 0 && n->m_next != NULL) {
            n = n->m_next;
        }
        if ((n->m_flags & INET_M_EOR) != 0) {
            InetMbuf *tail = sb->sb_mb;
            InetMbuf *next;
            while (tail->m_nextpkt != NULL) {
                tail = tail->m_nextpkt;
            }
            sb->sb_cc += (u32)m->m_len;
            sb->sb_mbcnt += 256U;
            if ((m->m_flags & INET_M_EXT) != 0) {
                sb->sb_mbcnt += M_EXT(m).ext_size;
            }
            tail->m_nextpkt = m;
            next = m->m_next;
            m->m_next = NULL;
            if (next != NULL && (m->m_flags & INET_M_EOR) != 0) {
                m->m_flags &= (u16)~INET_M_EOR;
                next->m_flags |= INET_M_EOR;
            }
            inetSbCompress(sb, next, m);
            return;
        }
    }

    inetSbCompress(sb, m, n);
}

u32 INET_NOINLINE inetCmsgAlignMask(void)
{
    return 3U;
}

int INET_NOINLINE inetSockCreateInternal(int domain, int type, int protocol)
{
    int error = 0;
    InetThreadInfo *ti;
    InetSocket *so = NULL;
    int spl;
    int sockId;

    ti = sceNet_lib_CA3CF5EB(&error);
    if (ti == NULL) {
        return -1;
    }

    spl = sceNet_lib_94B44F26();
    sockId = inetSockAllocId();
    if (sockId < 0) {
        error = INET_EMFILE;
    } else {
        error = inetSoCreate(domain, &so, type, protocol);
        if (error == 0) {
            so->so_sock_id = sockId;
        }
    }
    sceNet_lib_515B2F33(spl);

    if (sceNet_lib_757085B0(ti, error) >= 0) {
        return so->so_sock_id;
    }
    return -1;
}
