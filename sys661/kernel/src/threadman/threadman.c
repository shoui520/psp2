#include "threadman_internal.h"

TmanGlobals g_tman;
u32 ThreadManForKernel_921B7ACD = 0;

/* UID callbacks for Thread */
static int threadUidCreate(SceSysmemUidCB *cb, SceSysmemUidCB *type, int func_id, void *args);
static int threadUidDelete(SceSysmemUidCB *cb, SceSysmemUidCB *type, int func_id, void *args);
static void tmanCleanerThreadEntry(SceSize args, void *argp);
static int tmanSwiIntrHandler(int intr_num, void *arg);
static int tmanBootCallback(void *arg);
static void tmanThreadStartEntry(void);
static void tmanThreadExitTrampoline(void);
static int tmanK0CorruptedHandler(TmanThreadCB *th);
static int doTerminateDeleteThread(SceUID thid, int delete_after);

const TmanUidFunc g_uid_thread_funcs[3] = {
    { TMAN_UID_FUNC_CREATE, (void *)threadUidCreate },
    { TMAN_UID_FUNC_DELETE, (void *)threadUidDelete },
    { 0, NULL },
};

/* ============================================================================
 * Kernel Thread Local Storage (KTLS)
 * ============================================================================ */

int sceKernelAllocateKTLS(int size, int (*cb)(unsigned int *size, void *arg), void *arg)
{
    if (tmanReadIntrDepth() != 0)
        return (int)SCE_ERROR_KERNEL_CANNOT_BE_CALLED_FROM_INTERRUPT;

    int intr = sceKernelCpuSuspendIntr();
    int id;
    for (id = 0; id < 16; id++) {
        if (g_tman.ktls[id].used == 0)
            break;
    }
    if (id >= 16) {
        sceKernelCpuResumeIntr(intr);
        return (int)SCE_ERROR_KERNEL_KTLS_IS_FULL;
    }

    TmanKtlsEntry *slot = &g_tman.ktls[id];
    slot->ref_count = 0;
    slot->size = (u32)size;
    slot->callback = (void (*)(void *, void *))cb;
    slot->common = arg;
    slot->gp_reg = (u32)sceKernelGetModuleGPByAddressForKernel((u32)cb);
    if (slot->gp_reg == 0)
        slot->gp_reg = tmanGetGP();
    slot->used = 1;

    if (g_tman.log_handler != NULL)
        g_tman.log_handler(167, (u32)slot, 4, size, cb, arg, id);

    sceKernelCpuResumeIntr(intr);
    return id;
}

int sceKernelFreeKTLS(int id)
{
    if (tmanReadIntrDepth() != 0)
        return (int)SCE_ERROR_KERNEL_CANNOT_BE_CALLED_FROM_INTERRUPT;
    if ((u32)id >= 16U)
        return (int)SCE_ERROR_KERNEL_ILLEGAL_KTLS;

    int intr = sceKernelCpuSuspendIntr();
    if (g_tman.log_handler != NULL)
        g_tman.log_handler(168, (u32)id, 1, id);

    TmanKtlsEntry *slot = &g_tman.ktls[id];
    if (slot->used == 0) {
        sceKernelCpuResumeIntr(intr);
        return (int)SCE_ERROR_KERNEL_ILLEGAL_KTLS;
    }
    if (slot->ref_count != 0) {
        sceKernelCpuResumeIntr(intr);
        return (int)SCE_ERROR_KERNEL_KTLS_IS_BUSY;
    }

    slot->used = 0;
    sceKernelCpuResumeIntr(intr);
    return 0;
}

void *sceKernelGetThreadKTLS(int id, SceUID thid, int mode)
{
    if (tmanReadIntrDepth() != 0 || (u32)id >= 16U)
        return NULL;

    int intr = sceKernelCpuSuspendIntr();
    TmanThreadCB *th;
    if (thid != 0) {
        SceSysmemUidCB *cb = NULL;
        if (sceKernelGetUIDcontrolBlockWithType(thid, g_tman.uid_thread, &cb) != 0) {
            sceKernelCpuResumeIntr(intr);
            return NULL;
        }
        th = (TmanThreadCB *)tmanUidData(cb, g_tman.uid_thread);
    } else {
        th = g_tman.current_thread;
        thid = th->uid;
    }

    TmanKtlsEntry *slot = &g_tman.ktls[id];
    if (slot->used == 0) {
        sceKernelCpuResumeIntr(intr);
        return NULL;
    }

    void *ptr = th->ktls[id];
    if (ptr != NULL) {
        if (g_tman.log_handler != NULL)
            g_tman.log_handler(170, (u32)ptr, 4, id, thid, mode, ptr);
        sceKernelCpuResumeIntr(intr);
        return ptr;
    }

    if (mode == 0) {
        sceKernelCpuResumeIntr(intr);
        return NULL;
    }

    ptr = sceKernelAllocHeapMemory(g_tman.heap_id, slot->size);
    if (g_tman.log_handler != NULL)
        g_tman.log_handler(170, (u32)ptr, 4, id, thid, mode, ptr);
    if (ptr != NULL) {
        memset(ptr, 0, slot->size);
        th->ktls[id] = ptr;
        slot->ref_count++;
    }

    sceKernelCpuResumeIntr(intr);
    return ptr;
}

int tmanFreeThreadKtls(TmanThreadCB *th)
{
    int deferred = 0;
    int id;
    for (id = 0; id < 16; id++) {
        void *ptr = th->ktls[id];
        if (ptr == NULL)
            continue;
        TmanKtlsEntry *slot = &g_tman.ktls[id];
        if (slot->callback != NULL) {
            if (th != g_tman.current_thread) {
                deferred = 1;
                continue;
            }
            slot->callback(ptr, slot->common);
        }
        sceKernelFreeHeapMemory(g_tman.heap_id, ptr);
        int intr = sceKernelCpuSuspendIntr();
        th->ktls[id] = NULL;
        slot->ref_count--;
        sceKernelCpuResumeIntr(intr);
    }
    return deferred;
}

void *sceKernelGetKTLS(int id)
{
    return sceKernelGetThreadKTLS(id, 0, 1);
}

/* ============================================================================
 * Ready Queue & Context Switching
 * ============================================================================ */

void tmanInitThreadType(void)
{
    sceKernelCreateUIDtypeInherit("WaitQ", "Thread", sizeof(TmanThreadCB),
                                  (SceSysmemUidLookupFunc *)g_uid_thread_funcs,
                                  NULL, &g_tman.uid_thread);
}

static int threadUidCreate(SceSysmemUidCB *cb, SceSysmemUidCB *type, int func_id, void *args)
{
    sceKernelCallUIDObjCommonFunction(cb, type, func_id, args);
    TmanThreadCB *th = (TmanThreadCB *)tmanUidData(cb, type);
    sceKernelMemset32(th, 0, sizeof(*th));
    th->status = 0;
    th->uid = cb->uid ^ (s32)g_tman.uid_xor_key;
    tmanListInit(&th->link);
    tmanListInit(&th->owned_mutex_list);
    tmanListInit(&th->owned_lwmutex_list);
    tmanListInit(&th->cb_list);
    return cb->uid ^ (s32)g_tman.uid_xor_key;
}

static int threadUidDelete(SceSysmemUidCB *cb, SceSysmemUidCB *type, int func_id, void *args)
{
    int intr = sceKernelCpuSuspendIntr();
    TmanThreadCB *th = (TmanThreadCB *)tmanUidData(cb, type);
    if (th->status != 0) {
        int ret = doTerminateDeleteThread(th->uid, 1);
        if (ret != 0) {
            sceKernelCpuResumeIntr(intr);
            return ret;
        }
    }
    tmanRestoreExtStacks(th);
    if (th->stack != NULL)
        sceKernelFreePartitionMemory(th->stack_block_id);
    if ((s32)th->attr < 0) {
        if (th->kstack != NULL)
            sceKernelFreePartitionMemory(th->kstack_block_id);
        if (th->profiler_regs != NULL)
            sceKernelFreeHeapMemory(g_tman.heap_id, th->profiler_regs);
    }
    if (th->ctx != NULL) {
        if (tmanReadCfc0(8) == (u32)th->ctx)
            tmanWriteCtc0(8, 0);
        sceKernelFreeHeapMemory(g_tman.heap_id, th->ctx);
    }
    if (th->vfpu_ctx != NULL) {
        if (g_tman.vfpu_owner_ctx == th->vfpu_ctx)
            g_tman.vfpu_owner_ctx = NULL;
        tman_vfpu_flush();
        sceKernelFreeHeapMemory(g_tman.heap_id, th->vfpu_ctx);
    }
    sceKernelCpuResumeIntr(intr);
    sceKernelCallUIDObjCommonFunction(cb, type, func_id, args);
    return 0;
}

void tmanInitThreadContext(TmanThreadContext *ctx)
{
    int idx;
    for (idx = 0; idx < 32; idx++)
        ctx->gpr[idx] = 0xDEADBEEFU;
    ctx->gpr[26] = 0;
    ctx->gpr[27] = 0;
    for (idx = 0; idx < 32; idx++)
        ((u32 *)ctx->fpr)[idx] = 0x7F800001U;
    ctx->fcr31 = 0x0E00U;
    ctx->lo = 0xDEADBEEFU;
    ctx->cause = 0;
    ctx->hi = 0xDEADBEEFU;
    ctx->sr = 0;
    ctx->epc = 0;
    ctx->gpr[0] = 0;
}

void tmanAddReadyQueue(TmanThreadCB *th)
{
    u32 prio = (u32)th->current_priority;
    TmanGlobals *g = &g_tman;
    TmanListNode *head = &g->ready_queues[prio];
    TmanListNode *prev = head->prev;
    th->link.prev = prev;
    head->prev = &th->link;
    th->link.next = head;
    g->ready_bitmap[prio >> 5] |= tmanRor32(0x80000000U, prio);
    prev->next = &th->link;
}

void tmanRemoveReadyQueue(TmanThreadCB *th, u32 prio)
{
    TmanListNode *next = th->link.next;
    TmanListNode *prev = th->link.prev;
    if (next == prev && next != &th->link)
        g_tman.ready_bitmap[prio >> 5] &= ~tmanRor32(0x80000000U, prio);
    tmanListRemove(&th->link);
}

void tmanMakeReadyAndPreempt(TmanThreadCB *th, int intr_state)
{
    TmanThreadCB *cur = g_tman.current_thread;
    if (th->current_priority >= cur->current_priority || g_tman.dispatch_suspended != 0) {
        th->status = TMAN_TH_READY;
        tmanAddReadyQueue(th);
        return;
    }

    if (intr_state != 0) {
        u32 cur_prio = (u32)cur->current_priority;
        TmanListNode *head = &g_tman.ready_queues[cur_prio];
        tmanListInsertBefore(head->next, &cur->link);
        g_tman.ready_bitmap[cur_prio >> 5] |= tmanRor32(0x80000000U, cur_prio);
        cur->status = TMAN_TH_READY;
        th->status = TMAN_TH_RUNNING;
        g_tman.next_thread = th;
    } else {
        th->status = TMAN_TH_READY;
        tmanAddReadyQueue(th);
        g_tman.next_thread = NULL;
    }
    tmanSchedule(0, intr_state);
}

void tmanSelectNextThread(void)
{
    TmanThreadCB *cur = g_tman.current_thread;
    g_tman.next_thread = cur;

    if (g_tman.cleaner_needed != 0) {
        g_tman.cleaner_needed = 0;
        TmanThreadCB *cleaner = g_tman.cleaner_thread;
        cleaner->status = TMAN_TH_RUNNING;
        g_tman.next_thread = cleaner;
        if (cur->status == TMAN_TH_RUNNING) {
            u32 prio = (u32)cur->current_priority;
            TmanListNode *head = &g_tman.ready_queues[prio];
            tmanListInsertBefore(head->next, &cur->link);
            g_tman.ready_bitmap[prio >> 5] |= tmanRor32(0x80000000U, prio);
            cur->status = TMAN_TH_READY;
        }
        return;
    }

    int group;
    s32 best_prio = -1;
    for (group = 0; group < 4; group++) {
        if (g_tman.ready_bitmap[group] != 0) {
            best_prio = (s32)(group * 32 + (int)tmanClz(g_tman.ready_bitmap[group]));
            break;
        }
    }

    if (best_prio < 0) {
        if (cur->status != TMAN_TH_RUNNING) {
            Kprintf("Panic: not found executable Thread\n");
            tmanBreak();
        }
        return;
    }

    if (cur->status == TMAN_TH_RUNNING && cur->current_priority <= best_prio)
        return;

    TmanListNode *head = &g_tman.ready_queues[best_prio];
    TmanThreadCB *picked = (TmanThreadCB *)head->next;
    tmanRemoveReadyQueue(picked, (u32)best_prio);

    if (cur->status == TMAN_TH_RUNNING) {
        cur->status = TMAN_TH_READY;
        tmanAddReadyQueue(cur);
    }

    picked->status = TMAN_TH_RUNNING;
    g_tman.next_thread = picked;
}

void tmanSwitchVfpuContext(TmanThreadCB *th)
{
    if (tmanReadCfc0(8) != 0 && (TmanThreadContext *)tmanReadCfc0(8) != th->ctx)
        tmanSaveFpuContext((TmanThreadContext *)tmanReadCfc0(8));

    if (g_tman.vfpu_owner_ctx != th->vfpu_ctx) {
        s32 mode = (s32)(th->attr >> 31);
        if (g_tman.vfpu_owner_mode != mode) {
            tman_vfpu_flush();
            if (mode != 0)
                TMAN_REG_VFPU_CTRL &= ~0x20U;
            else
                TMAN_REG_VFPU_CTRL |= 0x20U;
            tmanSync();
            g_tman.vfpu_owner_mode = mode;
        }
        if (g_tman.vfpu_owner_ctx != NULL)
            tman_vfpu_save(g_tman.vfpu_owner_ctx);
        g_tman.vfpu_owner_ctx = th->vfpu_ctx;
        if (g_tman.vfpu_owner_ctx != NULL)
            tman_vfpu_restore(g_tman.vfpu_owner_ctx);
        g_tman.vfpu_switch_count++;
    }
}

