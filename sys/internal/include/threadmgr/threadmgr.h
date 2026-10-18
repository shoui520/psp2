/*
 * SceKernelThreadMgr (os0:kd/threadmgr.skprx): its interface.
 *
 * First the SDK's thread manager constants and types (SDK Release 03.570.011,
 * target/include/kernel/threadmgr.h) under Sony's names and layouts: what the kernel fills for
 * its callers and takes from them. They move to sys/sdk/target/include/kernel/threadmgr.h with
 * the user-mode SDK headers. Then the kernel's own.
 */

#pragma once

#include <psp2kern/types.h>
#include <stdint.h>

#ifndef SCE_KERNEL_MAX_CPU
#define SCE_KERNEL_MAX_CPU			(4)	/* SDK kernel/cpu.h */
#endif

/* SceKernelThreadInfo.status */
#define SCE_KERNEL_THREAD_STATUS_RUNNING	0x00000001U
#define SCE_KERNEL_THREAD_STATUS_READY		0x00000002U
#define SCE_KERNEL_THREAD_STATUS_STANDBY	0x00000004U
#define SCE_KERNEL_THREAD_STATUS_WAITING	0x00000008U
#define SCE_KERNEL_THREAD_STATUS_DORMANT	0x00000010U
#define SCE_KERNEL_THREAD_STATUS_DELETED	0x00000020U
#define SCE_KERNEL_THREAD_STATUS_DEAD		0x00000040U
#define SCE_KERNEL_THREAD_STATUS_STAGNANT	0x00000080U
#define SCE_KERNEL_THREAD_STATUS_SUSPENDED	0x00000100U
#define SCE_KERNEL_THREAD_STATUS_MASK		0x0000ffffU

/* SceKernelThreadInfo.waitType; the _CB forms check callbacks while waiting */
#define SCE_KERNEL_WAITTYPE_DELAY		0x00000001U
#define SCE_KERNEL_WAITTYPE_WAITTHEND		0x00000002U
#define SCE_KERNEL_WAITTYPE_SIGNAL		0x00000004U
#define SCE_KERNEL_WAITTYPE_WAITTHSUSPEND	0x00000008U
#define SCE_KERNEL_WAITTYPE_EVENTFLAG		0x00000010U
#define SCE_KERNEL_WAITTYPE_SEMAPHORE		0x00000020U
#define SCE_KERNEL_WAITTYPE_MUTEX		0x00000040U
#define SCE_KERNEL_WAITTYPE_RW_LOCK		0x00000080U
#define SCE_KERNEL_WAITTYPE_COND_SIGNAL		0x00000100U
#define SCE_KERNEL_WAITTYPE_COND_MUTEX		0x00000200U
#define SCE_KERNEL_WAITTYPE_FAST_MUTEX		0x00001000U
#define SCE_KERNEL_WAITTYPE_FAST_MUTEX_SPIN	0x00002000U
#define SCE_KERNEL_WAITTYPE_EVENT		0x00010000U
#define SCE_KERNEL_WAITTYPE_MP_EVENTS		0x00020000U
#define SCE_KERNEL_WAITTYPE_MSG_PIPE		0x00040000U
#define SCE_KERNEL_WAITTYPE_LW_MUTEX		0x00100000U
#define SCE_KERNEL_WAITTYPE_LW_COND_SIGNAL	0x00200000U
#define SCE_KERNEL_WAITTYPE_LW_COND_LW_MUTEX	0x00400000U
#define SCE_KERNEL_WAITTYPE_CB			0x80000000U	/* ours: the bit the _CB forms add */

