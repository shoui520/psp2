/* Copyright (C) 2011, 2012 The uOFW team
   See the file COPYING for copying permission.
*/

#include <dmacman.h>
#include <common_imp.h>

#include <interruptman.h>
#include <sysmem_kdebug.h>
#include <sysmem_suspend_kernel.h>
#include <sysmem_sysclib.h>
#include <threadman_kernel.h>

#define DMACMAN_VERSION_MAJOR   (1)
#define DMACMAN_VERSION_MINOR   (8)

typedef struct {
    sceKernelDmaOperation *activeOps[16];     // 0: active channel ops
    sceKernelDmaOperation ops[32];       // 64
    u32 excludedChMask;                         // excluded channels
    u32 reservedChMask;                         // reserved channels
    u32 inUseChMask;                         // inUse bit-vector
    sceKernelDmaOperation *freeList;       // Free list
    sceKernelDmaOperation *queueHead;       // First/head
    sceKernelDmaOperation *queueTail;       // Last/tail
    s32 (*ddrFlushFunc)(s32);                 // ddr flush func
    SceUID evFlagId;                      // evid passed to sceKernelWaitEventFlag
} SceDmacMan;

SceDmacMan g_dmacman;

SCE_MODULE_INFO("sceDMAManager", SCE_MODULE_KERNEL | SCE_MODULE_ATTR_EXCLUSIVE_START |
                                 SCE_MODULE_ATTR_EXCLUSIVE_LOAD | SCE_MODULE_ATTR_CANT_STOP,
                                 DMACMAN_VERSION_MAJOR, DMACMAN_VERSION_MINOR);
SCE_MODULE_BOOTSTART("_sceDmacManModuleStart");
SCE_MODULE_REBOOT_BEFORE("sceKernelDmaExit");
SCE_SDK_VERSION(SDK_VERSION);

static void abortDmaChannel(sceKernelDmaOperation *op);
static s32 interruptHandler(s32 arg0, s32 arg1);
static void _sceKernelDmaSchedule(void);
static s32 suspendHandler(s32 flag, void *param);
static s32 resumeHandler(s32 flag, void *param);
static s32 _dummyDdrFlush(s32 arg);
static void _sceKernelDmacClkEnable(s32 arg0);
s32 sceKernelDmaRegisterDdrFlush(void *ddrFlushFunc);
s32 sceKernelDmaOpAllCancel(void);

inline static s32 pspBitrev(s32 a) {
    s32 ret;
    asm __volatile__ ("bitrev %0, %1;" : "=r" (ret) : "r"(a));
    return ret;
}

inline static s32 pspClz(s32 a) {
    s32 ret;
    asm __volatile__ ("clz %0, %1" : "=r"(ret) : "r" (a));
    return ret;
}

inline static s32 lsbPosition(s32 a) {
    return pspClz(pspBitrev(a));
}

s32 _sceDmacManModuleStart(SceSize args __attribute__((unused)), void *argp __attribute__((unused)))
{
    u32 intr;
    s32 err = SCE_ERROR_OK;

    intr = sceKernelCpuSuspendIntr();
    memset(&g_dmacman.ops, 0, 2048);

    g_dmacman.freeList = &g_dmacman.ops[0];
    int i;
    for (i = 0; i < 31; i++) {
        g_dmacman.ops[i].pNext = &g_dmacman.ops[i+1];
        g_dmacman.ops[i+1].pPrev = &g_dmacman.ops[i];
        g_dmacman.ops[i].uiFlag = 0x1000;
        g_dmacman.ops[i].evpat = 1 << i;
    }
    g_dmacman.ops[31].uiFlag = 0x1000;
    g_dmacman.ops[31].evpat = 1 << 31;

    g_dmacman.queueHead = NULL;
    g_dmacman.queueTail = NULL;
    g_dmacman.evFlagId = 0;

    sceKernelRegisterSuspendHandler(20, suspendHandler, NULL);
    sceKernelRegisterResumeHandler(20, resumeHandler, NULL);

    err |= sceKernelRegisterIntrHandler(22, 2, interruptHandler, (void *)0, NULL);
    err |= sceKernelEnableIntr(22);
    err |= sceKernelRegisterIntrHandler(23, 2, interruptHandler, (void *)1, NULL);
    err |= sceKernelEnableIntr(23);

    if (err) {
        sceKernelReleaseIntrHandler(22);
        sceKernelReleaseIntrHandler(23);
        err = 0x800202BC;
    }

    sceKernelDmaRegisterDdrFlush(_dummyDdrFlush);

    sceKernelCpuResumeIntr(intr);

    g_dmacman.evFlagId = sceKernelCreateEventFlag("SceDmacmanEvflag", 0x200, 0, 0);

    return err;
}

