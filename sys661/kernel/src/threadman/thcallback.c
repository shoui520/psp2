#include "threadman_internal.h"

static int threvUidCreate(SceSysmemUidCB *cb, SceSysmemUidCB *type, int func_id, void *args);
static int threvUidDelete(SceSysmemUidCB *cb, SceSysmemUidCB *type, int func_id, void *args);
static int cbUidCreate(SceSysmemUidCB *cb, SceSysmemUidCB *type, int func_id, void *args);
static int cbUidDelete(SceSysmemUidCB *cb, SceSysmemUidCB *type, int func_id, void *args);
static int tmanKernelHandlerTrampoline(void);


const TmanUidFunc g_uid_threv_funcs[3] = {
    { TMAN_UID_FUNC_CREATE, (void *)threvUidCreate },
    { TMAN_UID_FUNC_DELETE, (void *)threvUidDelete },
    { 0, NULL },
};

const TmanUidFunc g_uid_cb_funcs[3] = {
    { TMAN_UID_FUNC_CREATE, (void *)cbUidCreate },
    { TMAN_UID_FUNC_DELETE, (void *)cbUidDelete },
    { 0, NULL },
};


void tmanInitEventAndCallbackTypes(void)
{
    sceKernelCreateUIDtypeInherit("WaitQ", "ThreadEventHandler",
                                  48,
                                  (SceSysmemUidLookupFunc *)g_uid_threv_funcs,
                                  NULL, &g_tman.uid_threv);
    sceKernelCreateUIDtypeInherit("WaitQ", "Callback",
                                  48,
                                  (SceSysmemUidLookupFunc *)g_uid_cb_funcs,
                                  NULL, &g_tman.uid_cb);
}

static int threvUidCreate(SceSysmemUidCB *cb, SceSysmemUidCB *type, int func_id, void *args)
{
    sceKernelCallUIDObjCommonFunction(cb, type, func_id, args);
    return cb->uid ^ (s32)g_tman.uid_xor_key;
}

static int threvUidDelete(SceSysmemUidCB *cb, SceSysmemUidCB *type, int func_id, void *args)
{
    int intr = sceKernelCpuSuspendIntr();
    TmanThreadEventHandlerCB *ev = (TmanThreadEventHandlerCB *)tmanUidData(cb, type);

    if (ev->deleting != 0) {
        sceKernelCpuResumeIntr(intr);
        return (int)SCE_ERROR_KERNEL_NOT_FOUND_THREAD_EVENT_HANDLER;
    }

    ev->deleting = 1;
    while (ev->active_count > 0) {
        tmanCheckKernelStackGuard();
        int ret = tmanWaitOnObject(cb, TMAN_WAIT_EVENT_HANDLER, 0, NULL, NULL);
        if (ret != 0) {
            sceKernelCpuResumeIntr(intr);
            return ret;
        }
    }

    tmanListRemove(&ev->link);
    sceKernelCpuResumeIntr(intr);
    sceKernelCallUIDObjCommonFunction(cb, type, func_id, args);
    return 0;
}

static int cbUidCreate(SceSysmemUidCB *cb, SceSysmemUidCB *type, int func_id, void *args)
{
    sceKernelCallUIDObjCommonFunction(cb, type, func_id, args);
    return cb->uid ^ (s32)g_tman.uid_xor_key;
}

static int cbUidDelete(SceSysmemUidCB *cb, SceSysmemUidCB *type, int func_id, void *args)
{
    int intr = sceKernelCpuSuspendIntr();
    TmanCallbackCB *cb_obj = (TmanCallbackCB *)tmanUidData(cb, type);

    if (cb_obj->deleting != 0) {
        sceKernelCpuResumeIntr(intr);
        return (int)SCE_ERROR_KERNEL_NOT_FOUND_CALLBACK;
    }

    cb_obj->deleting = 1;
    while (cb_obj->active_count > 0) {
        tmanCheckKernelStackGuard();
        int ret = tmanWaitOnObject(cb, TMAN_WAIT_CALLBACK_DEL, 0, NULL, NULL);
        if (ret != 0) {
            sceKernelCpuResumeIntr(intr);
            return ret;
        }
    }

    tmanListRemove(&cb_obj->link);
    sceKernelCpuResumeIntr(intr);
    sceKernelCallUIDObjCommonFunction(cb, type, func_id, args);
    return 0;
}

