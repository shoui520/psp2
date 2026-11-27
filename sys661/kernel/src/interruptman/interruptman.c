/* Copyright (C) 2011, 2012 The uOFW team
   See the file COPYING for copying permission.
*/

#include <common_imp.h>
#include <exceptionman.h>
#include <sysmem_kernel.h>
#include <sysmem_suspend_kernel.h>

#include <interruptman.h>

#include "interruptman_int.h"
#include "start.h"

SceResidentLibraryEntryTable intrEntry = {
    .libName = "InterruptManager",
    .version = {
        0x11,
        0
    },
    .attribute = 0x4000,
    .len = 4,
    .vStubCount = 0,
    .stubCount = 9,
    .entryTable = (void*)(s32[]) {
        0x5CB5A78B,
        0x7860E0DC,
        0x8A389411,
        0xCA04A2B9,
        0xD2E8363F,
        0xD61E6961,
        0xEEE43F47,
        0xFB8E22EC,
        0xFC4374B8,
        (s32)sceKernelSuspendSubIntr,
        (s32)sceKernelResumeSubIntr,
        (s32)sceKernelDisableSubIntr,
        (s32)sceKernelRegisterSubIntrHandler,
        (s32)QueryIntrHandlerInfoForUser,
        (s32)sceKernelReleaseSubIntrHandler,
        (s32)sceKernelRegisterUserSpaceIntrStack,
        (s32)sceKernelEnableSubIntr,
        (s32)sceKernelIsSubInterruptOccurred
    },
    .vStubCount16 = 0,
    .nidTableInfo = 0,
    .nidTableCount = 0
};

SCE_MODULE_INFO("sceInterruptManager", SCE_MODULE_KERNEL | SCE_MODULE_ATTR_CANT_STOP | SCE_MODULE_ATTR_EXCLUSIVE_LOAD |
                                       SCE_MODULE_ATTR_EXCLUSIVE_START, 1, 9);
SCE_MODULE_BOOTSTART("IntrManInit");
SCE_MODULE_REBOOT_BEFORE("IntrManTerminate");
SCE_SDK_VERSION(SDK_VERSION);

SceSyscallTable g_emptyTable = {
    .next = NULL,
    .seed = 0,
    .funcTableSize = 0,
    .tableSize = 0,
};

SceSyscallTable g_syscallTable = {
    .next = &g_emptyTable,
    .seed = 0,
    .funcTableSize = 0x00000100,
    .tableSize = 0x00000110,
    .syscalls = {
        invalidSyscallHandler, invalidSyscallHandler, returnFromUserIntrHandler, invalidSyscallHandler,
        invalidSyscallHandler, invalidSyscallHandler, invalidSyscallHandler, invalidSyscallHandler,
        syscallCpuSuspendIntr, syscallCpuResumeIntr, syscallCpuResumeIntrWithSync, syscallInvertIntrBit,
        syscallIsIntrContext, syscallGetIntrState, syscallSuspendIntrAndDisableCache, syscallResumeIntrAndEnableCache,
        syscallGetCpuClockCounter, syscallGetCpuPrid, syscallGetCpuId, syscallIsCop2Enabled,
        syscallIsKernelMode, invalidSyscallHandler, invalidSyscallHandler, invalidSyscallHandler,
        invalidSyscallHandler, invalidSyscallHandler, invalidSyscallHandler, invalidSyscallHandler,
        invalidSyscallHandler, invalidSyscallHandler, invalidSyscallHandler, invalidSyscallHandler,
        invalidSyscallHandler, invalidSyscallHandler, invalidSyscallHandler, invalidSyscallHandler,
        invalidSyscallHandler, invalidSyscallHandler, invalidSyscallHandler, invalidSyscallHandler,
        invalidSyscallHandler, invalidSyscallHandler, invalidSyscallHandler, invalidSyscallHandler,
        invalidSyscallHandler, invalidSyscallHandler, invalidSyscallHandler, invalidSyscallHandler,
        invalidSyscallHandler, invalidSyscallHandler, invalidSyscallHandler, invalidSyscallHandler,
        invalidSyscallHandler, invalidSyscallHandler, invalidSyscallHandler, invalidSyscallHandler,
        invalidSyscallHandler, invalidSyscallHandler, invalidSyscallHandler, invalidSyscallHandler,
        invalidSyscallHandler, invalidSyscallHandler, invalidSyscallHandler, invalidSyscallHandler
    }
};

s8 g_stackBottom[672];

s8 g_stackMiddle[7232];

s8 g_stackTop[304];

s8 g_syscCtx[16];

typedef struct {
    u32 reservedHeader[9];
    u16 saveKind;
    u16 fpuSaved;
    u32 fcsr;
    u32 hi;
    u32 lo;
    u32 status;
    u32 epc;
    u32 cop0Ctrl23;
    u32 cause;
    u32 gprAt;
    u32 gprV0;
    u32 gprV1;
    u32 gprA0;
    u32 gprA1;
    u32 gprA2;
    u32 gprA3;
    u32 gprT0;
    u32 gprT1;
    u32 gprT2;
    u32 gprT3;
    u32 gprT4;
    u32 gprT5;
    u32 gprT6;
    u32 gprT7;
    u32 gprS0;
    u32 gprS1;
    u32 gprS2;
    u32 gprS3;
    u32 gprS4;
    u32 gprS5;
    u32 gprS6;
    u32 gprS7;
    u32 gprT8;
    u32 gprT9;
    u32 gprK0;
    u32 gprK1;
    u32 gprGp;
    s32 gprSp;
    u32 gprFp;
    u32 gprRa;
} SceIntrKernelFrame;

typedef struct __attribute__((aligned(64)))
{
    s32 grpsOpt[24]; // 0
    u8 reservedFlag; // 96
    u8 userIntrDepth; // 97
    u8 defaultLoggingDisabled; // 98
    u8 reservedPad; // 99
    MonitorCb monitorCbBefore; // 100
    MonitorCb monitorCbAfter; // 104
    s32 savedIntrExcepEntry; // 108
    s32 intrStack; // 112
    s32 intrStackArg; // 116
    u32 clockCounterLo; // 120
    u32 clockCounterHi; // 124
    Interrupt intr[68]; // 128
    s32 subIntrMemoryPoolId; // 4480
    s32 intcState[2]; // 4484
    s32 count; // 4492
    s32 compare; // 4496
} InterruptInfo;

InterruptInfo intInfo;

