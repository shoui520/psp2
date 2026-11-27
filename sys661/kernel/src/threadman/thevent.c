#include "threadman_internal.h"

static int evfUidCreate(SceSysmemUidCB *cb, SceSysmemUidCB *type, int func_id, void *args);
static int evfUidDelete(SceSysmemUidCB *cb, SceSysmemUidCB *type, int func_id, void *args);
static int evfUidRemove(SceSysmemUidCB *cb, SceSysmemUidCB *type, int func_id, void *args);

const TmanUidFunc g_uid_evf_funcs[4] = {
    { TMAN_UID_FUNC_REMOVE_WAIT, (void *)evfUidRemove },
    { TMAN_UID_FUNC_CREATE,      (void *)evfUidCreate },
    { TMAN_UID_FUNC_DELETE,      (void *)evfUidDelete },
    { 0, NULL },
};


/* ============================================================================
 * Event Flags
 * ============================================================================ */

void tmanInitEventFlagType(void)
{
    sceKernelCreateUIDtypeInherit("WaitQ", "EventFlag", 48,
                                  (SceSysmemUidLookupFunc *)g_uid_evf_funcs,
                                  NULL, &g_tman.uid_evf);
}

static int evfUidCreate(SceSysmemUidCB *cb, SceSysmemUidCB *type, int func_id, void *args)
{
    sceKernelCallUIDObjCommonFunction(cb, type, func_id, args);
    return cb->uid ^ (s32)g_tman.uid_xor_key;
}

static int evfUidDelete(SceSysmemUidCB *cb, SceSysmemUidCB *type, int func_id, void *args)
{
    TmanEvfBlock *evf = (TmanEvfBlock *)tmanUidData(cb, g_tman.uid_evf);
    if (evf->wait_owner != NULL)
        tmanListRemove(&evf->owner_link);
    sceKernelCallUIDObjCommonFunction(cb, type, func_id, args);
    return 0;
}

static int evfUidRemove(SceSysmemUidCB *cb, SceSysmemUidCB *type, int func_id, void *args)
{
    (void)type;
    (void)func_id;
    TmanThreadCB *th = (args != NULL) ? *(TmanThreadCB **)args : NULL;
    TmanWaitQueue *wq = (TmanWaitQueue *)tmanUidData(cb, g_tman.uid_waitq);
    TmanEvfBlock *evf = (TmanEvfBlock *)tmanUidData(cb, g_tman.uid_evf);
    wq->num_wait_threads--;
    if (th != NULL) {
        if (th->wait_result != (s32)SCE_ERROR_KERNEL_WAIT_TIMEOUT && evf->wait_owner != NULL) {
            evf->wait_owner_refs--;
            if (evf->wait_owner_refs == 0) {
                evf->wait_owner = NULL;
                tmanListRemove(&evf->owner_link);
            }
        }
        u32 *out_bits = (u32 *)th->wait_param[2];
        if (out_bits != NULL)
            *out_bits = evf->cur_pattern;
    }
    return 0;
}

void tmanReleaseOwnedEventFlags(TmanThreadCB *th)
{
    TmanListNode *head = &th->owned_lwmutex_list;
    TmanListNode *node = head->next;
    while (node != head) {
        TmanListNode *next = node->next;
        TmanEvfBlock *evf = (TmanEvfBlock *)((u8 *)node - offsetof(TmanEvfBlock, owner_link));
        if (evf->wait_owner != NULL) {
            evf->wait_owner = NULL;
            evf->wait_owner_refs = 0;
            tmanListRemove(node);
        }
        node = next;
    }
}

static inline __attribute__((always_inline)) int tmanEvalEventFlag(u32 cur_pattern, u32 req_bits, u32 wait_mode)
{
    if ((wait_mode & SCE_KERNEL_EW_OR) != 0)
        return (cur_pattern & req_bits) != 0;
    return (cur_pattern & req_bits) == req_bits;
}