int sceKernelRegisterThreadEventHandler(const char *name, SceUID thid, int mask,
                                        void *handler, void *common)
{
    int is_user = tmanIsUserCaller();
    if (!tmanCheckUserAddr((u32)name) || !tmanCheckUserAddr((u32)handler) ||
        (!is_user && (s32)(u32)handler >= 0))
        return (int)SCE_ERROR_KERNEL_ILLEGAL_ADDR;

    if (tmanReadIntrDepth() != 0 || g_tman.current_thread->event_mask_active != 0)
        return (int)SCE_ERROR_KERNEL_CANNOT_BE_CALLED_FROM_INTERRUPT;

    if ((mask & ~0x0F) != 0)
        return (int)SCE_ERROR_KERNEL_ILLEGAL_MASK;

    int intr = sceKernelCpuSuspendIntr();
    void *target = NULL;

    if (thid == -1 || thid == -16 || thid == -8) {
        if (is_user && thid != -16) {
            sceKernelCpuResumeIntr(intr);
            return (int)SCE_ERROR_KERNEL_NOT_FOUND_THREAD;
        }
        target = (void *)(intptr_t)thid;
    } else {
        SceUID lookup_id = thid;
        if (lookup_id == 0) {
            if (mask != TMAN_THREV_EXIT) {
                sceKernelCpuResumeIntr(intr);
                return (int)SCE_ERROR_KERNEL_ILLEGAL_ATTR;
            }
            lookup_id = g_tman.current_thread->uid;
        }
        SceSysmemUidCB *th_cb = NULL;
        if (sceKernelGetUIDcontrolBlockWithType(lookup_id, g_tman.uid_thread, &th_cb) != 0) {
            sceKernelCpuResumeIntr(intr);
            return (int)SCE_ERROR_KERNEL_NOT_FOUND_THREAD;
        }
        if (!tmanCheckUidAttr(th_cb, 2)) {
            sceKernelCpuResumeIntr(intr);
            return (int)SCE_ERROR_KERNEL_ILLEGAL_PERMISSION;
        }
        TmanThreadCB *th = (TmanThreadCB *)tmanUidData(th_cb, g_tman.uid_thread);
        if (is_user &&
            ((g_tman.current_thread->attr ^ 0x80000000U) >> 28) != ((th->attr ^ 0x80000000U) >> 28)) {
            sceKernelCpuResumeIntr(intr);
            return (int)SCE_ERROR_KERNEL_ILLEGAL_PERMISSION;
        }
        target = th;
    }

    SceSysmemUidCB *ev_cb = NULL;
    int ret = sceKernelCreateUID(g_tman.uid_threv, name, (char)(is_user ? 0xFF : 0x00), &ev_cb);
    if (ret != 0) {
        sceKernelCpuResumeIntr(intr);
        return ret;
    }

    TmanThreadEventHandlerCB *ev = (TmanThreadEventHandlerCB *)tmanUidData(ev_cb, g_tman.uid_threv);
    SceUID uid = ev_cb->uid ^ (s32)g_tman.uid_xor_key;
    ev->uid = uid;
    ev->active_count = 0;
    ev->deleting = 0;
    ev->target_thread = target;
    ev->is_user = (u32)is_user;
    ev->mask = (u32)mask;
    ev->handler = handler;
    ev->common = common;
    ev->gp_reg = (u32)sceKernelGetModuleGPByAddressForKernel((u32)handler);
    if (ev->gp_reg == 0)
        ev->gp_reg = tmanGetGP();

    tmanListInsertTail(&g_tman.event_handler_list, &ev->link);
    if (g_tman.log_handler != NULL)
        g_tman.log_handler(45, (u32)ev, 5, thid, mask, handler, common, uid);

    sceKernelCpuResumeIntr(intr);
    return uid;
}

