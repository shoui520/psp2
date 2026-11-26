#include "threadman_internal.h"

/* UID callbacks for MsgPipe, VPL, FPL, TLSPL, Timer, Delay, Alarm, VTimer */
static int mppUidCreate(SceSysmemUidCB *cb, SceSysmemUidCB *type, int func_id, void *args);
static int mppUidDelete(SceSysmemUidCB *cb, SceSysmemUidCB *type, int func_id, void *args);
static int mppUidCheck(SceSysmemUidCB *cb, SceSysmemUidCB *type, int func_id, void *args);
static int mppUidRemove(SceSysmemUidCB *cb, SceSysmemUidCB *type, int func_id, void *args);
static int vplUidCreate(SceSysmemUidCB *cb, SceSysmemUidCB *type, int func_id, void *args);
static int vplUidDelete(SceSysmemUidCB *cb, SceSysmemUidCB *type, int func_id, void *args);
static int vplUidCheck(SceSysmemUidCB *cb, SceSysmemUidCB *type, int func_id, void *args);
static int fplUidCreate(SceSysmemUidCB *cb, SceSysmemUidCB *type, int func_id, void *args);
static int fplUidDelete(SceSysmemUidCB *cb, SceSysmemUidCB *type, int func_id, void *args);
static int tlsplUidCreate(SceSysmemUidCB *cb, SceSysmemUidCB *type, int func_id, void *args);
static int tlsplUidDelete(SceSysmemUidCB *cb, SceSysmemUidCB *type, int func_id, void *args);
static int timerUidCreate(SceSysmemUidCB *cb, SceSysmemUidCB *type, int func_id, void *args);
static int timerUidDelete(SceSysmemUidCB *cb, SceSysmemUidCB *type, int func_id, void *args);
static int delayUidCreate(SceSysmemUidCB *cb, SceSysmemUidCB *type, int func_id, void *args);
static int delayUidDelete(SceSysmemUidCB *cb, SceSysmemUidCB *type, int func_id, void *args);
static int alarmUidCreate(SceSysmemUidCB *cb, SceSysmemUidCB *type, int func_id, void *args);
static int alarmUidDelete(SceSysmemUidCB *cb, SceSysmemUidCB *type, int func_id, void *args);
static int vtimerUidCreate(SceSysmemUidCB *cb, SceSysmemUidCB *type, int func_id, void *args);
static int vtimerUidDelete(SceSysmemUidCB *cb, SceSysmemUidCB *type, int func_id, void *args);

const TmanUidFunc g_uid_msgpipe_funcs[5] = {
    { TMAN_UID_FUNC_REMOVE_WAIT, (void *)mppUidRemove },
    { TMAN_UID_FUNC_CHECK_WAIT,  (void *)mppUidCheck },
    { TMAN_UID_FUNC_CREATE,      (void *)mppUidCreate },
    { TMAN_UID_FUNC_DELETE,      (void *)mppUidDelete },
    { 0, NULL },
};

const TmanUidFunc g_uid_vpl_funcs[4] = {
    { TMAN_UID_FUNC_CHECK_WAIT, (void *)vplUidCheck },
    { TMAN_UID_FUNC_CREATE,     (void *)vplUidCreate },
    { TMAN_UID_FUNC_DELETE,     (void *)vplUidDelete },
    { 0, NULL },
};

const TmanUidFunc g_uid_fpl_funcs[3] = {
    { TMAN_UID_FUNC_CREATE, (void *)fplUidCreate },
    { TMAN_UID_FUNC_DELETE, (void *)fplUidDelete },
    { 0, NULL },
};

const TmanUidFunc g_uid_tlspl_funcs[3] = {
    { TMAN_UID_FUNC_CREATE, (void *)tlsplUidCreate },
    { TMAN_UID_FUNC_DELETE, (void *)tlsplUidDelete },
    { 0, NULL },
};

const TmanUidFunc g_uid_delay_funcs[3] = {
    { TMAN_UID_FUNC_CREATE, (void *)delayUidCreate },
    { TMAN_UID_FUNC_DELETE, (void *)delayUidDelete },
    { 0, NULL },
};

const TmanUidFunc g_uid_alarm_funcs[3] = {
    { TMAN_UID_FUNC_CREATE, (void *)alarmUidCreate },
    { TMAN_UID_FUNC_DELETE, (void *)alarmUidDelete },
    { 0, NULL },
};

const TmanUidFunc g_uid_vtimer_funcs[3] = {
    { TMAN_UID_FUNC_CREATE, (void *)vtimerUidCreate },
    { TMAN_UID_FUNC_DELETE, (void *)vtimerUidDelete },
    { 0, NULL },
};

const TmanUidFunc g_uid_timer_funcs[3] = {
    { TMAN_UID_FUNC_CREATE, (void *)timerUidCreate },
    { TMAN_UID_FUNC_DELETE, (void *)timerUidDelete },
    { 0, NULL },
};

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

/* ============================================================================
 * Variable-size Memory Pools (VPL)
 * ============================================================================ */

void tmanInitVplType(void)
{
    sceKernelCreateUIDtypeInherit("WaitQ", "Vpl", 48,
                                  (SceSysmemUidLookupFunc *)g_uid_vpl_funcs,
                                  NULL, &g_tman.uid_vpl);
}

SceUID tmanFindVplByMemBlock(SceUID block_id)
{
    SceSysmemUidCB *head = g_tman.uid_vpl;
    SceSysmemUidCB *cb = head->PARENT0;
    while (cb != head) {
        TmanVplCB *vpl = (TmanVplCB *)tmanUidData(cb, head);
        if (vpl->mem_id == block_id || vpl->heap_id == block_id)
            return cb->uid ^ (s32)g_tman.uid_xor_key;
        cb = cb->PARENT0;
    }
    return 0;
}

static int vplUidCreate(SceSysmemUidCB *cb, SceSysmemUidCB *type, int func_id, void *args)
{
    sceKernelCallUIDObjCommonFunction(cb, type, func_id, args);
    return cb->uid ^ (s32)g_tman.uid_xor_key;
}

static int vplUidDelete(SceSysmemUidCB *cb, SceSysmemUidCB *type, int func_id, void *args)
{
    TmanVplCB *vpl = (TmanVplCB *)tmanUidData(cb, type);
    if (vpl->heap_id > 0) {
        sceKernelDeleteHeap(vpl->heap_id);
        vpl->heap_id = 0;
    }
    sceKernelCallUIDObjCommonFunction(cb, type, func_id, args);
    return 0;
}

static int vplUidCheck(SceSysmemUidCB *cb, SceSysmemUidCB *type, int func_id, void *args)
{
    (void)type;
    (void)func_id;
    (void)args;
    TmanWaitQueue *wq = (TmanWaitQueue *)tmanUidData(cb, g_tman.uid_waitq);
    TmanVplCB *vpl = (TmanVplCB *)tmanUidData(cb, g_tman.uid_vpl);
    TmanListNode *head = &wq->wait_list;
    TmanListNode *node = head->next;
    int woken = 0;

    while (node != head) {
        TmanThreadCB *th = (TmanThreadCB *)node;
        node = node->next;
        void *mem = sceKernelAllocHeapMemory(vpl->heap_id, th->wait_param[0]);
        if (mem == NULL)
            break;
        void **out = (void **)th->wait_param[1];
        if (out != NULL)
            *out = mem;
        woken += tmanWakeupWaitingThread(th);
    }
    return woken;
}

SceUID sceKernelCreateVpl(const char *name, int part, int attr, unsigned int size,
                          SceKernelVplOptParam *opt)
{
    if (!tmanCheckUserAddr((u32)name) || !tmanCheckUserPtr(opt, sizeof(*opt)))
        return (SceUID)SCE_ERROR_KERNEL_ILLEGAL_ADDR;
    if (tmanReadIntrDepth() != 0)
        return (SceUID)SCE_ERROR_KERNEL_CANNOT_BE_CALLED_FROM_INTERRUPT;
    if ((attr & ~0x41FF) != 0)
        return (SceUID)SCE_ERROR_KERNEL_ILLEGAL_ATTR;
    if (size < 32)
        return (SceUID)SCE_ERROR_KERNEL_ILLEGAL_MEMSIZE;

    int is_user = tmanIsUserCaller();
    int intr = sceKernelCpuSuspendIntr();
    int heap_attr = ((attr & 0x4000) != 0) ? 2 : 0;
    SceUID heap_id = sceKernelCreateHeap(part, size, heap_attr, name);
    if (heap_id < 0) {
        sceKernelCpuResumeIntr(intr);
        return (SceUID)SCE_ERROR_KERNEL_NO_MEMORY;
    }

    SceSysmemUidCB *cb = NULL;
    int ret = sceKernelCreateUID(g_tman.uid_vpl, name,
                                 (char)((attr & 0xFF) | (is_user ? 0xFF : 0x00)), &cb);
    if (ret != 0) {
        sceKernelDeleteHeap(heap_id);
        sceKernelCpuResumeIntr(intr);
        return ret;
    }

    TmanWaitQueue *wq = (TmanWaitQueue *)tmanUidData(cb, g_tman.uid_waitq);
    TmanVplCB *vpl = (TmanVplCB *)tmanUidData(cb, g_tman.uid_vpl);
    SceUID uid = cb->uid ^ (s32)g_tman.uid_xor_key;
    wq->attr = (u32)attr;
    vpl->opt = opt;
    vpl->mem_id = heap_id;
    vpl->heap_id = heap_id;
    vpl->pool = NULL;
    vpl->pool_size = size;
    vpl->attr = (u32)attr;

    sceKernelCpuResumeIntr(intr);
    return uid;
}

int sceKernelDeleteVpl(SceUID uid)
{
    if (tmanReadIntrDepth() != 0)
        return (int)SCE_ERROR_KERNEL_CANNOT_BE_CALLED_FROM_INTERRUPT;

    int intr = sceKernelCpuSuspendIntr();
    SceSysmemUidCB *cb = NULL;
    if (sceKernelGetUIDcontrolBlockWithType(uid, g_tman.uid_vpl, &cb) != 0) {
        sceKernelCpuResumeIntr(intr);
        return (int)SCE_ERROR_KERNEL_NOT_FOUND_VPOOL;
    }
    if (!tmanCheckUidAttr(cb, 0x10)) {
        sceKernelCpuResumeIntr(intr);
        return (int)SCE_ERROR_KERNEL_ILLEGAL_PERMISSION;
    }

    sceKernelDeleteUID(uid);
    sceKernelCpuResumeIntr(intr);
    return 0;
}

