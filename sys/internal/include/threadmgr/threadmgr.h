/*
 * SceKernelThreadMgr (os0:kd/threadmgr.skprx): its interface.
 *
 * The SDK's thread manager constants and types are Sony's kernel/threadmgr.h
 * (sys/sdk/target/include); this header adds the kernel's own.
 */

#pragma once

#include <scetypes.h>
#include <kernel/constant.h>
#include <sysmem/spinlock.h>
#include <kernel/types.h>
#include <kernel/threadmgr.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * The kernel's sceKernelCreateThread option (0x1c). The SDK's is the first eight bytes
 * (kernel/threadmgr.h, user mode); the kernel reads the rest when attr names them (threadmgr.c's
 * THREAD_OPT_*): the memory block types of the kernel stack, the user stack and the TLS, a user
 * stack block the caller gives, and the exceptions handed to the thread. The kernel-only names
 * are ours.
 */
typedef struct _SceKernelThreadOptParam {
	SceSize size;
	SceUInt32 attr;
	SceUInt32 kStackMemType;		/* 0x08 */
	SceUInt32 uStackMemType;
	SceUInt32 uTLSMemType;
	SceUInt32 uStackMemid;			/* 0x14 */
	SceUInt32 handedExceptions;
} SceKernelThreadOptParam;

/* the SDK's sceKernelCreateThread option: the first eight bytes of the kernel's */
typedef struct SceKernelThreadOptParamV1 {
	SceSize size;
	SceUInt32 attr;
} SceKernelThreadOptParamV1;

/* NID 0x9DCB4B7A (vitasdk: ksceKernelGetProcessId); our NID db names it this way */
SceUID sceKernelGetProcessId(void);

int sceKernelChangeThreadCpuAffinityMask(SceUID uid, uint32_t mask);
int sceKernelGetThreadCpuAffinityMask(SceUID thid);

/* our NID db name for the kernel thread exit (vitasdk: ksceKernelExitThread) */
int sceKernelExitThread(int status);

/* nonzero when the calling thread runs in user mode (a syscall) */
int sceKernelIsUserModeThread(void);

/* name unknown: nonzero while the caller may already own a lock it waits on (thread-state query) */
int SceThreadmgrForDriver_20C228E4(void);
int SceThreadmgrForDriver_56C18B11(void);				/* the current thread's I/O flags */
int SceThreadmgrForDriver_84EB1EA4(int value);			/* set them, returning the old */

/* ARM performance monitor control of a thread (0: the caller) */
int sceKernelPMonSetControlRegister(SceUID pid, uint32_t value);
int sceKernelPMonSetUserEnableRegister(SceUID pid, uint32_t value);
int pmonCpuSetEventSelf(SceUID threadId, uint32_t counter, uint32_t eventType);

/* Names unknown: per-thread ARM performance counters. */
int SceThreadmgrForKernel_7F831213(SceUID threadId, uint32_t mask);
int SceThreadmgrForKernel_1D2A6815(SceUID threadId, uint32_t mask);
int SceThreadmgrForKernel_7B3368F1(SceUID threadId, uint32_t counter, uint32_t value);
int SceThreadmgrForKernel_170F69D6(SceUID threadId, uint32_t counter, uint32_t *value);

struct ThreadmgrFastMutex;			/* threadmgr/fast_mutex_object.h */

/* sceKernelInitializeFastMutex's option */
typedef struct ThreadmgrFastMutexOptParam {
	SceSize size;
	int ceilingPriority;
} ThreadmgrFastMutexOptParam;

/* what sceKernelGetFastMutexInfo gives (0x44); lockCount is Sony's name ("inf.lockCount") */
typedef struct ThreadmgrFastMutexInfo {
	SceSize size;
	SceUID uid;
	char name[32];
	uint32_t attr;				/* 0x28 */
	struct ThreadmgrFastMutex *workArea;
	int lockCount;				/* 0x30 */
	SceUID currentOwnerId;			/* 0x34 */
	int ceilingPriority;
	int numWaitThreads;
	uint32_t numSpinThreads;		/* 0x40 */
} ThreadmgrFastMutexInfo;

/* a thread event handler's information (0x40, SceThreadmgrForDriver_6657429E) */
typedef struct ThreadmgrThreadEventInfo {
	SceSize size;
	SceUID uid;
	char name[32];
	uint32_t attr;				/* 0x28 */
	uint32_t mask;
	SceUID target;				/* 0x30 */
	SceUID pid;
	uintptr_t handler;			/* 0x38 */
	uint32_t arg;
} ThreadmgrThreadEventInfo;

/* what sceKernelExtendKernelStackWide runs on a stack of its own */
typedef int64_t (*ThreadmgrStackFunction)(void *arg);

/* an alarm's handler: returns the delay to the next call, 0 to end */
typedef SceUInt32 (*ThreadmgrAlarmHandler)(void *arg);

/* a thread's registers as the debugger sees them (0x48, SceThreadmgrForDriver_5022689D) */
typedef struct ThreadmgrDebugRegisters {
	uint32_t r[13];				/* 0x00 */
	uint32_t sp;				/* 0x34 */
	uint32_t lr;				/* 0x38 */
	uint32_t pc;				/* 0x3c */
	uint32_t cpsr;				/* 0x40 */
	uint32_t fpscr;				/* 0x44 */
} ThreadmgrDebugRegisters;

