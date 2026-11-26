#include "adhoc_int.h"

static struct {
    PdpPcb *head;
    PdpPcb **tail;
} g_pdp_list;

static u16 g_pdp_anon_port;
static u16 g_pdp_seq;

static struct {
    u32 rx_ok;
    u32 rx_drop_full;
    u32 rx_frags;
    u32 rx_frag_err;
    u32 rx_no_port;
    u32 rx_frag_timeout;
} g_pdp_stats;

static s32 pdpSendInternal(AdhocSocket *sock, const u8 *dst_mac, u16 dport, const void *buf, s32 len, u32 timeout, s32 flags);
static s32 pdpRecvInternal(AdhocSocket *sock, u8 *src_mac, u16 *sport, void *buf, s32 *len, u32 timeout, s32 flags, PdpRecvExtra *extra);
static s32 pdpRecvById(s32 id, u8 *src_mac, u16 *sport, void *buf, s32 *len, u32 timeout, s32 flags, PdpRecvExtra *extra);
static void pdpFlushQueues(PdpPcb *pdp);
static s32 pdpAllocPcb(AdhocSocket *sock);
static s32 pdpBindPcb(PdpPcb *pdp, const u8 *mac, u16 port);
static s32 pdpSetBufSize(PdpPcb *pdp, u32 bufsize);
static s32 pdpFragTimeout(PdpFragEntry *frag);

static inline void pdpUnlinkWaiter(AdhocSocket *sock, AdhocWaiter *waiter)
{
    AdhocWaiter *cur = sock->wait_list;
    if (cur == waiter) {
        sock->wait_list = waiter->next;
    } else {
        while (cur->next != waiter)
            cur = cur->next;
        cur->next = waiter->next;
    }
}

/* 0x001130 */
s32 sceNetAdhocPdpCreate(const u8 *mac, u16 port, u32 bufsize, s32 flags)
{
    u32 delay;
    s32 intr;
    s32 ret;
    s32 id;
    AdhocSocket *sock = NULL;

    if (_sceKernelCheckThreadStack() < 2000)
        return ADHOC_ERR_STACK_SMALL;
    if (mac == NULL || flags != 0)
        return ADHOC_ERR_INVALID_ARG;

    delay = sceNetIfhandle_lib_07505747(3);
    if (delay != 0)
        sceKernelDelayThread(delay);

    intr = sceNet_lib_94B44F26();
    if (intr < 0)
        return intr;

    ret = ADHOC_ERR_NOT_INITIALIZED;
    if (g_adhoc_inited != 0) {
        id = adhocSockAllocId();
        ret = ADHOC_ERR_SOCKET_ID_NOT_AVAIL;
        if (id >= 0) {
            ret = adhocSockCreate(1, &sock);
            if (ret >= 0) {
                ret = pdpAllocPcb(sock);
                if (ret >= 0) {
                    ret = pdpSetBufSize((PdpPcb *)sock->pcb, bufsize);
                    if (ret >= 0) {
                        ret = pdpBindPcb((PdpPcb *)sock->pcb, mac, port);
                        if (ret >= 0) {
                            sock->id = id;
                            sceNet_lib_515B2F33(intr);
                            return id;
                        }
                        pdpFlushQueues((PdpPcb *)sock->pcb);
                    }
                    pdpFreePcb(sock);
                }
                adhocSockTryFree(sock);
            }
        }
    }

    sceNet_lib_515B2F33(intr);
    return ret;
}

/* 0x0012ac */
s32 sceNetAdhocPdpSend(s32 id, const u8 *dst_mac, u16 dport, const void *buf, s32 len, u32 timeout, s32 flags)
{
    s32 err = 0;
    AdhocWaiter *waiter;
    s32 intr;

    if (_sceKernelCheckThreadStack() < 1984)
        return ADHOC_ERR_STACK_SMALL;
    if (dst_mac == NULL || (flags & ~1) != 0)
        return ADHOC_ERR_INVALID_ARG;

    waiter = (AdhocWaiter *)sceNet_lib_CA3CF5EB(&err);
    if (waiter == NULL)
        return err;

    intr = sceNet_lib_94B44F26();
    if (intr < 0) {
        err = intr;
    } else if (g_adhoc_inited == 0) {
        err = ADHOC_ERR_NOT_INITIALIZED;
    } else {
        AdhocSocket *sock = adhocSockFind(1, id);
        if (sock == NULL) {
            err = ADHOC_ERR_INVALID_SOCKET_ID;
        } else {
            waiter->next = sock->wait_list;
            sock->wait_list = waiter;
            err = pdpSendInternal(sock, dst_mac, dport, buf, len, timeout, flags);
            pdpUnlinkWaiter(sock, waiter);
            if ((sock->flags & 1u) != 0) {
                if (sock->wait_list == NULL) {
                    pdpFreePcb(sock);
                    adhocSockTryFree(sock);
                }
                err = ADHOC_ERR_SOCKET_DELETED;
            }
        }
    }

    sceNet_lib_515B2F33(intr);
    sceNet_lib_757085B0(waiter, 0);
    return err;
}

