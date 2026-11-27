#include "threadman_internal.h"

static int mppUidCreate(SceSysmemUidCB *cb, SceSysmemUidCB *type, int func_id, void *args);
static int mppUidDelete(SceSysmemUidCB *cb, SceSysmemUidCB *type, int func_id, void *args);
static int mppUidCheck(SceSysmemUidCB *cb, SceSysmemUidCB *type, int func_id, void *args);
static int mppUidRemove(SceSysmemUidCB *cb, SceSysmemUidCB *type, int func_id, void *args);

const TmanUidFunc g_uid_msgpipe_funcs[5] = {
    { TMAN_UID_FUNC_REMOVE_WAIT, (void *)mppUidRemove },
    { TMAN_UID_FUNC_CHECK_WAIT,  (void *)mppUidCheck },
    { TMAN_UID_FUNC_CREATE,      (void *)mppUidCreate },
    { TMAN_UID_FUNC_DELETE,      (void *)mppUidDelete },
    { 0, NULL },
};


/* ============================================================================
 * Message Pipes
 * ============================================================================ */

void tmanMsgPipeWriteRing(TmanMsgPipeCB *mpp, const u8 *src, u32 len)
{
    if (len == 0 || mpp->buf == NULL)
        return;
    u32 first = mpp->buf_size - mpp->write_pos;
    if (first > len)
        first = len;
    memcpy(mpp->buf + mpp->write_pos, src, first);
    if (len > first)
        memcpy(mpp->buf, src + first, len - first);
    mpp->write_pos = (mpp->write_pos + len) % mpp->buf_size;
    mpp->free_size -= len;
}

static void tmanMsgPipeReadRing(TmanMsgPipeCB *mpp, u8 *dst, u32 len)
{
    if (len == 0 || mpp->buf == NULL)
        return;
    u32 first = mpp->buf_size - mpp->read_pos;
    if (first > len)
        first = len;
    memcpy(dst, mpp->buf + mpp->read_pos, first);
    if (len > first)
        memcpy(dst + first, mpp->buf, len - first);
    mpp->read_pos = (mpp->read_pos + len) % mpp->buf_size;
    mpp->free_size += len;
}

SceUID sceKernelCreateMsgPipe(const char *name, int part, int attr, void *buf_size_arg, void *opt)
{
    u32 buf_size = (u32)buf_size_arg;
    if (!tmanCheckUserAddr((u32)name) || !tmanCheckUserPtr(opt, 4))
        return (SceUID)SCE_ERROR_KERNEL_ILLEGAL_ADDR;
    if (tmanReadIntrDepth() != 0)
        return (SceUID)SCE_ERROR_KERNEL_CANNOT_BE_CALLED_FROM_INTERRUPT;
    if ((attr & ~0x31FF) != 0)
        return (SceUID)SCE_ERROR_KERNEL_ILLEGAL_ATTR;

    int is_user = tmanIsUserCaller();
    int intr = sceKernelCpuSuspendIntr();
    SceUID mem_id = 0;
    u8 *buf = NULL;

    if (buf_size > 0) {
        int alloc_type = ((attr & 0x4000) != 0) ? 1 : 0;
        mem_id = sceKernelAllocPartitionMemory(part, name, alloc_type, buf_size, NULL);
        if (mem_id < 0) {
            sceKernelCpuResumeIntr(intr);
            return (SceUID)SCE_ERROR_KERNEL_NO_MEMORY;
        }
        buf = (u8 *)sceKernelGetBlockHeadAddr(mem_id);
    }

    SceSysmemUidCB *cb = NULL;
    int ret = sceKernelCreateUID(g_tman.uid_msgpipe, name,
                                 (char)((attr & 0xFF) | (is_user ? 0xFF : 0x00)), &cb);
    if (ret != 0) {
        if (mem_id > 0)
            sceKernelFreePartitionMemory(mem_id);
        sceKernelCpuResumeIntr(intr);
        return ret;
    }

    TmanWaitQueue *wq = (TmanWaitQueue *)tmanUidData(cb, g_tman.uid_waitq);
    TmanMsgPipeCB *mpp = (TmanMsgPipeCB *)tmanUidData(cb, g_tman.uid_msgpipe);
    SceUID uid = cb->uid ^ (s32)g_tman.uid_xor_key;
    wq->attr = (u32)attr;
    mpp->opt = opt;
    mpp->buf_block_id = mem_id;
    mpp->buf = buf;
    mpp->buf_size = buf_size;
    mpp->free_size = buf_size;
    mpp->read_pos = 0;
    mpp->write_pos = 0;
    tmanListInit(&mpp->send_wait_list);
    mpp->num_send_wait = 0;
    mpp->attr = (u32)attr;

    sceKernelCpuResumeIntr(intr);
    return uid;
}

