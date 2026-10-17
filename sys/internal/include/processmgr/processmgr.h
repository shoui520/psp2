/*
 * SceProcessmgr (os0:kd/processmgr.skprx) driver interface additions to vitasdk.
 */

#pragma once

#include <psp2kern/types.h>

/* the process event parameters processmgr passes to the handlers: sysmem/procevent.h */
#include <sysmem/procevent.h>

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

int SceProcessmgrForDriver_2CEB1C7A(SceUID pid, SceUInt32 *buf, SceSize size);	/* random bytes */
int SceProcessmgrForDriver_54D469E2(SceUInt32 *flag);
int SceProcessmgrForDriver_5E882B60(void (*callback)(SceUID pid, int exitStatus));
int SceProcessmgrForDriver_879153D9(SceUID pid, SceUInt32 kind);
int SceProcessmgrForDriver_A1CF74FA(SceUID pid);
int SceProcessmgrForDriver_BA0A06A4(int level, void *buf, SceSize size, SceSize *count);
int SceProcessmgrForDriver_C715591F(void);	/* a process refuses suspend */
int SceProcessmgrForDriver_E1A67C86(SceUID pid, SceUInt32 *value);
int SceProcessmgrForDriver_FC2A424E(void *ptr);
int SceProcessmgrForKernel_8321B827(SceUInt32 *timeout);
int SceProcessmgrForKernel_BC110987(SceUID pid, SceUInt32 event);
int SceProcessmgrForKernel_D5DF895D(void);
int SceProcessmgrForKernel_F160B4D8(SceUID pid, int kind);	/* release the slot */
SceUInt32 ksceKernelGetProcessTimeLowCore(void);
SceUInt64 ksceKernelGetProcessTimeWideCore(void);
void *sceKernelAllocRemoteProcessHeap(SceUID pid, SceSize size, void *opt);
void *sceKernelAllocRemoteToolHeap(SceUID pid, SceSize size);
int sceKernelGetProcessType(SceUID pid);
int sceKernelKillProcess(SceUID pid, int kind);
int sceKernelResumeProcess(SceUID pid);
int sceKernelSetPMUSERENR(SceUID pid, SceUInt32 value);
int sceKernelSetProcessLibdb(SceUID pid, void *libdb);
int sceKernelSetProcessStatus(SceUID pid, SceUInt32 status);
int sceKernelSetSyscallTraceFlag(SceUID pid, SceUInt32 flag);
SceUID sceKernelSpawnProcessExt(const char *name, SceUInt32 type, const char *path, SceSize args,
				 void *argp, const SceKernelProcessOpt *opt, SceUInt32 flags);
int sceKernelStartProcessExt(SceUID pid, int waitDebugger, SceSize args, void *argp, SceUInt32 flags);
int sceKernelWaitProcessEnd(SceUID pid, int *exitStatus, SceUInt32 *timeout);
int sceKernelWaitProcessEventCompletion(SceUID pid, SceUInt32 event, SceUInt32 *timeout);
