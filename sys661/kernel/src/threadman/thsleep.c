#include "threadman_internal.h"

int sceKernelSleepThread(void)
{
    if (tmanReadIntrDepth() != 0)
        return (int)SCE_ERROR_KERNEL_CANNOT_BE_CALLED_FROM_INTERRUPT;

    int intr = sceKernelCpuSuspendIntr();
    if (intr == 0 || g_tman.dispatch_suspended != 0) {
        sceKernelCpuResumeIntr(intr);
        return (int)SCE_ERROR_KERNEL_WAIT_CAN_NOT_WAIT;
    }

    if (g_tman.log_handler != NULL)
        g_tman.log_handler(48, 0, 0);

    tmanCheckKernelStackGuard();
    TmanThreadCB *cur = g_tman.current_thread;
    cur->wait_result = 0;

    if (cur->wakeup_count > 0) {
        cur->wakeup_count--;
    } else {
        tmanParkCurrentThread(&g_tman.sleeping_list, TMAN_WAIT_SLEEP);
        g_tman.next_thread = NULL;
        tmanSchedule(1, 1);
    }

    sceKernelCpuResumeIntr(intr);
    return cur->wait_result;
}

int sceKernelSleepThreadCB(void)
{
    if (tmanReadIntrDepth() != 0)
        return (int)SCE_ERROR_KERNEL_CANNOT_BE_CALLED_FROM_INTERRUPT;

    int intr = sceKernelCpuSuspendIntr();
    if (intr == 0 || g_tman.dispatch_suspended != 0) {
        sceKernelCpuResumeIntr(intr);
        return (int)SCE_ERROR_KERNEL_WAIT_CAN_NOT_WAIT;
    }

    if (g_tman.log_handler != NULL)
        g_tman.log_handler(49, 0, 0);

    tmanCheckKernelStackGuard();
    TmanThreadCB *cur = g_tman.current_thread;

    for (;;) {
        while (cur->cb_notify_pending != 0)
            tmanCheckPendingCallbacks(intr);

        cur->wait_result = 0;
        if (cur->wakeup_count > 0) {
            cur->wakeup_count--;
        } else {
            tmanParkCurrentThread(&g_tman.sleeping_list, TMAN_WAIT_SLEEP);
            cur->cb_wait_enabled = 1;
            g_tman.next_thread = NULL;
            tmanSchedule(1, 1);
        }

        if (cur->wait_result != (s32)SCE_ERROR_KERNEL_WAIT_STATUS_RELEASED_CALLBACK)
            break;
    }

    sceKernelCpuResumeIntr(intr);
    return cur->wait_result;
}

int sceKernelWakeupThread(SceUID thid)
{
    if (thid == 0)
        return (int)SCE_ERROR_KERNEL_ILLEGAL_THREAD;

    int intr = sceKernelCpuSuspendIntr();
    int in_intr = (int)tmanReadIntrDepth();
    SceSysmemUidCB *th_cb = NULL;

    if (sceKernelGetUIDcontrolBlockWithType(thid, g_tman.uid_thread, &th_cb) != 0) {
        sceKernelCpuResumeIntr(intr);
        return (int)SCE_ERROR_KERNEL_NOT_FOUND_THREAD;
    }

    if (!tmanCheckUidAttr(th_cb, 2)) {
        sceKernelCpuResumeIntr(intr);
        return (int)SCE_ERROR_KERNEL_ILLEGAL_PERMISSION;
    }

    TmanThreadCB *th = (TmanThreadCB *)tmanUidData(th_cb, g_tman.uid_thread);
    if (in_intr == 0 && th == g_tman.current_thread) {
        sceKernelCpuResumeIntr(intr);
        return (int)SCE_ERROR_KERNEL_ILLEGAL_THREAD;
    }

    if (th->status == TMAN_TH_STOPPED || th->status == TMAN_TH_KILLED) {
        sceKernelCpuResumeIntr(intr);
        return (int)SCE_ERROR_KERNEL_THREAD_ALREADY_DORMANT;
    }

    if (g_tman.log_handler != NULL)
        g_tman.log_handler(50, (u32)th, 1, thid);

    if ((th->status == TMAN_TH_WAIT || th->status == TMAN_TH_WAITSUSPEND) &&
        th->wait_type == TMAN_WAIT_SLEEP) {
        tmanWakeupAndReschedule(th, in_intr, intr);
    } else {
        th->wakeup_count++;
    }

    sceKernelCpuResumeIntr(intr);
    return 0;
}