/* 0x001470 */
s32 sceNetAdhocPdpDelete(s32 id, s32 flags)
{
    s32 ret = 0;
    s32 intr;

    if (_sceKernelCheckThreadStack() < 2016)
        return ADHOC_ERR_STACK_SMALL;
    if (flags != 0)
        return ADHOC_ERR_INVALID_ARG;

    intr = sceNet_lib_94B44F26();
    if (intr < 0)
        return intr;

    if (g_adhoc_inited == 0) {
        ret = ADHOC_ERR_NOT_INITIALIZED;
    } else {
        AdhocSocket *sock = adhocSockFind(1, id);
        if (sock == NULL) {
            ret = ADHOC_ERR_INVALID_SOCKET_ID;
        } else {
            PdpPcb *pdp = (PdpPcb *)sock->pcb;
            AdhocIfnet *ifp;

            if ((pdp->rx_wait_flags & 1u) != 0) {
                pdp->rx_wait_flags &= ~1u;
                sceNet_lib_7574FDA1(&pdp->rx_head);
            }
            if ((pdp->rx_wait_flags & 2u) != 0) {
                pdp->rx_wait_flags &= ~2u;
                sceNet_lib_7574FDA1(&g_global_wait_head);
            }

            ifp = (AdhocIfnet *)sceNet_lib_D60225A3(pdp->mac);
            if (ifp != NULL) {
                if ((ifp->snd_wait_flags & 1u) != 0) {
                    ifp->snd_wait_flags &= ~1u;
                    sceNet_lib_7574FDA1(&ifp->snd_head);
                }
                if ((ifp->snd_wait_flags & 2u) != 0) {
                    ifp->snd_wait_flags &= ~2u;
                    sceNet_lib_7574FDA1(&g_global_wait_head);
                }
            } else {
                sceNet_lib_7574FDA1(sock->chan_tx);
                sceNet_lib_7574FDA1(&g_global_wait_head);
            }

            pdpFlushQueues(pdp);
            sock->flags |= 1u;
            sock->id = -1;
            if (sock->wait_list == NULL) {
                pdpFreePcb(sock);
                adhocSockTryFree(sock);
            }
        }
    }

    sceNet_lib_515B2F33(intr);
    return ret;
}