s32 sceKernelDmaExit(SceSize args __attribute__((unused)), void *argp __attribute__((unused)))
{
    u32 intr, intr2;
    s32 err = SCE_ERROR_OK;

    intr = sceKernelCpuSuspendIntr();

    for (int j = 0; j < 8; j++) {
        HW(0xBC900100 + j*32 + 16) |= 0x40000;
        while (HW(0xBC900100 + j*32 + 16) & 0x20000);
        HW(0xBC900100 + j*32 + 16) &= 0xFFFF3FFE;
    }

    for (int j = 0; j < 8; j++) {
        HW(0xBCA00100 + j*32 + 16) |= 0x40000;
        while (HW(0xBCA00100 + j*32 + 16) & 0x20000);
        HW(0xBCA00100 + j*32 + 16) &= 0xFFFF3FFE;
    }

    sceKernelDmaOpAllCancel();

    HW(0xBC900030) &= ~0x1;
    HW(0xBC900008) = 0xFF;
    HW(0xBC900010) = 0xFF;
    err |= sceKernelReleaseIntrHandler(22);
    intr2 = sceKernelCpuSuspendIntr();
    HW(0xBC100050) &= ~0x20;
    sceKernelCpuResumeIntr(intr2);

    HW(0xBCA00030) &= ~0x1;
    HW(0xBCA00008) = 0xFF;
    HW(0xBCA00010) = 0xFF;
    err |= sceKernelReleaseIntrHandler(23);
    intr2 = sceKernelCpuSuspendIntr();
    HW(0xBC100050) &= ~0x40;
    sceKernelCpuResumeIntr(intr2);

    sceKernelCpuResumeIntr(intr);

    if (err) {
        return 0x800202BC;
    }

    sceKernelDeleteEventFlag(g_dmacman.evFlagId);
    return SCE_ERROR_OK;
}

//0x300
s32 sceKernelDmaRegisterDdrFlush(void *ddrFlushFunc)
{
    g_dmacman.ddrFlushFunc = ddrFlushFunc ? ddrFlushFunc : _dummyDdrFlush;
    return SCE_ERROR_OK;
}

//0x328
s32 sceKernelDmaSoftRequest(u32 arg0, u32 arg1, u32 arg2, u32 arg3)
{
    u32 baseaddr = arg0 ? 0xBCA00000 : 0xBC900000;

    if (arg3) {
        if (arg2) {
            HW(baseaddr + 44) = (1 << arg1);
        } else {
            HW(baseaddr + 40) = (1 << arg1);
        }
    } else if (arg2) {
        HW(baseaddr + 32) = (arg2 << arg1);
    } else {
        HW(baseaddr + 34) = (1 << arg1);
    }

    return SCE_ERROR_OK;
}

//0x388
s32 sceKernelDmaOpFree(sceKernelDmaOperation *op)
{
    s32 ret;
    u32 intr;

    intr = sceKernelCpuSuspendIntr();

    if (op) {
        if (op->uiFlag & 0x1000) {
            ret = 0x800202C3;
        } else if (op->uiFlag & 0x1) {
            ret = 0x800202BE;
        } else if (op->uiFlag & 0x2) {
            ret = 0x800202C0;
        } else {
            sceKernelDmaOperation *head = g_dmacman.freeList;
            op->pNext = head;
            op->pPrev = NULL;
            op->cbfunc = NULL;
            op->uiFlag = 0x1000;
            op->dmaCh = -1;
            op->uiSrcAddr = 0;
            op->uiDestAddr = 0;
            op->uiLLI = 0;
            op->uiControl = 0;
            op->uiConfiguration = 0;
            op->HeadLLI = NULL;
            if (head)
                head->pPrev = op;
            g_dmacman.freeList = op;
            sceKernelClearEventFlag(g_dmacman.evFlagId, ~op->evpat);
            ret = SCE_ERROR_OK;
        }
    } else {
        ret = 0x800202CF;
    }

    sceKernelCpuResumeIntr(intr);
    return ret;
}

