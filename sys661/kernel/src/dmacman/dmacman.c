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
static s32 interruptHandler(s32 intrNo, s32 dmacIdx);
static void _sceKernelDmaSchedule(void);
static s32 suspendHandler(s32 flag, void *param);
static s32 resumeHandler(s32 flag, void *param);
static s32 _defaultDdrFlush(s32 mode);
static void _sceKernelDmacClkEnable(s32 dmacIdx);
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

    sceKernelDmaRegisterDdrFlush(_defaultDdrFlush);

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
    g_dmacman.ddrFlushFunc = ddrFlushFunc ? ddrFlushFunc : _defaultDdrFlush;
    return SCE_ERROR_OK;
}

//0x328
s32 sceKernelDmaSoftRequest(u32 dmacIdx, u32 reqLine, u32 isBurst, u32 isLast)
{
    u32 baseaddr = dmacIdx ? 0xBCA00000 : 0xBC900000;

    if (isLast) {
        if (isBurst) {
            HW(baseaddr + 40) = (1 << reqLine);
        } else {
            HW(baseaddr + 44) = (1 << reqLine);
        }
    } else if (isBurst) {
        HW(baseaddr + 32) = (1 << reqLine);
    } else {
        HW(baseaddr + 36) = (1 << reqLine);
    }
    pspSync();

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
    s32 ret = SCE_ERROR_OK;
    sceKernelDmaOperation *cur = op;

    if (!op)
        return 0x800202CF;
    if (op->pParent)
        return 0x800202CD;

    intr = sceKernelCpuSuspendIntr();
    for (; cur; cur = cur->pChild) {
        if (cur->uiFlag & 0x1) {
            ret = 0x800202BE;
            break;
        } else if (cur->uiFlag & 0x1000) {
            ret = 0x800202C3;
            break;
        } else if (!(cur->uiFlag & 0x100)) {
            ret = 0x800202C1;
            break;
        } else if (cur->uiFlag & 0x2) {
            ret = 0x800202C0;
            break;
        } else if (cur->uiFlag & 0x70) {
            ret = 0x800202C4;
            break;
        }
    }

    if (ret == SCE_ERROR_OK) {
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
    }

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

        op->pPrev = NULL;
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
int sceKernelDmaOpSetCallback(sceKernelDmaOperation *op, int (*func)(int, int), int cookie)
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

    op->pCookie = (void *)cookie;
    op->cbfunc = (void *)func;
    return SCE_ERROR_OK;
}

//0x808
s32 sceKernelDmaOpSetupMemcpy(sceKernelDmaOperation *op, s32 dstAddr, s32 srcAddr, u32 transferSize)
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
    if (transferSize >= 0x1000)
        return 0x800202CE;

    op->uiSrcAddr = srcAddr;
    op->uiDestAddr = dstAddr;
    op->uiLLI = 0;
    op->uiControl = transferSize | 0x8C4BF000;
    op->uiConfiguration |= 0xC001;

    return SCE_ERROR_OK;
}

//0x8A8
s32 sceKernelDmaOpSetupNormal(sceKernelDmaOperation *op, s32 config, s32 dstAddr, s32 srcAddr, s32 control)
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

    op->uiSrcAddr = srcAddr;
    op->uiDestAddr = dstAddr;
    op->uiLLI = 0;
    op->uiControl = control | 0x80000000;
    op->uiConfiguration |= (config | 0xC001);

    return SCE_ERROR_OK;
}

//0x938
int sceKernelDmaOpSetupLink(sceKernelDmaOperation *op, int config, u32 *lliHead)
{
    PL080_LLI *tail;
    PL080_LLI *head;

    if (!op)
        return 0x800202CF;
    if ((op->uiFlag & 0x1172) != 0x100) {
        if (op->uiFlag & 0x1000)
            return 0x800202C3;
        if ((op->uiFlag & 0x1172) == 0)
            return 0x800202C1;
        if (op->uiFlag & 0x2)
            return 0x800202C0;
        return 0x800202C4;
    }

    op->uiConfiguration |= (config | 0xC001);
    if (!((u32)lliHead & 0x3)) {
        for (tail = KUNCACHED(lliHead); tail->LLI; tail = KUNCACHED(tail->LLI));
        op->EndLLI = tail;
        tail->Control |= 0x80000000;
        head = KUNCACHED(lliHead);
    } else {
        head = KCACHED((u32)lliHead & 0xFFFFFFFC);
    }

    op->HeadLLI = head;
    op->uiSrcAddr = head->SrcAddr;
    op->uiDestAddr = head->DestAddr;
    op->uiLLI = head->LLI;
    op->uiControl = head->Control;
    if (!((u32)lliHead & 0x3)) {
        g_dmacman.ddrFlushFunc(4);
    }

    return SCE_ERROR_OK;
}

