#include "threadman_internal.h"

/* Forward declarations for UID callbacks */
static int waitqUidCreate(SceSysmemUidCB *cb, SceSysmemUidCB *type, int func_id, void *args);
static int waitqUidDelete(SceSysmemUidCB *cb, SceSysmemUidCB *type, int func_id, void *args);
static int waitqUidCancel(SceSysmemUidCB *cb, SceSysmemUidCB *type, int func_id, void *args);
static int waitqUidCheck(SceSysmemUidCB *cb, SceSysmemUidCB *type, int func_id, void *args);
static int waitqUidRemove(SceSysmemUidCB *cb, SceSysmemUidCB *type, int func_id, void *args);

const TmanUidFunc g_uid_waitq_funcs[6] = {
    { TMAN_UID_FUNC_REMOVE_WAIT, (void *)waitqUidRemove },
    { TMAN_UID_FUNC_CHECK_WAIT,  (void *)waitqUidCheck },
    { TMAN_UID_FUNC_CANCEL_WAIT, (void *)waitqUidCancel },
    { TMAN_UID_FUNC_CREATE,      (void *)waitqUidCreate },
    { TMAN_UID_FUNC_DELETE,      (void *)waitqUidDelete },
    { 0, NULL },
};


int tmanNeedReschedule(void)
{
    if (g_tman.dispatch_suspended != 0)
        return 0;
    if (g_tman.next_thread == g_tman.current_thread)
        return 0;
    g_tman.current_thread->stat_counter_ptr = &g_tman.current_thread->preempt_count;
    return 1;
}

void tmanInsertThreadByPriority(TmanThreadCB *th, SceSysmemUidCB *wait_obj)
{
    TmanWaitQueue *wq = (TmanWaitQueue *)tmanUidData(wait_obj, g_tman.uid_waitq);
    TmanListNode *head = &wq->wait_list;
    TmanListNode *cur = head->next;

    while (cur != head) {
        TmanThreadCB *other = (TmanThreadCB *)cur;
        if (th->current_priority < other->current_priority)
            break;
        cur = cur->next;
    }

    if (wq->skip_first_pri != 0 && cur->prev == head)
        cur = cur->next;

    tmanListInsertBefore(cur, &th->link);
}

int tmanAddThreadToWaitQueue(SceSysmemUidCB *wait_obj, u32 wait_type, u32 attr,
                             u64 *timeout_abs, u32 *timeout_out, u32 *wait_counter)
{
    TmanThreadCB *cur = g_tman.current_thread;
    TmanWaitQueue *wq = (TmanWaitQueue *)tmanUidData(wait_obj, g_tman.uid_waitq);
    SceUID delay_id = 0;

    cur->timeout_delay_id = 0;
    if (timeout_out != NULL) {
        delay_id = tmanCreateDelayTimer(timeout_abs, tmanWaitTimeoutCallback, cur);
        if (delay_id < 0)
            return delay_id;
        cur->timeout_ptr = timeout_out;
        cur->timeout_delay_id = delay_id;
    }

    if (cur->cb_wait_enabled != 0 && cur->cb_notify_pending != 0) {
        cur->cb_wait_enabled = 0;
        if (delay_id != 0) {
            tmanCancelDelayTimer(delay_id, NULL);
            cur->timeout_ptr = NULL;
            cur->timeout_delay_id = 0;
        }
        return (int)SCE_ERROR_KERNEL_WAIT_STATUS_RELEASED_CALLBACK;
    }

    cur->status = TMAN_TH_WAIT;
    cur->wait_type = wait_type;
    cur->wait_obj = wait_obj;
    cur->wait_result = 0;
    wq->num_wait_threads++;

    if ((attr & SCE_KERNEL_AT_THPRI) != 0)
        tmanInsertThreadByPriority(cur, wait_obj);
    else
        tmanListInsertTail(&wq->wait_list, &cur->link);

    if (wait_counter != NULL)
        (*wait_counter)++;

    g_tman.next_thread = NULL;
    tmanSchedule(1, 1);
    return cur->wait_result;
}

int tmanWaitOnObject(SceSysmemUidCB *wait_obj, u32 wait_type, u32 attr,
                     u64 *timeout_abs, u32 *timeout_out)
{
    return tmanAddThreadToWaitQueue(wait_obj, wait_type, attr, timeout_abs, timeout_out, NULL);
}

void tmanParkCurrentThread(TmanListNode *list, u32 wait_type)
{
    TmanThreadCB *cur = g_tman.current_thread;
    TmanListNode *prev = list->prev;
    cur->link.next = list;
    cur->link.prev = prev;
    list->prev = &cur->link;
    cur->status = TMAN_TH_WAIT;
    cur->wait_type = wait_type;
    prev->next = &cur->link;
    cur->wait_obj = NULL;
    cur->wait_result = 0;
}