//0x488
int sceKernelDmaOpEnQueue(sceKernelDmaOperation *op)
{
    u32 intr;
    s32 ret;
    sceKernelDmaOperation *cur = op;

    if (!op)
        return 0x800202CF;
    if (op->pParent)
        return 0x800202CD;

    intr = sceKernelCpuSuspendIntr();
    for (; cur; cur = cur->pChild) {
        if (cur->uiFlag & 0x1) {
            ret = 0x800202BE;
            goto cleanup;
        } else if (cur->uiFlag & 0x1000) {
            ret = 0x800202C3;
            goto cleanup;
        } else if (!(cur->uiFlag & 0x100)) {
            ret = 0x800202C1;
            goto cleanup;
        } else if (cur->uiFlag & 0x2) {
            ret = 0x800202C0;
            goto cleanup;
        } else if (cur->uiFlag & 0x70) {
            ret = 0x800202C4;
            goto cleanup;
        }
    }

    if (!g_dmacman.queueHead)
        g_dmacman.queueHead = op;

    op->pNext = NULL;
    op->pPrev = g_dmacman.queueTail;
    if (g_dmacman.queueTail) {
        g_dmacman.queueTail->pNext = op;
    }

    g_dmacman.queueTail = op;
    op->uiFlag |= 1;
    _sceKernelDmaSchedule();
    ret = SCE_ERROR_OK;

cleanup:
    sceKernelCpuResumeIntr(intr);
    return ret;
}

//0x5D4
int sceKernelDmaOpDeQueue(sceKernelDmaOperation *op)
{
    u32 intr;
    s32 ret;

    intr = sceKernelCpuSuspendIntr();
    if (!op) {
        ret = 0x800202CF;
    } else if (op->uiFlag & 0x1000) {
        ret = 0x800202C3;
    } else if (!(op->uiFlag & 0x100)) {
        ret = 0x800202C1;
    } else if (op->uiFlag & 0x2) {
        ret = 0x800202C0;
    } else if (!(op->uiFlag & 0x1)) {
        ret = 0x800202BF;
    } else {
        if (op->pPrev) {
            op->pPrev->pNext = op->pNext;
        }
        if (op->pNext) {
            op->pNext->pPrev = op->pPrev;
        }
        if (g_dmacman.queueTail == op) {
            g_dmacman.queueTail = op->pPrev;
        }
        if (g_dmacman.queueHead == op) {
            g_dmacman.queueHead = op->pNext;
        }

        op->pNext = NULL;
        op->uiFlag &= ~0x1;
        ret = SCE_ERROR_OK;
    }

    sceKernelCpuResumeIntr(intr);
    return ret;
}

//0x6DC
s32 sceKernelDmaOpAllCancel(void)
{
    u32 intr;
    sceKernelDmaOperation *op = g_dmacman.queueHead;
    if (!op) {
        return 0x800202BF;
    }

    intr = sceKernelCpuSuspendIntr();
    for (int i = 0; i < 16; i++) {
        if (g_dmacman.activeOps[i]) {
            sceKernelCpuResumeIntr(intr);
            return 0x800202C0;
        }
    }

    op = g_dmacman.queueHead;
    while (op) {
        sceKernelDmaOperation *tmp = op->pNext;
        op->pNext = NULL;
        op->pPrev = NULL;
        op = tmp;
    }

    g_dmacman.queueHead = NULL;
    g_dmacman.queueTail = NULL;
    sceKernelCpuResumeIntr(intr);
    return SCE_ERROR_OK;
}

//0x798
int sceKernelDmaOpSetCallback(sceKernelDmaOperation *op, int (*func)(int, int), int arg2)
{
    if (!op)
        return 0x800202CF;
    if (op->uiFlag & 0x1000)
        return 0x800202C3;
    if (!(op->uiFlag & 0x800))
        return 0x800202C1;
    if (op->uiFlag & 0x2)
        return 0x800202C0;
    if (op->uiFlag & 0x70)
        return 0x800202C4;

    op->pCookie = (void *)arg2;
    op->cbfunc = (void *)func;
    return SCE_ERROR_OK;
}

//0x808
s32 sceKernelDmaOpSetupMemcpy(sceKernelDmaOperation *op, s32 arg1, s32 arg2, s32 arg3)
{
    if (!op)
        return 0x800202CF;
    if (op->uiFlag & 0x1000)
        return 0x800202C3;
    if (!(op->uiFlag & 0x100))
        return 0x800202C1;
    if (op->uiFlag & 0x2)
        return 0x800202C0;
    if (op->uiFlag & 0x70)
        return 0x800202C4;
    if (arg3 < 0x1000) {
        op->uiSrcAddr = arg2;
        op->uiDestAddr = arg1;
        op->uiLLI = 0;
        op->uiControl = arg3 & 0x8C4BF000;
        op->uiConfiguration |= 0xC001;
    }

    return SCE_ERROR_OK;
}