/* the process and thread running on this CPU (SceThreadmgrForKernel_6C1F092F) */
typedef struct ThreadCBInfo {
	SceUID processId;
	SceUID threadId;
} ThreadCBInfo;

#ifndef __cplusplus
_Static_assert(sizeof(SceKernelThreadOptParam) == 0x1c, "SceKernelThreadOptParam size");
_Static_assert(sizeof(SceKernelThreadOptParamV1) == 8, "SceKernelThreadOptParamV1 size");
_Static_assert(sizeof(ThreadmgrFastMutexInfo) == 0x44, "fast-mutex info size");
_Static_assert(sizeof(ThreadmgrThreadEventInfo) == 0x40, "thread event info size");
_Static_assert(sizeof(ThreadmgrDebugRegisters) == 0x48, "debugger registers size");
#endif

/* the tracers' hooks: an event, the count of the values that follow, the values */
typedef void (*ThreadmgrTraceHook)(uint32_t event, uint32_t argc, ...);
typedef void (*ThreadmgrOnCpuHook)(SceUID processId, SceUID threadId, int priority,
				   uint32_t cpuAffinityMask);
typedef void (*ThreadmgrOffCpuHook)(SceUID processId, SceUID threadId, uint32_t why);
typedef void (*ThreadmgrPriorityChangeHook)(SceUID processId, SceUID threadId, int newPriority,
					    int oldPriority);

/* Names unknown: install the on-CPU, off-CPU and priority change hooks. */
void SceThreadmgrForKernel_A3975A5A(ThreadmgrOnCpuHook hook);
void SceThreadmgrForKernel_F885ECCA(ThreadmgrOffCpuHook hook);
void SceThreadmgrForKernel_9605DF7D(ThreadmgrPriorityChangeHook hook);

/* Simple events and event callbacks. */
SceUID sceKernelCreateSimpleEvent(const char *name, SceUInt32 attr, uint32_t initPattern,
				  const SceKernelSimpleEventOptParam *option);
/* notifyId: what the callback is notified with */
int sceKernelRegisterCallbackToEventForDriver(SceUID eventId, SceUID callbackId, uint32_t notifyId);
SceInt32 sceKernelUnregisterCallbackFromEvent(SceUID eventId, SceUID callbackId);

/* Wait-context query and idle entry registration, recovered from ThreadMgr. */
int sceKernelCheckWaitableStatus(void);
void SceThreadmgrForDriver_B4A05763(void (*idleEntry)(void));

/* NID 0xB7DF3EDF: the kernel side of libkernel's sceKernelSetEventWithNotifyCallback. */
int sceKernelSetEventWithNotifyCallback(SceUID eventId, uint32_t pattern, uint64_t userData,
					uint32_t arg);

/* Timers (SceThreadmgrForDriver; the non-k names are Sony's). */
SceUID sceKernelCreateTimer(const char *name, SceUInt32 attr, const SceKernelTimerOptParam *option);
int sceKernelSetTimerEvent(SceUID timerId, SceUInt32 type, SceUInt64 *interval, int repeat);
int sceKernelStartTimer(SceUID timer);
int sceKernelStopTimer(SceUID timer);
int SceThreadmgrForDriver_91A8F38B(SceUID timer);	/* delete timer */
/* calls handler(arg) delay microseconds from now, in interrupt context */
/* vitasdk: ksceKernelRegisterTimer */
SceUID SceThreadmgrForDriver_C58DF384(const char *name, SceUInt32 delay,
				      ThreadmgrAlarmHandler handler, void *arg);
/* the same, with a 64-bit delay */
SceUID SceThreadmgrForDriver_10517330(const char *name, const SceUInt64 *delay,
				      ThreadmgrAlarmHandler handler, void *arg);

int sceKernelDelayThread(SceUInt32 usec);
SceInt64 sceKernelGetSystemTimeWide(void);
/* vitasdk: ksceKernelWaitEvent */
int SceThreadmgrForDriver_C529EA32(SceUID eventId, uint32_t pattern, uint32_t *outPattern,
				   uint64_t *outUserData, SceUInt32 *timeout);

/* thread-local slots (names unknown) */
/* allocate a TLS slot */
int SceThreadmgrForDriver_B645C7EF(uint32_t size,
				   void (*destructor)(void *memory, uintptr_t argument),
				   uintptr_t argument);
int SceThreadmgrForDriver_8C3C1F78(uint32_t slot);			/* free TLS slot */
void *SceThreadmgrForDriver_3A72C6D8(uint32_t slot);		/* the current thread's TLS slot */

int SceThreadmgrForDriver_1096042E(SceUID uid);
int SceThreadmgrForDriver_332E127C(void);
int SceThreadmgrForDriver_5311CFB5(uintptr_t hook, uintptr_t arg);
SceUID SceThreadmgrForDriver_FD6150D5(const char *name, SceUID target, SceUID pid, uint32_t mask,
				      uintptr_t handler, uint32_t arg);
int SceThreadmgrForKernel_15D6357C(SceUID threadId, int priority);
int SceThreadmgrForKernel_16D1961A(void);
int SceThreadmgrForKernel_490DAF14(SceUID threadId);
void *SceThreadmgrForKernel_565BD2DA(void);
void SceThreadmgrForKernel_8D53CA9B(unsigned int cpuId);
int SceThreadmgrForKernel_9928DFBA(SceUID threadId, int priority);
int SceThreadmgrForKernel_CBADD6B3(SceUID threadId, SceSize argSize, const void *arg);
int sceKernelClearEvent(SceUID eventId, uint32_t keep);
int sceKernelPulseEvent(SceUID eventId, uint32_t pattern, uint64_t userData);
int sceKernelPulseEventWithNotifyCallback(SceUID eventId, uint32_t pattern, uint64_t userData,
					  uint32_t arg);
