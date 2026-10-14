/*
 * SceProcessmgr (os0:kd/processmgr.skprx) driver interface additions to vitasdk.
 */

#pragma once

#include <psp2kern/types.h>

/* Processmgr supplies this layout to process event handlers. */
typedef struct ProcEventParam {
	SceSize size;
	SceUInt32 state;
	SceUInt32 kind;
	SceUInt32 type;
} ProcEventParam;

_Static_assert(sizeof(ProcEventParam) == 0x10, "Process event parameter ABI");

typedef struct ProcEventCreateParam {
	SceSize size;
	SceUID pid; /* Vitasdk's SceProcEventInvokeParam2; Processmgr initializes zero. */
	SceUInt32 kind;
	SceUID prevPid;
	SceUInt32 type;
} ProcEventCreateParam;

_Static_assert(sizeof(ProcEventCreateParam) == 0x14, "Process create event ABI");

/* names from SceSysmodule's error messages */
int SceProcessmgrForDriver_61B9B6FA(SceUID pid, SceUInt32 value);	/* sceKernelSetInitialPMCR */
int SceProcessmgrForDriver_B1C3EFCA(SceUID pid, SceUInt32 value);	/* sceKernelSetInitialPMUSERENR */

/* name unknown: the process's SDK version (SceSysmodule: >= 0x1800000 selects another library) */
int SceProcessmgrForDriver_D141C076(SceUID pid, SceUInt32 *sdkVersion);

/* name unknown: convert a system timestamp to the process's running time */
int SceProcessmgrForDriver_8CB01675(SceUID pid, SceUInt64 *time, SceUInt64 srcTime);

int sceKernelSuspendProcess(SceUID pid, SceUInt32 flags, SceUInt32 *timeout);

int sceKernelRegisterCoredumpHandler(void *handler, SceSize stackSize, void *arg);
int SceProcessmgrForDriver_33B3D026(void);
SceUID sceKernelCallCoredumpHandler(SceUID pid);

/* names unknown (os0:kd/deci4p_pamp.skprx) */
SceUID SceProcessmgrForKernel_9BC44974(SceUID pid);
int SceProcessmgrForKernel_EFFF2975(int level, void *buf, SceSize size, SceSize *count);	/* process list */

typedef struct SceKernelProcessResourceLimit {
	SceSize size;				/* 0x10 */
	SceUInt32 limit0;
	SceUInt32 limit1;
	SceUInt32 budgetType;
} SceKernelProcessResourceLimit;

int sceKernelGetProcessResourceLimit(SceUID pid, SceKernelProcessResourceLimit *limit);

/* the options of sceKernelCreateProcess */
typedef struct SceKernelProcessOpt {
	SceSize size;				/* 0x40 */
	SceUInt32 flags;
	SceUInt32 cpuAffinity;			/* flags 0x1000 */
	SceInt32 priority;
	SceSize stackSize;
	SceUInt32 reserved;
	SceUID budgetId;			/* flags 0x4000 */
	SceUID parentPid;			/* flags 0x10000 */
	SceUInt32 kind;				/* flags 0x20000 */
	SceUID prevPid;
	const void *klicensee;			/* flags 0x40000 */
	SceUInt32 limit0;			/* flags 0x80000 */
	SceUInt32 limit1;			/* flags 0x100000 */
	SceSize uniqueHeapSize;			/* flags 0x200000 */
	SceUInt32 resolutionFlags;		/* flags 0x800000: display's, SceProcessmgrForDriver_E1A67C86 */
	SceUInt32 powerConfigFlags;		/* flags 0x1000000: power's, SceProcessmgrForDriver_879153D9 */
} SceKernelProcessOpt;

_Static_assert(sizeof(SceKernelProcessOpt) == 0x40, "SceKernelProcessOpt size");