int tmanWakeupWaitingThread(TmanThreadCB *th)
{
    if (th->status == TMAN_TH_KILLED)
        return 0;

    tmanListRemove(&th->link);
    th->cb_wait_enabled = 0;

    if (th->timeout_delay_id != 0) {
        tmanCancelDelayTimer(th->timeout_delay_id, th->timeout_ptr);
        th->timeout_delay_id = 0;
        th->timeout_ptr = NULL;
    }

    if (th->wait_obj != NULL)
        sceKernelCallUIDObjFunction(th->wait_obj, (s32)TMAN_UID_FUNC_REMOVE_WAIT, th);

    if (th->status == TMAN_TH_WAITSUSPEND) {
        th->status = TMAN_TH_SUSPEND;
        tmanListInsertTail(&g_tman.suspended_list, &th->link);
        return 0;
    }

    th->status = TMAN_TH_READY;
    tmanAddReadyQueue(th);
    return 1;
}

int tmanWakeupAndReschedule(TmanThreadCB *th, int in_intr, int intr_state)
{
    if (th->status == TMAN_TH_KILLED)
        return 0;

    tmanListRemove(&th->link);
    th->cb_wait_enabled = 0;

    if (th->timeout_delay_id != 0) {
        tmanCancelDelayTimer(th->timeout_delay_id, th->timeout_ptr);
        th->timeout_delay_id = 0;
        th->timeout_ptr = NULL;
    }

    if (th->wait_obj != NULL)
        sceKernelCallUIDObjFunction(th->wait_obj, (s32)TMAN_UID_FUNC_REMOVE_WAIT, th);

    if (th->status == TMAN_TH_WAITSUSPEND) {
        th->status = TMAN_TH_SUSPEND;
        tmanListInsertTail(&g_tman.suspended_list, &th->link);
        return 0;
    }

    th->status = TMAN_TH_READY;
    if (in_intr != 0) {
        tmanAddReadyQueue(th);
        g_tman.next_thread = NULL;
    } else {
        tmanMakeReadyAndPreempt(th, intr_state);
    }
    return 1;
}

int tmanWakeupFirstWaiter(SceSysmemUidCB *wait_obj, s32 wait_result, int in_intr, int intr_state)
{
    TmanWaitQueue *wq = (TmanWaitQueue *)tmanUidData(wait_obj, g_tman.uid_waitq);
    TmanThreadCB *th = (TmanThreadCB *)wq->wait_list.next;
    th->wait_result = wait_result;
    return tmanWakeupAndReschedule(th, in_intr, intr_state);
}

int tmanWakeupAllWaiters(SceSysmemUidCB *wait_obj, s32 wait_result)
{
    TmanWaitQueue *wq = (TmanWaitQueue *)tmanUidData(wait_obj, g_tman.uid_waitq);
    TmanListNode *head = &wq->wait_list;
    TmanListNode *cur = head->next;
    int woken = 0;

    while (cur != head) {
        TmanThreadCB *th = (TmanThreadCB *)cur;
        cur = cur->next;
        th->wait_result = wait_result;
        tmanWakeupWaitingThread(th);
        woken++;
    }
    return woken;
}

void tmanClearThreadWait(TmanThreadCB *th)
{
    th->cb_wait_enabled = 0;
    if (th->timeout_delay_id != 0) {
        tmanCancelDelayTimer(th->timeout_delay_id, th->timeout_ptr);
        th->timeout_delay_id = 0;
        th->timeout_ptr = NULL;
    }
    if (th->wait_obj != NULL)
        sceKernelCallUIDObjFunction(th->wait_obj, (s32)TMAN_UID_FUNC_REMOVE_WAIT, th);
}

int tmanNotifyWaitObject(SceSysmemUidCB *wait_obj)
{
    return sceKernelCallUIDObjFunction(wait_obj, (s32)TMAN_UID_FUNC_CHECK_WAIT);
}

void tmanInitWaitQueueType(void)
{
    sceKernelCreateUIDtype("WaitQ", sizeof(TmanWaitQueue),
                           (SceSysmemUidLookupFunc *)g_uid_waitq_funcs, NULL, &g_tman.uid_waitq);
}