int sceKernelReleaseThreadEventHandler(SceUID id)
{
    if (tmanReadIntrDepth() != 0 || g_tman.current_thread->event_mask_active != 0)
        return (int)SCE_ERROR_KERNEL_CANNOT_BE_CALLED_FROM_INTERRUPT;

    int intr = sceKernelCpuSuspendIntr();
    if (intr == 0 || g_tman.dispatch_suspended != 0) {
        sceKernelCpuResumeIntr(intr);
        return (int)SCE_ERROR_KERNEL_WAIT_CAN_NOT_WAIT;
    }

    SceSysmemUidCB *ev_cb = NULL;
    if (sceKernelGetUIDcontrolBlockWithType(id, g_tman.uid_threv, &ev_cb) != 0) {
        sceKernelCpuResumeIntr(intr);
        return (int)SCE_ERROR_KERNEL_NOT_FOUND_THREAD_EVENT_HANDLER;
    }

    if (!tmanCheckUidAttr(ev_cb, 0x10)) {
        sceKernelCpuResumeIntr(intr);
        return (int)SCE_ERROR_KERNEL_ILLEGAL_PERMISSION;
    }

    if (g_tman.log_handler != NULL)
        g_tman.log_handler(46, (u32)ev_cb, 1, id);

    int ret = sceKernelDeleteUID(id);
    sceKernelCpuResumeIntr(intr);
    return ret;
}

static int tmanThreadEventMatches(const TmanThreadEventHandlerCB *ev, u32 mask, const TmanThreadCB *target)
{
    if ((ev->mask & mask) == 0 || ev->deleting != 0)
        return 0;

    TmanThreadCB *cur = g_tman.current_thread;
    if ((s32)cur->attr < 0) {
        if (((cur->attr ^ 0x80000000U) >> 28) != ((target->attr ^ 0x80000000U) >> 28))
            return 0;
    }

    intptr_t tgt = (intptr_t)ev->target_thread;
    if (tgt == -1)
        return 1;
    if ((s32)target->attr < 0) {
        if (tgt == -16 && (s32)cur->attr < 0)
            return 1;
    } else if (tgt == -8) {
        return 1;
    }
    return ev->target_thread == (const void *)target;
}

int tmanDispatchThreadEvents(u32 mask, TmanThreadCB *target, int intr_state)
{
    TmanThreadCB *cur = g_tman.current_thread;
    TmanListNode *head = &g_tman.event_handler_list;
    TmanListNode *node = head->next;
    int woken = 0;

    cur->event_mask_active |= mask;
    while (node != head) {
        TmanThreadEventHandlerCB *ev = (TmanThreadEventHandlerCB *)node;
        SceSysmemUidCB *ev_cb = tmanDataToUid(ev, g_tman.uid_threv);

        if (!tmanThreadEventMatches(ev, mask, target)) {
            node = node->next;
            continue;
        }

        cur->active_event_handler = ev_cb;
        ev->active_count++;
        if (g_tman.log_handler != NULL)
            g_tman.log_handler(8, (u32)ev, 4, ev_cb->uid ^ g_tman.uid_xor_key, mask, ev->common, ev->handler);

        int ret = tmanExecuteHandlerContext((int)ev->is_user, ev->handler,
                                            mask, (u32)target->uid, (u32)ev->common, 0, ev->gp_reg);

        cur->active_event_handler = NULL;
        ev->active_count--;
        node = ev->link.next;

        if (ev->deleting != 0 && ev->active_count == 0) {
            tmanListRemove(&ev->link);
            woken += tmanWakeupAllWaiters(ev_cb, 0);
        } else if (ev->deleting == 0 && ret != 0) {
            sceKernelDeleteUID(ev->uid);
        }
    }

    if (g_tman.dispatch_suspended != 0)
        tmanResumeDispatchInternal(1, intr_state);

    if (woken != 0) {
        g_tman.next_thread = NULL;
        tmanSchedule(0, intr_state);
    }

    cur->event_mask_active &= ~mask;
    return 0;
}