int sceKernelCancelWakeupThread(SceUID thid)
{
    int in_intr = (int)tmanReadIntrDepth();
    if (thid == 0) {
        if (in_intr != 0)
            return (int)SCE_ERROR_KERNEL_ILLEGAL_THREAD;
        thid = g_tman.current_thread->uid;
    }

    int intr = sceKernelCpuSuspendIntr();
    SceSysmemUidCB *th_cb = NULL;
    if (sceKernelGetUIDcontrolBlockWithType(thid, g_tman.uid_thread, &th_cb) != 0) {
        sceKernelCpuResumeIntr(intr);
        return (int)SCE_ERROR_KERNEL_NOT_FOUND_THREAD;
    }

    if (!tmanCheckUidAttr(th_cb, 8)) {
        sceKernelCpuResumeIntr(intr);
        return (int)SCE_ERROR_KERNEL_ILLEGAL_PERMISSION;
    }

    TmanThreadCB *th = (TmanThreadCB *)tmanUidData(th_cb, g_tman.uid_thread);
    if (g_tman.log_handler != NULL)
        g_tman.log_handler(51, (u32)th, 1, thid);

    int prev_count = th->wakeup_count;
    th->wakeup_count = 0;
    sceKernelCpuResumeIntr(intr);
    return prev_count;
}

int sceKernelDonateWakeupThread(SceUID thid)
{
    if (thid == 0)
        return (int)SCE_ERROR_KERNEL_ILLEGAL_THREAD;
    if (tmanReadIntrDepth() != 0)
        return (int)SCE_ERROR_KERNEL_CANNOT_BE_CALLED_FROM_INTERRUPT;

    int intr = sceKernelCpuSuspendIntr();
    if (intr == 0 || g_tman.dispatch_suspended != 0) {
        sceKernelCpuResumeIntr(intr);
        return (int)SCE_ERROR_KERNEL_WAIT_CAN_NOT_WAIT;
    }

    tmanCheckKernelStackGuard();
    SceSysmemUidCB *th_cb = NULL;
    if (sceKernelGetUIDcontrolBlockWithType(thid, g_tman.uid_thread, &th_cb) != 0) {
        sceKernelCpuResumeIntr(intr);
        return (int)SCE_ERROR_KERNEL_NOT_FOUND_THREAD;
    }

    if (!tmanCheckUidAttr(th_cb, 2)) {
        sceKernelCpuResumeIntr(intr);
        return (int)SCE_ERROR_KERNEL_ILLEGAL_PERMISSION;
    }

    TmanThreadCB *cur = g_tman.current_thread;
    TmanThreadCB *target = (TmanThreadCB *)tmanUidData(th_cb, g_tman.uid_thread);
    if (target == cur) {
        sceKernelCpuResumeIntr(intr);
        return (int)SCE_ERROR_KERNEL_ILLEGAL_THREAD;
    }

    if (target->status == TMAN_TH_STOPPED || target->status == TMAN_TH_KILLED) {
        sceKernelCpuResumeIntr(intr);
        return (int)SCE_ERROR_KERNEL_THREAD_ALREADY_DORMANT;
    }

    s32 donated = cur->wakeup_count;
    if (g_tman.log_handler != NULL)
        g_tman.log_handler(52, (u32)target, 2, thid, donated);

    cur->wakeup_count = 0;
    cur->wait_result = 0;
    tmanParkCurrentThread(&g_tman.sleeping_list, TMAN_WAIT_SLEEP);

    target->wakeup_count += donated;
    if ((target->status == TMAN_TH_WAIT || target->status == TMAN_TH_WAITSUSPEND) &&
        target->wait_type == TMAN_WAIT_SLEEP) {
        tmanWakeupWaitingThread(target);
    }

    g_tman.next_thread = NULL;
    tmanSchedule(1, 1);
    sceKernelCpuResumeIntr(intr);
    return cur->wait_result;
}