int sceKernelSetEvent(SceUID eventId, uint32_t pattern, uint64_t userData);
/* vitasdk: ksceKernelWaitEventCB */
int SceThreadmgrForDriver_360C655C(SceUID eventId, uint32_t pattern, uint32_t *outPattern,
				   uint64_t *outUserData, SceUInt32 *timeout);
int sceKernelCancelAlarm(SceUID alarmId);
int sceKernelCancelEventFlag(SceUID evfId, uint32_t setPattern, int *numWaitThreads);
int sceKernelCancelSema(SceUID semaId, int setCount, int *numWaitThreads);
int sceKernelCpuScanThreadForProcessmgr(SceUID processId);
int sceKernelDeleteSimpleEvent(SceUID simpleEventId);
int sceKernelExitDeleteThread(int status);
int sceKernelGetEventPattern(SceUID eventId, uint32_t *pattern);
int sceKernelGetThreadExitStatus(SceUID uid, int *status);
int sceKernelSetSyslibtraceThreadCBEnable(int enable);

void SceThreadmgrForDriver_032E8F73(SceKernelSpinlock *lock);
SceUID SceThreadmgrForDriver_0486F239(SceUID pid);	/* vitasdk: ksceKernelSetProcessIdToTLS */
/* vitasdk: ksceKernelChangeThreadSuspendStatus */
int SceThreadmgrForDriver_04C6764B(SceUID threadId, uint32_t reason);
int SceThreadmgrForDriver_0892D8DF(SceUID callbackId);	/* vitasdk: ksceKernelGetCallbackCount */
int SceThreadmgrForDriver_09346D1A(SceUID pid, int level, SceKernelLwCondInfo *infos, SceSize size,
				   SceSize *count);
int SceThreadmgrForDriver_0A4AD37A(SceKernelSpinlock *lock);
int SceThreadmgrForDriver_0D0644DA(unsigned int index);
int SceThreadmgrForDriver_0EB0ABF0(SceUID pid, int level, SceKernelEventFlagInfo *infos,
				   SceSize size, SceSize *count);
int SceThreadmgrForDriver_12FC0FAB(SceUID workTaskId, SceUInt32 *timeout);
int SceThreadmgrForDriver_1378F6EF(unsigned int index, uint32_t value);
int SceThreadmgrForDriver_154F2C2A(SceUID workQueueId, SceUInt32 *timeout);
/* vitasdk: ksceKernelGetMsgPipeInfo */
int SceThreadmgrForDriver_207E0E18(SceUID msgPipeId, SceKernelMsgPipeInfo *info);
int SceThreadmgrForDriver_2B60B793(SceUInt32 usec, SceUInt32 *remaining);
int SceThreadmgrForDriver_2E95B628(SceUID msgPipeId, void *buffer, SceSize size, uint32_t mode,
				   SceSize *result);
int SceThreadmgrForDriver_2F9F75AA(SceUID msgPipeId, void *buffer, SceSize size, uint32_t mode,
				   SceSize *result, SceUInt32 *timeout);
int SceThreadmgrForDriver_2FC0FFF6(SceUID msgPipeId, const SceKernelMsgPipeVector *vector,
				   int count, uint32_t mode, SceSize *result, SceUInt32 *timeout);
int SceThreadmgrForDriver_33E85E9E(SceKernelThreadRunStatus *output);
int SceThreadmgrForDriver_3C297724(int level, SceKernelTimerInfo *infos, SceSize size,
				   SceSize *count);
int SceThreadmgrForDriver_3F5E1D56(SceUID rwLockId, SceUInt32 *timeout);
int SceThreadmgrForDriver_419E6736(SceUInt64 *time);
int SceThreadmgrForDriver_477F7C8A(SceUID pid, int level, SceKernelSemaInfo *infos, SceSize size,
				   SceSize *count);
int SceThreadmgrForDriver_47F26712(unsigned int group, unsigned int index);
SceUInt32 SceThreadmgrForDriver_47F6DE49(void);	/* vitasdk: ksceKernelGetSystemTimeLow */
int SceThreadmgrForDriver_48C06CD6(SceUID workQueueId, SceUInt32 *timeout);
int SceThreadmgrForDriver_49A0B679(SceUID threadId, const uint32_t *vfp, uint32_t mask);
int SceThreadmgrForDriver_4DE77569(int level, SceKernelEventInfo *infos, SceSize size,
				   SceSize *count);
/* vitasdk: ksceKernelGetThreadCpuRegisters */
int SceThreadmgrForDriver_5022689D(SceUID threadId, ThreadmgrDebugRegisters *regs);
int SceThreadmgrForDriver_54566860(SceUID rwLockId, SceUInt32 *timeout);
int SceThreadmgrForDriver_5870C73A(int level, SceKernelEventFlagInfo *infos, SceSize size,
				   SceSize *count);
/* vitasdk: ksceKernelGetVfpRegisterForDebugger */
int SceThreadmgrForDriver_5CDE387A(SceUID threadId, uint32_t *vfp);
int SceThreadmgrForDriver_5D8F88B1(int level, ThreadmgrThreadEventInfo *infos, SceSize size,
				   SceSize *count);
