#include "threadman_internal.h"

static int semaUidCreate(SceSysmemUidCB *cb, SceSysmemUidCB *type, int func_id, void *args);
static int semaUidDelete(SceSysmemUidCB *cb, SceSysmemUidCB *type, int func_id, void *args);
static int semaUidCheck(SceSysmemUidCB *cb, SceSysmemUidCB *type, int func_id, void *args);

const TmanUidFunc g_uid_sema_funcs[4] = {
    { TMAN_UID_FUNC_CHECK_WAIT, (void *)semaUidCheck },
    { TMAN_UID_FUNC_CREATE,     (void *)semaUidCreate },
    { TMAN_UID_FUNC_DELETE,     (void *)semaUidDelete },
    { 0, NULL },
};


/* ============================================================================
 * Semaphores
 * ============================================================================ */

void tmanInitSemaType(void)
{
    sceKernelCreateUIDtypeInherit("WaitQ", "Semaphore", 48,
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

static inline __attribute__((always_inline)) int tmanWaitSemaInternal(SceUID semaid, int signal, SceUInt *timeout, int use_cb)
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