int sceKernelDeleteMsgPipe(SceUID uid)
{
    if (tmanReadIntrDepth() != 0)
        return (int)SCE_ERROR_KERNEL_CANNOT_BE_CALLED_FROM_INTERRUPT;

    int intr = sceKernelCpuSuspendIntr();
    SceSysmemUidCB *cb = NULL;
    if (sceKernelGetUIDcontrolBlockWithType(uid, g_tman.uid_msgpipe, &cb) != 0) {
        sceKernelCpuResumeIntr(intr);
        return (int)SCE_ERROR_KERNEL_NOT_FOUND_MESSAGE_PIPE;
    }
    if (!tmanCheckUidAttr(cb, 0x10)) {
        sceKernelCpuResumeIntr(intr);
        return (int)SCE_ERROR_KERNEL_ILLEGAL_PERMISSION;
    }

    sceKernelDeleteUID(uid);
    sceKernelCpuResumeIntr(intr);
    return 0;
}

static inline __attribute__((always_inline)) int tmanSendMsgPipeInternal(SceUID uid, void *message, unsigned int size,
                                   int wait_mode, void *result, unsigned int *timeout,
                                   int is_try, int use_cb)
{
    if (!tmanCheckUserPtr(message, size) || !tmanCheckUserPtr(result, sizeof(u32)) ||
        !tmanCheckUserPtr(timeout, sizeof(u32)))
        return (int)SCE_ERROR_KERNEL_ILLEGAL_ADDR;
    if (!is_try && tmanReadIntrDepth() != 0)
        return (int)SCE_ERROR_KERNEL_CANNOT_BE_CALLED_FROM_INTERRUPT;
    if ((wait_mode & ~1) != 0)
        return (int)SCE_ERROR_KERNEL_ILLEGAL_MODE;

    int intr = sceKernelCpuSuspendIntr();
    if (!is_try && (intr == 0 || g_tman.dispatch_suspended != 0)) {
        sceKernelCpuResumeIntr(intr);
        return (int)SCE_ERROR_KERNEL_WAIT_CAN_NOT_WAIT;
    }

    SceSysmemUidCB *cb = NULL;
    if (sceKernelGetUIDcontrolBlockWithType(uid, g_tman.uid_msgpipe, &cb) != 0) {
        sceKernelCpuResumeIntr(intr);
        return (int)SCE_ERROR_KERNEL_NOT_FOUND_MESSAGE_PIPE;
    }
    if (!tmanCheckUidAttr(cb, 2)) {
        sceKernelCpuResumeIntr(intr);
        return (int)SCE_ERROR_KERNEL_ILLEGAL_PERMISSION;
    }

    TmanWaitQueue *wq = (TmanWaitQueue *)tmanUidData(cb, g_tman.uid_waitq);
    TmanMsgPipeCB *mpp = (TmanMsgPipeCB *)tmanUidData(cb, g_tman.uid_msgpipe);
    if (mpp->buf_size != 0 && size > mpp->buf_size && wait_mode == 0) {
        sceKernelCpuResumeIntr(intr);
        return (int)SCE_ERROR_KERNEL_ILLEGAL_SIZE;
    }

    u64 abs_time = 0;
    if (timeout != NULL)
        tmanCalcAbsTimeoutUsec(&abs_time, *timeout);

    const u8 *src = (const u8 *)message;
    u32 sent = 0;
    TmanThreadCB *cur = g_tman.current_thread;
    int ret = 0;

    for (;;) {
        if (use_cb != 0 && cur->cb_notify_pending != 0)
            tmanCheckPendingCallbacks(intr);

        /* First deliver directly to waiting receivers */
        while (sent < size && !tmanListEmpty(&wq->wait_list)) {
            TmanThreadCB *rcvr = (TmanThreadCB *)wq->wait_list.next;
            u8 *dst = (u8 *)rcvr->wait_param[0];
            u32 dst_rem = rcvr->wait_param[1];
            u32 chunk = size - sent;
            if (chunk > dst_rem)
                chunk = dst_rem;
            memcpy(dst, src + sent, chunk);
            sent += chunk;
            rcvr->wait_param[0] += chunk;
            rcvr->wait_param[1] -= chunk;
            rcvr->wait_param[3] += chunk;
            if (rcvr->wait_param[1] == 0 || (rcvr->wait_param[2] & 1) != 0) {
                if (rcvr->wait_extra[0] != 0)
                    *(u32 *)rcvr->wait_extra[0] = rcvr->wait_param[3];
                tmanWakeupWaitingThread(rcvr);
                g_tman.next_thread = NULL;
            }
        }

        /* Buffer remaining bytes into ring buffer */
        if (sent < size && mpp->free_size > 0) {
            u32 chunk = size - sent;
            if (chunk > mpp->free_size)
                chunk = mpp->free_size;
            if (wait_mode == 0 && chunk < (size - sent))
                chunk = 0;
            if (chunk > 0) {
                tmanMsgPipeWriteRing(mpp, src + sent, chunk);
                sent += chunk;
            }
        }

        if (sent == size || (wait_mode != 0 && sent > 0)) {
            if (result != NULL)
                *(u32 *)result = sent;
            if (g_tman.next_thread == NULL && tmanReadIntrDepth() == 0)
                tmanSchedule(0, intr);
            ret = 0;
            break;
        }

        if (is_try) {
            ret = (int)SCE_ERROR_KERNEL_MESSAGE_PIPE_FULL;
            break;
        }

        cur->wait_param[0] = (u32)(src + sent);
        cur->wait_param[1] = size - sent;
        cur->wait_param[2] = (u32)wait_mode;
        cur->wait_param[3] = sent;
        cur->wait_extra[0] = (u32)result;
        cur->cb_wait_enabled = use_cb;
        ret = tmanAddThreadToWaitQueue(cb, TMAN_WAIT_MSGPIPE, wq->attr, &abs_time, timeout,
                                       (u32 *)&mpp->num_send_wait);
        if (use_cb == 0 || ret != (int)SCE_ERROR_KERNEL_WAIT_STATUS_RELEASED_CALLBACK)
            break;
    }

    sceKernelCpuResumeIntr(intr);
    return ret;
}