/* vitasdk: ksceKernelGetEventFlagInfo */
int SceThreadmgrForDriver_612277C9(SceUID evfId, SceKernelEventFlagInfo *info);
int SceThreadmgrForDriver_64E89DE9(SceUID threadId, const ThreadmgrDebugRegisters *regs,
				   uint32_t mask);
int SceThreadmgrForDriver_6657429E(SceUID uid, ThreadmgrThreadEventInfo *info);
/* vitasdk: ksceKernelGetThreadTLSAddr */
void *SceThreadmgrForDriver_66EEA46A(SceUID uid, unsigned int index);
int SceThreadmgrForDriver_67022B18(int level, SceKernelCondInfo *infos, SceSize size,
				   SceSize *count);
/* vitasdk: ksceKernelSendMsgPipeVector */
int SceThreadmgrForDriver_67DD3BAD(SceUID msgPipeId, const SceKernelMsgPipeVector *vector,
				   int count, uint32_t mode, SceSize *result, SceUInt32 *timeout);
int SceThreadmgrForDriver_6D4C1EB7(int level, ThreadmgrFastMutexInfo *infos, SceSize size,
				   SceSize *count);
int SceThreadmgrForDriver_738FF63D(int level, SceKernelLwCondInfo *infos, SceSize size,
				   SceSize *count);
int SceThreadmgrForDriver_767CA30C(int level, SceKernelCallbackInfo *infos, SceSize size,
				   SceSize *count);
/* vitasdk: ksceKernelGetThreadStackFreeSize */
int SceThreadmgrForDriver_7B278A0B(SceUID threadId);
int SceThreadmgrForDriver_7D12344A(SceUID pid, int level, SceKernelEventInfo *infos, SceSize size,
				   SceSize *count);
int SceThreadmgrForDriver_7E280B69(SceKernelSystemInfo *output);
/* vitasdk: ksceKernelSetTimerTimeWide */
SceUInt64 SceThreadmgrForDriver_85195A16(SceUID timerId, SceUInt64 time);
int SceThreadmgrForDriver_85F4DC8C(SceUID msgPipeId, void *buffer, SceSize size, uint32_t mode,
				   SceSize *result);
uint32_t SceThreadmgrForDriver_86DAE59B(void);
SceUID SceThreadmgrForDriver_88206D29(const char *name);
/* vitasdk: ksceKernelWaitEventFlagCB */
int SceThreadmgrForDriver_8A35F714(SceUID evfId, uint32_t pattern, uint32_t mode,
				   uint32_t *outPattern, SceUInt32 *timeout);
int SceThreadmgrForDriver_8C48E53B(const SceKernelWaitEvent *events, uint32_t count, uint32_t mode,
				   SceKernelResultEvent *results, SceUInt32 *timeout);
int SceThreadmgrForDriver_995F32E1(int level, SceKernelEventInfo *infos, SceSize size,
				   SceSize *count);
/* vitasdk: ksceKernelWaitThreadEndCB */
int SceThreadmgrForDriver_9EF4F62C(SceUID threadId, int *status, SceUInt32 *timeout);
/* vitasdk: ksceKernelIsThreadDebugSuspended */
int SceThreadmgrForDriver_A0B1AB21(SceUID threadId);
int SceThreadmgrForDriver_A64B00F6(SceUID rwLockId, SceUInt32 *timeout);
/* vitasdk: ksceKernelGetTimerBaseWide */
SceUInt64 SceThreadmgrForDriver_A6D11DD3(SceUID timerId);
SceKernelIntrStatus SceThreadmgrForDriver_A90B1E01(SceKernelSpinlock *lock);
int SceThreadmgrForDriver_AB977C72(SceUInt32 *attributes);
int SceThreadmgrForDriver_AF302193(SceUID uid, ThreadmgrFastMutexInfo *info);
int SceThreadmgrForDriver_B247EC4B(SceUID workTaskId, SceUInt32 *timeout);
int SceThreadmgrForDriver_B6DA4669(int level, SceKernelMutexInfo *infos, SceSize size,
				   SceSize *count);
void SceThreadmgrForDriver_C03799E2(SceKernelSpinlock *lock);
int SceThreadmgrForDriver_C040EC1C(SceUID callbackId);	/* vitasdk: ksceKernelCancelCallback */
/* vitasdk: ksceKernelGetTimerTimeWide */
SceUInt64 SceThreadmgrForDriver_C1286004(SceUID timerId);
int SceThreadmgrForDriver_C2DA6286(int level, SceKernelLwMutexInfo *infos, SceSize size,
				   SceSize *count);
int SceThreadmgrForDriver_C327CA7C(SceUID pid, int level, SceKernelTimerInfo *infos, SceSize size,
				   SceSize *count);
int SceThreadmgrForDriver_C4456EC9(SceUID pid, int level, ThreadmgrThreadEventInfo *infos,
				   SceSize size, SceSize *count);
int SceThreadmgrForDriver_C6741986(int level, SceKernelSemaInfo *infos, SceSize size,
				   SceSize *count);
int SceThreadmgrForDriver_C7F5FFE0(int level, SceKernelMsgPipeInfo *infos, SceSize size,
				   SceSize *count);
SceUID SceThreadmgrForDriver_C8E57BB4(SceUID pid, const char *name, SceKernelThreadEntry entry,
				      int initPriority, SceSize stackSize, uint32_t attr,
				      uint32_t cpuAffinityMask,
				      const SceKernelThreadOptParam *option);
