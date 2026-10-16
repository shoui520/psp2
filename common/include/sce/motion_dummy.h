/* SceMotionDevDummy import additions and recovered timer interface. */
#pragma once
#include <psp2kern/types.h>
#include <sce/processmgr.h>

SceUID sceKernelGetProcessId(void);
SceUID ksceKernelRegisterTimer(const char *name, unsigned period,
			     int (*handler)(void *argp), void *argp);
SceUInt64 sceKernelGetSystemTimeWide(void);
int ksceSblACMgrIsSystemProgram(SceUID pid);
void *__memset_chk(void *dst, int value, unsigned size, unsigned dstSize);