static inline __attribute__((always_inline)) void tmanApplyEventFlagClear(TmanEvfBlock *evf, u32 req_bits, u32 wait_mode)
{
    if ((wait_mode & SCE_KERNEL_EW_CLEAR_ALL) != 0)
        evf->cur_pattern = 0;
    else if ((wait_mode & SCE_KERNEL_EW_CLEAR_PAT) != 0)
        evf->cur_pattern &= ~req_bits;
}

SceUID sceKernelCreateEventFlag(const char *name, int attr, int bits, SceKernelEventFlagOptParam *opt)
{
    if (!tmanCheckUserAddr((u32)name) || !tmanCheckUserPtr(opt, sizeof(*opt)))
        return (SceUID)SCE_ERROR_KERNEL_ILLEGAL_ADDR;
    if (tmanReadIntrDepth() != 0)
        return (SceUID)SCE_ERROR_KERNEL_CANNOT_BE_CALLED_FROM_INTERRUPT;
    if ((attr & ~0x3FF) != 0)
        return (SceUID)SCE_ERROR_KERNEL_ILLEGAL_ATTR;

    int is_user = tmanIsUserCaller();
    int intr = sceKernelCpuSuspendIntr();
    SceSysmemUidCB *cb = NULL;
    int ret = sceKernelCreateUID(g_tman.uid_evf, name,
                                 (char)((attr & 0xFF) | (is_user ? 0xFF : 0x00)), &cb);
    if (ret != 0) {
        sceKernelCpuResumeIntr(intr);
        return ret;
    }

    TmanWaitQueue *wq = (TmanWaitQueue *)tmanUidData(cb, g_tman.uid_waitq);
    TmanEvfBlock *evf = (TmanEvfBlock *)tmanUidData(cb, g_tman.uid_evf);
    SceUID uid = cb->uid ^ (s32)g_tman.uid_xor_key;
    wq->attr = (u32)attr;
    evf->opt = opt;
    evf->init_pattern = (u32)bits;
    evf->cur_pattern = (u32)bits;

    if (g_tman.log_handler != NULL)
        g_tman.log_handler(65, (u32)evf, 3, attr, bits, uid);

    sceKernelCpuResumeIntr(intr);
    return uid;
}

int sceKernelDeleteEventFlag(SceUID evid)
{
    if (tmanReadIntrDepth() != 0)
        return (int)SCE_ERROR_KERNEL_CANNOT_BE_CALLED_FROM_INTERRUPT;

    int intr = sceKernelCpuSuspendIntr();
    SceSysmemUidCB *cb = NULL;
    if (sceKernelGetUIDcontrolBlockWithType(evid, g_tman.uid_evf, &cb) != 0) {
        sceKernelCpuResumeIntr(intr);
        return (int)SCE_ERROR_KERNEL_NOT_FOUND_EVENT_FLAG;
    }
    if (!tmanCheckUidAttr(cb, 0x10)) {
        sceKernelCpuResumeIntr(intr);
        return (int)SCE_ERROR_KERNEL_ILLEGAL_PERMISSION;
    }

    if (g_tman.log_handler != NULL)
        g_tman.log_handler(66, (u32)cb, 1, evid);

    sceKernelDeleteUID(evid);
    sceKernelCpuResumeIntr(intr);
    return 0;
}