#define SCE_KERNEL_WAITTYPE_DELAY_CB		0x80000001U
#define SCE_KERNEL_WAITTYPE_WAITTHEND_CB	0x80000002U
#define SCE_KERNEL_WAITTYPE_SIGNAL_CB		0x80000004U
#define SCE_KERNEL_WAITTYPE_WAITTHSUSPEND_CB	0x80000008U
#define SCE_KERNEL_WAITTYPE_EVENTFLAG_CB	0x80000010U
#define SCE_KERNEL_WAITTYPE_SEMAPHORE_CB	0x80000020U
#define SCE_KERNEL_WAITTYPE_MUTEX_CB		0x80000040U
#define SCE_KERNEL_WAITTYPE_RW_LOCK_CB		0x80000080U
#define SCE_KERNEL_WAITTYPE_COND_SIGNAL_CB	0x80000100U
#define SCE_KERNEL_WAITTYPE_COND_MUTEX_CB	0x80000200U
#define SCE_KERNEL_WAITTYPE_FAST_MUTEX_CB	0x80001000U
#define SCE_KERNEL_WAITTYPE_FAST_MUTEX_SPIN_CB	0x80002000U
#define SCE_KERNEL_WAITTYPE_EVENT_CB		0x80010000U
#define SCE_KERNEL_WAITTYPE_MP_EVENTS_CB	0x80020000U
#define SCE_KERNEL_WAITTYPE_MSG_PIPE_CB		0x80040000U
#define SCE_KERNEL_WAITTYPE_LW_MUTEX_CB		0x80100000U
#define SCE_KERNEL_WAITTYPE_LW_COND_SIGNAL_CB	0x80200000U
#define SCE_KERNEL_WAITTYPE_LW_COND_LW_MUTEX_CB	0x80400000U

/* the UID classes sceKernelGetThreadmgrUIDClass tells apart */
#define SCE_KERNEL_THREADMGR_UID_CLASS_THREAD		1
#define SCE_KERNEL_THREADMGR_UID_CLASS_SEMA		2
#define SCE_KERNEL_THREADMGR_UID_CLASS_EVENT_FLAG	3
#define SCE_KERNEL_THREADMGR_UID_CLASS_MUTEX		4
#define SCE_KERNEL_THREADMGR_UID_CLASS_COND		5
#define SCE_KERNEL_THREADMGR_UID_CLASS_TIMER		6
#define SCE_KERNEL_THREADMGR_UID_CLASS_MSG_PIPE		7
#define SCE_KERNEL_THREADMGR_UID_CLASS_CALLBACK		8
#define SCE_KERNEL_THREADMGR_UID_CLASS_THREAD_EVENT	9
#define SCE_KERNEL_THREADMGR_UID_CLASS_LW_MUTEX		10
#define SCE_KERNEL_THREADMGR_UID_CLASS_LW_COND		11
#define SCE_KERNEL_THREADMGR_UID_CLASS_RW_LOCK		12
#define SCE_KERNEL_THREADMGR_UID_CLASS_SIMPLE_EVENT	13

/* synchronization objects' attributes */
#define SCE_KERNEL_ATTR_SINGLE			0x00000000U	/* one waiting thread */
#define SCE_KERNEL_ATTR_MULTI			0x00001000U	/* several */
#define SCE_KERNEL_ATTR_TH_FIFO			0x00000000U	/* waiters in arrival order */
#define SCE_KERNEL_ATTR_TH_PRIO			0x00002000U	/* in priority order */
#define SCE_KERNEL_ATTR_OPENABLE		0x00000080U	/* sceKernelOpen* finds it */

/* event flags: how sceKernelWaitEventFlag matches and clears */
#define SCE_KERNEL_EVF_WAITMODE_AND		0x00000000U
#define SCE_KERNEL_EVF_WAITMODE_OR		0x00000001U
#define SCE_KERNEL_EVF_WAITMODE_CLEAR_ALL	0x00000002U
#define SCE_KERNEL_EVF_WAITMODE_CLEAR_PAT	0x00000004U

#define SCE_KERNEL_EVF_ATTR_TH_FIFO		SCE_KERNEL_ATTR_TH_FIFO
#define SCE_KERNEL_EVF_ATTR_TH_PRIO		SCE_KERNEL_ATTR_TH_PRIO
#define SCE_KERNEL_EVF_ATTR_SINGLE		SCE_KERNEL_ATTR_SINGLE
#define SCE_KERNEL_EVF_ATTR_MULTI		SCE_KERNEL_ATTR_MULTI