/* vitasdk: ksceKernelGetSemaInfo */
int SceThreadmgrForDriver_C99DCF0D(SceUID semaId, SceKernelSemaInfo *info);
int SceThreadmgrForDriver_CA704239(SceUID pid, int level, SceKernelCondInfo *infos, SceSize size,
				   SceSize *count);
int SceThreadmgrForDriver_CFA2FFAF(SceUID rwLockId, SceUInt32 *timeout);
int SceThreadmgrForDriver_D0262C55(SceUID msgPipeId, void *buffer, SceSize size, uint32_t mode,
				   SceSize *result, SceUInt32 *timeout);
/* vitasdk: ksceKernelLockMutexCB_089 */
int SceThreadmgrForDriver_D06F2886(SceUID mutexId, int count, SceUInt32 *timeout);
int SceThreadmgrForDriver_D1FBDFA6(SceUID condId, SceKernelCondInfo *info);
int SceThreadmgrForDriver_D26BFF26(SceUID threadId);
SceSize SceThreadmgrForDriver_D74F66BD(void);
/* vitasdk: ksceKernelReceiveMsgPipeVector */
int SceThreadmgrForDriver_DA1F256B(SceUID msgPipeId, const SceKernelMsgPipeVector *vector,
				   int count, uint32_t mode, SceSize *result, SceUInt32 *timeout);
/* vitasdk: ksceKernelReceiveMsgPipeVectorCB */
int SceThreadmgrForDriver_DA5C9AC6(SceUID msgPipeId, const SceKernelMsgPipeVector *vector,
				   int count, uint32_t mode, SceSize *result, SceUInt32 *timeout);
int SceThreadmgrForDriver_E0C7BCBB(SceUID pid, int level, SceKernelEventInfo *infos, SceSize size,
				   SceSize *count);
int SceThreadmgrForDriver_E2B57231(int level, SceKernelRWLockInfo *infos, SceSize size,
				   SceSize *count);
/* vitasdk: ksceKernelRunWithStack */
int64_t SceThreadmgrForDriver_E54FD746(SceSize size, int (*function)(void *arg), void *arg);
int SceThreadmgrForDriver_E6D80698(SceUID msgPipeId, void *buffer, SceSize size, uint32_t mode,
				   SceSize *result, SceUInt32 *timeout);
void *SceThreadmgrForDriver_E938FB20(unsigned int index);	/* vitasdk: ksceKernelGetTLSAddr */
int SceThreadmgrForDriver_E9A99C50(SceUInt32 usec, SceUInt32 *remaining);
int SceThreadmgrForDriver_E9E50096(SceUID pid, int level, SceKernelMsgPipeInfo *infos, SceSize size,
				   SceSize *count);
SceUID SceThreadmgrForDriver_F311808F(SceUID threadId);	/* vitasdk: ksceKernelGetUserThreadId */
int SceThreadmgrForDriver_F5074173(SceUID msgPipeId, void *buffer, SceSize size, uint32_t mode,
				   SceSize *result, SceUInt32 *timeout);
int SceThreadmgrForDriver_F55E4D86(SceUID semaId, int needCount, SceUInt32 *timeout);
SceKernelIntrStatus SceThreadmgrForDriver_F8A6013B(SceKernelSpinlock *lock);
void SceThreadmgrForDriver_F96FE0D5(SceKernelSpinlock *lock, SceKernelIntrStatus state);
SceUID SceThreadmgrForDriver_FA54D49A(void);	/* vitasdk: ksceKernelGetProcessIdFromTLS */
int SceThreadmgrForDriver_FF8DA217(SceUID pid, int level, SceKernelCallbackInfo *infos,
				   SceSize size, SceSize *count);
int SceThreadmgrForKernel_0788C9BC(unsigned int cpuId, void (*fn)(void *), void *arg, SceSize size);
int SceThreadmgrForKernel_sceKernelLockMutex(SceUID mutexId, int count, SceUInt32 *timeout);
void SceThreadmgrForKernel_10853CCD(ThreadmgrTraceHook hook);
struct SceKernelThreadmgrPrivate *SceThreadmgrForKernel_19440C83(void);
int SceThreadmgrForKernel_392599F1(SceUID threadId, SceUInt32 *timeout);
int SceThreadmgrForKernel_444CE648(SceUID threadId, uint32_t *vfp);
int64_t SceThreadmgrForKernel_4BAA5F3B(SceSize size, ThreadmgrStackFunction function, void *arg);
int SceThreadmgrForKernel_5075AE5E(SceUID threadId, const ThreadmgrDebugRegisters *regs,
				   uint32_t mask, int mode);