static inline __attribute__((always_inline)) int tmanAllocVplInternal(SceUID uid, unsigned int size, void **data,
                                unsigned int *timeout, int is_try, int use_cb)
{
    if (!tmanCheckUserPtr(data, sizeof(void *)) || !tmanCheckUserPtr(timeout, sizeof(u32)))
        return (int)SCE_ERROR_KERNEL_ILLEGAL_ADDR;
    if (!is_try && tmanReadIntrDepth() != 0)
        return (int)SCE_ERROR_KERNEL_CANNOT_BE_CALLED_FROM_INTERRUPT;
    if (size == 0)
        return (int)SCE_ERROR_KERNEL_ILLEGAL_MEMSIZE;

    int intr = sceKernelCpuSuspendIntr();
    if (!is_try && (intr == 0 || g_tman.dispatch_suspended != 0)) {
        sceKernelCpuResumeIntr(intr);
        return (int)SCE_ERROR_KERNEL_WAIT_CAN_NOT_WAIT;
    }

    SceSysmemUidCB *cb = NULL;
    if (sceKernelGetUIDcontrolBlockWithType(uid, g_tman.uid_vpl, &cb) != 0) {
        sceKernelCpuResumeIntr(intr);
        return (int)SCE_ERROR_KERNEL_NOT_FOUND_VPOOL;
    }
    if (!tmanCheckUidAttr(cb, 4)) {
        sceKernelCpuResumeIntr(intr);
        return (int)SCE_ERROR_KERNEL_ILLEGAL_PERMISSION;
    }

    TmanWaitQueue *wq = (TmanWaitQueue *)tmanUidData(cb, g_tman.uid_waitq);
    TmanVplCB *vpl = (TmanVplCB *)tmanUidData(cb, g_tman.uid_vpl);
    if (size > vpl->pool_size) {
        sceKernelCpuResumeIntr(intr);
        return (int)SCE_ERROR_KERNEL_ILLEGAL_MEMSIZE;
    }

    u64 abs_time = 0;
    if (timeout != NULL)
        tmanCalcAbsTimeoutUsec(&abs_time, *timeout);

    TmanThreadCB *cur = g_tman.current_thread;
    int ret;
    for (;;) {
        if (use_cb != 0 && cur->cb_notify_pending != 0)
            tmanCheckPendingCallbacks(intr);

        if (!tmanWaitQueueHasWaiters(wq)) {
            void *mem = sceKernelAllocHeapMemory(vpl->heap_id, size);
            if (mem != NULL) {
                if (data != NULL)
                    *data = mem;
                ret = 0;
                break;
            }
        }

        if (is_try) {
            ret = (int)SCE_ERROR_KERNEL_NO_MEMORY;
            break;
        }

        cur->wait_param[0] = size;
        cur->wait_param[1] = (u32)data;
        cur->cb_wait_enabled = use_cb;
        ret = tmanWaitOnObject(cb, TMAN_WAIT_VPL, wq->attr, &abs_time, timeout);
        if (use_cb == 0 || ret != (int)SCE_ERROR_KERNEL_WAIT_STATUS_RELEASED_CALLBACK)
            break;
    }

    sceKernelCpuResumeIntr(intr);
    return ret;
}

int sceKernelAllocateVpl(SceUID uid, unsigned int size, void **data, unsigned int *timeout)
{
    return tmanAllocVplInternal(uid, size, data, timeout, 0, 0);
}

int sceKernelAllocateVplCB(SceUID uid, unsigned int size, void **data, unsigned int *timeout)
{
    return tmanAllocVplInternal(uid, size, data, timeout, 0, 1);
}

int sceKernelTryAllocateVpl(SceUID uid, unsigned int size, void **data)
{
    return tmanAllocVplInternal(uid, size, data, NULL, 1, 0);
}

int sceKernelFreeVpl(SceUID uid, void *data)
{
    if (!tmanCheckUserAddr((u32)data))
        return (int)SCE_ERROR_KERNEL_ILLEGAL_ADDR;

    int intr = sceKernelCpuSuspendIntr();
    SceSysmemUidCB *cb = NULL;
    if (sceKernelGetUIDcontrolBlockWithType(uid, g_tman.uid_vpl, &cb) != 0) {
        sceKernelCpuResumeIntr(intr);
        return (int)SCE_ERROR_KERNEL_NOT_FOUND_VPOOL;
    }
    if (!tmanCheckUidAttr(cb, 2)) {
        sceKernelCpuResumeIntr(intr);
        return (int)SCE_ERROR_KERNEL_ILLEGAL_PERMISSION;
    }

    TmanVplCB *vpl = (TmanVplCB *)tmanUidData(cb, g_tman.uid_vpl);
    int ret = sceKernelFreeHeapMemory(vpl->heap_id, data);
    if (ret < 0) {
        sceKernelCpuResumeIntr(intr);
        return (int)SCE_ERROR_KERNEL_ILLEGAL_MEMBLOCK;
    }

    if (vplUidCheck(cb, g_tman.uid_vpl, 0, NULL) > 0) {
        g_tman.next_thread = NULL;
        if (tmanReadIntrDepth() == 0)
            tmanSchedule(0, intr);
    }

    sceKernelCpuResumeIntr(intr);
    return 0;
}

int sceKernelCancelVpl(SceUID uid, int *pnum)
{
    if (!tmanCheckUserPtr(pnum, sizeof(int)))
        return (int)SCE_ERROR_KERNEL_ILLEGAL_ADDR;

    int intr = sceKernelCpuSuspendIntr();
    SceSysmemUidCB *cb = NULL;
    if (sceKernelGetUIDcontrolBlockWithType(uid, g_tman.uid_vpl, &cb) != 0) {
        sceKernelCpuResumeIntr(intr);
        return (int)SCE_ERROR_KERNEL_NOT_FOUND_VPOOL;
    }
    if (!tmanCheckUidAttr(cb, 8)) {
        sceKernelCpuResumeIntr(intr);
        return (int)SCE_ERROR_KERNEL_ILLEGAL_PERMISSION;
    }

    int woken = sceKernelCallUIDObjFunction(cb, (s32)TMAN_UID_FUNC_CANCEL_WAIT);
    if (pnum != NULL)
        *pnum = woken;

    sceKernelCpuResumeIntr(intr);
    return 0;
}

int sceKernelReferVplStatus(SceUID uid, SceKernelVplInfo *info)
{
    if (!tmanCheckUserPtr(info, sizeof(*info)))
        return (int)SCE_ERROR_KERNEL_ILLEGAL_ADDR;

    int intr = sceKernelCpuSuspendIntr();
    SceSysmemUidCB *cb = NULL;
    if (sceKernelGetUIDcontrolBlockWithType(uid, g_tman.uid_vpl, &cb) != 0) {
        sceKernelCpuResumeIntr(intr);
        return (int)SCE_ERROR_KERNEL_NOT_FOUND_VPOOL;
    }
    if (!tmanCheckUidAttr(cb, 1)) {
        sceKernelCpuResumeIntr(intr);
        return (int)SCE_ERROR_KERNEL_ILLEGAL_PERMISSION;
    }

    TmanWaitQueue *wq = (TmanWaitQueue *)tmanUidData(cb, g_tman.uid_waitq);
    TmanVplCB *vpl = (TmanVplCB *)tmanUidData(cb, g_tman.uid_vpl);
    SceKernelVplInfo local;
    memset(&local, 0, sizeof(local));
    local.size = sizeof(local);
    if (cb->name != NULL)
        strncpy(local.name, cb->name, SCE_UID_NAME_LEN);
    local.attr = wq->attr;
    local.poolSize = (int)vpl->pool_size;
    local.freeSize = (int)sceKernelHeapTotalFreeSize(vpl->heap_id);
    local.numWaitThreads = wq->num_wait_threads;

    SceSize copy_size = info->size < local.size ? info->size : local.size;
    memcpy(info, &local, copy_size);
    sceKernelCpuResumeIntr(intr);
    return 0;
}

/* ============================================================================
 * Fixed-size Memory Pools (FPL)
 * ============================================================================ */

void tmanInitFplType(void)
{
    sceKernelCreateUIDtypeInherit("WaitQ", "Fpl", 48,
                                  (SceSysmemUidLookupFunc *)g_uid_fpl_funcs,
                                  NULL, &g_tman.uid_fpl);
}

SceUID tmanFindFplByMemBlock(SceUID block_id)
{
    SceSysmemUidCB *head = g_tman.uid_fpl;
    SceSysmemUidCB *cb = head->PARENT0;
    while (cb != head) {
        TmanFplCB *fpl = (TmanFplCB *)tmanUidData(cb, head);
        if (fpl->mem_id == block_id)
            return cb->uid ^ (s32)g_tman.uid_xor_key;
        cb = cb->PARENT0;
    }
    return 0;
}

static int fplUidCreate(SceSysmemUidCB *cb, SceSysmemUidCB *type, int func_id, void *args)
{
    sceKernelCallUIDObjCommonFunction(cb, type, func_id, args);
    return cb->uid ^ (s32)g_tman.uid_xor_key;
}

static int fplUidDelete(SceSysmemUidCB *cb, SceSysmemUidCB *type, int func_id, void *args)
{
    TmanFplCB *fpl = (TmanFplCB *)tmanUidData(cb, type);
    if (fpl->link_table != NULL) {
        sceKernelFreeHeapMemory(g_tman.heap_id, fpl->link_table);
        fpl->link_table = NULL;
    }
    if (fpl->mem_id > 0) {
        sceKernelFreePartitionMemory(fpl->mem_id);
        fpl->mem_id = 0;
    }
    sceKernelCallUIDObjCommonFunction(cb, type, func_id, args);
    return 0;
}

int tmanFplFreeBlock(TmanFplCB *fpl, s32 idx)
{
    void **slot = &fpl->link_table[idx];
    if (*slot != NULL)
        return -1;
    void **tail = fpl->free_tail;
    fpl->free_blocks++;
    if (tail != NULL) {
        void *head = *tail;
        fpl->free_tail = slot;
        *slot = head;
        *tail = slot;
    } else {
        fpl->free_tail = slot;
        *slot = slot;
    }
    return 0;
}

int tmanFplAllocBlock(TmanFplCB *fpl)
{
    void **tail = fpl->free_tail;
    if (tail == NULL)
        return -1;
    void **head = (void **)*tail;
    void **next_tail = tail;
    fpl->free_blocks--;
    if (tail == head)
        next_tail = NULL;
    else
        *tail = *head;
    fpl->free_tail = next_tail;
    *head = NULL;
    return (int)(head - fpl->link_table);
}

