/* Copyright (C) 2011, 2012 The uOFW team
   See the file COPYING for copying permission.
*/

#include <common_imp.h>

#include "excep.h"
#include "exceptions.h"
#include "intr.h"
#include "nmi.h"

#include "exceptionman.h"

/*
 * Registered exception handlers begin with an 8-byte chaining header:
 * word 0 stores the pointer to the next handler entry (at header + 8),
 * and the handler code itself starts immediately after the header.
 */
typedef struct SceExceptionHandlerStub
{
    void *nextEntry;
    u32 reservedWord;
} SceExceptionHandlerStub;

typedef struct
{
    void *activeHandlers[32];
    SceExceptionHandler *registeredHandlers[32];
    void *defaultHdlr;
    SceExceptionHandler *curPool;
    SceExceptionHandler pool[32];
} SceExceptions;

SCE_MODULE_INFO("sceExceptionManager", SCE_MODULE_KERNEL | SCE_MODULE_ATTR_CANT_STOP | SCE_MODULE_ATTR_EXCLUSIVE_LOAD
                                       | SCE_MODULE_ATTR_EXCLUSIVE_START, 1, 3);
SCE_MODULE_BOOTSTART("ExcepManInit");
SCE_SDK_VERSION(SDK_VERSION);

int g_nmiActive = 0;
int g_activeNmiHandler = 0;

char g_stack[2104] = {0};
char g_stackCtx[192] = {0};

SceExceptions ExcepManCB = { { NULL }, { NULL }, NULL, NULL, { { NULL, NULL } }};

int ExcepManInit(SceSize argSize __attribute__((unused)), const void *argBlock __attribute__((unused)))
{
    dbg_init(1, FB_HARDWARE, FAT_HARDWARE);
    dbg_printf("-- ExcepManInit()\n");
    int oldIntr = suspendIntr();
    int i;
    for (i = 0; i < 32; i++)
        ExcepManCB.registeredHandlers[i] = NULL;
    ExcepManCB.defaultHdlr = NULL;
    Allocexceppool();
    pspCop0CtrlSet(25, 0);
    initExceptionVectors();
    int cpuid = pspCop0StateGet(COP0_STATE_CPUID);
    int *src, *srcEnd, *dst;
    if (cpuid == 1)
    {
        src = (int*)&nmiExceptionJumpStub;
        dst = (int*)(0xBFC00000 + (int)ExceptionManagerForKernel_79454858 - (int)resetExceptionStub);
        srcEnd = (int*)&nmiExceptionJumpStubEnd;
    }
    else
    {
        dst = (int*)(0xBFC00000);
        src = (int*)&resetExceptionStub;
        srcEnd = (int*)&ExceptionManagerForKernel_79454858;
    }
    while (src != srcEnd)
        *(dst++) = *(src++);
    sceKernelRegisterDefaultExceptionHandler(defaultExceptionSpinLoop);
    NmiManInit();
    dbg_printf("-- init ended\n");
    resumeIntr(oldIntr);
    return 0;
}

int sceKernelRegisterPriorityExceptionHandler(int exno, int prio, void (*func)())
{
    dbg_printf("sceKernelRegisterPriorityExceptionHandler(ex = %d, prio = %d, %08x)\n", exno, prio, func);
    int oldIntr = suspendIntr();
    SceExceptionHandlerStub *stub = (SceExceptionHandlerStub *)func;
    if (stub->nextEntry != NULL)
    {
        resumeIntr(oldIntr);
        dbg_printf("(ERROR)\n");
        return 0x80020034;
    }
    if (exno < 0 || exno >= 32)
    {
        resumeIntr(oldIntr);
        dbg_printf("(ERROR)\n");
        return 0x80020032;
    }
    prio &= 0x3;
    SceExceptionHandler *newEx = newExcepCB();
    SceExceptionHandler **lastEx = &ExcepManCB.registeredHandlers[exno];
    SceExceptionHandler *ex = *lastEx;
    newEx->cb = (void*)(((int)func & 0xFFFFFFFC) | prio);
    while (ex != NULL)
    {
        if (((int)ex->cb & 3) >= prio)
            break;
        lastEx = &ex->next;
        ex = ex->next;
    }
    newEx->next = ex;
    *lastEx = newEx;
    build_exectbl();
    resumeIntr(oldIntr);
    dbg_printf("(end)\n");
    return 0;
}

int sceKernelRegisterDefaultExceptionHandler(void *func)
{
    dbg_printf("sceKernelRegisterDefaultExceptionHandler(%08x)\n", func);
    int oldIntr = suspendIntr();
    SceExceptionHandlerStub *stub = (SceExceptionHandlerStub *)func;
    if (stub->nextEntry != NULL || (func == defaultExceptionSpinLoop && ExcepManCB.defaultHdlr != NULL))
    {
        resumeIntr(oldIntr);
        dbg_printf("(ERROR)\n");
        return 0x80020034;
    }
    stub->nextEntry = ExcepManCB.defaultHdlr;
    ExcepManCB.defaultHdlr = stub + 1;
    build_exectbl();
    resumeIntr(oldIntr);
    dbg_printf("(end)\n");
    return 0;
}