void tmanThreadIsDead(SceSysmemUidCB *cb)
{
    TmanThreadCB *th = (TmanThreadCB *)tmanUidData(cb, g_tman.uid_thread);
    if (g_tman.dispatch_suspended != 0) {
        g_tman.dispatch_suspended = 0;
        Kprintf("thread_is_dead: Thread dispatch is disabled!\n");
    }
    if (th->link.next == th->link.prev && th->link.next != &th->link)
        g_tman.ready_bitmap[(u32)th->current_priority >> 5] &= ~tmanRor32(0x80000000U, (u32)th->current_priority);
    tmanListRemove(&th->link);

    tmanWakeupAllWaiters(cb, (s32)SCE_ERROR_KERNEL_THREAD_IS_TERMINATED);
    tmanCleanupActiveEventHandler(th);
    tmanCleanupActiveCallback(th);
    tmanFreeThreadKtls(th);
    tmanReleaseOwnedEventFlags(th);
    tmanReleaseOwnedMutexes(th);
    tmanDeleteThreadCallbacks(th);
    tmanFreeThreadTlspl(th);

    const char *name = (cb->name != NULL) ? cb->name : "";
    Kprintf("Thread \"%s\" (thid=0x%08x) is DEAD\n", name, th->uid);
    TmanThreadContext *ctx = th->ctx;
    if (ctx != NULL && ctx->status_flag == 3) {
        Kprintf(" Saved registers:\n");
        Kprintf(" at=%08x  v0-1=%08x,%08x  a0-3=%08x,%08x,%08x,%08x\n",
                ctx->gpr[1], ctx->gpr[2], ctx->gpr[3],
                ctx->gpr[4], ctx->gpr[5], ctx->gpr[6], ctx->gpr[7]);
        Kprintf(" t0-7=%08x,%08x,%08x,%08x, %08x,%08x,%08x,%08x\n",
                ctx->gpr[8], ctx->gpr[9], ctx->gpr[10], ctx->gpr[11],
                ctx->gpr[12], ctx->gpr[13], ctx->gpr[14], ctx->gpr[15]);
        Kprintf(" s0-7=%08x,%08x,%08x,%08x, %08x,%08x,%08x,%08x\n",
                ctx->gpr[16], ctx->gpr[17], ctx->gpr[18], ctx->gpr[19],
                ctx->gpr[20], ctx->gpr[21], ctx->gpr[22], ctx->gpr[23]);
        Kprintf(" t8=%08x t9=%08x\tk0=%08x k1=%08x\t  gp=%08x sp=%08x\n",
                ctx->gpr[24], ctx->gpr[25], ctx->gpr[26], ctx->gpr[27],
                ctx->gpr[28], ctx->gpr[29]);
        Kprintf(" fp=%08x ra=%08x\tlo=%08x hi=%08x\t  PC=%08x\n",
                ctx->gpr[30], ctx->gpr[31], ctx->lo, ctx->hi, ctx->epc);
    }
    if (th->syscall_stack != NULL) {
        Kprintf(" Syscall return address = 0x%08x, sp = 0x%08x\n",
                th->syscall_stack->user_ra, th->syscall_stack->user_sp);
    }
    th->status = TMAN_TH_KILLED;
    tmanListInsertTail(&g_tman.dormant_list, &th->link);
    if (g_tman.dead_thread_handler != NULL)
        g_tman.dead_thread_handler(th->uid);
    g_tman.next_thread = NULL;
}

void tmanHandleStackOverflow(TmanThreadCB *th, int is_kernel, u32 sp)
{
    SceSysmemUidCB *cb = tmanDataToUid(th, g_tman.uid_thread);
    void *base = is_kernel ? th->kstack : th->stack;
    u32 sz = is_kernel ? th->kstack_size : th->stack_size;
    Kprintf("\nThread (thid=0x%08x) %sstack overflow\n Stack = 0x%08x, Stack size = 0x%08x, SP = 0x%08x\n",
            th->uid, is_kernel ? "kernel " : "", (u32)base, sz, sp);
    tmanThreadIsDead(cb);
}

static int tmanK0CorruptedHandler(TmanThreadCB *th)
{
    SceSysmemUidCB *cb = tmanDataToUid(th, g_tman.uid_thread);
    Kprintf("\nThread (thid=0x%08x) k0 reg corrupted\n", th->uid);
    tmanThreadIsDead(cb);
    return 0;
}

TmanThreadContext *tmanSwitchContext(TmanThreadContext *cur_ctx, int reason)
{
    if (g_tman.dispatch_suspended != 0)
        return cur_ctx;

    TmanThreadCB *cur = g_tman.current_thread;
    if (cur->profiler_regs != NULL) {
        u32 *dst = (u32 *)cur->profiler_regs + 4;
        int idx;
        for (idx = 0; idx < 21; idx++)
            dst[idx] = TMAN_REG_PROF_BASE[idx];
    }

    if (cur->status != TMAN_TH_KILLED) {
        if ((s32)cur->attr < 0) {
            u32 user_sp;
            u32 kern_sp;
            if ((cur->ctx->sr & 0x18U) != 0) {
                user_sp = cur->ctx->gpr[29];
                kern_sp = (u32)cur->kstack_top;
            } else {
                kern_sp = cur->ctx->gpr[29];
                user_sp = (cur->syscall_stack != NULL) ? cur->syscall_stack->user_sp : 0xFFFFFFFFU;
            }
            if (*(u32 *)cur->stack != (u32)cur->uid || user_sp < (u32)cur->stack)
                tmanHandleStackOverflow(cur, 0, user_sp);
            if (*(u32 *)cur->kstack != (u32)cur->uid || kern_sp < (u32)cur->kstack)
                tmanHandleStackOverflow(cur, 1, kern_sp);
            if ((u32)cur->user_tls != cur->ctx->gpr[26])
                tmanK0CorruptedHandler(cur);
        } else {
            u32 sp = cur->ctx->gpr[29];
            if (*(u32 *)cur->stack != (u32)cur->uid || sp < (u32)cur->stack)
                tmanHandleStackOverflow(cur, 1, sp);
        }
    }

    if (g_tman.next_thread == NULL)
        tmanSelectNextThread();

    TmanThreadCB *next = g_tman.next_thread;
    if (cur == next) {
        g_tman.noswitch_count++;
    } else {
        u64 now = tmanGetSystemTimeWide();
        u64 elapsed = now - g_tman.last_switch_time;
        g_tman.last_switch_time = now;
        u64 total = (((u64)cur->run_clocks_hi << 32) | cur->run_clocks_lo) + elapsed;
        cur->run_clocks_lo = (u32)total;
        cur->run_clocks_hi = (u32)(total >> 32);
        if (cur == g_tman.idle_thread)
            g_tman.comes_out_of_idle_count++;

        g_tman.switch_count++;
        if (cur->stat_counter_ptr != NULL)
            (*cur->stat_counter_ptr)++;

        if ((next->ctx->sr & 0x40000000U) != 0 && g_tman.vfpu_owner_ctx != next->vfpu_ctx)
            reason |= 0x100;

        if (g_tman.log_handler != NULL)
            g_tman.log_handler(1, 0, 5, cur->uid, cur->current_priority,
                               next->uid, next->current_priority, reason);

        g_tman.current_thread = next;
        cur = next;
    }

    if (cur->profiler_regs != NULL) {
        const u32 *src = (const u32 *)cur->profiler_regs + 4;
        int idx;
        for (idx = 0; idx < 21; idx++)
            TMAN_REG_PROF_BASE[idx] = src[idx];
        tmanSync();
    }

    tmanWriteCtc0(25, 0);
    if ((cur->ctx->sr & 0x40000000U) != 0)
        tmanSwitchVfpuContext(cur);
    tmanWriteCtc0(14, 0);
    tmanWriteCtc0(15, 0);
    tmanWriteCtc0(16, 0);
    return cur->ctx;
}

/* ============================================================================
 * Boot, Cleaner Thread, Reboot
 * ============================================================================ */

static int tmanSwiIntrHandler(int intr_num, void *arg)
{
    (void)intr_num;
    (void)arg;
    return -1;
}

static int tmanBootCallback(void *arg)
{
    (void)arg;
    sceKernelExitDeleteThread(0);
    return 0;
}

static void tmanThreadExitTrampoline(void)
{
    s32 exit_code;
    __asm__ volatile("move %0, $2" : "=r"(exit_code));
    sceKernelExitThread(exit_code);
}

static void tmanThreadStartEntry(void)
{
    TmanThreadCB *cur = g_tman.current_thread;
    int intr = sceKernelCpuSuspendIntr();
    tmanDispatchThreadEvents(TMAN_THREV_START, cur, intr);

    tmanInitThreadContext(cur->ctx);
    TmanThreadContext *ctx = cur->ctx;
    ctx->status_flag = 3;
    ctx->gpr[4] = cur->arg_len;
    ctx->gpr[5] = (u32)cur->arg_ptr;
    ctx->gpr[26] = (u32)cur->user_tls;
    ctx->gpr[28] = cur->gp_reg;
    ctx->gpr[29] = (u32)cur->init_sp;
    ctx->gpr[30] = (u32)cur->init_sp;
    ctx->gpr[31] = ((s32)cur->attr < 0) ? (u32)cur->init_sp : (u32)tmanThreadExitTrampoline;
    ctx->sr = ((s32)cur->attr < 0) ? 19U : 3U;
    ctx->epc = (u32)cur->entry;
    if ((s32)cur->attr < 0 && cur->user_tls != NULL)
        ((u32 *)cur->user_tls)[49] = 0;

    cur->attr = (cur->attr & ~SCE_KERNEL_TH_USE_VFPU) | (cur->init_attr & SCE_KERNEL_TH_USE_VFPU);
    if (cur->vfpu_ctx != NULL) {
        u32 *vctx = (u32 *)cur->vfpu_ctx;
        int idx;
        for (idx = 0; idx < 128; idx++)
            vctx[idx] = 0x7F800001U;
        vctx[128] = 228;
        vctx[129] = 228;
        vctx[130] = 0;
        vctx[131] = 63;
        vctx[132] = 0;
        vctx[136] = 0x3F800001U;
        vctx[137] = 0x3F800002U;
        vctx[138] = 0x3F800004U;
        vctx[139] = 0x3F800008U;
        vctx[140] = 0x3F800000U;
        vctx[141] = 0x3F800000U;
        vctx[142] = 0x3F800000U;
        vctx[143] = 0x3F800000U;
        if (g_tman.vfpu_owner_ctx == cur->vfpu_ctx)
            g_tman.vfpu_owner_ctx = NULL;
    }
    if ((cur->attr & SCE_KERNEL_TH_USE_VFPU) != 0) {
        cur->ctx->sr |= 0x40000000U;
        tmanSwitchVfpuContext(cur);
    }
    InterruptManagerForKernel_551097F2(cur->ctx);
}

static void tmanCleanerThreadEntry(SceSize args, void *argp)
{
    (void)args;
    (void)argp;
    for (;;) {
        int intr = sceKernelCpuSuspendIntr();
        TmanCleanerNode *node = &g_tman.cleaner_head;
        while (node != NULL) {
            int idx;
            for (idx = 0; idx < node->count; idx++) {
                SceUID uid = node->uids[idx];
                SceSysmemUidCB *cb = NULL;
                if (sceKernelGetUIDcontrolBlockWithType(uid, g_tman.uid_thread, &cb) == 0) {
                    TmanThreadCB *th = (TmanThreadCB *)tmanUidData(cb, g_tman.uid_thread);
                    tmanFreeThreadResources(th);
                }
                sceKernelDeleteUID(uid);
            }
            TmanCleanerNode *next = node->next;
            node->count = 0;
            node->next = NULL;
            if (node != &g_tman.cleaner_head)
                sceKernelFreeHeapMemory(g_tman.heap_id, node);
            node = next;
        }
        g_tman.current_thread->status = TMAN_TH_WAIT;
        g_tman.next_thread = NULL;
        tmanSchedule(1, 1);
        sceKernelCpuResumeIntr(intr);
    }
}

int tmanQueueThreadForCleaner(SceUID thid)
{
    TmanCleanerNode *node = &g_tman.cleaner_head;
    for (;;) {
        int idx;
        for (idx = 0; idx < node->count; idx++) {
            if (node->uids[idx] == thid)
                return (int)SCE_ERROR_KERNEL_NOT_FOUND_THREAD;
        }
        if (node->count < 8) {
            node->uids[node->count++] = thid;
            g_tman.next_thread = NULL;
            g_tman.cleaner_needed = 1;
            return 0;
        }
        if (node->next == NULL) {
            TmanCleanerNode *fresh = (TmanCleanerNode *)sceKernelAllocHeapMemory(
                g_tman.heap_id, sizeof(TmanCleanerNode));
            if (fresh == NULL)
                return (int)SCE_ERROR_KERNEL_NO_MEMORY;
            fresh->next = NULL;
            fresh->count = 0;
            node->next = fresh;
        }
        node = node->next;
    }
}