/* 0x001620 */
static s32 __attribute__((noinline))
pdpSendInternal(AdhocSocket *sock, const u8 *dst_mac, u16 dport, const void *buf, s32 len, u32 timeout, s32 flags)
{
    PdpPcb *pdp = (PdpPcb *)sock->pcb;
    const u8 *src_bytes = (const u8 *)buf;
    s32 is_bcast = 0;
    u16 seq;
    u16 be_dport;
    u16 be_seq;
    s32 is_empty;
    u32 offset = 0;
    s32 rem;
    AdhocMbuf *chain_head = NULL;
    AdhocMbuf **chain_tail = NULL;
    s32 ret = 0;

    if (sceNet_lib_E0A81C7C(dst_mac, g_zero_mac, 6) == 0)
        return ADHOC_ERR_INVALID_ADDR;

    if ((dst_mac[0] & 1u) != 0) {
        is_bcast = 1;
        if (sceNet_lib_E0A81C7C(dst_mac, g_bcast_mac, 6) != 0)
            return ADHOC_ERR_INVALID_ADDR;
    }

    if (dport == 0 || dport == 0xFFFF)
        return ADHOC_ERR_INVALID_PORT;
    if ((u32)len > 0xFFF3u || (len != 0 && buf == NULL))
        return ADHOC_ERR_INVALID_BUFLEN;

    seq = g_pdp_seq++;
    be_dport = adhocSwap16(dport);
    be_seq = adhocSwap16(seq);
    is_empty = (len == 0);
    rem = len;

    for (;;) {
        s32 chunk = (rem < 1444) ? rem : 1444;
        s32 pad_len = (chunk + 12) % 16;
        AdhocMbuf *mbuf;
        u8 *hdr;
        u16 be_word;
        u16 next_off;

        mbuf = (AdhocMbuf *)sceNetIfhandle_lib_15CFE3C0(sizeof(AdhocMbuf));
        if (mbuf == NULL) {
            ret = ADHOC_ERR_NO_MEM;
            break;
        }
        mbuf->m_type = 1;
        mbuf->m_data = mbuf->payload.inline_buf;
        mbuf->m_flags = 2;
        mbuf->m_next = NULL;
        mbuf->m_nextpkt = NULL;
        mbuf->m_pkthdr_aux = 0;

        if (pad_len > 0)
            pad_len = 16 - pad_len;

        if (chunk + pad_len + 60 >= 209) {
            void *ext = sceNetIfhandle_lib_15CFE3C0(2048);
            if (ext == NULL)
                ext = sceNetIfhandle_lib_15CFE3C0(2048);
            mbuf->payload.ext.ext_buf = ext;
            if (ext != NULL) {
                mbuf->m_data = (u8 *)ext;
                mbuf->m_flags |= 9u;
                mbuf->payload.ext.ext_size = 2048;
                mbuf->payload.ext.ext_free_fn = 0;
                mbuf->payload.ext.ext_arg = 0;
                mbuf->payload.ext.ext_prevref = mbuf;
                mbuf->payload.ext.ext_nextref = mbuf;
            }
            if ((mbuf->m_flags & 1u) == 0) {
                sceNetIfhandle_lib_F8825DC4(mbuf);
                ret = ADHOC_ERR_NO_MEM;
                break;
            }
        }

        hdr = mbuf->m_data;
        sceKernelMemcpy(hdr, dst_mac, 6);
        sceKernelMemcpy(hdr + 6, pdp->mac, 6);
        sceKernelMemcpy(hdr + 12, &g_adhoc_ethertype, 2);
        sceKernelMemcpy(hdr + 14, &g_adhoc_hdr_ver, 2);

        hdr = mbuf->m_data;
        next_off = (u16)(offset + (u32)chunk);
        hdr[48] = 1;
        hdr[49] = (next_off != (u16)len) ? 1 : 0;
        be_word = adhocSwap16((u16)(chunk + 12));
        sceKernelMemcpy(hdr + 50, &be_word, 2);
        be_word = adhocSwap16(pdp->port);
        sceKernelMemcpy(hdr + 52, &be_word, 2);
        sceKernelMemcpy(hdr + 54, &be_dport, 2);
        sceKernelMemcpy(hdr + 56, &be_seq, 2);
        be_word = adhocSwap16((u16)offset);
        sceKernelMemcpy(hdr + 58, &be_word, 2);

        if (chunk > 0)
            sceKernelMemcpy(mbuf->m_data + 60, src_bytes + offset, (u32)chunk);
        if (pad_len > 0)
            sceKernelMemset(mbuf->m_data + 60 + chunk, 0, (u32)pad_len);

        mbuf->m_len = chunk + pad_len + 60;
        if (is_bcast != 0)
            mbuf->m_flags |= 0x100u;
        mbuf->m_rcvif = NULL;
        mbuf->m_pkthdr_len = mbuf->m_len;

        if (chain_head != NULL) {
            *chain_tail = mbuf;
            chain_tail = &mbuf->m_nextpkt;
        } else {
            chain_head = mbuf;
            chain_tail = &mbuf->m_nextpkt;
        }

        offset = next_off;
        rem = len - (s32)next_off;
        if ((s32)next_off >= len && (!is_empty || chain_head != NULL)) {
            s64 start_time = 0;
            for (;;) {
                AdhocIfnet *ifp;
                u32 wait_rem = 0;
                s32 wait_err;

                if ((sock->flags & 0x10u) != 0) {
                    ret = ADHOC_ERR_SOCKET_ALERTED;
                    break;
                }
                ifp = (AdhocIfnet *)sceNet_lib_D60225A3(pdp->mac);
                if (ifp == NULL) {
                    ret = 0;
                    break;
                }
                if (ifp->snd_len < ifp->snd_maxlen) {
                    AdhocMbuf *pkt = chain_head;
                    chain_head = pkt->m_nextpkt;
                    pkt->m_nextpkt = NULL;
                    ifp->if_obytes += (u64)(s64)pkt->m_pkthdr_len;
                    adhocIfOutput(ifp, pkt);
                    if (chain_head == NULL)
                        return 0;
                    continue;
                }
                if ((flags & 1) != 0) {
                    ret = ADHOC_ERR_WOULD_BLOCK;
                    break;
                }
                if (timeout != 0) {
                    s64 now;
                    if (start_time == 0)
                        start_time = sceKernelGetSystemTimeWide();
                    now = sceKernelGetSystemTimeWide();
                    if ((u64)(now - start_time) >= (u64)timeout) {
                        ret = ADHOC_ERR_TIMEOUT;
                        break;
                    }
                    wait_rem = timeout - (u32)(now - start_time);
                }

                ifp->snd_wait_flags |= 1u;
                wait_err = sceNet_lib_F6DB0A0B(&ifp->snd_head, wait_rem);
                if ((sock->flags & 0x10u) != 0)
                    wait_err = 5;
                if ((sock->flags & 1u) != 0)
                    wait_err = 32;
                if (wait_err != 0) {
                    if (wait_err == 4)
                        ret = ADHOC_ERR_THREAD_ABORTED;
                    else if (wait_err == 5)
                        ret = ADHOC_ERR_SOCKET_ALERTED;
                    else if (wait_err == 11)
                        ret = ADHOC_ERR_TIMEOUT;
                    else if (wait_err == 32)
                        ret = ADHOC_ERR_SOCKET_DELETED;
                    else
                        ret = ADHOC_ERR_INTERNAL;
                    break;
                }
            }
            break;
        }
    }

    while (chain_head != NULL) {
        AdhocMbuf *next = chain_head->m_nextpkt;
        chain_head->m_nextpkt = NULL;
        sceNetIfhandle_lib_F56FAC82(chain_head);
        chain_head = next;
    }
    return ret;
}

