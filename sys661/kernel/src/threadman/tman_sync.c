#include "threadman_internal.h"

/* UID callbacks for sync primitives */
static int semaUidCreate(SceSysmemUidCB *cb, SceSysmemUidCB *type, int func_id, void *args);
static int semaUidDelete(SceSysmemUidCB *cb, SceSysmemUidCB *type, int func_id, void *args);
static int semaUidCheck(SceSysmemUidCB *cb, SceSysmemUidCB *type, int func_id, void *args);
static int evfUidCreate(SceSysmemUidCB *cb, SceSysmemUidCB *type, int func_id, void *args);
static int evfUidDelete(SceSysmemUidCB *cb, SceSysmemUidCB *type, int func_id, void *args);
static int evfUidRemove(SceSysmemUidCB *cb, SceSysmemUidCB *type, int func_id, void *args);
static int mutexUidCreate(SceSysmemUidCB *cb, SceSysmemUidCB *type, int func_id, void *args);
static int mutexUidDelete(SceSysmemUidCB *cb, SceSysmemUidCB *type, int func_id, void *args);
static int mutexUidCheck(SceSysmemUidCB *cb, SceSysmemUidCB *type, int func_id, void *args);
static int lwmutexUidCreate(SceSysmemUidCB *cb, SceSysmemUidCB *type, int func_id, void *args);
static int lwmutexUidDelete(SceSysmemUidCB *cb, SceSysmemUidCB *type, int func_id, void *args);
static int lwmutexUidRemove(SceSysmemUidCB *cb, SceSysmemUidCB *type, int func_id, void *args);
static int lwmutexUidCheck(SceSysmemUidCB *cb, SceSysmemUidCB *type, int func_id, void *args);
static int mbxUidCreate(SceSysmemUidCB *cb, SceSysmemUidCB *type, int func_id, void *args);
static int mbxUidDelete(SceSysmemUidCB *cb, SceSysmemUidCB *type, int func_id, void *args);

const TmanUidFunc g_uid_sema_funcs[4] = {
    { TMAN_UID_FUNC_CHECK_WAIT, (void *)semaUidCheck },
    { TMAN_UID_FUNC_CREATE,     (void *)semaUidCreate },
    { TMAN_UID_FUNC_DELETE,     (void *)semaUidDelete },
    { 0, NULL },
};

const TmanUidFunc g_uid_evf_funcs[4] = {
    { TMAN_UID_FUNC_REMOVE_WAIT, (void *)evfUidRemove },
    { TMAN_UID_FUNC_CREATE,      (void *)evfUidCreate },
    { TMAN_UID_FUNC_DELETE,      (void *)evfUidDelete },
    { 0, NULL },
};

const TmanUidFunc g_uid_mutex_funcs[4] = {
    { TMAN_UID_FUNC_CHECK_WAIT, (void *)mutexUidCheck },
    { TMAN_UID_FUNC_CREATE,     (void *)mutexUidCreate },
    { TMAN_UID_FUNC_DELETE,     (void *)mutexUidDelete },
    { 0, NULL },
};

const TmanUidFunc g_uid_lwmutex_funcs[5] = {
    { TMAN_UID_FUNC_REMOVE_WAIT, (void *)lwmutexUidRemove },
    { TMAN_UID_FUNC_CHECK_WAIT,  (void *)lwmutexUidCheck },
    { TMAN_UID_FUNC_CREATE,      (void *)lwmutexUidCreate },
    { TMAN_UID_FUNC_DELETE,      (void *)lwmutexUidDelete },
    { 0, NULL },
};

const TmanUidFunc g_uid_mbx_funcs[3] = {
    { TMAN_UID_FUNC_CREATE, (void *)mbxUidCreate },
    { TMAN_UID_FUNC_DELETE, (void *)mbxUidDelete },
    { 0, NULL },
};

int tmanWaitQueueHasWaiters(const TmanWaitQueue *wq)
{
    if (tmanListEmpty(&wq->wait_list))
        return 0;
    if ((wq->attr & SCE_KERNEL_AT_THPRI) == 0)
        return 1;
    TmanThreadCB *first = (TmanThreadCB *)wq->wait_list.next;
    return first->current_priority <= g_tman.current_thread->current_priority;
}

/* ============================================================================
 * Semaphores
 * ============================================================================ */

void tmanInitSemaType(void)
{
    sceKernelCreateUIDtypeInherit("WaitQ", "Semaphore", sizeof(TmanSemaCB),
                                  (SceSysmemUidLookupFunc *)g_uid_sema_funcs,
                                  NULL, &g_tman.uid_sema);
}

static int semaUidCreate(SceSysmemUidCB *cb, SceSysmemUidCB *type, int func_id, void *args)
{
    sceKernelCallUIDObjCommonFunction(cb, type, func_id, args);
    return cb->uid ^ (s32)g_tman.uid_xor_key;
}

static int semaUidDelete(SceSysmemUidCB *cb, SceSysmemUidCB *type, int func_id, void *args)
{
    sceKernelCallUIDObjCommonFunction(cb, type, func_id, args);
    return 0;
}

static int semaUidCheck(SceSysmemUidCB *cb, SceSysmemUidCB *type, int func_id, void *args)
{
    (void)type;
    (void)func_id;
    (void)args;
    TmanWaitQueue *wq = (TmanWaitQueue *)tmanUidData(cb, g_tman.uid_waitq);
    TmanSemaCB *sema = (TmanSemaCB *)tmanUidData(cb, g_tman.uid_sema);
    TmanListNode *head = &wq->wait_list;
    TmanListNode *node = head->next;
    int woken = 0;

    while (node != head) {
        TmanThreadCB *th = (TmanThreadCB *)node;
        node = node->next;
        if (g_tman.user_threads_suspended != 0 &&
            ((s32)th->attr < 0 || (th->attr & 0x08000000U) != 0))
            continue;
        s32 need = (s32)th->wait_param[0];
        if (sema->cur_count < need)
            break;
        sema->cur_count -= need;
        woken += tmanWakeupWaitingThread(th);
    }
    return woken;
}

SceUID sceKernelCreateSema(const char *name, SceUInt attr, int init_val, int max_val,
                           SceKernelSemaOptParam *opt)
{
    if (!tmanCheckUserAddr((u32)name) || !tmanCheckUserPtr(opt, sizeof(*opt)))
        return (SceUID)SCE_ERROR_KERNEL_ILLEGAL_ADDR;
    if (tmanReadIntrDepth() != 0)
        return (SceUID)SCE_ERROR_KERNEL_CANNOT_BE_CALLED_FROM_INTERRUPT;
    if ((attr & ~0x1FFU) != 0)
        return (SceUID)SCE_ERROR_KERNEL_ILLEGAL_ATTR;

    int is_user = tmanIsUserCaller();
    int intr = sceKernelCpuSuspendIntr();
    SceSysmemUidCB *cb = NULL;
    int ret = sceKernelCreateUID(g_tman.uid_sema, name,
                                 (char)((attr & 0xFFU) | (is_user ? 0xFFU : 0x00U)), &cb);
    if (ret != 0) {
        sceKernelCpuResumeIntr(intr);
        return ret;
    }

    TmanWaitQueue *wq = (TmanWaitQueue *)tmanUidData(cb, g_tman.uid_waitq);
    TmanSemaCB *sema = (TmanSemaCB *)tmanUidData(cb, g_tman.uid_sema);
    SceUID uid = cb->uid ^ (s32)g_tman.uid_xor_key;
    wq->attr = attr;
    sema->opt = opt;
    sema->cur_count = init_val;
    sema->max_count = max_val;
    sema->init_count = init_val;

    if (g_tman.log_handler != NULL)
        g_tman.log_handler(57, 0, 1, uid);

    sceKernelCpuResumeIntr(intr);
    return uid;
}

