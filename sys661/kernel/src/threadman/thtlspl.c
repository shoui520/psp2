#include "threadman_internal.h"

static int tlsplUidCreate(SceSysmemUidCB *cb, SceSysmemUidCB *type, int func_id, void *args);
static int tlsplUidDelete(SceSysmemUidCB *cb, SceSysmemUidCB *type, int func_id, void *args);

const TmanUidFunc g_uid_tlspl_funcs[3] = {
    { TMAN_UID_FUNC_CREATE, (void *)tlsplUidCreate },
    { TMAN_UID_FUNC_DELETE, (void *)tlsplUidDelete },
    { 0, NULL },
};


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

