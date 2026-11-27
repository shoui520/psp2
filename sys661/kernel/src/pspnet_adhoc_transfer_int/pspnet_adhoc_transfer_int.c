#include <common_imp.h>

/* Imports */
extern int sceNetGetLocalEtherAddr(u8 *mac);
extern int sceNet_lib_E0A81C7C(const void *lhs, const void *rhs, u32 n);

extern int sceNetAdhocPtpOpen(const u8 *srcmac, u16 srcport, const u8 *destmac, u16 destport, u32 bufsize, u32 delay, int count, int flag);
extern int sceNetAdhocPtpConnect(int id, u32 timeout, int flag);
extern int sceNetAdhocPtpListen(const u8 *srcmac, u16 srcport, u32 bufsize, u32 delay, int count, int queue, int flag);
extern int sceNetAdhocPtpAccept(int id, u8 *mac, u16 *port, u32 timeout, int flag);
extern int sceNetAdhocPtpSend(int id, const void *data, int *datasize, u32 timeout, int flag);
extern int sceNetAdhocPtpRecv(int id, void *data, int *datasize, u32 timeout, int flag);
extern int sceNetAdhocPtpFlush(int id, u32 timeout, int flag);
extern int sceNetAdhocPtpClose(int id, int flag);
extern int sceNetAdhocSetSocketAlert(int id, int flags);

extern int sceNetAdhocMatchingCreate(int mode, int maxpeers, u16 port, int bufsize, u32 hellodelay, u32 pingdelay, int initcount, u32 msgdelay, void *callback);
extern int sceNetAdhocMatchingDelete(int matchingid);
extern int sceNetAdhocMatchingStart(int matchingid, int evthpri, int evthstack, int inthpri, int inthstack, int optlen, void *optdata);
extern int sceNetAdhocMatchingStop(int matchingid);
extern int sceNetAdhocMatchingSelectTarget(int matchingid, const u8 *mac, int optlen, const void *optdata);
extern int sceNetAdhocMatchingCancelTarget(int matchingid, const u8 *mac);

extern int sceKernelCpuSuspendIntr(void);
extern void sceKernelCpuResumeIntr(int state);
extern void *sceKernelMemset(void *s, int c, u32 n);
extern void *sceKernelMemcpy(void *dst, const void *src, u32 n);
extern int sceKernelCreateLwMutex(void *workarea, const char *name, u32 attr, int initcount, void *opt);
extern int sceKernelDeleteLwMutex(void *workarea);
extern int sceKernelLockLwMutex(void *workarea, int count, u32 *timeout);
extern int sceKernelUnlockLwMutex(void *workarea, int count);

extern int sceKernelDelayThread(u32 delay);
extern int sceKernelCreateEventFlag(const char *name, u32 attr, u32 bits, void *opt);
extern int sceKernelDeleteEventFlag(int evid);
extern int sceKernelSetEventFlag(int evid, u32 bits);
extern int sceKernelWaitEventFlag(int evid, u32 bits, u32 wait, u32 *outbits, u32 *timeout);
extern int sceKernelCreateVpl(const char *name, int part, u32 attr, u32 size, void *opt);
extern int sceKernelDeleteVpl(int uid);
extern int sceKernelTryAllocateVpl(int uid, u32 size, void **data);
extern int sceKernelFreeVpl(int uid, void *data);
extern int sceKernelReferVplStatus(int uid, void *info);

typedef struct TransPeerNode {
    struct TransPeerNode *next; /* 0x00 */
    u32 optlen;                 /* 0x04 */
    void *optdata;              /* 0x08 */
    u8 mac[6];                  /* 0x0c..0x11 */
    u8 pad[2];                  /* 0x12..0x13 */
} TransPeerNode;

typedef struct TransReplySession {
    int state;             /* 0x00 */
    u32 optlen;            /* 0x04 */
    void *optdata;         /* 0x08 */
    int matching_id;       /* 0x0c */
    int evf_id;            /* 0x10 */
    int ptp_id;            /* 0x14 */
    u8 peer_mac[6];        /* 0x18..0x1d */
    u8 pad[2];             /* 0x1e..0x1f */
    TransPeerNode *head;   /* 0x20 */
    TransPeerNode *tail;   /* 0x24 */
} TransReplySession;

typedef struct TransReqSession {
    int state;             /* 0x00 */
    u32 optlen;            /* 0x04 */
    void *optdata;         /* 0x08 */
    int matching_id;       /* 0x0c */
    int evf_id;            /* 0x10 */
    int ptp_id;            /* 0x14 */
    u8 peer_mac[6];        /* 0x18..0x1d */
    u8 pad[2];             /* 0x1e..0x1f */
} TransReqSession;