int sceKernelDeleteSema(SceUID semaid)
{
    if (tmanReadIntrDepth() != 0)
        return (int)SCE_ERROR_KERNEL_CANNOT_BE_CALLED_FROM_INTERRUPT;

    int intr = sceKernelCpuSuspendIntr();
    SceSysmemUidCB *cb = NULL;
    if (sceKernelGetUIDcontrolBlockWithType(semaid, g_tman.uid_sema, &cb) != 0) {
        sceKernelCpuResumeIntr(intr);
        return (int)SCE_ERROR_KERNEL_NOT_FOUND_SEMAPHORE;
    }
    if (!tmanCheckUidAttr(cb, 0x10)) {
        sceKernelCpuResumeIntr(intr);
        return (int)SCE_ERROR_KERNEL_ILLEGAL_PERMISSION;
    }

    if (g_tman.log_handler != NULL)
        g_tman.log_handler(58, (u32)cb, 1, semaid);

    sceKernelDeleteUID(semaid);
    sceKernelCpuResumeIntr(intr);
    return 0;
}

int sceKernelSignalSema(SceUID semaid, int signal)
{
    int intr = sceKernelCpuSuspendIntr();
    int in_intr = (int)tmanReadIntrDepth();
    SceSysmemUidCB *cb = NULL;

    if (sceKernelGetUIDcontrolBlockWithType(semaid, g_tman.uid_sema, &cb) != 0) {
        sceKernelCpuResumeIntr(intr);
        return (int)SCE_ERROR_KERNEL_NOT_FOUND_SEMAPHORE;
    }
    if (!tmanCheckUidAttr(cb, 2)) {
        sceKernelCpuResumeIntr(intr);
        return (int)SCE_ERROR_KERNEL_ILLEGAL_PERMISSION;
    }

    TmanWaitQueue *wq = (TmanWaitQueue *)tmanUidData(cb, g_tman.uid_waitq);
    TmanSemaCB *sema = (TmanSemaCB *)tmanUidData(cb, g_tman.uid_sema);
    if (g_tman.log_handler != NULL)
        g_tman.log_handler(59, (u32)sema, 2, semaid, signal);

    s32 projected = sema->cur_count + signal;
    TmanListNode *head = &wq->wait_list;
    TmanListNode *node = head->next;
    while (node != head) {
        TmanThreadCB *th = (TmanThreadCB *)node;
        projected -= (s32)th->wait_param[0];
        node = node->next;
    }

    if (projected > sema->max_count) {
        sceKernelCpuResumeIntr(intr);
        return (int)SCE_ERROR_KERNEL_SEMA_OVERFLOW;
    }

    sema->cur_count += signal;
    TmanThreadCB *last_woken = NULL;
    int woken = 0;
    node = head->next;
    while (node != head) {
        TmanThreadCB *th = (TmanThreadCB *)node;
        node = node->next;
        if (g_tman.user_threads_suspended != 0 &&
            ((s32)th->attr < 0 || (th->attr & 0x08000000U) != 0))
            continue;
        s32 need = (s32)th->wait_param[0];
        if (sema->cur_count < need)
            break;
        sema->cur_count -= need;
        if (tmanWakeupWaitingThread(th) > 0) {
            last_woken = th;
            woken++;
        }
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

static int tmanWaitSemaInternal(SceUID semaid, int signal, SceUInt *timeout, int use_cb)
{
    if (!tmanCheckUserPtr(timeout, sizeof(SceUInt)))
        return (int)SCE_ERROR_KERNEL_ILLEGAL_ADDR;
    if (tmanReadIntrDepth() != 0)
        return (int)SCE_ERROR_KERNEL_CANNOT_BE_CALLED_FROM_INTERRUPT;
    if (signal <= 0)
        return (int)SCE_ERROR_KERNEL_ILLEGAL_COUNT;

    int intr = sceKernelCpuSuspendIntr();
    if (intr == 0 || g_tman.dispatch_suspended != 0) {
        sceKernelCpuResumeIntr(intr);
        return (int)SCE_ERROR_KERNEL_WAIT_CAN_NOT_WAIT;
    }

    tmanCheckKernelStackGuard();
    SceSysmemUidCB *cb = NULL;
    if (sceKernelGetUIDcontrolBlockWithType(semaid, g_tman.uid_sema, &cb) != 0) {
        sceKernelCpuResumeIntr(intr);
        return (int)SCE_ERROR_KERNEL_NOT_FOUND_SEMAPHORE;
    }
    if (!tmanCheckUidAttr(cb, 4)) {
        sceKernelCpuResumeIntr(intr);
        return (int)SCE_ERROR_KERNEL_ILLEGAL_PERMISSION;
    }

    TmanWaitQueue *wq = (TmanWaitQueue *)tmanUidData(cb, g_tman.uid_waitq);
    TmanSemaCB *sema = (TmanSemaCB *)tmanUidData(cb, g_tman.uid_sema);
    if (signal > sema->max_count) {
        sceKernelCpuResumeIntr(intr);
        return (int)SCE_ERROR_KERNEL_ILLEGAL_COUNT;
    }

    if (g_tman.log_handler != NULL)
        g_tman.log_handler(use_cb ? 61 : 60, (u32)&g_tman.current_thread, 2, semaid, signal);

    u64 abs_time = 0;
    if (timeout != NULL)
        tmanCalcAbsTimeoutUsec(&abs_time, *timeout);

    TmanThreadCB *cur = g_tman.current_thread;
    int ret;
    for (;;) {
        if (use_cb != 0 && cur->cb_notify_pending != 0)
            tmanCheckPendingCallbacks(intr);

        if (!tmanWaitQueueHasWaiters(wq) && sema->cur_count >= signal) {
            sema->cur_count -= signal;
            ret = 0;
            break;
        }

        cur->wait_param[0] = (u32)signal;
        cur->cb_wait_enabled = use_cb;
        ret = tmanWaitOnObject(cb, TMAN_WAIT_SEMA, wq->attr, &abs_time, timeout);
        if (use_cb == 0 || ret != (int)SCE_ERROR_KERNEL_WAIT_STATUS_RELEASED_CALLBACK)
            break;
    }

    sceKernelCpuResumeIntr(intr);
    return ret;
}

int sceKernelWaitSema(SceUID semaid, int signal, SceUInt *timeout)
{
    return tmanWaitSemaInternal(semaid, signal, timeout, 0);
}

int sceKernelWaitSemaCB(SceUID semaid, int signal, SceUInt *timeout)
{
    return tmanWaitSemaInternal(semaid, signal, timeout, 1);
}

int sceKernelPollSema(SceUID semaid, int signal)
{
    if (signal <= 0)
        return (int)SCE_ERROR_KERNEL_ILLEGAL_COUNT;

    int intr = sceKernelCpuSuspendIntr();
    SceSysmemUidCB *cb = NULL;
    if (sceKernelGetUIDcontrolBlockWithType(semaid, g_tman.uid_sema, &cb) != 0) {
        sceKernelCpuResumeIntr(intr);
        return (int)SCE_ERROR_KERNEL_NOT_FOUND_SEMAPHORE;
    }
    if (!tmanCheckUidAttr(cb, 4)) {
        sceKernelCpuResumeIntr(intr);
        return (int)SCE_ERROR_KERNEL_ILLEGAL_PERMISSION;
    }

    TmanWaitQueue *wq = (TmanWaitQueue *)tmanUidData(cb, g_tman.uid_waitq);
    TmanSemaCB *sema = (TmanSemaCB *)tmanUidData(cb, g_tman.uid_sema);
    if (signal > sema->max_count) {
        sceKernelCpuResumeIntr(intr);
        return (int)SCE_ERROR_KERNEL_ILLEGAL_COUNT;
    }

    if (g_tman.log_handler != NULL)
        g_tman.log_handler(62, (u32)sema, 2, semaid, signal);

    if (tmanWaitQueueHasWaiters(wq) || sema->cur_count < signal) {
        sceKernelCpuResumeIntr(intr);
        return (int)SCE_ERROR_KERNEL_SEMA_ZERO;
    }

    sema->cur_count -= signal;
    sceKernelCpuResumeIntr(intr);
    return 0;
}

int sceKernelCancelSema(SceUID semaid, int set_count, int *num_wait_threads)
{
    if (!tmanCheckUserPtr(num_wait_threads, sizeof(int)))
        return (int)SCE_ERROR_KERNEL_ILLEGAL_ADDR;

    int intr = sceKernelCpuSuspendIntr();
    SceSysmemUidCB *cb = NULL;
    if (sceKernelGetUIDcontrolBlockWithType(semaid, g_tman.uid_sema, &cb) != 0) {
        sceKernelCpuResumeIntr(intr);
        return (int)SCE_ERROR_KERNEL_NOT_FOUND_SEMAPHORE;
    }
    if (!tmanCheckUidAttr(cb, 8)) {
        sceKernelCpuResumeIntr(intr);
        return (int)SCE_ERROR_KERNEL_ILLEGAL_PERMISSION;
    }

    TmanSemaCB *sema = (TmanSemaCB *)tmanUidData(cb, g_tman.uid_sema);
    if (set_count > sema->max_count) {
        sceKernelCpuResumeIntr(intr);
        return (int)SCE_ERROR_KERNEL_ILLEGAL_COUNT;
    }

    if (g_tman.log_handler != NULL)
        g_tman.log_handler(63, (u32)sema, 2, semaid, set_count);

    sema->cur_count = (set_count < 0) ? sema->init_count : set_count;
    int woken = sceKernelCallUIDObjFunction(cb, (s32)TMAN_UID_FUNC_CANCEL_WAIT);
    if (num_wait_threads != NULL)
        *num_wait_threads = woken;

    sceKernelCpuResumeIntr(intr);
    return 0;
}

int sceKernelReferSemaStatus(SceUID semaid, SceKernelSemaInfo *info)
{
    if (!tmanCheckUserPtr(info, sizeof(*info)))
        return (int)SCE_ERROR_KERNEL_ILLEGAL_ADDR;

    int intr = sceKernelCpuSuspendIntr();
    SceSysmemUidCB *cb = NULL;
    if (sceKernelGetUIDcontrolBlockWithType(semaid, g_tman.uid_sema, &cb) != 0) {
        sceKernelCpuResumeIntr(intr);
        return (int)SCE_ERROR_KERNEL_NOT_FOUND_SEMAPHORE;
    }
    if (!tmanCheckUidAttr(cb, 1)) {
        sceKernelCpuResumeIntr(intr);
        return (int)SCE_ERROR_KERNEL_ILLEGAL_PERMISSION;
    }

    if (g_tman.log_handler != NULL)
        g_tman.log_handler(64, (u32)&g_tman.current_thread, 2, semaid, info);

    TmanWaitQueue *wq = (TmanWaitQueue *)tmanUidData(cb, g_tman.uid_waitq);
    TmanSemaCB *sema = (TmanSemaCB *)tmanUidData(cb, g_tman.uid_sema);
    SceKernelSemaInfo local;
    memset(&local, 0, sizeof(local));
    local.size = sizeof(local);
    if (cb->name != NULL)
        strncpy(local.name, cb->name, SCE_UID_NAME_LEN);
    local.attr = wq->attr;
    local.initCount = sema->init_count;
    local.currentCount = sema->cur_count;
    local.maxCount = sema->max_count;
    local.numWaitThreads = wq->num_wait_threads;

    SceSize copy_size = info->size < local.size ? info->size : local.size;
    memcpy(info, &local, copy_size);
    sceKernelCpuResumeIntr(intr);
    return 0;
}

/* ============================================================================
 * Event Flags
 * ============================================================================ */

void tmanInitEventFlagType(void)
{
    sceKernelCreateUIDtypeInherit("WaitQ", "EventFlag", sizeof(TmanEventFlagCB),
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
    sceKernelCallUIDObjCommonFunction(cb, type, func_id, args);
    return 0;
}

static int evfUidRemove(SceSysmemUidCB *cb, SceSysmemUidCB *type, int func_id, void *args)
{
    sceKernelCallUIDObjCommonFunction(cb, type, func_id, args);
    return 0;
}

static int tmanEvalEventFlag(u32 cur_pattern, u32 req_bits, u32 wait_mode)
{
    if ((wait_mode & SCE_KERNEL_EW_OR) != 0)
        return (cur_pattern & req_bits) != 0;
    return (cur_pattern & req_bits) == req_bits;
}

static void tmanApplyEventFlagClear(TmanEventFlagCB *evf, u32 req_bits, u32 wait_mode)
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
    TmanEventFlagCB *evf = (TmanEventFlagCB *)tmanUidData(cb, g_tman.uid_evf);
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
    TmanEventFlagCB *evf = (TmanEventFlagCB *)tmanUidData(cb, g_tman.uid_evf);
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

    TmanEventFlagCB *evf = (TmanEventFlagCB *)tmanUidData(cb, g_tman.uid_evf);
    if (g_tman.log_handler != NULL)
        g_tman.log_handler(68, (u32)evf, 2, evid, bits);

    evf->cur_pattern &= bits;
    sceKernelCpuResumeIntr(intr);
    return 0;
}

static int tmanWaitEventFlagInternal(SceUID evid, u32 bits, u32 wait_mode, u32 *out_bits,
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
    TmanEventFlagCB *evf = (TmanEventFlagCB *)tmanUidData(cb, g_tman.uid_evf);
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
    TmanEventFlagCB *evf = (TmanEventFlagCB *)tmanUidData(cb, g_tman.uid_evf);
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

    TmanEventFlagCB *evf = (TmanEventFlagCB *)tmanUidData(cb, g_tman.uid_evf);
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
    TmanEventFlagCB *evf = (TmanEventFlagCB *)tmanUidData(cb, g_tman.uid_evf);
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

/* ============================================================================
 * Mutexes
 * ============================================================================ */

void tmanInitMutexType(void)
{
    sceKernelCreateUIDtypeInherit("WaitQ", "Mutex", sizeof(TmanMutexCB),
                                  (SceSysmemUidLookupFunc *)g_uid_mutex_funcs,
                                  NULL, &g_tman.uid_mutex);
}

static int mutexUidCreate(SceSysmemUidCB *cb, SceSysmemUidCB *type, int func_id, void *args)
{
    TmanMutexCB *mtx = (TmanMutexCB *)tmanUidData(cb, type);
    sceKernelCallUIDObjCommonFunction(cb, type, func_id, args);
    tmanListInit(&mtx->owner_link);
    return cb->uid ^ (s32)g_tman.uid_xor_key;
}

static int mutexUidDelete(SceSysmemUidCB *cb, SceSysmemUidCB *type, int func_id, void *args)
{
    TmanMutexCB *mtx = (TmanMutexCB *)tmanUidData(cb, type);
    if (mtx->owner != NULL) {
        tmanListRemove(&mtx->owner_link);
        mtx->owner = NULL;
    }
    sceKernelCallUIDObjCommonFunction(cb, type, func_id, args);
    return 0;
}

static int mutexUidCheck(SceSysmemUidCB *cb, SceSysmemUidCB *type, int func_id, void *args)
{
    (void)type;
    (void)func_id;
    (void)args;
    TmanWaitQueue *wq = (TmanWaitQueue *)tmanUidData(cb, g_tman.uid_waitq);
    TmanMutexCB *mtx = (TmanMutexCB *)tmanUidData(cb, g_tman.uid_mutex);
    if (mtx->owner != NULL || tmanListEmpty(&wq->wait_list))
        return 0;

    TmanThreadCB *next_owner = (TmanThreadCB *)wq->wait_list.next;
    mtx->owner = next_owner;
    mtx->lock_count = (s32)next_owner->wait_param[0];
    tmanListInsertTail(&next_owner->owned_mutex_list, &mtx->owner_link);
    return tmanWakeupWaitingThread(next_owner);
}

void tmanRecalcThreadPriority(TmanThreadCB *th)
{
    s32 best = th->base_priority;
    if (th->inherited_priority < best)
        best = th->inherited_priority;
    if (best != th->current_priority) {
        if (th->status == TMAN_TH_READY) {
            tmanRemoveReadyQueue(th, (u32)th->current_priority);
            th->current_priority = best;
            tmanAddReadyQueue(th);
        } else {
            th->current_priority = best;
        }
    }
}

void tmanReleaseOwnedMutexes(TmanThreadCB *th)
{
    TmanListNode *head = &th->owned_mutex_list;
    while (!tmanListEmpty(head)) {
        TmanMutexCB *mtx = (TmanMutexCB *)head->next;
        SceSysmemUidCB *cb = tmanDataToUid(mtx, g_tman.uid_mutex);
        tmanListRemove(&mtx->owner_link);
        mtx->owner = NULL;
        mtx->lock_count = 0;
        if (mutexUidCheck(cb, g_tman.uid_mutex, 0, NULL) > 0)
            g_tman.next_thread = NULL;
    }
}

s32 sceKernelCreateMutex(char *name, s32 attr, s32 init_count, const SceKernelMutexOptParam *opt)
{
    if (!tmanCheckUserAddr((u32)name) || !tmanCheckUserPtr(opt, sizeof(*opt)))
        return (s32)SCE_ERROR_KERNEL_ILLEGAL_ADDR;
    if (tmanReadIntrDepth() != 0)
        return (s32)SCE_ERROR_KERNEL_CANNOT_BE_CALLED_FROM_INTERRUPT;
    if ((attr & ~0x3FF) != 0)
        return (s32)SCE_ERROR_KERNEL_ILLEGAL_ATTR;
    if (init_count < 0)
        return (s32)SCE_ERROR_KERNEL_ILLEGAL_COUNT;
    if (init_count > 1 && (attr & SCE_KERNEL_MUTEX_ATTR_RECURSIVE) == 0)
        return (s32)SCE_ERROR_KERNEL_MUTEX_RECURSIVE_NOT_ALLOWED;

    int is_user = tmanIsUserCaller();
    int intr = sceKernelCpuSuspendIntr();
    SceSysmemUidCB *cb = NULL;
    int ret = sceKernelCreateUID(g_tman.uid_mutex, name,
                                 (char)((attr & 0xFF) | (is_user ? 0xFF : 0x00)), &cb);
    if (ret != 0) {
        sceKernelCpuResumeIntr(intr);
        return ret;
    }

    TmanWaitQueue *wq = (TmanWaitQueue *)tmanUidData(cb, g_tman.uid_waitq);
    TmanMutexCB *mtx = (TmanMutexCB *)tmanUidData(cb, g_tman.uid_mutex);
    SceUID uid = cb->uid ^ (s32)g_tman.uid_xor_key;
    wq->attr = (u32)attr;
    mtx->uid = uid;
    mtx->init_count = init_count;
    mtx->lock_count = init_count;
    mtx->opt = (void *)opt;
    if (init_count > 0) {
        mtx->owner = g_tman.current_thread;
        tmanListInsertTail(&g_tman.current_thread->owned_mutex_list, &mtx->owner_link);
    } else {
        mtx->owner = NULL;
    }

    if (g_tman.log_handler != NULL)
        g_tman.log_handler(171, (u32)mtx, 3, attr, init_count, uid);

    sceKernelCpuResumeIntr(intr);
    return uid;
}

s32 sceKernelDeleteMutex(SceUID mutex_id)
{
    if (tmanReadIntrDepth() != 0)
        return (s32)SCE_ERROR_KERNEL_CANNOT_BE_CALLED_FROM_INTERRUPT;

    int intr = sceKernelCpuSuspendIntr();
    SceSysmemUidCB *cb = NULL;
    if (sceKernelGetUIDcontrolBlockWithType(mutex_id, g_tman.uid_mutex, &cb) != 0) {
        sceKernelCpuResumeIntr(intr);
        return (s32)SCE_ERROR_KERNEL_MUTEX_NOT_FOUND;
    }
    if (!tmanCheckUidAttr(cb, 0x10)) {
        sceKernelCpuResumeIntr(intr);
        return (s32)SCE_ERROR_KERNEL_ILLEGAL_PERMISSION;
    }

    if (g_tman.log_handler != NULL)
        g_tman.log_handler(172, (u32)cb, 1, mutex_id);

    sceKernelDeleteUID(mutex_id);
    sceKernelCpuResumeIntr(intr);
    return 0;
}

s32 sceKernelUnlockMutex(SceUID mutex_id, s32 unlock_count)
{
    if (tmanReadIntrDepth() != 0)
        return (s32)SCE_ERROR_KERNEL_CANNOT_BE_CALLED_FROM_INTERRUPT;
    if (unlock_count <= 0)
        return (s32)SCE_ERROR_KERNEL_ILLEGAL_COUNT;

    int intr = sceKernelCpuSuspendIntr();
    SceSysmemUidCB *cb = NULL;
    if (sceKernelGetUIDcontrolBlockWithType(mutex_id, g_tman.uid_mutex, &cb) != 0) {
        sceKernelCpuResumeIntr(intr);
        return (s32)SCE_ERROR_KERNEL_MUTEX_NOT_FOUND;
    }
    if (!tmanCheckUidAttr(cb, 2)) {
        sceKernelCpuResumeIntr(intr);
        return (s32)SCE_ERROR_KERNEL_ILLEGAL_PERMISSION;
    }

    TmanWaitQueue *wq = (TmanWaitQueue *)tmanUidData(cb, g_tman.uid_waitq);
    TmanMutexCB *mtx = (TmanMutexCB *)tmanUidData(cb, g_tman.uid_mutex);
    if (mtx->owner != g_tman.current_thread) {
        sceKernelCpuResumeIntr(intr);
        return (s32)SCE_ERROR_KERNEL_MUTEX_UNLOCKED;
    }
    if (mtx->lock_count < unlock_count) {
        sceKernelCpuResumeIntr(intr);
        return (s32)SCE_ERROR_KERNEL_MUTEX_UNLOCK_UNDERFLOW;
    }

    if (g_tman.log_handler != NULL)
        g_tman.log_handler(176, (u32)mtx, 2, mutex_id, unlock_count);

    mtx->lock_count -= unlock_count;
    if (mtx->lock_count == 0) {
        tmanListRemove(&mtx->owner_link);
        mtx->owner = NULL;
        if (!tmanListEmpty(&wq->wait_list)) {
            TmanThreadCB *next_owner = (TmanThreadCB *)wq->wait_list.next;
            mtx->owner = next_owner;
            mtx->lock_count = (s32)next_owner->wait_param[0];
            tmanListInsertTail(&next_owner->owned_mutex_list, &mtx->owner_link);
            tmanWakeupAndReschedule(next_owner, 0, intr);
        }
    }

    sceKernelCpuResumeIntr(intr);
    return 0;
}

static s32 tmanLockMutexInternal(SceUID mutex_id, s32 lock_count, u32 *timeout, int use_cb)
{
    if (!tmanCheckUserPtr(timeout, sizeof(u32)))
        return (s32)SCE_ERROR_KERNEL_ILLEGAL_ADDR;
    if (tmanReadIntrDepth() != 0)
        return (s32)SCE_ERROR_KERNEL_CANNOT_BE_CALLED_FROM_INTERRUPT;
    if (lock_count <= 0)
        return (s32)SCE_ERROR_KERNEL_ILLEGAL_COUNT;

    int intr = sceKernelCpuSuspendIntr();
    if (intr == 0 || g_tman.dispatch_suspended != 0) {
        sceKernelCpuResumeIntr(intr);
        return (s32)SCE_ERROR_KERNEL_WAIT_CAN_NOT_WAIT;
    }

    tmanCheckKernelStackGuard();
    SceSysmemUidCB *cb = NULL;
    if (sceKernelGetUIDcontrolBlockWithType(mutex_id, g_tman.uid_mutex, &cb) != 0) {
        sceKernelCpuResumeIntr(intr);
        return (s32)SCE_ERROR_KERNEL_MUTEX_NOT_FOUND;
    }
    if (!tmanCheckUidAttr(cb, 4)) {
        sceKernelCpuResumeIntr(intr);
        return (s32)SCE_ERROR_KERNEL_ILLEGAL_PERMISSION;
    }

    TmanWaitQueue *wq = (TmanWaitQueue *)tmanUidData(cb, g_tman.uid_waitq);
    TmanMutexCB *mtx = (TmanMutexCB *)tmanUidData(cb, g_tman.uid_mutex);
    if (lock_count > 1 && (wq->attr & SCE_KERNEL_MUTEX_ATTR_RECURSIVE) == 0) {
        sceKernelCpuResumeIntr(intr);
        return (s32)SCE_ERROR_KERNEL_MUTEX_RECURSIVE_NOT_ALLOWED;
    }

    if (g_tman.log_handler != NULL)
        g_tman.log_handler(use_cb ? 174 : 173, (u32)mtx, 2, mutex_id, lock_count);

    u64 abs_time = 0;
    if (timeout != NULL)
        tmanCalcAbsTimeoutUsec(&abs_time, *timeout);

    TmanThreadCB *cur = g_tman.current_thread;
    s32 ret;
    for (;;) {
        if (use_cb != 0 && cur->cb_notify_pending != 0)
            tmanCheckPendingCallbacks(intr);

        if (mtx->owner == NULL) {
            mtx->owner = cur;
            mtx->lock_count = lock_count;
            tmanListInsertTail(&cur->owned_mutex_list, &mtx->owner_link);
            ret = 0;
            break;
        }
        if (mtx->owner == cur) {
            if ((wq->attr & SCE_KERNEL_MUTEX_ATTR_RECURSIVE) == 0) {
                ret = (s32)SCE_ERROR_KERNEL_MUTEX_RECURSIVE_NOT_ALLOWED;
                break;
            }
            if (mtx->lock_count + lock_count < mtx->lock_count) {
                ret = (s32)SCE_ERROR_KERNEL_MUTEX_LOCK_OVERFLOW;
                break;
            }
            mtx->lock_count += lock_count;
            ret = 0;
            break;
        }

        cur->wait_param[0] = (u32)lock_count;
        cur->cb_wait_enabled = use_cb;
        ret = tmanWaitOnObject(cb, TMAN_WAIT_MUTEX, wq->attr, &abs_time, timeout);
        if (use_cb == 0 || ret != (s32)SCE_ERROR_KERNEL_WAIT_STATUS_RELEASED_CALLBACK)
            break;
    }

    sceKernelCpuResumeIntr(intr);
    return ret;
}

s32 sceKernelLockMutex(SceUID mutex_id, s32 lock_count, u32 *timeout)
{
    return tmanLockMutexInternal(mutex_id, lock_count, timeout, 0);
}

s32 sceKernelLockMutexCB(SceUID mutex_id, s32 lock_count, u32 *timeout)
{
    return tmanLockMutexInternal(mutex_id, lock_count, timeout, 1);
}

s32 sceKernelTryLockMutex(SceUID mutex_id, s32 lock_count)
{
    if (tmanReadIntrDepth() != 0)
        return (s32)SCE_ERROR_KERNEL_CANNOT_BE_CALLED_FROM_INTERRUPT;
    if (lock_count <= 0)
        return (s32)SCE_ERROR_KERNEL_ILLEGAL_COUNT;

    int intr = sceKernelCpuSuspendIntr();
    SceSysmemUidCB *cb = NULL;
    if (sceKernelGetUIDcontrolBlockWithType(mutex_id, g_tman.uid_mutex, &cb) != 0) {
        sceKernelCpuResumeIntr(intr);
        return (s32)SCE_ERROR_KERNEL_MUTEX_NOT_FOUND;
    }
    if (!tmanCheckUidAttr(cb, 4)) {
        sceKernelCpuResumeIntr(intr);
        return (s32)SCE_ERROR_KERNEL_ILLEGAL_PERMISSION;
    }

    TmanWaitQueue *wq = (TmanWaitQueue *)tmanUidData(cb, g_tman.uid_waitq);
    TmanMutexCB *mtx = (TmanMutexCB *)tmanUidData(cb, g_tman.uid_mutex);
    if (lock_count > 1 && (wq->attr & SCE_KERNEL_MUTEX_ATTR_RECURSIVE) == 0) {
        sceKernelCpuResumeIntr(intr);
        return (s32)SCE_ERROR_KERNEL_MUTEX_RECURSIVE_NOT_ALLOWED;
    }

    if (g_tman.log_handler != NULL)
        g_tman.log_handler(175, (u32)mtx, 2, mutex_id, lock_count);

    TmanThreadCB *cur = g_tman.current_thread;
    s32 ret = 0;
    if (mtx->owner == NULL) {
        mtx->owner = cur;
        mtx->lock_count = lock_count;
        tmanListInsertTail(&cur->owned_mutex_list, &mtx->owner_link);
    } else if (mtx->owner == cur) {
        if ((wq->attr & SCE_KERNEL_MUTEX_ATTR_RECURSIVE) == 0)
            ret = (s32)SCE_ERROR_KERNEL_MUTEX_RECURSIVE_NOT_ALLOWED;
        else if (mtx->lock_count + lock_count < mtx->lock_count)
            ret = (s32)SCE_ERROR_KERNEL_MUTEX_LOCK_OVERFLOW;
        else
            mtx->lock_count += lock_count;
    } else {
        ret = (s32)SCE_ERROR_KERNEL_MUTEX_LOCKED;
    }

    sceKernelCpuResumeIntr(intr);
    return ret;
}

s32 sceKernelCancelMutex(SceUID mutex_id, s32 new_count, s32 *num_wait_threads)
{
    if (!tmanCheckUserPtr(num_wait_threads, sizeof(s32)))
        return (s32)SCE_ERROR_KERNEL_ILLEGAL_ADDR;

    int intr = sceKernelCpuSuspendIntr();
    SceSysmemUidCB *cb = NULL;
    if (sceKernelGetUIDcontrolBlockWithType(mutex_id, g_tman.uid_mutex, &cb) != 0) {
        sceKernelCpuResumeIntr(intr);
        return (s32)SCE_ERROR_KERNEL_MUTEX_NOT_FOUND;
    }
    if (!tmanCheckUidAttr(cb, 8)) {
        sceKernelCpuResumeIntr(intr);
        return (s32)SCE_ERROR_KERNEL_ILLEGAL_PERMISSION;
    }

    TmanWaitQueue *wq = (TmanWaitQueue *)tmanUidData(cb, g_tman.uid_waitq);
    TmanMutexCB *mtx = (TmanMutexCB *)tmanUidData(cb, g_tman.uid_mutex);
    if (new_count > 1 && (wq->attr & SCE_KERNEL_MUTEX_ATTR_RECURSIVE) == 0) {
        sceKernelCpuResumeIntr(intr);
        return (s32)SCE_ERROR_KERNEL_MUTEX_RECURSIVE_NOT_ALLOWED;
    }

    if (g_tman.log_handler != NULL)
        g_tman.log_handler(177, (u32)mtx, 2, mutex_id, new_count);

    if (mtx->owner != NULL) {
        tmanListRemove(&mtx->owner_link);
        mtx->owner = NULL;
    }
    s32 count = (new_count < 0) ? mtx->init_count : new_count;
    mtx->lock_count = count;
    if (count > 0) {
        mtx->owner = g_tman.current_thread;
        tmanListInsertTail(&g_tman.current_thread->owned_mutex_list, &mtx->owner_link);
    }

    int woken = sceKernelCallUIDObjFunction(cb, (s32)TMAN_UID_FUNC_CANCEL_WAIT);
    if (num_wait_threads != NULL)
        *num_wait_threads = woken;

    sceKernelCpuResumeIntr(intr);
    return 0;
}

s32 sceKernelReferMutexStatus(SceUID mutex_id, SceKernelMutexInfo *info)
{
    if (!tmanCheckUserPtr(info, sizeof(*info)))
        return (s32)SCE_ERROR_KERNEL_ILLEGAL_ADDR;

    int intr = sceKernelCpuSuspendIntr();
    SceSysmemUidCB *cb = NULL;
    if (sceKernelGetUIDcontrolBlockWithType(mutex_id, g_tman.uid_mutex, &cb) != 0) {
        sceKernelCpuResumeIntr(intr);
        return (s32)SCE_ERROR_KERNEL_MUTEX_NOT_FOUND;
    }
    if (!tmanCheckUidAttr(cb, 1)) {
        sceKernelCpuResumeIntr(intr);
        return (s32)SCE_ERROR_KERNEL_ILLEGAL_PERMISSION;
    }

    if (g_tman.log_handler != NULL)
        g_tman.log_handler(178, (u32)&g_tman.current_thread, 2, mutex_id, info);

    TmanWaitQueue *wq = (TmanWaitQueue *)tmanUidData(cb, g_tman.uid_waitq);
    TmanMutexCB *mtx = (TmanMutexCB *)tmanUidData(cb, g_tman.uid_mutex);
    SceKernelMutexInfo local;
    memset(&local, 0, sizeof(local));
    local.size = sizeof(local);
    if (cb->name != NULL)
        strncpy(local.name, cb->name, SCE_UID_NAME_LEN);
    local.attr = wq->attr;
    local.initCount = mtx->init_count;
    local.currentCount = mtx->lock_count;
    local.currentOwner = (mtx->owner != NULL) ? mtx->owner->uid : 0;
    local.numWaitThreads = wq->num_wait_threads;

    SceSize copy_size = info->size < local.size ? info->size : local.size;
    memcpy(info, &local, copy_size);
    sceKernelCpuResumeIntr(intr);
    return 0;
}

/* ============================================================================
 * Lightweight Mutexes
 * ============================================================================ */

void tmanInitLwMutexType(void)
{
    sceKernelCreateUIDtypeInherit("WaitQ", "LwMutex", sizeof(TmanLwMutexCB),
                                  (SceSysmemUidLookupFunc *)g_uid_lwmutex_funcs,
                                  NULL, &g_tman.uid_lwmutex);
}

static int lwmutexUidCreate(SceSysmemUidCB *cb, SceSysmemUidCB *type, int func_id, void *args)
{
    TmanLwMutexCB *lw = (TmanLwMutexCB *)tmanUidData(cb, type);
    sceKernelCallUIDObjCommonFunction(cb, type, func_id, args);
    tmanListInit(&lw->owner_link);
    return cb->uid ^ (s32)g_tman.uid_xor_key;
}

static int lwmutexUidDelete(SceSysmemUidCB *cb, SceSysmemUidCB *type, int func_id, void *args)
{
    TmanLwMutexCB *lw = (TmanLwMutexCB *)tmanUidData(cb, type);
    if (lw->work_area != NULL) {
        lw->work_area->id = -1;
        lw->work_area->numWaitThreads = 0;
    }
    sceKernelCallUIDObjCommonFunction(cb, type, func_id, args);
    return 0;
}

static int lwmutexUidRemove(SceSysmemUidCB *cb, SceSysmemUidCB *type, int func_id, void *args)
{
    TmanWaitQueue *wq = (TmanWaitQueue *)tmanUidData(cb, g_tman.uid_waitq);
    TmanLwMutexCB *lw = (TmanLwMutexCB *)tmanUidData(cb, g_tman.uid_lwmutex);
    sceKernelCallUIDObjCommonFunction(cb, type, func_id, args);
    if (lw->work_area != NULL)
        lw->work_area->numWaitThreads = wq->num_wait_threads;
    return 0;
}

static int lwmutexUidCheck(SceSysmemUidCB *cb, SceSysmemUidCB *type, int func_id, void *args)
{
    (void)type;
    (void)func_id;
    (void)args;
    TmanWaitQueue *wq = (TmanWaitQueue *)tmanUidData(cb, g_tman.uid_waitq);
    TmanLwMutexCB *lw = (TmanLwMutexCB *)tmanUidData(cb, g_tman.uid_lwmutex);
    if (lw->work_area == NULL || lw->work_area->thid != -1 || tmanListEmpty(&wq->wait_list))
        return 0;

    TmanThreadCB *next_owner = (TmanThreadCB *)wq->wait_list.next;
    lw->work_area->thid = next_owner->uid;
    lw->work_area->lockCount = (s32)next_owner->wait_param[0];
    return tmanWakeupWaitingThread(next_owner);
}

void tmanReleaseOwnedLwMutexes(TmanThreadCB *th)
{
    (void)th;
}

int sceKernelCreateLwMutex(SceLwMutex *work_area, const char *name, u32 attr,
                           s32 init_count, void *opt)
{
    if (!tmanCheckUserPtr(work_area, sizeof(*work_area)) ||
        !tmanCheckUserAddr((u32)name) || !tmanCheckUserPtr(opt, 4))
        return (int)SCE_ERROR_KERNEL_ILLEGAL_ADDR;
    if (tmanReadIntrDepth() != 0)
        return (int)SCE_ERROR_KERNEL_CANNOT_BE_CALLED_FROM_INTERRUPT;
    if ((attr & ~0x3FFU) != 0)
        return (int)SCE_ERROR_KERNEL_ILLEGAL_ATTR;
    if (init_count < 0)
        return (int)SCE_ERROR_KERNEL_ILLEGAL_COUNT;
    if (init_count > 1 && (attr & SCE_KERNEL_LWMUTEX_RECURSIVE) == 0)
        return (int)SCE_ERROR_KERNEL_LWMUTEX_RECURSIVE_NOT_ALLOWED;

    int is_user = tmanIsUserCaller();
    int intr = sceKernelCpuSuspendIntr();
    SceSysmemUidCB *cb = NULL;
    int ret = sceKernelCreateUID(g_tman.uid_lwmutex, name,
                                 (char)((attr & 0xFFU) | (is_user ? 0xFFU : 0x00U)), &cb);
    if (ret != 0) {
        sceKernelCpuResumeIntr(intr);
        return ret;
    }

    TmanWaitQueue *wq = (TmanWaitQueue *)tmanUidData(cb, g_tman.uid_waitq);
    TmanLwMutexCB *lw = (TmanLwMutexCB *)tmanUidData(cb, g_tman.uid_lwmutex);
    SceUID uid = cb->uid ^ (s32)g_tman.uid_xor_key;
    wq->attr = attr;
    lw->work_area = work_area;
    lw->init_count = init_count;
    lw->opt = opt;

    work_area->lockCount = init_count;
    work_area->thid = (init_count > 0) ? g_tman.current_thread->uid : -1;
    work_area->flags = attr;
    work_area->numWaitThreads = 0;
    work_area->id = uid;

    sceKernelCpuResumeIntr(intr);
    return 0;
}

int sceKernelDeleteLwMutex(SceLwMutex *work_area)
{
    if (!tmanCheckUserPtr(work_area, sizeof(*work_area)))
        return (int)SCE_ERROR_KERNEL_ILLEGAL_ADDR;
    if (tmanReadIntrDepth() != 0)
        return (int)SCE_ERROR_KERNEL_CANNOT_BE_CALLED_FROM_INTERRUPT;

    int intr = sceKernelCpuSuspendIntr();
    SceUID uid = work_area->id;
    SceSysmemUidCB *cb = NULL;
    if (sceKernelGetUIDcontrolBlockWithType(uid, g_tman.uid_lwmutex, &cb) != 0) {
        sceKernelCpuResumeIntr(intr);
        return (int)SCE_ERROR_KERNEL_LWMUTEX_NOT_FOUND;
    }
    if (!tmanCheckUidAttr(cb, 0x10)) {
        sceKernelCpuResumeIntr(intr);
        return (int)SCE_ERROR_KERNEL_ILLEGAL_PERMISSION;
    }

    sceKernelDeleteUID(uid);
    sceKernelCpuResumeIntr(intr);
    return 0;
}

static s32 tmanLockLwMutexInternal(SceLwMutex *work_area, s32 lock_count, u32 *timeout, int use_cb)
{
    if (!tmanCheckUserPtr(work_area, sizeof(*work_area)) || !tmanCheckUserPtr(timeout, sizeof(u32)))
        return (s32)SCE_ERROR_KERNEL_ILLEGAL_ADDR;
    if (tmanReadIntrDepth() != 0)
        return (s32)SCE_ERROR_KERNEL_CANNOT_BE_CALLED_FROM_INTERRUPT;
    if (lock_count <= 0)
        return (s32)SCE_ERROR_KERNEL_ILLEGAL_COUNT;

    int intr = sceKernelCpuSuspendIntr();
    if (intr == 0 || g_tman.dispatch_suspended != 0) {
        sceKernelCpuResumeIntr(intr);
        return (s32)SCE_ERROR_KERNEL_WAIT_CAN_NOT_WAIT;
    }

    tmanCheckKernelStackGuard();
    SceSysmemUidCB *cb = NULL;
    if (sceKernelGetUIDcontrolBlockWithType(work_area->id, g_tman.uid_lwmutex, &cb) != 0) {
        sceKernelCpuResumeIntr(intr);
        return (s32)SCE_ERROR_KERNEL_LWMUTEX_NOT_FOUND;
    }
    if (!tmanCheckUidAttr(cb, 4)) {
        sceKernelCpuResumeIntr(intr);
        return (s32)SCE_ERROR_KERNEL_ILLEGAL_PERMISSION;
    }

    TmanWaitQueue *wq = (TmanWaitQueue *)tmanUidData(cb, g_tman.uid_waitq);
    if (lock_count > 1 && (wq->attr & SCE_KERNEL_LWMUTEX_RECURSIVE) == 0) {
        sceKernelCpuResumeIntr(intr);
        return (s32)SCE_ERROR_KERNEL_LWMUTEX_RECURSIVE_NOT_ALLOWED;
    }

    u64 abs_time = 0;
    if (timeout != NULL)
        tmanCalcAbsTimeoutUsec(&abs_time, *timeout);

    TmanThreadCB *cur = g_tman.current_thread;
    s32 ret;
    for (;;) {
        if (use_cb != 0 && cur->cb_notify_pending != 0)
            tmanCheckPendingCallbacks(intr);

        if (work_area->thid == -1) {
            work_area->thid = cur->uid;
            work_area->lockCount = lock_count;
            ret = 0;
            break;
        }
        if (work_area->thid == cur->uid) {
            if ((wq->attr & SCE_KERNEL_LWMUTEX_RECURSIVE) == 0) {
                ret = (s32)SCE_ERROR_KERNEL_LWMUTEX_RECURSIVE_NOT_ALLOWED;
                break;
            }
            if (work_area->lockCount + lock_count < work_area->lockCount) {
                ret = (s32)SCE_ERROR_KERNEL_LWMUTEX_LOCK_OVERFLOW;
                break;
            }
            work_area->lockCount += lock_count;
            ret = 0;
            break;
        }

        cur->wait_param[0] = (u32)lock_count;
        cur->cb_wait_enabled = use_cb;
        ret = tmanAddThreadToWaitQueue(cb, TMAN_WAIT_LWMUTEX, wq->attr, &abs_time, timeout,
                                       (u32 *)&work_area->numWaitThreads);
        if (use_cb == 0 || ret != (s32)SCE_ERROR_KERNEL_WAIT_STATUS_RELEASED_CALLBACK)
            break;
    }

    sceKernelCpuResumeIntr(intr);
    return ret;
}

s32 _sceKernelLockLwMutex(SceLwMutex *work_area, s32 lock_count, u32 *timeout)
{
    return tmanLockLwMutexInternal(work_area, lock_count, timeout, 0);
}

s32 _sceKernelLockLwMutexCB(SceLwMutex *work_area, s32 lock_count, u32 *timeout)
{
    return tmanLockLwMutexInternal(work_area, lock_count, timeout, 1);
}

s32 _sceKernelTryLockLwMutex(SceLwMutex *work_area, s32 lock_count)
{
    if (!tmanCheckUserPtr(work_area, sizeof(*work_area)))
        return (s32)SCE_ERROR_KERNEL_ILLEGAL_ADDR;
    if (tmanReadIntrDepth() != 0)
        return (s32)SCE_ERROR_KERNEL_CANNOT_BE_CALLED_FROM_INTERRUPT;
    if (lock_count <= 0)
        return (s32)SCE_ERROR_KERNEL_ILLEGAL_COUNT;

    int intr = sceKernelCpuSuspendIntr();
    SceSysmemUidCB *cb = NULL;
    if (sceKernelGetUIDcontrolBlockWithType(work_area->id, g_tman.uid_lwmutex, &cb) != 0) {
        sceKernelCpuResumeIntr(intr);
        return (s32)SCE_ERROR_KERNEL_LWMUTEX_NOT_FOUND;
    }

    TmanWaitQueue *wq = (TmanWaitQueue *)tmanUidData(cb, g_tman.uid_waitq);
    TmanThreadCB *cur = g_tman.current_thread;
    s32 ret = 0;

    if (work_area->thid == -1) {
        work_area->thid = cur->uid;
        work_area->lockCount = lock_count;
    } else if (work_area->thid == cur->uid) {
        if ((wq->attr & SCE_KERNEL_LWMUTEX_RECURSIVE) == 0)
            ret = (s32)SCE_ERROR_KERNEL_LWMUTEX_RECURSIVE_NOT_ALLOWED;
        else if (work_area->lockCount + lock_count < work_area->lockCount)
            ret = (s32)SCE_ERROR_KERNEL_LWMUTEX_LOCK_OVERFLOW;
        else
            work_area->lockCount += lock_count;
    } else {
        ret = (s32)SCE_ERROR_KERNEL_LWMUTEX_LOCKED;
    }

    sceKernelCpuResumeIntr(intr);
    return ret;
}

s32 _sceKernelUnlockLwMutex(SceLwMutex *work_area, s32 unlock_count)
{
    if (!tmanCheckUserPtr(work_area, sizeof(*work_area)))
        return (s32)SCE_ERROR_KERNEL_ILLEGAL_ADDR;
    if (tmanReadIntrDepth() != 0)
        return (s32)SCE_ERROR_KERNEL_CANNOT_BE_CALLED_FROM_INTERRUPT;
    if (unlock_count <= 0)
        return (s32)SCE_ERROR_KERNEL_ILLEGAL_COUNT;

    int intr = sceKernelCpuSuspendIntr();
    SceSysmemUidCB *cb = NULL;
    if (sceKernelGetUIDcontrolBlockWithType(work_area->id, g_tman.uid_lwmutex, &cb) != 0) {
        sceKernelCpuResumeIntr(intr);
        return (s32)SCE_ERROR_KERNEL_LWMUTEX_NOT_FOUND;
    }

    TmanWaitQueue *wq = (TmanWaitQueue *)tmanUidData(cb, g_tman.uid_waitq);
    if (work_area->thid != g_tman.current_thread->uid) {
        sceKernelCpuResumeIntr(intr);
        return (s32)SCE_ERROR_KERNEL_LWMUTEX_UNLOCKED;
    }
    if (work_area->lockCount < unlock_count) {
        sceKernelCpuResumeIntr(intr);
        return (s32)SCE_ERROR_KERNEL_LWMUTEX_UNLOCK_UNDERFLOW;
    }

    work_area->lockCount -= unlock_count;
    if (work_area->lockCount == 0) {
        work_area->thid = -1;
        if (!tmanListEmpty(&wq->wait_list)) {
            TmanThreadCB *next_owner = (TmanThreadCB *)wq->wait_list.next;
            work_area->thid = next_owner->uid;
            work_area->lockCount = (s32)next_owner->wait_param[0];
            tmanWakeupAndReschedule(next_owner, 0, intr);
        }
    }

    sceKernelCpuResumeIntr(intr);
    return 0;
}

int sceKernelReferLwMutexStatusByID(SceUID id, TmanLwMutexInfo *info)
{
    if (!tmanCheckUserPtr(info, sizeof(*info)))
        return (int)SCE_ERROR_KERNEL_ILLEGAL_ADDR;

    int intr = sceKernelCpuSuspendIntr();
    SceSysmemUidCB *cb = NULL;
    if (sceKernelGetUIDcontrolBlockWithType(id, g_tman.uid_lwmutex, &cb) != 0) {
        sceKernelCpuResumeIntr(intr);
        return (int)SCE_ERROR_KERNEL_LWMUTEX_NOT_FOUND;
    }
    if (!tmanCheckUidAttr(cb, 1)) {
        sceKernelCpuResumeIntr(intr);
        return (int)SCE_ERROR_KERNEL_ILLEGAL_PERMISSION;
    }

    TmanWaitQueue *wq = (TmanWaitQueue *)tmanUidData(cb, g_tman.uid_waitq);
    TmanLwMutexCB *lw = (TmanLwMutexCB *)tmanUidData(cb, g_tman.uid_lwmutex);
    TmanLwMutexInfo local;
    memset(&local, 0, sizeof(local));
    local.size = sizeof(local);
    if (cb->name != NULL)
        strncpy(local.name, cb->name, SCE_UID_NAME_LEN);
    local.attr = wq->attr;
    local.uid = id;
    local.work_area = lw->work_area;
    local.init_count = lw->init_count;
    if (lw->work_area != NULL) {
        local.current_count = lw->work_area->lockCount;
        local.current_owner = (lw->work_area->thid == -1) ? 0 : lw->work_area->thid;
    }
    local.num_wait_threads = wq->num_wait_threads;

    SceSize copy_size = info->size < local.size ? info->size : local.size;
    memcpy(info, &local, copy_size);
    sceKernelCpuResumeIntr(intr);
    return 0;
}

/* ============================================================================
 * Message Boxes
 * ============================================================================ */

void tmanInitMbxType(void)
{
    sceKernelCreateUIDtypeInherit("WaitQ", "Mbx", sizeof(TmanMbxCB),
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

static TmanMsgPacket *tmanMbxDequeueMsg(TmanMbxCB *mbx)
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

static int tmanReceiveMbxInternal(SceUID mbxid, void **pmessage, SceUInt *timeout, int use_cb)
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

/* ============================================================================
 * Message Pipes
 * ============================================================================ */

static void tmanMsgPipeWriteRing(TmanMsgPipeCB *mpp, const u8 *src, u32 len)
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

static int tmanSendMsgPipeInternal(SceUID uid, void *message, unsigned int size,
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

static int tmanReceiveMsgPipeInternal(SceUID uid, void *message, unsigned int size,
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