int SceThreadmgrForKernel_60798313(uint32_t *value);
int SceThreadmgrForKernel_614BF2BE(SceUID threadId, SceUInt32 *timeout);
/* vitasdk: ksceKernelGetThreadContextInfo */
int SceThreadmgrForKernel_6C1F092F(ThreadCBInfo *info);
int SceThreadmgrForKernel_6DC1BE7A(void);
int SceThreadmgrForKernel_800ACB9D(SceUID threadId, int priority);
void SceThreadmgrForKernel_8C07C8EC(ThreadmgrTraceHook hook);
int SceThreadmgrForKernel_sceKernelSetThreadAccessLevel(int permission);
int SceThreadmgrForKernel_A7E5C83D(int *enable);
void SceThreadmgrForKernel_ABBE9BA5(ThreadmgrTraceHook hook);
int SceThreadmgrForKernel_AC726660(SceUID threadId, const uint32_t *vfp, uint32_t mask, int mode);
int SceThreadmgrForKernel_BB56D42F(void);
int SceThreadmgrForKernel_C4735A7E(SceUID threadId, ThreadmgrDebugRegisters *regs);
int SceThreadmgrForKernel_DF59DF99(unsigned int cpuId, void (*fn)(void *), void *arg);
void SceThreadmgrForKernel_E2DACBC2(ThreadmgrTraceHook hook);
int SceThreadmgrForKernel_sceKernelWaitSema(SceUID semaId, int needCount, SceUInt32 *timeout);
int SceThreadmgrForKernel_F46783E2(void);
void SceThreadmgrForKernel_F55FDEFB(ThreadmgrTraceHook hook);
SceUID SceThreadmgrForKernel_sceKernelCreateMutex(const char *name, SceUInt32 attr, int initCount,
						  const SceKernelMutexOptParam *option);
SceUID SceThreadmgrForKernel_sceKernelCreateSema(const char *name, SceUInt32 attr, int initCount,
						 int maxCount, const SceKernelSemaOptParam *option);
int SceThreadmgrForKernel_sceKernelDeleteMutex(SceUID mutexId);
int SceThreadmgrForKernel_sceKernelDeleteSema(SceUID semaId);
int SceThreadmgrForKernel_sceKernelSignalSema(SceUID semaId, int signalCount);
int SceThreadmgrForKernel_sceKernelUnlockMutex(SceUID mutexId, int count);
int SceThreadmgr_sceKernelGetThreadCpuAffinityMask(SceUID uid);
int _sceKernelCancelEvent(SceUID eventId, int *numWaitThreads);
int _sceKernelCancelEventFlag(SceUID evfId, uint32_t pattern, int *numWaitThreads);
int _sceKernelCancelMsgPipe(SceUID msgPipeId, int *numSendWaitThreads, int *numReceiveWaitThreads);
int _sceKernelCancelMutex(SceUID mutexId, int newCount, int *numWaitThreads);
int _sceKernelCancelRWLock(SceUID rwLockId, int *numReadWaitThreads, int *numWriteWaitThreads,
			   SceUInt32 type);
int _sceKernelCancelSema(SceUID semaId, int setCount, int *numWaitThreads);
int _sceKernelCancelTimer(SceUID uid, uint32_t *count);
SceUID _sceKernelCreateCond(const char *name, SceUInt32 attr, SceUID mutexId, const void *option);
SceUID _sceKernelCreateEventFlag(const char *name, SceUInt32 attr, uint32_t initPattern,
				 const void *option);
SceUID _sceKernelCreateMutex(const char *name, SceUInt32 attr, int initCount, const void *option);
SceUID _sceKernelCreateRWLock(const char *name, SceUInt32 attr, const void *option);
SceUID _sceKernelCreateSimpleEvent(const char *name, SceUInt32 attr, uint32_t initPattern,
				   const void *option);
int _sceKernelCreateTimer(const char *name, uint32_t attributes, const uint32_t *options);
int _sceKernelDeleteLwMutex(void *work);
int _sceKernelGetEventFlagInfo(SceUID evfId, SceKernelEventFlagInfo *info, const uint32_t *args);
int _sceKernelGetEventPattern(SceUID eventId, uint32_t *pattern);
int _sceKernelGetThreadExitStatus(SceUID threadId, int *userStatus);
int _sceKernelGetTimerBase(SceUID uid, uint64_t *userValue);
int _sceKernelGetTimerEventRemainingTime(SceUID uid, uint64_t *output);
int _sceKernelGetTimerInfo(SceUID uid, uint32_t *output, const uint32_t *options);
int _sceKernelGetTimerTime(SceUID uid, uint64_t *userValue);
int _sceKernelLockMutex(SceUID mutexId, int count, SceUInt32 *timeout);
int _sceKernelLockMutexCB(SceUID mutexId, int count, SceUInt32 *timeout);
int _sceKernelLockReadRWLock(SceUID rwLockId, SceUInt32 *timeout);
int _sceKernelLockReadRWLockCB(SceUID rwLockId, SceUInt32 *timeout);
int _sceKernelLockWriteRWLock(SceUID rwLockId, SceUInt32 *timeout);
int _sceKernelLockWriteRWLockCB(SceUID rwLockId, SceUInt32 *timeout);
int _sceKernelPMonThreadGetCounter(SceUID threadId, uint32_t counter, uint32_t *value);
int _sceKernelPollEvent(SceUID eventId, uint32_t pattern, uint32_t *outPattern,
			uint64_t *outUserData);
int _sceKernelPollEventFlag(SceUID evfId, uint32_t pattern, uint32_t mode, uint32_t *outPattern);
int _sceKernelSetEventWithNotifyCallback(SceUID eventId, uint32_t pattern, const uint32_t *args);
int _sceKernelSetTimerEvent(SceUID uid, uint32_t type, const uint64_t *interval, uint32_t repeat);
int _sceKernelSetTimerTime(SceUID uid, uint64_t *userValue);
int _sceKernelStartThread(SceUID threadId, SceSize argSize, const void *arg);
int _sceKernelWaitCond(SceUID condId, SceUInt32 *timeout);
int _sceKernelWaitCondCB(SceUID condId, SceUInt32 *timeout);
int _sceKernelWaitSema(SceUID semaId, int needCount, SceUInt32 *timeout);
int _sceKernelWaitSemaCB(SceUID semaId, int needCount, SceUInt32 *timeout);
int _sceKernelWaitThreadEnd(SceUID threadId, int *userStatus, SceUInt32 *userTimeout);
int _sceKernelWaitThreadEndCB(SceUID threadId, int *userStatus, SceUInt32 *userTimeout);
int lockFastMutex(struct ThreadmgrFastMutex *mutex);
int sceKernelCancelCallback(SceUID uid);
int sceKernelCancelEvent(SceUID eventId, int *numWaitThreads);
int sceKernelCancelEventWithSetPattern(SceUID eventId, uint32_t setPattern, uint64_t userData,
				       int *numWaitThreads);