void tmanFreeThreadResources(TmanThreadCB *th)
{
    tmanDeleteThreadCallbacks(th);
    tmanFreeThreadKtls(th);

    if (th->vfpu_ctx != NULL) {
        if (g_tman.vfpu_owner_ctx == th->vfpu_ctx)
            g_tman.vfpu_owner_ctx = NULL;
        sceKernelFreeHeapMemory(g_tman.heap_id, th->vfpu_ctx);
        th->vfpu_ctx = NULL;
    }
    if (th->ctx != NULL) {
        if ((TmanThreadContext *)tmanReadCfc0(8) == th->ctx)
            tmanWriteCtc0(8, 0);
        sceKernelFreeHeapMemory(g_tman.heap_id, th->ctx);
        th->ctx = NULL;
    }
    if (th->profiler_regs != NULL) {
        sceKernelFreeHeapMemory(g_tman.heap_id, th->profiler_regs);
        th->profiler_regs = NULL;
    }
    if (th->stack_block_id > 0) {
        if ((th->init_attr & SCE_KERNEL_TH_CLEAR_STACK) != 0 && th->stack != NULL)
            sceKernelMemset32(th->stack, 0, th->stack_size);
        sceKernelFreePartitionMemory(th->stack_block_id);
        th->stack_block_id = 0;
    }
    if (th->kstack_block_id > 0) {
        sceKernelFreePartitionMemory(th->kstack_block_id);
        th->kstack_block_id = 0;
    }
}

int module_bootstart(SceSize args, void *argp)
{
    (void)args;
    (void)argp;
    int intr = sceKernelCpuSuspendIntr();
    u32 gp_val = tmanGetGP();
    u32 align_opt[2] = { 8, 64 };

    memset(&g_tman, 0, sizeof(g_tman));
    g_tman.cpu_prid = tmanMfc0(22);
    g_tman.uid_xor_key = (u32)sceKernelGetInitialRandomValue();

    tmanInitWaitQueueType();
    tmanInitThreadType();
    tmanInitEventAndCallbackTypes();
    tmanInitSemaType();
    tmanInitEventFlagType();
    tmanInitMutexType();
    tmanInitLwMutexType();
    tmanInitMbxType();
    tmanInitMsgPipeType();
    tmanInitVplType();
    tmanInitFplType();
    tmanInitTlsplType();
    tmanInitTimerTypes();

    tmanListInit(&g_tman.sleeping_list);
    tmanListInit(&g_tman.delayed_list);
    tmanListInit(&g_tman.stopped_list);
    tmanListInit(&g_tman.suspended_list);
    tmanListInit(&g_tman.dormant_list);
    tmanListInit(&g_tman.event_handler_list);

    int prio;
    for (prio = 0; prio < 128; prio++)
        tmanListInit(&g_tman.ready_queues[prio]);

    g_tman.vfpu_owner_mode = -1;
    if (sceKernelDipsw(23) != 0) {
        TMAN_REG_SYS_CTRL_30 |= 0x300U;
        Kprintf("CPU Profiler enabled: ");
        if (sceKernelDipsw(22) != 0) {
            g_tman.profiler_mode = 1;
            Kprintf("Thread Mode\n");
        } else {
            g_tman.profiler_mode = 2;
            Kprintf("Global Mode\n");
        }
    } else if (sceKernelIsDevelopmentToolMode() != 0 && sceKernelDipsw(26) == 0) {
        g_tman.profiler_mode = (sceKernelDipsw(22) != 0) ? 3U : 4U;
    }

    g_tman.heap_id = sceKernelCreateHeap(1, 2048, 1, "SceThreadman");

    /* Create Idle thread */
    SceSysmemUidCB *idle_cb = NULL;
    sceKernelCreateUID(g_tman.uid_thread, "SceThreadmanIdle", 0, &idle_cb);
    TmanThreadCB *idle = (TmanThreadCB *)tmanUidData(idle_cb, g_tman.uid_thread);
    idle->stack_size = 256;
    idle->stack_block_id = sceKernelAllocPartitionMemory(1, "stack:SceThreadmanIdle", 0, 256, NULL);
    idle->kstack_partition_id = 1;
    idle->stack = sceKernelGetBlockHeadAddr(idle->stack_block_id);
    *(u32 *)idle->stack = (u32)idle->uid;
    idle->status = TMAN_TH_READY;
    idle->current_priority = 127;
    idle->init_priority = 127;
    idle->base_priority = 127;
    idle->inherited_priority = 127;
    idle->gp_reg = gp_val;
    idle->entry = (g_tman.profiler_mode != 0) ? (void *)tman_idle_halt : (void *)tman_idle_loop;
    idle->ctx = (TmanThreadContext *)sceKernelAllocHeapMemoryWithOption(g_tman.heap_id, sizeof(TmanThreadContext), align_opt);
    tmanInitThreadContext(idle->ctx);
    idle->ctx->status_flag = 3;
    idle->ctx->sr = 3;
    idle->ctx->epc = (u32)idle->entry;
    idle->ctx->gpr[28] = gp_val;
    idle->ctx->gpr[29] = (u32)idle->stack + idle->stack_size;
    idle->ctx->gpr[30] = idle->ctx->gpr[29];
    idle->ctx->gpr[31] = (u32)tmanThreadExitTrampoline;
    g_tman.idle_thread = idle;
    tmanAddReadyQueue(idle);

    /* Create Cleaner thread */
    SceSysmemUidCB *cleaner_cb = NULL;
    sceKernelCreateUID(g_tman.uid_thread, "SceThreadmanCleaner", 0, &cleaner_cb);
    TmanThreadCB *cleaner = (TmanThreadCB *)tmanUidData(cleaner_cb, g_tman.uid_thread);
    cleaner->stack_size = 2048;
    cleaner->stack_block_id = sceKernelAllocPartitionMemory(1, "stack:SceThreadmanCleaner", 1, 2048, NULL);
    cleaner->kstack_partition_id = 1;
    cleaner->stack = sceKernelGetBlockHeadAddr(cleaner->stack_block_id);
    *(u32 *)cleaner->stack = (u32)cleaner->uid;
    cleaner->status = TMAN_TH_WAIT;
    cleaner->current_priority = 1;
    cleaner->init_priority = 1;
    cleaner->base_priority = 1;
    cleaner->inherited_priority = 127;
    cleaner->gp_reg = gp_val;
    cleaner->entry = (void *)tmanCleanerThreadEntry;
    cleaner->ctx = (TmanThreadContext *)sceKernelAllocHeapMemoryWithOption(g_tman.heap_id, sizeof(TmanThreadContext), align_opt);
    tmanInitThreadContext(cleaner->ctx);
    cleaner->ctx->status_flag = 3;
    cleaner->ctx->sr = 3;
    cleaner->ctx->epc = (u32)cleaner->entry;
    cleaner->ctx->gpr[28] = gp_val;
    cleaner->ctx->gpr[29] = (u32)cleaner->stack + cleaner->stack_size;
    cleaner->ctx->gpr[30] = cleaner->ctx->gpr[29];
    cleaner->ctx->gpr[31] = (u32)tmanThreadExitTrampoline;
    g_tman.cleaner_thread = cleaner;

    /* Create Init thread representing the current boot stack */
    SceSysmemUidCB *init_cb = NULL;
    sceKernelCreateUID(g_tman.uid_thread, "SceThreadmanInit", 0, &init_cb);
    TmanThreadCB *init = (TmanThreadCB *)tmanUidData(init_cb, g_tman.uid_thread);
    SceUID cur_stack_id = 0;
    u32 part_id = 0;
    u32 block_info[14];
    sceKernelQueryMemoryInfo(align_opt, &part_id, &cur_stack_id);
    block_info[0] = sizeof(block_info);
    sceKernelQueryMemoryBlockInfo(cur_stack_id, block_info);
    init->stack_block_id = cur_stack_id;
    init->kstack_partition_id = 1;
    init->stack = (void *)block_info[10];
    init->stack_size = block_info[11];
    *(u32 *)init->stack = (u32)init->uid;
    init->status = TMAN_TH_RUNNING;
    init->current_priority = 1;
    init->init_priority = 32;
    init->base_priority = 32;
    init->inherited_priority = 127;
    init->gp_reg = gp_val;
    init->ctx = (TmanThreadContext *)sceKernelAllocHeapMemoryWithOption(g_tman.heap_id, sizeof(TmanThreadContext), align_opt);
    tmanInitThreadContext(init->ctx);
    g_tman.current_thread = init;
    g_tman.next_thread = init;

    tmanWriteCtc0(14, 0);
    tmanWriteCtc0(15, 0);
    tmanWriteCtc0(16, 0);
    InterruptManagerForKernel_E790EAED((void *)tmanNeedReschedule, (void *)tmanSwitchContext);
    tmanInitSysTimer();
    sceKernelRegisterIntrHandler(65, 0, (void *)tmanSwiIntrHandler, NULL, NULL);
    sceKernelEnableIntr(65);
    sceKernelRegisterExceptionHandler(11, (void *)tman_fpu_exc_handler);
    tmanWriteCtc0(8, 0);

    g_tman.syscall_exit_thread = (u32)sceKernelQuerySystemCall((void *)sceKernelExitThread);
    g_tman.syscall_ret_change_stack = (u32)sceKernelQuerySystemCall((void *)_sceKernelReturnFromChangeStack);
    g_tman.syscall_ret_callback = (u32)sceKernelQuerySystemCall((void *)_sceKernelReturnFromCallback);
    g_tman.sys_stat_evf = sceKernelCreateEventFlag("SceThreadmanSysStat", 0x200, 0, NULL);
    sceKernelSetBootCallbackLevel((void *)tmanBootCallback, 3, 0);

    sceKernelCpuResumeIntr(intr);
    sceKernelCpuEnableIntr();
    return 0;
}

int module_reboot_before(SceSize args, void *argp)
{
    (void)args;
    (void)argp;
    int intr = sceKernelCpuSuspendIntr();
    sceKernelReleaseExceptionHandler(11, (void *)tman_fpu_exc_handler);
    sceKernelReleaseIntrHandler(g_tman.sys_timer_intr);
    sceKernelReleaseIntrHandler(65);
    sceKernelCpuResumeIntr(intr);
    return 0;
}

/* ============================================================================
 * Thread Lifecycle APIs
 * ============================================================================ */

