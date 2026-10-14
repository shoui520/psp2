/*
 * SceKernelIntrMgr (os0:kd/intrmgr.skprx) driver interface, as used so far.
 */

#pragma once

#include <psp2kern/types.h>

typedef int (*SceKernelIntrHandler)(int intrCode, void *userCtx);

/*
 * ksceKernelRegisterIntrHandler's option (vitasdk's SceKernelIntrOptParam;
 * our name, so both headers can be included).
 */
typedef struct SceKernelIntrRegisterOpt {
	SceSize size;				/* sizeof(SceKernelIntrRegisterOpt) */
	SceUInt32 num;				/* the sub-interrupt handler slots to allocate */
	void *handlers;				/* the sub-interrupt callbacks */
	SceUInt32 attr;				/* SCE_KERNEL_INTR_OPT_HAS_CONFIG */
	SceUInt32 config;			/* SCE_KERNEL_INTR_CONFIG_* */
} SceKernelIntrRegisterOpt;

#define SCE_KERNEL_INTR_OPT_HAS_CONFIG	0x1
#define SCE_KERNEL_INTR_CONFIG_LEVEL	1	/* the GIC trigger configuration */
#define SCE_KERNEL_INTR_CONFIG_EDGE	3
typedef int (*SceKernelSubIntrHandler)(void *subintrArg, void *registerArg, unsigned char intrPriority);

/*
 * Sub-interrupt callbacks, in the order consumed by IntrMgr. The SDK's
 * SceKernelIntrOptHandlers uses pointer-to-pointer members; keep this typed
 * view separate so callers can also include the SDK header.
 */
typedef struct SceKernelIntrCallbacks {
	SceSize size;
	int (*preRegister)(int intrCode, int subintrCode,
			   SceKernelSubIntrHandler handler, void *registerArg);
	int (*postRegister)(int intrCode, int subintrCode,
			    SceKernelSubIntrHandler handler, void *registerArg);
	int (*preRelease)(int intrCode, int subintrCode);
	int (*postRelease)(int intrCode, int subintrCode);
	int (*enable)(int intrCode, int subintrCode);
	int (*disable)(int intrCode, int subintrCode);
	int (*suspend)(int intrCode, int subintrCode, unsigned int *enabled);
	int (*resume)(int intrCode, int subintrCode, unsigned int stateValue);
	int (*isOccurred)(int intrCode, int subintrCode);
} SceKernelIntrCallbacks;

int ksceKernelRegisterIntrHandler(int intrCode, const char *name, int type,
				  SceKernelIntrHandler handler, void *userCtx,
				  int priority, int targetCpu, void *opt);
int ksceKernelReleaseIntrHandler(int intrCode);
int ksceKernelEnableIntr(int intrCode);
int ksceKernelDisableIntr(int intrCode);
int ksceKernelEnableSubIntr(int intrCode, int subintrCode);
int ksceKernelDisableSubIntr(int intrCode, int subintrCode);
int ksceKernelSetIntrTarget(int intrCode, int targetCpu);
int ksceKernelIsIntrAllowedInCurrentContext(void);
int ksceKernelRegisterSubIntrHandler(int intrCode, int subintrCode, const char *name,
				     SceKernelSubIntrHandler handler, void *registerArg);
int ksceKernelReleaseSubIntrHandler(int intrCode, int subintrCode);
int ksceKernelTriggerSGI(int intrCode, unsigned int targetListFilter, unsigned int cpuTargetList);
int sceKernelGetIntrPendingStatusForDebugger(int intrCode);
int sceKernelClearIntrPendingStatusForDebugger(int intrCode);

/* Name unknown: install the syscall entry and return trace hooks. */
void SceIntrmgrForKernel_217AC350(void *enter, void *leave);