int sceKernelSuspendThread(SceUID thid)
{
    if (thid == 0)
        return (int)SCE_ERROR_KERNEL_ILLEGAL_THREAD;

    int intr = sceKernelCpuSuspendIntr();
    int in_intr = (int)tmanReadIntrDepth();
    SceSysmemUidCB *th_cb = NULL;

    if (sceKernelGetUIDcontrolBlockWithType(thid, g_tman.uid_thread, &th_cb) != 0) {
        sceKernelCpuResumeIntr(intr);
        return (int)SCE_ERROR_KERNEL_NOT_FOUND_THREAD;
    }

    if (!tmanCheckUidAttr(th_cb, 2)) {
        sceKernelCpuResumeIntr(intr);
        return (int)SCE_ERROR_KERNEL_ILLEGAL_PERMISSION;
    }

    TmanThreadCB *th = (TmanThreadCB *)tmanUidData(th_cb, g_tman.uid_thread);
    if (in_intr == 0 && th == g_tman.current_thread) {
        sceKernelCpuResumeIntr(intr);
        return (int)SCE_ERROR_KERNEL_ILLEGAL_THREAD;
    }

    if (g_tman.log_handler != NULL)
        g_tman.log_handler(53, (u32)th, 1, thid);

    int ret = 0;
    switch (th->status) {
        case TMAN_TH_RUNNING:
            if (g_tman.dispatch_suspended != 0) {
                if (g_tman.dispatch_suspended != 3)
                    g_tman.dispatch_suspended = 2;
                break;
            }
            /* fall through */
        case TMAN_TH_READY:
            if (in_intr != 0 && th == g_tman.current_thread)
                g_tman.next_thread = NULL;
            else
                tmanRemoveReadyQueue(th, (u32)th->current_priority);
            th->status = TMAN_TH_SUSPEND;
            tmanListInsertTail(&g_tman.suspended_list, &th->link);
            break;
        case TMAN_TH_WAIT:
            th->status = TMAN_TH_WAITSUSPEND;
            break;
        case TMAN_TH_SUSPEND:
        case TMAN_TH_WAITSUSPEND:
            ret = (int)SCE_ERROR_KERNEL_THREAD_ALREADY_SUSPEND;
            break;
        default:
            ret = (int)SCE_ERROR_KERNEL_THREAD_ALREADY_DORMANT;
            break;
    }

    sceKernelCpuResumeIntr(intr);
    return ret;
}

