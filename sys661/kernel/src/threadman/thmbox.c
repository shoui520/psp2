#include "threadman_internal.h"

static int mbxUidCreate(SceSysmemUidCB *cb, SceSysmemUidCB *type, int func_id, void *args);
static int mbxUidDelete(SceSysmemUidCB *cb, SceSysmemUidCB *type, int func_id, void *args);

const TmanUidFunc g_uid_mbx_funcs[3] = {
    { TMAN_UID_FUNC_CREATE, (void *)mbxUidCreate },
    { TMAN_UID_FUNC_DELETE, (void *)mbxUidDelete },
    { 0, NULL },
};


/* ============================================================================
 * Message Boxes
 * ============================================================================ */

void tmanInitMbxType(void)
{
    sceKernelCreateUIDtypeInherit("WaitQ", "Mbx", 48,
                                  (SceSysmemUidLookupFunc *)g_uid_mbx_funcs,
                                  NULL, &g_tman.uid_mbx);
}

static int mbxUidCreate(SceSysmemUidCB *cb, SceSysmemUidCB *type, int func_id, void *args)
{
    sceKernelCallUIDObjCommonFunction(cb, type, func_id, args);
    return cb->uid ^ (s32)g_tman.uid_xor_key;
}

static int mbxUidDelete(SceSysmemUidCB *cb, SceSysmemUidCB *type, int func_id, void *args)
{
    sceKernelCallUIDObjCommonFunction(cb, type, func_id, args);
    return 0;
}

static int tmanMbxEnqueueMsg(TmanMbxCB *mbx, TmanMsgPacket *pkt, u32 attr)
{
    if (mbx->msg_head == NULL) {
        pkt->next = pkt;
        mbx->msg_head = pkt;
        mbx->num_msgs = 1;
        return 0;
    }

    TmanMsgPacket *tail = mbx->msg_head;
    TmanMsgPacket *cur = tail->next;
    do {
        if (cur == pkt)
            return (int)SCE_ERROR_KERNEL_MESSAGEBOX_DUPLICATE_MESSAGE;
        cur = cur->next;
    } while (cur != tail->next);

    if ((attr & 0x0400U) != 0) {
        TmanMsgPacket *prev = tail;
        cur = tail->next;
        do {
            if (pkt->msg_priority < cur->msg_priority) {
                pkt->next = cur;
                prev->next = pkt;
                mbx->num_msgs++;
                return 0;
            }
            prev = cur;
            cur = cur->next;
        } while (cur != tail->next);
    }

    pkt->next = tail->next;
    tail->next = pkt;
    mbx->msg_head = pkt;
    mbx->num_msgs++;
    return 0;
}

static inline __attribute__((always_inline)) TmanMsgPacket *tmanMbxDequeueMsg(TmanMbxCB *mbx)
{
    if (mbx->msg_head == NULL)
        return NULL;

    TmanMsgPacket *tail = mbx->msg_head;
    TmanMsgPacket *first = tail->next;
    if (first == tail)
        mbx->msg_head = NULL;
    else
        tail->next = first->next;
    mbx->num_msgs--;
    first->next = NULL;
    return first;
}

