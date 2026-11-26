/* Copyright (C) 2011, 2012, 2013 The uOFW team
   See the file COPYING for copying permission.
*/

#include "usersystemlib_int.h"
#include <threadman_user.h>

s32 sceKernelIsCpuIntrSuspended(s32 intr)
{
    return (intr == 0);
}

s32 sceKernelGetThreadId(void)
{
    if (g_thread == NULL) {
        return SCE_ERROR_KERNEL_CANNOT_BE_CALLED_FROM_INTERRUPT;
    }

    return g_thread->id;
}

s32 _sceKernelCheckThreadStack(void)
{
    s32 available;

    if (g_thread == NULL) {
        return sceKernelCheckThreadStack();
    }

    available = pspGetSp() - g_thread->stackBottom;

    if (available < 64) {
        return sceKernelCheckThreadStack();
    }

    return available;
}

void *sceKernelGetTlsAddr(SceUID uid)
{
    void *addr;
    s32 *threadCtx;
    s32 res;

    threadCtx = (s32 *)pspGetK0();

    s32 intr = pspMfic();

    if (threadCtx == NULL) {
        return NULL;
    }

    if (intr == 0) {
        return NULL;
    }

    if (threadCtx[49] != 0) {
        return NULL;
    }

    if (uid < 0) {
        return NULL;
    }

    addr = (void *)threadCtx[((uid >> 3) & 0xF) + 16];

    if (addr == NULL) {
        void *allocAddr;
        res = _sceKernelAllocateTlspl(uid, &allocAddr, 0);

        if (res < 0) {
            return NULL;
        }
        addr = allocAddr;
    }

    return addr;
}