int sceKernelSendMsgPipe(SceUID uid, void *message, unsigned int size,
                         int wait_mode, void *result, unsigned int *timeout)
{
    return tmanSendMsgPipeInternal(uid, message, size, wait_mode, result, timeout, 0, 0);
}

int sceKernelSendMsgPipeCB(SceUID uid, void *message, unsigned int size,
                           int wait_mode, void *result, unsigned int *timeout)
{
    return tmanSendMsgPipeInternal(uid, message, size, wait_mode, result, timeout, 0, 1);
}

int sceKernelTrySendMsgPipe(SceUID uid, void *message, unsigned int size,
                            int wait_mode, void *result)
{
    return tmanSendMsgPipeInternal(uid, message, size, wait_mode, result, NULL, 1, 0);
}

static inline __attribute__((always_inline)) int tmanReceiveMsgPipeInternal(SceUID uid, void *message, unsigned int size,
                                      int wait_mode, void *result, unsigned int *timeout,
                                      int is_try, int use_cb)
{
    if (!tmanCheckUserPtr(message, size) || !tmanCheckUserPtr(result, sizeof(u32)) ||
        !tmanCheckUserPtr(timeout, sizeof(u32)))
        return (int)SCE_ERROR_KERNEL_ILLEGAL_ADDR;
    if (!is_try && tmanReadIntrDepth() != 0)
        return (int)SCE_ERROR_KERNEL_CANNOT_BE_CALLED_FROM_INTERRUPT;
    if ((wait_mode & ~1) != 0)
        return (int)SCE_ERROR_KERNEL_ILLEGAL_MODE;

    int intr = sceKernelCpuSuspendIntr();
    if (!is_try && (intr == 0 || g_tman.dispatch_suspended != 0)) {
        sceKernelCpuResumeIntr(intr);
        return (int)SCE_ERROR_KERNEL_WAIT_CAN_NOT_WAIT;
    }

    SceSysmemUidCB *cb = NULL;
    if (sceKernelGetUIDcontrolBlockWithType(uid, g_tman.uid_msgpipe, &cb) != 0) {
        sceKernelCpuResumeIntr(intr);
        return (int)SCE_ERROR_KERNEL_NOT_FOUND_MESSAGE_PIPE;
    }
    if (!tmanCheckUidAttr(cb, 4)) {
        sceKernelCpuResumeIntr(intr);
        return (int)SCE_ERROR_KERNEL_ILLEGAL_PERMISSION;
    }

    TmanWaitQueue *wq = (TmanWaitQueue *)tmanUidData(cb, g_tman.uid_waitq);
    TmanMsgPipeCB *mpp = (TmanMsgPipeCB *)tmanUidData(cb, g_tman.uid_msgpipe);
    if (mpp->buf_size != 0 && size > mpp->buf_size && wait_mode == 0) {
        sceKernelCpuResumeIntr(intr);
        return (int)SCE_ERROR_KERNEL_ILLEGAL_SIZE;
    }

    u64 abs_time = 0;
    if (timeout != NULL)
        tmanCalcAbsTimeoutUsec(&abs_time, *timeout);

    u8 *dst = (u8 *)message;
    u32 recvd = 0;
    TmanThreadCB *cur = g_tman.current_thread;
    int ret = 0;

    for (;;) {
        if (use_cb != 0 && cur->cb_notify_pending != 0)
            tmanCheckPendingCallbacks(intr);

        u32 avail = mpp->buf_size - mpp->free_size;
        if (avail > 0) {
            u32 chunk = size - recvd;
            if (chunk > avail)
                chunk = avail;
            if (wait_mode == 0 && chunk < (size - recvd))
                chunk = 0;
            if (chunk > 0) {
                tmanMsgPipeReadRing(mpp, dst + recvd, chunk);
                recvd += chunk;
            }
        }

        if (recvd == size || (wait_mode != 0 && recvd > 0)) {
            if (result != NULL)
                *(u32 *)result = recvd;
            ret = 0;
            break;
        }

        if (is_try) {
            ret = (int)SCE_ERROR_KERNEL_MESSAGE_PIPE_EMPTY;
            break;
        }

        cur->wait_param[0] = (u32)(dst + recvd);
        cur->wait_param[1] = size - recvd;
        cur->wait_param[2] = (u32)wait_mode;
        cur->wait_param[3] = recvd;
        cur->wait_extra[0] = (u32)result;
        cur->cb_wait_enabled = use_cb;
        ret = tmanWaitOnObject(cb, TMAN_WAIT_MSGPIPE, wq->attr, &abs_time, timeout);
        if (use_cb == 0 || ret != (int)SCE_ERROR_KERNEL_WAIT_STATUS_RELEASED_CALLBACK)
            break;
    }

    sceKernelCpuResumeIntr(intr);
    return ret;
}