s32 IntrManInit(void)
{
    dbg_init(1, FB_NONE, FAT_HARDWARE);
    dbg_printf("Called %s\n", __FUNCTION__);
    s32 oldIc = sceKernelCpuSuspendIntr();
    pspCop0CtrlSet(COP0_CTRL_IS_INTERRUPT, 0);
    pspCop0CtrlSet(COP0_CTRL_SP_KERNEL, (s32)g_stackTop);
    pspCop0CtrlSet(COP0_CTRL_SP_USER, 0);
    mymemset(&intInfo, 0, sizeof(InterruptInfo));
    intInfo.defaultLoggingDisabled = 1;
    intcMaskAll();
    ReleaseContextHooks();
    s32 i;
    for (i = 0; i < 68; i++) {
        intInfo.intr[i].minClockLo = (u32)-1;
        intInfo.intr[i].minClockHi = (u32)-1;
    }
    sceKernelRegisterExceptionHandler(EXCEP_INT, (void*)intrExcepHandler);
    sceKernelRegisterPriorityExceptionHandler(EXCEP_INT, 3, (void*)intrExcepHandler2);
    sceKernelRegisterExceptionHandler(EXCEP_SYS, (void*)syscallExcepHandler);
    sceKernelRegisterIntrHandler(67, 0, cp0TimerIntrHandler, 0, 0);
    sceKernelRegisterSuspendHandler(29, SuspendIntc, 0);
    sceKernelRegisterResumeHandler(29, ResumeIntc, 0);
    sceKernelSetIntrLogging(67, 0);
    pspCop0StateSet(COP0_STATE_STATUS, (pspCop0StateGet(COP0_STATE_STATUS) & 0xFFFF00FF) | 0x0400);
    sceKernelEnableIntr(67);
    pspCop0StateSet(COP0_STATE_COUNT, 0);
    pspCop0StateSet(COP0_STATE_COMPARE, 0x80000000);
    sceKernelCpuResumeIntr(oldIc);
    s32 ret = sceKernelRegisterLibrary(&intrEntry);
    dbg_printf("init ok\n");
    if (ret < 0)
        return ret;
    return 0;
}

s32 sceKernelRegisterIntrHandler(s32 intrNum, s32 saveContext, void *func, void *common, SceIntrHandler *handler)
{
    dbg_printf("sceKernelRegisterIntrHandler(%d, %d, %08x, %08x, %08x)\n", intrNum, saveContext, func, common, handler);
    if (sceKernelIsIntrContext() != 0)
        return SCE_ERROR_KERNEL_CANNOT_BE_CALLED_FROM_INTERRUPT;
    if ((u32)intrNum >= 68)
        return SCE_ERROR_KERNEL_INVALID_INTERRUPT;
    if (handler != NULL && handler->size != 12)
        return SCE_ERROR_KERNEL_INVALID_INTERRUPT_ARGUMENT;
    s32 oldIc = sceKernelCpuSuspendIntr();
    if (handler != NULL && handler->attr > 0 && intInfo.subIntrMemoryPoolId == 0) {
        s32 ret = sceKernelCreateHeap(1, 0x800, 1, "SceInterruptManager");
        if (ret > 0)
            intInfo.subIntrMemoryPoolId = ret;
        if (ret < 0) {
            sceKernelCpuResumeIntr(oldIc);
            return ret;
        }
    }
    Interrupt *intr = &intInfo.intr[intrNum];
    if (intr->handler != 0 && (intr->handler & 3) != 1) {
        sceKernelCpuResumeIntr(oldIc);
        return SCE_ERROR_KERNEL_HANDLER_ALREADY_EXISTS;
    }
    if ((s32)func >= 0) {
        sceKernelCpuResumeIntr(oldIc);
        return SCE_ERROR_KERNEL_INVALID_INTERRUPT;
    }
    intr->subIntrs = NULL;
    intr->cb = NULL;
    s32 size;
    if (handler == NULL || (size = handler->attr) <= 0) {
        if (intr->subIntrs != NULL) {
            sceKernelFreeHeapMemory(intInfo.subIntrMemoryPoolId, intr->subIntrs);
            intr->subIntrs = NULL;
        }
    } else {
        s32 allocSize = size << 6;
        SubInterrupt *subIntrs = sceKernelAllocHeapMemory(intInfo.subIntrMemoryPoolId, allocSize);
        if (subIntrs != NULL)
            mymemset(subIntrs, 0, allocSize);
        intr->subIntrs = subIntrs;
        if (subIntrs == NULL) {
            sceKernelCpuResumeIntr(oldIc);
            return SCE_ERROR_KERNEL_NO_MEMORY;
        }
        intr->cb = handler->cb;
    }
    sceKernelSuspendIntr(intrNum, 0);
    s32 intrLevel;
    if (intr->handler != 1 && intrNum < 64)
        intrLevel = 3;
    else
        intrLevel = 0;
    if (saveContext != 0)
        saveContext = 2;
    intr->flags = (intr->flags & 0xFFFFFCFFu) | ((u32)(intrLevel << 8) & 0x300u);
    intr->handler = saveContext | (s32)func;
    intr->gp = sceKernelGetModuleGPByAddressForKernel((u32)func);
    intr->arg = common;
    intr->bits.subCount = 0;
    if (handler != NULL)
        intr->bits.subCount = (u8)handler->attr;
    intr->flags = ((intr->flags & 0x7FFF7FFFu) | ((u32)(intInfo.defaultLoggingDisabled << 15) & 0x8000u)) & 0x7FFFFFFFu;
    intr->bits.intrNum = (u8)intrNum;
    sceKernelCpuResumeIntr(oldIc);
    return 0;
}

s32 sceKernelSetUserModeIntrHanlerAcceptable(s32 intrNum, s32 subIntrNum, s32 setBit)
{
    dbg_printf("Called %s\n", __FUNCTION__);
    if (sceKernelIsIntrContext() != 0)
        return SCE_ERROR_KERNEL_CANNOT_BE_CALLED_FROM_INTERRUPT;
    if ((u32)intrNum >= 68)
        return SCE_ERROR_KERNEL_INVALID_INTERRUPT;
    s32 oldIntr = sceKernelCpuSuspendIntr();
    Interrupt *intr = &intInfo.intr[intrNum];
    if (intr->handler == 0 || (intr->handler & 3) == 1) {
        sceKernelCpuResumeIntr(oldIntr);
        return SCE_ERROR_KERNEL_HANDLER_NOTFOUND;
    }
    if (subIntrNum < 0 || subIntrNum >= (s32)intr->bits.subCount) {
        sceKernelCpuResumeIntr(oldIntr);
        return SCE_ERROR_KERNEL_INVALID_INTERRUPT;
    }
    SubInterrupt *subIntr = &intr->subIntrs[subIntrNum];
    subIntr->flags = (subIntr->flags & 0xFFFFFBFFu) | ((u32)((setBit != 0) << 10) & 0x00000400u);
    intr->flags &= 0xFFFFF7FFu;
    u8 num = intr->bits.subCount;
    u8 acceptable = (u8)((subIntr->flags >> 10) & 1u);
    s32 i;
    for (i = 0; i < (s32)num; i++) {
        if (acceptable != 0) {
            intr->flags |= 0x800u;
            break;
        }
    }
    if (((intr->flags >> 11) & 1u) != 0)
        sceKernelSetIntrLevel(intrNum, 2);
    sceKernelCpuResumeIntr(oldIntr);
    return 0;
}

