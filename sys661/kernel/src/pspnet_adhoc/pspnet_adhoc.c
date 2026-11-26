#include "adhoc_int.h"

const u16 g_adhoc_ethertype = 0xC888;
const u16 g_adhoc_hdr_ver = 0x0200;
const u8 g_bcast_mac[6] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
const u8 g_zero_mac[6] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

s32 g_adhoc_inited;
u32 g_global_wait_head;
static s32 g_last_sock_id;

static struct {
    AdhocMbuf *head;
    AdhocMbuf *tail;
    s32 count;
    s32 max_count;
    s32 drops;
} g_rx_queue;

static struct {
    AdhocSocket *head;
    AdhocSocket **tail;
} g_sock_list;

static void adhocRxSoftIntr(void);
static s32 adhocGetRxQueue(s32 unused_first, s32 unused_second, void **out);
static void adhocIfDetachNotify(AdhocIfnet *ifp);
static s32 adhocPollSockets(AdhocPollEntry *entries, s32 count);
static s32 adhocSockInit(void);
static void adhocSockTerm(void);
static s32 adhocSockHasActive(void);
static void adhocSockWakeWaiters(AdhocSocket *sock, s16 alert_flags);

static inline void adhocUnlinkWaiter(AdhocSocket *sock, AdhocWaiter *waiter)
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

/* 0x000000 */
s32 sceNetAdhocInit(void)
{
    s32 intr;
    s32 ret;

    if (_sceKernelCheckThreadStack() < 2032)
        return ADHOC_ERR_STACK_SMALL;

    intr = sceNet_lib_94B44F26();
    if (intr < 0)
        return intr;

    ret = ADHOC_ERR_ALREADY_INITIALIZED;
    if (g_adhoc_inited <= 0) {
        sceKernelMemset(&g_rx_queue, 0, sizeof(g_rx_queue));
        g_rx_queue.max_count = 50;

        ret = adhocSockInit();
        if (ret >= 0) {
            ret = pdpInit();
            if (ret >= 0) {
                ret = ptpInit();
                if (ret >= 0) {
                    ret = sceNet_lib_B3A48B7F(adhocRxSoftIntr, adhocIfDetachNotify);
                    if (ret >= 0) {
                        ret = sceNet_lib_1F94AFD9(adhocGetRxQueue);
                        if (ret >= 0) {
                            if (gameModeInit() >= 0) {
                                g_adhoc_inited = 1;
                                sceNet_lib_515B2F33(intr);
                                return 0;
                            }
                            sceNet_lib_0A5A8751();
                        }
                        sceNet_lib_949F1FBB();
                    }
                    ptpTerm();
                }
                pdpTerm();
            }
            adhocSockTerm();
        }
    }

    sceNet_lib_515B2F33(intr);
    return ret;
}

/* 0x000148 */
s32 sceNetAdhocTerm(void)
{
    s32 intr;
    s32 ret = 0;

    if (_sceKernelCheckThreadStack() < 2016)
        return ADHOC_ERR_STACK_SMALL;

    intr = sceNet_lib_94B44F26();
    if (intr < 0)
        return intr;

    if (g_adhoc_inited != 0) {
        if (adhocSockHasActive() != 0) {
            ret = ADHOC_ERR_BUSY;
        } else {
            AdhocMbuf *mbuf;
            gameModeTerm();
            sceNet_lib_0A5A8751();
            sceNet_lib_949F1FBB();
            ptpTerm();
            pdpTerm();
            adhocSockTerm();
            while ((mbuf = g_rx_queue.head) != NULL) {
                g_rx_queue.head = mbuf->m_nextpkt;
                if (g_rx_queue.head == NULL)
                    g_rx_queue.tail = NULL;
                mbuf->m_nextpkt = NULL;
                g_rx_queue.count--;
                sceNetIfhandle_lib_F56FAC82(mbuf);
            }
            g_adhoc_inited = 0;
        }
    }

    sceNet_lib_515B2F33(intr);
    return ret;
}