int sceKernelReceiveMsgPipe(SceUID uid, void *message, unsigned int size,
                            int wait_mode, void *result, unsigned int *timeout)
{
    return tmanReceiveMsgPipeInternal(uid, message, size, wait_mode, result, timeout, 0, 0);
}

int sceKernelReceiveMsgPipeCB(SceUID uid, void *message, unsigned int size,
                              int wait_mode, void *result, unsigned int *timeout)
{
    return tmanReceiveMsgPipeInternal(uid, message, size, wait_mode, result, timeout, 0, 1);
}

int sceKernelTryReceiveMsgPipe(SceUID uid, void *message, unsigned int size,
                               int wait_mode, void *result)
{
    return tmanReceiveMsgPipeInternal(uid, message, size, wait_mode, result, NULL, 1, 0);
}

int sceKernelCancelMsgPipe(SceUID uid, int *psend, int *precv)
{
    if (!tmanCheckUserPtr(psend, sizeof(int)) || !tmanCheckUserPtr(precv, sizeof(int)))
        return (int)SCE_ERROR_KERNEL_ILLEGAL_ADDR;

    int intr = sceKernelCpuSuspendIntr();
    SceSysmemUidCB *cb = NULL;
    if (sceKernelGetUIDcontrolBlockWithType(uid, g_tman.uid_msgpipe, &cb) != 0) {
        sceKernelCpuResumeIntr(intr);
        return (int)SCE_ERROR_KERNEL_NOT_FOUND_MESSAGE_PIPE;
    }
    if (!tmanCheckUidAttr(cb, 8)) {
        sceKernelCpuResumeIntr(intr);
        return (int)SCE_ERROR_KERNEL_ILLEGAL_PERMISSION;
    }

    TmanMsgPipeCB *mpp = (TmanMsgPipeCB *)tmanUidData(cb, g_tman.uid_msgpipe);
    int send_cnt = mpp->num_send_wait;
    int total = sceKernelCallUIDObjFunction(cb, (s32)TMAN_UID_FUNC_CANCEL_WAIT);
    mpp->read_pos = 0;
    mpp->write_pos = 0;
    mpp->free_size = mpp->buf_size;
    mpp->num_send_wait = 0;

    if (psend != NULL)
        *psend = send_cnt;
    if (precv != NULL)
        *precv = total - send_cnt;

    sceKernelCpuResumeIntr(intr);
    return 0;
}