/* Data / BSS */
static int g_vpl_id = -1;                      /* 0x2420 */
static int g_max_used;                         /* 0x2430 */
static int g_abort_flag;                       /* 0x2434 */
static int g_parent_inited;                    /* 0x2438 */
static int g_child_inited;                     /* 0x243c */
static TransReplySession *g_reply_sess_ptr;    /* 0x2440 */
static TransReplySession g_reply_sess;         /* 0x2444..0x246b */
static TransReqSession *g_req_sess_ptr;        /* 0x246c */
static TransPeerNode *g_child_list;            /* 0x2470 */
static TransReqSession g_req_sess;             /* 0x2474..0x2493 */
static u32 g_child_mutex[8];                   /* 0x2494..0x24b3 */

static void replyMatchingCallback(int matchingid, int event, const u8 *mac, u32 optlen, const void *optdata);
static void reqMatchingCallback(int matchingid, int event, const u8 *mac, u32 optlen, const void *optdata);

__attribute__((noinline))
static int ptpSendStream(int *ptp_out, const u8 *destmac, int total_size, void *arg, int (*cb)(void *, void **, u32 *))
{
    u8 srcmac[16];
    int retries = 20;
    int ptpId = -1;
    int ret = sceNetGetLocalEtherAddr(srcmac);
    if (ret < 0)
        return ret;

    do {
        for (;;) {
            ptpId = sceNetAdhocPtpOpen(srcmac, 0x15, destmac, 0x15, 0x2d20, 0x30d40, 0x32, 0);
            if (ptpId < 0) {
                ret = ptpId;
                break;
            }
            *ptp_out = ptpId;
            ret = sceNetAdhocPtpConnect(ptpId, 0x4c4b40, 0);
            if (ret >= 0)
                break;
            if (ret != (int)0x80410718 || retries <= 0)
                break;
            retries--;
            sceNetAdhocPtpClose(ptpId, 0);
            sceKernelDelayThread(0x7a120);
        }
        if (ret < 0)
            break;

        u32 swapped_size = __builtin_bswap32((u32)total_size);
        int send_len = 4;
        ret = sceNetAdhocPtpSend(ptpId, &swapped_size, &send_len, 0, 0);
        if (ret < 0)
            break;

        ret = sceNetAdhocPtpFlush(ptpId, 0, 0);
        if (ret < 0)
            break;

        int rem = total_size;
        int sendFailed = 0;
        while (rem > 0) {
            void *chunk_ptr = NULL;
            u32 chunk_size = 0;
            if (cb(arg, &chunk_ptr, &chunk_size) < 0) {
                ret = 0x8041110b;
                sendFailed = 1;
                break;
            }
            const u8 *chunkBytes = (const u8 *)chunk_ptr;
            u32 sent = 0;
            while (sent < chunk_size) {
                u32 cur = chunk_size - sent;
                if (cur >= 0x5a5)
                    cur = 0x5a4;
                send_len = (int)cur;
                ret = sceNetAdhocPtpSend(ptpId, chunkBytes + sent, &send_len, 0, 0);
                sent += 0x5a4;
                if (ret < 0) {
                    sendFailed = 1;
                    break;
                }
            }
            if (sendFailed)
                break;
            rem -= (int)chunk_size;
        }
        if (sendFailed)
            break;

        ret = sceNetAdhocPtpFlush(ptpId, 0, 0);
    } while (0);

    if (ptpId >= 0) {
        *ptp_out = -1;
        sceNetAdhocPtpClose(ptpId, 0);
    }
    return ret;
}

__attribute__((noinline))
static int ptpRecvStream(int *ptp_out, const u8 *target_mac, void *buf, u32 bufsize, void *arg, int (*cb)(void *, u32, void *, u32))
{
    u8 mac[16];
    u16 port;
    int ret = sceNetGetLocalEtherAddr(mac);
    if (ret < 0)
        return ret;

    int ptpId = sceNetAdhocPtpListen(mac, 0x15, 0xb480, 0x30d40, 0x32, 1, 0);
    ret = ptpId;
    if (ptpId >= 0) {
        *ptp_out = ptpId;
        int matched = 0;
        while (g_abort_flag <= 0) {
            ret = sceNetAdhocPtpAccept(ptpId, mac, &port, 0x4c4b40, 0);
            if (ret < 0) {
                matched = 1;
                break;
            }
            if (sceNet_lib_E0A81C7C(mac, target_mac, 6) == 0 && port == 0x15) {
                matched = 1;
                *ptp_out = ret;
                sceNetAdhocPtpClose(ptpId, 0);
                ptpId = ret;
                if (g_abort_flag > 0) {
                    ret = 0x8041110a;
                    break;
                }
                u32 net_total = 0;
                int recv_len = 4;
                ret = sceNetAdhocPtpRecv(ptpId, &net_total, &recv_len, 0x4c4b40, 0);
                if (ret < 0)
                    break;
                u32 total = __builtin_bswap32(net_total);
                if (total == 0)
                    break;
                u32 recvd = 0;
                do {
                    int chunk = (int)bufsize;
                    ret = sceNetAdhocPtpRecv(ptpId, buf, &chunk, 0x4c4b40, 0);
                    if (ret < 0)
                        break;
                    if (cb(arg, total, buf, (u32)chunk) < 0) {
                        ret = 0x8041110c;
                        break;
                    }
                    recvd += (u32)chunk;
                } while (recvd < total);
                break;
            }
            sceNetAdhocPtpClose(ret, 0);
        }
        if (!matched)
            ret = 0x8041110a;
    }

    if (ptpId >= 0) {
        *ptp_out = -1;
        sceNetAdhocPtpClose(ptpId, 0);
    }
    g_abort_flag = 0;
    return ret;
}