//0xA64
s32 sceKernelDmaOpSync(sceKernelDmaOperation *op, s32 command, u32 timeout)
{
    s32 err;
    u32 to;

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
        to = timeout;
        err = sceKernelWaitEventFlag(g_dmacman.evFlagId, op->evpat, 33, NULL, &to);
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
    u32 ch = (u32)(s32)op->dmaCh;
    u32 chOff = (ch & 7) << 5;
    u32 base = (ch >> 3) ? 0xBCA00000 : 0xBC900000;
    u32 address = chOff + base + 0x100;

    HW(address + 16) &= ~0x1;

    while (HW(address + 16) & 0x1);

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

    sceKernelCpuResumeIntrWithSync(intr);
    return ret;
}

//0xD80
int sceKernelDmaOpConcatenate(sceKernelDmaOperation *op, void *lliHead)
{
    PL080_LLI *tail;
    u32 intr;
    s32 ret;
    u32 addr;
    u32 addr16;

    if (!op)
        return 0x800202CF;

    for (tail = KUNCACHED(lliHead); tail->LLI; tail = KUNCACHED(tail->LLI));
    tail->Control |= 0x80000000;

    intr = sceKernelCpuSuspendIntr();

    if ((op->uiFlag & 0x1170) != 0x100) {
        if (op->uiFlag & 0x1000)
            ret = 0x800202C3;
        else if (!(op->uiFlag & 0x100))
            ret = 0x800202C1;
        else
            ret = 0x800202C4;
        sceKernelCpuResumeIntr(intr);
        return ret;
    }

    if (!(op->uiFlag & 0x2)) {
        if (op->uiFlag & 0x1) {
            op->EndLLI->LLI = (u32)lliHead;
            op->EndLLI = tail;
            ret = SCE_ERROR_OK;
        } else {
            ret = SCE_ERROR_KERNEL_ERROR;
        }
    } else {
        addr = (op->dmaCh / 8) ? 0xBCA00100 : 0xBC900100;
        addr += (op->dmaCh & 7) << 5;
        addr16 = HW(addr + 16) | 0x40000;
        HW(addr + 16) = addr16;
        if (!HW(addr + 8)) {
            s32 mask = addr16 & 0x1003800;
            if (mask == 0x1000000 || mask == 0x1001000 || mask == 0x1003000) {
                ret = 0x800202BC;
            } else {
                while (HW(addr + 16) & 0x20000);

                if (!(HW(addr + 12) & 0xFFF)) {
                    ret = 0x800202C4;
                } else {
                    HW(addr + 16) &= ~0x1;
                    while (HW(addr + 16) & 0x1);

                    HW(addr + 8) = (u32)lliHead;
                    HW(addr + 16) |= 1;
                    op->EndLLI = tail;
                    ret = SCE_ERROR_OK;
                }
            }
        } else {
            op->EndLLI->LLI = (u32)lliHead;
            op->EndLLI = tail;
            ret = SCE_ERROR_OK;
        }
        HW(addr + 16) &= ~0x40000;
    }

    g_dmacman.ddrFlushFunc(4);
    sceKernelCpuResumeIntrWithSync(intr);
    return ret;
}

//0xFC0
s32 sceKernelDmaOpAssignMultiple(sceKernelDmaOperation **ops, s32 size)
{
    u32 intr;
    s32 remaining = size;

    if ((u32)(size - 2) >= 15)
        return 0x800202BD;

    for (int i = 0; i < 16; i++) {
        if ((ops[0]->uiChMask[0] >> i) & 1)
            remaining--;
    }

    if (remaining < 0)
        return 0x800202BD;

    for (int i = 0; i < size - 1; i++) {
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
        ops[i+1]->uiChMask[0] = ops[0]->uiChMask[0];
    }
    ops[0]->uiChMask[1] = size;
    sceKernelCpuResumeIntr(intr);
    return SCE_ERROR_OK;
}

