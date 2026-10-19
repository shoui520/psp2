/*
 * SceProcessmgr (os0:kd/processmgr.skprx): the process manager's exports, each declared once
 * with its definition as the prototype.
 */

#pragma once

#include <scetypes.h>
#include <authmgr/sblauthmgr.h>

/* the process event parameters processmgr passes to the handlers: sysmem/procevent.h */
#include <sysmem/procevent.h>
#include <sysmem/uid_class.h>
#include <processmgr/process_info.h>
#include <processmgr/process_budget.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct SceProcessObject SceProcessObject;	/* processmgr/process.h */

/* the name of a process local storage key */
typedef struct ProcessLocalStorageName {
	char name[0x20];
} ProcessLocalStorageName;

/* name unknown: a process local storage key's name and size */
typedef struct ProcessLocalStorageInfo {
	SceSize size;				/* 0x28 */
	ProcessLocalStorageName name;
	SceSize storageSize;
} ProcessLocalStorageInfo;

/* the user mode addresses LibKernel registers */
typedef struct LibkernelAddresses {
	SceSize size;				/* 0x1c */
	void *addr[6];
} LibkernelAddresses;

typedef SceUInt32 SceKernelTime;		/* the user mode C library's time_t */

/* the user mode struct timeval / struct timezone */
typedef struct ProcessTimeval {
	SceInt32 sec;
	SceInt32 usec;
} ProcessTimeval;

typedef struct ProcessTimezone {
	SceInt32 minuteswest;
	SceInt32 dsttime;
} ProcessTimezone;

/* the user mode struct tm */
typedef struct ProcessTm {
	SceInt32 field[9];
} ProcessTm;

/* the kernel libc time implementation (SceProcessmgrForDriver_4140D633); rtc's RtcLibcTimeFunctions */
typedef struct ProcessmgrLibcOps {
	SceSize size;
	SceUInt32 (*clock)(void);
	SceKernelTime (*time)(SceKernelTime *t);
	int (*gettimeofday)(void *tv, void *tz);
	int (*gmtime_r)(const SceKernelTime *t, void *tm);
	int (*localtime_r)(const SceKernelTime *t, void *tm);
	int (*mktime)(void *tm, SceKernelTime *t, SceUInt64 *wide);
	int (*getCurrentTick)(void *a, void *b, void *c);
	int (*getCurrentSecureTick)(void *a, void *b, void *c);
} ProcessmgrLibcOps;

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
/* vitasdk: ksceKernelGetProcessTimeLowCore */
SceUInt32 SceProcessmgrForDriver_02179E12(void);
/* vitasdk: ksceKernelGetProcessTimeWideCore */
SceUInt64 SceProcessmgrForDriver_82D94BE9(void);
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

int SceProcessmgrForDriver_11BEFBBE(void *ptr);
int SceProcessmgrForDriver_1C70501E(SceUID pid);
int sceKernelRegisterKPLS(const char *name, SceSize size);
void *SceProcessmgrForDriver_3B33FFBA(SceSize size, void *opt);
int SceProcessmgrForDriver_4140D633(const ProcessmgrLibcOps *ops);
void *sceKernelAllocOwnProcessHeap(SceSize size, void *opt);
int SceProcessmgrForDriver_594319C7(SceUID pid);
int SceProcessmgrForDriver_5BDA978F(SceUID pid, SceUInt64 *time);
/* vitasdk: ksceKernelGetProcessLocalStorageAddrForPid */
int SceProcessmgrForDriver_AF80F39C(SceUID pid, SceUInt32 key, void **addr, int create);
/* vitasdk: ksceKernelGetRemoteProcessTime */
int SceProcessmgrForDriver_C074EB31(SceUID pid, SceUInt64 *time);
int SceProcessmgrForDriver_C5CF15ED(SceUInt32 key, ProcessLocalStorageInfo *info);
int SceProcessmgrForDriver_E8005AFC(int level, void *buf, SceSize size, SceSize *count);
int SceProcessmgrForDriver_EC283166(SceUInt64 *time);	/* vitasdk: ksceKernelGetProcessTimeCore */
/* vitasdk: ksceKernelGetProcessLocalStorageAddr */
void *SceProcessmgrForDriver_EE694840(SceUInt32 key);
/* vitasdk: ksceKernelLibcGettimeofday */
int SceProcessmgrForKernel_014F5113(void *tv, void *tz);
int SceProcessmgrForKernel_1197DB8A(SceUInt32 index, SceUInt32 value, SceUInt32 control);
SceUID SceProcessmgrForKernel_1E5B38E5(const char *name, SceUInt32 type, const char *path,
				       SceSize args, void *argp, const SceKernelProcessOpt *opt);
void SceProcessmgrForKernel_344DDE0B(SceUID pid, SceUID prevPid);
int SceProcessmgrForKernel_3EBC93BB(void *a, void *b, void *c);
int SceProcessmgrForKernel_459A223B(SceUID budgetId);
int SceProcessmgrForKernel_5137B783(SceUID pid, int waitDebugger, SceSize args, void *argp);
int SceProcessmgrForKernel_6DEC45C3(SceUInt32 index, SceUInt32 *value, SceUInt32 *control);
int SceProcessmgrForKernel_8B4086AF(SceUID pid, int kind);	/* reserve a module slot */
void *SceProcessmgrForKernel_923AF3CC(int kind);
SceClass *SceProcessmgrForKernel_98AE4BC8(void);	/* vitasdk: ksceKernelGetUIDProcessClass */
int SceProcessmgrForKernel_9FA66773(SceUInt32 index, SceUInt32 *value, SceUInt32 *control);
/* vitasdk: ksceKernelLibcTime */
SceKernelTime SceProcessmgrForKernel_B0E076D6(SceKernelTime *t);
int SceProcessmgrForKernel_D231AE5B(void *a, void *b, void *c);
SceUID SceProcessmgrForKernel_DC18FCEF(const char *name, SceUInt32 type,
				       const ProcessBudgetOpt *opt);