/* 0x001c48 */
static s32 __attribute__((noinline))
pdpRecvInternal(AdhocSocket *sock, u8 *src_mac, u16 *sport, void *buf, s32 *len, u32 timeout, s32 flags, PdpRecvExtra *extra)
{
    s64 start_time = 0;
    PdpPcb *pdp = (PdpPcb *)sock->pcb;
    AdhocMbuf *mbuf;
    u8 *pkt;
    u16 raw_word;
    s32 payload_len;

    for (;;) {
        u32 wait_rem = 0;
        s32 wait_err;

        if ((sock->flags & 0x20u) != 0)
            return ADHOC_ERR_SOCKET_ALERTED;
        mbuf = pdp->rx_head;
        if (mbuf != NULL)
            break;
        if ((flags & 1) != 0)
            return ADHOC_ERR_WOULD_BLOCK;

        if (timeout != 0) {
            s64 now;
            if (start_time == 0)
                start_time = sceKernelGetSystemTimeWide();
            now = sceKernelGetSystemTimeWide();
            if ((u64)(now - start_time) >= (u64)timeout) {
                sceNetIfhandle_lib_F56FAC82(NULL);
                return ADHOC_ERR_TIMEOUT;
            }
            wait_rem = timeout - (u32)(now - start_time);
        }

        pdp->rx_wait_flags |= 1u;
        wait_err = sceNet_lib_F6DB0A0B(&pdp->rx_head, wait_rem);
        if ((sock->flags & 0x20u) != 0)
            wait_err = 5;
        if ((sock->flags & 1u) != 0)
            wait_err = 32;
        if (wait_err != 0) {
            switch (wait_err) {
            case 4:
                return ADHOC_ERR_THREAD_ABORTED;
            case 5:
                return ADHOC_ERR_SOCKET_ALERTED;
            case 11:
                return ADHOC_ERR_TIMEOUT;
            case 32:
                return ADHOC_ERR_SOCKET_DELETED;
            default:
                return ADHOC_ERR_INTERNAL;
            }
        }
    }

    pdp->rx_head = mbuf->m_nextpkt;
    if (pdp->rx_head == NULL)
        pdp->rx_tail = NULL;
    mbuf->m_nextpkt = NULL;
    pdp->rx_cc = pdp->rx_cc - (u32)mbuf->m_pkthdr_len + 60u;

    pkt = mbuf->m_data;
    if (src_mac != NULL) {
        sceKernelMemcpy(src_mac, pkt + 6, 6);
        pkt = mbuf->m_data;
    }
    sceKernelMemcpy(&raw_word, pkt + 52, 2);
    if (sport != NULL)
        *sport = adhocSwap16(raw_word);

    sceKernelMemcpy(&raw_word, pkt + 50, 2);
    payload_len = (s32)adhocSwap16(raw_word) - 12;
    if (*len < payload_len) {
        mbuf->m_nextpkt = pdp->rx_head;
        if (pdp->rx_tail == NULL)
            pdp->rx_tail = mbuf;
        pdp->rx_head = mbuf;
        pdp->rx_cc = pdp->rx_cc + (u32)mbuf->m_pkthdr_len - 60u;
        *len = payload_len;
        return ADHOC_ERR_BUFFER_TOO_SMALL;
    }

    *len = payload_len;
    if (payload_len > 0)
        sceNetIfhandle_lib_9A6261EC(mbuf, 60, payload_len, buf);
    if (extra != NULL) {
        extra->rssi = mbuf->hdr_meta.meta.rssi;
        extra->rate = mbuf->hdr_meta.meta.rate;
    }
    sceNetIfhandle_lib_F56FAC82(mbuf);
    return 0;
}

/* 0x001f30 */
s32 sceNetAdhocPdpRecv(s32 id, u8 *src_mac, u16 *sport, void *buf, s32 *len, u32 timeout, s32 flags)
{
    if (_sceKernelCheckThreadStack() < 2016)
        return ADHOC_ERR_STACK_SMALL;
    if (buf == NULL || len == NULL || (flags & ~1) != 0)
        return ADHOC_ERR_INVALID_ARG;
    return pdpRecvById(id, src_mac, sport, buf, len, timeout, flags, NULL);
}