/* 0x000270 */
static void __attribute__((noinline))
adhocRxSoftIntr(void)
{
    AdhocMbuf *mbuf;
    u8 proto;

    while ((mbuf = g_rx_queue.head) != NULL) {
        g_rx_queue.head = mbuf->m_nextpkt;
        if (g_rx_queue.head == NULL)
            g_rx_queue.tail = NULL;
        mbuf->m_nextpkt = NULL;
        g_rx_queue.count--;

        if (mbuf->m_pkthdr_len < 49) {
            sceNetIfhandle_lib_F56FAC82(mbuf);
            continue;
        }
        if (mbuf->m_len < 49)
            sceNetIfhandle_lib_9A6261EC(mbuf, 48, 1, &proto);
        else
            proto = mbuf->m_data[48];

        if (proto == 1)
            pdpInputPacket(mbuf);
        else if (proto == 2)
            ptpInputPacket(mbuf);
        else
            sceNetIfhandle_lib_F56FAC82(mbuf);
    }
}

/* 0x00036c */
s32 __attribute__((noinline))
adhocIfOutput(AdhocIfnet *ifp, AdhocMbuf *mbuf)
{
    u8 *sadl = ifp->if_sadl;
    s32 cmp = sceNet_lib_E0A81C7C(mbuf->m_data, sadl + sadl[5] + 8, 6);
    if (cmp != 0) {
        mbuf->m_nextpkt = NULL;
        mbuf->m_flags |= 0x8000u;
        if (ifp->snd_tail != NULL)
            ifp->snd_tail->m_nextpkt = mbuf;
        else
            ifp->snd_head = mbuf;
        ifp->snd_tail = mbuf;
        ifp->snd_len++;
        if ((ifp->if_flags & 0x400) == 0)
            return ifp->if_start(ifp);
        return cmp;
    }

    if (g_rx_queue.count < g_rx_queue.max_count) {
        mbuf->m_rcvif = ifp;
        mbuf->m_nextpkt = NULL;
        if (g_rx_queue.tail != NULL)
            g_rx_queue.tail->m_nextpkt = mbuf;
        else
            g_rx_queue.head = mbuf;
        g_rx_queue.tail = mbuf;
        g_rx_queue.count++;
        return sceNet_lib_05D525E4(29);
    }

    g_rx_queue.drops++;
    return (s32)sceNetIfhandle_lib_F56FAC82(mbuf);
}

/* 0x00047c */
static s32 __attribute__((noinline))
adhocGetRxQueue(s32 unused_first, s32 unused_second, void **out)
{
    (void)unused_first;
    (void)unused_second;
    *out = &g_rx_queue;
    return 0;
}

/* 0x000490 */
static void __attribute__((noinline))
adhocIfDetachNotify(AdhocIfnet *ifp)
{
    pdpIfOutputWake(ifp);
}