//0x8A8
s32 sceKernelDmaOpSetupNormal(sceKernelDmaOperation *op, s32 arg1, s32 arg2, s32 arg3, s32 arg4)
{
    if (!op)
        return 0x800202CF;
    if (op->uiFlag & 0x1000)
        return 0x800202C3;
    if (!(op->uiFlag & 0x100))
        return 0x800202C1;
    if (op->uiFlag & 0x2)
        return 0x800202C0;
    if (op->uiFlag & 0x70)
        return 0x800202C4;

    op->uiSrcAddr = arg3;
    op->uiDestAddr = arg2;
    op->uiLLI = 0;
    op->uiControl = arg4 | 0x80000000;
    op->uiConfiguration |= (arg1 | 0xC001);

    return SCE_ERROR_OK;
}

//0x938
int sceKernelDmaOpSetupLink(sceKernelDmaOperation *op, int arg1, u32 *arg2)
{
    PL080_LLI *tail;
    PL080_LLI *head;

    if (!op)
        return 0x800202CF;
    if ((op->uiFlag & 0x1172) != 0x100) {
        if (op->uiFlag & 0x1000)
            return 0x800202C3;
        if ((op->uiFlag & 0x1172) != 0)
            return 0x800202C1;
        if (op->uiFlag & 0x2)
            return 0x800202C0;
        return 0x800202C4;
    }

    op->uiConfiguration |= (arg1 | 0xC001);
    if (!((u32)arg2 & 0x3)) {
        for (tail = KUNCACHED(arg2); tail->LLI; tail = KUNCACHED(tail->LLI));
        op->EndLLI = tail;
        tail->Control |= 0x80000000;
        head = KUNCACHED(arg2);
    } else {
        head = KCACHED((u32)arg2 & 0xFFFFFFF8);
    }

    op->HeadLLI = head;
    op->uiSrcAddr = head->SrcAddr;
    op->uiDestAddr = head->DestAddr;
    op->uiLLI = head->LLI;
    op->uiControl = head->Control;
    if (!((u32)arg2 & 0x3)) {
        g_dmacman.ddrFlushFunc(4);
    }

    return SCE_ERROR_OK;
}

//0xA64
s32 sceKernelDmaOpSync(sceKernelDmaOperation *op, s32 command, u32 *timeout)
{
    s32 err;

    if (!op)
        return 0x800202CF;
    if (op->uiFlag & 0x1000)
        return 0x800202C3;
    if (!(op->uiFlag & 0x100))
        return 0x800202C1;
    if (op->uiFlag & 0x20)
        return 0x800202C6;

    switch (command) {
    case 0: {
        if (op->uiFlag & 0x40)
            return SCE_ERROR_OK;
        if (op->uiFlag & 0x10)
            return 0x800202C7;
        if (op->uiFlag & 0x1)
            return 0x800202BE;
        if (op->uiFlag & 0x2)
            return 0x800202C0;
        return 0x800202BF;
    }
    case 1:
        return sceKernelWaitEventFlag(g_dmacman.evFlagId, op->evpat, 33, NULL, NULL) ?
            SCE_ERROR_KERNEL_ERROR : SCE_ERROR_OK;
    case 2:
        err = sceKernelWaitEventFlag(g_dmacman.evFlagId, op->evpat, 33, NULL, timeout);
        if (err == SCE_ERROR_OK)
            return SCE_ERROR_OK;
        if (err == SCE_ERROR_KERNEL_WAIT_TIMEOUT) {
            abortDmaChannel(op);
            return 0x800202C2;
        }
        // intentional fall through
    default:
        return SCE_ERROR_KERNEL_ERROR;
    }
}

//0xBCC
static void abortDmaChannel(sceKernelDmaOperation *op)
{
    s32 ch = op->dmaCh;
    s32 chOff = (ch & 7) << 5;
    u32 base = (ch >> 3 == 0) ? 0xBCA00000 : 0xBC900000;
    u32 address = chOff + base + 0x100;

    HW(address) &= ~0x1;

    while (HW(address) & 0x1);

    op->uiFlag = 0x20;
    op->dmaCh = -1;
    g_dmacman.inUseChMask &= ~(1 << ch);
    g_dmacman.activeOps[ch & 15] = NULL;

    HW(base + 8) = 1 << (ch & 7);
    HW(base + 16) = 1 << (ch & 7);
}