SceUID sceKernelCreateThread(const char *name, SceKernelThreadEntry entry, s32 init_priority,
                             SceSize stack_size, SceUInt attr, SceKernelThreadOptParam *option)
{
    int is_user = tmanIsUserCaller();
    if (!tmanCheckUserAddr((u32)name) || !tmanCheckUserAddr((u32)entry) ||
        !tmanCheckUserPtr(option, sizeof(*option)))
        return (SceUID)SCE_ERROR_KERNEL_ILLEGAL_ADDR;
    if (tmanReadIntrDepth() != 0 || g_tman.current_thread->event_mask_active != 0)
        return (SceUID)SCE_ERROR_KERNEL_CANNOT_BE_CALLED_FROM_INTERRUPT;
    if ((attr & 0x070F9F00U) != 0)
        return (SceUID)SCE_ERROR_KERNEL_ILLEGAL_ATTR;

    u32 eff_attr = attr;
    if (is_user) {
        if ((attr & SCE_KERNEL_TH_UNK_800000) != 0)
            eff_attr = (attr & 0x077FFFFFU) | SCE_KERNEL_TH_USER_MODE;
        else
            eff_attr = (attr & 0x07FFFFFFU) | (g_tman.current_thread->attr & 0xF0000000U);
    }

    if ((u32)(init_priority - 1) >= 126U)
        return (SceUID)SCE_ERROR_KERNEL_ILLEGAL_PRIORITY;
    if ((s32)eff_attr < 0) {
        eff_attr |= 0xFFU;
        if ((u32)(init_priority - 8) >= 112U)
            return (SceUID)SCE_ERROR_KERNEL_ILLEGAL_PRIORITY;
    }
    if (((u32)entry & 3U) != 0)
        return (SceUID)SCE_ERROR_KERNEL_ILLEGAL_THREAD_ENTRY_ADDR;
    if (stack_size < 0x200U)
        return (SceUID)SCE_ERROR_KERNEL_ILLEGAL_STACK_SIZE;

    int intr = sceKernelCpuSuspendIntr();
    SceKernelThreadOptParam opt_copy = { 0, 0 };
    if (option != NULL) {
        SceSize cp = option->size < sizeof(opt_copy) ? option->size : sizeof(opt_copy);
        memcpy(&opt_copy, option, cp);
    }

    SceSysmemUidCB *cb = NULL;
    int ret = sceKernelCreateUID(g_tman.uid_thread, name,
                                 (char)((eff_attr & 0xFFU) | (is_user ? 0xFFU : 0x00U)), &cb);
    if (ret != 0) {
        sceKernelCpuResumeIntr(intr);
        return ret;
    }

    TmanThreadCB *th = (TmanThreadCB *)tmanUidData(cb, g_tman.uid_thread);
    SceUID uid = th->uid;
    th->opt_param = option;
    th->inherited_priority = 127;
    th->current_priority = init_priority;
    th->attr = eff_attr;
    th->init_attr = eff_attr;
    th->entry = (void *)entry;
    th->init_priority = (s16)init_priority;
    th->base_priority = (s16)init_priority;
    th->gp_reg = (u32)sceKernelGetModuleGPByAddressForKernel((u32)entry);
    if (th->gp_reg == 0)
        th->gp_reg = tmanGetGP();

    if ((s32)eff_attr >= 0 && (s32)(u32)entry >= 0) {
        sceKernelDeleteUID(uid);
        sceKernelCpuResumeIntr(intr);
        return (SceUID)SCE_ERROR_KERNEL_ILLEGAL_ADDR;
    }

    char stack_name[32];
    snprintf(stack_name, sizeof(stack_name) - 1, "stack:%s", name);
    SceUID mpid = opt_copy.stackMpid;
    if (mpid == 0)
        mpid = ((s32)eff_attr < 0) ? 2 : 1;

    u32 part_info[4] = { sizeof(part_info), 0, 0, 0 };
    ret = sceKernelQueryMemoryPartitionInfo(mpid, part_info);
    if (ret != 0) {
        sceKernelDeleteUID(uid);
        sceKernelCpuResumeIntr(intr);
        return ret;
    }

    if (((s32)eff_attr >= 0 && (part_info[3] & 3U) != 0) ||
        ((s32)eff_attr < 0 && (part_info[3] & 3U) != 3U)) {
        sceKernelDeleteUID(uid);
        sceKernelCpuResumeIntr(intr);
        return (SceUID)SCE_ERROR_KERNEL_ILLEGAL_PERMISSION;
    }

    u32 aligned_stack = (stack_size + 255U) & ~255U;
    int alloc_type = ((eff_attr & SCE_KERNEL_TH_LOW_STACK) != 0) ? 0 : 1;
    SceUID stack_id = sceKernelAllocPartitionMemory(mpid, stack_name, alloc_type, aligned_stack, NULL);
    if (stack_id <= 0) {
        sceKernelDeleteUID(uid);
        sceKernelCpuResumeIntr(intr);
        return (SceUID)SCE_ERROR_KERNEL_NO_MEMORY;
    }

    th->stack_block_id = stack_id;
    th->stack_partition_id = mpid;
    th->kstack_partition_id = ((s32)eff_attr < 0) ? 1 : mpid;
    th->stack_size = aligned_stack;
    th->stack = sceKernelGetBlockHeadAddr(stack_id);

    if ((s32)eff_attr < 0) {
        snprintf(stack_name, sizeof(stack_name) - 1, "kstack:%s", name);
        SceUID kstack_id = sceKernelAllocPartitionMemory(1, stack_name, 1, 2048, NULL);
        if (kstack_id <= 0) {
            tmanFreeThreadResources(th);
            sceKernelDeleteUID(uid);
            sceKernelCpuResumeIntr(intr);
            return (SceUID)SCE_ERROR_KERNEL_NO_MEMORY;
        }
        th->kstack_size = 2048;
        th->kstack_block_id = kstack_id;
        th->kstack = sceKernelGetBlockHeadAddr(kstack_id);
        if (g_tman.profiler_mode == 1 || g_tman.profiler_mode == 3) {
            u32 *prof = (u32 *)sceKernelAllocHeapMemory(g_tman.heap_id, 100);
            if (prof == NULL) {
                tmanFreeThreadResources(th);
                sceKernelDeleteUID(uid);
                sceKernelCpuResumeIntr(intr);
                return (SceUID)SCE_ERROR_KERNEL_NO_MEMORY;
            }
            memset(prof, 0, 100);
            prof[0] = 0xBC400000U;
            prof[1] = 1;
            prof[2] = 1;
            th->profiler_regs = prof;
        }
    }

    u32 align_opt[2] = { 8, 64 };
    th->ctx = (TmanThreadContext *)sceKernelAllocHeapMemoryWithOption(g_tman.heap_id, sizeof(TmanThreadContext), align_opt);
    if (th->ctx == NULL) {
        tmanFreeThreadResources(th);
        sceKernelDeleteUID(uid);
        sceKernelCpuResumeIntr(intr);
        return (SceUID)SCE_ERROR_KERNEL_NO_MEMORY;
    }

    if ((eff_attr & SCE_KERNEL_TH_USE_VFPU) != 0) {
        th->vfpu_ctx = sceKernelAllocHeapMemoryWithOption(g_tman.heap_id, 576, align_opt);
        if (th->vfpu_ctx == NULL) {
            tmanFreeThreadResources(th);
            sceKernelDeleteUID(uid);
            sceKernelCpuResumeIntr(intr);
            return (SceUID)SCE_ERROR_KERNEL_NO_MEMORY;
        }
    }

    th->exit_status = (s32)SCE_ERROR_KERNEL_THREAD_ALREADY_DORMANT;
    th->status = TMAN_TH_STOPPED;
    tmanListInsertTail(&g_tman.stopped_list, &th->link);
    sceKernelCpuResumeIntr(intr);

    if ((eff_attr & SCE_KERNEL_TH_NO_FILLSTACK) == 0) {
        sceKernelMemset32(th->stack, 0xFFFFFFFFU, th->stack_size);
        if ((s32)eff_attr < 0)
            sceKernelMemset32(th->kstack, 0xFFFFFFFFU, th->kstack_size);
    }

    intr = sceKernelCpuSuspendIntr();
    if (g_tman.log_handler != NULL)
        g_tman.log_handler(16, 64, 2, entry, uid);
    tmanDispatchThreadEvents(TMAN_THREV_CREATE, th, intr);
    sceKernelCpuResumeIntr(intr);
    return uid;
}

int sceKernelDeleteThread(SceUID thid)
{
    if (tmanReadIntrDepth() != 0 || g_tman.current_thread->event_mask_active != 0)
        return (int)SCE_ERROR_KERNEL_CANNOT_BE_CALLED_FROM_INTERRUPT;
    if (thid == 0)
        return (int)SCE_ERROR_KERNEL_THREAD_IS_NOT_DORMANT;

    int intr = sceKernelCpuSuspendIntr();
    SceSysmemUidCB *cb = NULL;
    if (sceKernelGetUIDcontrolBlockWithType(thid, g_tman.uid_thread, &cb) != 0) {
        sceKernelCpuResumeIntr(intr);
        return (int)SCE_ERROR_KERNEL_NOT_FOUND_THREAD;
    }
    if (!tmanCheckUidAttr(cb, 0x10)) {
        sceKernelCpuResumeIntr(intr);
        return (int)SCE_ERROR_KERNEL_ILLEGAL_PERMISSION;
    }

    TmanThreadCB *th = (TmanThreadCB *)tmanUidData(cb, g_tman.uid_thread);
    if (th == g_tman.idle_thread || th == g_tman.cleaner_thread) {
        sceKernelCpuResumeIntr(intr);
        return (int)SCE_ERROR_KERNEL_NOT_FOUND_THREAD;
    }
    if (th->status != TMAN_TH_STOPPED && th->status != TMAN_TH_KILLED) {
        sceKernelCpuResumeIntr(intr);
        return (int)SCE_ERROR_KERNEL_THREAD_IS_NOT_DORMANT;
    }
    if (th->event_mask_active != 0) {
        sceKernelCpuResumeIntr(intr);
        return (int)SCE_ERROR_KERNEL_CANNOT_BE_CALLED_FROM_INTERRUPT;
    }

    if (g_tman.log_handler != NULL)
        g_tman.log_handler(17, (u32)th, 1, thid);

    tmanDispatchThreadEvents(TMAN_THREV_DELETE, th, intr);
    tmanReleaseOwnedEventFlags(th);
    tmanReleaseOwnedMutexes(th);
    tmanListRemove(&th->link);
    th->status = 0;
    tmanFreeThreadResources(th);
    sceKernelDeleteUID(th->uid);
    sceKernelCpuResumeIntr(intr);
    return 0;
}

void tmanRestoreExtStacks(TmanThreadCB *th)
{
    TmanExtStackNode *ext = th->ext_kstack_list;
    while (ext != NULL) {
        SceUID old_blk;
        if ((s32)th->attr >= 0) {
            old_blk = th->stack_block_id;
            th->stack = ext->stack_base;
            th->stack_block_id = ext->block_id;
            th->stack_size = ext->stack_size;
        } else {
            old_blk = th->kstack_block_id;
            th->kstack = ext->stack_base;
            th->kstack_block_id = ext->block_id;
            th->kstack_size = ext->stack_size;
        }
        th->ext_kstack_list = ext->next;
        sceKernelFreePartitionMemory(old_blk);
        ext = th->ext_kstack_list;
    }
    if ((s32)th->attr < 0) {
        TmanExtStackNode *uext = th->ext_stack_list;
        while (uext != NULL) {
            SceUID old_blk = th->stack_block_id;
            th->stack = uext->stack_base;
            th->stack_block_id = uext->block_id;
            th->stack_size = uext->stack_size;
            th->ext_stack_list = uext->next;
            sceKernelFreeHeapMemory(g_tman.heap_id, uext);
            if (old_blk != 0)
                sceKernelFreePartitionMemory(old_blk);
            uext = th->ext_stack_list;
        }
    }
}

int sceKernelStartThread(SceUID thid, SceSize arglen, void *argp)
{
    if (!tmanCheckUserPtr(argp, arglen))
        return (int)SCE_ERROR_KERNEL_ILLEGAL_ADDR;
    if (tmanReadIntrDepth() != 0 || g_tman.current_thread->event_mask_active != 0)
        return (int)SCE_ERROR_KERNEL_CANNOT_BE_CALLED_FROM_INTERRUPT;
    if (thid == 0)
        return (int)SCE_ERROR_KERNEL_ILLEGAL_THREAD;

    int intr = sceKernelCpuSuspendIntr();
    SceSysmemUidCB *cb = NULL;
    if (sceKernelGetUIDcontrolBlockWithType(thid, g_tman.uid_thread, &cb) != 0) {
        sceKernelCpuResumeIntr(intr);
        return (int)SCE_ERROR_KERNEL_NOT_FOUND_THREAD;
    }
    if (!tmanCheckUidAttr(cb, 2)) {
        sceKernelCpuResumeIntr(intr);
        return (int)SCE_ERROR_KERNEL_ILLEGAL_PERMISSION;
    }

    TmanThreadCB *th = (TmanThreadCB *)tmanUidData(cb, g_tman.uid_thread);
    if (th->status != TMAN_TH_STOPPED) {
        sceKernelCpuResumeIntr(intr);
        return (int)SCE_ERROR_KERNEL_THREAD_IS_NOT_DORMANT;
    }
    if (th->event_mask_active != 0) {
        sceKernelCpuResumeIntr(intr);
        return (int)SCE_ERROR_KERNEL_CANNOT_BE_CALLED_FROM_INTERRUPT;
    }

    if (g_tman.log_handler != NULL)
        g_tman.log_handler(18, (u32)th, 3, thid, arglen, argp);

    u32 *sp = (u32 *)((u8 *)th->stack + (th->stack_size & ~3U));
    if ((s32)th->attr < 0) {
        sp -= 64;
        th->user_tls = sp;
        memset(sp, 0, 256);
        sp[48] = (u32)thid;
        sp[50] = (u32)th->stack;
        sp[62] = 0xFFFFFFFFU;
        sp[63] = 0xFFFFFFFFU;
    }
    *(u32 *)th->stack = (u32)th->uid;
    if ((s32)th->attr < 0)
        *(u32 *)th->kstack = (u32)th->uid;

    memset(th->ktls, 0, sizeof(th->ktls));
    if (argp != NULL && arglen != 0) {
        sp = (u32 *)((u8 *)sp - ((arglen + 15U) & ~15U));
        memcpy(sp, argp, arglen);
        th->arg_len = arglen;
        th->arg_ptr = sp;
    } else {
        th->arg_len = 0;
        th->arg_ptr = NULL;
    }

    if ((s32)th->attr < 0) {
        sp -= 16;
        sp[0] = ((g_tman.syscall_exit_thread << 6) & 0x03FFFFC0U) | 0x0CU;
        sp[1] = 0x0DU;
        sp[2] = 0x1000FFFFU;
        sp[3] = 0;
        tmanCacheOp(0x1BU, sp);
        tmanCacheOp(0x08U, sp);
        tmanSync();
    }

    th->attr = th->init_attr;
    th->inherited_priority = 127;
    th->current_priority = th->init_priority;
    th->base_priority = th->init_priority;
    th->wait_type = TMAN_WAIT_NONE;
    th->wait_obj = NULL;
    th->wakeup_count = 0;

    tmanInitThreadContext(th->ctx);
    th->init_sp = sp;
    th->ctx->status_flag = 3;
    th->ctx->gpr[26] = (u32)th->user_tls;
    if ((s32)th->attr < 0) {
        th->kstack_top = (u8 *)th->kstack + th->kstack_size;
        th->ctx->gpr[29] = (u32)th->kstack_top;
    } else {
        th->kstack_top = NULL;
        th->ctx->gpr[29] = (u32)sp;
    }
    th->ctx->gpr[30] = th->ctx->gpr[29];
    th->ctx->gpr[28] = th->gp_reg;
    th->ctx->epc = (u32)tmanThreadStartEntry;
    th->ctx->sr = 3;
    th->exit_status = (s32)SCE_ERROR_KERNEL_THREAD_IS_NOT_DORMANT;

    tmanListRemove(&th->link);
    if (g_tman.user_threads_suspended != 0 &&
        ((s32)th->attr < 0 || (th->attr & 0x08000000U) != 0)) {
        th->status = TMAN_TH_SUSPEND;
        tmanListInsertTail(&g_tman.suspended_list, &th->link);
        sceKernelCpuResumeIntr(intr);
        return 0;
    }

    tmanMakeReadyAndPreempt(th, intr);
    sceKernelCpuResumeIntr(intr);
    return 0;
}