int sceKernelReleaseExceptionHandler(int exno, void (*func)())
{
    dbg_printf("exec %s\n", __FUNCTION__);
    int oldIntr = suspendIntr();
    if (exno < 0 || exno >= 32)
    {
        resumeIntr(oldIntr);
        dbg_printf("(ERROR)\n");
        return 0x80020032;
    }
    SceExceptionHandler **lastEx = &ExcepManCB.registeredHandlers[exno];
    SceExceptionHandler *ex = *lastEx;
    while (ex != NULL)
    {
        SceExceptionHandlerStub *cb = (SceExceptionHandlerStub *)((int)ex->cb & 0xFFFFFFFC);
        if ((void *)cb == (void *)func)
        {
            cb->nextEntry = NULL;
            *lastEx = ex->next;
            FreeExcepCB(ex);
            build_exectbl();
            resumeIntr(oldIntr);
            dbg_printf("(end)\n");
            return 0;
        }
        lastEx = &ex->next;
        ex = ex->next;
    }
    resumeIntr(oldIntr);
    dbg_printf("(ERROR)\n");
    return 0x80020033;
}

int sceKernelReleaseDefaultExceptionHandler(void (*func)())
{
    dbg_printf("exec %s\n", __FUNCTION__);
    int oldIntr = suspendIntr();
    SceExceptionHandlerStub *releaseEx = (SceExceptionHandlerStub *)func;
    void *actualFunc = releaseEx + 1;
    if (func != defaultExceptionSpinLoop && ExcepManCB.defaultHdlr != NULL)
    {
        void **cur = &ExcepManCB.defaultHdlr;
        while (*cur != NULL)
        {
            if (*cur == actualFunc)
            {
                *cur = releaseEx->nextEntry;
                releaseEx->nextEntry = NULL;
                build_exectbl();
                resumeIntr(oldIntr);
                dbg_printf("(end)\n");
                return 0;
            }
            cur = &(((SceExceptionHandlerStub *)(*cur)) - 1)->nextEntry;
        }
    }
    resumeIntr(oldIntr);
    dbg_printf("(ERROR)\n");
    return 0x80020033;
}

void build_exectbl(void)
{
    dbg_printf("exec %s\n", __FUNCTION__);
    int i;
    for (i = 0; i < 32; i++)
    {
        SceExceptionHandler *hdlr = ExcepManCB.registeredHandlers[i];
        if (hdlr != NULL)
        {
            dbg_printf("== listing handlers for excep %d ==\n", i);
            while (hdlr->next != NULL) {
                SceExceptionHandlerStub *curStub = (SceExceptionHandlerStub *)((int)hdlr->cb & 0xFFFFFFFC);
                SceExceptionHandlerStub *nextStub = (SceExceptionHandlerStub *)((int)hdlr->next->cb & 0xFFFFFFFC);
                dbg_printf("cb %08x, prio %d\n", (int)hdlr->cb & 0xFFFFFFFC, (int)hdlr->cb & 3);
                curStub->nextEntry = nextStub + 1;
                hdlr = hdlr->next;
            }
            dbg_printf("cb %08x, prio %d\n", (int)hdlr->cb & 0xFFFFFFFC, (int)hdlr->cb & 3);
            dbg_printf("== done ==\n");
            SceExceptionHandlerStub *tailStub = (SceExceptionHandlerStub *)((int)hdlr->cb & 0xFFFFFFFC);
            tailStub->nextEntry = ExcepManCB.defaultHdlr;
        }
    }
    u32 op = syscallHandler;
    for (i = 0; i < 32; i++)
    {
        SceExceptionHandler *regEntry = ExcepManCB.registeredHandlers[i];
        void *activeEntry;
        if (regEntry != NULL) {
            SceExceptionHandlerStub *regStub = (SceExceptionHandlerStub *)((int)regEntry->cb & 0xFFFFFFFC);
            activeEntry = regStub + 1;
        }
        else
            activeEntry = ExcepManCB.defaultHdlr;

        ExcepManCB.activeHandlers[i] = activeEntry;
        if (i == 8)
        {
            op = (((int)ExcepManCB.activeHandlers[8] >> 2) & 0x03FFFFFF) + 0x08000000;
            pspCache(0x1A, &syscallHandler);
        }
    }
    syscallHandler = op;
}

SceExceptionHandler *newExcepCB(void)
{
    dbg_printf("exec %s\n", __FUNCTION__);
    SceExceptionHandler *excep = ExcepManCB.curPool;
    if (excep != NULL)
        ExcepManCB.curPool = ExcepManCB.curPool->next;
    return excep;
}

void FreeExcepCB(SceExceptionHandler *ex)
{
    dbg_printf("exec %s\n", __FUNCTION__);
    ex->next = ExcepManCB.curPool;
    ExcepManCB.curPool = ex;
}

void Allocexceppool(void)
{
    ExcepManCB.curPool = &ExcepManCB.pool[0];
    dbg_printf("Allocexceppool()\n");
    int i;
    for (i = 0; i < 31; i++)
        ExcepManCB.pool[i].next = &ExcepManCB.pool[i + 1];
    ExcepManCB.pool[31].next = NULL;
}

int sceKernelRegisterExceptionHandler(int exno, void (*func)())
{
    dbg_printf("exec %s\n", __FUNCTION__);
    return sceKernelRegisterPriorityExceptionHandler(exno, 2, func);
}

void *sceKernelGetActiveDefaultExceptionHandler(void)
{
    dbg_printf("exec %s\n", __FUNCTION__);
    if (ExcepManCB.defaultHdlr == (void *)defaultExceptionSpinLoop)
        return NULL;
    return ExcepManCB.defaultHdlr;
}