__attribute__((noinline))
static int ptpAbortTransfer(int ptp_id)
{
    g_abort_flag = 1;
    return sceNetAdhocSetSocketAlert(ptp_id, 0x3f0);
}

__attribute__((noinline))
static int transPoolCreate(u32 size)
{
    int id = sceKernelCreateVpl("SceNetAdhocTransferMemoryPool", 2, 0, size, NULL);
    g_vpl_id = id;
    return (id < 0) ? id : 0;
}

__attribute__((noinline))
static void transPoolDelete(void)
{
    if (g_vpl_id >= 0) {
        sceKernelDeleteVpl(g_vpl_id);
        g_vpl_id = -1;
    }
}

__attribute__((noinline))
static void *transPoolAlloc(u32 size)
{
    u32 info[16];
    void *ptr;
    if (sceKernelTryAllocateVpl(g_vpl_id, size, &ptr) < 0)
        return NULL;
    info[0] = 0x34;
    if (sceKernelReferVplStatus(g_vpl_id, info) == 0) {
        int used = (int)(info[10] - info[11]);
        if (used > g_max_used)
            g_max_used = used;
    }
    return ptr;
}

__attribute__((noinline))
static int transPoolFree(void *ptr)
{
    return sceKernelFreeVpl(g_vpl_id, ptr);
}

__attribute__((noinline))
static int transPoolGetStatus(u32 *info)
{
    info[0] = 0x34;
    int ret = sceKernelReferVplStatus(g_vpl_id, info);
    return (ret < 0) ? ret : 0;
}

__attribute__((noinline))
static void replyPeerEnqueue(TransReplySession *sess, TransPeerNode *node)
{
    node->next = NULL;
    if (sess->tail)
        sess->tail->next = node;
    else
        sess->head = node;
    sess->tail = node;
}

__attribute__((noinline))
static TransPeerNode *replyPeerDequeue(TransReplySession *sess)
{
    TransPeerNode *node = sess->head;
    if (node) {
        sess->head = node->next;
        if (!node->next)
            sess->tail = NULL;
        node->next = NULL;
    }
    return node;
}

__attribute__((noinline))
static TransPeerNode *replyPeerPeek(TransReplySession *sess)
{
    return sess->head;
}

__attribute__((noinline))
static TransPeerNode *replyPeerFind(TransReplySession *sess, const u8 *mac)
{
    TransPeerNode *cur = sess->head;
    while (cur) {
        if (sceNet_lib_E0A81C7C(cur->mac, mac, 6) == 0)
            return cur;
        cur = cur->next;
    }
    return NULL;
}

__attribute__((noinline))
static void replyPeerRemove(TransReplySession *sess, TransPeerNode *node)
{
    TransPeerNode *cur = sess->head;
    TransPeerNode *prev = NULL;
    while (cur) {
        if (cur == node) {
            if (prev)
                prev->next = node->next;
            else
                sess->head = node->next;
            break;
        }
        prev = cur;
        cur = cur->next;
    }
    if (sess->tail == node)
        sess->tail = prev;
    node->next = NULL;
}

int sceNetAdhocTransInt_69F19666(u32 optlen, const void *optdata)
{
    if (!g_parent_inited)
        return 0x80411101;
    if (g_reply_sess_ptr)
        return 0x80411104;

    sceKernelMemset(&g_reply_sess, 0, sizeof(g_reply_sess));
    g_reply_sess.state = 0;
    g_reply_sess_ptr = &g_reply_sess;
    g_reply_sess.optlen = optlen;
    g_reply_sess.optdata = NULL;
    if (optlen != 0) {
        g_reply_sess.optdata = transPoolAlloc(optlen);
        if (!g_reply_sess_ptr->optdata) {
            g_reply_sess_ptr = NULL;
            return 0x80411106;
        }
        sceKernelMemcpy(g_reply_sess_ptr->optdata, optdata, g_reply_sess_ptr->optlen);
    }
    sceKernelMemset(g_reply_sess_ptr->peer_mac, 0, 6);
    int mid = sceNetAdhocMatchingCreate(1, 2, 0x15, 0x800, 0xf4240, 0x1c9c380, 3, 0xf4240, replyMatchingCallback);
    if (mid < 0) {
        transPoolFree(g_reply_sess_ptr->optdata);
        g_reply_sess_ptr = NULL;
        return mid;
    }
    g_reply_sess_ptr->matching_id = mid;
    return 0;
}

