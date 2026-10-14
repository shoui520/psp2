/*
 * SceIntrmgrForTZS: the secure-world interrupt manager (slb2:kbl/
 * SceKernelIntrMgr) as its client modules call it.
 */

#pragma once

#include <psp2kern/types.h>

typedef int (*SceKernelIntrHandlerTzs)(int intrCode, void *userCtx);
/* an SMC service of the secure monitor: the caller's r0-r3 */
typedef int (*SceKernelMonitorCallHandler)(int arg0, int arg1, int arg2, int arg3);

int sceKernelRegisterIntrHandler(int intrCode, const char *name, int type,
				 SceKernelIntrHandlerTzs handler, void *userCtx,
				 int priority, int targetCpu, void *opt);
int sceKernelEnableIntr(int intrCode);
int sceKernelRegisterMonitorCallHandler(int callId, SceKernelMonitorCallHandler handler);