//0xC70
int sceKernelDmaOpQuit(sceKernelDmaOperation *op)
{
    u32 intr;
    s32 ret = SCE_ERROR_OK;

    intr = sceKernelCpuSuspendIntr();
    if (!op)
        ret = 0x800202CF;
    else if (op->uiFlag & 0x1000)
        ret = 0x800202C3;
    else if (!(op->uiFlag & 0x100))
        ret = 0x800202C1;
    else if (op->uiFlag & 0x40)
        ret = 0x800202BF;
    else if (op->uiFlag & 0x1)
        ret = 0x800202BE;
    else if (op->uiFlag & 0x10)
        ret = 0x800202C7;
    else if (op->uiFlag & 0x20)
        ret = 0x800202C6;
    else if (!(op->uiFlag & 0x2))
        ret = SCE_ERROR_KERNEL_ERROR;
    else
        abortDmaChannel(op);

    sceKernelCpuResumeIntr(intr);
    return ret;
}

//0xD80
int sceKernelDmaOpConcatenate(sceKernelDmaOperation *op, void *arg1)
{
    PL080_LLI *tail;
    u32 intr;
    s32 ret;
    u32 addr;
    u32 addr16;

    if (!op)
        return 0x800202CF;

    for (tail = KUNCACHED(arg1); tail->LLI; tail = KUNCACHED(tail->LLI));
    tail->Control |= 0x80000000;

    intr = sceKernelCpuSuspendIntr();

    if ((op->uiFlag & 0x1170) != 0x100) {
        if (op->uiFlag & 0x1000)
            ret = 0x800202C3;
        else if (!(op->uiFlag & 0x100))
            ret = 0x800202C1;
        else
            ret = 0x800202C4;
        goto cleanup;
    }

    if (!(op->uiFlag & 0x2)) {
        if (op->uiFlag & 0x1) {
            op->EndLLI->LLI = (u32)arg1;
            op->EndLLI = tail;
            ret = SCE_ERROR_OK;
        } else {
            ret = SCE_ERROR_KERNEL_ERROR;
        }
        goto sync_exit;
    }

    addr = (op->dmaCh / 8) ? 0xBCA00100 : 0xBC900100;
    addr += (op->dmaCh & 7) << 5;
    addr16 = HW(addr + 16) | 0x40000;
    HW(addr + 16) = addr16;
    if (!HW(addr + 8)) {
        s32 mask = addr16 & 0x1003800;
        if (mask == 0x1000000 || mask == 0x1001000 || mask == 0x1003000) {
            ret = 0x800202BC;
            goto clear_halt;
        }

        while (HW(addr + 16) & 0x20000);

        if (!(HW(addr + 12) & 0xFFF)) {
            ret = 0x800202C4;
            goto clear_halt;
        }

        HW(addr + 16) &= ~0x1;
        while (HW(addr + 16) & 0x1);

        HW(addr + 8) = (u32)arg1;
        HW(addr + 16) |= 1;
        op->EndLLI = tail;
    } else {
        op->EndLLI->LLI = (u32)arg1;
        op->EndLLI = tail;
    }
    ret = SCE_ERROR_OK;

clear_halt:
    HW(addr + 16) &= ~0x40000;
sync_exit:
    g_dmacman.ddrFlushFunc(4);
    sceKernelCpuResumeIntrWithSync(intr);
    return ret;

cleanup:
    sceKernelCpuResumeIntr(intr);
    return ret;
}

//0xFC0
s32 sceKernelDmaOpAssignMultiple(sceKernelDmaOperation **ops, s32 size)
{
    u32 intr;
    s32 remaining = size;

    if (size > 16)
        return 0x800202BD;

    for (int i = 0; i < 16; i++) {
        if ((ops[0]->uiChMask[0] >> i) & 1)
            remaining--;
    }

    if (remaining < 0)
        return 0x800202BD;

    for (int i = size - 1; i; i--) {
        sceKernelDmaOperation *op = ops[i];
        if (!op)
            return 0x800202CF;
        if (op->uiFlag & 0x1000)
            return 0x800202C3;
        if (!(op->uiFlag & 0x100))
            return 0x800202C1;
        if (op->uiFlag & 0x1)
            return 0x800202BE;
        if (op->uiFlag & 0x2)
            return 0x800202C0;
        if (op->uiFlag & 0x70)
            return 0x800202C4;
    }

    intr = sceKernelCpuSuspendIntr();
    for (int i = 0; i < size - 1; i++) {
        ops[i]->pChild = ops[i+1];
        ops[i+1]->pParent = ops[i];
        ops[i]->pNext = NULL;
        ops[i]->pPrev = NULL;
        ops[i+1]->uiChMask[0] = ops[i]->uiChMask[0];
    }
    ops[0]->uiChMask[1] = size;
    sceKernelCpuResumeIntr(intr);
    return SCE_ERROR_OK;
}