s32 sceKernelReleaseIntrHandler(s32 intrNum)
{
    dbg_printf("Called %s\n", __FUNCTION__);
    if (sceKernelIsIntrContext() != 0)
        return SCE_ERROR_KERNEL_CANNOT_BE_CALLED_FROM_INTERRUPT;
    if ((u32)intrNum >= 68)
        return SCE_ERROR_KERNEL_INVALID_INTERRUPT;
    s32 oldIntr = sceKernelCpuSuspendIntr();
    Interrupt *intr = &intInfo.intr[intrNum];
    if (intr->handler == 0) {
        sceKernelCpuResumeIntr(oldIntr);
        return SCE_ERROR_KERNEL_HANDLER_NOTFOUND;
    }
    if (((intr->flags >> 8) & 3u) != 0) {
        intr->handler = 0;
        sceKernelSuspendIntr(intrNum, 0);
    } else
        intr->handler = 1;
    intr->cb = NULL;
    if (intr->subIntrs != NULL)
        sceKernelFreeHeapMemory(intInfo.subIntrMemoryPoolId, intr->subIntrs);
    intr->subIntrs = NULL;
    sceKernelCpuResumeIntr(oldIntr);
    return 0;
}

s32 sceKernelSetIntrLevel(s32 intrNum, s32 level)
{
    dbg_printf("Called %s\n", __FUNCTION__);
    if ((u32)intrNum >= 64)
        return SCE_ERROR_KERNEL_INVALID_INTERRUPT;
    if (level < 1 || level >= 4)
        return SCE_ERROR_KERNEL_INVALID_INTERRUPT_LEVEL;
    Interrupt *intr = &intInfo.intr[intrNum];
    s32 oldIntr = sceKernelCpuSuspendIntr();
    if (intr->handler == 0 || (intr->handler & 3) == 1) {
        sceKernelCpuResumeIntr(oldIntr);
        return SCE_ERROR_KERNEL_HANDLER_NOTFOUND;
    }
    if (((intr->flags >> 11) & 1u) != 0 && ((level ^ 2) != 0)) {
        sceKernelCpuResumeIntr(oldIntr);
        return SCE_ERROR_KERNEL_INVALID_INTERRUPT_LEVEL;
    }
    if ((((u32)intInfo.grpsOpt[((u32)intrNum >> 5) + 6] >> (intrNum & 0x1F)) & 1u) == 0) {
        intr->flags = (intr->flags & 0xFFFFFCFFu) | ((u32)(level << 8) & 0x00000300u);
    } else {
        if ((s32)((intr->flags >> 8) & 3u) < level) {
            InterruptDisableInTable((u32)intrNum);
        }
        intr->flags = (intr->flags & 0xFFFFFCFFu) | ((u32)(level << 8) & 0x00000300u);
        InterruptEnableInTable((u32)intrNum);
    }
    sceKernelCpuResumeIntr(oldIntr);
    return 0;
}

s32 sceKernelSetIntrLogging(s32 intrNum, s32 enable)
{
    dbg_printf("Called %s\n", __FUNCTION__);
    if ((u32)intrNum >= 68)
        return SCE_ERROR_KERNEL_INVALID_INTERRUPT;
    s32 oldIc = sceKernelCpuSuspendIntr();
    Interrupt *intr = &intInfo.intr[intrNum];
    if (enable == 0) {
        intr->flags |= 0x00008000u;
    } else
        intr->flags &= 0xFFFF7FFFu;
    sceKernelCpuResumeIntr(oldIc);
    return 0;
}

s32 sceKernelEnableIntr(s32 intNum)
{
    dbg_printf("sceKernelEnableIntr(%d)\n", intNum);
    s32 mask;
    if ((u32)intNum >= 68)
        return SCE_ERROR_KERNEL_INVALID_INTERRUPT;
    s32 oldIc = sceKernelCpuSuspendIntr();
    if (intInfo.intr[intNum].handler == 0) {
        sceKernelCpuResumeIntr(oldIc);
        return SCE_ERROR_KERNEL_HANDLER_NOTFOUND;
    }
    if (intNum < 64) {
        InterruptEnableInTable((u32)intNum);
        sceKernelCpuResumeIntr(oldIc);
        return 0;
    }
    if (intNum == 64)
        mask = 0x100;
    else if (intNum == 65)
        mask = 0x200;
    else if (intNum == 66)
        mask = 0x1000;
    else if (intNum == 67)
        mask = 0x8000;
    else
        mask = 0;
    pspCop0StateSet(COP0_STATE_STATUS, pspCop0StateGet(COP0_STATE_STATUS) | (mask & 0xFF00));
    sceKernelCpuResumeIntr(oldIc);
    return 0;
}

s32 sceKernelSuspendIntr(s32 intrNum, s32 *prevState)
{
    dbg_printf("sceKernelSuspendIntr(%d, %08x)\n", intrNum, prevState);
    s32 ret = 0;
    s32 mask;
    if ((u32)intrNum >= 68)
        return SCE_ERROR_KERNEL_INVALID_INTERRUPT;
    s32 oldIc = sceKernelCpuSuspendIntr();
    if (intrNum == 64)
        mask = 0x100;
    else if (intrNum == 65)
        mask = 0x200;
    else if (intrNum == 66)
        mask = 0x1000;
    else if (intrNum == 67)
        mask = 0x8000;
    else
        mask = 0;
    if (intInfo.intr[intrNum].handler != 0) {
        if (prevState != NULL) {
            if (intrNum >= 64) {
                *prevState = (pspCop0StateGet(COP0_STATE_STATUS) & 0xFF00) & mask;
            } else
                *prevState = ((u32)intInfo.grpsOpt[((u32)intrNum >> 5) + 6] >> (intrNum & 0x1F)) & 1u;
        }
    } else {
        ret = SCE_ERROR_KERNEL_HANDLER_NOTFOUND;
        if (prevState != NULL)
            *prevState = 0;
    }
    if (intrNum < 64) {
        AllLevelInterruptDisable((u32)intrNum);
    } else
        pspCop0StateSet(COP0_STATE_STATUS, pspCop0StateGet(COP0_STATE_STATUS) & ~(mask & 0xFF00));
    sceKernelCpuResumeIntr(oldIc);
    return ret;
}

