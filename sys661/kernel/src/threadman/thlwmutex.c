#include "threadman_internal.h"

static int lwmutexUidCreate(SceSysmemUidCB *cb, SceSysmemUidCB *type, int func_id, void *args);
static int lwmutexUidDelete(SceSysmemUidCB *cb, SceSysmemUidCB *type, int func_id, void *args);
static int lwmutexUidRemove(SceSysmemUidCB *cb, SceSysmemUidCB *type, int func_id, void *args);
static int lwmutexUidCheck(SceSysmemUidCB *cb, SceSysmemUidCB *type, int func_id, void *args);

const TmanUidFunc g_uid_lwmutex_funcs[5] = {
    { TMAN_UID_FUNC_REMOVE_WAIT, (void *)lwmutexUidRemove },
    { TMAN_UID_FUNC_CHECK_WAIT,  (void *)lwmutexUidCheck },
    { TMAN_UID_FUNC_CREATE,      (void *)lwmutexUidCreate },
    { TMAN_UID_FUNC_DELETE,      (void *)lwmutexUidDelete },
    { 0, NULL },
};


/* ============================================================================
 * Lightweight Mutexes
 * ============================================================================ */

void tmanInitLwMutexType(void)
{
    sceKernelCreateUIDtypeInherit("WaitQ", "LwMutex", 48,
                                  (SceSysmemUidLookupFunc *)g_uid_lwmutex_funcs,
                                  NULL, &g_tman.uid_lwmutex);
}

static int lwmutexUidCreate(SceSysmemUidCB *cb, SceSysmemUidCB *type, int func_id, void *args)
{
    sceKernelCallUIDObjCommonFunction(cb, type, func_id, args);
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

static inline __attribute__((always_inline)) s32 tmanLockLwMutexInternal(SceLwMutex *work_area, s32 lock_count, u32 *timeout, int use_cb)
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