int sceNetAdhocTransInt_40D77905(u32 *out)
{
    if (!g_parent_inited)
        return 0x80411101;
    if (!g_reply_sess_ptr)
        return 0x80411105;
    if (g_reply_sess_ptr->state != 1)
        return 0x80411107;

    int intr = sceKernelCpuSuspendIntr();
    TransPeerNode *node = replyPeerPeek(g_reply_sess_ptr);
    int ret = 0;
    if (node) {
        if (!out) {
            ret = (int)(((node->optlen + 3) & ~3u) + 0x14);
        } else {
            node = replyPeerDequeue(g_reply_sess_ptr);
            ret = 1;
            out[0] = 0;
            out[1] = node->optlen;
            sceKernelMemcpy(&out[3], node->mac, 6);
            out[2] = (u32)&out[5];
            sceKernelMemcpy(&out[5], node->optdata, out[1]);
        }
    }
    sceKernelCpuResumeIntr(intr);
    return ret;
}

int sceNetAdhocTransInt_AD0E88C6(const u8 *mac, int mode)
{
    if (!g_parent_inited)
        return 0x80411101;
    if (!g_reply_sess_ptr)
        return 0x80411105;
    if (g_reply_sess_ptr->state != 1)
        return 0x80411107;
    if (!mac)
        return 0x80411103;

    if (mode == 1) {
        int evf = sceKernelCreateEventFlag("SceNetAdhocTransferReplySession", 0, 0, NULL);
        if (evf < 0)
            return evf;
        g_reply_sess_ptr->evf_id = evf;
        g_reply_sess_ptr->state = 2;
        int ret = sceNetAdhocMatchingSelectTarget(g_reply_sess_ptr->matching_id, mac, 0, NULL);
        if (ret < 0) {
            g_reply_sess_ptr->state = 4;
            sceKernelDeleteEventFlag(g_reply_sess_ptr->evf_id);
            if ((u32)(ret + 0x7fbef7f4) < 2)
                ret = 0x80411108;
            return ret;
        }
        u32 outbits;
        sceKernelWaitEventFlag(g_reply_sess_ptr->evf_id, 3, 0x11, &outbits, NULL);
        sceKernelDeleteEventFlag(g_reply_sess_ptr->evf_id);
        if (outbits & 2)
            return 0x8041110a;
        if (g_reply_sess_ptr->state != 3)
            ret = 0x80411108;
        sceKernelMemcpy(g_reply_sess_ptr->peer_mac, mac, 6);
        return ret;
    } else if (mode == 0) {
        int ret = sceNetAdhocMatchingCancelTarget(g_reply_sess_ptr->matching_id, mac);
        if (ret == (int)0x8041080c)
            ret = 0;
        return ret;
    } else if (mode == 2) {
        sceNetAdhocMatchingStop(g_reply_sess_ptr->matching_id);
        g_reply_sess_ptr->state = 4;
        return 0;
    }
    return 0x80411103;
}

int sceNetAdhocTransInt_9C6EE447(int total_size, void *arg, int (*cb)(void *, void **, u32 *))
{
    if (!g_parent_inited)
        return 0x80411101;
    if (!g_reply_sess_ptr)
        return 0x80411105;
    if (!cb || g_reply_sess_ptr->state != 3)
        return 0x80411107;

    int ret = ptpSendStream(&g_reply_sess_ptr->ptp_id, g_reply_sess_ptr->peer_mac, total_size, arg, cb);
    g_reply_sess_ptr->state = 4;
    if (ret == (int)0x80410708)
        ret = 0x8041110a;
    return ret;
}

int sceNetAdhocTransInt_6D8B0D00(void *buf, u32 bufsize, void *arg, int (*cb)(void *, u32, void *, u32))
{
    if (!g_parent_inited)
        return 0x80411101;
    if (!g_reply_sess_ptr)
        return 0x80411105;
    if (!cb)
        return 0x80411103;
    if (g_reply_sess_ptr->state != 3)
        return 0x80411107;

    int ret = ptpRecvStream(&g_reply_sess_ptr->ptp_id, g_reply_sess_ptr->peer_mac, buf, bufsize, arg, cb);
    g_reply_sess_ptr->state = 4;
    if (ret == (int)0x80410708)
        ret = 0x8041110a;
    return ret;
}