#define SCE_KERNEL_MUTEX_ATTR_RECURSIVE		0x00000002U
#define SCE_KERNEL_MUTEX_ATTR_CEILING		0x00000004U

/* VFP exceptions a thread takes (sceKernelChangeThreadVfpException) */
#define SCE_KERNEL_VFP_EXCEPTION_MASK_QCE	0x08000000U
#define SCE_KERNEL_VFP_EXCEPTION_MASK_IDE	0x00000080U
#define SCE_KERNEL_VFP_EXCEPTION_MASK_IXE	0x00000010U
#define SCE_KERNEL_VFP_EXCEPTION_MASK_UFE	0x00000008U
#define SCE_KERNEL_VFP_EXCEPTION_MASK_OFE	0x00000004U
#define SCE_KERNEL_VFP_EXCEPTION_MASK_DZE	0x00000002U
#define SCE_KERNEL_VFP_EXCEPTION_MASK_IOE	0x00000001U

/* thread event handlers (sceKernelRegisterThreadEventHandler) */
#define SCE_KERNEL_THREAD_EVENT_TYPE_START	0x04
#define SCE_KERNEL_THREAD_EVENT_TYPE_EXIT	0x08

/* timers: how the timer's event is notified */
#define SCE_KERNEL_TIMER_TYPE_SET_EVENT		0
#define SCE_KERNEL_TIMER_TYPE_PULSE_EVENT	1

typedef SceInt32 (*SceKernelThreadEntry)(SceSize argSize, void *pArgBlock);
typedef SceInt32 (*SceKernelChangeStackFunction)(void *pArg);
typedef SceInt32 (*SceKernelCallbackFunction)(SceUID notifyId, SceInt32 notifyCount,
					      SceInt32 notifyArg, void *pCommon);
typedef SceInt32 (*SceKernelThreadEventHandler)(SceInt32 type, SceUID threadId, SceInt32 arg,
						void *pCommon);

/*
 * sceKernelCreateThread's option (0x1c). The SDK's is the first eight bytes; the kernel reads
 * the rest when attr names them (threadmgr.c's THREAD_OPT_*): the memory block types of the
 * kernel stack, the user stack and the TLS, a user stack block the caller gives, and the
 * exceptions handed to the thread. The kernel-only names are ours.
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

/* sceKernelGetThreadInfo (0x80) */
typedef struct _SceKernelThreadInfo {
	SceSize size;
	SceUID processId;
	char name[SCE_UID_NAMELEN + 1];
	SceUInt32 attr;				/* 0x28 */
	SceUInt32 status;
	SceKernelThreadEntry entry;		/* 0x30 */
	void *pStack;
	SceSize stackSize;
	SceInt32 initPriority;			/* 0x3c */
	SceInt32 currentPriority;
	SceInt32 initCpuAffinityMask;
	SceInt32 currentCpuAffinityMask;	/* 0x48 */
	SceInt32 currentCpuId;
	SceInt32 lastExecutedCpuId;
	SceUInt32 waitType;			/* 0x54 */
	SceUID waitId;
	SceInt32 exitStatus;
	SceKernelSysClock runClocks;		/* 0x60 */
	SceUInt32 intrPreemptCount;		/* 0x68 */
	SceUInt32 threadPreemptCount;
	SceUInt32 threadReleaseCount;
	SceInt32 changeCpuCount;		/* 0x74 */
	SceInt32 fNotifyCallback;
	SceInt32 reserved;
} SceKernelThreadInfo;

/* sceKernelGetThreadRunStatus: the thread running on each CPU (0x34) */
typedef struct _SceKernelThreadRunStatus {
	SceSize size;
	struct {
		SceUID processId;
		SceUID threadId;
		SceInt32 priority;
	} cpuInfo[SCE_KERNEL_MAX_CPU];
} SceKernelThreadRunStatus;

/* sceKernelGetSystemInfo: each CPU's idle time and switches (0x48) */
typedef struct _SceKernelSystemInfo {
	SceSize size;
	SceUInt32 activeCpuMask;
	struct {
		SceKernelSysClock idleClock;
		SceUInt32 comesOutOfIdleCount;
		SceUInt32 threadSwitchCount;
	} cpuInfo[SCE_KERNEL_MAX_CPU];
} SceKernelSystemInfo;