/* 0x002004 */
s32 sceNetAdhoc_67346A2A(s32 id, u8 *src_mac, u16 *sport, void *buf, s32 *len, u32 timeout, s32 flags, PdpRecvExtra *extra)
{
    if (buf == NULL || len == NULL || (flags & ~1) != 0 || extra == NULL)
        return ADHOC_ERR_INVALID_ARG;
    return pdpRecvById(id, src_mac, sport, buf, len, timeout, flags, extra);
}

/* 0x002060 */
void sceNetAdhoc_lib_F9240FBA(void)
{
    sceKernelMemset(&g_pdp_stats, 0, sizeof(g_pdp_stats));
}

/* 0x002088 */
s32 sceNetAdhoc_lib_1059683E(void *out_stats)
{
    if (out_stats == NULL)
        return ADHOC_ERR_INVALID_ARG;
    sceKernelMemcpy(out_stats, &g_pdp_stats, sizeof(g_pdp_stats));
    return 0;
}

/* 0x0020c4 */
static s32 __attribute__((noinline))
pdpRecvById(s32 id, u8 *src_mac, u16 *sport, void *buf, s32 *len, u32 timeout, s32 flags, PdpRecvExtra *extra)
{
    s32 err = 0;
    AdhocWaiter *waiter = (AdhocWaiter *)sceNet_lib_CA3CF5EB(&err);
    s32 intr;

    if (waiter == NULL)
        return err;

    intr = sceNet_lib_94B44F26();
    if (intr < 0) {
        err = intr;
    } else if (g_adhoc_inited == 0) {
        err = ADHOC_ERR_NOT_INITIALIZED;
    } else {
        AdhocSocket *sock = adhocSockFind(1, id);
        if (sock == NULL) {
            err = ADHOC_ERR_INVALID_SOCKET_ID;
        } else {
            waiter->next = sock->wait_list;
            sock->wait_list = waiter;
            err = pdpRecvInternal(sock, src_mac, sport, buf, len, timeout, flags, extra);
            pdpUnlinkWaiter(sock, waiter);
            if ((sock->flags & 1u) != 0) {
                if (sock->wait_list == NULL) {
                    pdpFreePcb(sock);
                    adhocSockTryFree(sock);
                }
                err = ADHOC_ERR_SOCKET_DELETED;
            }
        }
    }

    sceNet_lib_515B2F33(intr);
    sceNet_lib_757085B0(waiter, 0);
    return err;
}

/* 0x00225c */
static void __attribute__((noinline))
pdpFlushQueues(PdpPcb *pdp)
{
    AdhocMbuf *mbuf;
    PdpFragEntry *frag;

    while ((mbuf = pdp->rx_head) != NULL) {
        pdp->rx_head = mbuf->m_nextpkt;
        if (pdp->rx_head == NULL)
            pdp->rx_tail = NULL;
        mbuf->m_nextpkt = NULL;
        pdp->rx_cc = pdp->rx_cc - (u32)mbuf->m_pkthdr_len + 60u;
        sceNetIfhandle_lib_F56FAC82(mbuf);
    }
    pdp->rx_hiwat = 0;

    while ((frag = pdp->frag_head) != NULL) {
        sceNet_lib_A93A93E9(&frag->timer);
        sceNetIfhandle_lib_F56FAC82(frag->m_head);
        if (frag->next != NULL)
            frag->next->prev = frag->prev;
        else
            pdp->frag_tail = frag->prev;
        *frag->prev = frag->next;
        sceNetIfhandle_lib_76BAD213(frag);
    }
}