void tmanExitCurrentThread(int delete_self, int intr_state, s32 exit_status)
{
    TmanThreadCB *cur = g_tman.current_thread;
    SceSysmemUidCB *cb = tmanDataToUid(cur, g_tman.uid_thread);

    cur->exit_status = exit_status;
    tmanCleanupActiveCallback(cur);
    tmanCleanupActiveEventHandler(cur);
    tmanDispatchThreadEvents(TMAN_THREV_EXIT, cur, intr_state);
    tmanReleaseOwnedEventFlags(cur);
    tmanReleaseOwnedMutexes(cur);
    tmanDeleteThreadCallbacks(cur);
    tmanFreeThreadTlspl(cur);
    tmanFreeThreadKtls(cur);
    tmanWakeupAllWaiters(cb, exit_status);

    if (delete_self) {
        cur->status = TMAN_TH_KILLED;
        tmanListInsertTail(&g_tman.dormant_list, &cur->link);
        tmanQueueThreadForCleaner(cur->uid);
    } else {
        cur->status = TMAN_TH_STOPPED;
        tmanListInsertTail(&g_tman.stopped_list, &cur->link);
    }

    g_tman.next_thread = NULL;
    tmanSchedule(1, 1);
}

int sceKernelExitThread(s32 status)
{
    if (tmanReadIntrDepth() != 0 || g_tman.current_thread->event_mask_active != 0)
        return (int)SCE_ERROR_KERNEL_CANNOT_BE_CALLED_FROM_INTERRUPT;

    if ((tmanGetIntrMask() == 0 || g_tman.dispatch_suspended != 0) &&
        sceKernelGetCompiledSdkVersion() > 0x0307FFFF)
        return (int)SCE_ERROR_KERNEL_WAIT_CAN_NOT_WAIT;

    if (status < 0)
        status = (s32)SCE_ERROR_KERNEL_ILLEGAL_ARGUMENT;

    int intr = sceKernelCpuSuspendIntr();
    if (g_tman.log_handler != NULL)
        g_tman.log_handler(19, (u32)status, 1, status);
    tmanExitCurrentThread(0, intr, status);
    return 0;
}

s32 sceKernelExitDeleteThread(s32 exit_status)
{
    if (tmanReadIntrDepth() != 0 || g_tman.current_thread->event_mask_active != 0)
        return (s32)SCE_ERROR_KERNEL_CANNOT_BE_CALLED_FROM_INTERRUPT;

    if ((tmanGetIntrMask() == 0 || g_tman.dispatch_suspended != 0) &&
        sceKernelGetCompiledSdkVersion() > 0x0307FFFF) {
        Kprintf("doExitDeleteThread: Thread dispatch is disabled!\n");
        return (s32)SCE_ERROR_KERNEL_WAIT_CAN_NOT_WAIT;
    }

    if (exit_status < 0)
        exit_status = (s32)SCE_ERROR_KERNEL_ILLEGAL_ARGUMENT;

    int intr = sceKernelCpuSuspendIntr();
    if (g_tman.log_handler != NULL)
        g_tman.log_handler(20, (u32)exit_status, 1, exit_status);
    tmanExitCurrentThread(1, intr, exit_status);
    return 0;
}

int _sceKernelExitThread(void)
{
    TmanThreadCB *cur = g_tman.current_thread;
    s32 status = (cur->syscall_stack != NULL) ? (s32)cur->syscall_stack->return_val : 0;
    int ret = sceKernelExitThread(status);
    tmanThreadIsDead(tmanDataToUid(cur, g_tman.uid_thread));
    return ret;
}

static int doTerminateDeleteThread(SceUID thid, int delete_after)
{
    if (tmanReadIntrDepth() != 0 || g_tman.current_thread->event_mask_active != 0)
        return (int)SCE_ERROR_KERNEL_CANNOT_BE_CALLED_FROM_INTERRUPT;
    if (thid == 0)
        return (int)SCE_ERROR_KERNEL_ILLEGAL_THREAD;

    int intr = sceKernelCpuSuspendIntr();
    if (g_tman.dispatch_suspended != 0) {
        Kprintf("doTerminateDeleteThread: Thread dispatch is disabled!\n");
        sceKernelCpuResumeIntr(intr);
        return (int)SCE_ERROR_KERNEL_WAIT_CAN_NOT_WAIT;
    }

    SceSysmemUidCB *cb = NULL;
    if (sceKernelGetUIDcontrolBlockWithType(thid, g_tman.uid_thread, &cb) != 0) {
        sceKernelCpuResumeIntr(intr);
        return (int)SCE_ERROR_KERNEL_NOT_FOUND_THREAD;
    }
    if (!tmanCheckUidAttr(cb, delete_after ? 0x10 : 2)) {
        sceKernelCpuResumeIntr(intr);
        return (int)SCE_ERROR_KERNEL_ILLEGAL_PERMISSION;
    }

    TmanThreadCB *th = (TmanThreadCB *)tmanUidData(cb, g_tman.uid_thread);
    if (th == g_tman.current_thread || th == g_tman.idle_thread || th == g_tman.cleaner_thread) {
        sceKernelCpuResumeIntr(intr);
        return (int)SCE_ERROR_KERNEL_ILLEGAL_THREAD;
    }
    if (th->status == TMAN_TH_STOPPED || th->status == TMAN_TH_KILLED) {
        sceKernelCpuResumeIntr(intr);
        return (int)SCE_ERROR_KERNEL_THREAD_ALREADY_DORMANT;
    }

    if (th->status == TMAN_TH_READY) {
        tmanRemoveReadyQueue(th, (u32)th->current_priority);
    } else if (th->status == TMAN_TH_WAIT || th->status == TMAN_TH_WAITSUSPEND) {
        tmanListRemove(&th->link);
        tmanClearThreadWait(th);
    } else if (th->status == TMAN_TH_SUSPEND) {
        tmanListRemove(&th->link);
    }

    th->exit_status = (s32)SCE_ERROR_KERNEL_THREAD_IS_TERMINATED;
    tmanCleanupActiveCallback(th);
    tmanCleanupActiveEventHandler(th);
    tmanReleaseOwnedEventFlags(th);
    tmanReleaseOwnedMutexes(th);
    tmanFreeThreadTlspl(th);
    tmanWakeupAllWaiters(cb, th->exit_status);

    if (delete_after) {
        th->status = 0;
        tmanFreeThreadResources(th);
        sceKernelDeleteUID(th->uid);
    } else {
        th->status = TMAN_TH_STOPPED;
        tmanListInsertTail(&g_tman.stopped_list, &th->link);
    }

    g_tman.next_thread = NULL;
    tmanSchedule(0, intr);
    sceKernelCpuResumeIntr(intr);
    return 0;
}

int sceKernelTerminateThread(SceUID thid)
{
    if (thid == 0)
        return (int)SCE_ERROR_KERNEL_ILLEGAL_THREAD;
    if (tmanReadIntrDepth() != 0 && sceKernelGetCompiledSdkVersion() > 0x0307FFFF)
        return (int)SCE_ERROR_KERNEL_CANNOT_BE_CALLED_FROM_INTERRUPT;
    if (g_tman.dispatch_suspended != 0 && sceKernelGetCompiledSdkVersion() > 0x0307FFFF)
        return (int)SCE_ERROR_KERNEL_WAIT_CAN_NOT_WAIT;

    int intr = sceKernelCpuSuspendIntr();
    SceSysmemUidCB *cb = NULL;
    if (sceKernelGetUIDcontrolBlockWithType(thid, g_tman.uid_thread, &cb) != 0) {
        sceKernelCpuResumeIntr(intr);
        return (int)SCE_ERROR_KERNEL_NOT_FOUND_THREAD;
    }
    if (!tmanCheckUidAttr(cb, 2)) {
        sceKernelCpuResumeIntr(intr);
        return (int)SCE_ERROR_KERNEL_ILLEGAL_PERMISSION;
    }
    if (g_tman.log_handler != NULL)
        g_tman.log_handler(21, (u32)intr, 1, cb->uid ^ (s32)g_tman.uid_xor_key);
    int ret = doTerminateDeleteThread(thid, 0);
    sceKernelCpuResumeIntr(intr);
    return ret;
}

int sceKernelTerminateDeleteThread(SceUID thid)
{
    if (thid == 0)
        return (int)SCE_ERROR_KERNEL_ILLEGAL_THREAD;
    if (g_tman.dispatch_suspended != 0 && sceKernelGetCompiledSdkVersion() > 0x0307FFFF)
        return (int)SCE_ERROR_KERNEL_WAIT_CAN_NOT_WAIT;

    int intr = sceKernelCpuSuspendIntr();
    SceSysmemUidCB *cb = NULL;
    if (sceKernelGetUIDcontrolBlockWithType(thid, g_tman.uid_thread, &cb) != 0) {
        sceKernelCpuResumeIntr(intr);
        return (int)SCE_ERROR_KERNEL_NOT_FOUND_THREAD;
    }
    if (!tmanCheckUidAttr(cb, 2)) {
        sceKernelCpuResumeIntr(intr);
        return (int)SCE_ERROR_KERNEL_ILLEGAL_PERMISSION;
    }
    if (g_tman.log_handler != NULL)
        g_tman.log_handler(22, (u32)intr, 1, cb->uid ^ (s32)g_tman.uid_xor_key);
    int ret = doTerminateDeleteThread(thid, 1);
    sceKernelCpuResumeIntr(intr);
    return ret;
}

int sceKernelSuspendDispatchThread(void)
{
    if (tmanReadIntrDepth() != 0)
        return (int)SCE_ERROR_KERNEL_CANNOT_BE_CALLED_FROM_INTERRUPT;

    int intr = sceKernelCpuSuspendIntr();
    if (intr == 0) {
        sceKernelCpuResumeIntr(0);
        return (int)0x80020066;
    }
    if (g_tman.log_handler != NULL)
        g_tman.log_handler(23, (u32)&g_tman, 0);
    int prev = 0;
    if (g_tman.dispatch_suspended == 0) {
        g_tman.dispatch_suspended = 1;
        prev = 1;
    }
    if ((s32)g_tman.current_thread->attr < 0 && g_tman.current_thread->user_tls != NULL)
        ((u32 *)g_tman.current_thread->user_tls)[49] = 1;
    sceKernelCpuResumeIntr(intr);
    return prev;
}

int tmanResumeDispatchInternal(int mode, int intr_state)
{
    TmanThreadCB *cur = g_tman.current_thread;
    if (mode == 0) {
        if (g_tman.dispatch_suspended == 0)
            g_tman.dispatch_suspended = 1;
        if ((s32)cur->attr < 0 && cur->user_tls != NULL)
            ((u32 *)cur->user_tls)[49] = 1;
        return 0;
    }
    int prev = g_tman.dispatch_suspended;
    g_tman.dispatch_suspended = 0;
    if ((s32)cur->attr < 0 && cur->user_tls != NULL)
        ((u32 *)cur->user_tls)[49] = 0;
    if (prev == 1) {
        g_tman.next_thread = NULL;
        tmanSchedule(0, intr_state);
    } else if (prev == 2) {
        cur->status = TMAN_TH_SUSPEND;
        tmanListInsertTail(&g_tman.suspended_list, &cur->link);
        g_tman.next_thread = NULL;
        tmanSchedule(1, 1);
    } else if (prev == 3) {
        tmanExitCurrentThread(0, 0, (s32)SCE_ERROR_KERNEL_THREAD_IS_TERMINATED);
    }
    return 0;
}

int sceKernelResumeDispatchThread(int state)
{
    if (tmanReadIntrDepth() != 0)
        return (int)SCE_ERROR_KERNEL_CANNOT_BE_CALLED_FROM_INTERRUPT;

    int intr = sceKernelCpuSuspendIntr();
    int prev = g_tman.dispatch_suspended;
    if (state == 0 && prev != 0)
        tmanResumeDispatchInternal(1, intr);
    sceKernelCpuResumeIntr(intr);
    return prev;
}

int sceKernelChangeCurrentThreadAttr(u32 clear_attr, u32 set_attr)
{
    TmanThreadCB *cur = g_tman.current_thread;
    if (tmanReadIntrDepth() != 0 || cur->event_mask_active != 0)
        return (int)SCE_ERROR_KERNEL_CANNOT_BE_CALLED_FROM_INTERRUPT;
    if (((clear_attr | set_attr) & ~SCE_KERNEL_TH_USE_VFPU) != 0)
        return (int)SCE_ERROR_KERNEL_ILLEGAL_ATTR;
    if (g_tman.log_handler != NULL)
        g_tman.log_handler(25, clear_attr, 2, clear_attr, set_attr);

    int intr = sceKernelCpuSuspendIntr();
    u32 new_attr = (cur->attr & ~clear_attr) | set_attr;
    cur->attr = new_attr;
    u32 sr = tmanMfc0(12);
    if ((new_attr & SCE_KERNEL_TH_USE_VFPU) != 0 && (sr & 0x40000000U) == 0) {
        if (cur->vfpu_ctx == NULL) {
            u32 align_opt[2] = { 8, 64 };
            u32 *vctx = (u32 *)sceKernelAllocHeapMemoryWithOption(g_tman.heap_id, 576, align_opt);
            if (vctx == NULL) {
                cur->attr &= ~SCE_KERNEL_TH_USE_VFPU;
                sceKernelCpuResumeIntr(intr);
                return (int)SCE_ERROR_KERNEL_NO_MEMORY;
            }
            int idx;
            for (idx = 0; idx < 128; idx++)
                vctx[idx] = 0x7F800001U;
            vctx[128] = 228;
            vctx[129] = 228;
            vctx[130] = 0;
            vctx[131] = 63;
            vctx[132] = 0;
            vctx[136] = 0x3F800001U;
            vctx[137] = 0x3F800002U;
            vctx[138] = 0x3F800004U;
            vctx[139] = 0x3F800008U;
            vctx[140] = 0x3F800000U;
            vctx[141] = 0x3F800000U;
            vctx[142] = 0x3F800000U;
            vctx[143] = 0x3F800000U;
            cur->vfpu_ctx = vctx;
        }
        cur->ctx->sr |= 0x40000000U;
        if (cur->syscall_stack != NULL)
            cur->syscall_stack->prev_kstack |= 0x40000000U;
        tmanMtc0(12, tmanMfc0(12) | 0x40000000U);
        tmanSwitchVfpuContext(cur);
    } else if ((cur->attr & SCE_KERNEL_TH_USE_VFPU) == 0 && (sr & 0x40000000U) != 0) {
        cur->ctx->sr &= ~0x40000000U;
        if (cur->syscall_stack != NULL)
            cur->syscall_stack->prev_kstack &= ~0x40000000U;
        tmanMtc0(12, tmanMfc0(12) & ~0x40000000U);
    }
    sceKernelCpuResumeIntr(intr);
    return 0;
}