int sceKernelCancelMsgPipe(SceUID msgPipeId, int *numSendWaitThreads, int *numReceiveWaitThreads);
int sceKernelCancelMutex(SceUID mutexId, int newCount, int *numWaitThreads);
int sceKernelCancelRWLock(SceUID rwLockId, int *numReadWaitThreads, int *numWriteWaitThreads,
			  SceUInt32 type);
int sceKernelCancelTimer(SceUID timerId, int *numWaitThreads);
int sceKernelChangeActiveCpuMask(uint32_t mask);
int sceKernelChangeCurrentThreadAttr(uint32_t clear, uint32_t set);
int sceKernelChangeThreadPriority(SceUID uid, uint32_t priority);
int sceKernelChangeThreadPriority2(SceUID uid, uint32_t priority);
int sceKernelChangeThreadPriority2ForUser(SceUID uid, uint32_t priority);
int sceKernelChangeThreadPriorityForUser(SceUID uid, uint32_t priority);
uint32_t sceKernelChangeThreadVfpException(uint32_t clear, uint32_t set);
int sceKernelChangeThreadVfpExceptionForUser(uint32_t clear, uint32_t set);
int sceKernelCheckCallback(void);
int sceKernelClearEventFlag(SceUID evfId, uint32_t mask);
int sceKernelClearThreadMgrTestMode(void);
int sceKernelCloseCond(SceUID condId);
int sceKernelCloseEventFlag(SceUID uid);
int sceKernelCloseMsgPipe(SceUID msgPipeId);
int sceKernelCloseMutex(SceUID mutexId);
int sceKernelCloseRWLock(SceUID rwLockId);
int sceKernelCloseSema(SceUID semaId);
int sceKernelCloseSimpleEvent(SceUID simpleEventId);
int sceKernelCloseTimer(SceUID uid);
int sceKernelCountWaitingThreadsProc(SceUID pid, SceUID waitId);
SceUID sceKernelCreateCallback(const char *name, SceUInt32 attr, SceKernelCallbackFunction entry,
			       void *common);
SceUID sceKernelCreateCond(const char *name, SceUInt32 attr, SceUID mutexId,
			   const SceKernelCondOptParam *option);
SceUID sceKernelCreateEventFlag(const char *name, SceUInt32 attr, uint32_t initPattern,
				const SceKernelEventFlagOptParam *option);
SceUID sceKernelCreateMutex(const char *name, SceUInt32 attr, int initCount,
			    const SceKernelMutexOptParam *option);
SceUID sceKernelCreateRWLock(const char *name, SceUInt32 attr,
			     const SceKernelRWLockOptParam *option);
SceUID sceKernelCreateSema(const char *name, SceUInt32 attr, int initCount, int maxCount,
			   const SceKernelSemaOptParam *option);
SceUID sceKernelCreateThread(const char *name, SceKernelThreadEntry entry, int initPriority,
			     SceSize stackSize, uint32_t attr, uint32_t cpuAffinityMask,
			     const SceKernelThreadOptParam *option);
int sceKernelDebugResumeThread(SceUID threadId, uint32_t reason);
int sceKernelDebugSuspendThread(SceUID threadId, uint32_t reason);
int sceKernelDelayThread200(SceUInt32 usec);
int sceKernelDelayThreadCB(SceUInt32 usec);
int sceKernelDelayThreadCB200(SceUInt32 usec);
int sceKernelDeleteCallback(SceUID callbackId);
int sceKernelDeleteCond(SceUID condId);
int sceKernelDeleteEventFlag(SceUID evfId);
int sceKernelDeleteMsgPipe(SceUID msgPipeId);
int sceKernelDeleteMutex(SceUID mutexId);
int sceKernelDeleteRWLock(SceUID rwLockId);
int sceKernelDeleteSema(SceUID semaId);
int sceKernelDeleteThread(SceUID threadId);
int64_t sceKernelExtendKernelStackWide(SceSize size, ThreadmgrStackFunction function, void *arg);
int sceKernelFinalizeFastMutex(struct ThreadmgrFastMutex *mutex);
int sceKernelGetCallbackCount(SceUID uid);
uint32_t sceKernelGetCurrentThreadVfpException(void);
int sceKernelGetFastMutexInfo(struct ThreadmgrFastMutex *mutex, ThreadmgrFastMutexInfo *info);
int sceKernelGetLwMutexInfoAllProc(SceUID pid, int level, SceKernelLwMutexInfo *infos, SceSize size,
				   SceSize *count);
SceUID sceKernelGetMsgPipeCreatorId(const char *userName);
int sceKernelGetMutexInfoAllProc(SceUID pid, int level, SceKernelMutexInfo *infos, SceSize size,
				 SceSize *count);