//0x1148
static s32 interruptHandler(s32 arg0 __attribute__((unused)), s32 arg1)
{
    u32 intr = sceKernelCpuSuspendIntr();
    u32 addr = arg1 ? 0xBCA00000 : 0xBC900000;

    u32 tcStatus, errStatus, intStatus, evMask = 0, cbStatus;

    tcStatus = HW(addr + 4);
    errStatus = HW(addr + 12);
    intStatus = (tcStatus | errStatus) & 0xFF;
    HW(addr + 8) = 0;

    for (int i = 0; intStatus >= (1U << i); i++) {
        if (intStatus & (1U << i)) {
            if (!g_dmacman.activeOps[i + 8*arg1]) {
                if ((u32)g_dmacman.queueTail & (1 << i))
                    Kprintf("Fatal error: Stray interrupt occurred\n");
            } else {
                sceKernelDmaOperation *op = g_dmacman.activeOps[i + 8*arg1];
                op->uiSrcAddr = HW(addr + 0x100);
                op->uiDestAddr = HW(addr + 0x100 + 4);
                op->uiLLI = HW(addr + 0x100 + 8);
                op->uiControl = HW(addr + 0x100 + 12);
                op->uiConfiguration = HW(addr + 0x100 + 16);
                if (tcStatus & (1 << i)) {
                    if (!HW(addr + 0x100 + 8) && !(HW(addr + 0x100 + 16) & 0x1)) {
                        g_dmacman.inUseChMask &= ~(1 << i);
                        evMask |= op->evpat;
                        op->uiFlag |= 0x40;
                        op->uiFlag &= ~0x2;
                        op->dmaCh = -1;
                        g_dmacman.activeOps[i + 8*arg1] = NULL;
                    }
                    HW(addr + 16) = (1 << i);
                    cbStatus = 0;
                } else {
                    if (HW(addr + 0x100 + 8) && (HW(addr + 0x100 + 16) & 0x1)) {
                        abortDmaChannel(op);
                    }
                    g_dmacman.inUseChMask &= ~(1 << i);
                    g_dmacman.activeOps[i + 8*arg1] = NULL;
                    evMask |= op->evpat;
                    op->uiFlag |= 0x40;
                    op->uiFlag &= ~0x2;
                    op->dmaCh = -1;
                    HW(addr + 16) = (1 << i);
                    cbStatus = -1;
                }
                if (op->cbfunc && op->cbfunc(op, cbStatus, i + 8*arg1, op->pCookie) == 1) {
                    if (op->HeadLLI) {
                        op->uiSrcAddr = op->HeadLLI->SrcAddr;
                        op->uiDestAddr = op->HeadLLI->DestAddr;
                        op->uiLLI = op->HeadLLI->LLI;
                        op->uiControl = op->HeadLLI->Control;
                    }
                    if (!g_dmacman.queueHead)
                        g_dmacman.queueHead = op;
                    op->pNext = NULL;
                    if (g_dmacman.queueTail) {
                        g_dmacman.queueTail->pNext = op;
                    }
                    op->pPrev = g_dmacman.queueTail;
                    g_dmacman.queueTail = op;
                    op->uiFlag |= 0x1;
                }
            }
        }
    }
    sceKernelSetEventFlag(g_dmacman.evFlagId, evMask);
    if (!g_dmacman.queueHead)
        _sceKernelDmaSchedule();
    if (!((g_dmacman.inUseChMask | g_dmacman.excludedChMask) & 0xFF)) {
        u32 intr2 = sceKernelCpuSuspendIntr();
        HW(0xBC100050) &= ~(1 << 5);
        sceKernelCpuResumeIntr(intr2);
    }
    if (!((g_dmacman.inUseChMask | g_dmacman.excludedChMask) & 0xFF00)) {
        u32 intr2 = sceKernelCpuSuspendIntr();
        HW(0xBC100050) &= ~(1 << 6);
        sceKernelCpuResumeIntr(intr2);
    }
    sceKernelCpuResumeIntr(intr);
    return -1;
}

