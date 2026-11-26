#include "adhoc_int.h"

#define PTP_STATE_CLOSED      0
#define PTP_STATE_LISTEN      1
#define PTP_STATE_SYN_SENT    2
#define PTP_STATE_SYN_RCVD    3
#define PTP_STATE_ESTABLISHED 4

#define PTP_PKT_SYN       1
#define PTP_PKT_SYN_ACK   2
#define PTP_PKT_ACK       3
#define PTP_PKT_DATA      4
#define PTP_PKT_DATA_ACK  5
#define PTP_PKT_NACK      6
#define PTP_PKT_RST       7

static struct {
    PtpPcb *head;
    PtpPcb **tail;
} g_ptp_list;

static u16 g_ptp_anon_port;
static u32 g_ptp_conn_seq;

static struct {
    u32 tx_syn;
    u32 rx_syn_ok;
    u32 rx_syn_dup;
    u32 rx_syn_drop;
    u32 tx_syn_ack;
    u32 rx_syn_ack_ok;
    u32 rx_syn_ack_dup;
    u32 rx_syn_ack_drop;
    u32 tx_ack;
    u32 rx_ack_ok;
    u32 rx_ack_dup;
    u32 rx_ack_drop;
    u32 tx_data;
    u32 rx_data_ok;
    u32 rx_data_full;
    u32 rx_data_old;
    u32 rx_data_drop;
    u32 tx_data_ack;
    u32 rx_data_ack_ok;
    u32 rx_data_ack_dup;
    u32 rx_data_ack_drop;
    u32 tx_nack;
    u32 rx_nack_ok;
    u32 rx_nack_drop;
    u32 tx_rst;
    u32 rx_rst_ok;
    u32 rx_rst_drop;
    u32 rx_no_pcb;
    u32 rexmt_data;
    u32 tx_probe;
    u32 tx_wnd_probe;
} g_ptp_stats;

static s32 ptpCreateSocket(const u8 *self_mac, u16 self_port, const u8 *peer_mac, u16 peer_port, u32 tx_bufsize, u32 rx_bufsize, u32 rexmt_intvl, s32 rexmt_cnt, AdhocSocket **out_sock);
static void ptpDestroySocket(AdhocSocket *sock);
static s32 ptpWaitConnect(AdhocSocket *sock, u32 timeout, s32 flags);
static s32 ptpAcceptInternal(AdhocSocket *sock, u8 *peer_mac, u16 *peer_port, u32 timeout, s32 flags);
static s32 ptpSendInternal(AdhocSocket *sock, const void *buf, s32 *len, u32 timeout, s32 flags);
static s32 ptpRecvInternal(AdhocSocket *sock, void *buf, s32 *len, u32 timeout, s32 flags);
static s32 ptpFlushInternal(AdhocSocket *sock, u32 timeout, s32 flags);
static s32 ptpAllocPcb(AdhocSocket *sock);
static void ptpDropQueues(PtpPcb *pcb);
static PtpPcb *ptpSpawnChild(PtpPcb *listener, const u8 *peer_mac, u16 peer_port, u32 conn_id);
static void ptpMarkConnected(PtpPcb *pcb);
static void ptpDisconnectPcb(PtpPcb *pcb);
static PtpPcb *ptpFindPcb(const u8 *self_mac, u16 self_port, const u8 *peer_mac, u16 peer_port);
static s32 ptpBindPcb(PtpPcb *pcb, const u8 *self_mac, u16 self_port, const u8 *peer_mac, u16 peer_port);
static s32 ptpSetBufSizes(PtpPcb *pcb, u32 tx_bufsize, u32 rx_bufsize);
static void ptpSetRexmtParams(PtpPcb *pcb, u32 rexmt_intvl, s32 rexmt_cnt);
static s32 ptpStartConnect(PtpPcb *pcb, const u8 *peer_mac, u16 peer_port);
static s32 ptpStartListen(PtpPcb *pcb, s32 backlog);
static void ptpFreeListenQueue(PtpPcb *pcb);
static u16 ptpAllocAnonPort(const u8 *self_mac);
static s32 ptpBuildAndSendPacket(const u8 *src_mac, u16 src_port, const u8 *dst_mac, u16 dst_port, u8 pkt_type, u32 seq_id, AdhocMbuf *payload);
static void ptpSendControlPacket(PtpPcb *pcb, u8 pkt_type);
static void ptpTransmitData(PtpPcb *pcb, s32 is_rexmt);
static void ptpRexmtTimeout(PtpPcb *pcb);
static void ptpProbeTimeout(PtpPcb *pcb);

static inline void ptpUnlinkWaiter(AdhocSocket *sock, AdhocWaiter *waiter)
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

/* 0x002eec */
s32 sceNetAdhocPtpOpen(const u8 *self_mac, u16 self_port, const u8 *peer_mac, u16 peer_port, u32 bufsize, u32 rexmt_intvl, s32 rexmt_cnt, s32 flags)
{
    s32 intr;
    s32 ret;
    AdhocSocket *sock = NULL;

    if (_sceKernelCheckThreadStack() < 1968)
        return ADHOC_ERR_STACK_SMALL;
    if (self_mac == NULL || peer_mac == NULL || flags != 0)
        return ADHOC_ERR_INVALID_ARG;

    intr = sceNet_lib_94B44F26();
    if (intr < 0)
        return intr;

    ret = ADHOC_ERR_NOT_INITIALIZED;
    if (g_adhoc_inited != 0) {
        u32 delay = sceNetIfhandle_lib_07505747(0);
        if (delay != 0)
            sceKernelDelayThread(delay);

        ret = ptpCreateSocket(self_mac, self_port, peer_mac, peer_port, bufsize, bufsize, rexmt_intvl, rexmt_cnt, &sock);
        if (ret >= 0) {
            ret = ptpStartConnect((PtpPcb *)sock->pcb, peer_mac, peer_port);
            if (ret < 0)
                ptpDestroySocket(sock);
            else
                ret = sock->id;
        }
    }

    sceNet_lib_515B2F33(intr);
    return ret;
}