int sceKernelCreateFpl(const char *name, int part, int attr, unsigned int block_size,
                       unsigned int blocks, SceKernelFplOptParam *opt)
{
    if (!tmanCheckUserAddr((u32)name) || !tmanCheckUserPtr(opt, sizeof(*opt)))
        return (int)SCE_ERROR_KERNEL_ILLEGAL_ADDR;
    if (tmanReadIntrDepth() != 0)
        return (int)SCE_ERROR_KERNEL_CANNOT_BE_CALLED_FROM_INTERRUPT;
    if ((attr & ~0x41FF) != 0)
        return (int)SCE_ERROR_KERNEL_ILLEGAL_ATTR;
    if (block_size == 0 || blocks == 0)
        return (int)SCE_ERROR_KERNEL_ILLEGAL_MEMSIZE;

    u32 aligned_size = (block_size + 3U) & ~3U;
    u32 total_size = aligned_size * blocks;
    if (total_size / blocks != aligned_size)
        return (int)SCE_ERROR_KERNEL_ILLEGAL_MEMSIZE;

    int is_user = tmanIsUserCaller();
    int intr = sceKernelCpuSuspendIntr();
    void **link_table = (void **)sceKernelAllocHeapMemory(g_tman.heap_id, blocks * sizeof(void *));
    if (link_table == NULL) {
        sceKernelCpuResumeIntr(intr);
        return (int)SCE_ERROR_KERNEL_NO_MEMORY;
    }
    memset(link_table, 0, blocks * sizeof(void *));

    int alloc_type = ((attr & 0x4000) != 0) ? 1 : 0;
    SceUID mem_id = sceKernelAllocPartitionMemory(part, name, alloc_type, total_size, NULL);
    if (mem_id < 0) {
        sceKernelFreeHeapMemory(g_tman.heap_id, link_table);
        sceKernelCpuResumeIntr(intr);
        return (int)SCE_ERROR_KERNEL_NO_MEMORY;
    }

    SceSysmemUidCB *cb = NULL;
    int ret = sceKernelCreateUID(g_tman.uid_fpl, name,
                                 (char)((attr & 0xFF) | (is_user ? 0xFF : 0x00)), &cb);
    if (ret != 0) {
        sceKernelFreePartitionMemory(mem_id);
        sceKernelFreeHeapMemory(g_tman.heap_id, link_table);
        sceKernelCpuResumeIntr(intr);
        return ret;
    }

    TmanWaitQueue *wq = (TmanWaitQueue *)tmanUidData(cb, g_tman.uid_waitq);
    TmanFplCB *fpl = (TmanFplCB *)tmanUidData(cb, g_tman.uid_fpl);
    SceUID uid = cb->uid ^ (s32)g_tman.uid_xor_key;
    wq->attr = (u32)attr;
    fpl->opt = opt;
    fpl->mem_id = mem_id;
    fpl->pool = sceKernelGetBlockHeadAddr(mem_id);
    fpl->block_size = aligned_size;
    fpl->num_blocks = blocks;
    fpl->free_blocks = 0;
    fpl->free_tail = NULL;
    fpl->link_table = link_table;
    fpl->attr = (u32)attr;

    u32 idx;
    for (idx = 0; idx < blocks; idx++)
        tmanFplFreeBlock(fpl, (s32)idx);

    sceKernelCpuResumeIntr(intr);
    return uid;
}

int sceKernelDeleteFpl(SceUID uid)
{
    if (tmanReadIntrDepth() != 0)
        return (int)SCE_ERROR_KERNEL_CANNOT_BE_CALLED_FROM_INTERRUPT;

    int intr = sceKernelCpuSuspendIntr();
    SceSysmemUidCB *cb = NULL;
    if (sceKernelGetUIDcontrolBlockWithType(uid, g_tman.uid_fpl, &cb) != 0) {
        sceKernelCpuResumeIntr(intr);
        return (int)SCE_ERROR_KERNEL_NOT_FOUND_FPOOL;
    }
    if (!tmanCheckUidAttr(cb, 0x10)) {
        sceKernelCpuResumeIntr(intr);
        return (int)SCE_ERROR_KERNEL_ILLEGAL_PERMISSION;
    }

    sceKernelDeleteUID(uid);
    sceKernelCpuResumeIntr(intr);
    return 0;
}

static inline __attribute__((always_inline)) int tmanAllocFplInternal(SceUID uid, void **data, unsigned int *timeout,
                                int is_try, int use_cb)
{
    if (!tmanCheckUserPtr(data, sizeof(void *)) || !tmanCheckUserPtr(timeout, sizeof(u32)))
        return (int)SCE_ERROR_KERNEL_ILLEGAL_ADDR;
    if (!is_try && tmanReadIntrDepth() != 0)
        return (int)SCE_ERROR_KERNEL_CANNOT_BE_CALLED_FROM_INTERRUPT;

    int intr = sceKernelCpuSuspendIntr();
    if (!is_try && (intr == 0 || g_tman.dispatch_suspended != 0)) {
        sceKernelCpuResumeIntr(intr);
        return (int)SCE_ERROR_KERNEL_WAIT_CAN_NOT_WAIT;
    }

    SceSysmemUidCB *cb = NULL;
    if (sceKernelGetUIDcontrolBlockWithType(uid, g_tman.uid_fpl, &cb) != 0) {
        sceKernelCpuResumeIntr(intr);
        return (int)SCE_ERROR_KERNEL_NOT_FOUND_FPOOL;
    }
    if (!tmanCheckUidAttr(cb, 4)) {
        sceKernelCpuResumeIntr(intr);
        return (int)SCE_ERROR_KERNEL_ILLEGAL_PERMISSION;
    }

    TmanWaitQueue *wq = (TmanWaitQueue *)tmanUidData(cb, g_tman.uid_waitq);
    TmanFplCB *fpl = (TmanFplCB *)tmanUidData(cb, g_tman.uid_fpl);
    u64 abs_time = 0;
    if (timeout != NULL)
        tmanCalcAbsTimeoutUsec(&abs_time, *timeout);

    TmanThreadCB *cur = g_tman.current_thread;
    int ret;
    for (;;) {
        if (use_cb != 0 && cur->cb_notify_pending != 0)
            tmanCheckPendingCallbacks(intr);

        int idx = tmanFplAllocBlock(fpl);
        if (idx >= 0) {
            if (data != NULL)
                *data = (u8 *)fpl->pool + (u32)idx * fpl->block_size;
            ret = 0;
            break;
        }

        if (is_try) {
            ret = (int)SCE_ERROR_KERNEL_NO_MEMORY;
            break;
        }

        cur->wait_param[0] = (u32)data;
        cur->cb_wait_enabled = use_cb;
        ret = tmanWaitOnObject(cb, TMAN_WAIT_FPL, wq->attr, &abs_time, timeout);
        if (use_cb == 0 || ret != (int)SCE_ERROR_KERNEL_WAIT_STATUS_RELEASED_CALLBACK)
            break;
    }

    sceKernelCpuResumeIntr(intr);
    return ret;
}

int sceKernelAllocateFpl(SceUID uid, void **data, unsigned int *timeout)
{
    return tmanAllocFplInternal(uid, data, timeout, 0, 0);
}

int sceKernelAllocateFplCB(SceUID uid, void **data, unsigned int *timeout)
{
    return tmanAllocFplInternal(uid, data, timeout, 0, 1);
}

int sceKernelTryAllocateFpl(SceUID uid, void **data)
{
    return tmanAllocFplInternal(uid, data, NULL, 1, 0);
}

int sceKernelFreeFpl(SceUID uid, void *data)
{
    if (!tmanCheckUserAddr((u32)data))
        return (int)SCE_ERROR_KERNEL_ILLEGAL_ADDR;

    int intr = sceKernelCpuSuspendIntr();
    int in_intr = (int)tmanReadIntrDepth();
    SceSysmemUidCB *cb = NULL;
    if (sceKernelGetUIDcontrolBlockWithType(uid, g_tman.uid_fpl, &cb) != 0) {
        sceKernelCpuResumeIntr(intr);
        return (int)SCE_ERROR_KERNEL_NOT_FOUND_FPOOL;
    }
    if (!tmanCheckUidAttr(cb, 2)) {
        sceKernelCpuResumeIntr(intr);
        return (int)SCE_ERROR_KERNEL_ILLEGAL_PERMISSION;
    }

    TmanWaitQueue *wq = (TmanWaitQueue *)tmanUidData(cb, g_tman.uid_waitq);
    TmanFplCB *fpl = (TmanFplCB *)tmanUidData(cb, g_tman.uid_fpl);
    u32 offset = (u32)((u8 *)data - (u8 *)fpl->pool);
    if ((u8 *)data < (u8 *)fpl->pool || (offset % fpl->block_size) != 0) {
        sceKernelCpuResumeIntr(intr);
        return (int)SCE_ERROR_KERNEL_ILLEGAL_MEMBLOCK;
    }
    u32 idx = offset / fpl->block_size;
    if (idx >= fpl->num_blocks || fpl->link_table[idx] != NULL) {
        sceKernelCpuResumeIntr(intr);
        return (int)SCE_ERROR_KERNEL_ILLEGAL_MEMBLOCK;
    }

    if (!tmanListEmpty(&wq->wait_list)) {
        TmanThreadCB *waiter = (TmanThreadCB *)wq->wait_list.next;
        void **out = (void **)waiter->wait_param[0];
        if (out != NULL)
            *out = data;
        tmanWakeupFirstWaiter(cb, 0, in_intr, intr);
    } else {
        tmanFplFreeBlock(fpl, (s32)idx);
    }

    sceKernelCpuResumeIntr(intr);
    return 0;
}

int sceKernelCancelFpl(SceUID uid, int *pnum)
{
    if (!tmanCheckUserPtr(pnum, sizeof(int)))
        return (int)SCE_ERROR_KERNEL_ILLEGAL_ADDR;

    int intr = sceKernelCpuSuspendIntr();
    SceSysmemUidCB *cb = NULL;
    if (sceKernelGetUIDcontrolBlockWithType(uid, g_tman.uid_fpl, &cb) != 0) {
        sceKernelCpuResumeIntr(intr);
        return (int)SCE_ERROR_KERNEL_NOT_FOUND_FPOOL;
    }
    if (!tmanCheckUidAttr(cb, 8)) {
        sceKernelCpuResumeIntr(intr);
        return (int)SCE_ERROR_KERNEL_ILLEGAL_PERMISSION;
    }

    int woken = sceKernelCallUIDObjFunction(cb, (s32)TMAN_UID_FUNC_CANCEL_WAIT);
    if (pnum != NULL)
        *pnum = woken;

    sceKernelCpuResumeIntr(intr);
    return 0;
}

int sceKernelReferFplStatus(SceUID uid, SceKernelFplInfo *info)
{
    if (!tmanCheckUserPtr(info, sizeof(*info)))
        return (int)SCE_ERROR_KERNEL_ILLEGAL_ADDR;

    int intr = sceKernelCpuSuspendIntr();
    SceSysmemUidCB *cb = NULL;
    if (sceKernelGetUIDcontrolBlockWithType(uid, g_tman.uid_fpl, &cb) != 0) {
        sceKernelCpuResumeIntr(intr);
        return (int)SCE_ERROR_KERNEL_NOT_FOUND_FPOOL;
    }
    if (!tmanCheckUidAttr(cb, 1)) {
        sceKernelCpuResumeIntr(intr);
        return (int)SCE_ERROR_KERNEL_ILLEGAL_PERMISSION;
    }

    TmanWaitQueue *wq = (TmanWaitQueue *)tmanUidData(cb, g_tman.uid_waitq);
    TmanFplCB *fpl = (TmanFplCB *)tmanUidData(cb, g_tman.uid_fpl);
    SceKernelFplInfo local;
    memset(&local, 0, sizeof(local));
    local.size = sizeof(local);
    if (cb->name != NULL)
        strncpy(local.name, cb->name, SCE_UID_NAME_LEN);
    local.attr = wq->attr;
    local.blockSize = (int)fpl->block_size;
    local.numBlocks = (int)fpl->num_blocks;
    local.freeBlocks = (int)fpl->free_blocks;
    local.numWaitThreads = wq->num_wait_threads;

    SceSize copy_size = info->size < local.size ? info->size : local.size;
    memcpy(info, &local, copy_size);
    sceKernelCpuResumeIntr(intr);
    return 0;
}