/* 0x0004ac */
s32 sceNetAdhoc_7A662D6B(SceNetAdhocPollSd *sds, s32 count, u32 timeout, s32 flags)
{
    s32 err = 0;
    s32 intr;
    AdhocWaiter *waiter;
    AdhocPollEntry *entries;
    s64 start_time;
    s32 i;

    if (_sceKernelCheckThreadStack() < 1984)
        return ADHOC_ERR_STACK_SMALL;
    if (sds == NULL || count < 1 || (flags & ~1) != 0)
        return ADHOC_ERR_INVALID_ARG;

    waiter = (AdhocWaiter *)sceNet_lib_CA3CF5EB(&err);
    if (waiter == NULL)
        return err;

    intr = sceNet_lib_94B44F26();
    if (intr < 0) {
        sceNet_lib_515B2F33(intr);
        sceNet_lib_757085B0(waiter, 0);
        return intr;
    }

    if (g_adhoc_inited == 0) {
        sceNet_lib_515B2F33(intr);
        sceNet_lib_757085B0(waiter, 0);
        return ADHOC_ERR_NOT_INITIALIZED;
    }

    entries = (AdhocPollEntry *)sceNetIfhandle_lib_15CFE3C0((u32)(count * 8));
    start_time = 0;
    if (entries == NULL) {
        err = ADHOC_ERR_NO_MEM;
    } else {
        for (i = 0; i < count; i++) {
            AdhocSocket *sock = g_sock_list.head;
            while (sock != NULL && sock->id != sds[i].id)
                sock = sock->next;
            if (sock != NULL) {
                entries[i].sock = sock;
                entries[i].sd = &sds[i];
            } else {
                err = ADHOC_ERR_INVALID_SOCKET_ID;
                sds[0].revents = 256;
            }
        }

        if (err >= 0) {
            for (i = 0; i < count; i++) {
                AdhocSocket *sock = entries[i].sock;
                if ((sock->flags & 0x40u) != 0) {
                    err = ADHOC_ERR_EXCEPTION_EVENT;
                    entries[i].sd->revents = 1024;
                }
                if ((sock->flags & 0x8000u) != 0) {
                    err = ADHOC_ERR_EXCEPTION_EVENT;
                    entries[i].sd->revents = 2048;
                }
            }
        }

        if (err >= 0) {
            for (;;) {
                u32 wait_rem = 0;
                err = adhocPollSockets(entries, count);
                if (err != 0 || (flags & 1) != 0)
                    break;

                if (timeout != 0) {
                    s64 now;
                    if (start_time == 0)
                        start_time = sceKernelGetSystemTimeWide();
                    now = sceKernelGetSystemTimeWide();
                    if ((u64)(now - start_time) >= (u64)timeout) {
                        err = ADHOC_ERR_TIMEOUT;
                        break;
                    }
                    wait_rem = timeout - (u32)(now - start_time);
                }

                for (i = 0; i < count; i++) {
                    AdhocSocket *sock = entries[i].sock;
                    waiter->next = sock->wait_list;
                    sock->wait_list = waiter;
                }

                err = sceNet_lib_F6DB0A0B(&g_global_wait_head, wait_rem);

                for (i = 0; i < count; i++) {
                    AdhocSocket *sock = entries[i].sock;
                    adhocUnlinkWaiter(sock, waiter);
                    if ((sock->flags & 0x40u) != 0) {
                        entries[i].sd->revents = 1024;
                        err = 5;
                    }
                    if ((sock->flags & 0x8000u) != 0) {
                        entries[i].sd->revents = 2048;
                        err = 5;
                    }
                    if ((sock->flags & 1u) != 0) {
                        entries[i].sd->revents = 512;
                        err = 5;
                        if (sock->wait_list == NULL) {
                            if (sock->type == 1)
                                pdpFreePcb(sock);
                            else if (sock->type == 2)
                                ptpFreePcb(sock);
                            if (sock->pcb == NULL) {
                                if (sock->next != NULL)
                                    sock->next->prev = sock->prev;
                                else
                                    g_sock_list.tail = sock->prev;
                                *sock->prev = sock->next;
                                sceNetIfhandle_lib_76BAD213(sock);
                            }
                        }
                    }
                }

                if (err != 0) {
                    if (err == 4)
                        err = ADHOC_ERR_THREAD_ABORTED;
                    else if (err == 5)
                        err = ADHOC_ERR_EXCEPTION_EVENT;
                    else if (err == 11)
                        err = ADHOC_ERR_TIMEOUT;
                    else
                        err = ADHOC_ERR_INTERNAL;
                    break;
                }
            }
        }

        sceNetIfhandle_lib_76BAD213(entries);
    }

    sceNet_lib_515B2F33(intr);
    sceNet_lib_757085B0(waiter, 0);
    return err;
}

/* 0x000974 */
static s32 __attribute__((noinline))
adhocPollSockets(AdhocPollEntry *entries, s32 count)
{
    s32 ready = 0;
    s32 i;

    for (i = 0; i < count; i++) {
        AdhocSocket *sock = entries[i].sock;
        SceNetAdhocPollSd *sd = entries[i].sd;
        if (sock->type == 1)
            sd->revents = pdpPollEvents(sock, sd->events);
        else if (sock->type == 2)
            sd->revents = ptpPollEvents(sock, sd->events);
        if (sd->revents != 0)
            ready++;
    }
    return ready;
}