int sceKernelGetRWLockInfo(SceUID rwLockId, SceKernelRWLockInfo *info);
int sceKernelGetRWLockInfoAllProc(SceUID pid, int level, SceKernelRWLockInfo *infos, SceSize size,
				  SceSize *count);
int sceKernelGetTHBP(SceUID threadId, int selector, uint32_t *value0, uint32_t *value1);
int sceKernelGetThreadCurrentPriority(void);
SceUID sceKernelGetThreadId(void);
int sceKernelGetThreadIdList(SceUID pid, SceUID *ids, SceSize size, SceSize *count);
int sceKernelGetThreadInfo(SceUID threadId, SceKernelThreadInfo *output);
int sceKernelGetThreadStackFreeSize(SceUID threadId);
void *sceKernelGetThreadTLSAddr(SceUID threadId, unsigned int index);
int sceKernelGetThreadmgrUIDClass(SceUID uid);
int sceKernelGetTimerBase(SceUID timerId, SceUInt64 *base);
uint64_t sceKernelGetTimerBaseWide(SceUID uid);
int sceKernelGetTimerEventRemainingTime(SceUID timerId, SceUInt64 *remaining);
int sceKernelGetTimerTime(SceUID timerId, SceUInt64 *time);
uint64_t sceKernelGetTimerTimeWide(SceUID uid);
int sceKernelInitializeFastMutex(struct ThreadmgrFastMutex *mutex, const char *name, SceUInt32 attr,
				 const ThreadmgrFastMutexOptParam *option);
int sceKernelLockMutex(SceUID mutexId, int count, SceUInt32 *timeout);
int sceKernelLockMutexCB(SceUID mutexId, int count, SceUInt32 *timeout);
int sceKernelNotifyCallback(SceUID callbackId, int arg);
SceUID sceKernelOpenCond(const char *userName);
int sceKernelOpenEventFlag(const char *userName);
SceUID sceKernelOpenMutex(const char *userName);
SceUID sceKernelOpenRWLock(const char *userName);
SceUID sceKernelOpenSema(const char *userName);
int sceKernelOpenTimer(const char *userName);
int sceKernelPollEvent(SceUID eventId, uint32_t pattern, uint32_t *outPattern,
		       uint64_t *outUserData);
int sceKernelPollEventFlag(SceUID evfId, uint32_t pattern, uint32_t mode, uint32_t *outPattern);
int sceKernelPollSema(SceUID semaId, int needCount);
int sceKernelRegisterCallbackToEvent(SceUID eventId, SceUID callbackId);
int sceKernelResumeThreadForVM(SceUID threadId);
SceUID sceKernelScheduleWorkTask(SceUID workQueueId, const char *name, int (*entry)(void *arg),
				  void *arg);
int sceKernelSendSignal(SceUID threadId);
int sceKernelSetDTraceThreadCBEnable(int enable);
int sceKernelSetEventFlag(SceUID evfId, uint32_t pattern);
int sceKernelSetTHBP(SceUID threadId, int selector, uint32_t value0, uint32_t value1);
int sceKernelSetThreadAccessLevel(int permission);
int sceKernelSetThreadMgrTestMode(void);
int sceKernelSetTimerTime(SceUID timerId, SceUInt64 *time);
uint64_t sceKernelSetTimerTimeWide(SceUID uid, uint64_t value);
int sceKernelSignalCond(SceUID condId);
int sceKernelSignalCondAll(SceUID condId);
int sceKernelSignalCondTo(SceUID condId, SceUID threadId);
int sceKernelSignalSema(SceUID semaId, int signalCount);
int sceKernelStartThread(SceUID threadId, SceSize argSize, const void *arg);
int sceKernelStartThreadForUser(SceUID threadId, SceSize argSize, const void *arg);
int sceKernelSuspendThreadForVM(SceUID threadId);
int sceKernelTryLockFastMutex(struct ThreadmgrFastMutex *mutex);
int sceKernelTryLockMutex(SceUID mutexId, int count);
int sceKernelTryLockReadRWLock(SceUID rwLockId);
int sceKernelTryLockWriteRWLock(SceUID rwLockId);
int sceKernelTryReceiveMsgPipeVector(SceUID msgPipeId, const SceKernelMsgPipeVector *vector,
				     int count, uint32_t mode, SceSize *result);
int sceKernelTrySendMsgPipeVector(SceUID msgPipeId, const SceKernelMsgPipeVector *vector, int count,
				  uint32_t mode, SceSize *result);
int sceKernelUnlockFastMutex(struct ThreadmgrFastMutex *mutex);
int sceKernelUnlockMutex(SceUID mutexId, int count);
int sceKernelUnlockReadRWLock(SceUID rwLockId);
int sceKernelUnlockWriteRWLock(SceUID rwLockId);
int sceKernelUnregisterCallbackFromEventAll(SceUID eventId);
int sceKernelUnregisterThreadEventHandler(SceUID uid);
int sceKernelWaitCond(SceUID condId, SceUInt32 *timeout);
int sceKernelWaitEventFlag(SceUID evfId, uint32_t pattern, uint32_t mode, uint32_t *outPattern,
			   SceUInt32 *timeout);
int sceKernelWaitSema(SceUID semaId, int needCount, SceUInt32 *timeout);
int sceKernelWaitThreadEnd(SceUID threadId, int *status, SceUInt32 *timeout);
struct ThreadRegisterContext *setupNextContext_intr(uintptr_t unused, uint32_t reason);
int threadmgrGpuExceptionHandler(SceUID pid);

#ifdef __cplusplus
}
#endif
