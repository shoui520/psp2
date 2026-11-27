#include "threadman_internal.h"

static int fplUidCreate(SceSysmemUidCB *cb, SceSysmemUidCB *type, int func_id, void *args);
static int fplUidDelete(SceSysmemUidCB *cb, SceSysmemUidCB *type, int func_id, void *args);

const TmanUidFunc g_uid_fpl_funcs[3] = {
    { TMAN_UID_FUNC_CREATE, (void *)fplUidCreate },
    { TMAN_UID_FUNC_DELETE, (void *)fplUidDelete },
    { 0, NULL },
};


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