/* ============================================================================
 * Thread Local Storage Pools (TLSPL)
 * ============================================================================ */

void tmanInitTlsplType(void)
{
    sceKernelCreateUIDtypeInherit("WaitQ", "Tlspl", 48,
                                  (SceSysmemUidLookupFunc *)g_uid_tlspl_funcs,
                                  NULL, &g_tman.uid_tlspl);
}

SceUID tmanFindTlsplByMemBlock(SceUID block_id)
{
    SceSysmemUidCB *head = g_tman.uid_tlspl;
    SceSysmemUidCB *cb = head->PARENT0;
    while (cb != head) {
        TmanTlsplCB *tls = (TmanTlsplCB *)tmanUidData(cb, head);
        if (tls->mem_id == block_id)
            return cb->uid ^ (s32)g_tman.uid_xor_key;
        cb = cb->PARENT0;
    }
    return 0;
}

static int tlsplUidCreate(SceSysmemUidCB *cb, SceSysmemUidCB *type, int func_id, void *args)
{
    sceKernelCallUIDObjCommonFunction(cb, type, func_id, args);
    return cb->uid ^ (s32)g_tman.uid_xor_key;
}

static int tlsplUidDelete(SceSysmemUidCB *cb, SceSysmemUidCB *type, int func_id, void *args)
{
    TmanTlsplCB *tls = (TmanTlsplCB *)tmanUidData(cb, type);
    if (tls->link_table != NULL) {
        sceKernelFreeHeapMemory(g_tman.heap_id, tls->link_table);
        tls->link_table = NULL;
    }
    if (tls->mem_id > 0) {
        sceKernelFreePartitionMemory(tls->mem_id);
        tls->mem_id = 0;
    }
    sceKernelCallUIDObjCommonFunction(cb, type, func_id, args);
    return 0;
}

int tmanTlsplFreeBlock(TmanTlsplCB *tls, s32 idx)
{
    void **slot = &tls->link_table[idx];
    if (*slot != NULL)
        return -1;
    void **tail = tls->free_tail;
    tls->free_blocks++;
    if (tail != NULL) {
        void *head = *tail;
        tls->free_tail = slot;
        *slot = head;
        *tail = slot;
    } else {
        tls->free_tail = slot;
        *slot = slot;
    }
    return 0;
}

int tmanTlsplAllocBlock(TmanTlsplCB *tls)
{
    void **tail = tls->free_tail;
    if (tail == NULL)
        return -1;
    void **head = (void **)*tail;
    void **next_tail = tail;
    tls->free_blocks--;
    if (tail == head)
        next_tail = NULL;
    else
        *tail = *head;
    tls->free_tail = next_tail;
    *head = NULL;
    return (int)(head - tls->link_table);
}

int tmanTlsplValidateBlock(TmanTlsplCB *tls, void *ptr)
{
    u32 addr = (u32)ptr;
    u32 base = (u32)tls->pool;
    if (addr < base)
        return (int)SCE_ERROR_KERNEL_ILLEGAL_MEMBLOCK;
    u32 size = tls->block_size;
    if (addr >= base + size * tls->num_blocks)
        return (int)SCE_ERROR_KERNEL_ILLEGAL_MEMBLOCK;
    u32 idx = (addr - base) / size;
    if (addr != base + size * idx)
        return (int)SCE_ERROR_KERNEL_ILLEGAL_MEMBLOCK;
    if (tls->link_table[idx] != NULL)
        return (int)SCE_ERROR_KERNEL_ILLEGAL_MEMBLOCK;
    return (int)idx;
}

void tmanFreeThreadTlspl(TmanThreadCB *th)
{
    if (th->user_tls == NULL)
        return;
    int intr = sceKernelCpuSuspendIntr();
    u32 *slots = (u32 *)th->user_tls;
    int idx;
    for (idx = 0; idx < 16; idx++) {
        SceUID uid = (SceUID)slots[16 + idx];
        slots[16 + idx] = 0;
        if (uid == 0)
            continue;
        SceSysmemUidCB *cb = NULL;
        if (sceKernelGetUIDcontrolBlockWithType(uid, g_tman.uid_tlspl, &cb) == 0) {
            TmanTlsplCB *tls = (TmanTlsplCB *)tmanUidData(cb, g_tman.uid_tlspl);
            TmanWaitQueue *wq = (TmanWaitQueue *)tmanUidData(cb, g_tman.uid_waitq);
            void *blk = (void *)slots[idx];
            int slot_idx = tmanTlsplValidateBlock(tls, blk);
            if (slot_idx >= 0) {
                if (!tmanListEmpty(&wq->wait_list)) {
                    TmanThreadCB *waiter = (TmanThreadCB *)wq->wait_list.next;
                    void **out = (void **)waiter->wait_param[0];
                    if (out != NULL)
                        *out = blk;
                    tmanWakeupFirstWaiter(cb, 0, (int)tmanReadIntrDepth(), intr);
                } else {
                    tmanTlsplFreeBlock(tls, slot_idx);
                }
            }
        }
        slots[idx] = 0;
    }
    sceKernelCpuResumeIntr(intr);
}

SceUID sceKernelCreateTlspl(const char *name, int part, u32 attr, u32 block_size,
                            u32 blocks, void *opt)
{
    if (!tmanCheckUserAddr((u32)name) || !tmanCheckUserPtr(opt, 4))
        return (SceUID)SCE_ERROR_KERNEL_ILLEGAL_ADDR;
    if (tmanReadIntrDepth() != 0)
        return (SceUID)SCE_ERROR_KERNEL_CANNOT_BE_CALLED_FROM_INTERRUPT;
    if (block_size == 0 || blocks == 0)
        return (SceUID)SCE_ERROR_KERNEL_ILLEGAL_MEMSIZE;

    u32 aligned_size = (block_size + 3U) & ~3U;
    int is_user = tmanIsUserCaller();
    int intr = sceKernelCpuSuspendIntr();

    s32 tls_idx = -1;
    int slot_i;
    for (slot_i = 0; slot_i < 16; slot_i++) {
        if ((g_tman.tls_index_bitmap & (1U << slot_i)) == 0) {
            tls_idx = slot_i;
            break;
        }
    }
    if (tls_idx < 0) {
        sceKernelCpuResumeIntr(intr);
        return (SceUID)SCE_ERROR_KERNEL_NO_MEMORY;
    }

    void **link_table = (void **)sceKernelAllocHeapMemory(g_tman.heap_id, blocks * sizeof(void *));
    if (link_table == NULL) {
        sceKernelCpuResumeIntr(intr);
        return (SceUID)SCE_ERROR_KERNEL_NO_MEMORY;
    }
    memset(link_table, 0, blocks * sizeof(void *));

    SceUID mem_id = sceKernelAllocPartitionMemory(part, name, ((attr & 0x4000U) != 0) ? 1 : 0,
                                                  aligned_size * blocks, NULL);
    if (mem_id < 0) {
        sceKernelFreeHeapMemory(g_tman.heap_id, link_table);
        sceKernelCpuResumeIntr(intr);
        return (SceUID)SCE_ERROR_KERNEL_NO_MEMORY;
    }

    SceSysmemUidCB *cb = NULL;
    int ret = sceKernelCreateUID(g_tman.uid_tlspl, name,
                                 (char)((attr & 0xFFU) | (is_user ? 0xFFU : 0x00U)), &cb);
    if (ret != 0) {
        sceKernelFreePartitionMemory(mem_id);
        sceKernelFreeHeapMemory(g_tman.heap_id, link_table);
        sceKernelCpuResumeIntr(intr);
        return ret;
    }

    TmanWaitQueue *wq = (TmanWaitQueue *)tmanUidData(cb, g_tman.uid_waitq);
    TmanTlsplCB *tls = (TmanTlsplCB *)tmanUidData(cb, g_tman.uid_tlspl);
    SceUID uid = cb->uid ^ (s32)g_tman.uid_xor_key;
    wq->attr = attr;
    tls->opt = opt;
    tls->mem_id = mem_id;
    tls->pool = sceKernelGetBlockHeadAddr(mem_id);
    tls->block_size = aligned_size;
    tls->num_blocks = blocks;
    tls->free_blocks = 0;
    tls->free_tail = NULL;
    tls->link_table = link_table;
    tls->tls_index = tls_idx;
    tls->attr = attr;
    g_tman.tls_index_bitmap |= (1U << tls_idx);

    u32 idx;
    for (idx = 0; idx < blocks; idx++)
        tmanTlsplFreeBlock(tls, (s32)idx);

    sceKernelCpuResumeIntr(intr);
    return uid;
}

int sceKernelDeleteTlspl(SceUID uid)
{
    if (tmanReadIntrDepth() != 0)
        return (int)SCE_ERROR_KERNEL_CANNOT_BE_CALLED_FROM_INTERRUPT;

    int intr = sceKernelCpuSuspendIntr();
    SceSysmemUidCB *cb = NULL;
    if (sceKernelGetUIDcontrolBlockWithType(uid, g_tman.uid_tlspl, &cb) != 0) {
        sceKernelCpuResumeIntr(intr);
        return (int)SCE_ERROR_KERNEL_UNKNOWN_TLSPLID;
    }
    if (!tmanCheckUidAttr(cb, 0x10)) {
        sceKernelCpuResumeIntr(intr);
        return (int)SCE_ERROR_KERNEL_ILLEGAL_PERMISSION;
    }

    TmanTlsplCB *tls = (TmanTlsplCB *)tmanUidData(cb, g_tman.uid_tlspl);
    if (tls->free_blocks != tls->num_blocks) {
        sceKernelCpuResumeIntr(intr);
        return (int)SCE_ERROR_KERNEL_UTLS_BUSY;
    }

    g_tman.tls_index_bitmap &= ~(1U << tls->tls_index);
    sceKernelDeleteUID(uid);
    sceKernelCpuResumeIntr(intr);
    return 0;
}