//0x1148
static s32 interruptHandler(s32 intrNo __attribute__((unused)), s32 dmacIdx)
{
    u32 intr = sceKernelCpuSuspendIntr();
    u32 addr = dmacIdx ? 0xBCA00000 : 0xBC900000;

    u32 tcStatus, errStatus, intStatus, evMask = 0, cbStatus;

    tcStatus = HW(addr + 4);
    errStatus = HW(addr + 12);
    intStatus = (tcStatus | errStatus) & 0xFF;

    for (int i = 0; intStatus >= (1U << i); i++) {
        if (intStatus & (1U << i)) {
            int ch = i + 8 * dmacIdx;
            if (!g_dmacman.activeOps[ch]) {
                if ((g_dmacman.excludedChMask & (1 << ch)) == 0)
                    Kprintf("Fatal error: Stray interrupt occurred\n");
            } else {
                sceKernelDmaOperation *op = g_dmacman.activeOps[ch];
                u32 chAddr = addr + 0x100 + i * 32;
                op->uiSrcAddr = HW(chAddr);
                op->uiDestAddr = HW(chAddr + 4);
                op->uiLLI = HW(chAddr + 8);
                op->uiControl = HW(chAddr + 12);
                op->uiConfiguration = HW(chAddr + 16);
                if (tcStatus & (1 << i)) {
                    if (!HW(chAddr + 8) && !(HW(chAddr + 16) & 0x1)) {
                        g_dmacman.inUseChMask &= ~(1 << ch);
                        evMask |= op->evpat;
                        op->uiFlag |= 0x40;
                        op->uiFlag &= ~0x2;
                        op->dmaCh = -1;
                        g_dmacman.activeOps[ch] = NULL;
                    }
                    HW(addr + 8) = (1 << i);
                    cbStatus = 0;
                } else {
                    if (HW(chAddr + 8) && (HW(chAddr + 16) & 0x1)) {
                        abortDmaChannel(op);
                    }
                    g_dmacman.inUseChMask &= ~(1 << ch);
                    g_dmacman.activeOps[ch] = NULL;
                    evMask |= op->evpat;
                    op->uiFlag |= 0x40;
                    op->uiFlag &= ~0x2;
                    op->dmaCh = -1;
                    HW(addr + 16) = (1 << i);
                    cbStatus = -1;
                }
                if (op->cbfunc && op->cbfunc(op, cbStatus, ch, op->pCookie) == 1) {
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
    sceKernelDmaOperation *queuedOp = g_dmacman.queueHead;
    u32 busyMask = g_dmacman.inUseChMask | g_dmacman.excludedChMask;
    u32 availMask = busyMask ^ 0xFFFF;

    for (; queuedOp && availMask; queuedOp = queuedOp->pNext) {
        if (queuedOp->pChild && !queuedOp->pParent) {
            u32 eligibleMask = queuedOp->uiChMask[0] & ~busyMask;
            s32 neededCh = queuedOp->uiChMask[1];
            for (int i = 0; i < 16; i++) {
                neededCh -= (eligibleMask >> i) & 0x1;
            }
            if (neededCh < 0) {
                if (queuedOp->pNext) {
                    sceKernelDmaOperation *nextOp = queuedOp->pNext;
                    nextOp->pPrev = queuedOp->pPrev;
                    queuedOp->pPrev = nextOp;
                    queuedOp->pNext = nextOp->pNext;
                    if (nextOp->pPrev)
                        nextOp->pPrev->pNext = nextOp;
                    else
                        g_dmacman.queueHead = nextOp;
                    if (!nextOp->pNext)
                        g_dmacman.queueTail = queuedOp;
                    else
                        queuedOp->pNext->pPrev = queuedOp;
                    nextOp->pNext = queuedOp;
                }
                continue;
            }

            sceKernelDmaOperation *op = queuedOp;
            for (int j = 0; op && j < 16; j++) {
                if (eligibleMask & (1 << j)) {
                    g_dmacman.activeOps[j] = op;
                    op->dmaCh = j;
                    op = op->pChild;
                    busyMask |= (1 << j);
                }
            }
        }

        for (sceKernelDmaOperation *curOp = queuedOp; curOp; curOp = curOp->pChild) {
            if (curOp->dmaCh < 0) {
                u32 pos = lsbPosition(~busyMask & curOp->uiChMask[0]);
                if (pos >= 16) {
                    break;
                }
                g_dmacman.activeOps[pos] = curOp;
                curOp->dmaCh = pos;
                busyMask |= (1 << pos);
            }

            u32 base = (curOp->dmaCh / 8) ? 0xBCA00000 : 0xBC900000;
            if (!(HW(0xBC100050) & (0x20 << (curOp->dmaCh / 8)))) {
                HW(0xBC100050) |= 0x20 << (curOp->dmaCh / 8);
                HW(base + 48) |= 1;
                HW(base + 52) = 0;

                for (int i = 0; i < 8; i++) {
                    HW(base + 0x100 + 32*i) = 0;
                    HW(base + 0x104 + 32*i) = 0;
                    HW(base + 0x108 + 32*i) = 0;
                    HW(base + 0x10C + 32*i) = 0;
                    HW(base + 0x110 + 32*i) = 0;
                }

                HW(base + 8) = 0xFF;
                HW(base + 16) = 0xFF;
            }
            HW(base + 0x100 + (curOp->dmaCh & 7) * 32) = curOp->uiSrcAddr;
            HW(base + 0x104 + (curOp->dmaCh & 7) * 32) = curOp->uiDestAddr;
            HW(base + 0x108 + (curOp->dmaCh & 7) * 32) = curOp->uiLLI;
            HW(base + 0x10C + (curOp->dmaCh & 7) * 32) = curOp->uiControl;
            HW(base + 0x110 + (curOp->dmaCh & 7) * 32) = curOp->uiConfiguration;
            curOp->uiFlag |= 2;
            if (!curOp->pParent) {
                curOp->uiFlag &= ~0x1;
                if (curOp == g_dmacman.queueTail)
                    g_dmacman.queueTail = curOp->pPrev;
                if (curOp == g_dmacman.queueHead)
                    g_dmacman.queueHead = curOp->pNext;
                if (curOp->pPrev) {
                    curOp->pPrev->pNext = curOp->pNext;
                    curOp->pPrev = NULL;
                }
                if (curOp->pNext) {
                    curOp->pNext->pPrev = curOp->pPrev;
                    curOp->pNext = NULL;
                }
            }
        }
        availMask = busyMask ^ 0xFFFF;
    }

    g_dmacman.inUseChMask = ~g_dmacman.excludedChMask & busyMask;
}

//0x1804
s32 sceKernelDmaChExclude(u32 ch, u32 exclude)
{
    u32 intr;
    u32 ret;

    if (ch >= 16)
        return 0x800202CF;

    intr = sceKernelCpuSuspendIntr();
    if (g_dmacman.reservedChMask & (1 << ch)) {
        ret = 0x800202C8;
    } else {
        if (!exclude) {
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
s32 sceKernelDmaChReserve(u32 ch, s32 reserve)
{
    u32 intr;
    u32 ret;

    intr = sceKernelCpuSuspendIntr();
    if (g_dmacman.excludedChMask & (1 << ch)) {
        ret = 0x800202C9;
    } else {
        if (reserve < 0) {
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
            op->uiFlag = 0;
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
int sceKernelDmaOpAssign(sceKernelDmaOperation *op, int dmac0Mask, int dmac1Mask, int config, int reserved __attribute__((unused)))
{
    if (!op)
        return 0x800202CF;
    if (op->uiFlag & 0x1000)
        return 0x800202C3;
    if (op->uiFlag & 0x1)
        return 0x800202BE;
    if (op->uiFlag & 0x2)
        return 0x800202C0;

    op->uiConfiguration = config;
    op->uiChMask[0] = (dmac0Mask & 0xFF) | ((dmac1Mask & 0xFF) << 8);
    op->uiFlag = 0x100;
    return SCE_ERROR_OK;
}

//0x1A60
s32 sceKernelDmaOpLLIConcatenate(sceKernelDmaOperation *op, void *lliHead)
{
    PL080_LLI *head;

    op->EndLLI->LLI = (u32)lliHead;
    g_dmacman.ddrFlushFunc(4);
    for (head = KUNCACHED(lliHead); head->LLI; head = KUNCACHED(head->LLI));
    op->EndLLI = head;
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

//0x1BF4 default DDR flush
static s32 _defaultDdrFlush(s32 mode __attribute__((unused)))
{
    pspSync();
    return SCE_ERROR_OK;
}

//0x1C14
static void _sceKernelDmacClkEnable(s32 dmacIdx)
{
    u32 addr = dmacIdx ? 0xBCA00000 : 0xBC900000;
    if (HW(0xBC100050) & (0x20 << dmacIdx))
        return;
    HW(0xBC100050) |= (0x20 << dmacIdx);
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