int SceProcessmgrForKernel_DDE282CF(SceUInt32 index, SceUInt32 value, SceUInt32 control);
int SceProcessmgrForKernel_E025F5A9(SceUID pid, int index, SceUInt32 *value, SceUInt32 *control);
int SceProcessmgrForKernel_FC80A606(SceUID pid, int index, SceUInt32 value, SceUInt32 control);
void *sceKernelAllocRemoteCDialogHeap(SceUID pid, SceSize size, void *opt);
int sceKernelChangeProcessCpuAffinity(SceUID pid, SceUInt32 cpuAffinity);
SceUID sceKernelCreateProcess(const char *name, SceUInt32 type, const char *path,
			      const SceKernelProcessOpt *opt);
int sceKernelDisablePHWP(SceUID pid, SceUInt32 index);
int sceKernelEnablePHWP(SceUID pid, SceUInt32 index);
int sceKernelExitProcess(int status);
int sceKernelFreeRemoteCDialogHeap(SceUID pid, void *ptr);
int sceKernelFreeRemoteProcessHeap(SceUID pid, void *ptr);
int sceKernelFreeRemoteToolHeap(SceUID pid, void *ptr);
int sceKernelGetBudgetInfo(SceUID budgetId, SceKernelBudgetInfo *info);
int sceKernelGetPHBP(SceUID pid, SceUInt32 index, SceUInt32 *value, SceUInt32 *control);
int sceKernelGetPHWP(SceUID pid, SceUInt32 index, SceUInt32 *value, SceUInt32 *control);
SceUID sceKernelGetProcessApp(SceUID pid);
int sceKernelGetProcessInfo(SceUID pid, SceKernelProcessInfo *info);
void *sceKernelGetProcessLibdb(SceUID pid);
SceUID sceKernelGetProcessMainThreadForDebugger(SceUID pid);
SceUID sceKernelGetProcessParentId(SceUID pid);
int sceKernelGetProcessSelfAuthInfo(SceUID pid, SceSelfAuthInfo *authInfo);
int sceKernelGetProcessStatus(SceUID pid, SceUInt32 *status);
int sceKernelGrowBudget(SceUID budgetId, int index, SceSize size);
int sceKernelIsCDialogAvailable(void);
int sceKernelIsGameBudget(void);
int sceKernelIsProcessSuspending(SceUID pid);
int sceKernelKillProcessForDebugger(SceUID pid, int kind);
int sceKernelProcessAssocApp(SceUID pid, SceUID app);
int sceKernelProcessDebugResume(SceUID pid, SceUInt32 reserved);
int sceKernelProcessDebugSuspend(SceUID pid, SceUInt32 state);
int sceKernelResetGameBudget(void);
int sceKernelResumeProcessForDebugger(SceUID pid);
int sceKernelSetPHBP(SceUID pid, SceUInt32 index, SceUInt32 value, SceUInt32 control);
int sceKernelSetPHWP(SceUID pid, SceUInt32 index, SceUInt32 value, SceUInt32 control);
int sceKernelSuspendProcessForDebugger(SceUID pid);
SceProcessObject *sceKernelUIDtoProcess(SceUID pid);

/* SceProcessmgr: the user syscalls */
void _sceKernelExitProcessForUser(int status);
int _sceKernelGetTimer5RegForUser(void *uaddr);
int _sceKernelRegisterLibkernelAddressesForUser(const LibkernelAddresses *uaddr);
int sceKernelCDialogSessionClose(void);
int sceKernelCDialogSetLeaseLimit(SceUInt32 limit);
int sceKernelCallAbortHandlerForUser(SceUInt32 code, const void *opt);
SceUID sceKernelGetCurrentProcess(void);
SceUID sceKernelGetExtraTty(void);
int sceKernelGetProcessName(char *name, SceSize size);
void *sceKernelGetProcessParam(void);
int sceKernelGetProcessTimeCore(SceUInt64 *time);
SceUInt32 sceKernelGetProcessTimeLowCore(void);
SceUInt64 sceKernelGetProcessTimeWideCore(void);
int sceKernelGetProcessTitleId(char *titleId, SceSize size);
int sceKernelGetRemoteProcessTime(SceUID pid, SceUInt64 *time);
SceUID sceKernelGetStderr(void);
SceUID sceKernelGetStdin(void);
SceUID sceKernelGetStdout(void);
SceUInt32 sceKernelLibcClock(void);
int sceKernelLibcGettimeofday(ProcessTimeval *utv, ProcessTimezone *utz);
int sceKernelLibcGmtime_r(const SceKernelTime *ut, ProcessTm *utm);
int sceKernelLibcLocaltime_r(const SceKernelTime *ut, ProcessTm *utm);
int sceKernelLibcMktime(ProcessTm *utm, SceKernelTime *ut, SceUInt64 *uwide);
SceKernelTime sceKernelLibcTime(SceKernelTime *ut);
int sceKernelPowerLock(int type);
int sceKernelPowerTick(int type);
int sceKernelProcessPowerUnlockForUser(int type);
int sceKernelRegisterProcessTerminationCallback(SceUID pid, SceUID cb);
int sceKernelUnregisterProcessTerminationCallback(SceUID pid, SceUID cb);

#ifdef __cplusplus
}
#endif
