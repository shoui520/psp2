/*
 * SceIntrmgrForTZS: the secure-world interrupt manager (slb2:kbl/
 * SceKernelIntrMgr) as its client modules call it.
 */

#pragma once

#include <psp2kern/types.h>

/* an SMC service of the secure monitor: the caller's r0-r3 */
typedef int (*SceKernelMonitorCallHandler)(int arg0, int arg1, int arg2, int arg3);

struct SceKernelIntrRegisterOpt;			/* intrmgr/intrmgr.h */

/* the same prototypes as the non-secure kernel's (intrmgr/intrmgr.h) */
int sceKernelRegisterIntrHandler(unsigned int intrCode, const char *name, unsigned int type,
				 void *handler, void *arg, unsigned int priority,
				 unsigned int target, const struct SceKernelIntrRegisterOpt *opt);
int sceKernelReleaseIntrHandler(unsigned int intrCode);
int sceKernelEnableIntr(unsigned int intrCode);
int sceKernelDisableIntr(unsigned int intrCode);
int sceKernelGenerateSoftIntr(unsigned int intrCode, unsigned int filter, unsigned int target);
int sceKernelRegisterMonitorCallHandler(unsigned int callId, SceKernelMonitorCallHandler handler);
void sceKernelUsleep(SceUInt32 usec);

int sceKernelSetIntrTargetCpu(unsigned int intrCode, unsigned int target);