int tmanChangeThreadPriorityInternal(TmanThreadCB *th, s32 new_prio, int from_user)
{
    int in_intr = (int)tmanReadIntrDepth();
    if (th == g_tman.current_thread) {
        th->current_priority = new_prio;
        if (g_tman.dispatch_suspended != 0 || in_intr != 0)
            return 0;
        if (from_user != 0)
            g_tman.next_thread = NULL;
        return 0;
    }

    if (th->status == TMAN_TH_READY) {
        tmanRemoveReadyQueue(th, (u32)th->current_priority);
        th->current_priority = new_prio;
        if (in_intr != 0 || g_tman.dispatch_suspended != 0) {
            tmanAddReadyQueue(th);
            g_tman.next_thread = NULL;
            return 0;
        }
        if (new_prio >= g_tman.current_thread->current_priority) {
            tmanAddReadyQueue(th);
            return 0;
        }
        tmanAddReadyQueue(th);
        g_tman.next_thread = NULL;
        return 0;
    }

    th->current_priority = new_prio;
    if ((th->status == TMAN_TH_WAIT || th->status == TMAN_TH_WAITSUSPEND) && th->wait_obj != NULL) {
        TmanWaitQueue *wq = (TmanWaitQueue *)tmanUidData(th->wait_obj, g_tman.uid_waitq);
        if ((wq->attr & SCE_KERNEL_AT_THPRI) != 0) {
            tmanListRemove(&th->link);
            tmanInsertThreadByPriority(th, th->wait_obj);
        }
        tmanNotifyWaitObject(th->wait_obj);
    }
    return 0;
}

int sceKernelChangeThreadPriority(SceUID thid, int priority)
{
    int in_intr = (int)tmanReadIntrDepth();
    int intr = sceKernelCpuSuspendIntr();
    TmanThreadCB *th;

    if (thid == 0) {
        if (in_intr != 0) {
            sceKernelCpuResumeIntr(intr);
            return (int)SCE_ERROR_KERNEL_ILLEGAL_THREAD;
        }
        th = g_tman.current_thread;
    } else {
        SceSysmemUidCB *cb = NULL;
        if (sceKernelGetUIDcontrolBlockWithType(thid, g_tman.uid_thread, &cb) != 0) {
            sceKernelCpuResumeIntr(intr);
            return (int)SCE_ERROR_KERNEL_NOT_FOUND_THREAD;
        }
        if (!tmanCheckUidAttr(cb, 2)) {
            sceKernelCpuResumeIntr(intr);
            return (int)SCE_ERROR_KERNEL_ILLEGAL_PERMISSION;
        }
        th = (TmanThreadCB *)tmanUidData(cb, g_tman.uid_thread);
    }

    if (priority == 0)
        priority = th->init_priority;
    if ((u32)(priority - 1) >= 126U || ((s32)th->attr < 0 && (u32)(priority - 8) >= 112U)) {
        sceKernelCpuResumeIntr(intr);
        return (int)SCE_ERROR_KERNEL_ILLEGAL_PRIORITY;
    }
    if (th->status == TMAN_TH_STOPPED || th->status == TMAN_TH_KILLED) {
        sceKernelCpuResumeIntr(intr);
        return (int)SCE_ERROR_KERNEL_THREAD_ALREADY_DORMANT;
    }

    th->base_priority = (s16)priority;
    s32 eff = (th->inherited_priority < priority) ? th->inherited_priority : priority;
    tmanChangeThreadPriorityInternal(th, eff, 1);
    if (in_intr == 0) {
        g_tman.next_thread = NULL;
        tmanSchedule(0, intr);
    }
    sceKernelCpuResumeIntr(intr);
    return 0;
}

int sceKernelRotateThreadReadyQueue(int priority)
{
    int in_intr = (int)tmanReadIntrDepth();
    if (priority == 0) {
        if (in_intr != 0)
            return (int)SCE_ERROR_KERNEL_ILLEGAL_PRIORITY;
        priority = g_tman.current_thread->current_priority;
    }
    if ((u32)(priority - 1) >= 126U)
        return (int)SCE_ERROR_KERNEL_ILLEGAL_PRIORITY;

    int intr = sceKernelCpuSuspendIntr();
    TmanListNode *head = &g_tman.ready_queues[priority];
    if (!tmanListEmpty(head)) {
        TmanThreadCB *cur = g_tman.current_thread;
        if (cur->current_priority == priority && cur->status == TMAN_TH_RUNNING) {
            cur->status = TMAN_TH_READY;
            tmanAddReadyQueue(cur);
            g_tman.next_thread = NULL;
            if (in_intr == 0)
                tmanSchedule(0, intr);
        } else {
            TmanListNode *first = head->next;
            tmanListRemove(first);
            tmanListInsertTail(head, first);
        }
    }
    sceKernelCpuResumeIntr(intr);
    return 0;
}

int sceKernelReleaseWaitThread(SceUID thid)
{
    if (thid == 0)
        return (int)SCE_ERROR_KERNEL_ILLEGAL_THREAD;

    int intr = sceKernelCpuSuspendIntr();
    int in_intr = (int)tmanReadIntrDepth();
    SceSysmemUidCB *cb = NULL;
    if (sceKernelGetUIDcontrolBlockWithType(thid, g_tman.uid_thread, &cb) != 0) {
        sceKernelCpuResumeIntr(intr);
        return (int)SCE_ERROR_KERNEL_NOT_FOUND_THREAD;
    }
    if (!tmanCheckUidAttr(cb, 2)) {
        sceKernelCpuResumeIntr(intr);
        return (int)SCE_ERROR_KERNEL_ILLEGAL_PERMISSION;
    }

    TmanThreadCB *th = (TmanThreadCB *)tmanUidData(cb, g_tman.uid_thread);
    if (th->status != TMAN_TH_WAIT && th->status != TMAN_TH_WAITSUSPEND) {
        sceKernelCpuResumeIntr(intr);
        return (int)SCE_ERROR_KERNEL_THREAD_IS_NOT_WAIT;
    }

    th->wait_result = (s32)SCE_ERROR_KERNEL_WAIT_STATUS_RELEASED;
    tmanWakeupAndReschedule(th, in_intr, intr);
    sceKernelCpuResumeIntr(intr);
    return 0;
}

int sceKernelGetThreadExitStatus(SceUID thid)
{
    if (thid == 0)
        return (int)SCE_ERROR_KERNEL_ILLEGAL_THREAD;

    int intr = sceKernelCpuSuspendIntr();
    SceSysmemUidCB *cb = NULL;
    if (sceKernelGetUIDcontrolBlockWithType(thid, g_tman.uid_thread, &cb) != 0) {
        sceKernelCpuResumeIntr(intr);
        return (int)SCE_ERROR_KERNEL_NOT_FOUND_THREAD;
    }
    if (!tmanCheckUidAttr(cb, 1)) {
        sceKernelCpuResumeIntr(intr);
        return (int)SCE_ERROR_KERNEL_ILLEGAL_PERMISSION;
    }

    TmanThreadCB *th = (TmanThreadCB *)tmanUidData(cb, g_tman.uid_thread);
    int status = th->exit_status;
    sceKernelCpuResumeIntr(intr);
    return status;
}

/* ============================================================================
 * Stack Extension & Stack Checking
 * ============================================================================ */

int _sceKernelReturnFromChangeStack(void)
{
    TmanThreadCB *cur = g_tman.current_thread;
    if (tmanReadIntrDepth() != 0 || cur->ext_stack_list == NULL)
        tmanBreak();

    int intr = sceKernelCpuSuspendIntr();
    TmanExtStackNode *node = cur->ext_stack_list;
    TmanSyscallStack *ss = cur->syscall_stack;
    u32 old_size = cur->stack_size;
    SceUID old_blk = cur->stack_block_id;
    void *old_stack = cur->stack;
    int ret_val = (int)ss->return_val;

    ss->saved_sp = node->saved_sp;
    ss->user_ra = node->saved_user_ra;
    ss->user_sp = node->saved_user_sp;
    cur->ext_stack_list = node->next;
    cur->stack = node->stack_base;
    cur->stack_block_id = node->block_id;
    cur->stack_size = node->stack_size;
    sceKernelCpuResumeIntr(intr);

    if ((cur->attr & SCE_KERNEL_TH_CLEAR_STACK) != 0)
        sceKernelMemset32(old_stack, 0, old_size);
    if (old_blk != 0)
        sceKernelFreePartitionMemory(old_blk);
    sceKernelFreeHeapMemory(g_tman.heap_id, node);
    return ret_val;
}

int tmanExtendThreadStackCommon(SceUID block_id, void *stack_base, u32 stack_size,
                                void *entry, void *arg)
{
    if (tmanReadIntrDepth() != 0)
        return (int)SCE_ERROR_KERNEL_CANNOT_BE_CALLED_FROM_INTERRUPT;
    if ((s32)(u32)entry < 0)
        return (int)SCE_ERROR_KERNEL_ILLEGAL_ADDR;

    TmanThreadCB *cur = g_tman.current_thread;
    TmanExtStackNode *node = (TmanExtStackNode *)sceKernelAllocHeapMemory(
        g_tman.heap_id, sizeof(TmanExtStackNode));
    if (node == NULL)
        return (int)SCE_ERROR_KERNEL_NO_MEMORY;

    if ((cur->attr & SCE_KERNEL_TH_NO_FILLSTACK) == 0)
        memset(stack_base, 0xFF, stack_size);
    *(u32 *)stack_base = (u32)cur->uid;

    int intr = sceKernelCpuSuspendIntr();
    TmanSyscallStack *ss = cur->syscall_stack;
    node->next = cur->ext_stack_list;
    node->stack_base = cur->stack;
    node->block_id = cur->stack_block_id;
    node->stack_size = cur->stack_size;
    node->saved_sp = ss->saved_sp;
    node->saved_user_ra = ss->user_ra;
    node->saved_user_sp = ss->user_sp;

    cur->stack_block_id = block_id;
    cur->stack = stack_base;
    cur->stack_size = stack_size;
    cur->ext_stack_list = node;

    u32 *tramp = (u32 *)((u8 *)stack_base + stack_size - 64U);
    u32 arg_val = (u32)arg;
    tramp[0] = 0x3C040000U | (arg_val >> 16);
    tramp[1] = 0x0C000000U | (((u32)entry >> 2) & 0x03FFFFFFU);
    tramp[2] = 0x34840000U | (arg_val & 0xFFFFU);
    tramp[3] = 0x8FBF0038U;
    tramp[4] = 0x8FBD003CU;
    tramp[5] = ((g_tman.syscall_ret_change_stack << 6) & 0x03FFFFC0U) | 0x0CU;
    tramp[6] = 0x03E00008U;
    tramp[7] = 0;
    tramp[8] = 0;
    tramp[9] = 0;
    tramp[10] = 0;
    tramp[11] = 0;
    tramp[12] = 0;
    tramp[13] = 0;
    tramp[14] = node->saved_user_ra;
    tramp[15] = node->saved_user_sp;

    ss->saved_sp = (u32)tramp;
    ss->user_sp = (u32)tramp;
    ss->user_ra = 0;

    tmanCacheOp(0x08U, tramp);
    tmanCacheOp(0x1BU, tramp);
    sceKernelCpuResumeIntrWithSync(intr);
    return 0;
}

int ThreadManForUser_28BFD974(SceUID block_id, void *entry, void *arg)
{
    u32 blk_info[14];
    blk_info[0] = 56;
    int ret = sceKernelQueryMemoryBlockInfo(block_id, blk_info);
    if (ret < 0)
        return ret;
    u32 base = blk_info[10];
    u32 size = blk_info[11];
    if (!tmanCheckUserPtr((void *)base, size))
        return (int)SCE_ERROR_KERNEL_ILLEGAL_ADDR;
    if (size < 0x200U)
        return (int)SCE_ERROR_KERNEL_ILLEGAL_STACK_SIZE;
    ret = tmanExtendThreadStackCommon(0, (void *)base, size, entry, arg);
    if (ret < 0)
        return ret;
    if (g_tman.log_handler != NULL)
        g_tman.log_handler(40, tmanGetK1(), 3, block_id);
    return (int)0xDEADBEEFU;
}

int sceKernelExtendThreadStack(u32 stack_size, void *entry, void *arg)
{
    if (stack_size < 0x200U)
        return (int)SCE_ERROR_KERNEL_ILLEGAL_STACK_SIZE;
    u32 aligned_size = (stack_size + 255U) & ~255U;
    TmanThreadCB *cur = g_tman.current_thread;
    SceSysmemUidCB *cb = tmanDataToUid(cur, g_tman.uid_thread);
    char stack_name[32];
    int intr = sceKernelCpuSuspendIntr();
    snprintf(stack_name, 31, "exstack:%s", cb->name ? cb->name : "");
    int alloc_type = ((cur->attr & SCE_KERNEL_TH_LOW_STACK) != 0) ? 0 : 1;
    SceUID blk_id = sceKernelAllocPartitionMemory(cur->stack_partition_id, stack_name,
                                                  alloc_type, aligned_size, NULL);
    sceKernelCpuResumeIntr(intr);
    if (blk_id <= 0)
        return (int)SCE_ERROR_KERNEL_NO_MEMORY;

    void *base = sceKernelGetBlockHeadAddr(blk_id);
    int ret = tmanExtendThreadStackCommon(blk_id, base, aligned_size, entry, arg);
    if (ret < 0) {
        sceKernelFreePartitionMemory(blk_id);
        return ret;
    }
    if (g_tman.log_handler != NULL)
        g_tman.log_handler(41, (u32)blk_id, 3, aligned_size, entry, arg);
    return (int)0xDEADBEEFU;
}