s32 sceKernelResumeIntr(s32 intrNum, s32 prevState)
{
    dbg_printf("Called %s\n", __FUNCTION__);
    if ((u32)intrNum >= 68)
        return SCE_ERROR_KERNEL_INVALID_INTERRUPT;
    s32 oldIntr = sceKernelCpuSuspendIntr();
    s32 mask;
    if (intrNum == 64)
        mask = 0x100;
    else if (intrNum == 65)
        mask = 0x200;
    else if (intrNum == 66)
        mask = 0x1000;
    else if (intrNum == 67)
        mask = 0x8000;
    else
        mask = 0;
    if (intInfo.intr[intrNum].handler != 0) {
        if (intrNum >= 64) {
            if (prevState != 0) {
                pspCop0StateSet(COP0_STATE_STATUS, pspCop0StateGet(COP0_STATE_STATUS) | (mask & 0xFF00));
                sceKernelCpuResumeIntr(oldIntr);
                return 0;
            }
        } else {
            if (prevState == 0) {
                AllLevelInterruptDisable((u32)intrNum);
            } else
                InterruptEnableInTable((u32)intrNum);
            sceKernelCpuResumeIntr(oldIntr);
            return 0;
        }
    } else if (intrNum < 64) {
        AllLevelInterruptDisable((u32)intrNum);
        sceKernelCpuResumeIntr(oldIntr);
        return SCE_ERROR_KERNEL_HANDLER_NOTFOUND;
    } else {
        pspCop0StateSet(COP0_STATE_STATUS, pspCop0StateGet(COP0_STATE_STATUS) & ~(mask & 0xFF00));
        sceKernelCpuResumeIntr(oldIntr);
        return SCE_ERROR_KERNEL_HANDLER_NOTFOUND;
    }
    pspCop0StateSet(COP0_STATE_STATUS, pspCop0StateGet(COP0_STATE_STATUS) & ~(mask & 0xFF00));
    sceKernelCpuResumeIntr(oldIntr);
    return 0;
}

void ReleaseContextHooks(void)
{
    dbg_printf("Called %s\n", __FUNCTION__);
    InterruptManagerForKernel_E790EAED(defaultContextHook, defaultContextHook);
}

void InterruptManagerForKernel_E790EAED(s32 (*beforeHook)(), s32 (*afterHook)())
{
    dbg_printf("Called %s\n", __FUNCTION__);
    s32 oldIc = sceKernelCpuSuspendIntr();
    contextHookAfterInsn = 0x0C000000 + (((s32)afterHook >> 2) & 0x3FFFFFF);
    pspCache(0x1A, &contextHookAfterInsn);
    pspCache(0x08, &contextHookAfterInsn);
    contextHookBeforeInsn = 0x0C000000 + (((s32)beforeHook >> 2) & 0x3FFFFFF);
    pspCache(0x1A, &contextHookBeforeInsn);
    pspCache(0x08, &contextHookBeforeInsn);
    sceKernelCpuResumeIntr(oldIc);
}

s32 sceKernelCallSubIntrHandler(s32 intrNum, s32 subIntrNum, s32 userArg, s32 extraArg)
{
    dbg_printf("Called %s\n", __FUNCTION__);
    SubInterrupt *subIntr = &intInfo.intr[intrNum].subIntrs[subIntrNum];
    if ((u32)subIntr->handler < 2u)
        return SCE_ERROR_KERNEL_HANDLER_NOTFOUND;
    if (intInfo.monitorCbBefore != NULL) {
        if (((subIntr->flags >> 15) & 1u) == 0)
            subIntr->callCount++;
        intInfo.monitorCbBefore(intrNum, subIntrNum, userArg, subIntr->arg, extraArg, subIntr->handler, intInfo.userIntrDepth);
    }
    s32 oldGp = pspGetGp();
    pspSetGp(subIntr->gp);
    s32 ret;
    if (subIntr->handler >= 0) {
        ret = sceKernelCallUserIntrHandler(userArg, subIntr->arg, extraArg, 0, subIntr->handler, sceKernelGetUserIntrStack());
    } else {
        s32 (*func)(s32, s32, s32) = (void*)subIntr->handler;
        ret = func(userArg, subIntr->arg, extraArg);
    }
    pspSetGp(oldGp);
    if (intInfo.monitorCbAfter != 0) {
        intInfo.monitorCbAfter(intrNum, subIntrNum, userArg, subIntr->arg, extraArg, ret, intInfo.userIntrDepth);
    }
    return ret;
}

s32 sceKernelGetUserIntrStack(void)
{
    dbg_printf("Called %s\n", __FUNCTION__);
    if (intInfo.userIntrDepth == 0)
        return intInfo.intrStack;
    const SceIntrKernelFrame *frame = (const SceIntrKernelFrame *)pspCop0CtrlGet(COP0_CTRL_SP_KERNEL);
    return frame->gprSp;
}