int sceKernelSetEventFlag(SceUID evid, u32 bits)
{
    int intr = sceKernelCpuSuspendIntr();
    int in_intr = (int)tmanReadIntrDepth();
    SceSysmemUidCB *cb = NULL;

    if (sceKernelGetUIDcontrolBlockWithType(evid, g_tman.uid_evf, &cb) != 0) {
        sceKernelCpuResumeIntr(intr);
        return (int)SCE_ERROR_KERNEL_NOT_FOUND_EVENT_FLAG;
    }
    if (!tmanCheckUidAttr(cb, 2)) {
        sceKernelCpuResumeIntr(intr);
        return (int)SCE_ERROR_KERNEL_ILLEGAL_PERMISSION;
    }

    TmanWaitQueue *wq = (TmanWaitQueue *)tmanUidData(cb, g_tman.uid_waitq);
    TmanEvfBlock *evf = (TmanEvfBlock *)tmanUidData(cb, g_tman.uid_evf);
    if (g_tman.log_handler != NULL)
        g_tman.log_handler(67, (u32)evf, 2, evid, bits);

    evf->cur_pattern |= bits;

    TmanListNode *head = &wq->wait_list;
    TmanListNode *node = head->next;
    TmanThreadCB *last_woken = NULL;
    int woken = 0;

    while (node != head) {
        TmanThreadCB *th = (TmanThreadCB *)node;
        node = node->next;
        u32 req_bits = th->wait_param[0];
        u32 wait_mode = th->wait_param[1];
        u32 *out_bits = (u32 *)th->wait_param[2];

        if (!tmanEvalEventFlag(evf->cur_pattern, req_bits, wait_mode))
            continue;

        if (out_bits != NULL)
            *out_bits = evf->cur_pattern;
        tmanApplyEventFlagClear(evf, req_bits, wait_mode);

        if (tmanWakeupWaitingThread(th) > 0) {
            last_woken = th;
            woken++;
        }
        if (evf->cur_pattern == 0)
            break;
    }

    if (in_intr != 0) {
        if (woken > 0)
            g_tman.next_thread = NULL;
    } else if (woken == 1 && last_woken != NULL) {
        tmanRemoveReadyQueue(last_woken, (u32)last_woken->current_priority);
        tmanMakeReadyAndPreempt(last_woken, intr);
    } else if (woken > 0) {
        g_tman.next_thread = NULL;
        tmanSchedule(0, intr);
    }

    sceKernelCpuResumeIntr(intr);
    return 0;
}

int sceKernelClearEventFlag(SceUID evid, u32 bits)
{
    int intr = sceKernelCpuSuspendIntr();
    SceSysmemUidCB *cb = NULL;
    if (sceKernelGetUIDcontrolBlockWithType(evid, g_tman.uid_evf, &cb) != 0) {
        sceKernelCpuResumeIntr(intr);
        return (int)SCE_ERROR_KERNEL_NOT_FOUND_EVENT_FLAG;
    }
    if (!tmanCheckUidAttr(cb, 2)) {
        sceKernelCpuResumeIntr(intr);
        return (int)SCE_ERROR_KERNEL_ILLEGAL_PERMISSION;
    }

    TmanEvfBlock *evf = (TmanEvfBlock *)tmanUidData(cb, g_tman.uid_evf);
    if (g_tman.log_handler != NULL)
        g_tman.log_handler(68, (u32)evf, 2, evid, bits);

    evf->cur_pattern &= bits;
    sceKernelCpuResumeIntr(intr);
    return 0;
}