int sceKernelExtendKernelStack(int size, s32 (*cb)(void *), void *arg)
{
    if (tmanReadIntrDepth() != 0)
        return (int)SCE_ERROR_KERNEL_CANNOT_BE_CALLED_FROM_INTERRUPT;
    if (size < 0x200)
        return (int)SCE_ERROR_KERNEL_ILLEGAL_STACK_SIZE;

    int intr = sceKernelCpuSuspendIntr();
    SceUID block_id = sceKernelAllocPartitionMemory(1, "SceThreadmanExtKStack", 1, (u32)size, NULL);
    if (block_id < 0) {
        sceKernelCpuResumeIntr(intr);
        return (int)SCE_ERROR_KERNEL_NO_MEMORY;
    }

    TmanThreadCB *cur = g_tman.current_thread;
    void *old_kstack = cur->kstack;
    u32 old_kstack_size = cur->kstack_size;
    void *new_kstack = sceKernelGetBlockHeadAddr(block_id);
    *(u32 *)new_kstack = (u32)cur->uid;
    cur->kstack = new_kstack;
    cur->kstack_size = (u32)size;
    sceKernelCpuResumeIntr(intr);

    s32 ret = cb(arg);

    intr = sceKernelCpuSuspendIntr();
    cur->kstack = old_kstack;
    cur->kstack_size = old_kstack_size;
    sceKernelFreePartitionMemory(block_id);
    sceKernelCpuResumeIntr(intr);
    return ret;
}

void tmanCheckKernelStackGuard(void)
{
    volatile u32 probe = 0;
    TmanThreadCB *cur = g_tman.current_thread;
    u32 base = ((s32)cur->attr < 0) ? (u32)cur->kstack : (u32)cur->stack;
    if (base == 0)
        return;
    s32 free_bytes = (s32)((u32)&probe - base);
    if (free_bytes < 64) {
        sceKernelCpuSuspendIntr();
        SaveThreadContext(cur->ctx);
        tmanHandleStackOverflow(cur, 1, cur->ctx->gpr[29]);
        tmanSchedule(1, 1);
    }
}

int _sceKernelCheckThreadStack(void)
{
    if (tmanReadIntrDepth() != 0)
        return 0;
    TmanThreadCB *cur = g_tman.current_thread;
    if (g_tman.log_handler != NULL)
        g_tman.log_handler(32, (u32)&g_tman, 0);

    if ((s32)cur->attr < 0) {
        u32 user_sp = cur->syscall_stack->user_sp;
        s32 free_bytes = (s32)(user_sp - (u32)cur->stack);
        if (free_bytes < 64) {
            sceKernelCpuSuspendIntr();
            SaveThreadContext(cur->ctx);
            tmanHandleStackOverflow(cur, 0, user_sp);
            tmanSchedule(1, 1);
        }
        return (int)free_bytes;
    }

    volatile u32 probe = 0;
    if (cur->stack == NULL)
        return 0;
    s32 free_bytes = (s32)((u32)&probe - (u32)cur->stack);
    if (free_bytes < 64) {
        sceKernelCpuSuspendIntr();
        SaveThreadContext(cur->ctx);
        tmanHandleStackOverflow(cur, 0, cur->ctx->gpr[29]);
        tmanSchedule(1, 1);
    }
    return (int)free_bytes;
}

int sceKernelCheckThreadKernelStack(void)
{
    if (tmanReadIntrDepth() != 0)
        return 0;
    volatile u32 probe = 0;
    TmanThreadCB *cur = g_tman.current_thread;
    u32 base = ((s32)cur->attr < 0) ? (u32)cur->kstack : (u32)cur->stack;
    if (base == 0)
        return 0;
    s32 free_bytes = (s32)((u32)&probe - base);
    if (free_bytes < 64) {
        sceKernelCpuSuspendIntr();
        SaveThreadContext(cur->ctx);
        tmanHandleStackOverflow(cur, 1, cur->ctx->gpr[29]);
        tmanSchedule(1, 1);
    }
    return (int)free_bytes;
}

static inline __attribute__((always_inline)) int tmanCalcStackFreeSize(const void *base, u32 size)
{
    const u32 *words = (const u32 *)base;
    u32 count = size >> 2;
    u32 idx;
    for (idx = 1; idx < count; idx++) {
        if (words[idx] != 0xFFFFFFFFU)
            break;
    }
    return (int)(idx << 2);
}

int sceKernelGetThreadStackFreeSize(SceUID thid)
{
    int intr = sceKernelCpuSuspendIntr();
    TmanThreadCB *th;
    if (thid == 0) {
        th = g_tman.current_thread;
    } else {
        SceSysmemUidCB *cb = NULL;
        if (sceKernelGetUIDcontrolBlockWithType(thid, g_tman.uid_thread, &cb) != 0) {
            sceKernelCpuResumeIntr(intr);
            return (int)SCE_ERROR_KERNEL_NOT_FOUND_THREAD;
        }
        if (!tmanCheckUidAttr(cb, 1)) {
            sceKernelCpuResumeIntr(intr);
            return (int)SCE_ERROR_KERNEL_ILLEGAL_PERMISSION;
        }
        th = (TmanThreadCB *)tmanUidData(cb, g_tman.uid_thread);
    }

    int free_size = tmanCalcStackFreeSize(th->stack, th->stack_size);
    sceKernelCpuResumeIntr(intr);
    return free_size;
}

int sceKernelGetThreadKernelStackFreeSize(SceUID thid)
{
    int intr = sceKernelCpuSuspendIntr();
    TmanThreadCB *th;
    if (thid == 0) {
        th = g_tman.current_thread;
    } else {
        SceSysmemUidCB *cb = NULL;
        if (sceKernelGetUIDcontrolBlockWithType(thid, g_tman.uid_thread, &cb) != 0) {
            sceKernelCpuResumeIntr(intr);
            return (int)SCE_ERROR_KERNEL_NOT_FOUND_THREAD;
        }
        th = (TmanThreadCB *)tmanUidData(cb, g_tman.uid_thread);
    }

    const void *base = ((s32)th->attr < 0) ? th->kstack : th->stack;
    u32 size = ((s32)th->attr < 0) ? th->kstack_size : th->stack_size;
    int free_size = tmanCalcStackFreeSize(base, size);
    sceKernelCpuResumeIntr(intr);
    return free_size;
}

/* ============================================================================
 * Status, Info, Query, Profiler APIs
 * ============================================================================ */

int sceKernelReferThreadStatus(SceUID thid, TmanThreadInfo *info)
{
    if (!tmanCheckUserPtr(info, sizeof(*info)))
        return (int)SCE_ERROR_KERNEL_ILLEGAL_ADDR;

    int intr = sceKernelCpuSuspendIntr();
    SceSysmemUidCB *cb = NULL;
    TmanThreadCB *th;
    if (thid == 0) {
        th = g_tman.current_thread;
        cb = tmanDataToUid(th, g_tman.uid_thread);
    } else {
        if (sceKernelGetUIDcontrolBlockWithType(thid, g_tman.uid_thread, &cb) != 0) {
            sceKernelCpuResumeIntr(intr);
            return (int)SCE_ERROR_KERNEL_NOT_FOUND_THREAD;
        }
        if (!tmanCheckUidAttr(cb, 1)) {
            sceKernelCpuResumeIntr(intr);
            return (int)SCE_ERROR_KERNEL_ILLEGAL_PERMISSION;
        }
        th = (TmanThreadCB *)tmanUidData(cb, g_tman.uid_thread);
    }

    TmanThreadInfo local;
    memset(&local, 0, sizeof(local));
    local.size = sizeof(local);
    if (cb->name != NULL)
        strncpy(local.name, cb->name, SCE_UID_NAME_LEN);
    local.attr = th->attr;
    local.status = (s32)th->status;
    local.entry = th->entry;
    local.stack = th->stack;
    local.stack_size = (s32)th->stack_size;
    local.gp_reg = th->gp_reg;
    local.init_priority = th->init_priority;
    local.current_priority = th->current_priority;
    local.wait_type = (s32)th->wait_type;
    local.wait_id = (th->wait_obj != NULL) ? (th->wait_obj->uid ^ (s32)g_tman.uid_xor_key) : 0;
    local.wakeup_count = th->wakeup_count;
    local.exit_status = th->exit_status;
    local.run_clocks.low = th->run_clocks_lo;
    local.run_clocks.hi = th->run_clocks_hi;
    local.intr_preempt_count = th->preempt_count;
    local.thread_preempt_count = th->release_count;
    local.release_count = th->wait_release_count;

    SceSize copy_size = info->size < local.size ? info->size : local.size;
    memcpy(info, &local, copy_size);
    sceKernelCpuResumeIntr(intr);
    return 0;
}

void tmanFillThreadDebugStatus(SceSysmemUidCB *cb, void *status)
{
    TmanThreadCB *th = (TmanThreadCB *)tmanUidData(cb, g_tman.uid_thread);
    TmanThreadInfo *dst = (TmanThreadInfo *)status;
    memset(dst, 0, sizeof(*dst));
    dst->size = sizeof(*dst);
    if (cb->name != NULL)
        strncpy(dst->name, cb->name, SCE_UID_NAME_LEN);
    dst->attr = th->attr;
    dst->status = (s32)th->status;
    dst->entry = th->entry;
    dst->stack = th->stack;
    dst->stack_size = (s32)th->stack_size;
    dst->gp_reg = th->gp_reg;
    dst->init_priority = th->init_priority;
    dst->current_priority = th->current_priority;
    dst->wait_type = (s32)th->wait_type;
    dst->wait_id = (th->wait_obj != NULL) ? (th->wait_obj->uid ^ (s32)g_tman.uid_xor_key) : 0;
    dst->wakeup_count = th->wakeup_count;
    dst->exit_status = th->exit_status;
    if (th == g_tman.current_thread) {
        u64 now = tmanGetSystemTimeWide();
        u64 clocks = (((u64)th->run_clocks_hi << 32) | th->run_clocks_lo) + (now - g_tman.last_switch_time);
        dst->run_clocks.low = (u32)clocks;
        dst->run_clocks.hi = (u32)(clocks >> 32);
    } else {
        dst->run_clocks.low = th->run_clocks_lo;
        dst->run_clocks.hi = th->run_clocks_hi;
    }
    dst->intr_preempt_count = th->preempt_count;
    dst->thread_preempt_count = th->release_count;
    dst->release_count = th->wait_release_count;
    if (th->ctx != NULL && (u32)th->ctx == tmanReadCfc0(8))
        tmanSaveFpuContext(th->ctx);
}

int sceKernelReferThreadDebugStatus(SceUID thid, TmanThreadInfo *info)
{
    if (!tmanCheckUserPtr(info, sizeof(*info)))
        return (int)SCE_ERROR_KERNEL_ILLEGAL_ADDR;

    int intr = sceKernelCpuSuspendIntr();
    SceSysmemUidCB *cb = NULL;
    if (thid == 0) {
        cb = tmanDataToUid(g_tman.current_thread, g_tman.uid_thread);
    } else {
        if (sceKernelGetUIDcontrolBlockWithType(thid, g_tman.uid_thread, &cb) != 0) {
            sceKernelCpuResumeIntr(intr);
            return (int)SCE_ERROR_KERNEL_NOT_FOUND_THREAD;
        }
        if (!tmanCheckUidAttr(cb, 1)) {
            sceKernelCpuResumeIntr(intr);
            return (int)SCE_ERROR_KERNEL_ILLEGAL_PERMISSION;
        }
    }

    TmanThreadInfo local;
    tmanFillThreadDebugStatus(cb, &local);
    SceSize copy_size = info->size < local.size ? info->size : local.size;
    memcpy(info, &local, copy_size);
    sceKernelCpuResumeIntr(intr);
    return 0;
}

int sceKernelReferThreadRunStatus(SceUID thid, TmanThreadRunStatus *info)
{
    if (!tmanCheckUserPtr(info, sizeof(*info)))
        return (int)SCE_ERROR_KERNEL_ILLEGAL_ADDR;

    int intr = sceKernelCpuSuspendIntr();
    TmanThreadCB *th;
    if (thid == 0) {
        th = g_tman.current_thread;
    } else {
        SceSysmemUidCB *cb = NULL;
        if (sceKernelGetUIDcontrolBlockWithType(thid, g_tman.uid_thread, &cb) != 0) {
            sceKernelCpuResumeIntr(intr);
            return (int)SCE_ERROR_KERNEL_NOT_FOUND_THREAD;
        }
        if (!tmanCheckUidAttr(cb, 1)) {
            sceKernelCpuResumeIntr(intr);
            return (int)SCE_ERROR_KERNEL_ILLEGAL_PERMISSION;
        }
        th = (TmanThreadCB *)tmanUidData(cb, g_tman.uid_thread);
    }

    TmanThreadRunStatus local;
    memset(&local, 0, sizeof(local));
    local.size = sizeof(local);
    local.status = (s32)th->status;
    local.current_priority = th->current_priority;
    local.wait_type = (s32)th->wait_type;
    local.wait_id = (th->wait_obj != NULL) ? (th->wait_obj->uid ^ (s32)g_tman.uid_xor_key) : 0;
    local.wakeup_count = th->wakeup_count;
    local.run_clocks.low = th->run_clocks_lo;
    local.run_clocks.hi = th->run_clocks_hi;
    local.intr_preempt_count = th->preempt_count;
    local.thread_preempt_count = th->release_count;
    local.release_count = th->wait_release_count;

    SceSize copy_size = info->size < local.size ? info->size : local.size;
    memcpy(info, &local, copy_size);
    sceKernelCpuResumeIntr(intr);
    return 0;
}