/* 0x003060 */
s32 sceNetAdhocPtpConnect(s32 id, u32 timeout, s32 flags)
{
    u32 delay;
    s32 err = 0;
    AdhocWaiter *waiter;
    s32 intr;

    delay = sceNetIfhandle_lib_07505747(4);
    if (delay != 0)
        sceKernelDelayThread(delay);

    if (_sceKernelCheckThreadStack() < 2000)
        return ADHOC_ERR_STACK_SMALL;
    if ((flags & ~1) != 0)
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
        AdhocSocket *sock = adhocSockFind(2, id);
        if (sock == NULL) {
            err = ADHOC_ERR_INVALID_SOCKET_ID;
        } else {
            waiter->next = sock->wait_list;
            sock->wait_list = waiter;
            err = ptpWaitConnect(sock, timeout, flags);
            ptpUnlinkWaiter(sock, waiter);
            if ((sock->flags & 1u) != 0) {
                if (sock->wait_list == NULL) {
                    ptpFreePcb(sock);
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

/* 0x0031fc */
s32 sceNetAdhocPtpListen(const u8 *self_mac, u16 self_port, u32 bufsize, u32 rexmt_intvl, s32 rexmt_cnt, s32 backlog, s32 flags)
{
    s32 intr;
    s32 ret;
    AdhocSocket *sock = NULL;

    if (_sceKernelCheckThreadStack() < 1968)
        return ADHOC_ERR_STACK_SMALL;
    if (self_mac == NULL || flags != 0)
        return ADHOC_ERR_INVALID_ARG;

    intr = sceNet_lib_94B44F26();
    if (intr < 0)
        return intr;

    ret = ADHOC_ERR_NOT_INITIALIZED;
    if (g_adhoc_inited != 0) {
        ret = ptpCreateSocket(self_mac, self_port, NULL, 0, bufsize, bufsize, rexmt_intvl, rexmt_cnt, &sock);
        if (ret >= 0) {
            ret = ptpStartListen((PtpPcb *)sock->pcb, backlog);
            if (ret < 0)
                ptpDestroySocket(sock);
            else
                ret = sock->id;
        }
    }

    sceNet_lib_515B2F33(intr);
    return ret;
}

/* 0x003334 */
s32 sceNetAdhocPtpAccept(s32 id, u8 *peer_mac, u16 *peer_port, u32 timeout, s32 flags)
{
    s32 err = 0;
    AdhocWaiter *waiter;
    s32 intr;

    if (_sceKernelCheckThreadStack() < 2000)
        return ADHOC_ERR_STACK_SMALL;
    if ((flags & ~1) != 0)
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
        AdhocSocket *sock = adhocSockFind(2, id);
        if (sock == NULL) {
            err = ADHOC_ERR_INVALID_SOCKET_ID;
        } else {
            u32 delay = sceNetIfhandle_lib_07505747(2);
            if (delay != 0)
                sceKernelDelayThread(delay);
            waiter->next = sock->wait_list;
            sock->wait_list = waiter;
            err = ptpAcceptInternal(sock, peer_mac, peer_port, timeout, flags);
            ptpUnlinkWaiter(sock, waiter);
            if ((sock->flags & 1u) != 0) {
                if (sock->wait_list == NULL) {
                    ptpFreePcb(sock);
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

/* 0x0034ec */
s32 sceNetAdhocPtpSend(s32 id, const void *buf, s32 *len, u32 timeout, s32 flags)
{
    s32 err = 0;
    AdhocWaiter *waiter;
    s32 intr;

    if (_sceKernelCheckThreadStack() < 2000)
        return ADHOC_ERR_STACK_SMALL;
    if (len == NULL || (flags & ~1) != 0)
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
        AdhocSocket *sock = adhocSockFind(2, id);
        if (sock == NULL) {
            err = ADHOC_ERR_INVALID_SOCKET_ID;
        } else if ((sock->flags & 0xC000u) == 0) {
            err = ADHOC_ERR_NOT_CONNECTED;
        } else {
            waiter->next = sock->wait_list;
            sock->wait_list = waiter;
            err = ptpSendInternal(sock, buf, len, timeout, flags);
            ptpUnlinkWaiter(sock, waiter);
            if ((sock->flags & 1u) != 0) {
                if (sock->wait_list == NULL) {
                    ptpFreePcb(sock);
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

/* 0x0036a8 */
s32 sceNetAdhocPtpRecv(s32 id, void *buf, s32 *len, u32 timeout, s32 flags)
{
    s32 err = 0;
    AdhocWaiter *waiter;
    s32 intr;

    if (_sceKernelCheckThreadStack() < 2000)
        return ADHOC_ERR_STACK_SMALL;
    if (buf == NULL || len == NULL || (flags & ~1) != 0)
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
        AdhocSocket *sock = adhocSockFind(2, id);
        if (sock == NULL) {
            err = ADHOC_ERR_INVALID_SOCKET_ID;
        } else if ((sock->flags & 0xC000u) == 0) {
            err = ADHOC_ERR_NOT_CONNECTED;
        } else {
            waiter->next = sock->wait_list;
            sock->wait_list = waiter;
            err = ptpRecvInternal(sock, buf, len, timeout, flags);
            ptpUnlinkWaiter(sock, waiter);
            if ((sock->flags & 1u) != 0) {
                if (sock->wait_list == NULL) {
                    ptpFreePcb(sock);
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

/* 0x003870 */
s32 sceNetAdhocPtpFlush(s32 id, u32 timeout, s32 flags)
{
    s32 err = 0;
    AdhocWaiter *waiter;
    s32 intr;

    if (_sceKernelCheckThreadStack() < 2000)
        return ADHOC_ERR_STACK_SMALL;
    if ((flags & ~1) != 0)
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
        AdhocSocket *sock = adhocSockFind(2, id);
        if (sock == NULL) {
            err = ADHOC_ERR_INVALID_SOCKET_ID;
        } else if ((sock->flags & 0xC000u) == 0) {
            err = ADHOC_ERR_NOT_CONNECTED;
        } else {
            waiter->next = sock->wait_list;
            sock->wait_list = waiter;
            err = ptpFlushInternal(sock, timeout, flags);
            ptpUnlinkWaiter(sock, waiter);
            if ((sock->flags & 1u) != 0) {
                if (sock->wait_list == NULL) {
                    ptpFreePcb(sock);
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

/* 0x003a00 */
s32 sceNetAdhocPtpClose(s32 id, s32 flags)
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
        AdhocSocket *sock = adhocSockFind(2, id);
        if (sock == NULL) {
            ret = ADHOC_ERR_INVALID_SOCKET_ID;
        } else {
            ptpDisconnectPcb((PtpPcb *)sock->pcb);
            ptpFreeListenQueue((PtpPcb *)sock->pcb);
            sock->flags |= 1u;
            sock->id = -1;
            if (sock->wait_list == NULL) {
                ptpFreePcb(sock);
                adhocSockTryFree(sock);
            }
        }
    }

    sceNet_lib_515B2F33(intr);
    return ret;
}

/* 0x003afc */
static s32 __attribute__((noinline))
ptpCreateSocket(const u8 *self_mac, u16 self_port, const u8 *peer_mac, u16 peer_port, u32 tx_bufsize, u32 rx_bufsize, u32 rexmt_intvl, s32 rexmt_cnt, AdhocSocket **out_sock)
{
    s32 id;
    s32 ret;
    AdhocSocket *sock;

    id = adhocSockAllocId();
    if (id < 0)
        return ADHOC_ERR_SOCKET_ID_NOT_AVAIL;

    ret = adhocSockCreate(2, &sock);
    if (ret < 0)
        return ret;

    ret = ptpAllocPcb(sock);
    if (ret >= 0) {
        ret = ptpSetBufSizes((PtpPcb *)sock->pcb, tx_bufsize, rx_bufsize);
        if (ret >= 0) {
            ret = ptpBindPcb((PtpPcb *)sock->pcb, self_mac, self_port, peer_mac, peer_port);
            if (ret >= 0) {
                ptpSetRexmtParams((PtpPcb *)sock->pcb, rexmt_intvl, rexmt_cnt);
                sock->id = id;
                *out_sock = sock;
                return 0;
            }
            ptpDropQueues((PtpPcb *)sock->pcb);
        }
        ptpFreePcb(sock);
    }
    adhocSockTryFree(sock);
    return ret;
}

/* 0x003c44 */
static void __attribute__((noinline))
ptpDestroySocket(AdhocSocket *sock)
{
    ptpDropQueues((PtpPcb *)sock->pcb);
    ptpFreePcb(sock);
    adhocSockTryFree(sock);
}

/* 0x003c7c */
static s32 __attribute__((noinline))
ptpWaitConnect(AdhocSocket *sock, u32 timeout, s32 flags)
{
    s64 start_time = 0;
    PtpPcb *pcb = (PtpPcb *)sock->pcb;

    for (;;) {
        u32 wait_rem = 0;
        s32 wait_err;

        if ((sock->flags & 0x4000u) != 0)
            return 0;
        if ((sock->flags & 0x8000u) != 0)
            return ADHOC_ERR_CONNECTION_REFUSED;
        if ((sock->flags & 0x80u) != 0)
            return ADHOC_ERR_SOCKET_ALERTED;
        if ((sock->flags & 0x2000u) == 0)
            return ADHOC_ERR_NOT_OPENED;
        if ((flags & 1) != 0)
            return ADHOC_ERR_WOULD_BLOCK;

        if (timeout != 0) {
            s64 now;
            if (start_time == 0)
                start_time = sceKernelGetSystemTimeWide();
            now = sceKernelGetSystemTimeWide();
            if ((u64)(now - start_time) >= (u64)timeout)
                return ADHOC_ERR_TIMEOUT;
            wait_rem = timeout - (u32)(now - start_time);
        }

        pcb->rx_wait_flags |= 1u;
        wait_err = sceNet_lib_F6DB0A0B(&pcb->rx_head, wait_rem);
        if ((sock->flags & 0x80u) != 0)
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
}

/* 0x003e30 */
static s32 __attribute__((noinline))
ptpAcceptInternal(AdhocSocket *sock, u8 *peer_mac, u16 *peer_port, u32 timeout, s32 flags)
{
    s64 start_time = 0;
    PtpPcb *pcb = (PtpPcb *)sock->pcb;

    for (;;) {
        PtpPcb *child;
        u32 wait_rem = 0;
        s32 wait_err;

        if ((sock->flags & 0x8000u) != 0)
            return ADHOC_ERR_DISCONNECTED;
        if ((sock->flags & 0x100u) != 0)
            return ADHOC_ERR_SOCKET_ALERTED;
        if ((sock->flags & 0x1000u) == 0)
            return ADHOC_ERR_NOT_LISTENED;

        child = pcb->q_head;
        while (child != NULL) {
            AdhocSocket *child_sock = child->sock;
            if ((child_sock->flags & 0x4000u) != 0) {
                s32 new_id = adhocSockAllocId();
                if (new_id < 0)
                    return ADHOC_ERR_SOCKET_ID_NOT_AVAIL;
                if (child->acc_next != NULL)
                    child->acc_next->acc_prev = child->acc_prev;
                else
                    pcb->q_tail = child->acc_prev;
                *child->acc_prev = child->acc_next;
                pcb->q_len--;
                child->parent = NULL;
                if (peer_mac != NULL)
                    sceKernelMemcpy(peer_mac, child->peer_mac, 6);
                if (peer_port != NULL)
                    *peer_port = child->peer_port;
                child_sock->id = new_id;
                return new_id;
            }
            if ((child_sock->flags & 0x8000u) != 0) {
                PtpPcb *next_child = child->acc_next;
                if (next_child != NULL)
                    next_child->acc_prev = child->acc_prev;
                else
                    pcb->q_tail = child->acc_prev;
                *child->acc_prev = next_child;
                pcb->q_len--;
                ptpFreePcb(child_sock);
                adhocSockTryFree(child_sock);
                child = next_child;
            } else {
                child = child->acc_next;
            }
        }

        if ((flags & 1) != 0)
            return ADHOC_ERR_WOULD_BLOCK;

        if (timeout != 0) {
            s64 now;
            if (start_time == 0)
                start_time = sceKernelGetSystemTimeWide();
            now = sceKernelGetSystemTimeWide();
            if ((u64)(now - start_time) >= (u64)timeout)
                return ADHOC_ERR_TIMEOUT;
            wait_rem = timeout - (u32)(now - start_time);
        }

        pcb->rx_wait_flags |= 1u;
        wait_err = sceNet_lib_F6DB0A0B(&pcb->rx_head, wait_rem);
        if ((sock->flags & 0x100u) != 0)
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
}

/* 0x0040ec */
static s32 __attribute__((noinline))
ptpSendInternal(AdhocSocket *sock, const void *buf, s32 *len, u32 timeout, s32 flags)
{
    u32 alloc_retries = 0;
    s32 ret = 0;
    s32 orig_len = *len;
    s32 rem = orig_len;
    PtpPcb *pcb = (PtpPcb *)sock->pcb;
    const u8 *src_bytes = (const u8 *)buf;
    s64 start_time = 0;

    if (orig_len < 0)
        return ADHOC_ERR_INVALID_BUFLEN;
    if (orig_len != 0 && buf == NULL)
        return ADHOC_ERR_INVALID_BUFLEN;

    while (rem > 0) {
        u32 free_space;

        if ((sock->flags & 0x8000u) != 0) {
            ret = ADHOC_ERR_DISCONNECTED;
            break;
        }
        if ((sock->flags & 0x10u) != 0) {
            ret = ADHOC_ERR_SOCKET_ALERTED;
            break;
        }

        free_space = pcb->tx_hiwat - pcb->tx_cc;
        if ((s32)free_space >= rem || free_space >= pcb->tx_lowat) {
            s32 avail = ((s32)free_space > rem) ? rem : (s32)free_space;
            s32 chunk = ((s32)pcb->mss < avail) ? (s32)pcb->mss : avail;
            AdhocMbuf *mbuf = (AdhocMbuf *)sceNetIfhandle_lib_15CFE3C0(sizeof(AdhocMbuf));

            if (mbuf == NULL) {
                alloc_retries++;
                if (alloc_retries >= 5u) {
                    ret = ADHOC_ERR_NO_MEM;
                    break;
                }
                sceKernelDelayThread(5000);
                continue;
            }

            mbuf->m_type = 1;
            mbuf->m_data = mbuf->payload.inline_buf;
            mbuf->m_flags = 2;
            mbuf->m_next = NULL;
            mbuf->m_nextpkt = NULL;
            mbuf->m_pkthdr_aux = 0;

            if (chunk >= 209) {
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
                    alloc_retries++;
                    sceNetIfhandle_lib_F8825DC4(mbuf);
                    if (alloc_retries >= 5u) {
                        ret = ADHOC_ERR_NO_MEM;
                        break;
                    }
                    sceKernelDelayThread(5000);
                    continue;
                }
            }

            sceKernelMemcpy(mbuf->m_data, src_bytes + (*len - rem), (u32)chunk);
            mbuf->m_len = chunk;
            mbuf->m_pkthdr_len = chunk;
            alloc_retries = 0;
            mbuf->m_rcvif = NULL;
            sceKernelMemset(mbuf->hdr_meta.meta_bytes, 0, sizeof(mbuf->hdr_meta.meta_bytes));
            mbuf->m_nextpkt = NULL;
            if (pcb->tx_tail != NULL)
                pcb->tx_tail->m_nextpkt = mbuf;
            else
                pcb->tx_head = mbuf;
            pcb->tx_tail = mbuf;
            rem -= chunk;
            pcb->tx_cc += (u32)mbuf->m_pkthdr_len;
            ptpTransmitData(pcb, 0);
            pcb->snd_max++;
            continue;
        }

        if ((flags & 1) != 0) {
            ret = ADHOC_ERR_WOULD_BLOCK;
            break;
        }

        {
            u32 wait_rem = 0;
            s32 wait_err;

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

            pcb->tx_wait_flags |= 1u;
            wait_err = sceNet_lib_F6DB0A0B(&pcb->tx_head, wait_rem);
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
    }

    *len = *len - rem;
    return ret;
}

/* 0x0044d4 */
static s32 __attribute__((noinline))
ptpRecvInternal(AdhocSocket *sock, void *buf, s32 *len, u32 timeout, s32 flags)
{
    s64 start_time = 0;
    s32 rem = *len;
    PtpPcb *pcb = (PtpPcb *)sock->pcb;
    u8 *dst_bytes = (u8 *)buf;

    for (;;) {
        if ((sock->flags & 0x8000u) != 0 && pcb->rx_cc == 0)
            return ADHOC_ERR_DISCONNECTED;
        if ((sock->flags & 0x20u) != 0)
            return ADHOC_ERR_SOCKET_ALERTED;

        if (pcb->rx_cc >= pcb->rx_lowat || (sock->flags & 0x8000u) != 0) {
            s32 hiwat = (s32)pcb->rx_hiwat;
            s32 old_free = hiwat - (s32)pcb->rx_cc;
            AdhocMbuf *mbuf = pcb->rx_head;
            s32 quarter;
            s32 new_free;

            while (mbuf != NULL && rem > 0) {
                s32 pkt_len = mbuf->m_pkthdr_len;
                if (rem >= pkt_len) {
                    AdhocMbuf *head;
                    sceNetIfhandle_lib_9A6261EC(mbuf, 0, pkt_len, dst_bytes + (*len - rem));
                    rem -= pkt_len;
                    head = pcb->rx_head;
                    if (head != NULL) {
                        pcb->rx_head = head->m_nextpkt;
                        if (pcb->rx_head == NULL)
                            pcb->rx_tail = NULL;
                        head->m_nextpkt = NULL;
                        pcb->rx_cc -= (u32)head->m_pkthdr_len;
                    }
                    sceNetIfhandle_lib_F56FAC82(head);
                } else {
                    sceNetIfhandle_lib_9A6261EC(mbuf, 0, rem, dst_bytes + (*len - rem));
                    sceNetIfhandle_lib_BFF3CEA5(mbuf, rem);
                    pcb->rx_cc -= (u32)rem;
                    rem = 0;
                }
                mbuf = pcb->rx_head;
            }

            hiwat = (s32)pcb->rx_hiwat;
            quarter = hiwat / 4;
            new_free = hiwat - (s32)pcb->rx_cc;
            if (old_free < quarter && old_free < (s32)pcb->mss) {
                if (new_free >= quarter || new_free >= (s32)pcb->mss)
                    ptpSendControlPacket(pcb, PTP_PKT_DATA_ACK);
            }

            *len = *len - rem;
            return 0;
        }

        if ((flags & 1) != 0)
            return ADHOC_ERR_WOULD_BLOCK;

        {
            u32 wait_rem = 0;
            s32 wait_err;

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

            pcb->rx_wait_flags |= 1u;
            wait_err = sceNet_lib_F6DB0A0B(&pcb->rx_head, wait_rem);
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
    }
}

/* 0x004808 */
static s32 __attribute__((noinline))
ptpFlushInternal(AdhocSocket *sock, u32 timeout, s32 flags)
{
    s64 start_time = 0;
    PtpPcb *pcb = (PtpPcb *)sock->pcb;

    while (pcb->tx_cc != 0) {
        u32 wait_rem = 0;
        s32 wait_err;

        if ((sock->flags & 0x8000u) != 0)
            return ADHOC_ERR_DISCONNECTED;
        if ((sock->flags & 0x200u) != 0)
            return ADHOC_ERR_SOCKET_ALERTED;
        if ((flags & 1) != 0)
            return ADHOC_ERR_WOULD_BLOCK;

        if (timeout != 0) {
            s64 now;
            if (start_time == 0)
                start_time = sceKernelGetSystemTimeWide();
            now = sceKernelGetSystemTimeWide();
            if ((u64)(now - start_time) >= (u64)timeout)
                return ADHOC_ERR_TIMEOUT;
            wait_rem = timeout - (u32)(now - start_time);
        }

        pcb->tx_wait_flags |= 1u;
        wait_err = sceNet_lib_F6DB0A0B(&pcb->tx_head, wait_rem);
        if ((sock->flags & 0x200u) != 0)
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

    return 0;
}

/* 0x0049cc */
void sceNetAdhoc_lib_62B875A8(void)
{
    sceKernelMemset(&g_ptp_stats, 0, sizeof(g_ptp_stats));
}

/* 0x0049f4 */
s32 sceNetAdhoc_lib_2CA12790(void *out_stats)
{
    if (out_stats == NULL)
        return ADHOC_ERR_INVALID_ARG;
    sceKernelMemcpy(out_stats, &g_ptp_stats, sizeof(g_ptp_stats));
    return 0;
}

/* 0x004a30 */
static s32 __attribute__((noinline))
ptpAllocPcb(AdhocSocket *sock)
{
    PtpPcb *pcb = (PtpPcb *)sceNetIfhandle_lib_15CFE3C0(sizeof(PtpPcb));
    PtpPcb **old_tail;

    if (pcb == NULL)
        return ADHOC_ERR_NO_MEM;

    sceKernelMemset(pcb, 0, sizeof(PtpPcb));
    pcb->q_head = NULL;
    pcb->q_tail = &pcb->q_head;
    sceNet_lib_25CC373A(&pcb->timer_rexmt);
    sceNet_lib_25CC373A(&pcb->timer_probe);

    pcb->next = NULL;
    pcb->prev = g_ptp_list.tail;
    old_tail = g_ptp_list.tail;
    sock->pcb = pcb;
    g_ptp_list.tail = &pcb->next;
    *old_tail = pcb;
    ((PtpPcb *)sock->pcb)->state = PTP_STATE_CLOSED;
    ((PtpPcb *)sock->pcb)->sock = sock;
    return 0;
}

/* 0x004ad4 */
static void __attribute__((noinline))
ptpDropQueues(PtpPcb *pcb)
{
    AdhocMbuf *mbuf;

    while ((mbuf = pcb->tx_head) != NULL) {
        pcb->tx_head = mbuf->m_nextpkt;
        if (pcb->tx_head == NULL)
            pcb->tx_tail = NULL;
        mbuf->m_nextpkt = NULL;
        pcb->tx_cc -= (u32)mbuf->m_pkthdr_len;
        sceNetIfhandle_lib_F56FAC82(mbuf);
    }
    pcb->tx_hiwat = 0;

    while ((mbuf = pcb->rx_head) != NULL) {
        pcb->rx_head = mbuf->m_nextpkt;
        if (pcb->rx_head == NULL)
            pcb->rx_tail = NULL;
        mbuf->m_nextpkt = NULL;
        pcb->rx_cc -= (u32)mbuf->m_pkthdr_len;
        sceNetIfhandle_lib_F56FAC82(mbuf);
    }
    pcb->rx_hiwat = 0;
}

/* 0x004b7c */
static PtpPcb *__attribute__((noinline))
ptpSpawnChild(PtpPcb *listener, const u8 *peer_mac, u16 peer_port, u32 conn_id)
{
    AdhocSocket *child_sock = NULL;
    PtpPcb *child;

    if (sceNet_lib_E0A81C7C(peer_mac, g_zero_mac, 6) == 0)
        return NULL;
    if ((peer_mac[0] & 1u) != 0 && sceNet_lib_E0A81C7C(peer_mac, g_bcast_mac, 6) != 0)
        return NULL;
    if (peer_port == 0 || peer_port == 0xFFFF)
        return NULL;
    if (listener->q_len >= listener->q_maxlen)
        return NULL;

    if (adhocSockCreate(2, &child_sock) < 0)
        return NULL;
    if (ptpAllocPcb(child_sock) < 0) {
        adhocSockTryFree(child_sock);
        return NULL;
    }

    child = (PtpPcb *)child_sock->pcb;
    sceKernelMemcpy(child->self_mac, listener->self_mac, 6);
    sceKernelMemcpy(child->peer_mac, peer_mac, 6);
    child->tx_lowat = listener->tx_lowat;
    child->tx_hiwat = listener->tx_hiwat;
    child->rx_lowat = listener->rx_lowat;
    child->rx_hiwat = listener->rx_hiwat;
    child->conn_id = conn_id;
    child->state = PTP_STATE_SYN_RCVD;
    child->mss = listener->mss;
    child->rexmt_intvl = listener->rexmt_intvl;
    child->rexmt_max = listener->rexmt_max;
    child_sock->flags |= 0x2000u;
    child->self_port = listener->self_port;
    child->peer_port = peer_port;
    child_sock->chan_tx = &child->tx_head;
    child_sock->chan_rx = &child->rx_head;
    child_sock->chan_poll = &g_global_wait_head;

    child->acc_next = NULL;
    child->acc_prev = listener->q_tail;
    *listener->q_tail = child;
    listener->q_tail = &child->acc_next;
    listener->q_len++;
    child->parent = listener;
    return child;
}

/* 0x004d48 */
static void __attribute__((noinline))
ptpMarkConnected(PtpPcb *pcb)
{
    PtpPcb *parent;

    sceNet_lib_A93A93E9(&pcb->timer_rexmt);
    pcb->state = PTP_STATE_ESTABLISHED;
    pcb->sock->flags = (pcb->sock->flags & ~0x6000u) | 0x4000u;
    parent = pcb->parent;

    if (parent != NULL) {
        if ((parent->rx_wait_flags & 1u) != 0) {
            parent->rx_wait_flags &= ~1u;
            sceNet_lib_7574FDA1(&parent->rx_head);
            parent = pcb->parent;
        }
        if ((parent->rx_wait_flags & 2u) != 0) {
            parent->rx_wait_flags &= ~2u;
            sceNet_lib_7574FDA1(&g_global_wait_head);
        }
    } else {
        if ((pcb->rx_wait_flags & 1u) != 0) {
            pcb->rx_wait_flags &= ~1u;
            sceNet_lib_7574FDA1(&pcb->rx_head);
        }
        if ((pcb->rx_wait_flags & 2u) != 0) {
            pcb->rx_wait_flags &= ~2u;
            sceNet_lib_7574FDA1(&g_global_wait_head);
        }
    }

    sceNet_lib_A8B6205A(&pcb->timer_probe, pcb->rexmt_intvl, ptpProbeTimeout, pcb);
}

/* 0x004e3c */
static void __attribute__((noinline))
ptpDisconnectPcb(PtpPcb *pcb)
{
    PtpPcb *parent;

    sceNet_lib_A93A93E9(&pcb->timer_probe);
    sceNet_lib_A93A93E9(&pcb->timer_rexmt);

    if ((pcb->sock->flags & 0x6000u) != 0)
        ptpSendControlPacket(pcb, PTP_PKT_RST);

    pcb->sock->flags = (pcb->sock->flags & ~0xB000u) | 0x8000u;
    pcb->state = PTP_STATE_CLOSED;

    parent = pcb->parent;
    if (parent != NULL) {
        if ((parent->rx_wait_flags & 1u) != 0) {
            parent->rx_wait_flags &= ~1u;
            sceNet_lib_7574FDA1(&parent->rx_head);
            parent = pcb->parent;
        }
        if ((parent->rx_wait_flags & 2u) != 0) {
            parent->rx_wait_flags &= ~2u;
            sceNet_lib_7574FDA1(&g_global_wait_head);
            parent = pcb->parent;
        }
        if ((parent->tx_wait_flags & 1u) != 0) {
            parent->tx_wait_flags &= ~1u;
            sceNet_lib_7574FDA1(&parent->tx_head);
            parent = pcb->parent;
        }
        if ((parent->tx_wait_flags & 2u) != 0) {
            parent->tx_wait_flags &= ~2u;
            sceNet_lib_7574FDA1(&g_global_wait_head);
        }
    } else {
        if ((pcb->rx_wait_flags & 1u) != 0) {
            pcb->rx_wait_flags &= ~1u;
            sceNet_lib_7574FDA1(&pcb->rx_head);
        }
        if ((pcb->rx_wait_flags & 2u) != 0) {
            pcb->rx_wait_flags &= ~2u;
            sceNet_lib_7574FDA1(&g_global_wait_head);
        }
        if ((pcb->tx_wait_flags & 1u) != 0) {
            pcb->tx_wait_flags &= ~1u;
            sceNet_lib_7574FDA1(&pcb->tx_head);
        }
        if ((pcb->tx_wait_flags & 2u) != 0) {
            pcb->tx_wait_flags &= ~2u;
            sceNet_lib_7574FDA1(&g_global_wait_head);
        }
    }
}

/* 0x004fc8 */
s32 __attribute__((noinline))
ptpPollEvents(AdhocSocket *sock, s32 events)
{
    PtpPcb *pcb = (PtpPcb *)sock->pcb;
    s32 revents = 0;

    if ((events & 2) != 0 && pcb->rx_cc >= pcb->rx_lowat)
        revents = 2;

    if ((events & 1) != 0 && pcb->tx_hiwat - pcb->tx_cc >= pcb->tx_lowat)
        revents |= 1;

    if ((events & 4) != 0 && (sock->flags & 0x4000u) != 0)
        revents |= 4;

    if ((events & 8) != 0 && (sock->flags & 0x1000u) != 0) {
        PtpPcb *child = pcb->q_head;
        while (child != NULL) {
            if ((child->sock->flags & 0x4000u) != 0) {
                revents |= 8;
                break;
            }
            child = child->acc_next;
        }
    }

    if ((events & 0x10) != 0 && pcb->tx_cc == 0)
        revents |= 0x10;

    if (revents == 0) {
        if ((events & 2) != 0)
            pcb->rx_wait_flags |= 2u;
        if ((events & 1) != 0)
            pcb->tx_wait_flags |= 2u;
        if ((events & 4) != 0)
            pcb->rx_wait_flags |= 2u;
        if ((events & 8) != 0)
            pcb->rx_wait_flags |= 2u;
        if ((events & 0x10) != 0)
            pcb->tx_wait_flags |= 2u;
    }
    return revents;
}

/* 0x005104 */
s32 sceNetAdhocGetPtpStat(s32 *buflen, ScePtpStatOut *buf)
{
    s32 ret = 0;
    s32 intr;
    s32 written = 0;
    ScePtpStatOut *prev = NULL;

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
        PtpPcb *pcb = g_ptp_list.head;
        ScePtpStatOut *out = buf;
        while (pcb != NULL) {
            if (buf != NULL) {
                if (*buflen < written + (s32)sizeof(ScePtpStatOut))
                    break;
                out->next = NULL;
                out->ptp_id = pcb->sock->id;
                sceKernelMemcpy(out->self_mac, pcb->self_mac, 6);
                sceKernelMemcpy(out->peer_mac, pcb->peer_mac, 6);
                out->self_port = pcb->self_port;
                out->peer_port = pcb->peer_port;
                out->sent_data = pcb->tx_cc;
                out->rcvd_data = pcb->rx_cc;
                out->state = pcb->state;
                if (prev != NULL)
                    prev->next = out;
                prev = out;
                out++;
            }
            written += (s32)sizeof(ScePtpStatOut);
            pcb = pcb->next;
        }
    }

    sceNet_lib_515B2F33(intr);
    *buflen = written;
    return ret;
}

/* 0x005288 */
s32 __attribute__((noinline))
ptpInit(void)
{
    g_ptp_list.head = NULL;
    g_ptp_list.tail = &g_ptp_list.head;
    g_ptp_anon_port = (u16)(sceNet_lib_1858883D() / 0x20009u + 0x4000u);
    g_ptp_conn_seq = sceNet_lib_1858883D();
    return 0;
}

/* 0x0052e0 */
void __attribute__((noinline))
ptpTerm(void)
{
    PtpPcb *pcb;

    while ((pcb = g_ptp_list.head) != NULL) {
        AdhocSocket *sock = pcb->sock;
        PtpPcb *victim = (PtpPcb *)sock->pcb;
        sceNet_lib_A93A93E9(&pcb->timer_rexmt);
        ptpDropQueues(pcb);
        sock->pcb = NULL;
        if (victim->next != NULL)
            victim->next->prev = victim->prev;
        else
            g_ptp_list.tail = victim->prev;
        *victim->prev = victim->next;
        sceNetIfhandle_lib_76BAD213(victim);
    }
}

/* 0x00536c */
static PtpPcb *__attribute__((noinline))
ptpFindPcb(const u8 *self_mac, u16 self_port, const u8 *peer_mac, u16 peer_port)
{
    PtpPcb *pcb = g_ptp_list.head;

    while (pcb != NULL) {
        if (sceNet_lib_E0A81C7C(pcb->self_mac, self_mac, 6) == 0 && pcb->self_port == self_port) {
            if (peer_mac == NULL)
                return pcb;
            if (sceNet_lib_E0A81C7C(pcb->peer_mac, peer_mac, 6) == 0 && pcb->peer_port == peer_port)
                return pcb;
        }
        pcb = pcb->next;
    }
    return NULL;
}

/* 0x00542c */
static s32 __attribute__((noinline))
ptpBindPcb(PtpPcb *pcb, const u8 *self_mac, u16 self_port, const u8 *peer_mac, u16 peer_port)
{
    u16 chosen_port = self_port;
    PtpPcb *cur;

    if (sceNet_lib_D60225A3(self_mac) == NULL)
        return ADHOC_ERR_INVALID_ADDR;

    if (chosen_port == 0) {
        chosen_port = ptpAllocAnonPort(self_mac);
        if (chosen_port == 0)
            return ADHOC_ERR_PORT_NOT_AVAIL;
    } else if (chosen_port == 0xFFFF) {
        return ADHOC_ERR_INVALID_PORT;
    }

    for (cur = g_ptp_list.head; cur != NULL; cur = cur->next) {
        if (sceNet_lib_E0A81C7C(cur->self_mac, self_mac, 6) != 0)
            continue;
        if (cur->self_port != chosen_port)
            continue;
        if (peer_mac != NULL) {
            if (sceNet_lib_E0A81C7C(cur->peer_mac, peer_mac, 6) != 0)
                continue;
            if (cur->peer_port != peer_port)
                continue;
        }
        return ADHOC_ERR_PORT_IN_USE;
    }

    sceKernelMemcpy(pcb->self_mac, self_mac, 6);
    pcb->self_port = chosen_port;
    pcb->mss = 1444;
    pcb->sock->chan_tx = &pcb->tx_head;
    pcb->sock->chan_rx = &pcb->rx_head;
    pcb->sock->chan_poll = &g_global_wait_head;
    return 0;
}

/* 0x005590 */
void __attribute__((noinline))
ptpFreePcb(AdhocSocket *sock)
{
    PtpPcb *pcb = (PtpPcb *)sock->pcb;
    sock->pcb = NULL;
    if (pcb->next != NULL)
        pcb->next->prev = pcb->prev;
    else
        g_ptp_list.tail = pcb->prev;
    *pcb->prev = pcb->next;
    sceNetIfhandle_lib_76BAD213(pcb);
}

/* 0x0055dc */
static s32 __attribute__((noinline))
ptpSetBufSizes(PtpPcb *pcb, u32 tx_bufsize, u32 rx_bufsize)
{
    pcb->rx_lowat = 1;
    pcb->tx_hiwat = tx_bufsize;
    pcb->rx_hiwat = rx_bufsize;
    pcb->tx_lowat = 1;
    return 0;
}

/* 0x0055f8 */
static void __attribute__((noinline))
ptpSetRexmtParams(PtpPcb *pcb, u32 rexmt_intvl, s32 rexmt_cnt)
{
    pcb->rexmt_max = rexmt_cnt;
    pcb->rexmt_intvl = rexmt_intvl;
}

/* 0x005604 */
static s32 __attribute__((noinline))
ptpStartConnect(PtpPcb *pcb, const u8 *peer_mac, u16 peer_port)
{
    if (sceNet_lib_E0A81C7C(peer_mac, g_zero_mac, 6) == 0)
        return ADHOC_ERR_INVALID_ADDR;
    if ((peer_mac[0] & 1u) != 0 && sceNet_lib_E0A81C7C(peer_mac, g_bcast_mac, 6) != 0)
        return ADHOC_ERR_INVALID_ADDR;
    if (peer_port == 0 || peer_port == 0xFFFF)
        return ADHOC_ERR_INVALID_PORT;

    sceKernelMemcpy(pcb->peer_mac, peer_mac, 6);
    pcb->state = PTP_STATE_SYN_SENT;
    pcb->conn_id = g_ptp_conn_seq++;
    pcb->peer_port = peer_port;
    pcb->sock->flags |= 0x2000u;

    ptpSendControlPacket(pcb, PTP_PKT_SYN);
    pcb->rexmt_rem = pcb->rexmt_max;
    sceNet_lib_A8B6205A(&pcb->timer_rexmt, pcb->rexmt_intvl, ptpRexmtTimeout, pcb);
    return 0;
}

/* 0x00572c */
static s32 __attribute__((noinline))
ptpStartListen(PtpPcb *pcb, s32 backlog)
{
    pcb->state = PTP_STATE_LISTEN;
    pcb->q_maxlen = backlog;
    pcb->sock->flags |= 0x1000u;
    return 0;
}

/* 0x005750 */
static void __attribute__((noinline))
ptpFreeListenQueue(PtpPcb *pcb)
{
    PtpPcb *child;

    while ((child = pcb->q_head) != NULL) {
        AdhocSocket *child_sock = child->sock;
        PtpPcb *victim;

        if (child->acc_next != NULL)
            child->acc_next->acc_prev = child->acc_prev;
        else
            pcb->q_tail = child->acc_prev;
        *child->acc_prev = child->acc_next;
        pcb->q_len--;

        ptpDisconnectPcb(child);
        ptpFreeListenQueue(child);

        victim = (PtpPcb *)child_sock->pcb;
        child_sock->pcb = NULL;
        if (victim->next != NULL)
            victim->next->prev = victim->prev;
        else
            g_ptp_list.tail = victim->prev;
        *victim->prev = victim->next;
        sceNetIfhandle_lib_76BAD213(victim);
        adhocSockTryFree(child_sock);
    }

    sceKernelMemset(pcb->self_mac, 0, 6);
    sceKernelMemset(pcb->peer_mac, 0, 6);
    pcb->self_port = 0;
    ptpDropQueues(pcb);
}

/* 0x00584c */
static u16 __attribute__((noinline))
ptpAllocAnonPort(const u8 *self_mac)
{
    u16 cand = (g_ptp_anon_port == 0x7FFF) ? 0x4000 : (u16)(g_ptp_anon_port + 1);

    while (cand != g_ptp_anon_port) {
        PtpPcb *cur = g_ptp_list.head;
        while (cur != NULL) {
            if (sceNet_lib_E0A81C7C(cur->self_mac, self_mac, 6) == 0 && cur->self_port == cand)
                break;
            cur = cur->next;
        }
        if (cur == NULL) {
            g_ptp_anon_port = cand;
            return cand;
        }
        cand = (cand == 0x7FFF) ? 0x4000 : (u16)(cand + 1);
    }
    return 0;
}

/* 0x005938 */
void __attribute__((noinline))
ptpInputPacket(AdhocMbuf *mbuf)
{
    u8 *hdr;
    u16 body_len;
    u16 sport;
    u16 dport;
    u32 seq_id;
    u32 opt_wnd = 0;
    u8 pkt_type;
    u8 *src_mac;
    s32 pkt_total;

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

    sceKernelMemcpy(&body_len, hdr + 50, 2);
    body_len = adhocSwap16(body_len);
    sceKernelMemcpy(&sport, hdr + 52, 2);
    sport = adhocSwap16(sport);
    sceKernelMemcpy(&dport, hdr + 54, 2);
    dport = adhocSwap16(dport);
    sceKernelMemcpy(&seq_id, hdr + 56, 4);
    seq_id = adhocSwap32(seq_id);

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

    pkt_type = hdr[49];
    if ((u8)(pkt_type - 1) >= 7u) {
        sceNetIfhandle_lib_F56FAC82(mbuf);
        return;
    }

    src_mac = hdr + 6;
    if (pkt_type == PTP_PKT_SYN || pkt_type == PTP_PKT_SYN_ACK || pkt_type == PTP_PKT_DATA_ACK) {
        if (body_len < 16u) {
            sceNetIfhandle_lib_F56FAC82(mbuf);
            return;
        }
        if (mbuf->m_len < 64)
            sceNetIfhandle_lib_9A6261EC(mbuf, 60, 4, &opt_wnd);
        else
            sceKernelMemcpy(&opt_wnd, mbuf->m_data + 60, 4);
        opt_wnd = adhocSwap32(opt_wnd);
    }

    for (;;) {
        PtpPcb *pcb = ptpFindPcb(hdr, dport, src_mac, sport);
        if (pcb == NULL) {
            pkt_type = hdr[49];
            if (pkt_type == PTP_PKT_SYN)
                pcb = ptpFindPcb(hdr, dport, g_zero_mac, 0);
            if (pcb == NULL) {
                if (pkt_type != PTP_PKT_RST)
                    ptpBuildAndSendPacket(hdr, dport, src_mac, sport, PTP_PKT_RST, 0, NULL);
                g_ptp_stats.rx_no_pcb++;
                break;
            }
        } else {
            pkt_type = hdr[49];
        }

        switch (pkt_type) {
        case PTP_PKT_SYN:
            switch (pcb->state) {
            case PTP_STATE_CLOSED:
                g_ptp_stats.rx_syn_drop++;
                break;
            case PTP_STATE_LISTEN: {
                PtpPcb *child = ptpSpawnChild(pcb, src_mac, sport, seq_id);
                if (child != NULL) {
                    child->peer_wnd = opt_wnd;
                    if (opt_wnd < child->mss)
                        child->mss = opt_wnd;
                    ptpSendControlPacket(child, PTP_PKT_SYN_ACK);
                    child->rexmt_rem = child->rexmt_max;
                    sceNet_lib_A8B6205A(&child->timer_rexmt, child->rexmt_intvl, ptpRexmtTimeout, child);
                    g_ptp_stats.rx_syn_ok++;
                } else {
                    g_ptp_stats.rx_syn_drop++;
                }
                break;
            }
            case PTP_STATE_SYN_SENT:
            case PTP_STATE_ESTABLISHED:
                ptpDisconnectPcb(pcb);
                continue;
            case PTP_STATE_SYN_RCVD:
                if (pcb->conn_id == seq_id) {
                    ptpSendControlPacket(pcb, PTP_PKT_SYN_ACK);
                    pcb->rexmt_rem = pcb->rexmt_max;
                    sceNet_lib_A8B6205A(&pcb->timer_rexmt, pcb->rexmt_intvl, ptpRexmtTimeout, pcb);
                    g_ptp_stats.rx_syn_dup++;
                    break;
                }
                ptpDisconnectPcb(pcb);
                continue;
            default:
                break;
            }
            break;

        case PTP_PKT_SYN_ACK:
            switch (pcb->state) {
            case PTP_STATE_CLOSED:
                g_ptp_stats.rx_syn_ack_drop++;
                break;
            case PTP_STATE_LISTEN:
            case PTP_STATE_SYN_RCVD:
                ptpBuildAndSendPacket(hdr, dport, src_mac, sport, PTP_PKT_RST, 0, NULL);
                g_ptp_stats.rx_syn_ack_drop++;
                break;
            case PTP_STATE_SYN_SENT:
                if (pcb->conn_id == seq_id) {
                    pcb->peer_wnd = opt_wnd;
                    if (opt_wnd < pcb->mss)
                        pcb->mss = opt_wnd;
                    ptpMarkConnected(pcb);
                    ptpSendControlPacket(pcb, PTP_PKT_ACK);
                    g_ptp_stats.rx_syn_ack_ok++;
                } else {
                    ptpBuildAndSendPacket(hdr, dport, src_mac, sport, PTP_PKT_RST, 0, NULL);
                    g_ptp_stats.rx_syn_ack_drop++;
                }
                break;
            case PTP_STATE_ESTABLISHED:
                if (pcb->conn_id == seq_id) {
                    ptpSendControlPacket(pcb, PTP_PKT_ACK);
                    g_ptp_stats.rx_syn_ack_dup++;
                } else {
                    ptpBuildAndSendPacket(hdr, dport, src_mac, sport, PTP_PKT_RST, 0, NULL);
                    g_ptp_stats.rx_syn_ack_drop++;
                }
                break;
            default:
                break;
            }
            break;

        case PTP_PKT_ACK:
            switch (pcb->state) {
            case PTP_STATE_CLOSED:
                g_ptp_stats.rx_ack_drop++;
                break;
            case PTP_STATE_LISTEN:
            case PTP_STATE_SYN_SENT:
                ptpBuildAndSendPacket(hdr, dport, src_mac, sport, PTP_PKT_RST, 0, NULL);
                g_ptp_stats.rx_ack_drop++;
                break;
            case PTP_STATE_SYN_RCVD:
                if (pcb->conn_id == seq_id) {
                    ptpMarkConnected(pcb);
                    g_ptp_stats.rx_ack_ok++;
                }
                if (pcb->conn_id == seq_id) {
                    g_ptp_stats.rx_ack_dup++;
                } else {
                    ptpBuildAndSendPacket(hdr, dport, src_mac, sport, PTP_PKT_RST, 0, NULL);
                    g_ptp_stats.rx_ack_drop++;
                }
                break;
            case PTP_STATE_ESTABLISHED:
                if (pcb->conn_id == seq_id) {
                    g_ptp_stats.rx_ack_dup++;
                } else {
                    ptpBuildAndSendPacket(hdr, dport, src_mac, sport, PTP_PKT_RST, 0, NULL);
                    g_ptp_stats.rx_ack_drop++;
                }
                break;
            default:
                break;
            }
            break;

        case PTP_PKT_DATA:
            switch (pcb->state) {
            case PTP_STATE_CLOSED:
                g_ptp_stats.rx_data_drop++;
                break;
            case PTP_STATE_LISTEN:
            case PTP_STATE_SYN_SENT:
                ptpBuildAndSendPacket(hdr, dport, src_mac, sport, PTP_PKT_RST, 0, NULL);
                g_ptp_stats.rx_data_drop++;
                break;
            case PTP_STATE_SYN_RCVD:
                if (pcb->rcv_nxt == seq_id) {
                    ptpSendControlPacket(pcb, PTP_PKT_SYN_ACK);
                    pcb->rexmt_rem = pcb->rexmt_max;
                    sceNet_lib_A8B6205A(&pcb->timer_rexmt, pcb->rexmt_intvl, ptpRexmtTimeout, pcb);
                }
                break;
            case PTP_STATE_ESTABLISHED: {
                s32 diff = (s32)(pcb->rcv_nxt - seq_id);
                if (diff != 0) {
                    if (diff <= 0) {
                        if ((pcb->proto_flags & 1u) == 0) {
                            pcb->proto_flags |= 1u;
                            ptpSendControlPacket(pcb, PTP_PKT_NACK);
                        }
                        g_ptp_stats.rx_data_drop++;
                    } else {
                        ptpSendControlPacket(pcb, PTP_PKT_DATA_ACK);
                        g_ptp_stats.rx_data_old++;
                    }
                    break;
                }

                sceNetIfhandle_lib_BFF3CEA5(mbuf, 60);
                if ((s32)(pcb->rx_hiwat - pcb->rx_cc) < mbuf->m_pkthdr_len) {
                    g_ptp_stats.rx_data_full++;
                    break;
                }

                mbuf->m_nextpkt = NULL;
                pcb->proto_flags &= ~1u;
                if (pcb->rx_tail != NULL)
                    pcb->rx_tail->m_nextpkt = mbuf;
                else
                    pcb->rx_head = mbuf;
                pcb->rx_tail = mbuf;
                pcb->rx_cc += (u32)mbuf->m_pkthdr_len;
                pcb->rcv_nxt++;

                ptpSendControlPacket(pcb, PTP_PKT_DATA_ACK);
                if ((pcb->rx_wait_flags & 1u) != 0) {
                    pcb->rx_wait_flags &= ~1u;
                    sceNet_lib_7574FDA1(&pcb->rx_head);
                }
                if ((pcb->rx_wait_flags & 2u) != 0) {
                    pcb->rx_wait_flags &= ~2u;
                    sceNet_lib_7574FDA1(&g_global_wait_head);
                }
                g_ptp_stats.rx_data_ok++;
                return;
            }
            default:
                break;
            }
            break;

        case PTP_PKT_DATA_ACK:
            if (pcb->state < PTP_STATE_ESTABLISHED) {
                if (pcb->state >= PTP_STATE_CLOSED) {
                    if (pcb->state > PTP_STATE_CLOSED)
                        ptpBuildAndSendPacket(hdr, dport, src_mac, sport, PTP_PKT_RST, 0, NULL);
                    g_ptp_stats.rx_data_ack_drop++;
                }
            } else if (pcb->state == PTP_STATE_ESTABLISHED) {
                if ((s32)(pcb->snd_una - seq_id) <= 0) {
                    s32 bad_ack = 0;
                    while ((s32)(pcb->snd_una - seq_id) <= 0) {
                        AdhocMbuf *head = pcb->tx_head;
                        if (head == NULL) {
                            ptpDisconnectPcb(pcb);
                            g_ptp_stats.rx_data_ack_drop++;
                            bad_ack = 1;
                            break;
                        }
                        pcb->tx_head = head->m_nextpkt;
                        if (pcb->tx_head == NULL)
                            pcb->tx_tail = NULL;
                        pcb->snd_una++;
                        pcb->tx_cc -= (u32)head->m_pkthdr_len;
                        head->m_nextpkt = NULL;
                        sceNetIfhandle_lib_F56FAC82(head);
                    }
                    if (!bad_ack) {
                        if ((s32)(pcb->snd_nxt - pcb->snd_una) < 0)
                            pcb->snd_nxt = pcb->snd_una;
                        if (pcb->snd_una == pcb->snd_max)
                            sceNet_lib_A93A93E9(&pcb->timer_rexmt);
                        pcb->peer_wnd = opt_wnd;
                        if ((pcb->tx_wait_flags & 1u) != 0) {
                            pcb->tx_wait_flags &= ~1u;
                            sceNet_lib_7574FDA1(&pcb->tx_head);
                        }
                        if ((pcb->tx_wait_flags & 2u) != 0) {
                            pcb->tx_wait_flags &= ~2u;
                            sceNet_lib_7574FDA1(&g_global_wait_head);
                        }
                        ptpTransmitData(pcb, 0);
                        g_ptp_stats.rx_data_ack_ok++;
                    }
                } else {
                    pcb->peer_wnd = opt_wnd;
                    if ((pcb->tx_wait_flags & 1u) != 0) {
                        pcb->tx_wait_flags &= ~1u;
                        sceNet_lib_7574FDA1(&pcb->tx_head);
                    }
                    if ((pcb->tx_wait_flags & 2u) != 0) {
                        pcb->tx_wait_flags &= ~2u;
                        sceNet_lib_7574FDA1(&g_global_wait_head);
                    }
                    ptpTransmitData(pcb, 0);
                    g_ptp_stats.rx_data_ack_dup++;
                }
            }
            break;

        case PTP_PKT_NACK:
            if (pcb->state == PTP_STATE_ESTABLISHED) {
                if (pcb->snd_una == seq_id) {
                    sceNet_lib_A93A93E9(&pcb->timer_rexmt);
                    pcb->snd_nxt = pcb->snd_una;
                    ptpTransmitData(pcb, 0);
                    g_ptp_stats.rx_nack_ok++;
                    break;
                }
                g_ptp_stats.rx_nack_drop++;
            } else if (pcb->state < PTP_STATE_ESTABLISHED) {
                if (pcb->state >= PTP_STATE_CLOSED) {
                    if (pcb->state > PTP_STATE_CLOSED)
                        ptpBuildAndSendPacket(hdr, dport, src_mac, sport, PTP_PKT_RST, 0, NULL);
                    g_ptp_stats.rx_nack_drop++;
                }
            }
            break;

        case PTP_PKT_RST:
            if (pcb->state == PTP_STATE_CLOSED || pcb->state == PTP_STATE_LISTEN) {
                g_ptp_stats.rx_rst_drop++;
            } else if (pcb->state >= PTP_STATE_SYN_SENT && pcb->state <= PTP_STATE_ESTABLISHED) {
                g_ptp_stats.rx_rst_ok++;
                ptpDisconnectPcb(pcb);
            }
            break;

        default:
            break;
        }
        break;
    }

    if (mbuf != NULL)
        sceNetIfhandle_lib_F56FAC82(mbuf);
}

/* 0x00637c */
static s32 __attribute__((noinline))
ptpBuildAndSendPacket(const u8 *src_mac, u16 src_port, const u8 *dst_mac, u16 dst_port, u8 pkt_type, u32 seq_id, AdhocMbuf *payload)
{
    AdhocIfnet *ifp = (AdhocIfnet *)sceNet_lib_D60225A3(src_mac);
    s32 payload_len;
    AdhocMbuf *mbuf;
    s32 pad_len;
    s32 aligned_len;
    u8 *hdr;
    u16 be16;
    u32 be32;

    if (ifp == NULL || ifp->snd_len >= ifp->snd_maxlen)
        return (s32)ifp;

    payload_len = (payload != NULL) ? payload->m_pkthdr_len : 0;
    mbuf = (AdhocMbuf *)sceNetIfhandle_lib_15CFE3C0(sizeof(AdhocMbuf));
    if (mbuf == NULL)
        return 0;

    pad_len = (payload_len + 12) % 16;
    mbuf->m_type = 1;
    mbuf->m_data = mbuf->payload.inline_buf;
    mbuf->m_flags = 2;
    mbuf->m_next = NULL;
    mbuf->m_nextpkt = NULL;
    mbuf->m_pkthdr_aux = 0;

    if (pad_len > 0)
        pad_len = 16 - pad_len;
    aligned_len = payload_len + pad_len;

    if (aligned_len + 60 >= 209) {
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
        if ((mbuf->m_flags & 1u) == 0)
            return sceNetIfhandle_lib_F8825DC4(mbuf);
    }

    hdr = mbuf->m_data;
    sceKernelMemcpy(hdr, dst_mac, 6);
    sceKernelMemcpy(hdr + 6, src_mac, 6);
    sceKernelMemcpy(hdr + 12, &g_adhoc_ethertype, 2);
    sceKernelMemcpy(hdr + 14, &g_adhoc_hdr_ver, 2);

    hdr = mbuf->m_data;
    hdr[48] = 2;
    hdr[49] = pkt_type;
    be16 = adhocSwap16((u16)(payload_len + 12));
    sceKernelMemcpy(hdr + 50, &be16, 2);
    be16 = adhocSwap16(src_port);
    sceKernelMemcpy(hdr + 52, &be16, 2);
    be16 = adhocSwap16(dst_port);
    sceKernelMemcpy(hdr + 54, &be16, 2);
    be32 = adhocSwap32(seq_id);
    sceKernelMemcpy(hdr + 56, &be32, 4);

    if (payload_len > 0) {
        if (payload->m_len < payload_len)
            sceNetIfhandle_lib_9A6261EC(payload, 0, payload_len, mbuf->m_data + 60);
        else
            sceKernelMemcpy(mbuf->m_data + 60, payload->m_data, (u32)payload_len);
    }
    if (pad_len > 0)
        sceKernelMemset(mbuf->m_data + 60 + payload_len, 0, (u32)pad_len);

    mbuf->m_rcvif = NULL;
    mbuf->m_pkthdr_len = aligned_len + 60;
    mbuf->m_len = aligned_len + 60;
    ifp->if_obytes += (u64)(s64)mbuf->m_pkthdr_len;
    return adhocIfOutput(ifp, mbuf);
}

/* 0x00668c */
static void __attribute__((noinline))
ptpSendControlPacket(PtpPcb *pcb, u8 pkt_type)
{
    u32 seq_id;
    AdhocMbuf *opt_mbuf = NULL;

    switch (pkt_type) {
    case PTP_PKT_SYN:
    case PTP_PKT_SYN_ACK:
    case PTP_PKT_ACK:
    case PTP_PKT_RST:
        seq_id = pcb->conn_id;
        break;
    case PTP_PKT_DATA_ACK:
        seq_id = pcb->rcv_nxt - 1;
        break;
    case PTP_PKT_NACK:
        seq_id = pcb->rcv_nxt;
        break;
    default:
        seq_id = 0;
        break;
    }

    if (pkt_type == PTP_PKT_SYN || pkt_type == PTP_PKT_SYN_ACK || pkt_type == PTP_PKT_DATA_ACK) {
        u32 be_wnd;
        opt_mbuf = (AdhocMbuf *)sceNetIfhandle_lib_15CFE3C0(sizeof(AdhocMbuf));
        if (opt_mbuf == NULL)
            return;
        opt_mbuf->m_pkthdr_aux = 0;
        opt_mbuf->m_type = 1;
        opt_mbuf->m_flags = 2;
        opt_mbuf->m_next = NULL;
        opt_mbuf->m_nextpkt = NULL;
        opt_mbuf->m_data = opt_mbuf->payload.inline_buf;
        be_wnd = adhocSwap32(pcb->rx_hiwat - pcb->rx_cc);
        sceKernelMemcpy(opt_mbuf->payload.inline_buf, &be_wnd, 4);
        opt_mbuf->m_pkthdr_len = 4;
        opt_mbuf->m_len = 4;
        opt_mbuf->m_rcvif = NULL;
    }

    switch (pkt_type) {
    case PTP_PKT_SYN:
        g_ptp_stats.tx_syn++;
        break;
    case PTP_PKT_SYN_ACK:
        g_ptp_stats.tx_syn_ack++;
        break;
    case PTP_PKT_ACK:
        g_ptp_stats.tx_ack++;
        break;
    case PTP_PKT_DATA_ACK:
        g_ptp_stats.tx_data_ack++;
        break;
    case PTP_PKT_NACK:
        g_ptp_stats.tx_nack++;
        break;
    case PTP_PKT_RST:
        g_ptp_stats.tx_rst++;
        break;
    default:
        break;
    }

    ptpBuildAndSendPacket(pcb->self_mac, pcb->self_port, pcb->peer_mac, pcb->peer_port, pkt_type, seq_id, opt_mbuf);
    if (opt_mbuf != NULL)
        sceNetIfhandle_lib_F56FAC82(opt_mbuf);
}

/* 0x006878 */
static void __attribute__((noinline))
ptpTransmitData(PtpPcb *pcb, s32 is_rexmt)
{
    s32 sent = 0;
    s32 in_flight = 0;
    AdhocMbuf *cur = pcb->tx_head;
    u32 seq = pcb->snd_una;
    s32 start_timer = 0;

    while (cur != NULL && seq != pcb->snd_nxt) {
        in_flight += cur->m_pkthdr_len;
        cur = cur->m_nextpkt;
        seq++;
    }

    if (cur != NULL) {
        while (cur != NULL) {
            if ((s32)pcb->peer_wnd - in_flight < cur->m_pkthdr_len)
                break;
            sent++;
            ptpBuildAndSendPacket(pcb->self_mac, pcb->self_port, pcb->peer_mac, pcb->peer_port, PTP_PKT_DATA, pcb->snd_nxt, cur);
            if (cur->hdr_meta.meta.send_time == 0)
                cur->hdr_meta.meta.send_time = sceKernelGetSystemTimeWide();
            pcb->snd_nxt++;
            in_flight += cur->m_pkthdr_len;
            cur = cur->m_next;
            g_ptp_stats.tx_data++;
        }

        if (sent == 0 && cur != NULL) {
            if (is_rexmt != 0) {
                ptpBuildAndSendPacket(pcb->self_mac, pcb->self_port, pcb->peer_mac, pcb->peer_port, PTP_PKT_DATA, pcb->snd_una - 1, NULL);
                g_ptp_stats.tx_probe++;
                return;
            }
            if ((pcb->timer_rexmt.flags & 1u) == 0) {
                g_ptp_stats.tx_wnd_probe++;
                pcb->rexmt_rem = pcb->rexmt_max;
                sceNet_lib_A8B6205A(&pcb->timer_rexmt, pcb->rexmt_intvl, ptpRexmtTimeout, pcb);
            }
            return;
        }
    }

    if (is_rexmt == 0 && sent > 0)
        start_timer = 1;
    if (start_timer != 0) {
        pcb->rexmt_rem = pcb->rexmt_max;
        sceNet_lib_A8B6205A(&pcb->timer_rexmt, pcb->rexmt_intvl, ptpRexmtTimeout, pcb);
    }
}

/* 0x006a54 */
static void __attribute__((noinline))
ptpRexmtTimeout(PtpPcb *pcb)
{
    s32 state = pcb->state;
    if ((u32)state < 2u)
        return;

    if (pcb->rexmt_rem-- <= 0) {
        ptpDisconnectPcb(pcb);
        return;
    }

    if (state == PTP_STATE_SYN_SENT) {
        ptpSendControlPacket(pcb, PTP_PKT_SYN);
    } else if (state == PTP_STATE_SYN_RCVD) {
        ptpSendControlPacket(pcb, PTP_PKT_SYN_ACK);
    } else if (state == PTP_STATE_ESTABLISHED) {
        pcb->snd_nxt = pcb->snd_una;
        ptpTransmitData(pcb, 1);
        g_ptp_stats.rexmt_data++;
    }
    sceNet_lib_A8B6205A(&pcb->timer_rexmt, pcb->rexmt_intvl, ptpRexmtTimeout, pcb);
}

/* 0x006b20 */
static void __attribute__((noinline))
ptpProbeTimeout(PtpPcb *pcb)
{
    if (pcb->state == PTP_STATE_ESTABLISHED) {
        AdhocMbuf *head = pcb->tx_head;
        if (head != NULL) {
            s64 now = sceKernelGetSystemTimeWide();
            if (head->hdr_meta.meta.send_time != 0) {
                u64 elapsed = (u64)(now - head->hdr_meta.meta.send_time);
                u64 limit = (u64)(s64)(pcb->rexmt_max + 1) * (u64)pcb->rexmt_intvl;
                if (elapsed > limit) {
                    ptpDisconnectPcb(pcb);
                    return;
                }
            }
        }
        sceNet_lib_A8B6205A(&pcb->timer_probe, pcb->rexmt_intvl, ptpProbeTimeout, pcb);
    }
}