SceUID sceKernelCreateMbx(const char *name, u32 attr, void *opt)
{
    if (!tmanCheckUserAddr((u32)name) || !tmanCheckUserPtr(opt, 4))
        return (SceUID)SCE_ERROR_KERNEL_ILLEGAL_ADDR;
    if (tmanReadIntrDepth() != 0)
        return (SceUID)SCE_ERROR_KERNEL_CANNOT_BE_CALLED_FROM_INTERRUPT;
    if ((attr & ~0x5FFU) != 0)
        return (SceUID)SCE_ERROR_KERNEL_ILLEGAL_ATTR;

    int is_user = tmanIsUserCaller();
    int intr = sceKernelCpuSuspendIntr();
    SceSysmemUidCB *cb = NULL;
    int ret = sceKernelCreateUID(g_tman.uid_mbx, name,
                                 (char)((attr & 0xFFU) | (is_user ? 0xFFU : 0x00U)), &cb);
    if (ret != 0) {
        sceKernelCpuResumeIntr(intr);
        return ret;
    }

    TmanWaitQueue *wq = (TmanWaitQueue *)tmanUidData(cb, g_tman.uid_waitq);
    TmanMbxCB *mbx = (TmanMbxCB *)tmanUidData(cb, g_tman.uid_mbx);
    SceUID uid = cb->uid ^ (s32)g_tman.uid_xor_key;
    wq->attr = attr;
    mbx->opt = opt;
    mbx->num_msgs = 0;
    mbx->msg_head = NULL;

    if (g_tman.log_handler != NULL)
        g_tman.log_handler(74, (u32)mbx, 2, attr, uid);

    sceKernelCpuResumeIntr(intr);
    return uid;
}

int sceKernelDeleteMbx(SceUID mbxid)
{
    if (tmanReadIntrDepth() != 0)
        return (int)SCE_ERROR_KERNEL_CANNOT_BE_CALLED_FROM_INTERRUPT;

    int intr = sceKernelCpuSuspendIntr();
    SceSysmemUidCB *cb = NULL;
    if (sceKernelGetUIDcontrolBlockWithType(mbxid, g_tman.uid_mbx, &cb) != 0) {
        sceKernelCpuResumeIntr(intr);
        return (int)SCE_ERROR_KERNEL_NOT_FOUND_MESSAGE_BOX;
    }
    if (!tmanCheckUidAttr(cb, 0x10)) {
        sceKernelCpuResumeIntr(intr);
        return (int)SCE_ERROR_KERNEL_ILLEGAL_PERMISSION;
    }

    if (g_tman.log_handler != NULL)
        g_tman.log_handler(75, (u32)cb, 1, mbxid);

    sceKernelDeleteUID(mbxid);
    sceKernelCpuResumeIntr(intr);
    return 0;
}

int sceKernelSendMbx(SceUID mbxid, void *message)
{
    if (!tmanCheckUserPtr(message, sizeof(TmanMsgPacket)))
        return (int)SCE_ERROR_KERNEL_ILLEGAL_ADDR;

    int intr = sceKernelCpuSuspendIntr();
    int in_intr = (int)tmanReadIntrDepth();
    SceSysmemUidCB *cb = NULL;

    if (sceKernelGetUIDcontrolBlockWithType(mbxid, g_tman.uid_mbx, &cb) != 0) {
        sceKernelCpuResumeIntr(intr);
        return (int)SCE_ERROR_KERNEL_NOT_FOUND_MESSAGE_BOX;
    }
    if (!tmanCheckUidAttr(cb, 2)) {
        sceKernelCpuResumeIntr(intr);
        return (int)SCE_ERROR_KERNEL_ILLEGAL_PERMISSION;
    }

    TmanWaitQueue *wq = (TmanWaitQueue *)tmanUidData(cb, g_tman.uid_waitq);
    TmanMbxCB *mbx = (TmanMbxCB *)tmanUidData(cb, g_tman.uid_mbx);
    if (g_tman.log_handler != NULL)
        g_tman.log_handler(76, (u32)mbx, 2, mbxid, message);

    if (!tmanListEmpty(&wq->wait_list)) {
        TmanThreadCB *waiter = (TmanThreadCB *)wq->wait_list.next;
        void **out_msg = (void **)waiter->wait_param[0];
        if (out_msg != NULL)
            *out_msg = message;
        tmanWakeupFirstWaiter(cb, 0, in_intr, intr);
        sceKernelCpuResumeIntr(intr);
        return 0;
    }

    int ret = tmanMbxEnqueueMsg(mbx, (TmanMsgPacket *)message, wq->attr);
    sceKernelCpuResumeIntr(intr);
    return ret;
}