int sceKernelReferThreadEventHandlerStatus(SceUID id, TmanThreadEventHandlerInfo *info)
{
    if (!tmanCheckUserPtr(info, sizeof(*info)))
        return (int)SCE_ERROR_KERNEL_ILLEGAL_ADDR;

    int intr = sceKernelCpuSuspendIntr();
    SceSysmemUidCB *ev_cb = NULL;
    if (sceKernelGetUIDcontrolBlockWithType(id, g_tman.uid_threv, &ev_cb) != 0) {
        sceKernelCpuResumeIntr(intr);
        return (int)SCE_ERROR_KERNEL_NOT_FOUND_THREAD_EVENT_HANDLER;
    }

    if (!tmanCheckUidAttr(ev_cb, 1)) {
        sceKernelCpuResumeIntr(intr);
        return (int)SCE_ERROR_KERNEL_ILLEGAL_PERMISSION;
    }

    if (g_tman.log_handler != NULL)
        g_tman.log_handler(47, (u32)&g_tman.current_thread, 2, id, info);

    TmanThreadEventHandlerCB *ev = (TmanThreadEventHandlerCB *)tmanUidData(ev_cb, g_tman.uid_threv);
    TmanThreadEventHandlerInfo local;
    memset(&local, 0, sizeof(local));
    local.size = sizeof(local);
    if (ev_cb->name != NULL)
        strncpy(local.name, ev_cb->name, SCE_UID_NAME_LEN);

    intptr_t tgt = (intptr_t)ev->target_thread;
    if (tgt == -8 || tgt == -16 || tgt == -1)
        local.thread_id = (SceUID)tgt;
    else
        local.thread_id = ((TmanThreadCB *)ev->target_thread)->uid;

    local.mask = ev->mask;
    local.handler = ev->handler;
    local.common = ev->common;

    SceSize copy_size = info->size < local.size ? info->size : local.size;
    memcpy(info, &local, copy_size);
    sceKernelCpuResumeIntr(intr);
    return 0;
}

SceUID sceKernelCreateCallback(const char *name, SceKernelCallbackFunction func, void *arg)
{
    int is_user = tmanIsUserCaller();
    if (!tmanCheckUserAddr((u32)name) || !tmanCheckUserAddr((u32)func) ||
        (!is_user && (s32)(u32)func >= 0))
        return (SceUID)SCE_ERROR_KERNEL_ILLEGAL_ADDR;

    if (tmanReadIntrDepth() != 0)
        return (SceUID)SCE_ERROR_KERNEL_CANNOT_BE_CALLED_FROM_INTERRUPT;

    int intr = sceKernelCpuSuspendIntr();
    SceSysmemUidCB *cb = NULL;
    int ret = sceKernelCreateUID(g_tman.uid_cb, name, (char)(is_user ? 0xFF : 0x00), &cb);
    if (ret != 0) {
        sceKernelCpuResumeIntr(intr);
        return ret;
    }

    TmanCallbackCB *cb_obj = (TmanCallbackCB *)tmanUidData(cb, g_tman.uid_cb);
    TmanThreadCB *cur = g_tman.current_thread;
    SceUID uid = cb->uid ^ (s32)g_tman.uid_xor_key;

    cb_obj->uid = uid;
    cb_obj->active_count = 0;
    cb_obj->deleting = 0;
    cb_obj->owner = cur;
    cb_obj->notify_count = 0;
    cb_obj->notify_arg = 0;
    cb_obj->is_user = (u32)is_user;
    cb_obj->handler = (void *)func;
    cb_obj->common = arg;
    cb_obj->gp_reg = (u32)sceKernelGetModuleGPByAddressForKernel((u32)func);
    if (cb_obj->gp_reg == 0)
        cb_obj->gp_reg = tmanGetGP();

    tmanListInsertTail(&cur->cb_list, &cb_obj->link);
    if (g_tman.log_handler != NULL)
        g_tman.log_handler(156, (u32)cb_obj, 3, func, arg, uid);

    sceKernelCpuResumeIntr(intr);
    return uid;
}