/* 0x00231c */
void __attribute__((noinline))
pdpInputPacket(AdhocMbuf *mbuf)
{
    u8 *hdr;
    u8 *pdp_hdr;
    u16 raw_word;
    u32 body_len;
    s32 pkt_total;
    u16 dport;
    PdpPcb *pdp;

    if (mbuf->m_pkthdr_len < 60) {
        sceNetIfhandle_lib_F56FAC82(mbuf);
        return;
    }

    if (mbuf->m_len < 60) {
        mbuf = (AdhocMbuf *)sceNetIfhandle_lib_49EDBB18(mbuf, 60);
        if (mbuf == NULL)
            return;
    }
    hdr = mbuf->m_data;

    sceKernelMemcpy(&raw_word, hdr + 50, 2);
    body_len = adhocSwap16(raw_word);
    pdp_hdr = hdr + 48;
    if (body_len < 12u || mbuf->m_pkthdr_len < (s32)(body_len + 48u)) {
        sceNetIfhandle_lib_F56FAC82(mbuf);
        return;
    }

    pkt_total = (s32)(body_len + 48u);
    if (pkt_total < mbuf->m_pkthdr_len) {
        if (mbuf->m_len == mbuf->m_pkthdr_len) {
            mbuf->m_pkthdr_len = pkt_total;
            mbuf->m_len = pkt_total;
        } else {
            sceNetIfhandle_lib_BFF3CEA5(mbuf, pkt_total - mbuf->m_pkthdr_len);
        }
    }

    sceKernelMemcpy(&raw_word, hdr + 54, 2);
    dport = adhocSwap16(raw_word);

    if (sceNet_lib_E0A81C7C(g_bcast_mac, hdr, 6) != 0) {
        pdp = g_pdp_list.head;
        while (pdp != NULL) {
            if (sceNet_lib_E0A81C7C(pdp->mac, hdr, 6) == 0 && pdp->port == dport)
                break;
            pdp = pdp->next;
        }
    } else {
        AdhocIfnet *ifp = (AdhocIfnet *)mbuf->m_rcvif;
        u8 *sadl = ifp->if_sadl;
        u8 *if_mac = sadl + sadl[5] + 8;
        pdp = g_pdp_list.head;
        while (pdp != NULL) {
            if (sceNet_lib_E0A81C7C(pdp->mac, if_mac, 6) == 0 && pdp->port == dport)
                break;
            pdp = pdp->next;
        }
    }

    if (pdp == NULL) {
        g_pdp_stats.rx_no_port++;
        sceNetIfhandle_lib_F56FAC82(mbuf);
        return;
    }

    if (pdp_hdr[10] != 0 || pdp_hdr[11] != 0 || (pdp_hdr[1] & 1u) != 0) {
        u8 *base = mbuf->m_data;
        u8 *cur_pdp = base + 48;
        PdpFragEntry *frag = pdp->frag_head;

        while (frag != NULL) {
            u8 *first_hdr = frag->m_head->m_data;
            if (sceNet_lib_E0A81C7C(base + 6, first_hdr + 6, 6) == 0 &&
                sceNet_lib_E0A81C7C(cur_pdp + 8, first_hdr + 56, 2) == 0) {
                break;
            }
            frag = frag->next;
        }

        if (frag == NULL) {
            if (pdp_hdr[10] != 0 || pdp_hdr[11] != 0) {
                g_pdp_stats.rx_frag_err++;
                sceNetIfhandle_lib_F56FAC82(mbuf);
                return;
            }
            frag = (PdpFragEntry *)sceNetIfhandle_lib_15CFE3C0(sizeof(PdpFragEntry));
            if (frag == NULL) {
                g_pdp_stats.rx_frag_err++;
                sceNetIfhandle_lib_F56FAC82(mbuf);
                return;
            }
            sceKernelMemcpy(&frag->total_len, pdp_hdr + 2, 2);
            frag->total_len = adhocSwap16(frag->total_len);
            frag->m_head = mbuf;
            sceNet_lib_25CC373A(&frag->timer);
            frag->next = NULL;
            frag->prev = pdp->frag_tail;
            *pdp->frag_tail = frag;
            pdp->frag_tail = &frag->next;
            frag->owner = pdp;
            g_pdp_stats.rx_frags++;
            sceNet_lib_A8B6205A(&frag->timer, 1000000, pdpFragTimeout, frag);
            return;
        }

        sceKernelMemcpy(&raw_word, pdp_hdr + 10, 2);
        if (adhocSwap16(raw_word) != (u16)(frag->total_len - 12)) {
            sceNet_lib_A93A93E9(&frag->timer);
            sceNetIfhandle_lib_F56FAC82(frag->m_head);
            if (frag->next != NULL)
                frag->next->prev = frag->prev;
            else
                pdp->frag_tail = frag->prev;
            *frag->prev = frag->next;
            sceNetIfhandle_lib_76BAD213(frag);
            g_pdp_stats.rx_frag_err++;
            sceNetIfhandle_lib_F56FAC82(mbuf);
            return;
        }

        {
            u8 *first_hdr = frag->m_head->m_data;
            u8 more_frags = pdp_hdr[1];
            u16 new_total;

            sceKernelMemcpy(&raw_word, pdp_hdr + 2, 2);
            new_total = (u16)(frag->total_len + adhocSwap16(raw_word) - 12);
            frag->total_len = new_total;
            raw_word = adhocSwap16(new_total);
            sceKernelMemcpy(first_hdr + 50, &raw_word, 2);

            sceNetIfhandle_lib_BFF3CEA5(mbuf, 60);
            frag->m_head->m_pkthdr_len += mbuf->m_pkthdr_len;
            sceNetIfhandle_lib_3E8DD3F8(frag->m_head, mbuf);
            g_pdp_stats.rx_frags++;

            if ((more_frags & 1u) != 0) {
                sceNet_lib_A8B6205A(&frag->timer, 1000000, pdpFragTimeout, frag);
                return;
            }

            sceNet_lib_A93A93E9(&frag->timer);
            mbuf = frag->m_head;
            if (frag->next != NULL)
                frag->next->prev = frag->prev;
            else
                pdp->frag_tail = frag->prev;
            *frag->prev = frag->next;
            sceNetIfhandle_lib_76BAD213(frag);
        }
    }

    if (pdp->rx_hiwat < pdp->rx_cc + (u32)mbuf->m_pkthdr_len - 60u) {
        g_pdp_stats.rx_drop_full++;
        sceNetIfhandle_lib_F56FAC82(mbuf);
        return;
    }

    mbuf->m_nextpkt = NULL;
    if (pdp->rx_tail != NULL)
        pdp->rx_tail->m_nextpkt = mbuf;
    else
        pdp->rx_head = mbuf;
    pdp->rx_tail = mbuf;
    pdp->rx_cc = pdp->rx_cc + (u32)mbuf->m_pkthdr_len - 60u;

    if ((pdp->rx_wait_flags & 1u) != 0) {
        pdp->rx_wait_flags &= ~1u;
        sceNet_lib_7574FDA1(&pdp->rx_head);
    }
    if ((pdp->rx_wait_flags & 2u) != 0) {
        pdp->rx_wait_flags &= ~2u;
        sceNet_lib_7574FDA1(&g_global_wait_head);
    }
    g_pdp_stats.rx_ok++;
}

