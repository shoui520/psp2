#include "threadman_internal.h"

static int vplUidCreate(SceSysmemUidCB *cb, SceSysmemUidCB *type, int func_id, void *args);
static int vplUidDelete(SceSysmemUidCB *cb, SceSysmemUidCB *type, int func_id, void *args);
static int vplUidCheck(SceSysmemUidCB *cb, SceSysmemUidCB *type, int func_id, void *args);

const TmanUidFunc g_uid_vpl_funcs[4] = {
    { TMAN_UID_FUNC_CHECK_WAIT, (void *)vplUidCheck },
    { TMAN_UID_FUNC_CREATE,     (void *)vplUidCreate },
    { TMAN_UID_FUNC_DELETE,     (void *)vplUidDelete },
    { 0, NULL },
};


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