/* sceKernelGetCallbackInfo (0x44) */
typedef struct _SceKernelCallbackInfo {
	SceSize size;
	SceUID callbackId;
	char name[SCE_UID_NAMELEN + 1];
	SceUInt32 attr;				/* 0x28 */
	SceUID threadId;
	SceKernelCallbackFunction callbackFunc;	/* 0x30 */
	SceUID notifyId;
	SceInt32 notifyCount;
	SceInt32 notifyArg;			/* 0x3c */
	void *pCommon;
} SceKernelCallbackInfo;

typedef struct _SceKernelEventFlagOptParam {
	SceSize size;
} SceKernelEventFlagOptParam;

/* sceKernelGetEventFlagInfo (0x38) */
typedef struct _SceKernelEventFlagInfo {
	SceSize size;
	SceUID evfId;
	char name[SCE_UID_NAMELEN + 1];
	SceUInt32 attr;				/* 0x28 */
	SceUInt32 initPattern;
	SceUInt32 currentPattern;
	SceUInt32 numWaitThreads;		/* 0x34 */
} SceKernelEventFlagInfo;

typedef struct _SceKernelSemaOptParam {
	SceSize size;
} SceKernelSemaOptParam;

/* sceKernelGetSemaInfo (0x3c) */
typedef struct _SceKernelSemaInfo {
	SceSize size;
	SceUID semaId;
	char name[SCE_UID_NAMELEN + 1];
	SceUInt32 attr;				/* 0x28 */
	SceInt32 initCount;
	SceInt32 currentCount;
	SceInt32 maxCount;			/* 0x34 */
	SceUInt32 numWaitThreads;
} SceKernelSemaInfo;

typedef struct _SceKernelMutexOptParam {
	SceSize size;
	SceInt32 ceilingPriority;
} SceKernelMutexOptParam;

/* sceKernelGetMutexInfo (0x40) */
typedef struct _SceKernelMutexInfo {
	SceSize size;
	SceUID mutexId;
	char name[SCE_UID_NAMELEN + 1];
	SceUInt32 attr;				/* 0x28 */
	SceInt32 initCount;
	SceInt32 currentCount;
	SceUID currentOwnerId;			/* 0x34 */
	SceUInt32 numWaitThreads;
	SceInt32 ceilingPriority;		/* 0x3c */
} SceKernelMutexInfo;

/* a lightweight mutex's work area, in its owner's memory */
typedef struct _SceKernelLwMutexWork {
	SceInt64 data[4];
} SceKernelLwMutexWork;

typedef struct _SceKernelLwMutexOptParam {
	SceSize size;
} SceKernelLwMutexOptParam;

/* sceKernelGetLwMutexInfoById (0x40) */
typedef struct _SceKernelLwMutexInfo {
	SceSize size;
	SceUID uid;
	char name[SCE_UID_NAMELEN + 1];
	SceUInt32 attr;				/* 0x28 */
	SceKernelLwMutexWork *pWork;
	SceInt32 initCount;
	SceInt32 currentCount;			/* 0x34 */
	SceUID currentOwnerId;
	SceUInt32 numWaitThreads;		/* 0x3c */
} SceKernelLwMutexInfo;

typedef struct _SceKernelCondOptParam {
	SceSize size;
} SceKernelCondOptParam;

/* sceKernelGetCondInfo (0x34) */
typedef struct _SceKernelCondInfo {
	SceSize size;
	SceUID condId;
	char name[SCE_UID_NAMELEN + 1];
	SceUInt32 attr;				/* 0x28 */
	SceUID mutexId;
	SceUInt32 numWaitThreads;		/* 0x30 */
} SceKernelCondInfo;

typedef struct _SceKernelLwCondWork {
	SceInt32 data[4];
} SceKernelLwCondWork;

typedef struct _SceKernelLwCondOptParam {
	SceSize size;
} SceKernelLwCondOptParam;