s32 _sceKernelAllocateTlspl(SceUID uid, void **out_ptr, u32 *timeout)
{
    if (!tmanCheckUserPtr(out_ptr, sizeof(void *)) || !tmanCheckUserPtr(timeout, sizeof(u32)))
        return (s32)SCE_ERROR_KERNEL_ILLEGAL_ADDR;
    if (tmanReadIntrDepth() != 0)
        return (s32)SCE_ERROR_KERNEL_CANNOT_BE_CALLED_FROM_INTERRUPT;

    int intr = sceKernelCpuSuspendIntr();
    SceSysmemUidCB *cb = NULL;
    if (sceKernelGetUIDcontrolBlockWithType(uid, g_tman.uid_tlspl, &cb) != 0) {
        sceKernelCpuResumeIntr(intr);
        return (s32)SCE_ERROR_KERNEL_UNKNOWN_TLSPLID;
    }

    TmanTlsplCB *tls = (TmanTlsplCB *)tmanUidData(cb, g_tman.uid_tlspl);
    TmanThreadCB *cur = g_tman.current_thread;
    u32 *slots = (u32 *)cur->user_tls;
    if (slots != NULL && slots[tls->tls_index] != 0) {
        if (out_ptr != NULL)
            *out_ptr = (void *)slots[tls->tls_index];
        sceKernelCpuResumeIntr(intr);
        return 0;
    }

    int idx = tmanTlsplAllocBlock(tls);
    if (idx >= 0) {
        void *ptr = (u8 *)tls->pool + (u32)idx * tls->block_size;
        memset(ptr, 0, tls->block_size);
        if (slots != NULL) {
            slots[tls->tls_index] = (u32)ptr;
            slots[16 + tls->tls_index] = (u32)uid;
        }
        if (out_ptr != NULL)
            *out_ptr = ptr;
        sceKernelCpuResumeIntr(intr);
        return 0;
    }

    sceKernelCpuResumeIntr(intr);
    return (s32)SCE_ERROR_KERNEL_NO_MEMORY;
}

int sceKernelFreeTlspl(SceUID uid)
{
    int intr = sceKernelCpuSuspendIntr();
    SceSysmemUidCB *cb = NULL;
    if (sceKernelGetUIDcontrolBlockWithType(uid, g_tman.uid_tlspl, &cb) != 0) {
        sceKernelCpuResumeIntr(intr);
        return (int)SCE_ERROR_KERNEL_UNKNOWN_TLSPLID;
    }

    TmanTlsplCB *tls = (TmanTlsplCB *)tmanUidData(cb, g_tman.uid_tlspl);
    TmanWaitQueue *wq = (TmanWaitQueue *)tmanUidData(cb, g_tman.uid_waitq);
    TmanThreadCB *cur = g_tman.current_thread;
    u32 *slots = (u32 *)cur->user_tls;
    if (slots != NULL && slots[tls->tls_index] != 0) {
        void *blk = (void *)slots[tls->tls_index];
        int slot_idx = tmanTlsplValidateBlock(tls, blk);
        slots[tls->tls_index] = 0;
        slots[16 + tls->tls_index] = 0;
        if (slot_idx >= 0) {
            if (!tmanListEmpty(&wq->wait_list)) {
                TmanThreadCB *waiter = (TmanThreadCB *)wq->wait_list.next;
                void **out = (void **)waiter->wait_param[0];
                if (out != NULL)
                    *out = blk;
                tmanWakeupFirstWaiter(cb, 0, (int)tmanReadIntrDepth(), intr);
            } else {
                tmanTlsplFreeBlock(tls, slot_idx);
            }
        }
    }

    sceKernelCpuResumeIntr(intr);
    return 0;
}

int sceKernelReferTlsplStatus(SceUID uid, TmanTlsplInfo *info)
{
    if (!tmanCheckUserPtr(info, sizeof(*info)))
        return (int)SCE_ERROR_KERNEL_ILLEGAL_ADDR;

    int intr = sceKernelCpuSuspendIntr();
    SceSysmemUidCB *cb = NULL;
    if (sceKernelGetUIDcontrolBlockWithType(uid, g_tman.uid_tlspl, &cb) != 0) {
        sceKernelCpuResumeIntr(intr);
        return (int)SCE_ERROR_KERNEL_UNKNOWN_TLSPLID;
    }
    if (!tmanCheckUidAttr(cb, 1)) {
        sceKernelCpuResumeIntr(intr);
        return (int)SCE_ERROR_KERNEL_ILLEGAL_PERMISSION;
    }

    TmanWaitQueue *wq = (TmanWaitQueue *)tmanUidData(cb, g_tman.uid_waitq);
    TmanTlsplCB *tls = (TmanTlsplCB *)tmanUidData(cb, g_tman.uid_tlspl);
    TmanTlsplInfo local;
    memset(&local, 0, sizeof(local));
    local.size = sizeof(local);
    if (cb->name != NULL)
        strncpy(local.name, cb->name, SCE_UID_NAME_LEN);
    local.attr = wq->attr;
    local.index = 0;
    local.block_size = tls->block_size;
    local.total_blocks = tls->num_blocks;
    local.free_blocks = tls->free_blocks;
    local.num_wait_threads = wq->num_wait_threads;

    SceSize copy_size = info->size < local.size ? info->size : local.size;
    memcpy(info, &local, copy_size);
    sceKernelCpuResumeIntr(intr);
    return 0;
}

/* ============================================================================
 * System Timer, Delays, Alarms, VTimers
 * ============================================================================ */

u32 g_saved_sys_timer;

static int tmanSysTimerSuspendHandler(int id, void *arg)
{
    (void)id;
    (void)arg;
    g_saved_sys_timer = TMAN_REG_SYS_TIME_LO;
    sceKernelDisableIntr(g_tman.sys_timer_intr);
    u32 diff = (u32)g_tman.next_timer_alarm - g_saved_sys_timer;
    if (diff == 0 || diff > 0xF0000000U)
        g_saved_sys_timer = (u32)g_tman.next_timer_alarm - 1U;
    return 0;
}

static int tmanSysTimerResumeHandler(int id, void *arg)
{
    (void)id;
    (void)arg;
    TMAN_REG_SYS_TIME_PRESCALE = 1;
    TMAN_REG_SYS_TIME_CTRL = 48;
    TMAN_REG_SYS_TIME_MODE = 0;
    tmanSync();
    TMAN_REG_SYS_TIME_LO = g_saved_sys_timer;
    tmanSync();
    TMAN_REG_SYS_TIME_CMP = (u32)g_tman.next_timer_alarm;
    tmanSync();
    tmanSync();
    sceKernelEnableIntr(g_tman.sys_timer_intr);
    return 0;
}

static int tmanRunAlarmHandler(SceSysmemUidCB *cb, int intr_state)
{
    TmanAlarmCB *alarm = (TmanAlarmCB *)tmanUidData(cb, g_tman.uid_alarm);
    SceUID uid = cb->uid ^ (s32)g_tman.uid_xor_key;
    g_tman.active_timer_uid = uid;
    if (g_tman.log_handler != NULL)
        g_tman.log_handler(5, (u32)alarm, 3, uid, alarm->common, alarm->handler);

    int ret;
    if (alarm->is_user != 0) {
        u32 sp = sceKernelGetUserIntrStack();
        ret = sceKernelCallUserIntrHandler((u32)alarm->common, 0, 0, 0, (void *)alarm->handler, sp);
    } else {
        sceKernelCpuResumeIntr(intr_state);
        ret = (int)alarm->handler(alarm->common);
        sceKernelCpuSuspendIntr();
    }
    g_tman.active_timer_uid = 0;
    return ret;
}

static int tmanRunVTimerHandler(SceSysmemUidCB *cb, int intr_state)
{
    TmanVTimerCB *vt = (TmanVTimerCB *)tmanUidData(cb, g_tman.uid_vtimer);
    SceUID uid = cb->uid ^ (s32)g_tman.uid_xor_key;
    g_tman.active_timer_uid = uid;

    u64 now = tmanGetSystemTimeWide();
    u64 elapsed = now - vt->base_time;
    SceKernelSysClock sched_clk;
    SceKernelSysClock cur_clk;
    sched_clk.low = (u32)vt->schedule;
    sched_clk.hi = (u32)(vt->schedule >> 32);
    cur_clk.low = (u32)elapsed;
    cur_clk.hi = (u32)(elapsed >> 32);

    int ret;
    if (vt->is_user != 0) {
        u32 sp = sceKernelGetUserIntrStack() - 16U;
        SceKernelSysClock *user_clks = (SceKernelSysClock *)sp;
        user_clks[0] = sched_clk;
        user_clks[1] = cur_clk;
        ret = sceKernelCallUserIntrHandler((u32)uid, sp, sp + 8U, (u32)vt->common,
                                           (void *)vt->handler, sp);
    } else {
        sceKernelCpuResumeIntr(intr_state);
        ret = (int)vt->handler(uid, &sched_clk, &cur_clk, vt->common);
        sceKernelCpuSuspendIntr();
    }
    g_tman.active_timer_uid = 0;
    return ret;
}

static void tmanRecycleDelayUid(SceSysmemUidCB *cb)
{
    if (g_tman.delay_pool_count >= 32) {
        sceKernelDeleteUID(cb->uid ^ (s32)g_tman.uid_xor_key);
        return;
    }
    TmanTimerCB *tm = (TmanTimerCB *)tmanUidData(cb, g_tman.uid_timer);
    tmanListInsertTail(&g_tman.delay_pool, &tm->link);
    g_tman.delay_pool_count++;
}

static int tmanSysTimerIntrHandler(int intr_num, void *arg)
{
    (void)intr_num;
    (void)arg;
    int intr = sceKernelCpuSuspendIntr();
    u64 now = tmanGetSystemTimeWide();

    while (!tmanListEmpty(&g_tman.timer_list)) {
        TmanTimerCB *tm = (TmanTimerCB *)g_tman.timer_list.next;
        if (tm->schedule > now)
            break;

        g_tman.timer_iter = (TmanTimerCB *)tm->link.next;
        tmanListRemove(&tm->link);
        SceSysmemUidCB *cb = tmanDataToUid(tm, g_tman.uid_timer);

        if (sceKernelIsKindOf(cb, g_tman.uid_delay) != 0) {
            TmanDelayCB *del = (TmanDelayCB *)tmanUidData(cb, g_tman.uid_delay);
            void (*fn)(void *) = del->handler;
            void *fn_arg = del->arg;
            tmanRecycleDelayUid(cb);
            if (fn != NULL)
                fn(fn_arg);
        } else if (sceKernelIsKindOf(cb, g_tman.uid_alarm) != 0) {
            int next_delta = tmanRunAlarmHandler(cb, intr);
            if (next_delta > 0) {
                tm->schedule = tmanGetSystemTimeWide() + (u32)next_delta;
                tmanInsertTimerSorted(tm);
            } else {
                sceKernelDeleteUID(cb->uid ^ (s32)g_tman.uid_xor_key);
            }
        } else if (sceKernelIsKindOf(cb, g_tman.uid_vtimer) != 0) {
            TmanVTimerCB *vt = (TmanVTimerCB *)tmanUidData(cb, g_tman.uid_vtimer);
            int next_delta = tmanRunVTimerHandler(cb, intr);
            if (next_delta > 0 && vt->active != 0 && vt->handler != NULL) {
                vt->schedule += (u32)next_delta;
                tm->schedule = vt->base_time + vt->schedule;
                tmanInsertTimerSorted(tm);
            }
        }
        now = tmanGetSystemTimeWide();
    }

    g_tman.timer_iter = NULL;
    tmanProgramNextTimer();
    sceKernelCpuResumeIntr(intr);
    return -1;
}