int sceKernelResumeThread(SceUID thid)
{
    if (thid == 0)
        return (int)SCE_ERROR_KERNEL_ILLEGAL_THREAD;

    int intr = sceKernelCpuSuspendIntr();
    int in_intr = (int)tmanReadIntrDepth();
    SceSysmemUidCB *th_cb = NULL;

    if (sceKernelGetUIDcontrolBlockWithType(thid, g_tman.uid_thread, &th_cb) != 0) {
        sceKernelCpuResumeIntr(intr);
        return (int)SCE_ERROR_KERNEL_NOT_FOUND_THREAD;
    }

    if (!tmanCheckUidAttr(th_cb, 2)) {
        sceKernelCpuResumeIntr(intr);
        return (int)SCE_ERROR_KERNEL_ILLEGAL_PERMISSION;
    }

    TmanThreadCB *th = (TmanThreadCB *)tmanUidData(th_cb, g_tman.uid_thread);
    if (in_intr == 0 && th == g_tman.current_thread) {
        sceKernelCpuResumeIntr(intr);
        return (int)SCE_ERROR_KERNEL_ILLEGAL_THREAD;
    }

    if (g_tman.log_handler != NULL)
        g_tman.log_handler(54, (u32)th, 1, thid);

    int ret = 0;
    switch (th->status) {
        case TMAN_TH_RUNNING:
            if (g_tman.dispatch_suspended == 2)
                g_tman.dispatch_suspended = 1;
            else
                ret = (int)SCE_ERROR_KERNEL_THREAD_IS_NOT_SUSPEND;
            break;
        case TMAN_TH_SUSPEND:
            tmanListRemove(&th->link);
            th->status = TMAN_TH_READY;
            if (in_intr != 0) {
                tmanAddReadyQueue(th);
                g_tman.next_thread = NULL;
            } else {
                tmanMakeReadyAndPreempt(th, intr);
            }
            break;
        case TMAN_TH_WAITSUSPEND:
            th->status = TMAN_TH_WAIT;
            break;
        default:
            ret = (int)SCE_ERROR_KERNEL_THREAD_IS_NOT_SUSPEND;
            break;
    }

    sceKernelCpuResumeIntr(intr);
    return ret;
}

int sceKernelSuspendAllUserThreads(void)
{
    if (tmanReadIntrDepth() != 0)
        return (int)SCE_ERROR_KERNEL_CANNOT_BE_CALLED_FROM_INTERRUPT;

    int intr = sceKernelCpuSuspendIntr();
    SceSysmemUidCB *head = g_tman.uid_thread;
    SceSysmemUidCB *cb = head->PARENT0;

    while (cb != head) {
        TmanThreadCB *th = (TmanThreadCB *)tmanUidData(cb, head);
        if ((s32)th->attr < 0 || (th->attr & 0x08000000U) != 0) {
            if (th->status == TMAN_TH_READY) {
                tmanRemoveReadyQueue(th, (u32)th->current_priority);
                th->status = TMAN_TH_SUSPEND;
                tmanListInsertTail(&g_tman.suspended_list, &th->link);
            } else if (th->status == TMAN_TH_WAIT) {
                th->status = TMAN_TH_WAITSUSPEND;
            }
        }
        cb = cb->PARENT0;
    }

    g_tman.user_threads_suspended = 1;
    sceKernelCpuResumeIntr(intr);
    return 0;
}