int sceKernelDeleteCallback(SceUID cb_id)
{
    if (tmanReadIntrDepth() != 0 || g_tman.current_thread->active_cb != NULL)
        return (int)SCE_ERROR_KERNEL_CANNOT_BE_CALLED_FROM_INTERRUPT;

    int intr = sceKernelCpuSuspendIntr();
    if (intr == 0 || g_tman.dispatch_suspended != 0) {
        sceKernelCpuResumeIntr(intr);
        return (int)SCE_ERROR_KERNEL_WAIT_CAN_NOT_WAIT;
    }

    SceSysmemUidCB *cb = NULL;
    if (sceKernelGetUIDcontrolBlockWithType(cb_id, g_tman.uid_cb, &cb) != 0) {
        sceKernelCpuResumeIntr(intr);
        return (int)SCE_ERROR_KERNEL_NOT_FOUND_CALLBACK;
    }

    if (!tmanCheckUidAttr(cb, 0x10)) {
        sceKernelCpuResumeIntr(intr);
        return (int)SCE_ERROR_KERNEL_ILLEGAL_PERMISSION;
    }

    if (g_tman.log_handler != NULL)
        g_tman.log_handler(157, (u32)cb, 1, cb_id);

    int ret = sceKernelDeleteUID(cb_id);
    sceKernelCpuResumeIntr(intr);
    return ret;
}

int sceKernelNotifyCallback(SceUID cb_id, int arg)
{
    int in_intr = (int)tmanReadIntrDepth();
    int intr = sceKernelCpuSuspendIntr();
    SceSysmemUidCB *cb = NULL;

    if (sceKernelGetUIDcontrolBlockWithType(cb_id, g_tman.uid_cb, &cb) != 0) {
        sceKernelCpuResumeIntr(intr);
        return (int)SCE_ERROR_KERNEL_NOT_FOUND_CALLBACK;
    }

    if (!tmanCheckUidAttr(cb, 2)) {
        sceKernelCpuResumeIntr(intr);
        return (int)SCE_ERROR_KERNEL_ILLEGAL_PERMISSION;
    }

    TmanCallbackCB *cb_obj = (TmanCallbackCB *)tmanUidData(cb, g_tman.uid_cb);
    if (g_tman.log_handler != NULL)
        g_tman.log_handler(158, (u32)cb_obj, 2, cb_id, arg);

    TmanThreadCB *owner = cb_obj->owner;
    owner->cb_notify_pending = 1;
    cb_obj->notify_arg = arg;
    cb_obj->notify_count++;

    if (owner->cb_wait_enabled != 0 &&
        (owner->status == TMAN_TH_WAIT || owner->status == TMAN_TH_WAITSUSPEND)) {
        owner->wait_result = (s32)SCE_ERROR_KERNEL_WAIT_STATUS_RELEASED_CALLBACK;
        int need_sched = tmanWakeupWaitingThread(owner);
        if (owner->wait_obj != NULL)
            need_sched += tmanNotifyWaitObject(owner->wait_obj);
        if (need_sched != 0) {
            g_tman.next_thread = NULL;
            if (in_intr == 0)
                tmanSchedule(0, intr);
        }
    }

    sceKernelCpuResumeIntr(intr);
    return 0;
}

int sceKernelCancelCallback(SceUID cb_id)
{
    int intr = sceKernelCpuSuspendIntr();
    SceSysmemUidCB *cb = NULL;

    if (sceKernelGetUIDcontrolBlockWithType(cb_id, g_tman.uid_cb, &cb) != 0) {
        sceKernelCpuResumeIntr(intr);
        return (int)SCE_ERROR_KERNEL_NOT_FOUND_CALLBACK;
    }

    if (!tmanCheckUidAttr(cb, 8)) {
        sceKernelCpuResumeIntr(intr);
        return (int)SCE_ERROR_KERNEL_ILLEGAL_PERMISSION;
    }

    TmanCallbackCB *cb_obj = (TmanCallbackCB *)tmanUidData(cb, g_tman.uid_cb);
    if (g_tman.log_handler != NULL)
        g_tman.log_handler(159, (u32)&g_tman.current_thread, 1, cb_id);

    cb_obj->notify_arg = 0;
    cb_obj->notify_count = 0;
    sceKernelCpuResumeIntr(intr);
    return 0;
}