void tmanInitTimerTypes(void)
{
    sceKernelCreateUIDtype("Timer", 20,
                           (SceSysmemUidLookupFunc *)g_uid_timer_funcs,
                           NULL, &g_tman.uid_timer);
    sceKernelCreateUIDtypeInherit("Timer", "Delay", 48,
                                  (SceSysmemUidLookupFunc *)g_uid_delay_funcs,
                                  NULL, &g_tman.uid_delay);
    sceKernelCreateUIDtypeInherit("Timer", "Alarm", 48,
                                  (SceSysmemUidLookupFunc *)g_uid_alarm_funcs,
                                  NULL, &g_tman.uid_alarm);
    sceKernelCreateUIDtypeInherit("Timer", "VTimer", 48,
                                  (SceSysmemUidLookupFunc *)g_uid_vtimer_funcs,
                                  NULL, &g_tman.uid_vtimer);
}

void tmanInitSysTimer(void)
{
    g_tman.timer_res_hi = 1;
    g_tman.timer_res_lo = 1;
    g_tman.timer_min_delta = 100;
    g_tman.timer_max_delta = 200;
    g_tman.get_time_low_fn = (void *)sceKernelGetSystemTimeLow;
    g_tman.get_time_wide_fn = (void *)tmanGetSystemTimeWide;
    g_tman.sys_timer_intr = 15;

    tmanListInit(&g_tman.delay_pool);
    tmanListInit(&g_tman.timer_list);
    g_tman.delay_pool_count = 0;
    g_tman.timer_iter = NULL;
    g_tman.sys_timer_high = 0;
    g_tman.next_timer_alarm = 0x40000000ULL;

    TMAN_REG_SYS_TIME_PRESCALE = 1;
    TMAN_REG_SYS_TIME_CTRL = 48;
    TMAN_REG_SYS_TIME_MODE = 0;
    tmanSync();
    TMAN_REG_SYS_TIME_CMP = (u32)g_tman.next_timer_alarm;
    tmanSync();

    sceKernelRegisterIntrHandler(g_tman.sys_timer_intr, 1, (void *)tmanSysTimerIntrHandler,
                                 g_tman.current_thread, NULL);
    sceKernelSetIntrLevel(g_tman.sys_timer_intr, 2);
    sceKernelSetPrimarySyscallHandler(24, (void *)tman_systime_low_syscall);
    sceKernelRegisterSuspendHandler(24, (void *)tmanSysTimerSuspendHandler, NULL);
    sceKernelRegisterResumeHandler(24, (void *)tmanSysTimerResumeHandler, NULL);
    sceKernelEnableIntr(g_tman.sys_timer_intr);
}

u64 tmanGetSystemTimeWide(void)
{
    u32 lo = TMAN_REG_SYS_TIME_LO;
    u64 hi = g_tman.sys_timer_high;
    if (lo < (u32)hi)
        hi += 0x100000000ULL;
    g_tman.sys_timer_high = (hi & 0xFFFFFFFF00000000ULL) | lo;
    return g_tman.sys_timer_high;
}

void tmanInsertTimerSorted(TmanTimerCB *timer)
{
    TmanListNode *head = &g_tman.timer_list;
    TmanListNode *cur = head->next;
    while (cur != head) {
        TmanTimerCB *other = (TmanTimerCB *)cur;
        if (timer->schedule < other->schedule)
            break;
        cur = cur->next;
    }
    tmanListInsertBefore(cur, &timer->link);
}

void tmanProgramNextTimer(void)
{
    u64 now = tmanGetSystemTimeWide();
    u64 target = now + 0x40000000ULL;
    if (!tmanListEmpty(&g_tman.timer_list)) {
        TmanTimerCB *first = (TmanTimerCB *)g_tman.timer_list.next;
        if (first->schedule < target)
            target = first->schedule;
    }
    if (target < now + g_tman.timer_max_delta)
        target = now + g_tman.timer_max_delta;
    g_tman.next_timer_alarm = target;
    TMAN_REG_SYS_TIME_CMP = (u32)target;
    tmanSync();
}

void tmanCalcAbsTimeoutUsec(u64 *abs_out, u32 usec)
{
    *abs_out = tmanGetSystemTimeWide() + (u64)usec;
}

void tmanCalcAbsTimeoutClock(u64 *abs_out, const SceKernelSysClock *clk)
{
    u64 now = tmanGetSystemTimeWide();
    *abs_out = now + *(const u64 *)clk;
}

SceUID tmanCreateDelayTimer(const u64 *abs_time, void (*handler)(void *), void *arg)
{
    u64 now = tmanGetSystemTimeWide();
    if (*abs_time <= now)
        return (SceUID)SCE_ERROR_KERNEL_WAIT_TIMEOUT;

    SceSysmemUidCB *cb = NULL;
    if (g_tman.delay_pool_count > 0) {
        TmanTimerCB *tm = (TmanTimerCB *)g_tman.delay_pool.next;
        tmanListRemove(&tm->link);
        g_tman.delay_pool_count--;
        cb = tmanDataToUid(tm, g_tman.uid_timer);
    } else {
        if (sceKernelCreateUID(g_tman.uid_delay, "SceThreadmanDelay", 0, &cb) != 0)
            return (SceUID)SCE_ERROR_KERNEL_NO_MEMORY;
    }

    TmanTimerCB *tm = (TmanTimerCB *)tmanUidData(cb, g_tman.uid_timer);
    TmanDelayCB *del = (TmanDelayCB *)tmanUidData(cb, g_tman.uid_delay);
    tm->schedule = *abs_time;
    del->handler = handler;
    del->arg = arg;
    tmanInsertTimerSorted(tm);
    tmanProgramNextTimer();
    return cb->uid ^ (s32)g_tman.uid_xor_key;
}

int tmanCancelDelayTimer(SceUID delay_id, u32 *remain_usec)
{
    SceSysmemUidCB *cb = NULL;
    if (sceKernelGetUIDcontrolBlockWithType(delay_id, g_tman.uid_delay, &cb) != 0)
        return -1;

    TmanTimerCB *tm = (TmanTimerCB *)tmanUidData(cb, g_tman.uid_timer);
    if (remain_usec != NULL) {
        u64 now = tmanGetSystemTimeWide();
        u64 rem = (tm->schedule > now) ? (tm->schedule - now) : 0ULL;
        *remain_usec = (u32)rem;
    }

    if (tm == g_tman.timer_iter)
        g_tman.timer_iter = (TmanTimerCB *)tm->link.next;
    tmanListRemove(&tm->link);
    tmanRecycleDelayUid(cb);
    tmanProgramNextTimer();
    return 0;
}

static int timerUidCreate(SceSysmemUidCB *cb, SceSysmemUidCB *type, int func_id, void *args)
{
    sceKernelCallUIDObjCommonFunction(cb, type, func_id, args);
    TmanTimerCB *tm = (TmanTimerCB *)tmanUidData(cb, type);
    tm->flags = 0;
    tmanListInit(&tm->link);
    return cb->uid ^ (s32)g_tman.uid_xor_key;
}

static int timerUidDelete(SceSysmemUidCB *cb, SceSysmemUidCB *type, int func_id, void *args)
{
    sceKernelCallUIDObjCommonFunction(cb, type, func_id, args);
    return 0;
}

static int delayUidCreate(SceSysmemUidCB *cb, SceSysmemUidCB *type, int func_id, void *args)
{
    sceKernelCallUIDObjCommonFunction(cb, type, func_id, args);
    return cb->uid ^ (s32)g_tman.uid_xor_key;
}

static int delayUidDelete(SceSysmemUidCB *cb, SceSysmemUidCB *type, int func_id, void *args)
{
    sceKernelCallUIDObjCommonFunction(cb, type, func_id, args);
    return 0;
}

static int alarmUidCreate(SceSysmemUidCB *cb, SceSysmemUidCB *type, int func_id, void *args)
{
    sceKernelCallUIDObjCommonFunction(cb, type, func_id, args);
    return cb->uid ^ (s32)g_tman.uid_xor_key;
}

static int alarmUidDelete(SceSysmemUidCB *cb, SceSysmemUidCB *type, int func_id, void *args)
{
    sceKernelCallUIDObjCommonFunction(cb, type, func_id, args);
    return 0;
}

static int vtimerUidCreate(SceSysmemUidCB *cb, SceSysmemUidCB *type, int func_id, void *args)
{
    sceKernelCallUIDObjCommonFunction(cb, type, func_id, args);
    return cb->uid ^ (s32)g_tman.uid_xor_key;
}

static int vtimerUidDelete(SceSysmemUidCB *cb, SceSysmemUidCB *type, int func_id, void *args)
{
    TmanVTimerCB *vt = (TmanVTimerCB *)tmanUidData(cb, g_tman.uid_vtimer);
    TmanTimerCB *tm = (TmanTimerCB *)tmanUidData(cb, g_tman.uid_timer);
    if (vt->active != 0 && vt->handler != NULL) {
        if (tm == g_tman.timer_iter)
            g_tman.timer_iter = (TmanTimerCB *)tm->link.next;
        tmanListRemove(&tm->link);
        tmanProgramNextTimer();
    }
    sceKernelCallUIDObjCommonFunction(cb, type, func_id, args);
    return 0;
}

int sceKernelUSec2SysClock(SceUInt usec, SceKernelSysClock *clock)
{
    if (!tmanCheckUserPtr(clock, sizeof(*clock)))
        return (int)SCE_ERROR_KERNEL_ILLEGAL_ADDR;
    clock->low = usec;
    clock->hi = 0;
    return 0;
}

s64 sceKernelUSec2SysClockWide(SceUInt usec)
{
    return (s64)(u64)usec;
}

static void tmanDiv64By1000000(u64 val, u32 *sec_out, u32 *usec_out)
{
    u32 hi = (u32)(val >> 32);
    u32 lo = (u32)val;
    u32 rem = hi % 1000000U;
    u32 q_lo = hi / 1000000U;
    int idx;
    for (idx = 0; idx < 4; idx++) {
        u32 cur = (rem << 8) | (lo >> 24);
        lo <<= 8;
        rem = cur % 1000000U;
        q_lo = (q_lo << 8) + (cur / 1000000U);
    }
    if (sec_out != NULL)
        *sec_out = q_lo;
    if (usec_out != NULL)
        *usec_out = rem;
}

int sceKernelSysClock2USec(SceKernelSysClock *clock, u32 *low, u32 *high)
{
    if (!tmanCheckUserPtr(clock, sizeof(*clock)) ||
        !tmanCheckUserPtr(low, sizeof(u32)) || !tmanCheckUserPtr(high, sizeof(u32)))
        return (int)SCE_ERROR_KERNEL_ILLEGAL_ADDR;

    if (low != NULL) {
        u64 val = ((u64)clock->hi << 32) | clock->low;
        tmanDiv64By1000000(val, low, high);
    } else if (high != NULL) {
        *high = clock->low;
    }
    return 0;
}

int sceKernelSysClock2USecWide(s64 clock, u32 *low, u32 *high)
{
    if (!tmanCheckUserPtr(low, sizeof(u32)) || !tmanCheckUserPtr(high, sizeof(u32)))
        return (int)SCE_ERROR_KERNEL_ILLEGAL_ADDR;

    if (low != NULL)
        tmanDiv64By1000000((u64)clock, low, high);
    else if (high != NULL)
        *high = (u32)clock;
    return 0;
}