//0x14f4
static void _sceKernelDmaSchedule(void)
{
    sceKernelDmaOperation *t1 = g_dmacman.queueHead;
    u32 t2 = g_dmacman.inUseChMask & g_dmacman.excludedChMask;
    u32 a3 = t2 ^ 0xFFFF;

    for (; t1 && a3; t1 = t1->pNext) {
        if (t1->pChild && !t1->pParent) {
            u32 t0 = t1->uiChMask[0] & ~t2;
            s32 a1 = t1->uiChMask[1];
            for (int i = 0; i < 16; i++) {
                a1 -= (t0 >> i) & 0x1;
            }
            if (a1 < 0) {
                if (t1->pNext) {
                    sceKernelDmaOperation *a0 = t1->pNext;
                    a0->pPrev = t1->pPrev;
                    t1->pPrev = a0;
                    t1->pNext = a0->pNext;
                    if (a0->pPrev)
                        a0->pPrev->pNext = a0;
                    if (!a0->pNext)
                        g_dmacman.queueTail = t1;
                    else
                        t1->pNext->pPrev = t1;
                    a0->pNext = t1;
                }
                continue;
            }

            sceKernelDmaOperation *op = t1;
            for (int j = 0; op && j < 16; j++) {
                if (t0 & (1 << j)) {
                    if (g_dmacman.activeOps[j])
                        g_dmacman.activeOps[j]->dmaCh = j;
                    op = op->pChild;
                    t2 |= (1 << j);
                }
            }
        }

        for (sceKernelDmaOperation *a2 = t1; a2; a2 = a2->pChild) {
            if (a2->dmaCh < 0) {
                u32 pos = lsbPosition(~t2 & a2->uiChMask[0]);
                if (pos >= 16) {
                    break;
                }
                g_dmacman.activeOps[pos] = t1;
                t1->dmaCh = pos;
                t2 |= (1 << pos);
            }

            u32 base = (t1->dmaCh / 8) ? 0xBCA00000 : 0xBC900000;
            if (!(HW(0xBC100050) & (0x20 << (t1->dmaCh / 8)))) {
                HW(0xBC100050) |= 0x20 << (t1->dmaCh / 8);
                HW(base + 48) |= 1;
                HW(base + 52) = 0;

                for (int i = 8; i > 0; i--) {
                    HW(base + 0x100 + 32*i) = 0;
                    HW(base + 0x104 + 32*i) = 0;
                    HW(base + 0x108 + 32*i) = 0;
                    HW(base + 0x10C + 32*i) = 0;
                    HW(base + 0x110 + 32*i) = 0;
                }

                HW(base + 8) = 0xFF;
                HW(base + 16) = 0xFF;
            }
            HW(base + 0x100 + (a2->dmaCh & 7) * 32) = t1->uiSrcAddr;
            HW(base + 0x104 + (a2->dmaCh & 7) * 32) = t1->uiDestAddr;
            HW(base + 0x108 + (a2->dmaCh & 7) * 32) = t1->uiLLI;
            HW(base + 0x10C + (a2->dmaCh & 7) * 32) = t1->uiControl;
            HW(base + 0x110 + (a2->dmaCh & 7) * 32) = t1->uiConfiguration;
            a2->uiFlag |= 2;
            if (a2->pParent && !a2->pChild) {
                break;
            } else {
                a2->uiFlag &= ~0x1;
                if (a2 == g_dmacman.queueTail)
                    g_dmacman.queueTail = a2->pPrev;
                if (a2 == g_dmacman.queueHead)
                    g_dmacman.queueHead = a2->pNext;
                if (a2->pPrev) {
                    a2->pPrev->pNext = a2->pNext;
                    a2->pPrev = NULL;
                }
                if (a2->pNext) {
                    a2->pNext->pPrev = a2->pPrev;
                    a2->pNext = NULL;
                }
            }
        }
    }

    g_dmacman.inUseChMask = ~g_dmacman.excludedChMask & t2;
}

//0x1804
s32 sceKernelDmaChExclude(u32 ch, u32 arg1)
{
    u32 intr;
    u32 ret;

    if (ch > 16)
        return 0x800202CF;

    intr = sceKernelCpuSuspendIntr();
    if (g_dmacman.reservedChMask & (1 << ch)) {
        ret = 0x800202C8;
    } else {
        if (!arg1) {
            g_dmacman.excludedChMask &= ~(1 << ch);
        } else {
            g_dmacman.excludedChMask |= (1 << ch);
            _sceKernelDmacClkEnable(ch / 8);
        }
        ret = SCE_ERROR_OK;
    }
    sceKernelCpuResumeIntr(intr);
    return ret;
}