/* 0x000a2c */
s32 sceNetAdhoc_73BFD52D(s32 id, s32 alert_mask)
{
    s32 ret = 0;
    s32 intr;

    if (_sceKernelCheckThreadStack() < 2016)
        return ADHOC_ERR_STACK_SMALL;

    intr = sceNet_lib_94B44F26();
    if (intr < 0)
        return intr;

    if (g_adhoc_inited == 0) {
        ret = ADHOC_ERR_NOT_INITIALIZED;
    } else {
        AdhocSocket *sock = g_sock_list.head;
        while (sock != NULL && sock->id != id)
            sock = sock->next;

        if (sock == NULL) {
            ret = ADHOC_ERR_INVALID_SOCKET_ID;
        } else {
            s32 wake = 0;
            if ((alert_mask & 0x10) != 0) {
                if ((sock->flags & 0x10u) == 0) {
                    sock->flags |= 0x10u;
                    wake++;
                }
            } else {
                sock->flags &= ~0x10u;
            }
            if ((alert_mask & 0x20) != 0) {
                if ((sock->flags & 0x20u) == 0) {
                    sock->flags |= 0x20u;
                    wake++;
                }
            } else {
                sock->flags &= ~0x20u;
            }
            if ((alert_mask & 0x40) != 0) {
                if ((sock->flags & 0x40u) == 0) {
                    sock->flags |= 0x40u;
                    wake++;
                }
            } else {
                sock->flags &= ~0x40u;
            }
            if ((alert_mask & 0x80) != 0) {
                if ((sock->flags & 0x80u) == 0) {
                    sock->flags |= 0x80u;
                    wake++;
                }
            } else {
                sock->flags &= ~0x80u;
            }
            if ((alert_mask & 0x100) != 0) {
                if ((sock->flags & 0x100u) == 0) {
                    sock->flags |= 0x100u;
                    wake++;
                }
            } else {
                sock->flags &= ~0x100u;
            }
            if ((alert_mask & 0x200) != 0) {
                if ((sock->flags & 0x200u) == 0) {
                    sock->flags |= 0x200u;
                    wake++;
                }
            } else {
                sock->flags &= ~0x200u;
            }
            if (wake != 0)
                sceNet_lib_6B294EE4(sock, alert_mask);
        }
    }

    sceNet_lib_515B2F33(intr);
    return ret;
}

/* 0x000c58 */
s32 sceNetAdhoc_4D2CE199(s32 id, s32 *out_alert)
{
    s32 ret = 0;
    s32 intr;

    if (_sceKernelCheckThreadStack() < 2032)
        return ADHOC_ERR_STACK_SMALL;
    if (out_alert == NULL)
        return ADHOC_ERR_INVALID_ARG;

    intr = sceNet_lib_94B44F26();
    if (intr < 0)
        return intr;

    if (g_adhoc_inited == 0) {
        ret = ADHOC_ERR_NOT_INITIALIZED;
    } else {
        AdhocSocket *sock = g_sock_list.head;
        while (sock != NULL && sock->id != id)
            sock = sock->next;

        if (sock == NULL) {
            ret = ADHOC_ERR_INVALID_SOCKET_ID;
        } else {
            *out_alert = 0;
            if ((sock->flags & 0x10u) != 0)
                *out_alert = 0x10;
            if ((sock->flags & 0x20u) != 0)
                *out_alert |= 0x20;
            if ((sock->flags & 0x40u) != 0)
                *out_alert |= 0x40;
            if ((sock->flags & 0x80u) != 0)
                *out_alert |= 0x80;
            if ((sock->flags & 0x100u) != 0)
                *out_alert |= 0x100;
            if ((sock->flags & 0x200u) != 0)
                *out_alert |= 0x200;
        }
    }

    sceNet_lib_515B2F33(intr);
    return ret;
}