/* sceKernelGetLwCondInfoById (0x38) */
typedef struct _SceKernelLwCondInfo {
	SceSize size;
	SceUID uid;
	char name[SCE_UID_NAMELEN + 1];
	SceUInt32 attr;				/* 0x28 */
	SceKernelLwCondWork *pWork;
	SceKernelLwMutexWork *pLwMutex;		/* 0x30 */
	SceUInt32 numWaitThreads;
} SceKernelLwCondInfo;

typedef struct _SceKernelRWLockOptParam {
	SceSize size;
} SceKernelRWLockOptParam;

/* sceKernelGetRWLockInfo (0x3c) */
typedef struct _SceKernelRWLockInfo {
	SceSize size;
	SceUID rwLockId;
	char name[SCE_UID_NAMELEN + 1];
	SceUInt32 attr;				/* 0x28 */
	SceInt32 lockCount;
	SceUID writeOwnerId;			/* 0x30 */
	SceUInt32 numReadWaitThreads;
	SceUInt32 numWriteWaitThreads;		/* 0x38 */
} SceKernelRWLockInfo;

typedef struct _SceKernelTimerOptParam {
	SceSize size;
} SceKernelTimerOptParam;

/* sceKernelGetTimerInfo (0x60) */
typedef struct _SceKernelTimerInfo {
	SceSize size;
	SceUID timerId;
	char name[SCE_UID_NAMELEN + 1];
	SceUInt32 attr;				/* 0x28 */
	SceInt32 fActive;
	SceKernelSysClock baseTime;		/* 0x30 */
	SceKernelSysClock currentTime;
	SceKernelSysClock schedule;		/* 0x40 */
	SceKernelSysClock interval;
	SceInt32 type;				/* 0x50 */
	SceInt32 fRepeat;
	SceUInt32 numWaitThreads;
	SceInt32 reserved[1];
} SceKernelTimerInfo;

typedef struct _SceKernelSimpleEventOptParam {
	SceSize size;
} SceKernelSimpleEventOptParam;

/* sceKernelWaitMultipleEvents: one event to wait for, and how each wait ended */
typedef struct _SceKernelWaitEvent {
	SceUID eventId;
	SceUInt32 eventPattern;
} SceKernelWaitEvent;

typedef struct _SceKernelResultEvent {
	SceUID eventId;
	SceInt32 result;
	SceUInt32 resultPattern;
	SceInt32 reserved[1];
	SceUInt64 userData;			/* 0x10 */
} SceKernelResultEvent;

/* sceKernelGetEventInfo (0x40) */
typedef struct _SceKernelEventInfo {
	SceSize size;
	SceUID eventId;
	char name[SCE_UID_NAMELEN + 1];
	SceUInt32 attr;				/* 0x28 */
	SceUInt32 eventPattern;
	SceUInt64 userData;			/* 0x30 */
	SceUInt32 numWaitThreads;
	SceInt32 reserved[1];
} SceKernelEventInfo;

/* one buffer of a vectored message pipe transfer */
typedef struct _SceKernelMsgPipeVector {
	void *pBase;
	SceSize bufSize;
} SceKernelMsgPipeVector;

typedef struct _SceKernelMsgPipeOptParam {
	SceSize size;
	SceUInt32 attr;
	SceInt32 reserved[2];
	SceUInt32 openLimit;			/* 0x10 */
} SceKernelMsgPipeOptParam;

/* sceKernelGetMsgPipeInfo (0x3c) */
typedef struct _SceKernelMsgPipeInfo {
	SceSize size;
	SceUID msgPipeId;
	char name[SCE_UID_NAMELEN + 1];
	SceUInt32 attr;				/* 0x28 */
	SceSize bufferSize;
	SceSize freeSize;			/* 0x30 */
	SceUInt32 numSendWaitThreads;
	SceUInt32 numReceiveWaitThreads;	/* 0x38 */
} SceKernelMsgPipeInfo;