int sceKernelGetCallbackCount(SceUID cb_id)
{
    int intr = sceKernelCpuSuspendIntr();
    SceSysmemUidCB *cb = NULL;

    if (sceKernelGetUIDcontrolBlockWithType(cb_id, g_tman.uid_cb, &cb) != 0) {
        sceKernelCpuResumeIntr(intr);
        return (int)SCE_ERROR_KERNEL_NOT_FOUND_CALLBACK;
    }

    if (!tmanCheckUidAttr(cb, 1)) {
        sceKernelCpuResumeIntr(intr);
        return (int)SCE_ERROR_KERNEL_ILLEGAL_PERMISSION;
    }

    TmanCallbackCB *cb_obj = (TmanCallbackCB *)tmanUidData(cb, g_tman.uid_cb);
    int count = cb_obj->notify_count;
    if (g_tman.log_handler != NULL)
        g_tman.log_handler(160, (u32)&g_tman.current_thread, 2, cb_id, count);

    sceKernelCpuResumeIntr(intr);
    return count;
}

int sceKernelCheckCallback(void)
{
    TmanThreadCB *cur = g_tman.current_thread;
    if (tmanReadIntrDepth() != 0 || cur->active_cb != NULL)
        return (int)SCE_ERROR_KERNEL_CANNOT_BE_CALLED_FROM_INTERRUPT;

    int intr = sceKernelCpuSuspendIntr();
    if (intr == 0 || g_tman.dispatch_suspended != 0) {
        sceKernelCpuResumeIntr(intr);
        return (int)SCE_ERROR_KERNEL_WAIT_CAN_NOT_WAIT;
    }

    int ran = 0;
    if (cur->cb_notify_pending != 0)
        ran = (tmanCheckPendingCallbacks(intr) != 0) ? 1 : 0;

    if (g_tman.log_handler != NULL)
        g_tman.log_handler(162, (u32)cur, 1, ran);

    sceKernelCpuResumeIntr(intr);
    return ran;
}

int tmanCheckPendingCallbacks(int intr_state)
{
    TmanThreadCB *cur = g_tman.current_thread;
    TmanListNode *head = &cur->cb_list;
    TmanListNode *node = head->next;
    int ran = 0;
    int woken = 0;

    cur->cb_notify_pending = 0;
    while (node != head) {
        TmanCallbackCB *cb_obj = (TmanCallbackCB *)node;
        SceSysmemUidCB *cb = tmanDataToUid(cb_obj, g_tman.uid_cb);

        if (cb_obj->notify_count == 0 || cb_obj->deleting != 0) {
            node = node->next;
            continue;
        }

        s32 count = cb_obj->notify_count;
        s32 arg = cb_obj->notify_arg;
        cb_obj->notify_count = 0;
        cb_obj->notify_arg = 0;

        SceSysmemUidCB *prev_cb = cur->active_cb;
        cur->cb_nesting++;
        cur->active_cb = cb;
        cb_obj->active_count++;

        if (g_tman.log_handler != NULL)
            g_tman.log_handler(7, (u32)cb_obj, 5, cb->uid ^ g_tman.uid_xor_key,
                               count, arg, cb_obj->common, cb_obj->handler);

        int ret = tmanExecuteHandlerContext((int)cb_obj->is_user, cb_obj->handler,
                                            (u32)count, (u32)arg, (u32)cb_obj->common, 0, cb_obj->gp_reg);

        cur->active_cb = prev_cb;
        cur->cb_nesting--;
        cb_obj->active_count--;
        node = cb_obj->link.next;
        ran++;

        if (cb_obj->deleting != 0 && cb_obj->active_count == 0) {
            tmanListRemove(&cb_obj->link);
            woken += tmanWakeupAllWaiters(cb, 0);
        } else if (cb_obj->deleting == 0 && ret != 0) {
            sceKernelDeleteUID(cb_obj->uid);
        }
    }

    if (g_tman.dispatch_suspended != 0)
        tmanResumeDispatchInternal(1, intr_state);

    if (woken != 0) {
        g_tman.next_thread = NULL;
        tmanSchedule(0, intr_state);
    }
    return ran;
}