static inline __attribute__((always_inline)) int tmanReceiveMbxInternal(SceUID mbxid, void **pmessage, SceUInt *timeout, int use_cb)
{
    if (!tmanCheckUserPtr(pmessage, sizeof(void *)) || !tmanCheckUserPtr(timeout, sizeof(SceUInt)))
        return (int)SCE_ERROR_KERNEL_ILLEGAL_ADDR;
    if (tmanReadIntrDepth() != 0)
        return (int)SCE_ERROR_KERNEL_CANNOT_BE_CALLED_FROM_INTERRUPT;

    int intr = sceKernelCpuSuspendIntr();
    if (intr == 0 || g_tman.dispatch_suspended != 0) {
        sceKernelCpuResumeIntr(intr);
        return (int)SCE_ERROR_KERNEL_WAIT_CAN_NOT_WAIT;
    }

    tmanCheckKernelStackGuard();
    SceSysmemUidCB *cb = NULL;
    if (sceKernelGetUIDcontrolBlockWithType(mbxid, g_tman.uid_mbx, &cb) != 0) {
        sceKernelCpuResumeIntr(intr);
        return (int)SCE_ERROR_KERNEL_NOT_FOUND_MESSAGE_BOX;
    }
    if (!tmanCheckUidAttr(cb, 4)) {
        sceKernelCpuResumeIntr(intr);
        return (int)SCE_ERROR_KERNEL_ILLEGAL_PERMISSION;
    }

    TmanWaitQueue *wq = (TmanWaitQueue *)tmanUidData(cb, g_tman.uid_waitq);
    TmanMbxCB *mbx = (TmanMbxCB *)tmanUidData(cb, g_tman.uid_mbx);
    if (g_tman.log_handler != NULL)
        g_tman.log_handler(use_cb ? 78 : 77, (u32)mbx, 2, mbxid, pmessage);

    u64 abs_time = 0;
    if (timeout != NULL)
        tmanCalcAbsTimeoutUsec(&abs_time, *timeout);

    TmanThreadCB *cur = g_tman.current_thread;
    int ret;
    for (;;) {
        if (use_cb != 0 && cur->cb_notify_pending != 0)
            tmanCheckPendingCallbacks(intr);

        TmanMsgPacket *pkt = tmanMbxDequeueMsg(mbx);
        if (pkt != NULL) {
            if (pmessage != NULL)
                *pmessage = pkt;
            ret = 0;
            break;
        }

        cur->wait_param[0] = (u32)pmessage;
        cur->cb_wait_enabled = use_cb;
        ret = tmanWaitOnObject(cb, TMAN_WAIT_MBX, wq->attr, &abs_time, timeout);
        if (use_cb == 0 || ret != (int)SCE_ERROR_KERNEL_WAIT_STATUS_RELEASED_CALLBACK)
            break;
    }

    sceKernelCpuResumeIntr(intr);
    return ret;
}

int sceKernelReceiveMbx(SceUID mbxid, void **pmessage, SceUInt *timeout)
{
    return tmanReceiveMbxInternal(mbxid, pmessage, timeout, 0);
}

int sceKernelReceiveMbxCB(SceUID mbxid, void **pmessage, SceUInt *timeout)
{
    return tmanReceiveMbxInternal(mbxid, pmessage, timeout, 1);
}