#ifndef __cplusplus	/* C only; g++ 4.8 (C++98) has no static_assert */
_Static_assert(sizeof(SceKernelThreadOptParam) == 0x1c, "SceKernelThreadOptParam size");
_Static_assert(sizeof(SceKernelThreadOptParamV1) == 8, "SceKernelThreadOptParamV1 size");
_Static_assert(sizeof(SceKernelThreadInfo) == 0x80, "SceKernelThreadInfo size");
_Static_assert(sizeof(SceKernelThreadRunStatus) == 0x34, "SceKernelThreadRunStatus size");
_Static_assert(sizeof(SceKernelSystemInfo) == 0x48, "SceKernelSystemInfo size");
_Static_assert(sizeof(SceKernelCallbackInfo) == 0x44, "SceKernelCallbackInfo size");
_Static_assert(sizeof(SceKernelEventFlagInfo) == 0x38, "SceKernelEventFlagInfo size");
_Static_assert(sizeof(SceKernelSemaInfo) == 0x3c, "SceKernelSemaInfo size");
_Static_assert(sizeof(SceKernelMutexInfo) == 0x40, "SceKernelMutexInfo size");
_Static_assert(sizeof(SceKernelLwMutexWork) == 0x20, "SceKernelLwMutexWork size");
_Static_assert(sizeof(SceKernelLwMutexInfo) == 0x40, "SceKernelLwMutexInfo size");
_Static_assert(sizeof(SceKernelCondInfo) == 0x34, "SceKernelCondInfo size");
_Static_assert(sizeof(SceKernelLwCondWork) == 0x10, "SceKernelLwCondWork size");
_Static_assert(sizeof(SceKernelLwCondInfo) == 0x38, "SceKernelLwCondInfo size");
_Static_assert(sizeof(SceKernelRWLockInfo) == 0x3c, "SceKernelRWLockInfo size");
_Static_assert(sizeof(SceKernelTimerInfo) == 0x60, "SceKernelTimerInfo size");
_Static_assert(sizeof(SceKernelWaitEvent) == 8, "SceKernelWaitEvent size");
_Static_assert(sizeof(SceKernelResultEvent) == 0x18, "SceKernelResultEvent size");
_Static_assert(sizeof(SceKernelEventInfo) == 0x40, "SceKernelEventInfo size");
_Static_assert(sizeof(SceKernelMsgPipeVector) == 8, "SceKernelMsgPipeVector size");
_Static_assert(sizeof(SceKernelMsgPipeOptParam) == 0x14, "SceKernelMsgPipeOptParam size");
_Static_assert(sizeof(SceKernelMsgPipeInfo) == 0x3c, "SceKernelMsgPipeInfo size");
#endif

/* NID 0x9DCB4B7A (vitasdk: ksceKernelGetProcessId); our NID db names it this way */
SceUID sceKernelGetProcessId(void);

int ksceKernelChangeThreadCpuAffinityMask(SceUID uid, uint32_t mask);
int ksceKernelGetThreadCpuAffinityMask(SceUID thid);

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
int sceKernelSetPMUSERENR(SceUID thid, SceUInt32 value);
int pmonCpuSetEventSelf(SceUID threadId, uint32_t counter, uint32_t eventType);

/* Names unknown: per-thread ARM performance counters. */
int SceThreadmgrForKernel_7F831213(SceUID threadId, uint32_t mask);
int SceThreadmgrForKernel_1D2A6815(SceUID threadId, uint32_t mask);
int SceThreadmgrForKernel_7B3368F1(SceUID threadId, uint32_t counter, uint32_t value);
int SceThreadmgrForKernel_170F69D6(SceUID threadId, uint32_t counter, uint32_t *value);

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
SceUID ksceKernelCreateSimpleEvent(const char *name, SceUInt32 attr, uint32_t initPattern,
				   const SceKernelSimpleEventOptParam *option);
/* notifyId: what the callback is notified with */
int ksceKernelRegisterCallbackToEvent(SceUID eventId, SceUID callbackId, uint32_t notifyId);
SceInt32 ksceKernelUnregisterCallbackFromEvent(SceUID eventId, SceUID callbackId);

/* Wait-context query and idle entry registration, recovered from ThreadMgr. */
int sceKernelCheckWaitableStatus(void);
void SceThreadmgrForDriver_B4A05763(void (*idleEntry)(void));