/* 0x000dd0 */
static s32 __attribute__((noinline))
adhocSockInit(void)
{
    s32 ret;

    g_sock_list.head = NULL;
    g_sock_list.tail = &g_sock_list.head;
    ret = sceNet_lib_2E005032(adhocSockWakeWaiters);
    if (ret >= 0) {
        g_last_sock_id = 0;
        return 0;
    }
    return ret;
}

/* 0x000e18 */
static void __attribute__((noinline))
adhocSockTerm(void)
{
    AdhocSocket *sock;

    sceNet_lib_33B230BD();
    sock = g_sock_list.head;
    while (sock != NULL) {
        if (sock->pcb != NULL) {
            sock = g_sock_list.head;
        } else {
            if (sock->next != NULL)
                sock->next->prev = sock->prev;
            else
                g_sock_list.tail = sock->prev;
            *sock->prev = sock->next;
            sceNetIfhandle_lib_76BAD213(sock);
            sock = g_sock_list.head;
        }
    }
}

/* 0x000e90 */
AdhocSocket *__attribute__((noinline))
adhocSockFind(u8 type, s32 id)
{
    AdhocSocket *sock = g_sock_list.head;

    while (sock != NULL) {
        if (type == 0 || sock->type == type) {
            if (sock->id == id)
                return sock;
        }
        sock = sock->next;
    }
    return NULL;
}

/* 0x000ed8 */
s32 __attribute__((noinline))
adhocSockAllocId(void)
{
    s32 cand = (g_last_sock_id == 255) ? 1 : (g_last_sock_id + 1);

    while (cand != g_last_sock_id) {
        AdhocSocket *sock = g_sock_list.head;
        while (sock != NULL && sock->id != cand)
            sock = sock->next;
        if (sock == NULL) {
            g_last_sock_id = cand;
            return cand;
        }
        cand = (cand == 255) ? 1 : (cand + 1);
    }
    return -1;
}

/* 0x000f60 */
s32 __attribute__((noinline))
adhocSockCreate(u8 type, AdhocSocket **out_sock)
{
    AdhocSocket *sock = (AdhocSocket *)sceNetIfhandle_lib_15CFE3C0(sizeof(AdhocSocket));
    AdhocSocket **old_tail;

    if (sock == NULL)
        return ADHOC_ERR_NO_MEM;

    sceKernelMemset(sock, 0, sizeof(AdhocSocket));
    sock->type = type;
    sock->next = NULL;
    sock->prev = g_sock_list.tail;
    old_tail = g_sock_list.tail;
    g_sock_list.tail = &sock->next;
    *old_tail = sock;
    *out_sock = sock;
    (*out_sock)->id = -1;
    return 0;
}

/* 0x000ffc */
void __attribute__((noinline))
adhocSockTryFree(AdhocSocket *sock)
{
    if (sock->pcb == NULL) {
        if (sock->next != NULL)
            sock->next->prev = sock->prev;
        else
            g_sock_list.tail = sock->prev;
        *sock->prev = sock->next;
        sceNetIfhandle_lib_76BAD213(sock);
    }
}

/* 0x001044 */
static s32 __attribute__((noinline))
adhocSockHasActive(void)
{
    AdhocSocket *sock = g_sock_list.head;
    while (sock != NULL) {
        if (sock->wait_list != NULL)
            return 1;
        sock = sock->next;
    }
    return 0;
}

/* 0x001078 */
static void __attribute__((noinline))
adhocSockWakeWaiters(AdhocSocket *sock, s16 alert_flags)
{
    AdhocWaiter *waiter = sock->wait_list;
    s32 mask_poll = alert_flags & 0x40;
    s32 mask_tx = alert_flags & 0x210;
    s32 mask_rx = alert_flags & 0x1A0;

    while (waiter != NULL) {
        void *chan = waiter->wait_chan;
        if (chan != NULL) {
            if ((mask_tx != 0 && chan == sock->chan_tx) ||
                (mask_rx != 0 && chan == sock->chan_rx) ||
                (mask_poll != 0 && chan == sock->chan_poll)) {
                waiter->wait_chan = NULL;
                sceKernelSignalSema(waiter->sema_id, 1);
            }
        }
        waiter = waiter->next;
    }
}
