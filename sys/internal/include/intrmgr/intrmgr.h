/*
 * SceKernelIntrMgr (os0:kd/intrmgr.skprx) driver interface, as used so far.
 */

#pragma once

#include <stdint.h>
#include <scetypes.h>

typedef int (*SceKernelIntrHandler)(int intrCode, void *userCtx);

/* sceKernelRegisterIntrHandler's option (name unknown) */
typedef struct SceKernelIntrRegisterOpt {
	SceSize size;				/* sizeof(SceKernelIntrRegisterOpt) */
	SceUInt32 num;				/* the sub-interrupt handler slots to allocate */
	const void *handlers;			/* the sub-interrupt callbacks */
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
int sceKernelSuspendSubIntr(int intrCode, int subCode, unsigned int *enabled);
int sceKernelResumeSubIntr(int intrCode, int subCode, unsigned int stateValue);
int sceKernelIsSubInterruptOccurred(int intrCode, int subCode);

/* sceKernelQueryIntrHandlerInfo's output */
typedef struct SceKernelIntrHandlerInfo {
	SceSize size;
	void *handler;
	void *arg;
	SceUInt16 intrCode;
	SceUInt16 type;
	SceUInt16 subCount;
	SceUInt16 priority;
	SceUInt16 targetCpu;
	SceUInt16 enabled;
	SceUInt32 count[4];
	SceUInt32 reserved0[4];		/* always 0 */
	SceUInt32 reserved1[4];
	SceUInt32 reserved2[8];
	char *name;
	SceUInt32 typeFlag;		/* 0x10: the handler's type has 0x10 */
} SceKernelIntrHandlerInfo;

int sceKernelQueryIntrHandlerInfo(unsigned int intrCode, unsigned int subCode,
				  SceKernelIntrHandlerInfo *out);

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

/* intrmgr.c: the thread manager's event hook, and an event (0x100000) carrying a value */
void SceIntrmgrForKernel_E3B670BF(int (*event)(unsigned int, unsigned int, void *));
int SceIntrmgrForDriver_C568F145(unsigned int value);

/* intrmgr_stack.c: the thread manager's hooks */
struct ThreadMgrCpuCB;				/* threadmgr's per-CPU control block */
void SceIntrmgrForKernel_BCD3BB05(void);
void SceIntrmgrForKernel_9CB39268(void *handler);	/* before an interrupt handler */
void SceIntrmgrForKernel_46F04D82(void *handler);
void SceIntrmgrForKernel_764F140A(void *handler);	/* the next context after an interrupt */
void SceIntrmgrForKernel_04E635AA(struct ThreadMgrCpuCB *const *cpu);	/* one per CPU */

/* intrmgr_copy.c: copies 0x200 bytes, returning the end of the copy */
void *SceIntrmgrForKernel_71E08D71(void *destination, const void *source);

/*
 * intrmgr_cpu.S: a context saved like setjmp (0 when saved, nonzero when it is resumed), and
 * resumed like longjmp.
 */
struct ThreadRegisterContext;			/* threadmgr/threadcb.h */
int SceIntrmgrForKernel_71C5F42F(struct ThreadRegisterContext *context, uintptr_t vfp)
	__attribute__((returns_twice));
uint32_t SceIntrmgrForKernel_08FDB0CE(struct ThreadRegisterContext *context, uint32_t flags,
				      uintptr_t vfp);