int sceKernelReferCallbackStatus(SceUID cb_id, SceKernelCallbackInfo *info)
{
    if (!tmanCheckUserPtr(info, sizeof(*info)))
        return (int)SCE_ERROR_KERNEL_ILLEGAL_ADDR;

    int intr = sceKernelCpuSuspendIntr();
    SceSysmemUidCB *cb = NULL;
    if (sceKernelGetUIDcontrolBlockWithType(cb_id, g_tman.uid_cb, &cb) != 0) {
        sceKernelCpuResumeIntr(intr);
        return (int)SCE_ERROR_KERNEL_NOT_FOUND_CALLBACK;
    }

    if (!tmanCheckUidAttr(cb, 1)) {
        sceKernelCpuResumeIntr(intr);
        return (int)SCE_ERROR_KERNEL_ILLEGAL_PERMISSION;
    }

    if (g_tman.log_handler != NULL)
        g_tman.log_handler(163, (u32)&g_tman.current_thread, 2, cb_id, info);

    TmanCallbackCB *cb_obj = (TmanCallbackCB *)tmanUidData(cb, g_tman.uid_cb);
    SceKernelCallbackInfo local;
    memset(&local, 0, sizeof(local));
    local.size = sizeof(local);
    if (cb->name != NULL)
        strncpy(local.name, cb->name, SCE_UID_NAME_LEN);
    local.threadId = cb_obj->owner->uid;
    local.callback = (SceKernelCallbackFunction)cb_obj->handler;
    local.common = cb_obj->common;
    local.notifyCount = cb_obj->notify_count;
    local.notifyArg = cb_obj->notify_arg;

    SceSize copy_size = info->size < local.size ? info->size : local.size;
    memcpy(info, &local, copy_size);
    sceKernelCpuResumeIntr(intr);
    return 0;
}

int _sceKernelReturnFromCallback(void)
{
    sceKernelCpuSuspendIntr();
    TmanThreadCB *cur = g_tman.current_thread;
    cur->wait_result = (s32)cur->syscall_stack->return_val;
    return tmanReturnFromHandlerContext();
}

void tmanCleanupActiveEventHandler(TmanThreadCB *th)
{
    SceSysmemUidCB *ev_cb = th->active_event_handler;
    if (ev_cb == NULL)
        return;

    TmanThreadEventHandlerCB *ev = (TmanThreadEventHandlerCB *)tmanUidData(ev_cb, g_tman.uid_threv);
    ev->active_count--;
    if (ev->deleting != 0 && ev->active_count == 0) {
        tmanListRemove(&ev->link);
        tmanWakeupAllWaiters(ev_cb, 0);
    }
    th->active_event_handler = NULL;
    th->event_mask_active = 0;
}

void tmanCleanupActiveCallback(TmanThreadCB *th)
{
    SceSysmemUidCB *cb = th->active_cb;
    if (cb == NULL)
        return;

    TmanCallbackCB *cb_obj = (TmanCallbackCB *)tmanUidData(cb, g_tman.uid_cb);
    cb_obj->active_count--;
    if (cb_obj->deleting != 0 && cb_obj->active_count == 0) {
        tmanListRemove(&cb_obj->link);
        tmanWakeupAllWaiters(cb, 0);
    }
    th->active_cb = NULL;
}

void tmanDeleteThreadCallbacks(TmanThreadCB *th)
{
    TmanListNode *head = &th->cb_list;
    TmanListNode *node = head->next;
    while (node != head) {
        TmanCallbackCB *cb_obj = (TmanCallbackCB *)node;
        node = node->next;
        cb_obj->active_count = 0;
        sceKernelDeleteUID(cb_obj->uid);
    }
    th->cb_nesting = 0;
    th->cb_notify_pending = 0;
    th->active_cb = NULL;
}

int ThreadManForUser_8672E3D0(void)
{
    if (tmanReadIntrDepth() != 0)
        return (int)SCE_ERROR_KERNEL_CANNOT_BE_CALLED_FROM_INTERRUPT;

    TmanThreadCB *cur = g_tman.current_thread;
    if (g_tman.log_handler != NULL)
        g_tman.log_handler(161, (u32)cur, 2, cur->uid, cur->cb_nesting);
    return cur->cb_nesting;
}

