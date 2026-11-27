#include "threadman_internal.h"

static int mutexUidCreate(SceSysmemUidCB *cb, SceSysmemUidCB *type, int func_id, void *args);
static int mutexUidDelete(SceSysmemUidCB *cb, SceSysmemUidCB *type, int func_id, void *args);
static int mutexUidCheck(SceSysmemUidCB *cb, SceSysmemUidCB *type, int func_id, void *args);

const TmanUidFunc g_uid_mutex_funcs[4] = {
    { TMAN_UID_FUNC_CHECK_WAIT, (void *)mutexUidCheck },
    { TMAN_UID_FUNC_CREATE,     (void *)mutexUidCreate },
    { TMAN_UID_FUNC_DELETE,     (void *)mutexUidDelete },
    { 0, NULL },
};


/* ============================================================================
 * Mutexes
 * ============================================================================ */

void tmanInitMutexType(void)
{
    sceKernelCreateUIDtypeInherit("WaitQ", "Mutex", 48,
                                  (SceSysmemUidLookupFunc *)g_uid_mutex_funcs,
                                  NULL, &g_tman.uid_mutex);
}

static int mutexUidCreate(SceSysmemUidCB *cb, SceSysmemUidCB *type, int func_id, void *args)
{
    sceKernelCallUIDObjCommonFunction(cb, type, func_id, args);
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

void tmanMutexAcquire(SceSysmemUidCB *cb, TmanThreadCB *owner, s32 count)
{
    TmanWaitQueue *wq = (TmanWaitQueue *)tmanUidData(cb, g_tman.uid_waitq);
    TmanMutexCB *mtx = (TmanMutexCB *)tmanUidData(cb, g_tman.uid_mutex);
    mtx->lock_count = count;
    mtx->owner = owner;
    if ((wq->attr & 0x400U) != 0) {
        TmanListNode *head = &owner->owned_mutex_list;
        TmanListNode *cur = head->next;
        while (cur != head) {
            TmanMutexCB *other = (TmanMutexCB *)cur;
            if (mtx->prio_ceiling < other->prio_ceiling)
                break;
            cur = cur->next;
        }
        tmanListInsertBefore(cur, &mtx->owner_link);
        if (mtx->prio_ceiling < owner->inherited_priority)
            owner->inherited_priority = mtx->prio_ceiling;
        if (owner->inherited_priority < owner->current_priority)
            tmanChangeThreadPriorityInternal(owner, owner->inherited_priority, 0);
    } else {
        tmanListInsertTail(&owner->owned_mutex_list, &mtx->owner_link);
    }
}

void tmanMutexRelease(SceSysmemUidCB *cb)
{
    TmanMutexCB *mtx = (TmanMutexCB *)tmanUidData(cb, g_tman.uid_mutex);
    TmanThreadCB *owner = mtx->owner;
    mtx->lock_count = 0;
    mtx->owner = NULL;
    if (owner == NULL || owner == (TmanThreadCB *)-1)
        return;
    tmanListRemove(&mtx->owner_link);
    s32 new_prio;
    if (tmanListEmpty(&owner->owned_mutex_list)) {
        owner->inherited_priority = 127;
        new_prio = owner->base_priority;
    } else {
        TmanMutexCB *top = (TmanMutexCB *)owner->owned_mutex_list.next;
        SceSysmemUidCB *top_cb = tmanDataToUid(top, g_tman.uid_mutex);
        TmanWaitQueue *top_wq = (TmanWaitQueue *)tmanUidData(top_cb, g_tman.uid_waitq);
        if ((top_wq->attr & 0x400U) != 0) {
            owner->inherited_priority = top->prio_ceiling;
            new_prio = (top->prio_ceiling < owner->base_priority)
                     ? top->prio_ceiling : owner->base_priority;
        } else {
            owner->inherited_priority = 127;
            new_prio = owner->base_priority;
        }
    }
    tmanChangeThreadPriorityInternal(owner, new_prio, 0);
}

void tmanReleaseOwnedMutexes(TmanThreadCB *th)
{
    TmanListNode *head = &th->owned_mutex_list;
    int need_sched = 0;
    while (!tmanListEmpty(head)) {
        TmanMutexCB *mtx = (TmanMutexCB *)head->next;
        SceSysmemUidCB *cb = tmanDataToUid(mtx, g_tman.uid_mutex);
        TmanWaitQueue *wq = (TmanWaitQueue *)tmanUidData(cb, g_tman.uid_waitq);
        tmanListRemove(&mtx->owner_link);
        if ((wq->attr & 0x800U) != 0) {
            mtx->owner = (TmanThreadCB *)-1;
            mtx->lock_count = 0;
            need_sched |= tmanNotifyWaitObject(cb);
        } else {
            mtx->owner = NULL;
            mtx->lock_count = 0;
            need_sched |= tmanNotifyWaitObject(cb);
        }
    }
    if (need_sched != 0)
        tmanSchedule(1, 0);
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

static inline __attribute__((always_inline)) s32 tmanLockMutexInternal(SceUID mutex_id, s32 lock_count, u32 *timeout, int use_cb)
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

