/*
 * SceKernelIntrMgr (os0:kd/intrmgr.skprx) driver interface, as used so far.
 */

#pragma once

#include <psp2kern/types.h>

typedef int (*SceKernelIntrHandler)(int intrCode, void *userCtx);

/* sceKernelRegisterIntrHandler's option (name unknown) */
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
 * Sub-interrupt callbacks, in the order IntrMgr calls them (name unknown; IntrMgr itself
 * reads them as an array of function pointers, SubIntrCallback).
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

int sceKernelRegisterIntrHandler(unsigned int intrCode, const char *name, unsigned int type,
				 void *handler, void *arg, unsigned int priority,
				 unsigned int target, const SceKernelIntrRegisterOpt *opt);
int sceKernelReleaseIntrHandler(unsigned int intrCode);
int sceKernelEnableIntr(unsigned int intrCode);
int sceKernelDisableIntr(unsigned int intrCode);
int sceKernelEnableSubIntr(int intrCode, int subCode);
int sceKernelDisableSubIntr(int intrCode, int subCode);
int sceKernelSetIntrTargetCpu(unsigned int intrCode, unsigned int target);
int SceIntrmgrForDriver_182EE3E3(void);	/* vitasdk: ksceKernelIsIntrAllowedInCurrentContext */
int sceKernelRegisterSubIntrHandler(unsigned int intrCode, unsigned int subCode, const char *name,
				    void *handler, void *arg);
int sceKernelReleaseSubIntrHandler(int intrCode, int subCode);
int sceKernelCallSubIntrHandler(int intrCode, int subCode, void *value);
int sceKernelGenerateSoftIntr(unsigned int intrCode, unsigned int filter, unsigned int target);
int sceKernelGetIntrPendingStatusForDebugger(int intrCode);
int sceKernelClearIntrPendingStatusForDebugger(int intrCode);

/* Name unknown: install the syscall entry and return trace hooks. */
void SceIntrmgrForKernel_217AC350(void *enter, void *leave);

void *SceIntrmgrForKernel_01911612(unsigned int cpu, unsigned int level);
void SceIntrmgrForKernel_7E3D4560(void *handler);
void *SceIntrmgrForKernel_807B2004(void);
void SceIntrmgrForKernel_ABEB5A69(void *handler);
int sceKernelAllocSystemCallTable(unsigned int count, void **table);	/* the 0.990 prototype names it */

void SceIntrmgrForKernel_04DBA732(void *enter, void *leave);
void SceIntrmgrForKernel_6F5B8AF2(void (*registered)(unsigned int), void (*released)(unsigned int));
void SceIntrmgrForKernel_8D6E1289(void *enter, void *leave);
int sceKernelGetIntrPriority(unsigned int intrCode, unsigned int *priority);
int sceKernelGetIntrTargetCpu(unsigned int intrCode, unsigned int *target);
int sceKernelRegisterIntrHookHandler(int intrCode, void *handler, void **oldHandler);
int sceKernelReleaseIntrHookHandler(unsigned int intrCode);
int sceKernelResumeIntr(unsigned int intrCode, unsigned int enabled);
int sceKernelSetIntrPriority(unsigned int intrCode, unsigned int priority);
int sceKernelSuspendIntr(unsigned int intrCode, unsigned int *enabled);