static void replyMatchingCallback(int matchingid, int event, const u8 *mac, u32 optlen, const void *optdata)
{
    if (g_reply_sess_ptr->matching_id != matchingid)
        return;

    int intr;
    switch (event) {
    case 2:
        intr = sceKernelCpuSuspendIntr();
        if (g_reply_sess_ptr->state == 1) {
            TransPeerNode *node = transPoolAlloc(sizeof(TransPeerNode));
            if (node) {
                node->optlen = optlen;
                node->optdata = NULL;
                if (optlen != 0) {
                    void *buf = transPoolAlloc(optlen);
                    node->optdata = buf;
                    if (!buf) {
                        transPoolFree(node);
                        sceKernelCpuResumeIntr(intr);
                        return;
                    }
                    sceKernelMemcpy(buf, optdata, optlen);
                }
                sceKernelMemcpy(node->mac, mac, 6);
                replyPeerEnqueue(g_reply_sess_ptr, node);
            }
        }
        sceKernelCpuResumeIntr(intr);
        break;
    case 7:
        intr = sceKernelCpuSuspendIntr();
        if (g_reply_sess_ptr->state == 2) {
            g_reply_sess_ptr->state = 3;
            sceKernelSetEventFlag(g_reply_sess_ptr->evf_id, 1);
        }
        sceKernelCpuResumeIntr(intr);
        break;
    case 5:
    case 8:
    case 9:
    case 10:
        intr = sceKernelCpuSuspendIntr();
        if (g_reply_sess_ptr->state == 2 && sceNet_lib_E0A81C7C(g_reply_sess_ptr->peer_mac, mac, 6) == 0) {
            g_reply_sess_ptr->state = 4;
            sceKernelSetEventFlag(g_reply_sess_ptr->evf_id, 1);
        } else if (g_reply_sess_ptr->state == 1) {
            TransPeerNode *node = replyPeerFind(g_reply_sess_ptr, mac);
            if (node) {
                replyPeerRemove(g_reply_sess_ptr, node);
                transPoolFree(node);
            }
        }
        sceKernelCpuResumeIntr(intr);
        break;
    default:
        break;
    }
}

int sceNetAdhocTransInt_552E0C4F(u32 poolsize)
{
    if (g_parent_inited)
        return 0x80411102;
    int ret = transPoolCreate(poolsize);
    if (ret >= 0) {
        g_parent_inited = 1;
        ret = 0;
    }
    return ret;
}

int sceNetAdhocTransInt_658833CC(int prio, int stack)
{
    if (!g_parent_inited)
        return 0x80411101;
    if (!g_reply_sess_ptr)
        return 0x80411105;
    if (g_reply_sess_ptr->state != 0)
        return 0x80411107;

    g_reply_sess_ptr->state = 1;
    int ret = sceNetAdhocMatchingStart(g_reply_sess_ptr->matching_id, prio, 0x1c00, stack, 0, g_reply_sess_ptr->optlen, g_reply_sess_ptr->optdata);
    if (ret < 0) {
        g_reply_sess_ptr->state = 0;
        return ret;
    }
    return 0;
}

int sceNetAdhocTransInt_83FA2BBA(void)
{
    if (!g_parent_inited)
        return 0x80411101;
    if (!g_reply_sess_ptr)
        return 0x80411105;

    int intr = sceKernelCpuSuspendIntr();
    if (g_reply_sess_ptr->state != 2) {
        sceKernelCpuResumeIntr(intr);
        return 0x80411107;
    }
    g_reply_sess_ptr->state = 4;
    sceKernelSetEventFlag(g_reply_sess_ptr->evf_id, 2);
    sceKernelCpuResumeIntr(intr);
    return 0;
}

int sceNetAdhocTransInt_0B1F77E1(void)
{
    if (!g_parent_inited)
        return 0x80411101;
    if (!g_reply_sess_ptr)
        return 0x80411105;
    if (g_reply_sess_ptr->state != 3 || g_reply_sess_ptr->ptp_id <= 0)
        return 0x80411107;
    ptpAbortTransfer(g_reply_sess_ptr->ptp_id);
    return 0;
}

int sceNetAdhocTransInt_CD469448(void)
{
    if (!g_parent_inited)
        return 0x80411101;
    if (!g_reply_sess_ptr)
        return 0x80411105;

    sceNetAdhocMatchingStop(g_reply_sess_ptr->matching_id);
    g_reply_sess_ptr->state = 0;
    sceKernelMemset(g_reply_sess_ptr->peer_mac, 0, 6);
    for (;;) {
        TransPeerNode *node = replyPeerDequeue(g_reply_sess_ptr);
        if (!node)
            break;
        transPoolFree(node);
    }
    return 0;
}