int tmanReturnFromHandlerContext(void)
{
    TmanThreadCB *cur = g_tman.current_thread;
    if (tmanReadIntrDepth() != 0 || cur->saved_cb_ctx == NULL)
        tmanBreak();

    if ((cur->saved_cb_ctx->sr & 0x40000000U) != 0) {
        cur->attr |= SCE_KERNEL_TH_USE_VFPU;
        tmanSwitchVfpuContext(cur);
    } else {
        cur->attr &= ~SCE_KERNEL_TH_USE_VFPU;
    }

    InterruptManagerForKernel_551097F2(cur->saved_cb_ctx);
    return 0;
}

static int tmanKernelHandlerTrampoline(void)
{
    int saved;
    __asm__ volatile("move %0, $2" : "=r"(saved));
    sceKernelCpuSuspendIntr();
    g_tman.current_thread->wait_result = saved;
    return tmanReturnFromHandlerContext();
}

int tmanExecuteHandlerContext(int is_user, void *handler, u32 reg_a, u32 reg_b,
                              u32 reg_c, u32 reg_d, u32 gp_reg)
{
    u8 raw_ctx[sizeof(TmanThreadContext) + 64];
    TmanThreadContext *new_ctx = (TmanThreadContext *)(((u32)raw_ctx + 63U) & ~63U);

    tmanInitThreadContext(new_ctx);
    TmanThreadCB *cur = g_tman.current_thread;
    new_ctx->epc = (u32)handler;
    new_ctx->gpr[4] = reg_a;
    new_ctx->gpr[5] = reg_b;
    new_ctx->gpr[6] = reg_c;
    new_ctx->gpr[7] = reg_d;
    new_ctx->gpr[28] = gp_reg;

    u32 saved_vfpu_dirty = cur->vfpu_dirty;
    void *saved_kstack_top = cur->kstack_top;
    TmanSyscallStack *saved_syscall_stack = cur->syscall_stack;
    TmanThreadContext *prev_saved_ctx = cur->saved_cb_ctx;

    tmanCheckKernelStackGuard();
    cur->saved_cb_ctx = cur->ctx;

    if (SaveThreadContext(cur->ctx) != 0) {
        TmanThreadContext *restored = cur->saved_cb_ctx;
        cur->kstack_top = saved_kstack_top;
        cur->syscall_stack = saved_syscall_stack;
        cur->saved_cb_ctx = prev_saved_ctx;
        cur->vfpu_dirty = saved_vfpu_dirty;
        cur->ctx = restored;
        if (tmanReadCfc0(8) == (u32)new_ctx)
            tmanWriteCtc0(8, 0);
        tmanWriteCtc0(14, (u32)restored);
        tmanWriteCtc0(15, (u32)saved_kstack_top);
        return cur->wait_result;
    }

    cur->ctx = new_ctx;
    u32 target_sp;
    if (is_user != 0) {
        u32 user_sp = (cur->syscall_stack != NULL) ? cur->syscall_stack->user_sp : (u32)cur->init_sp;
        u32 *tramp = (u32 *)(user_sp - 64U);
        tramp[0] = ((g_tman.syscall_ret_callback << 6) & 0x03FFFFC0U) | 0x0CU;
        tramp[1] = 0;
        tramp[2] = 0;
        tramp[3] = 0;
        tmanCacheOp(0x1BU, tramp);
        tmanCacheOp(0x08U, tramp);
        tmanSync();
        new_ctx->gpr[31] = (u32)tramp;
        new_ctx->sr = 0x13U;
        cur->kstack_top = (void *)cur->saved_cb_ctx->gpr[29];
        target_sp = (u32)tramp;
    } else {
        target_sp = cur->saved_cb_ctx->gpr[29];
        new_ctx->gpr[31] = (u32)tmanKernelHandlerTrampoline;
        new_ctx->sr = 0x03U;
        cur->kstack_top = NULL;
    }

    new_ctx->status_flag = 3;
    new_ctx->gpr[26] = (u32)cur->user_tls;
    new_ctx->gpr[29] = target_sp;
    new_ctx->gpr[30] = target_sp;
    cur->vfpu_dirty = 0;
    cur->attr &= ~SCE_KERNEL_TH_USE_VFPU;

    tmanWriteCtc0(15, (u32)cur->kstack_top);
    tmanWriteCtc0(14, (u32)new_ctx);
    InterruptManagerForKernel_551097F2(new_ctx);
    return 0;
}