s32 sceKernelRegisterSubIntrHandler(s32 intrNum, s32 subIntrNum, void *handler, void *arg)
{
    dbg_printf("Called %s\n", __FUNCTION__);
    if ((u32)intrNum >= 68)
        return SCE_ERROR_KERNEL_INVALID_INTERRUPT;
    s32 oldK1 = pspShiftK1();
    s32 oldIntr = sceKernelCpuSuspendIntr();
    Interrupt *intr = &intInfo.intr[intrNum];
    if (intr->handler == 0) {
        sceKernelCpuResumeIntr(oldIntr);
        pspSetK1(oldK1);
        return SCE_ERROR_KERNEL_HANDLER_NOTFOUND;
    }
    if (((s32)handler >> 31) == 0) {
        if (intInfo.intrStack == 0) {
            return SCE_ERROR_KERNEL_INVALID_INTERRUPT;
        }
        if (((intr->flags >> 11) & 1u) == 0) {
            sceKernelCpuResumeIntr(oldIntr);
            pspSetK1(oldK1);
            return SCE_ERROR_KERNEL_INVALID_INTERRUPT;
        }
    } else if (pspK1IsUserMode()) {
        sceKernelCpuResumeIntr(oldIntr);
        pspSetK1(oldK1);
        return SCE_ERROR_KERNEL_INVALID_INTERRUPT;
    }
    if (subIntrNum < 0 || subIntrNum >= (s32)intr->bits.subCount) {
        sceKernelCpuResumeIntr(oldIntr);
        pspSetK1(oldK1);
        return SCE_ERROR_KERNEL_INVALID_INTERRUPT;
    }
    SubInterrupt *subIntr = &intr->subIntrs[subIntrNum];
    if (subIntr->handler != 0) {
        sceKernelCpuResumeIntr(oldIntr);
        pspSetK1(oldK1);
        return SCE_ERROR_KERNEL_HANDLER_ALREADY_EXISTS;
    }
    if (((s32)handler >> 31) == 0 && ((subIntr->flags >> 10) & 1u) == 0) {
        sceKernelCpuResumeIntr(oldIntr);
        pspSetK1(oldK1);
        return SCE_ERROR_KERNEL_INVALID_INTERRUPT;
    }
    if (intr->cb != NULL && intr->cb->cbRegBefore != NULL) {
        s32 ret = intr->cb->cbRegBefore(intrNum, subIntrNum, handler, arg);
        if (ret != 0) {
            sceKernelCpuResumeIntr(oldIntr);
            pspSetK1(oldK1);
            return ret;
        }
    }
    subIntr->handler = (s32)handler;
    subIntr->gp = sceKernelGetModuleGPByAddressForKernel((u32)handler);
    subIntr->bits.subCount = 0;
    subIntr->arg = (s32)arg;
    subIntr->flags = (subIntr->flags & 0xFFFF7FFFu) | (intr->flags & 0x00008000u) | 0x80000300u;
    if (intr->cb != NULL && intr->cb->cbRegAfter != NULL) {
        s32 ret = intr->cb->cbRegAfter(intrNum, subIntrNum, handler, arg);
        if (ret != 0) {
            subIntr->handler = 0;
            sceKernelCpuResumeIntr(oldIntr);
            pspSetK1(oldK1);
            return ret;
        }
    }
    sceKernelCpuResumeIntr(oldIntr);
    pspSetK1(oldK1);
    return 0;
}

s32 sceKernelReleaseSubIntrHandler(s32 intrNum, s32 subIntrNum)
{
    dbg_printf("Called %s\n", __FUNCTION__);
    if ((u32)intrNum >= 68)
        return SCE_ERROR_KERNEL_INVALID_INTERRUPT;
    s32 oldK1 = pspShiftK1();
    s32 oldIntr = sceKernelCpuSuspendIntr();
    Interrupt *intr = &intInfo.intr[intrNum];
    if (intr->handler == 0) {
        sceKernelCpuResumeIntr(oldIntr);
        pspSetK1(oldK1);
        return SCE_ERROR_KERNEL_HANDLER_NOTFOUND;
    }
    if (subIntrNum < 0 || subIntrNum >= (s32)intr->bits.subCount) {
        sceKernelCpuResumeIntr(oldIntr);
        pspSetK1(oldK1);
        return SCE_ERROR_KERNEL_INVALID_INTERRUPT;
    }
    SubInterrupt *subIntr = &intr->subIntrs[subIntrNum];
    if (subIntr->handler == 0 || (subIntr->handler < 0 && pspK1IsUserMode())) {
        sceKernelCpuResumeIntr(oldIntr);
        pspSetK1(oldK1);
        return SCE_ERROR_KERNEL_HANDLER_NOTFOUND;
    }
    if (intr->cb != NULL && intr->cb->cbRelBefore != NULL) {
        s32 ret = intr->cb->cbRelBefore(intrNum, subIntrNum);
        if (ret != 0) {
            sceKernelCpuResumeIntr(oldIntr);
            pspSetK1(oldK1);
            return ret;
        }
    }
    s32 oldHandler = subIntr->handler;
    subIntr->handler = 0;
    if (intr->cb != NULL && intr->cb->cbRelAfter != NULL) {
        s32 ret = intr->cb->cbRelAfter(intrNum, subIntrNum);
        if (ret != 0) {
            subIntr->handler = oldHandler;
            sceKernelCpuResumeIntr(oldIntr);
            pspSetK1(oldK1);
            return ret;
        }
    }
    sceKernelCpuResumeIntr(oldIntr);
    pspSetK1(oldK1);
    return 0;
}

s32 sceKernelEnableSubIntr(s32 intrNum, s32 subIntrNum)
{
    dbg_printf("Called %s\n", __FUNCTION__);
    if ((u32)intrNum >= 68)
        return SCE_ERROR_KERNEL_INVALID_INTERRUPT;
    s32 oldK1 = pspShiftK1();
    s32 oldIntr = sceKernelCpuSuspendIntr();
    Interrupt *intr = &intInfo.intr[intrNum];
    if (subIntrNum < 0 || subIntrNum >= (s32)intr->bits.subCount
     || (pspK1IsUserMode() && (((intr->flags >> 11) & 1u) == 0
                              || ((intr->subIntrs[subIntrNum].flags >> 10) & 1u) == 0))) {
        sceKernelCpuResumeIntr(oldIntr);
        pspSetK1(oldK1);
        return SCE_ERROR_KERNEL_INVALID_INTERRUPT;
    }
    if (intr->handler == 0 || intr->cb == NULL || intr->cb->cbEnable == NULL) {
        sceKernelCpuResumeIntr(oldIntr);
        pspSetK1(oldK1);
        return SCE_ERROR_KERNEL_HANDLER_NOTFOUND;
    }
    s32 ret = intr->cb->cbEnable(intrNum, subIntrNum);
    sceKernelCpuResumeIntr(oldIntr);
    pspSetK1(oldK1);
    return ret;
}

s32 sceKernelDisableSubIntr(s32 intrNum, s32 subIntrNum)
{
    dbg_printf("Called %s\n", __FUNCTION__);
    if ((u32)intrNum >= 68)
        return SCE_ERROR_KERNEL_INVALID_INTERRUPT;
    s32 oldK1 = pspShiftK1();
    s32 oldIntr = sceKernelCpuSuspendIntr();
    Interrupt *intr = &intInfo.intr[intrNum];
    if (subIntrNum < 0 || subIntrNum >= (s32)intr->bits.subCount
     || (pspK1IsUserMode() && (((intr->flags >> 11) & 1u) == 0
                              || ((intr->subIntrs[subIntrNum].flags >> 10) & 1u) == 0))) {
        sceKernelCpuResumeIntr(oldIntr);
        pspSetK1(oldK1);
        return SCE_ERROR_KERNEL_INVALID_INTERRUPT;
    }
    if (intr->handler == 0 || intr->cb == NULL || intr->cb->cbDisable == NULL) {
        sceKernelCpuResumeIntr(oldIntr);
        pspSetK1(oldK1);
        return SCE_ERROR_KERNEL_HANDLER_NOTFOUND;
    }
    s32 ret = intr->cb->cbDisable(intrNum, subIntrNum);
    sceKernelCpuResumeIntr(oldIntr);
    pspSetK1(oldK1);
    return ret;
}