int sceNetAdhocTransInt_E3707672(void)
{
    if (!g_parent_inited)
        return 0x80411101;
    if (!g_reply_sess_ptr)
        return 0x80411105;
    if (g_reply_sess_ptr->state != 0)
        return 0x80411107;

    sceNetAdhocMatchingDelete(g_reply_sess_ptr->matching_id);
    transPoolFree(g_reply_sess_ptr->optdata);
    g_reply_sess_ptr = NULL;
    return 0;
}

int sceNetAdhocTransInt_A735B9B2(void)
{
    if (!g_parent_inited)
        return 0;
    if (g_reply_sess_ptr)
        return 0x80411104;
    transPoolDelete();
    g_parent_inited = 0;
    return 0;
}

int sceNetAdhocTransInt_B0975880(u32 *info)
{
    if (!g_parent_inited)
        return 0x80411101;
    if (!info)
        return 0x80411103;
    return transPoolGetStatus(info);
}

__attribute__((noinline))
static int childPeerInit(TransPeerNode *node, const u8 *mac, u32 optlen, const void *optdata)
{
    node->optlen = optlen;
    node->optdata = NULL;
    if (optlen != 0) {
        void *buf = transPoolAlloc(optlen);
        node->optdata = buf;
        if (!buf)
            return -1;
    }
    node->optlen = optlen;
    sceKernelMemcpy(node->optdata, optdata, optlen);
    sceKernelMemcpy(node->mac, mac, 6);
    return 0;
}

__attribute__((noinline))
static TransPeerNode *childPeerFind(const u8 *mac)
{
    TransPeerNode *cur = g_child_list;
    while (cur) {
        if (sceNet_lib_E0A81C7C(cur->mac, mac, 6) == 0)
            return cur;
        cur = cur->next;
    }
    return NULL;
}

__attribute__((noinline))
static void childPeerAppend(TransPeerNode *node)
{
    if (!g_child_list) {
        g_child_list = node;
    } else {
        TransPeerNode *cur = g_child_list;
        while (cur->next)
            cur = cur->next;
        cur->next = node;
    }
    node->next = NULL;
}

__attribute__((noinline))
static void childPeerRemove(TransPeerNode *node)
{
    if (g_child_list == node) {
        g_child_list = node->next;
    } else {
        TransPeerNode *prev = g_child_list;
        while (prev && prev->next != node)
            prev = prev->next;
        if (prev)
            prev->next = node->next;
    }
    node->next = NULL;
}

int sceNetAdhocTransInt_CD9F1D46(u32 optlen, const void *optdata)
{
    if (!g_child_inited)
        return 0x80411101;
    if (g_req_sess_ptr)
        return 0x80411104;

    sceKernelMemset(&g_req_sess, 0, sizeof(g_req_sess));
    g_req_sess.state = 0;
    g_req_sess_ptr = &g_req_sess;
    g_req_sess.optlen = optlen;
    g_req_sess.optdata = NULL;
    if (optlen != 0) {
        g_req_sess.optdata = transPoolAlloc(optlen);
        if (!g_req_sess_ptr->optdata) {
            g_req_sess_ptr = NULL;
            return 0x80411106;
        }
        sceKernelMemcpy(g_req_sess_ptr->optdata, optdata, g_req_sess_ptr->optlen);
    }
    sceKernelMemset(g_req_sess_ptr->peer_mac, 0, 6);
    int mid = sceNetAdhocMatchingCreate(2, 2, 0x15, 0x800, 0xf4240, 0x1c9c380, 3, 0xf4240, reqMatchingCallback);
    if (mid < 0) {
        transPoolFree(g_req_sess_ptr->optdata);
        g_req_sess_ptr = NULL;
        return mid;
    }
    g_req_sess_ptr->matching_id = mid;
    return 0;
}

int sceNetAdhocTransInt_8897358A(int *buflen, u8 *buf)
{
    if (!g_child_inited)
        return 0x80411101;
    if (!g_req_sess_ptr)
        return 0x80411105;
    if (g_req_sess_ptr->state != 1)
        return 0x80411107;

    sceKernelLockLwMutex(g_child_mutex, 1, NULL);
    TransPeerNode *cur = g_child_list;
    TransPeerNode *prev_out = NULL;
    int total = 0;
    while (cur) {
        TransPeerNode *next = cur->next;
        int entry_size = (int)(((cur->optlen + 3) & ~3u) + 0x14);
        int next_total = total + entry_size;
        if (buf) {
            if (*buflen < next_total)
                break;
            u32 *out = (u32 *)(buf + total);
            sceKernelMemcpy(&out[3], cur->mac, 6);
            out[2] = (u32)&out[5];
            out[1] = cur->optlen;
            sceKernelMemcpy(&out[5], cur->optdata, cur->optlen);
            out[0] = 0;
            if (prev_out)
                prev_out->next = (TransPeerNode *)out;
            prev_out = (TransPeerNode *)out;
            g_child_list = cur->next;
            transPoolFree(cur->optdata);
            transPoolFree(cur);
        }
        total = next_total;
        cur = next;
    }
    sceKernelUnlockLwMutex(g_child_mutex, 1);
    *buflen = total;
    return 0;
}