int sceKernelGetSystemTime(SceKernelSysClock *time)
{
    if (!tmanCheckUserPtr(time, sizeof(*time)))
        return (int)SCE_ERROR_KERNEL_ILLEGAL_ADDR;

    int intr = sceKernelCpuSuspendIntr();
    u64 now = tmanGetSystemTimeWide();
    time->low = (u32)now;
    time->hi = (u32)(now >> 32);
    if (g_tman.log_handler != NULL)
        g_tman.log_handler(131, (u32)time, 2, time->hi, time->low);
    sceKernelCpuResumeIntr(intr);
    return 0;
}

s64 sceKernelGetSystemTimeWide(void)
{
    int intr = sceKernelCpuSuspendIntr();
    u64 now = tmanGetSystemTimeWide();
    if (g_tman.log_handler != NULL)
        g_tman.log_handler(132, 0, 1, (u32)now);
    sceKernelCpuResumeIntr(intr);
    return (s64)now;
}

unsigned int sceKernelGetSystemTimeLow(void)
{
    return TMAN_REG_SYS_TIME_LO;
}

static SceUID tmanSetAlarmInternal(u64 delta, SceKernelAlarmHandler handler, void *common)
{
    int is_user = tmanIsUserCaller();
    if (handler == NULL || !tmanCheckUserAddr((u32)handler) ||
        (!is_user && (s32)(u32)handler >= 0))
        return (SceUID)SCE_ERROR_KERNEL_ILLEGAL_ADDR;

    int intr = sceKernelCpuSuspendIntr();
    SceSysmemUidCB *cb = NULL;
    int ret = sceKernelCreateUID(g_tman.uid_alarm, "SceThreadmanAlarm",
                                 (char)(is_user ? 0xFF : 0x00), &cb);
    if (ret != 0) {
        sceKernelCpuResumeIntr(intr);
        return ret;
    }

    TmanTimerCB *tm = (TmanTimerCB *)tmanUidData(cb, g_tman.uid_timer);
    TmanAlarmCB *alarm = (TmanAlarmCB *)tmanUidData(cb, g_tman.uid_alarm);
    tm->schedule = tmanGetSystemTimeWide() + delta;
    alarm->is_user = (u32)is_user;
    alarm->handler = handler;
    alarm->common = common;
    alarm->gp_reg = (u32)sceKernelGetModuleGPByAddressForKernel((u32)handler);
    if (alarm->gp_reg == 0)
        alarm->gp_reg = tmanGetGP();

    tmanInsertTimerSorted(tm);
    tmanProgramNextTimer();
    SceUID uid = cb->uid ^ (s32)g_tman.uid_xor_key;
    sceKernelCpuResumeIntr(intr);
    return uid;
}

SceUID sceKernelSetAlarm(SceUInt clock, SceKernelAlarmHandler handler, void *common)
{
    SceUID uid = tmanSetAlarmInternal((u64)clock, handler, common);
    if (g_tman.log_handler != NULL)
        g_tman.log_handler(138, (u32)handler, 4, clock, handler, common, uid);
    return uid;
}

SceUID sceKernelSetSysClockAlarm(SceKernelSysClock *clock, SceKernelAlarmHandler handler, void *common)
{
    if (!tmanCheckUserPtr(clock, sizeof(*clock)))
        return (SceUID)SCE_ERROR_KERNEL_ILLEGAL_ADDR;
    u64 delta = ((u64)clock->hi << 32) | clock->low;
    SceUID uid = tmanSetAlarmInternal(delta, handler, common);
    if (g_tman.log_handler != NULL)
        g_tman.log_handler(139, (u32)handler, 5, clock->hi, clock->low, handler, common, uid);
    return uid;
}

int sceKernelCancelAlarm(SceUID alarmid)
{
    if (alarmid == g_tman.active_timer_uid)
        return (int)SCE_ERROR_KERNEL_NOT_FOUND_ALARM;

    int intr = sceKernelCpuSuspendIntr();
    SceSysmemUidCB *cb = NULL;
    if (sceKernelGetUIDcontrolBlockWithType(alarmid, g_tman.uid_alarm, &cb) != 0) {
        sceKernelCpuResumeIntr(intr);
        return (int)SCE_ERROR_KERNEL_NOT_FOUND_ALARM;
    }
    if (!tmanCheckUidAttr(cb, 0x10)) {
        sceKernelCpuResumeIntr(intr);
        return (int)SCE_ERROR_KERNEL_ILLEGAL_PERMISSION;
    }

    TmanTimerCB *tm = (TmanTimerCB *)tmanUidData(cb, g_tman.uid_timer);
    if (tm == g_tman.timer_iter)
        g_tman.timer_iter = (TmanTimerCB *)tm->link.next;
    tmanListRemove(&tm->link);
    sceKernelDeleteUID(alarmid);
    tmanProgramNextTimer();
    sceKernelCpuResumeIntr(intr);
    return 0;
}

int sceKernelReferAlarmStatus(SceUID alarmid, SceKernelAlarmInfo *info)
{
    if (!tmanCheckUserPtr(info, sizeof(*info)))
        return (int)SCE_ERROR_KERNEL_ILLEGAL_ADDR;

    int intr = sceKernelCpuSuspendIntr();
    SceSysmemUidCB *cb = NULL;
    if (sceKernelGetUIDcontrolBlockWithType(alarmid, g_tman.uid_alarm, &cb) != 0) {
        sceKernelCpuResumeIntr(intr);
        return (int)SCE_ERROR_KERNEL_NOT_FOUND_ALARM;
    }
    if (!tmanCheckUidAttr(cb, 1)) {
        sceKernelCpuResumeIntr(intr);
        return (int)SCE_ERROR_KERNEL_ILLEGAL_PERMISSION;
    }

    TmanTimerCB *tm = (TmanTimerCB *)tmanUidData(cb, g_tman.uid_timer);
    TmanAlarmCB *alarm = (TmanAlarmCB *)tmanUidData(cb, g_tman.uid_alarm);
    SceKernelAlarmInfo local;
    memset(&local, 0, sizeof(local));
    local.size = sizeof(local);
    local.schedule.low = (u32)tm->schedule;
    local.schedule.hi = (u32)(tm->schedule >> 32);
    local.handler = alarm->handler;
    local.common = alarm->common;

    SceSize copy_size = info->size < local.size ? info->size : local.size;
    memcpy(info, &local, copy_size);
    sceKernelCpuResumeIntr(intr);
    return 0;
}

SceUID sceKernelCreateVTimer(const char *name, void *opt)
{
    if (!tmanCheckUserAddr((u32)name) || !tmanCheckUserPtr(opt, 4))
        return (SceUID)SCE_ERROR_KERNEL_ILLEGAL_ADDR;
    if (tmanReadIntrDepth() != 0)
        return (SceUID)SCE_ERROR_KERNEL_CANNOT_BE_CALLED_FROM_INTERRUPT;

    int is_user = tmanIsUserCaller();
    int intr = sceKernelCpuSuspendIntr();
    SceSysmemUidCB *cb = NULL;
    int ret = sceKernelCreateUID(g_tman.uid_vtimer, name,
                                 (char)(is_user ? 0xFF : 0x00), &cb);
    if (ret != 0) {
        sceKernelCpuResumeIntr(intr);
        return ret;
    }

    TmanVTimerCB *vt = (TmanVTimerCB *)tmanUidData(cb, g_tman.uid_vtimer);
    memset(vt, 0, sizeof(*vt));
    vt->is_user = (u32)is_user;
    SceUID uid = cb->uid ^ (s32)g_tman.uid_xor_key;
    sceKernelCpuResumeIntr(intr);
    return uid;
}

int sceKernelDeleteVTimer(SceUID uid)
{
    if (tmanReadIntrDepth() != 0)
        return (int)SCE_ERROR_KERNEL_CANNOT_BE_CALLED_FROM_INTERRUPT;
    if (uid == g_tman.active_timer_uid)
        return (int)SCE_ERROR_KERNEL_ILLEGAL_VTIMER;

    int intr = sceKernelCpuSuspendIntr();
    SceSysmemUidCB *cb = NULL;
    if (sceKernelGetUIDcontrolBlockWithType(uid, g_tman.uid_vtimer, &cb) != 0) {
        sceKernelCpuResumeIntr(intr);
        return (int)SCE_ERROR_KERNEL_NOT_FOUND_VTIMER;
    }
    if (!tmanCheckUidAttr(cb, 0x10)) {
        sceKernelCpuResumeIntr(intr);
        return (int)SCE_ERROR_KERNEL_ILLEGAL_PERMISSION;
    }

    sceKernelDeleteUID(uid);
    sceKernelCpuResumeIntr(intr);
    return 0;
}

int sceKernelGetVTimerBase(SceUID uid, SceKernelSysClock *base)
{
    if (!tmanCheckUserPtr(base, sizeof(*base)))
        return (int)SCE_ERROR_KERNEL_ILLEGAL_ADDR;

    int intr = sceKernelCpuSuspendIntr();
    SceSysmemUidCB *cb = NULL;
    if (sceKernelGetUIDcontrolBlockWithType(uid, g_tman.uid_vtimer, &cb) != 0) {
        sceKernelCpuResumeIntr(intr);
        return (int)SCE_ERROR_KERNEL_NOT_FOUND_VTIMER;
    }
    TmanVTimerCB *vt = (TmanVTimerCB *)tmanUidData(cb, g_tman.uid_vtimer);
    u64 val = vt->active ? vt->base_time : 0ULL;
    base->low = (u32)val;
    base->hi = (u32)(val >> 32);
    sceKernelCpuResumeIntr(intr);
    return 0;
}

s64 sceKernelGetVTimerBaseWide(SceUID uid)
{
    int intr = sceKernelCpuSuspendIntr();
    SceSysmemUidCB *cb = NULL;
    if (sceKernelGetUIDcontrolBlockWithType(uid, g_tman.uid_vtimer, &cb) != 0 ||
        !tmanCheckUidAttr(cb, 1)) {
        sceKernelCpuResumeIntr(intr);
        return -1LL;
    }
    TmanVTimerCB *vt = (TmanVTimerCB *)tmanUidData(cb, g_tman.uid_vtimer);
    u64 val = vt->active ? vt->base_time : 0ULL;
    if (g_tman.log_handler != NULL)
        g_tman.log_handler(151, (u32)intr, 2, uid, (u32)val);
    sceKernelCpuResumeIntr(intr);
    return (s64)val;
}

int sceKernelGetVTimerTime(SceUID uid, SceKernelSysClock *time)
{
    if (!tmanCheckUserPtr(time, sizeof(*time)))
        return (int)SCE_ERROR_KERNEL_ILLEGAL_ADDR;

    int intr = sceKernelCpuSuspendIntr();
    SceSysmemUidCB *cb = NULL;
    if (sceKernelGetUIDcontrolBlockWithType(uid, g_tman.uid_vtimer, &cb) != 0) {
        sceKernelCpuResumeIntr(intr);
        return (int)SCE_ERROR_KERNEL_NOT_FOUND_VTIMER;
    }
    TmanVTimerCB *vt = (TmanVTimerCB *)tmanUidData(cb, g_tman.uid_vtimer);
    u64 val = vt->active ? (tmanGetSystemTimeWide() - vt->base_time) : vt->base_time;
    time->low = (u32)val;
    time->hi = (u32)(val >> 32);
    sceKernelCpuResumeIntr(intr);
    return 0;
}