s32 sceKernelSuspendSubIntr(s32 intrNum, s32 subIntrNum, s32 *prevState)
{
    dbg_printf("Called %s\n", __FUNCTION__);
    if ((u32)intrNum >= 68)
        return SCE_ERROR_KERNEL_INVALID_INTERRUPT;
    s32 oldK1 = pspShiftK1();
    if (!pspK1PtrOk(prevState)) {
        pspSetK1(oldK1);
        return SCE_ERROR_KERNEL_ILLEGAL_ADDR;
    }
    s32 oldIntr = sceKernelCpuSuspendIntr();
    Interrupt *intr = &intInfo.intr[intrNum];
    if (subIntrNum < 0 || subIntrNum >= (s32)intr->bits.subCount
     || (pspK1IsUserMode() && (((intr->flags >> 11) & 1u) == 0
                              || ((intr->subIntrs[subIntrNum].flags >> 10) & 1u) == 0))) {
        sceKernelCpuResumeIntr(oldIntr);
        pspSetK1(oldK1);
        return SCE_ERROR_KERNEL_INVALID_INTERRUPT;
    }
    if (intr->handler == 0 || intr->cb == NULL || intr->cb->cbSuspend == NULL) {
        if (prevState != NULL)
            *prevState = 0;
        sceKernelCpuResumeIntr(oldIntr);
        pspSetK1(oldK1);
        return SCE_ERROR_KERNEL_HANDLER_NOTFOUND;
    }

    s32 ret = intr->cb->cbSuspend(intrNum, subIntrNum, prevState);
    sceKernelCpuResumeIntr(oldIntr);
    pspSetK1(oldK1);
    return ret;
}

s32 sceKernelResumeSubIntr(s32 intrNum, s32 subIntrNum, s32 prevState)
{
    dbg_printf("Called %s\n", __FUNCTION__);
    if ((u32)intrNum >= 68)
        return SCE_ERROR_KERNEL_INVALID_INTERRUPT;
    s32 oldK1 = pspShiftK1();
    s32 oldIntr = sceKernelCpuSuspendIntr();
    Interrupt *intr = &intInfo.intr[intrNum];
    if (subIntrNum < 0 || subIntrNum >= (s32)intr->bits.subCount
     || (pspK1IsUserMode() && (((intr->flags >> 11) & 1u) == 0
                              || ((intr->subIntrs[subIntrNum].flags >> 10) & 1u) == 0))) {
        sceKernelCpuResumeIntr(oldIntr);
        pspSetK1(oldK1);
        return SCE_ERROR_KERNEL_INVALID_INTERRUPT;
    }
    if (intr->handler == 0 || intr->cb == NULL || intr->cb->cbResume == NULL) {
        sceKernelCpuResumeIntr(oldIntr);
        pspSetK1(oldK1);
        return SCE_ERROR_KERNEL_HANDLER_NOTFOUND;
    }
    s32 ret = intr->cb->cbResume(intrNum, subIntrNum, prevState);
    sceKernelCpuResumeIntr(oldIntr);
    pspSetK1(oldK1);
    return ret;
}

s32 sceKernelIsSubInterruptOccurred(s32 intrNum, s32 subIntrNum)
{
    dbg_printf("Called %s\n", __FUNCTION__);
    if ((u32)intrNum >= 68)
        return SCE_ERROR_KERNEL_INVALID_INTERRUPT;
    s32 oldK1 = pspShiftK1();
    s32 oldIntr = sceKernelCpuSuspendIntr();
    Interrupt *intr = &intInfo.intr[intrNum];
    if (subIntrNum < 0 || subIntrNum >= (s32)intr->bits.subCount
     || (pspK1IsUserMode() && (((intr->flags >> 11) & 1u) == 0
                              || ((intr->subIntrs[subIntrNum].flags >> 10) & 1u) == 0))) {
        sceKernelCpuResumeIntr(oldIntr);
        pspSetK1(oldK1);
        return SCE_ERROR_KERNEL_INVALID_INTERRUPT;
    }
    if (intr->handler == 0 || intr->cb == NULL || intr->cb->cbIsOccured == NULL) {
        sceKernelCpuResumeIntr(oldIntr);
        pspSetK1(oldK1);
        return SCE_ERROR_KERNEL_HANDLER_NOTFOUND;
    }
    s32 ret = intr->cb->cbIsOccured(intrNum, subIntrNum);
    sceKernelCpuResumeIntr(oldIntr);
    pspSetK1(oldK1);
    return ret;
}

s32 sceKernelQueryIntrHandlerInfo(s32 intrNum, s32 subIntrNum, PspIntrHandlerOptionParam *out)
{
    dbg_printf("Called %s\n", __FUNCTION__);
    if ((u32)intrNum >= 68)
        return SCE_ERROR_KERNEL_INVALID_INTERRUPT;
    if (out->size != 56)
        return SCE_ERROR_KERNEL_INVALID_INTERRUPT_ARGUMENT;
    s32 oldIntr = sceKernelCpuSuspendIntr();
    Interrupt *intr = &intInfo.intr[intrNum];
    u8 numSubIntr = intr->bits.subCount;
    if (numSubIntr != 0) {
        if (subIntrNum < 0 || subIntrNum >= (s32)numSubIntr) {
            if (subIntrNum != -1) {
                sceKernelCpuResumeIntr(oldIntr);
                return SCE_ERROR_KERNEL_INVALID_INTERRUPT;
            }
        } else {
            if (intr->subIntrs == NULL)
                subIntrNum = 0;
            else
                intr = (Interrupt*)&intr->subIntrs[subIntrNum];
        }
    }
    out->common = intr->arg;
    out->entry = intr->handler;
    out->gp = intr->gp;
    out->intrCode = (s16)intrNum;
    out->intrLevel = (s16)((intr->flags >> 8) & 3u);
    if (intrNum >= 64) {
        s32 st = pspCop0StateGet(COP0_STATE_STATUS) & 0xFF00;
        s32 mask;
        if (intrNum == 64)
            mask = 0x0100;
        else if (intrNum == 65)
            mask = 0x0200;
        else if (intrNum == 66)
            mask = 0x1000;
        else if (intrNum == 67)
            mask = 0x8000;
        else
            mask = 0;
        out->enabled = (s16)((st & mask) != 0);
    } else {
        u8 bit = (u8)(((u32)intInfo.grpsOpt[((u32)intrNum >> 5) + 6] >> (intrNum & 0x1F)) & 1u);
        out->enabled = (s16)bit;
        if (bit != 0 && subIntrNum >= 0) {
            if (intr->handler != 0) {
                if (intr->cb != NULL && intr->cb->cbSuspend != NULL && intr->cb->cbResume != NULL) {
                    s32 savedState;
                    intr->cb->cbSuspend(intrNum, subIntrNum, &savedState);
                    intr->cb->cbResume(intrNum, subIntrNum, savedState);
                    out->enabled = (s16)(savedState != 0);
                }
            } else {
                out->enabled = 0;
            }
        }
    }
    out->totalClockLo = intr->totalClockLo;
    out->totalClockHi = intr->totalClockHi;
    out->subCount = (s16)intr->bits.subCount;
    out->minClockLo = intr->minClockLo;
    out->minClockHi = intr->minClockHi;
    out->calls = intr->callCount;
    out->maxClockLo = intr->maxClockLo;
    out->maxClockHi = intr->maxClockHi;
    sceKernelCpuResumeIntr(oldIntr);
    return 0;
}

