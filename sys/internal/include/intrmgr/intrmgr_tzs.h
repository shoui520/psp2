/*
 * SceIntrmgrForTZS: the secure-world interrupt manager (slb2:kbl/
 * SceKernelIntrMgr) as its client modules call it.
 */

#pragma once

#include <psp2kern/types.h>

typedef int (*SceKernelIntrHandlerTzs)(int intrCode, void *userCtx);
/* an SMC service of the secure monitor: the caller's r0-r3 */
typedef int (*SceKernelMonitorCallHandler)(int arg0, int arg1, int arg2, int arg3);

int sceKernelRegisterIntrHandler(unsigned int intrCode, const char *name, unsigned int type,
				 SceKernelIntrHandlerTzs handler, void *userCtx,
				 unsigned int priority, unsigned int targetCpu,
				 const struct SceKernelIntrRegisterOpt *opt);
int sceKernelReleaseIntrHandler(unsigned int intrCode);
int sceKernelEnableIntr(unsigned int intrCode);
int sceKernelDisableIntr(unsigned int intrCode);
int sceKernelGenerateSoftIntr(unsigned int intrCode, unsigned int filter, unsigned int target);
int sceKernelRegisterMonitorCallHandler(unsigned int callId, SceKernelMonitorCallHandler handler);
void sceKernelUsleep(SceUInt32 usec);

int sceKernelSetIntrTargetCpu(unsigned int intrCode, unsigned int target);