s64 sceKernelGetVTimerTimeWide(SceUID uid)
{
    int intr = sceKernelCpuSuspendIntr();
    SceSysmemUidCB *cb = NULL;
    if (sceKernelGetUIDcontrolBlockWithType(uid, g_tman.uid_vtimer, &cb) != 0 ||
        !tmanCheckUidAttr(cb, 1)) {
        sceKernelCpuResumeIntr(intr);
        return -1LL;
    }
    TmanVTimerCB *vt = (TmanVTimerCB *)tmanUidData(cb, g_tman.uid_vtimer);
    u64 val = vt->active ? (tmanGetSystemTimeWide() - vt->base_time) : vt->base_time;
    if (g_tman.log_handler != NULL)
        g_tman.log_handler(153, (u32)intr, 2, uid, (u32)val);
    sceKernelCpuResumeIntr(intr);
    return (s64)val;
}

static inline __attribute__((always_inline)) u64 tmanSetVTimerTimeCommon(SceSysmemUidCB *cb, u64 new_time)
{
    TmanVTimerCB *vt = (TmanVTimerCB *)tmanUidData(cb, g_tman.uid_vtimer);
    TmanTimerCB *tm = (TmanTimerCB *)tmanUidData(cb, g_tman.uid_timer);
    u64 prev;

    if (vt->active != 0) {
        u64 now = tmanGetSystemTimeWide();
        prev = now - vt->base_time;
        vt->base_time = now - new_time;
        if (vt->handler != NULL) {
            if (tm == g_tman.timer_iter)
                g_tman.timer_iter = (TmanTimerCB *)tm->link.next;
            tmanListRemove(&tm->link);
            tm->schedule = vt->base_time + vt->schedule;
            tmanInsertTimerSorted(tm);
            tmanProgramNextTimer();
        }
    } else {
        prev = vt->base_time;
        vt->base_time = new_time;
    }
    return prev;
}

int sceKernelSetVTimerTime(SceUID uid, SceKernelSysClock *time)
{
    if (!tmanCheckUserPtr(time, sizeof(*time)))
        return (int)SCE_ERROR_KERNEL_ILLEGAL_ADDR;

    int intr = sceKernelCpuSuspendIntr();
    SceSysmemUidCB *cb = NULL;
    if (sceKernelGetUIDcontrolBlockWithType(uid, g_tman.uid_vtimer, &cb) != 0) {
        sceKernelCpuResumeIntr(intr);
        return (int)SCE_ERROR_KERNEL_NOT_FOUND_VTIMER;
    }
    u64 new_time = ((u64)time->hi << 32) | time->low;
    u64 prev = tmanSetVTimerTimeCommon(cb, new_time);
    time->low = (u32)prev;
    time->hi = (u32)(prev >> 32);
    sceKernelCpuResumeIntr(intr);
    return 0;
}

s64 sceKernelSetVTimerTimeWide(SceUID uid, s64 time)
{
    int intr = sceKernelCpuSuspendIntr();
    SceSysmemUidCB *cb = NULL;
    if (sceKernelGetUIDcontrolBlockWithType(uid, g_tman.uid_vtimer, &cb) != 0) {
        sceKernelCpuResumeIntr(intr);
        return 0;
    }
    u64 prev = tmanSetVTimerTimeCommon(cb, (u64)time);
    sceKernelCpuResumeIntr(intr);
    return (s64)prev;
}

int sceKernelStartVTimer(SceUID uid)
{
    if (uid == g_tman.active_timer_uid)
        return (int)SCE_ERROR_KERNEL_ILLEGAL_VTIMER;

    int intr = sceKernelCpuSuspendIntr();
    SceSysmemUidCB *cb = NULL;
    if (sceKernelGetUIDcontrolBlockWithType(uid, g_tman.uid_vtimer, &cb) != 0) {
        sceKernelCpuResumeIntr(intr);
        return (int)SCE_ERROR_KERNEL_NOT_FOUND_VTIMER;
    }
    TmanVTimerCB *vt = (TmanVTimerCB *)tmanUidData(cb, g_tman.uid_vtimer);
    TmanTimerCB *tm = (TmanTimerCB *)tmanUidData(cb, g_tman.uid_timer);
    int prev = (int)vt->active;
    if (vt->active == 0) {
        vt->active = 1;
        vt->base_time = tmanGetSystemTimeWide() - vt->base_time;
        if (vt->handler != NULL) {
            tm->schedule = vt->base_time + vt->schedule;
            tmanInsertTimerSorted(tm);
            tmanProgramNextTimer();
        }
    }
    sceKernelCpuResumeIntr(intr);
    return prev;
}

int sceKernelStopVTimer(SceUID uid)
{
    if (uid == g_tman.active_timer_uid)
        return (int)SCE_ERROR_KERNEL_ILLEGAL_VTIMER;

    int intr = sceKernelCpuSuspendIntr();
    SceSysmemUidCB *cb = NULL;
    if (sceKernelGetUIDcontrolBlockWithType(uid, g_tman.uid_vtimer, &cb) != 0) {
        sceKernelCpuResumeIntr(intr);
        return (int)SCE_ERROR_KERNEL_NOT_FOUND_VTIMER;
    }
    TmanVTimerCB *vt = (TmanVTimerCB *)tmanUidData(cb, g_tman.uid_vtimer);
    TmanTimerCB *tm = (TmanTimerCB *)tmanUidData(cb, g_tman.uid_timer);
    int prev = (int)vt->active;
    if (vt->active != 0) {
        vt->active = 0;
        vt->base_time = tmanGetSystemTimeWide() - vt->base_time;
        if (vt->handler != NULL) {
            if (tm == g_tman.timer_iter)
                g_tman.timer_iter = (TmanTimerCB *)tm->link.next;
            tmanListRemove(&tm->link);
            tmanProgramNextTimer();
        }
    }
    sceKernelCpuResumeIntr(intr);
    return prev;
}

static inline __attribute__((always_inline)) int tmanSetVTimerHandlerCommon(SceUID uid, u64 schedule, void *handler, void *common)
{
    int is_user = tmanIsUserCaller();
    if (!tmanCheckUserAddr((u32)handler) || (!is_user && handler != NULL && (s32)(u32)handler >= 0))
        return (int)SCE_ERROR_KERNEL_ILLEGAL_ADDR;
    if (uid == g_tman.active_timer_uid)
        return (int)SCE_ERROR_KERNEL_ILLEGAL_VTIMER;

    int intr = sceKernelCpuSuspendIntr();
    SceSysmemUidCB *cb = NULL;
    if (sceKernelGetUIDcontrolBlockWithType(uid, g_tman.uid_vtimer, &cb) != 0) {
        sceKernelCpuResumeIntr(intr);
        return (int)SCE_ERROR_KERNEL_NOT_FOUND_VTIMER;
    }

    TmanVTimerCB *vt = (TmanVTimerCB *)tmanUidData(cb, g_tman.uid_vtimer);
    TmanTimerCB *tm = (TmanTimerCB *)tmanUidData(cb, g_tman.uid_timer);
    if (vt->active != 0 && vt->handler != NULL) {
        if (tm == g_tman.timer_iter)
            g_tman.timer_iter = (TmanTimerCB *)tm->link.next;
        tmanListRemove(&tm->link);
    }

    vt->schedule = schedule;
    vt->handler = (u32 (*)(SceUID, SceKernelSysClock *, SceKernelSysClock *, void *))handler;
    vt->common = common;
    vt->is_user = (u32)is_user;
    vt->gp_reg = (handler != NULL) ? (u32)sceKernelGetModuleGPByAddressForKernel((u32)handler) : 0;

    if (vt->active != 0 && vt->handler != NULL) {
        tm->schedule = vt->base_time + vt->schedule;
        tmanInsertTimerSorted(tm);
    }
    tmanProgramNextTimer();
    sceKernelCpuResumeIntr(intr);
    return 0;
}

int sceKernelSetVTimerHandler(SceUID uid, SceKernelSysClock *time, void *handler, void *common)
{
    if (!tmanCheckUserPtr(time, sizeof(*time)))
        return (int)SCE_ERROR_KERNEL_ILLEGAL_ADDR;
    u64 sched = ((u64)time->hi << 32) | time->low;
    return tmanSetVTimerHandlerCommon(uid, sched, handler, common);
}

int sceKernelSetVTimerHandlerWide(SceUID uid, s64 time, void *handler, void *common)
{
    return tmanSetVTimerHandlerCommon(uid, (u64)time, handler, common);
}

int sceKernelCancelVTimerHandler(SceUID uid)
{
    return tmanSetVTimerHandlerCommon(uid, 0, NULL, NULL);
}

int sceKernelReferVTimerStatus(SceUID uid, TmanVTimerInfo *info)
{
    if (!tmanCheckUserPtr(info, sizeof(*info)))
        return (int)SCE_ERROR_KERNEL_ILLEGAL_ADDR;

    int intr = sceKernelCpuSuspendIntr();
    SceSysmemUidCB *cb = NULL;
    if (sceKernelGetUIDcontrolBlockWithType(uid, g_tman.uid_vtimer, &cb) != 0) {
        sceKernelCpuResumeIntr(intr);
        return (int)SCE_ERROR_KERNEL_NOT_FOUND_VTIMER;
    }
    if (!tmanCheckUidAttr(cb, 1)) {
        sceKernelCpuResumeIntr(intr);
        return (int)SCE_ERROR_KERNEL_ILLEGAL_PERMISSION;
    }

    TmanVTimerCB *vt = (TmanVTimerCB *)tmanUidData(cb, g_tman.uid_vtimer);
    TmanVTimerInfo local;
    memset(&local, 0, sizeof(local));
    local.size = sizeof(local);
    if (cb->name != NULL)
        strncpy(local.name, cb->name, SCE_UID_NAME_LEN);
    local.active = (s32)vt->active;
    if (vt->active != 0) {
        u64 cur = tmanGetSystemTimeWide() - vt->base_time;
        local.base.low = (u32)vt->base_time;
        local.base.hi = (u32)(vt->base_time >> 32);
        local.current.low = (u32)cur;
        local.current.hi = (u32)(cur >> 32);
    } else {
        local.current.low = (u32)vt->base_time;
        local.current.hi = (u32)(vt->base_time >> 32);
    }
    local.schedule.low = (u32)vt->schedule;
    local.schedule.hi = (u32)(vt->schedule >> 32);
    local.handler = (void *)vt->handler;
    local.common = vt->common;

    SceSize copy_size = info->size < local.size ? info->size : local.size;
    memcpy(info, &local, copy_size);
    sceKernelCpuResumeIntr(intr);
    return 0;
}

int _sceKernelReturnFromTimerHandler(void)
{
    return 0;
}