/* 0x002850 */
s32 sceNetAdhocGetPdpStat(s32 *buflen, ScePdpStatOut *buf)
{
    s32 ret = 0;
    s32 intr;
    s32 written = 0;
    ScePdpStatOut *prev = NULL;

    if (_sceKernelCheckThreadStack() < 2000)
        return ADHOC_ERR_STACK_SMALL;
    if (buflen == NULL)
        return ADHOC_ERR_INVALID_ARG;
    if (((u32)buf & 3u) != 0)
        return ADHOC_ERR_INVALID_ALIGNMENT;

    intr = sceNet_lib_94B44F26();
    if (intr < 0)
        return intr;

    if (g_adhoc_inited == 0) {
        ret = ADHOC_ERR_NOT_INITIALIZED;
    } else {
        PdpPcb *pdp = g_pdp_list.head;
        ScePdpStatOut *out = buf;
        while (pdp != NULL) {
            if (buf != NULL) {
                if (*buflen < written + (s32)sizeof(ScePdpStatOut))
                    break;
                out->next = NULL;
                out->pdp_id = pdp->sock->id;
                sceKernelMemcpy(out->mac, pdp->mac, 6);
                out->port = pdp->port;
                out->rcvd_data = pdp->rx_cc;
                if (prev != NULL)
                    prev->next = out;
                prev = out;
                out++;
            }
            written += (s32)sizeof(ScePdpStatOut);
            pdp = pdp->next;
        }
    }

    sceNet_lib_515B2F33(intr);
    *buflen = written;
    return ret;
}

/* 0x0029ac */
s32 __attribute__((noinline))
pdpInit(void)
{
    g_pdp_list.head = NULL;
    g_pdp_list.tail = &g_pdp_list.head;
    g_pdp_anon_port = (u16)(sceNet_lib_1858883D() / 0x20009u + 0x4000u);
    g_pdp_seq = (u16)(sceNet_lib_1858883D() / 0x8001u);
    return 0;
}

/* 0x002a18 */
void __attribute__((noinline))
pdpTerm(void)
{
    PdpPcb *pdp;

    while ((pdp = g_pdp_list.head) != NULL) {
        AdhocSocket *sock = pdp->sock;
        PdpPcb *victim = (PdpPcb *)sock->pcb;
        pdpFlushQueues(pdp);
        sock->pcb = NULL;
        if (victim->next != NULL)
            victim->next->prev = victim->prev;
        else
            g_pdp_list.tail = victim->prev;
        *victim->prev = victim->next;
        sceNetIfhandle_lib_76BAD213(victim);
    }
}

/* 0x002a9c */
static s32 __attribute__((noinline))
pdpAllocPcb(AdhocSocket *sock)
{
    PdpPcb *pdp = (PdpPcb *)sceNetIfhandle_lib_15CFE3C0(sizeof(PdpPcb));
    PdpPcb **old_tail;

    if (pdp == NULL)
        return ADHOC_ERR_NO_MEM;

    sceKernelMemset(pdp, 0, sizeof(PdpPcb));
    pdp->next = NULL;
    pdp->prev = g_pdp_list.tail;
    old_tail = g_pdp_list.tail;
    sock->pcb = pdp;
    g_pdp_list.tail = &pdp->next;
    *old_tail = pdp;
    ((PdpPcb *)sock->pcb)->frag_head = NULL;
    ((PdpPcb *)sock->pcb)->sock = sock;
    ((PdpPcb *)sock->pcb)->frag_tail = &pdp->frag_head;
    return 0;
}