void *mymemset(void *dstVoid, u8 fillByte, s32 count)
{
    dbg_printf("Called %s\n", __FUNCTION__);
    u8 *dst = (u8*)dstVoid;
    if (dst != NULL) {
        u8 *actDst = dst;
        while ((count--) > 0)
            *(actDst++) = fillByte;
    }
    return dst;
}

void InterruptDisableInTable(u32 intrNum)
{
    dbg_printf("Called %s\n", __FUNCTION__);
    s32 mask = ~(1 << (intrNum & 0x1F));
    s32 *ptr = &intInfo.grpsOpt[intrNum >> 5];
    s32 i = 3;
    while ((i--) >= 0) {
        *ptr &= mask;
        ptr += 2;
    }
}

void InterruptEnableInTable(u32 intrNum)
{
    dbg_printf("Called %s\n", __FUNCTION__);
    s32 val = (intInfo.intr[intrNum].flags >> 8) & 3u;
    s32 mask = 1 << (intrNum & 0x1F);
    s32 *ptr = &intInfo.grpsOpt[(intrNum >> 5) + (val << 1)];
    while ((val++) < 4) {
        *ptr |= mask;
        ptr += 2;
    }
    if (sceKernelIsIntrContext() == 0) {
        intcSetMask(&intInfo.grpsOpt[6]);
    }
    dbg_printf(" InterruptEnableInTable ok\n");
}

void AllLevelInterruptDisable(u32 intrNum)
{
    dbg_printf("Called %s\n", __FUNCTION__);
    s32 *ptr = &intInfo.grpsOpt[intrNum >> 5];
    s32 mask = ~(1 << (intrNum & 0x1F));
    s32 i = 11;
    while ((i--) >= 0) {
        *ptr &= mask;
        ptr += 2;
    }
    s32 maskBuf[2];
    intcGetMask(maskBuf);
    maskBuf[intrNum >> 5] &= mask;
    intcSetMask(maskBuf);
}

s32 sceKernelSetPrimarySyscallHandler(s32 syscallId, void (*syscall)())
{
    dbg_printf("Called %s\n", __FUNCTION__);
    s32 oldIntr = sceKernelCpuSuspendIntr();
    if (syscallId <= 0 || g_syscallTable.seed + g_syscallTable.funcTableSize < syscallId * 4) {
        sceKernelCpuResumeIntr(oldIntr);
        return SCE_ERROR_KERNEL_INVALID_SYSCALL_ID;
    }
    if (g_syscallTable.syscalls[syscallId] != g_syscallTable.syscalls[0]) {
        sceKernelCpuResumeIntr(oldIntr);
        return SCE_ERROR_KERNEL_SYSCALL_HANDLER_ALREADY_EXISTS;
    }
    if ((s32)syscall >= 0) {
        sceKernelCpuResumeIntr(oldIntr);
        return SCE_ERROR_KERNEL_ILLEGAL_ADDR;
    }
    g_syscallTable.syscalls[syscallId] = syscall;
    pspCache(0x1A, &g_syscallTable.syscalls[syscallId]);
    sceKernelCpuResumeIntr(oldIntr);
    return 0;
}

s32 IntrManTerminate(void *param __attribute__((unused)), s32 unusedSize __attribute__((unused)), s32 unusedFlag __attribute__((unused)), s32 unusedExtra __attribute__((unused)))
{
    dbg_printf("Called %s\n", __FUNCTION__);
    pspCop0StateSet(COP0_STATE_STATUS, pspCop0StateGet(COP0_STATE_STATUS) & 0xFFFF7BFF);
    sceKernelReleaseIntrHandler(67);
    sceKernelReleaseExceptionHandler(EXCEP_INT, (void*)intrExcepHandler);
    sceKernelReleaseExceptionHandler(EXCEP_INT, (void*)intrExcepHandler2);
    sceKernelReleaseExceptionHandler(EXCEP_SYS, (void*)syscallExcepHandler);
    return 0;
}

void sceKernelCpuEnableIntr(void)
{
    dbg_printf("Called %s\n", __FUNCTION__);
    asm("mtic %0, $0" : : "r" (1));
}

s32 InterruptManagerForKernel_6FCBA912(s32 set)
{
    dbg_printf("Called %s\n", __FUNCTION__);
    intInfo.defaultLoggingDisabled = (u8)(set == 0);
    return 0;
}

s32 sceKernelClearIntrLogging(s32 intrNum)
{
    dbg_printf("Called %s\n", __FUNCTION__);
    if ((u32)intrNum >= 68)
        return SCE_ERROR_KERNEL_INVALID_INTERRUPT;
    s32 oldIntr = sceKernelCpuSuspendIntr();
    Interrupt *intr = &intInfo.intr[intrNum];
    intr->minClockLo = (u32)-1;
    intr->minClockHi = (u32)-1;
    intr->maxClockLo = 0;
    intr->maxClockHi = 0;
    intr->totalClockLo = 0;
    intr->totalClockHi = 0;
    intr->callCount = 0;
    sceKernelCpuResumeIntr(oldIntr);
    return 0;
}

s32 sceKernelIsInterruptOccurred(s32 intrNum)
{
    dbg_printf("Called %s\n", __FUNCTION__);
    s32 pendingBuf[2];
    if ((u32)intrNum >= 64)
        return SCE_ERROR_KERNEL_INVALID_INTERRUPT;
    intcGetPending(pendingBuf);
    return ((u32)pendingBuf[(u32)intrNum >> 5] >> (intrNum & 0x1F)) & 1u;
}