static inline __attribute__((always_inline)) int tmanWaitEventFlagInternal(SceUID evid, u32 bits, u32 wait_mode, u32 *out_bits,
                                     SceUInt *timeout, int use_cb)
{
    if (!tmanCheckUserPtr(out_bits, sizeof(u32)) || !tmanCheckUserPtr(timeout, sizeof(SceUInt)))
        return (int)SCE_ERROR_KERNEL_ILLEGAL_ADDR;
    if (tmanReadIntrDepth() != 0)
        return (int)SCE_ERROR_KERNEL_CANNOT_BE_CALLED_FROM_INTERRUPT;
    if ((wait_mode & ~0x31U) != 0)
        return (int)SCE_ERROR_KERNEL_ILLEGAL_MODE;
    if (bits == 0)
        return (int)SCE_ERROR_KERNEL_EVENT_FLAG_ILLEGAL_WAIT_PATTERN;

    int intr = sceKernelCpuSuspendIntr();
    if (intr == 0 || g_tman.dispatch_suspended != 0) {
        sceKernelCpuResumeIntr(intr);
        return (int)SCE_ERROR_KERNEL_WAIT_CAN_NOT_WAIT;
    }

    tmanCheckKernelStackGuard();
    SceSysmemUidCB *cb = NULL;
    if (sceKernelGetUIDcontrolBlockWithType(evid, g_tman.uid_evf, &cb) != 0) {
        sceKernelCpuResumeIntr(intr);
        return (int)SCE_ERROR_KERNEL_NOT_FOUND_EVENT_FLAG;
    }
    if (!tmanCheckUidAttr(cb, 4)) {
        sceKernelCpuResumeIntr(intr);
        return (int)SCE_ERROR_KERNEL_ILLEGAL_PERMISSION;
    }

    TmanWaitQueue *wq = (TmanWaitQueue *)tmanUidData(cb, g_tman.uid_waitq);
    TmanEvfBlock *evf = (TmanEvfBlock *)tmanUidData(cb, g_tman.uid_evf);
    if ((wq->attr & SCE_KERNEL_EA_MULTI) == 0 && wq->num_wait_threads > 0) {
        sceKernelCpuResumeIntr(intr);
        return (int)SCE_ERROR_KERNEL_EVENT_FLAG_NO_MULTI_PERM;
    }

    if (g_tman.log_handler != NULL)
        g_tman.log_handler(use_cb ? 70 : 69, (u32)&g_tman.current_thread, 3, evid, bits, wait_mode);

    u64 abs_time = 0;
    if (timeout != NULL)
        tmanCalcAbsTimeoutUsec(&abs_time, *timeout);

    TmanThreadCB *cur = g_tman.current_thread;
    int ret;
    for (;;) {
        if (use_cb != 0 && cur->cb_notify_pending != 0)
            tmanCheckPendingCallbacks(intr);

        if (tmanEvalEventFlag(evf->cur_pattern, bits, wait_mode)) {
            if (out_bits != NULL)
                *out_bits = evf->cur_pattern;
            tmanApplyEventFlagClear(evf, bits, wait_mode);
            ret = 0;
            break;
        }

        cur->wait_param[0] = bits;
        cur->wait_param[1] = wait_mode;
        cur->wait_param[2] = (u32)out_bits;
        cur->cb_wait_enabled = use_cb;
        ret = tmanWaitOnObject(cb, TMAN_WAIT_EVENTFLAG, wq->attr, &abs_time, timeout);
        if (use_cb == 0 || ret != (int)SCE_ERROR_KERNEL_WAIT_STATUS_RELEASED_CALLBACK)
            break;
    }

    sceKernelCpuResumeIntr(intr);
    return ret;
}

int sceKernelWaitEventFlag(int evid, u32 bits, u32 wait, u32 *out_bits, SceUInt *timeout)
{
    return tmanWaitEventFlagInternal(evid, bits, wait, out_bits, timeout, 0);
}

int sceKernelWaitEventFlagCB(int evid, u32 bits, u32 wait, u32 *out_bits, SceUInt *timeout)
{
    return tmanWaitEventFlagInternal(evid, bits, wait, out_bits, timeout, 1);
}

int sceKernelPollEventFlag(int evid, u32 bits, u32 wait_mode, u32 *out_bits)
{
    if (!tmanCheckUserPtr(out_bits, sizeof(u32)))
        return (int)SCE_ERROR_KERNEL_ILLEGAL_ADDR;
    if ((wait_mode & ~0x31U) != 0)
        return (int)SCE_ERROR_KERNEL_ILLEGAL_MODE;
    if (bits == 0)
        return (int)SCE_ERROR_KERNEL_EVENT_FLAG_ILLEGAL_WAIT_PATTERN;

    int intr = sceKernelCpuSuspendIntr();
    SceSysmemUidCB *cb = NULL;
    if (sceKernelGetUIDcontrolBlockWithType(evid, g_tman.uid_evf, &cb) != 0) {
        sceKernelCpuResumeIntr(intr);
        return (int)SCE_ERROR_KERNEL_NOT_FOUND_EVENT_FLAG;
    }
    if (!tmanCheckUidAttr(cb, 4)) {
        sceKernelCpuResumeIntr(intr);
        return (int)SCE_ERROR_KERNEL_ILLEGAL_PERMISSION;
    }

    TmanWaitQueue *wq = (TmanWaitQueue *)tmanUidData(cb, g_tman.uid_waitq);
    TmanEvfBlock *evf = (TmanEvfBlock *)tmanUidData(cb, g_tman.uid_evf);
    if ((wq->attr & SCE_KERNEL_EA_MULTI) == 0 && wq->num_wait_threads > 0) {
        sceKernelCpuResumeIntr(intr);
        return (int)SCE_ERROR_KERNEL_EVENT_FLAG_NO_MULTI_PERM;
    }

    if (g_tman.log_handler != NULL)
        g_tman.log_handler(71, (u32)evf, 3, evid, bits, wait_mode);

    if (out_bits != NULL)
        *out_bits = evf->cur_pattern;

    if (!tmanEvalEventFlag(evf->cur_pattern, bits, wait_mode)) {
        sceKernelCpuResumeIntr(intr);
        return (int)SCE_ERROR_KERNEL_EVENT_FLAG_POLL_FAILED;
    }

    tmanApplyEventFlagClear(evf, bits, wait_mode);
    sceKernelCpuResumeIntr(intr);
    return 0;
}