static inline __attribute__((always_inline)) int tmanWaitThreadEndInternal(SceUID thid, SceUInt *timeout, int use_cb)
{
    if (!tmanCheckUserPtr(timeout, sizeof(SceUInt)))
        return (int)SCE_ERROR_KERNEL_ILLEGAL_ADDR;
    if (thid == 0)
        return (int)SCE_ERROR_KERNEL_ILLEGAL_THREAD;
    if (tmanReadIntrDepth() != 0)
        return (int)SCE_ERROR_KERNEL_CANNOT_BE_CALLED_FROM_INTERRUPT;

    int intr = sceKernelCpuSuspendIntr();
    if (intr == 0 || g_tman.dispatch_suspended != 0) {
        sceKernelCpuResumeIntr(intr);
        return (int)SCE_ERROR_KERNEL_WAIT_CAN_NOT_WAIT;
    }

    tmanCheckKernelStackGuard();
    SceSysmemUidCB *th_cb = NULL;
    if (sceKernelGetUIDcontrolBlockWithType(thid, g_tman.uid_thread, &th_cb) != 0) {
        sceKernelCpuResumeIntr(intr);
        return (int)SCE_ERROR_KERNEL_NOT_FOUND_THREAD;
    }

    if (!tmanCheckUidAttr(th_cb, 4)) {
        sceKernelCpuResumeIntr(intr);
        return (int)SCE_ERROR_KERNEL_ILLEGAL_PERMISSION;
    }

    TmanThreadCB *cur = g_tman.current_thread;
    TmanThreadCB *target = (TmanThreadCB *)tmanUidData(th_cb, g_tman.uid_thread);
    if (target == cur) {
        sceKernelCpuResumeIntr(intr);
        return (int)SCE_ERROR_KERNEL_ILLEGAL_THREAD;
    }

    if (g_tman.log_handler != NULL)
        g_tman.log_handler(use_cb ? 56 : 55, (u32)&g_tman.current_thread, 1, thid);

    u64 abs_time = 0;
    if (timeout != NULL)
        tmanCalcAbsTimeoutUsec(&abs_time, *timeout);

    int ret;
    for (;;) {
        if (use_cb != 0 && cur->cb_notify_pending != 0)
            tmanCheckPendingCallbacks(intr);

        if (target->status == TMAN_TH_STOPPED || target->status == TMAN_TH_KILLED) {
            ret = target->exit_status;
            break;
        }

        cur->cb_wait_enabled = use_cb;
        ret = tmanWaitOnObject(th_cb, TMAN_WAIT_THREAD_END, 0, &abs_time, timeout);
        if (use_cb == 0 || ret != (int)SCE_ERROR_KERNEL_WAIT_STATUS_RELEASED_CALLBACK)
            break;
    }

    sceKernelCpuResumeIntr(intr);
    return ret;
}

int sceKernelWaitThreadEnd(SceUID thid, SceUInt *timeout)
{
    return tmanWaitThreadEndInternal(thid, timeout, 0);
}

int sceKernelWaitThreadEndCB(SceUID thid, SceUInt *timeout)
{
    return tmanWaitThreadEndInternal(thid, timeout, 1);
}

static inline __attribute__((always_inline)) int tmanDelayThreadInternal(SceUInt delay_usec, int use_cb)
{
    if (tmanReadIntrDepth() != 0)
        return (int)SCE_ERROR_KERNEL_CANNOT_BE_CALLED_FROM_INTERRUPT;

    int intr = sceKernelCpuSuspendIntr();
    if (intr == 0 || g_tman.dispatch_suspended != 0) {
        sceKernelCpuResumeIntr(intr);
        return (int)SCE_ERROR_KERNEL_WAIT_CAN_NOT_WAIT;
    }

    tmanCheckKernelStackGuard();
    TmanThreadCB *cur = g_tman.current_thread;
    if (g_tman.log_handler != NULL)
        g_tman.log_handler(use_cb ? 135 : 134, (u32)&g_tman.current_thread, 1, delay_usec);

    u64 abs_time = 0;
    tmanCalcAbsTimeoutUsec(&abs_time, delay_usec);

    for (;;) {
        if (use_cb != 0 && cur->cb_notify_pending != 0)
            tmanCheckPendingCallbacks(intr);

        cur->timeout_ptr = NULL;
        SceUID delay_id = tmanCreateDelayTimer(&abs_time, tmanDelayTimeoutCallback, cur);
        if (delay_id < 0) {
            int ret = (delay_id == (SceUID)SCE_ERROR_KERNEL_WAIT_TIMEOUT) ? 0 : delay_id;
            sceKernelCpuResumeIntr(intr);
            return ret;
        }

        if (use_cb != 0 && cur->cb_notify_pending != 0) {
            tmanCancelDelayTimer(delay_id, NULL);
            cur->timeout_delay_id = 0;
            cur->wait_result = (s32)SCE_ERROR_KERNEL_WAIT_STATUS_RELEASED_CALLBACK;
        } else {
            cur->timeout_delay_id = delay_id;
            cur->wait_param[0] = delay_usec;
            cur->wait_param[1] = 0;
            tmanParkCurrentThread(&g_tman.delayed_list, TMAN_WAIT_DELAY);
            cur->cb_wait_enabled = use_cb;
            g_tman.next_thread = NULL;
            tmanSchedule(1, 1);
        }

        if (use_cb == 0 || cur->wait_result != (s32)SCE_ERROR_KERNEL_WAIT_STATUS_RELEASED_CALLBACK) {
            sceKernelCpuResumeIntr(intr);
            return cur->wait_result;
        }
    }
}