int sceKernelReferMsgPipeStatus(SceUID uid, SceKernelMppInfo *info)
{
    if (!tmanCheckUserPtr(info, sizeof(*info)))
        return (int)SCE_ERROR_KERNEL_ILLEGAL_ADDR;

    int intr = sceKernelCpuSuspendIntr();
    SceSysmemUidCB *cb = NULL;
    if (sceKernelGetUIDcontrolBlockWithType(uid, g_tman.uid_msgpipe, &cb) != 0) {
        sceKernelCpuResumeIntr(intr);
        return (int)SCE_ERROR_KERNEL_NOT_FOUND_MESSAGE_PIPE;
    }
    if (!tmanCheckUidAttr(cb, 1)) {
        sceKernelCpuResumeIntr(intr);
        return (int)SCE_ERROR_KERNEL_ILLEGAL_PERMISSION;
    }

    TmanWaitQueue *wq = (TmanWaitQueue *)tmanUidData(cb, g_tman.uid_waitq);
    TmanMsgPipeCB *mpp = (TmanMsgPipeCB *)tmanUidData(cb, g_tman.uid_msgpipe);
    SceKernelMppInfo local;
    memset(&local, 0, sizeof(local));
    local.size = sizeof(local);
    if (cb->name != NULL)
        strncpy(local.name, cb->name, SCE_UID_NAME_LEN);
    local.attr = wq->attr;
    local.bufSize = (int)mpp->buf_size;
    local.freeSize = (int)mpp->free_size;
    local.numSendWaitThreads = mpp->num_send_wait;
    local.numReceiveWaitThreads = wq->num_wait_threads - mpp->num_send_wait;

    SceSize copy_size = info->size < local.size ? info->size : local.size;
    memcpy(info, &local, copy_size);
    sceKernelCpuResumeIntr(intr);
    return 0;
}