int sceKernelCancelEventFlag(SceUID evid, SceUInt set_pattern, s32 *num_wait_threads)
{
    if (!tmanCheckUserPtr(num_wait_threads, sizeof(s32)))
        return (int)SCE_ERROR_KERNEL_ILLEGAL_ADDR;

    int intr = sceKernelCpuSuspendIntr();
    SceSysmemUidCB *cb = NULL;
    if (sceKernelGetUIDcontrolBlockWithType(evid, g_tman.uid_evf, &cb) != 0) {
        sceKernelCpuResumeIntr(intr);
        return (int)SCE_ERROR_KERNEL_NOT_FOUND_EVENT_FLAG;
    }
    if (!tmanCheckUidAttr(cb, 8)) {
        sceKernelCpuResumeIntr(intr);
        return (int)SCE_ERROR_KERNEL_ILLEGAL_PERMISSION;
    }

    TmanEvfBlock *evf = (TmanEvfBlock *)tmanUidData(cb, g_tman.uid_evf);
    if (g_tman.log_handler != NULL)
        g_tman.log_handler(72, (u32)evf, 2, evid, set_pattern);

    evf->cur_pattern = set_pattern;
    int woken = sceKernelCallUIDObjFunction(cb, (s32)TMAN_UID_FUNC_CANCEL_WAIT);
    if (num_wait_threads != NULL)
        *num_wait_threads = woken;

    sceKernelCpuResumeIntr(intr);
    return 0;
}

int sceKernelReferEventFlagStatus(SceUID evid, SceKernelEventFlagInfo *info)
{
    if (!tmanCheckUserPtr(info, sizeof(*info)))
        return (int)SCE_ERROR_KERNEL_ILLEGAL_ADDR;

    int intr = sceKernelCpuSuspendIntr();
    SceSysmemUidCB *cb = NULL;
    if (sceKernelGetUIDcontrolBlockWithType(evid, g_tman.uid_evf, &cb) != 0) {
        sceKernelCpuResumeIntr(intr);
        return (int)SCE_ERROR_KERNEL_NOT_FOUND_EVENT_FLAG;
    }
    if (!tmanCheckUidAttr(cb, 1)) {
        sceKernelCpuResumeIntr(intr);
        return (int)SCE_ERROR_KERNEL_ILLEGAL_PERMISSION;
    }

    if (g_tman.log_handler != NULL)
        g_tman.log_handler(73, (u32)&g_tman.current_thread, 2, evid, info);

    TmanWaitQueue *wq = (TmanWaitQueue *)tmanUidData(cb, g_tman.uid_waitq);
    TmanEvfBlock *evf = (TmanEvfBlock *)tmanUidData(cb, g_tman.uid_evf);
    SceKernelEventFlagInfo local;
    memset(&local, 0, sizeof(local));
    local.size = sizeof(local);
    if (cb->name != NULL)
        strncpy(local.name, cb->name, SCE_UID_NAME_LEN);
    local.attr = wq->attr;
    local.initPattern = evf->init_pattern;
    local.currentPattern = evf->cur_pattern;
    local.numWaitThreads = wq->num_wait_threads;

    SceSize copy_size = info->size < local.size ? info->size : local.size;
    memcpy(info, &local, copy_size);
    sceKernelCpuResumeIntr(intr);
    return 0;
}