int sceKernelDelayThread(SceUInt delay)
{
    return tmanDelayThreadInternal(delay, 0);
}

int sceKernelDelayThreadCB(SceUInt delay)
{
    return tmanDelayThreadInternal(delay, 1);
}

static inline __attribute__((always_inline)) int tmanDelaySysClockInternal(SceKernelSysClock *delay, int use_cb)
{
    if (tmanReadIntrDepth() != 0)
        return (int)SCE_ERROR_KERNEL_CANNOT_BE_CALLED_FROM_INTERRUPT;
    if (!tmanCheckUserPtr(delay, sizeof(*delay)))
        return (int)SCE_ERROR_KERNEL_ILLEGAL_ADDR;

    int intr = sceKernelCpuSuspendIntr();
    if (intr == 0 || g_tman.dispatch_suspended != 0) {
        sceKernelCpuResumeIntr(intr);
        return (int)SCE_ERROR_KERNEL_WAIT_CAN_NOT_WAIT;
    }

    tmanCheckKernelStackGuard();
    TmanThreadCB *cur = g_tman.current_thread;
    if (g_tman.log_handler != NULL)
        g_tman.log_handler(use_cb ? 137 : 136, (u32)&g_tman.current_thread, 2, delay->hi, delay->low);

    u64 abs_time = 0;
    tmanCalcAbsTimeoutClock(&abs_time, delay);

    for (;;) {
        if (use_cb != 0 && cur->cb_notify_pending != 0)
            tmanCheckPendingCallbacks(intr);

        cur->timeout_ptr = NULL;
        SceUID delay_id = tmanCreateDelayTimer(&abs_time, tmanDelayTimeoutCallback, cur);
        if (delay_id < 0) {
            int ret = (delay_id == (SceUID)SCE_ERROR_KERNEL_WAIT_TIMEOUT) ? 0 : delay_id;
            sceKernelCpuResumeIntr(intr);
            return ret;
        }

        if (use_cb != 0 && cur->cb_notify_pending != 0) {
            tmanCancelDelayTimer(delay_id, NULL);
            cur->timeout_delay_id = 0;
            cur->wait_result = (s32)SCE_ERROR_KERNEL_WAIT_STATUS_RELEASED_CALLBACK;
        } else {
            cur->timeout_delay_id = delay_id;
            cur->wait_param[0] = delay->low;
            cur->wait_param[1] = delay->hi;
            tmanParkCurrentThread(&g_tman.delayed_list, TMAN_WAIT_DELAY);
            cur->cb_wait_enabled = use_cb;
            g_tman.next_thread = NULL;
            tmanSchedule(1, 1);
        }

        if (use_cb == 0 || cur->wait_result != (s32)SCE_ERROR_KERNEL_WAIT_STATUS_RELEASED_CALLBACK) {
            sceKernelCpuResumeIntr(intr);
            return cur->wait_result;
        }
    }
}

int sceKernelDelaySysClockThread(SceKernelSysClock *delay)
{
    return tmanDelaySysClockInternal(delay, 0);
}

int sceKernelDelaySysClockThreadCB(SceKernelSysClock *delay)
{
    return tmanDelaySysClockInternal(delay, 1);
}

void tmanDelayTimeoutCallback(void *arg)
{
    TmanThreadCB *th = (TmanThreadCB *)arg;
    int intr = sceKernelCpuSuspendIntr();

    th->timeout_delay_id = 0;
    if (tmanWakeupWaitingThread(th) > 0)
        g_tman.next_thread = NULL;

    sceKernelCpuResumeIntr(intr);
}