int sceNetAdhocTransInt_0CAE4A7B(const u8 *mac)
{
    if (!g_child_inited)
        return 0x80411101;
    if (!g_req_sess_ptr)
        return 0x80411105;
    if (g_req_sess_ptr->state != 1)
        return 0x80411107;
    if (!mac)
        return 0x80411103;

    int evf = sceKernelCreateEventFlag("SceNetAdhocTransferRequestSession", 0, 0, NULL);
    if (evf < 0)
        return evf;
    g_req_sess_ptr->evf_id = evf;
    sceKernelMemcpy(g_req_sess_ptr->peer_mac, mac, 6);
    g_req_sess_ptr->state = 2;
    int ret = sceNetAdhocMatchingSelectTarget(g_req_sess_ptr->matching_id, mac, g_req_sess_ptr->optlen, g_req_sess_ptr->optdata);
    if (ret < 0) {
        g_req_sess_ptr->state = 4;
        if ((u32)(ret + 0x7fbef7f4) < 2)
            ret = 0x80411109;
        sceKernelDeleteEventFlag(g_req_sess_ptr->evf_id);
        return ret;
    }
    u32 outbits;
    sceKernelWaitEventFlag(g_req_sess_ptr->evf_id, 3, 0x11, &outbits, NULL);
    sceKernelDeleteEventFlag(g_req_sess_ptr->evf_id);
    if (outbits & 2)
        return 0x8041110a;
    return (g_req_sess_ptr->state == 3) ? 0 : (int)0x80411109;
}

int sceNetAdhocTransInt_97D389A3(int total_size, void *arg, int (*cb)(void *, void **, u32 *))
{
    if (!g_child_inited)
        return 0x80411101;
    if (!g_req_sess_ptr)
        return 0x80411105;
    if (!cb || g_req_sess_ptr->state != 3)
        return 0x80411107;

    int ret = ptpSendStream(&g_req_sess_ptr->ptp_id, g_req_sess_ptr->peer_mac, total_size, arg, cb);
    g_req_sess_ptr->state = 4;
    if (ret == (int)0x80410708)
        ret = 0x8041110a;
    return ret;
}

int sceNetAdhocTransInt_D438691D(void *buf, u32 bufsize, void *arg, int (*cb)(void *, u32, void *, u32))
{
    if (!g_child_inited)
        return 0x80411101;
    if (!g_req_sess_ptr)
        return 0x80411105;
    if (!cb)
        return 0x80411103;
    if (g_req_sess_ptr->state != 3)
        return 0x80411107;

    int ret = ptpRecvStream(&g_req_sess_ptr->ptp_id, g_req_sess_ptr->peer_mac, buf, bufsize, arg, cb);
    g_req_sess_ptr->state = 4;
    if (ret == (int)0x80410708)
        ret = 0x8041110a;
    return ret;
}

static void reqMatchingCallback(int matchingid, int event, const u8 *mac, u32 optlen, const void *optdata)
{
    if (g_req_sess_ptr->matching_id != matchingid)
        return;

    int intr;
    switch (event) {
    case 1:
        if (g_req_sess_ptr->state != 1)
            return;
        sceKernelLockLwMutex(g_child_mutex, 1, NULL);
        TransPeerNode *node = childPeerFind(mac);
        if (node) {
            if (node->optlen == optlen && sceNet_lib_E0A81C7C(node->optdata, optdata, optlen) == 0) {
                sceKernelUnlockLwMutex(g_child_mutex, 1);
                return;
            }
            childPeerRemove(node);
            if (node->optlen != 0)
                transPoolFree(node->optdata);
        } else {
            node = transPoolAlloc(sizeof(TransPeerNode));
            if (!node) {
                sceKernelUnlockLwMutex(g_child_mutex, 1);
                return;
            }
        }
        sceKernelMemset(node, 0, sizeof(TransPeerNode));
        if (childPeerInit(node, mac, optlen, optdata) < 0)
            transPoolFree(node);
        else
            childPeerAppend(node);
        sceKernelUnlockLwMutex(g_child_mutex, 1);
        break;
    case 6:
        intr = sceKernelCpuSuspendIntr();
        if (g_req_sess_ptr->state == 2) {
            g_req_sess_ptr->state = 3;
            sceKernelSetEventFlag(g_req_sess_ptr->evf_id, 1);
        }
        sceKernelCpuResumeIntr(intr);
        break;
    case 8:
    case 10:
        if (g_req_sess_ptr->state == 1) {
            sceKernelLockLwMutex(g_child_mutex, 1, NULL);
            TransPeerNode *n = childPeerFind(mac);
            if (n) {
                childPeerRemove(n);
                transPoolFree(n->optdata);
                transPoolFree(n);
            }
            sceKernelUnlockLwMutex(g_child_mutex, 1);
            break;
        }
        /* fallthrough */
    case 4:
    case 9:
        intr = sceKernelCpuSuspendIntr();
        if (g_req_sess_ptr->state == 2 && sceNet_lib_E0A81C7C(g_req_sess_ptr->peer_mac, mac, 6) == 0) {
            g_req_sess_ptr->state = 4;
            sceKernelSetEventFlag(g_req_sess_ptr->evf_id, 1);
        }
        sceKernelCpuResumeIntr(intr);
        break;
    default:
        break;
    }
}