/* ============================================================================
 * MsgPipe UID helpers
 * ============================================================================ */

void tmanInitMsgPipeType(void)
{
    sceKernelCreateUIDtypeInherit("WaitQ", "MsgPipe", 48,
                                  (SceSysmemUidLookupFunc *)g_uid_msgpipe_funcs,
                                  NULL, &g_tman.uid_msgpipe);
}

SceUID tmanFindMsgPipeByMemBlock(SceUID block_id)
{
    SceSysmemUidCB *head = g_tman.uid_msgpipe;
    SceSysmemUidCB *cb = head->PARENT0;
    while (cb != head) {
        TmanMsgPipeCB *mpp = (TmanMsgPipeCB *)tmanUidData(cb, head);
        if (mpp->buf_block_id == block_id)
            return cb->uid ^ (s32)g_tman.uid_xor_key;
        cb = cb->PARENT0;
    }
    return 0;
}

static int mppUidCreate(SceSysmemUidCB *cb, SceSysmemUidCB *type, int func_id, void *args)
{
    sceKernelCallUIDObjCommonFunction(cb, type, func_id, args);
    return cb->uid ^ (s32)g_tman.uid_xor_key;
}

static int mppUidDelete(SceSysmemUidCB *cb, SceSysmemUidCB *type, int func_id, void *args)
{
    TmanMsgPipeCB *mpp = (TmanMsgPipeCB *)tmanUidData(cb, type);
    if (mpp->buf_block_id > 0) {
        sceKernelFreePartitionMemory(mpp->buf_block_id);
        mpp->buf_block_id = 0;
    }
    sceKernelCallUIDObjCommonFunction(cb, type, func_id, args);
    return 0;
}

static int mppUidCheck(SceSysmemUidCB *cb, SceSysmemUidCB *type, int func_id, void *args)
{
    (void)type;
    (void)func_id;
    (void)args;
    TmanWaitQueue *wq = (TmanWaitQueue *)tmanUidData(cb, g_tman.uid_waitq);
    TmanMsgPipeCB *mpp = (TmanMsgPipeCB *)tmanUidData(cb, g_tman.uid_msgpipe);
    int woken = 0;
    if (wq->num_wait_threads > 0 && !tmanListEmpty(&wq->wait_list)) {
        TmanListNode *head = &wq->wait_list;
        TmanListNode *cur = head->next;
        while (cur != head) {
            TmanThreadCB *th = (TmanThreadCB *)cur;
            cur = cur->next;
            u32 remain = th->wait_param[1];
            u32 mode = th->wait_extra[0];
            u32 avail = mpp->free_size;
            u32 chunk = (avail < remain) ? avail : remain;
            if ((mode != 1U || chunk == 0) && (mode != 0U || chunk != remain))
                break;
            tmanMsgPipeWriteRing(mpp, (const u8 *)th->wait_param[0], chunk);
            th->wait_param[1] = remain - chunk;
            u32 *out_size = (u32 *)th->wait_param[3];
            if (out_size != NULL)
                *out_size = th->wait_param[2] - th->wait_param[1];
            woken += tmanWakeupWaitingThread(th);
        }
    }
    return woken;
}

static int mppUidRemove(SceSysmemUidCB *cb, SceSysmemUidCB *type, int func_id, void *args)
{
    (void)type;
    (void)func_id;
    (void)args;
    TmanWaitQueue *wq = (TmanWaitQueue *)tmanUidData(cb, g_tman.uid_waitq);
    wq->skip_first_pri = 0;
    if (!tmanListEmpty(&wq->wait_list)) {
        TmanThreadCB *head_th = (TmanThreadCB *)wq->wait_list.next;
        wq->skip_first_pri = (head_th->wait_param[2] != head_th->wait_param[1]) ? 1U : 0U;
    }
    wq->num_wait_threads--;
    return 0;
}