/* NID 0xB7DF3EDF: the kernel side of libkernel's sceKernelSetEventWithNotifyCallback. */
int sceKernelSetEventWithNotifyCallback(SceUID eventId, uint32_t pattern, uint64_t userData,
					uint32_t arg);

/* Timers (SceThreadmgrForDriver; the non-k names are Sony's). */
SceUID sceKernelCreateTimer(const char *name, SceUInt32 attr, const SceKernelTimerOptParam *option);
int sceKernelSetTimerEvent(SceUID timerId, SceUInt32 type, SceUInt64 *interval, int repeat);
int ksceKernelStartTimer(SceUID timer);
int ksceKernelStopTimer(SceUID timer);
int SceThreadmgrForDriver_91A8F38B(SceUID timer);	/* delete timer */
/* calls handler(arg) once, usec from now, in interrupt context */
SceUID ksceKernelRegisterTimer(const char *name, SceUInt32 usec, SceUInt32 (*handler)(void *arg),
			       void *arg);

int sceKernelDelayThread(SceUInt32 usec);
SceUInt64 sceKernelGetSystemTimeWide(void);
int ksceKernelWaitEvent(SceUID eventId, uint32_t pattern, uint32_t *outPattern,
			uint64_t *outUserData,
			SceUInt32 *timeout);

/* thread-local slots (names unknown) */
/* allocate a TLS slot */
int SceThreadmgrForDriver_B645C7EF(uint32_t size, void (*destructor)(void *memory, uintptr_t argument),
				   uintptr_t argument);
int SceThreadmgrForDriver_8C3C1F78(uint32_t slot);			/* free TLS slot */
void *SceThreadmgrForDriver_3A72C6D8(uint32_t slot);		/* the current thread's TLS slot */

int SceThreadmgrForDriver_1096042E(SceUID uid);
int SceThreadmgrForDriver_332E127C(void);
int SceThreadmgrForDriver_5311CFB5(uintptr_t hook, uintptr_t arg);
SceUID SceThreadmgrForDriver_FD6150D5(const char *name, SceUID target, SceUID pid, uint32_t mask, uintptr_t handler, uint32_t arg);
int SceThreadmgrForKernel_15D6357C(SceUID threadId, int priority);
int SceThreadmgrForKernel_16D1961A(void);
int SceThreadmgrForKernel_490DAF14(SceUID threadId);
void *SceThreadmgrForKernel_565BD2DA(void);
void SceThreadmgrForKernel_8D53CA9B(unsigned int cpuId);
int SceThreadmgrForKernel_9928DFBA(SceUID threadId, int priority);
int SceThreadmgrForKernel_CBADD6B3(SceUID threadId, SceSize argSize, const void *arg);
int ksceKernelClearEvent(SceUID eventId, uint32_t keep);
int ksceKernelPulseEvent(SceUID eventId, uint32_t pattern, uint64_t userData);
int ksceKernelPulseEventWithNotifyCallback(SceUID eventId, uint32_t pattern, uint64_t userData, uint32_t arg);
int ksceKernelSetEvent(SceUID eventId, uint32_t pattern, uint64_t userData);
int ksceKernelWaitEventCB(SceUID eventId, uint32_t pattern, uint32_t *outPattern, uint64_t *outUserData, SceUInt32 *timeout);
int sceKernelCancelAlarm(SceUID alarmId);
int sceKernelCancelEventFlag(SceUID evfId, uint32_t setPattern, int *numWaitThreads);
int sceKernelCancelSema(SceUID semaId, int setCount, int *numWaitThreads);
int sceKernelCpuScanThreadForProcessmgr(SceUID processId);
int sceKernelDeleteSimpleEvent(SceUID simpleEventId);
int sceKernelExitDeleteThread(int status);
int sceKernelGetEventPattern(SceUID eventId, uint32_t *pattern);
int sceKernelGetThreadExitStatus(SceUID uid, int *status);
int sceKernelSetSyslibtraceThreadCBEnable(int enable);