int sceKernelPollMbx(SceUID mbxid, void **pmessage)
{
    if (!tmanCheckUserPtr(pmessage, sizeof(void *)))
        return (int)SCE_ERROR_KERNEL_ILLEGAL_ADDR;

    int intr = sceKernelCpuSuspendIntr();
    SceSysmemUidCB *cb = NULL;
    if (sceKernelGetUIDcontrolBlockWithType(mbxid, g_tman.uid_mbx, &cb) != 0) {
        sceKernelCpuResumeIntr(intr);
        return (int)SCE_ERROR_KERNEL_NOT_FOUND_MESSAGE_BOX;
    }
    if (!tmanCheckUidAttr(cb, 4)) {
        sceKernelCpuResumeIntr(intr);
        return (int)SCE_ERROR_KERNEL_ILLEGAL_PERMISSION;
    }

    TmanMbxCB *mbx = (TmanMbxCB *)tmanUidData(cb, g_tman.uid_mbx);
    if (g_tman.log_handler != NULL)
        g_tman.log_handler(79, (u32)mbx, 2, mbxid, pmessage);

    TmanMsgPacket *pkt = tmanMbxDequeueMsg(mbx);
    if (pkt == NULL) {
        sceKernelCpuResumeIntr(intr);
        return (int)SCE_ERROR_KERNEL_MESSAGEBOX_NO_MESSAGE;
    }

    if (pmessage != NULL)
        *pmessage = pkt;
    sceKernelCpuResumeIntr(intr);
    return 0;
}

int sceKernelCancelReceiveMbx(SceUID mbxid, int *num_wait_threads)
{
    if (!tmanCheckUserPtr(num_wait_threads, sizeof(int)))
        return (int)SCE_ERROR_KERNEL_ILLEGAL_ADDR;

    int intr = sceKernelCpuSuspendIntr();
    SceSysmemUidCB *cb = NULL;
    if (sceKernelGetUIDcontrolBlockWithType(mbxid, g_tman.uid_mbx, &cb) != 0) {
        sceKernelCpuResumeIntr(intr);
        return (int)SCE_ERROR_KERNEL_NOT_FOUND_MESSAGE_BOX;
    }
    if (!tmanCheckUidAttr(cb, 8)) {
        sceKernelCpuResumeIntr(intr);
        return (int)SCE_ERROR_KERNEL_ILLEGAL_PERMISSION;
    }

    if (g_tman.log_handler != NULL)
        g_tman.log_handler(80, (u32)cb, 1, mbxid);

    int woken = sceKernelCallUIDObjFunction(cb, (s32)TMAN_UID_FUNC_CANCEL_WAIT);
    if (num_wait_threads != NULL)
        *num_wait_threads = woken;

    sceKernelCpuResumeIntr(intr);
    return 0;
}

int sceKernelReferMbxStatus(SceUID mbxid, TmanMbxInfo *info)
{
    if (!tmanCheckUserPtr(info, sizeof(*info)))
        return (int)SCE_ERROR_KERNEL_ILLEGAL_ADDR;

    int intr = sceKernelCpuSuspendIntr();
    SceSysmemUidCB *cb = NULL;
    if (sceKernelGetUIDcontrolBlockWithType(mbxid, g_tman.uid_mbx, &cb) != 0) {
        sceKernelCpuResumeIntr(intr);
        return (int)SCE_ERROR_KERNEL_NOT_FOUND_MESSAGE_BOX;
    }
    if (!tmanCheckUidAttr(cb, 1)) {
        sceKernelCpuResumeIntr(intr);
        return (int)SCE_ERROR_KERNEL_ILLEGAL_PERMISSION;
    }

    if (g_tman.log_handler != NULL)
        g_tman.log_handler(81, (u32)&g_tman.current_thread, 2, mbxid, info);

    TmanWaitQueue *wq = (TmanWaitQueue *)tmanUidData(cb, g_tman.uid_waitq);
    TmanMbxCB *mbx = (TmanMbxCB *)tmanUidData(cb, g_tman.uid_mbx);
    TmanMbxInfo local;
    memset(&local, 0, sizeof(local));
    local.size = sizeof(local);
    if (cb->name != NULL)
        strncpy(local.name, cb->name, SCE_UID_NAME_LEN);
    local.attr = wq->attr;
    local.num_wait_threads = wq->num_wait_threads;
    local.num_messages = mbx->num_msgs;
    local.first_message = (mbx->msg_head != NULL) ? mbx->msg_head->next : NULL;

    SceSize copy_size = info->size < local.size ? info->size : local.size;
    memcpy(info, &local, copy_size);
    sceKernelCpuResumeIntr(intr);
    return 0;
}