int sceNetAdhocTransInt_75DAEB6B(u32 poolsize)
{
    if (g_child_inited)
        return 0x80411102;
    int ret = transPoolCreate(poolsize);
    if (ret < 0)
        return ret;
    ret = sceKernelCreateLwMutex(g_child_mutex, "SceNetAdhocTransferChild", 0, 0, NULL);
    if (ret < 0) {
        transPoolDelete();
        return ret;
    }
    g_child_inited = 1;
    return 0;
}

int sceNetAdhocTransInt_A64B28F7(int prio, int stack)
{
    if (!g_child_inited)
        return 0x80411101;
    if (!g_req_sess_ptr)
        return 0x80411105;
    if (g_req_sess_ptr->state != 0)
        return 0x80411107;

    g_req_sess_ptr->state = 1;
    int ret = sceNetAdhocMatchingStart(g_req_sess_ptr->matching_id, prio, 0x1c00, stack, 0, 0, NULL);
    if (ret < 0) {
        g_req_sess_ptr->state = 0;
        return ret;
    }
    return 0;
}

int sceNetAdhocTransInt_48381015(void)
{
    if (!g_child_inited)
        return 0x80411101;
    if (!g_req_sess_ptr)
        return 0x80411105;

    int intr = sceKernelCpuSuspendIntr();
    if (g_req_sess_ptr->state != 2) {
        sceKernelCpuResumeIntr(intr);
        return 0x80411107;
    }
    g_req_sess_ptr->state = 4;
    sceKernelSetEventFlag(g_req_sess_ptr->evf_id, 2);
    sceKernelCpuResumeIntr(intr);
    return 0;
}

int sceNetAdhocTransInt_6F595DFA(void)
{
    if (!g_child_inited)
        return 0x80411101;
    if (!g_req_sess_ptr)
        return 0x80411105;
    if (g_req_sess_ptr->state != 3 || g_req_sess_ptr->ptp_id <= 0)
        return 0x80411107;
    ptpAbortTransfer(g_req_sess_ptr->ptp_id);
    return 0;
}

int sceNetAdhocTransInt_645E312A(void)
{
    if (!g_child_inited)
        return 0x80411101;
    if (!g_req_sess_ptr)
        return 0x80411105;

    sceNetAdhocMatchingStop(g_req_sess_ptr->matching_id);
    g_req_sess_ptr->state = 0;
    sceKernelMemset(g_req_sess_ptr->peer_mac, 0, 6);
    sceKernelLockLwMutex(g_child_mutex, 1, NULL);
    TransPeerNode *cur = g_child_list;
    while (cur) {
        TransPeerNode *next = cur->next;
        transPoolFree(cur->optdata);
        transPoolFree(cur);
        cur = next;
    }
    g_child_list = NULL;
    sceKernelUnlockLwMutex(g_child_mutex, 1);
    return 0;
}

int sceNetAdhocTransInt_24F04EF8(void)
{
    if (!g_child_inited)
        return 0x80411101;
    if (!g_req_sess_ptr)
        return 0x80411105;
    if (g_req_sess_ptr->state != 0)
        return 0x80411107;

    sceNetAdhocMatchingDelete(g_req_sess_ptr->matching_id);
    g_req_sess_ptr = NULL;
    return 0;
}

int sceNetAdhocTransInt_541E3EAB(void)
{
    if (!g_child_inited)
        return 0;
    if (g_req_sess_ptr)
        return 0x80411104;
    sceKernelDeleteLwMutex(g_child_mutex);
    transPoolDelete();
    g_child_inited = 0;
    return 0;
}

int sceNetAdhocTransInt_7CFB68C9(u32 *info)
{
    if (!g_child_inited)
        return 0x80411101;
    if (!info)
        return 0x80411103;
    return transPoolGetStatus(info);
}