int sceKernelGetThreadmanIdList(SceKernelIdListType type, SceUID *readbuf,
                                int readbufsize, int *idcount)
{
    if (!tmanCheckUserPtr(readbuf, (u32)readbufsize * sizeof(SceUID)) ||
        !tmanCheckUserPtr(idcount, sizeof(int)))
        return (int)SCE_ERROR_KERNEL_ILLEGAL_ADDR;

    int intr = sceKernelCpuSuspendIntr();
    SceSysmemUidCB *uid_type = NULL;
    TmanListNode *list_head = NULL;

    switch (type) {
        case SCE_KERNEL_TMID_Thread:             uid_type = g_tman.uid_thread; break;
        case SCE_KERNEL_TMID_Semaphore:          uid_type = g_tman.uid_sema; break;
        case SCE_KERNEL_TMID_EventFlag:          uid_type = g_tman.uid_evf; break;
        case SCE_KERNEL_TMID_Mbox:               uid_type = g_tman.uid_mbx; break;
        case SCE_KERNEL_TMID_Vpl:                uid_type = g_tman.uid_vpl; break;
        case SCE_KERNEL_TMID_Fpl:                uid_type = g_tman.uid_fpl; break;
        case SCE_KERNEL_TMID_Mpipe:              uid_type = g_tman.uid_msgpipe; break;
        case SCE_KERNEL_TMID_Callback:           uid_type = g_tman.uid_cb; break;
        case SCE_KERNEL_TMID_ThreadEventHandler: uid_type = g_tman.uid_threv; break;
        case SCE_KERNEL_TMID_Alarm:              uid_type = g_tman.uid_alarm; break;
        case SCE_KERNEL_TMID_VTimer:             uid_type = g_tman.uid_vtimer; break;
        case 12:                                 uid_type = g_tman.uid_mutex; break;
        case 13:                                 uid_type = g_tman.uid_lwmutex; break;
        case 14:                                 uid_type = g_tman.uid_tlspl; break;
        case SCE_KERNEL_TMID_SleepThread:        list_head = &g_tman.sleeping_list; break;
        case SCE_KERNEL_TMID_DelayThread:        list_head = &g_tman.delayed_list; break;
        case SCE_KERNEL_TMID_SuspendThread:      list_head = &g_tman.suspended_list; break;
        case SCE_KERNEL_TMID_DormantThread:      list_head = &g_tman.stopped_list; break;
        default:
            sceKernelCpuResumeIntr(intr);
            return (int)SCE_ERROR_KERNEL_ILLEGAL_TYPE;
    }

    int count = 0;
    if (uid_type != NULL) {
        SceSysmemUidCB *cb = uid_type->PARENT0;
        while (cb != uid_type) {
            if (tmanCheckUidAttr(cb, 0xFF)) {
                if (count < readbufsize && readbuf != NULL)
                    readbuf[count] = cb->uid ^ (s32)g_tman.uid_xor_key;
                count++;
            }
            cb = cb->PARENT0;
        }
    } else if (list_head != NULL) {
        TmanListNode *node = list_head->next;
        while (node != list_head) {
            TmanThreadCB *th = (TmanThreadCB *)node;
            if (count < readbufsize && readbuf != NULL)
                readbuf[count] = th->uid;
            count++;
            node = node->next;
        }
    }

    if (idcount != NULL)
        *idcount = count;
    int ret = (count < readbufsize) ? count : readbufsize;
    sceKernelCpuResumeIntr(intr);
    return ret;
}

SceKernelIdListType sceKernelGetThreadmanIdType(SceUID uid)
{
    int intr = sceKernelCpuSuspendIntr();
    SceSysmemUidCB *cb = NULL;
    if (sceKernelGetUIDcontrolBlock(uid, &cb) != 0 || !tmanCheckUidAttr(cb, 0xFF)) {
        sceKernelCpuResumeIntr(intr);
        return (SceKernelIdListType)SCE_ERROR_KERNEL_ILLEGAL_ARGUMENT;
    }

    if (g_tman.log_handler != NULL)
        g_tman.log_handler(166, (u32)uid, 1, uid);

    int type = (int)SCE_ERROR_KERNEL_ILLEGAL_ARGUMENT;
    if (sceKernelIsKindOf(cb, g_tman.uid_thread))       type = SCE_KERNEL_TMID_Thread;
    else if (sceKernelIsKindOf(cb, g_tman.uid_sema))    type = SCE_KERNEL_TMID_Semaphore;
    else if (sceKernelIsKindOf(cb, g_tman.uid_evf))     type = SCE_KERNEL_TMID_EventFlag;
    else if (sceKernelIsKindOf(cb, g_tman.uid_mbx))     type = SCE_KERNEL_TMID_Mbox;
    else if (sceKernelIsKindOf(cb, g_tman.uid_vpl))     type = SCE_KERNEL_TMID_Vpl;
    else if (sceKernelIsKindOf(cb, g_tman.uid_fpl))     type = SCE_KERNEL_TMID_Fpl;
    else if (sceKernelIsKindOf(cb, g_tman.uid_msgpipe)) type = SCE_KERNEL_TMID_Mpipe;
    else if (sceKernelIsKindOf(cb, g_tman.uid_cb))      type = SCE_KERNEL_TMID_Callback;
    else if (sceKernelIsKindOf(cb, g_tman.uid_threv))   type = SCE_KERNEL_TMID_ThreadEventHandler;
    else if (sceKernelIsKindOf(cb, g_tman.uid_alarm))   type = SCE_KERNEL_TMID_Alarm;
    else if (sceKernelIsKindOf(cb, g_tman.uid_vtimer))  type = SCE_KERNEL_TMID_VTimer;
    else if (sceKernelIsKindOf(cb, g_tman.uid_mutex))   type = 12;
    else if (sceKernelIsKindOf(cb, g_tman.uid_lwmutex)) type = 13;
    else if (sceKernelIsKindOf(cb, g_tman.uid_tlspl))   type = 14;

    sceKernelCpuResumeIntr(intr);
    return (SceKernelIdListType)type;
}

SceUID sceKernelGetThreadmanIdOwner(SceUID block_id, int *type_out)
{
    if (!tmanCheckUserPtr(type_out, sizeof(int)))
        return (SceUID)SCE_ERROR_KERNEL_ILLEGAL_ADDR;

    int intr = sceKernelCpuSuspendIntr();
    SceUID owner = tmanFindVplByMemBlock(block_id);
    if (owner > 0) {
        if (type_out != NULL) *type_out = SCE_KERNEL_TMID_Vpl;
        sceKernelCpuResumeIntr(intr);
        return owner;
    }
    owner = tmanFindFplByMemBlock(block_id);
    if (owner > 0) {
        if (type_out != NULL) *type_out = SCE_KERNEL_TMID_Fpl;
        sceKernelCpuResumeIntr(intr);
        return owner;
    }
    owner = tmanFindMsgPipeByMemBlock(block_id);
    if (owner > 0) {
        if (type_out != NULL) *type_out = SCE_KERNEL_TMID_Mpipe;
        sceKernelCpuResumeIntr(intr);
        return owner;
    }
    owner = tmanFindTlsplByMemBlock(block_id);
    if (owner > 0) {
        if (type_out != NULL) *type_out = 14;
        sceKernelCpuResumeIntr(intr);
        return owner;
    }

    SceSysmemUidCB *head = g_tman.uid_thread;
    SceSysmemUidCB *cb = head->PARENT0;
    while (cb != head) {
        TmanThreadCB *th = (TmanThreadCB *)tmanUidData(cb, head);
        if (th->stack_block_id == block_id || th->kstack_block_id == block_id) {
            if (type_out != NULL) *type_out = SCE_KERNEL_TMID_Thread;
            sceKernelCpuResumeIntr(intr);
            return th->uid;
        }
        TmanExtStackNode *ext;
        for (ext = th->ext_stack_list; ext != NULL; ext = ext->next) {
            if (ext->block_id == block_id) {
                if (type_out != NULL) *type_out = SCE_KERNEL_TMID_Thread;
                sceKernelCpuResumeIntr(intr);
                return th->uid;
            }
        }
        for (ext = th->ext_kstack_list; ext != NULL; ext = ext->next) {
            if (ext->block_id == block_id) {
                if (type_out != NULL) *type_out = SCE_KERNEL_TMID_Thread;
                sceKernelCpuResumeIntr(intr);
                return th->uid;
            }
        }
        cb = cb->PARENT0;
    }

    sceKernelCpuResumeIntr(intr);
    return (SceUID)SCE_ERROR_KERNEL_UNKNOWN_UID;
}

int ThreadManForKernel_D366D35A(int disable)
{
    if (disable != 0) {
        volatile u32 *code = (volatile u32 *)(void *)tmanK0CorruptedHandler;
        code[0] = 0x03E00008U;
        code[1] = 0x00000000U;
    }
    return 0;
}

int sceKernelSetThreadmanLogHandler(TmanLogHandler handler, TmanThreadCB **cur_out)
{
    g_tman.log_handler = handler;
    if (cur_out != NULL)
        *cur_out = (TmanThreadCB *)&g_tman;
    return 0;
}

s32 sceKernelGetThreadId(void)
{
    TmanThreadCB *cur = g_tman.current_thread;
    if (g_tman.log_handler != NULL)
        g_tman.log_handler(29, (u32)cur, 0);
    if (tmanReadIntrDepth() != 0)
        return (s32)SCE_ERROR_KERNEL_CANNOT_BE_CALLED_FROM_INTERRUPT;
    return cur->uid;
}

int sceKernelGetThreadCurrentPriority(void)
{
    TmanThreadCB *cur = g_tman.current_thread;
    if (g_tman.log_handler != NULL)
        g_tman.log_handler(30, (u32)cur, 1, cur->current_priority);
    if (tmanReadIntrDepth() != 0)
        return (int)SCE_ERROR_KERNEL_CANNOT_BE_CALLED_FROM_INTERRUPT;
    return cur->current_priority;
}

int sceKernelGetUserLevel(void)
{
    TmanThreadCB *cur = g_tman.current_thread;
    int level = (int)((cur->attr >> 28) ^ 8U);
    if (g_tman.log_handler != NULL)
        g_tman.log_handler(37, (u32)cur, 1, level);
    if (tmanReadIntrDepth() != 0)
        return (int)SCE_ERROR_KERNEL_CANNOT_BE_CALLED_FROM_INTERRUPT;
    return level;
}

int sceKernelIsUserModeThread(void)
{
    TmanThreadCB *cur = g_tman.current_thread;
    int is_user = (int)(cur->attr >> 31);
    if (g_tman.log_handler != NULL)
        g_tman.log_handler(38, (u32)cur, 1, is_user);
    if (tmanReadIntrDepth() != 0)
        return (int)SCE_ERROR_KERNEL_CANNOT_BE_CALLED_FROM_INTERRUPT;
    return is_user;
}

SceUID sceKernelGetSystemStatusFlag(void)
{
    return g_tman.sys_stat_evf;
}

int sceKernelRegisterDeadThreadHandler(void (*handler)(SceUID))
{
    g_tman.dead_thread_handler = handler;
    return 0;
}

void tmanFillSystemStatus(TmanSystemStatus *local, int intr_state)
{
    memset(local, 0, sizeof(*local));
    local->size = sizeof(*local);
    if (tmanReadIntrDepth() != 0) {
        local->status = 4;
    } else if (intr_state == 0) {
        local->status = 3;
    } else if (g_tman.dispatch_suspended != 0) {
        local->status = 1;
    } else {
        local->status = 0;
    }
    TmanThreadCB *idle = g_tman.idle_thread;
    local->idle_clocks.low = idle->run_clocks_lo;
    local->idle_clocks.hi = idle->run_clocks_hi;
    local->comes_out_of_idle_count = idle->release_count;
    local->thread_switch_count = g_tman.switch_count;
    local->vfpu_switch_count = g_tman.vfpu_switch_count;
}

int sceKernelReferSystemStatus(TmanSystemStatus *info)
{
    if (!tmanCheckUserPtr(info, sizeof(*info)))
        return (int)SCE_ERROR_KERNEL_ILLEGAL_ADDR;

    int intr = sceKernelCpuSuspendIntr();
    if (g_tman.log_handler != NULL)
        g_tman.log_handler(164, (u32)info, 1, info);

    TmanSystemStatus local;
    tmanFillSystemStatus(&local, intr);
    SceSize copy_size = info->size < local.size ? info->size : local.size;
    memcpy(info, &local, copy_size);
    sceKernelCpuResumeIntr(intr);
    return 0;
}

void *sceKernelReferThreadProfiler(void)
{
    if (tmanReadIntrDepth() != 0 || g_tman.profiler_mode != 1)
        return NULL;
    if (g_tman.current_thread->profiler_regs == NULL)
        return NULL;
    return (void *)0x5C400000U;
}

void *sceKernelReferGlobalProfiler(void)
{
    if (g_tman.profiler_mode != 2)
        return NULL;
    return (void *)0x5C400000U;
}