void tmanWaitTimeoutCallback(void *arg)
{
    TmanThreadCB *th = (TmanThreadCB *)arg;
    int intr = sceKernelCpuSuspendIntr();

    th->timeout_delay_id = 0;
    if (th->timeout_ptr != NULL)
        *th->timeout_ptr = 0;
    th->timeout_ptr = NULL;
    th->wait_result = (s32)SCE_ERROR_KERNEL_WAIT_TIMEOUT;

    int need_sched = tmanWakeupWaitingThread(th);
    if (th->wait_obj != NULL)
        need_sched += tmanNotifyWaitObject(th->wait_obj);
    if (need_sched != 0)
        g_tman.next_thread = NULL;

    sceKernelCpuResumeIntr(intr);
}

static int waitqUidCreate(SceSysmemUidCB *cb, SceSysmemUidCB *type, int func_id, void *args)
{
    sceKernelCallUIDObjCommonFunction(cb, type, func_id, args);
    TmanWaitQueue *wq = (TmanWaitQueue *)tmanUidData(cb, type);
    wq->skip_first_pri = 0;
    wq->num_wait_threads = 0;
    wq->attr = 0;
    tmanListInit(&wq->wait_list);
    return cb->uid ^ (s32)g_tman.uid_xor_key;
}

static int waitqUidDelete(SceSysmemUidCB *cb, SceSysmemUidCB *type, int func_id, void *args)
{
    int woken = tmanWakeupAllWaiters(cb, (s32)SCE_ERROR_KERNEL_WAIT_DELETE);
    sceKernelCallUIDObjCommonFunction(cb, type, func_id, args);
    if (woken != 0) {
        g_tman.next_thread = NULL;
        tmanSchedule(0, 0);
    }
    return 0;
}

static int waitqUidCancel(SceSysmemUidCB *cb, SceSysmemUidCB *type, int func_id, void *args)
{
    (void)type;
    (void)func_id;
    (void)args;
    int woken = tmanWakeupAllWaiters(cb, (s32)SCE_ERROR_KERNEL_WAIT_CANCELLED);
    if (woken != 0) {
        g_tman.next_thread = NULL;
        tmanSchedule(0, 0);
    }
    return woken;
}

static int waitqUidCheck(SceSysmemUidCB *cb, SceSysmemUidCB *type, int func_id, void *args)
{
    (void)cb;
    (void)type;
    (void)func_id;
    (void)args;
    return 0;
}

static int waitqUidRemove(SceSysmemUidCB *cb, SceSysmemUidCB *type, int func_id, void *args)
{
    TmanWaitQueue *wq = (TmanWaitQueue *)tmanUidData(cb, type);
    (void)func_id;
    (void)args;
    wq->num_wait_threads--;
    return 0;
}

void tmanSchedule(int save_wait_stat, int can_switch)
{
    TmanThreadCB *cur = g_tman.current_thread;
    int reason;

    if (save_wait_stat != 0) {
        cur->stat_counter_ptr = &cur->wait_release_count;
        reason = 2;
    } else {
        cur->stat_counter_ptr = &cur->release_count;
        reason = 1;
    }

    if (can_switch != 0) {
        if (g_tman.dispatch_suspended == 0) {
            u32 *prof_ctrl = (u32 *)tmanReadCfc0(25);
            if (prof_ctrl != NULL) {
                u32 depth = prof_ctrl[2];
                prof_ctrl[2] = depth + 1;
                if (depth == 0) {
                    u32 *hw_reg = (u32 *)prof_ctrl[0];
                    prof_ctrl[1] = *hw_reg;
                    *hw_reg = 0;
                    tmanSync();
                }
            }

            if (SaveThreadContext(cur->ctx) == 0) {
                TmanThreadContext *next_ctx = tmanSwitchContext(cur->ctx, reason);
                InterruptManagerForKernel_551097F2(next_ctx);
            }

            cur->cb_wait_enabled = 0;

            prof_ctrl = (u32 *)tmanReadCfc0(25);
            if (prof_ctrl != NULL) {
                u32 depth = prof_ctrl[2];
                if (depth != 0) {
                    prof_ctrl[2] = depth - 1;
                    if (depth == 1) {
                        u32 *hw_reg = (u32 *)prof_ctrl[0];
                        *hw_reg = prof_ctrl[1];
                        tmanSync();
                    }
                }
            }
        }
    } else {
        u32 cause = tmanMfc0(13);
        tmanMtc0(13, cause | 0x200U);
        sceKernelEnableIntr(65);
    }
}


int tmanWaitQueueHasWaiters(const TmanWaitQueue *wq)
{
    if (tmanListEmpty(&wq->wait_list))
        return 0;
    if ((wq->attr & SCE_KERNEL_AT_THPRI) == 0)
        return 1;
    TmanThreadCB *first = (TmanThreadCB *)wq->wait_list.next;
    return first->current_priority <= g_tman.current_thread->current_priority;
}