/* 0x002b2c */
static s32 __attribute__((noinline))
pdpBindPcb(PdpPcb *pdp, const u8 *mac, u16 port)
{
    AdhocIfnet *ifp = (AdhocIfnet *)sceNet_lib_D60225A3(mac);
    u16 chosen_port = port;

    if (ifp == NULL)
        return ADHOC_ERR_INVALID_ADDR;

    if (chosen_port == 0) {
        u16 cand = (g_pdp_anon_port == 0x7FFF) ? 0x4000 : (u16)(g_pdp_anon_port + 1);
        chosen_port = 0;
        while (cand != g_pdp_anon_port) {
            PdpPcb *cur = g_pdp_list.head;
            while (cur != NULL) {
                if (sceNet_lib_E0A81C7C(cur->mac, mac, 6) == 0 && cur->port == cand)
                    break;
                cur = cur->next;
            }
            if (cur == NULL) {
                g_pdp_anon_port = cand;
                chosen_port = cand;
                break;
            }
            cand = (cand == 0x7FFF) ? 0x4000 : (u16)(cand + 1);
        }
        if (chosen_port == 0)
            return ADHOC_ERR_PORT_NOT_AVAIL;
    } else {
        PdpPcb *cur;
        if (chosen_port == 0xFFFF)
            return ADHOC_ERR_INVALID_PORT;
        cur = g_pdp_list.head;
        while (cur != NULL) {
            if (sceNet_lib_E0A81C7C(cur->mac, mac, 6) == 0 && cur->port == chosen_port)
                return ADHOC_ERR_PORT_IN_USE;
            cur = cur->next;
        }
    }

    sceKernelMemcpy(pdp->mac, mac, 6);
    pdp->port = chosen_port;
    pdp->sock->chan_tx = &ifp->snd_head;
    pdp->sock->chan_rx = &pdp->rx_head;
    pdp->sock->chan_poll = &g_global_wait_head;
    return 0;
}

/* 0x002d04 */
void __attribute__((noinline))
pdpFreePcb(AdhocSocket *sock)
{
    PdpPcb *pdp = (PdpPcb *)sock->pcb;
    sock->pcb = NULL;
    if (pdp->next != NULL)
        pdp->next->prev = pdp->prev;
    else
        g_pdp_list.tail = pdp->prev;
    *pdp->prev = pdp->next;
    sceNetIfhandle_lib_76BAD213(pdp);
}

/* 0x002d50 */
static s32 __attribute__((noinline))
pdpSetBufSize(PdpPcb *pdp, u32 bufsize)
{
    pdp->rx_hiwat = bufsize;
    return 0;
}

/* 0x002d5c */
s32 __attribute__((noinline))
pdpIfOutputWake(AdhocIfnet *ifp)
{
    if ((ifp->snd_wait_flags & 1u) != 0) {
        ifp->snd_wait_flags &= ~1u;
        sceNet_lib_7574FDA1(&ifp->snd_head);
    }
    if ((ifp->snd_wait_flags & 2u) != 0) {
        ifp->snd_wait_flags &= ~2u;
        return sceNet_lib_7574FDA1(&g_global_wait_head);
    }
    return 0;
}

/* 0x002dc4 */
s32 __attribute__((noinline))
pdpPollEvents(AdhocSocket *sock, s32 events)
{
    PdpPcb *pdp = (PdpPcb *)sock->pcb;
    AdhocIfnet *ifp = (AdhocIfnet *)sceNet_lib_D60225A3(pdp->mac);
    s32 revents = 0;

    if ((events & 2) != 0 && pdp->rx_head != NULL)
        revents = 2;

    if ((events & 1) != 0) {
        if (ifp == NULL || ifp->snd_len < ifp->snd_maxlen)
            revents |= 1;
    }

    if (revents == 0) {
        if ((events & 2) != 0)
            pdp->rx_wait_flags |= 2u;
        if ((events & 1) != 0 && ifp != NULL)
            ifp->snd_wait_flags |= 2u;
    }
    return revents;
}

/* 0x002e78 */
static s32 __attribute__((noinline))
pdpFragTimeout(PdpFragEntry *frag)
{
    sceNet_lib_A93A93E9(&frag->timer);
    sceNetIfhandle_lib_F56FAC82(frag->m_head);
    if (frag->next != NULL)
        frag->next->prev = frag->prev;
    else
        frag->owner->frag_tail = frag->prev;
    *frag->prev = frag->next;
    sceNetIfhandle_lib_76BAD213(frag);
    g_pdp_stats.rx_frag_timeout++;
    return 0;
}