//0x18CC
s32 sceKernelDmaChReserve(u32 ch, s32 arg1)
{
    u32 intr;
    u32 ret;

    intr = sceKernelCpuSuspendIntr();
    if (g_dmacman.excludedChMask & (1 << ch)) {
        ret = 0x800202C9;
    } else {
        if (arg1 < 0) {
            g_dmacman.reservedChMask &= ~(1 << ch);
        } else {
            g_dmacman.reservedChMask |= (1 << ch);
        }
        ret = SCE_ERROR_OK;
    }
    sceKernelCpuResumeIntr(intr);
    return ret;
}

//0x1964
sceKernelDmaOperation *sceKernelDmaOpAlloc(void)
{
    u32 intr = sceKernelCpuSuspendIntr();
    sceKernelDmaOperation *op = g_dmacman.freeList;
    if (op) {
        if (op->uiFlag & 0x1000) {
            g_dmacman.freeList = op->pNext;
            op->uiChMask[0] = 0xFFFF;
            op->cbfunc = NULL;
            op->dmaCh = -1;
            op->pChild = NULL;
            op->pParent = NULL;
        } else {
            op = NULL;
        }
    }
    sceKernelCpuResumeIntr(intr);
    return op;
}

//0x19E8
int sceKernelDmaOpAssign(sceKernelDmaOperation *op, int arg1, int arg2, int arg3, int arg4 __attribute__((unused)))
{
    if (!op)
        return 0x800202CF;
    if (op->uiFlag & 0x1000)
        return 0x800202C3;
    if (op->uiFlag & 0x1)
        return 0x800202BE;
    if (op->uiFlag & 0x2)
        return 0x800202C0;

    op->uiConfiguration = arg3;
    op->uiChMask[0] = (arg1 & 0xFF) | ((arg2 & 0xFF) << 8);
    op->uiFlag = 0x100;
    return SCE_ERROR_OK;
}

//0x1A60
s32 sceKernelDmaOpLLIConcatenate(sceKernelDmaOperation *op1, void *op2)
{
    PL080_LLI *head;

    op1->EndLLI->LLI = (u32)op2;
    g_dmacman.ddrFlushFunc(4);
    for (head = KUNCACHED(op2); head->LLI; head = KUNCACHED(head->LLI));
    op1->EndLLI = head;
    return SCE_ERROR_OK;
}

//0x1ADC
s32 sceKernelDmaOnDebugMode(void)
{
    return SCE_ERROR_OK;
}

//0x1AE4
static s32 suspendHandler(s32 flag __attribute__((unused)), void *param __attribute__((unused)))
{
    for (int i = 0; i < 16; i++) {
        if (g_dmacman.activeOps[i])
            abortDmaChannel(g_dmacman.activeOps[i]);
    }

    g_dmacman.inUseChMask = 0;
    sceKernelDmaOpAllCancel();
    sceKernelDisableIntr(22);
    sceKernelDisableIntr(23);
    return SCE_ERROR_OK;
}

//0x1B74
static s32 resumeHandler(s32 flag __attribute__((unused)), void *param __attribute__((unused)))
{
    if (g_dmacman.excludedChMask & 0xFF00)
        _sceKernelDmacClkEnable(1);
    if (g_dmacman.excludedChMask & 0xFF)
        _sceKernelDmacClkEnable(0);

    sceKernelEnableIntr(22);
    sceKernelEnableIntr(23);
    return SCE_ERROR_OK;
}

//0x1BF4 dummyFlush
static s32 _dummyDdrFlush(s32 arg __attribute__((unused)))
{
    pspSync();
    return SCE_ERROR_OK;
}

//0x1C14
static void _sceKernelDmacClkEnable(s32 arg0)
{
    u32 addr = arg0 ? 0xBCA00000 : 0xBC900000;
    if (HW(0xBC100050) & (0x20 << arg0))
        return;
    HW(0xBC100050) |= (0x20 << arg0);
    HW(addr + 48) |= 0x1;
    HW(addr + 52) = 0;
    for (int i = 0; i < 8; i++) {
        HW(addr + 32*i + 256) = 0;
        HW(addr + 32*i + 260) = 0;
        HW(addr + 32*i + 264) = 0;
        HW(addr + 32*i + 268) = 0;
        HW(addr + 32*i + 272) = 0;
    }
    HW(addr + 8) = 0xFF;
    HW(addr + 16) = 0xFF;
}