s32 sceKernelDisableIntr(s32 intrNum)
{
    dbg_printf("Called %s\n", __FUNCTION__);
    return sceKernelSuspendIntr(intrNum, 0);
}

void RegisterSubIntrruptMonitor(MonitorCb before, MonitorCb after)
{
    dbg_printf("Called %s\n", __FUNCTION__);
    intInfo.monitorCbAfter = after;
    intInfo.monitorCbBefore = before;
}

void ReleaseSubIntrruptMonitor(void)
{
    dbg_printf("Called %s\n", __FUNCTION__);
    intInfo.monitorCbAfter = 0;
    intInfo.monitorCbBefore = 0;
}

s32 UnSupportIntr(s32 intrNum)
{
    dbg_printf("Called %s\n", __FUNCTION__);
    if ((u32)intrNum >= 64)
        return SCE_ERROR_KERNEL_INVALID_INTERRUPT;
    s32 oldIntr = sceKernelCpuSuspendIntr();
    sceKernelSuspendIntr(intrNum, 0);
    Interrupt *intr = &intInfo.intr[intrNum];
    intr->handler = 1;
    intr->flags &= 0xFFFFFCFFu;
    sceKernelCpuResumeIntr(oldIntr);
    return 0;
}

s32 InterruptManagerForKernel_8DFBD787(void)
{
    dbg_printf("Called %s\n", __FUNCTION__);
    if (intInfo.userIntrDepth != 0) {
        s32 *tcbPtr = (s32 *)pspCop0CtrlGet(COP0_CTRL_TCB);
        if (tcbPtr != NULL)
            return *tcbPtr;
    }
    return 0;
}

s32 QueryIntrHandlerInfoForUser(void)
{
    dbg_printf("Called %s\n", __FUNCTION__);
    return SCE_ERROR_KERNEL_ERROR;
}

s32 sceKernelRegisterUserSpaceIntrStack(s32 addr, s32 size, s32 stackFlagAddr)
{
    dbg_printf("Called %s\n", __FUNCTION__);
    if (size != 0x2000)
        return SCE_ERROR_INVALID_SIZE;
    s32 oldIntr = sceKernelCpuSuspendIntr();
    if (intInfo.intrStack != 0) {
        sceKernelCpuResumeIntr(oldIntr);
        return SCE_ERROR_KERNEL_STACK_ALREADY_SET;
    }
    if (addr < 0 || stackFlagAddr < 0) {
        sceKernelCpuResumeIntr(oldIntr);
        return SCE_ERROR_KERNEL_INVALID_STACK_ADDRESS;
    }
    intInfo.intrStackArg = stackFlagAddr;
    intInfo.intrStack = addr + 0x2000;
    sceKernelCpuResumeIntr(oldIntr);
    return 0;
}

s32 sceKernelGetCpuClockCounter(void)
{
    dbg_printf("Called %s\n", __FUNCTION__);
    return pspCop0StateGet(COP0_STATE_COUNT);
}

u64 sceKernelGetCpuClockCounterWide(void)
{
    dbg_printf("Called %s\n", __FUNCTION__);
    s32 oldIntr = sceKernelCpuSuspendIntr();
    u32 hi = intInfo.clockCounterHi;
    u32 count = pspCop0StateGet(COP0_STATE_COUNT);
    if (count >= intInfo.clockCounterLo)
        intInfo.clockCounterLo = count;
    else
        hi++;
    sceKernelCpuResumeIntr(oldIntr);
    return ((u64)hi << 32) | (u64)count;
}

u32 QueryInterruptManCB(void)
{
    dbg_printf("Called %s\n", __FUNCTION__);
    return intInfo.clockCounterLo;
}

u32 _sceKernelGetCpuClockCounterLow(void)
{
    dbg_printf("Called %s\n", __FUNCTION__);
    return intInfo.clockCounterHi;
}

s32 sceKernelRegisterSystemCallTable(SceSyscallTable *newMap)
{
    dbg_printf("Called %s\n", __FUNCTION__);
    if (newMap->next != NULL)
        return SCE_ERROR_KERNEL_SYSCALLTABLE_ALREADY_REGISTERED;
    if (newMap->funcTableSize < 0 || newMap->tableSize - 16 < newMap->funcTableSize)
        return SCE_ERROR_KERNEL_INVALID_SYSCALLTABLE;
    s32 oldIntr = sceKernelCpuSuspendIntr();
    SceSyscallTable *cur = g_syscallTable.next;
    SceSyscallTable *prev = &g_syscallTable;
    while (cur->seed != 0) {
        prev = cur;
        cur = cur->next;
    }
    newMap->next = cur;
    prev->next = newMap;
    pspCop0CtrlSet(COP0_CTRL_SC_TABLE, (s32)g_syscallTable.next);
    sceKernelCpuResumeIntr(oldIntr);
    return 0;
}

s32 sceKernelQuerySystemCall(void (*sysc)())
{
    SceSyscallTable *cur = &g_syscallTable;
    do {
        s32 count = (cur->tableSize - 16) / 4;
        s32 i;
        for (i = 0; i < count; i++) {
            void (*curSys)(void) = (void*)((s32)cur->syscalls[i] | 0x80000000);
            if (curSys == sysc) {
                cur->syscalls[i] = curSys;
                return cur->seed / 4 + i;
            }
        }
        cur = cur->next;
    } while (cur != NULL);
    return -1;
}

void InterruptManagerForKernel_E526B767(s32 mode)
{
    dbg_printf("Called %s\n", __FUNCTION__);
    if (mode == 0)
        syscallReturnScrubStartInsn = 0x42000018;
    if (mode == 1)
        syscallReturnScrubT3Insn = 0x00005821;
}

s32 SuspendIntc(s32 flag __attribute__((unused)), void *param __attribute__((unused)))
{
    dbg_printf("Called %s\n", __FUNCTION__);
    s32 maskBuf[2];
    intInfo.count = pspCop0StateGet(COP0_STATE_COUNT);
    intInfo.compare = pspCop0StateGet(COP0_STATE_COMPARE);
    intcGetMask(intInfo.intcState);
    mymemset(maskBuf, 0, 8);
    intcSetMask(maskBuf);
    return 0;
}

s32 ResumeIntc(s32 flag __attribute__((unused)), void *param __attribute__((unused)))
{
    dbg_printf("Called %s\n", __FUNCTION__);
    intcSetMask(intInfo.intcState);
    pspCop0StateSet(COP0_STATE_COMPARE, intInfo.compare);
    pspCop0StateSet(COP0_STATE_COUNT, intInfo.count);
    return 0;
}
