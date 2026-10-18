/*
 * kernel/threadmgr.h: the thread manager's constants and types, under Sony's names and layouts
 * (SDK Release 03.570.011, target/include/kernel/threadmgr.h; the kernel's DWARF names the header
 * at sys/sdk/target/include/kernel/threadmgr.h): what the kernel fills for its callers and takes
 * from them, and the user library's functions.
 */

#pragma once

#include <scetypes.h>
#include <kernel/constant.h>
#include <kernel/types.h>

#ifdef __cplusplus
extern "C" {
#endif

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

#ifndef PSP2_KERNEL
/* sceKernelCreateThread's option; a kernel unit has the kernel's longer one (threadmgr/threadmgr.h) */
typedef struct _SceKernelThreadOptParam {
	SceSize size;
	SceUInt32 attr;
} SceKernelThreadOptParam;
#endif

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

#ifndef PSP2_KERNEL
/*
 * The user library's thread manager (SceLibKernel and its syscalls), Sony's prototypes. A kernel
 * unit has the kernel's functions of these names instead (threadmgr/threadmgr.h).
 */
SceUID sceKernelCreateThread(const char *pName, SceKernelThreadEntry entry, SceInt32 initPriority,
			     SceSize stackSize, SceUInt32 attr, SceInt32 cpuAffinityMask,
			     const SceKernelThreadOptParam *pOptParam);
SceInt32 sceKernelStartThread(SceUID threadId, SceSize argSize, const void *pArgBlock);
SceInt32 sceKernelExitThread(SceInt32 exitStatus);
SceInt32 sceKernelDeleteThread(SceUID threadId);
SceInt32 sceKernelExitDeleteThread(SceInt32 exitStatus);
SceSize sceKernelCheckThreadStack(void);
SceInt32 sceKernelCallWithChangeStack(void *pBase, SceSize size,
				      SceKernelChangeStackFunction changeStackFunc, void *pCommon);
SceInt32 sceKernelChangeThreadCpuAffinityMask(SceUID threadId, SceInt32 cpuAffinityMask);
SceInt32 sceKernelGetThreadCpuAffinityMask(SceUID threadId);
SceInt32 sceKernelChangeThreadPriority(SceUID threadId, SceInt32 priority);
SceInt32 sceKernelChangeThreadPriority2(SceUID threadId, SceInt32 priority);
SceInt32 sceKernelGetThreadCurrentPriority(void);
SceUID sceKernelGetThreadId(void);
SceInt32 sceKernelChangeCurrentThreadAttr(SceUInt32 clearAttr, SceUInt32 setAttr);
SceInt32 sceKernelGetThreadExitStatus(SceUID threadId, SceInt32 *pExitStatus);
SceUID sceKernelGetProcessId(void);
SceInt32 sceKernelCheckWaitableStatus(void);
SceInt32 sceKernelGetThreadInfo(SceUID threadId, SceKernelThreadInfo *pInfo);
SceInt32 sceKernelGetThreadRunStatus(SceKernelThreadRunStatus *pStatus);
SceInt32 sceKernelGetSystemInfo(SceKernelSystemInfo *pInfo);
SceInt32 sceKernelGetThreadmgrUIDClass(SceUID uid);
SceInt32 sceKernelChangeThreadVfpException(SceInt32 clearMask, SceInt32 setMask);
SceInt32 sceKernelGetCurrentThreadVfpException(void);
SceInt32 sceKernelDelayThread(SceUInt32 usec);
SceInt32 sceKernelDelayThreadCB(SceUInt32 usec);
SceInt32 sceKernelWaitThreadEnd(SceUID threadId, SceInt32 *pExitStatus, SceUInt32 *pTimeout);
SceInt32 sceKernelWaitThreadEndCB(SceUID threadId, SceInt32 *pExitStatus, SceUInt32 *pTimeout);
SceUID sceKernelCreateCallback(const char *pName, SceUInt32 attr,
			       SceKernelCallbackFunction callbackFunc, void *pCommon);
SceInt32 sceKernelDeleteCallback(SceUID callbackId);
SceInt32 sceKernelNotifyCallback(SceUID callbackId, SceInt32 notifyArg);
SceInt32 sceKernelCancelCallback(SceUID callbackId);
SceInt32 sceKernelGetCallbackCount(SceUID callbackId);
SceInt32 sceKernelCheckCallback(void);
SceInt32 sceKernelGetCallbackInfo(SceUID callbackId, SceKernelCallbackInfo *pInfo);
SceInt32 sceKernelRegisterCallbackToEvent(SceUID eventId, SceUID callbackId);
SceInt32 sceKernelUnregisterCallbackFromEvent(SceUID eventId, SceUID callbackId);
SceInt32 sceKernelUnregisterCallbackFromEventAll(SceUID eventId);
SceInt32 sceKernelWaitEvent(SceUID eventId, SceUInt32 waitPattern, SceUInt32 *pResultPattern,
			    SceUInt64 *pUserData, SceUInt32 *pTimeout);
SceInt32 sceKernelWaitEventCB(SceUID eventId, SceUInt32 waitPattern, SceUInt32 *pResultPattern,
			      SceUInt64 *pUserData, SceUInt32 *pTimeout);
SceInt32 sceKernelPollEvent(SceUID eventId, SceUInt32 bitPattern, SceUInt32 *pResultPattern,
			    SceUInt64 *pUserData);
SceInt32 sceKernelWaitMultipleEvents(SceKernelWaitEvent *pWaitEventList, SceUInt32 numEvents,
				     SceUInt32 waitMode, SceKernelResultEvent *pResultEventList,
				     SceUInt32 *pTimeout);
SceInt32 sceKernelWaitMultipleEventsCB(SceKernelWaitEvent *pWaitEventList, SceUInt32 numEvents,
				       SceUInt32 waitMode, SceKernelResultEvent *pResultEventList,
				       SceUInt32 *pTimeout);
SceInt32 sceKernelSetEvent(SceUID eventId, SceUInt32 setPattern, SceUInt64 userData);
SceInt32 sceKernelSetEventWithNotifyCallback(SceUID eventId, SceUInt32 setPattern,
					     SceUInt64 userData, SceInt32 notifyArg);
SceInt32 sceKernelPulseEvent(SceUID eventId, SceUInt32 pulsePattern, SceUInt64 userData);
SceInt32 sceKernelPulseEventWithNotifyCallback(SceUID eventId, SceUInt32 pulsePattern,
					       SceUInt64 userData, SceInt32 notifyArg);
SceInt32 sceKernelClearEvent(SceUID eventId, SceUInt32 clearPattern);
SceInt32 sceKernelCancelEventWithSetPattern(SceUID eventId, SceUInt32 setPattern,
					    SceUInt64 userData, SceUInt32 *pNumWaitThreads);
SceInt32 sceKernelGetEventPattern(SceUID eventId, SceUInt32 *pPattern);
SceInt32 sceKernelCancelEvent(SceUID eventId, SceUInt32 *pNumWaitThreads);
SceInt32 sceKernelGetEventInfo(SceUID eventId, SceKernelEventInfo *pInfo);
SceUID sceKernelCreateEventFlag(const char *pName, SceUInt32 attr, SceUInt32 initPattern,
				const SceKernelEventFlagOptParam *pOptParam);
SceInt32 sceKernelDeleteEventFlag(SceUID evfId);
SceUID sceKernelOpenEventFlag(const char *pName);
SceInt32 sceKernelCloseEventFlag(SceUID evfId);
SceInt32 sceKernelWaitEventFlag(SceUID evfId, SceUInt32 bitPattern, SceUInt32 waitMode,
				SceUInt32 *pResultPat, SceUInt32 *pTimeout);
SceInt32 sceKernelWaitEventFlagCB(SceUID evfId, SceUInt32 bitPattern, SceUInt32 waitMode,
				  SceUInt32 *pResultPat, SceUInt32 *pTimeout);
SceInt32 sceKernelPollEventFlag(SceUID evfId, SceUInt32 bitPattern, SceUInt32 waitMode,
				SceUInt32 *pResultPat);
SceInt32 sceKernelSetEventFlag(SceUID evfId, SceUInt32 bitPattern);
SceInt32 sceKernelClearEventFlag(SceUID evfId, SceUInt32 bitPattern);
SceInt32 sceKernelCancelEventFlag(SceUID evfId, SceUInt32 setPattern, SceUInt32 *pNumWaitThreads);
SceInt32 sceKernelGetEventFlagInfo(SceUID evfId, SceKernelEventFlagInfo *pInfo);
SceUID sceKernelCreateSema(const char *pName, SceUInt32 attr, SceInt32 initCount, SceInt32 maxCount,
			   const SceKernelSemaOptParam *pOptParam);
SceInt32 sceKernelDeleteSema(SceUID semaId);
SceUID sceKernelOpenSema(const char *pName);
SceInt32 sceKernelCloseSema(SceUID semaId);
SceInt32 sceKernelWaitSema(SceUID semaId, SceInt32 needCount, SceUInt32 *pTimeout);
SceInt32 sceKernelWaitSemaCB(SceUID semaId, SceInt32 needCount, SceUInt32 *pTimeout);
SceInt32 sceKernelPollSema(SceUID semaId, SceInt32 needCount);
SceInt32 sceKernelSignalSema(SceUID semaId, SceInt32 signalCount);
SceInt32 sceKernelCancelSema(SceUID semaId, SceInt32 setCount, SceUInt32 *pNumWaitThreads);
SceInt32 sceKernelGetSemaInfo(SceUID semaId, SceKernelSemaInfo *pInfo);
SceUID sceKernelCreateMutex(const char *pName, SceUInt32 attr, SceInt32 initCount,
			    const SceKernelMutexOptParam *pOptParam);
SceInt32 sceKernelDeleteMutex(SceUID mutexId);
SceUID sceKernelOpenMutex(const char *pName);
SceInt32 sceKernelCloseMutex(SceUID mutexId);
SceInt32 sceKernelLockMutex(SceUID mutexId, SceInt32 lockCount, SceUInt32 *pTimeout);
SceInt32 sceKernelLockMutexCB(SceUID mutexId, SceInt32 lockCount, SceUInt32 *pTimeout);
SceInt32 sceKernelTryLockMutex(SceUID mutexId, SceInt32 lockCount);
SceInt32 sceKernelUnlockMutex(SceUID mutexId, SceInt32 unlockCount);
SceInt32 sceKernelCancelMutex(SceUID mutexId, SceInt32 newCount, SceUInt32 *pNumWaitThreads);
SceInt32 sceKernelGetMutexInfo(SceUID mutexId, SceKernelMutexInfo *pInfo);
SceInt32 sceKernelCreateLwMutex(SceKernelLwMutexWork *pWork, const char *pName, SceUInt32 attr,
				SceInt32 initCount, const SceKernelLwMutexOptParam *pOptParam);
SceInt32 sceKernelDeleteLwMutex(SceKernelLwMutexWork *pWork);
SceInt32 sceKernelLockLwMutex(SceKernelLwMutexWork *pWork, SceInt32 lockCount, SceUInt32 *pTimeout);
SceInt32 sceKernelLockLwMutexCB(SceKernelLwMutexWork *pWork, SceInt32 lockCount,
				SceUInt32 *pTimeout);
SceInt32 sceKernelTryLockLwMutex(SceKernelLwMutexWork *pWork, SceInt32 lockCount);
SceInt32 sceKernelUnlockLwMutex(SceKernelLwMutexWork *pWork, SceInt32 unlockCount);
SceInt32 sceKernelGetLwMutexInfo(SceKernelLwMutexWork *pWork, SceKernelLwMutexInfo *pInfo);
SceInt32 sceKernelGetLwMutexInfoById(SceUID lwMutexId, SceKernelLwMutexInfo *pInfo);
SceUID sceKernelCreateCond(const char *pName, SceUInt32 attr, SceUID mutexId,
			   const SceKernelCondOptParam *pOptParam);
SceInt32 sceKernelDeleteCond(SceUID condId);
SceInt32 sceKernelWaitCond(SceUID condId, SceUInt32 *pTimeout);
SceInt32 sceKernelSignalCond(SceUID condId);
SceInt32 sceKernelSignalCondAll(SceUID condId);
SceInt32 sceKernelSignalCondTo(SceUID condId, SceUID threadId);
SceInt32 sceKernelGetCondInfo(SceUID condId, SceKernelCondInfo *pInfo);
SceInt32 sceKernelCreateLwCond(SceKernelLwCondWork *pWork, const char *pName, SceUInt32 attr,
			       SceKernelLwMutexWork *pLwMutex,
			       const SceKernelLwCondOptParam *pOptParam);
SceInt32 sceKernelDeleteLwCond(SceKernelLwCondWork *pWork);
SceInt32 sceKernelWaitLwCond(SceKernelLwCondWork *pWork, SceUInt32 *pTimeout);
SceInt32 sceKernelSignalLwCond(SceKernelLwCondWork *pWork);
SceInt32 sceKernelSignalLwCondAll(SceKernelLwCondWork *pWork);
SceInt32 sceKernelSignalLwCondTo(SceKernelLwCondWork *pWork, SceUID threadId);
SceInt32 sceKernelGetLwCondInfo(SceKernelLwCondWork *pWork, SceKernelLwCondInfo *pInfo);
SceInt32 sceKernelGetLwCondInfoById(SceUID lwCondId, SceKernelLwCondInfo *pInfo);
SceUID sceKernelCreateRWLock(const char *pName, SceUInt32 attr,
			     const SceKernelRWLockOptParam *pOptParam);
SceInt32 sceKernelDeleteRWLock(SceUID rwLockId);
SceUID sceKernelOpenRWLock(const char *pName);
SceInt32 sceKernelCloseRWLock(SceUID rwLockId);
SceInt32 sceKernelLockReadRWLock(SceUID rwLockId, SceUInt32 *pTimeout);
SceInt32 sceKernelLockReadRWLockCB(SceUID rwLockId, SceUInt32 *pTimeout);
SceInt32 sceKernelTryLockReadRWLock(SceUID rwLockId);
SceInt32 sceKernelUnlockReadRWLock(SceUID rwLockId);
SceInt32 sceKernelLockWriteRWLock(SceUID rwLockId, SceUInt32 *pTimeout);
SceInt32 sceKernelLockWriteRWLockCB(SceUID rwLockId, SceUInt32 *pTimeout);
SceInt32 sceKernelTryLockWriteRWLock(SceUID rwLockId);
SceInt32 sceKernelUnlockWriteRWLock(SceUID rwLockId);
SceInt32 sceKernelCancelRWLock(SceUID rwLockId, SceUInt32 *pNumReadWaitThreads,
			       SceUInt32 *pNumWriteWaitThreads, SceInt32 flag);
SceInt32 sceKernelGetRWLockInfo(SceUID rwLockId, SceKernelRWLockInfo *pInfo);
SceUID sceKernelCreateTimer(const char *pName, SceUInt32 attr,
			    const SceKernelTimerOptParam *pOptParam);
SceInt32 sceKernelDeleteTimer(SceUID timerId);
SceUID sceKernelOpenTimer(const char *pName);
SceInt32 sceKernelCloseTimer(SceUID timerId);
SceInt32 sceKernelStartTimer(SceUID timerId);
SceInt32 sceKernelStopTimer(SceUID timerId);
SceInt32 sceKernelGetTimerBase(SceUID timerId, SceKernelSysClock *pBase);
SceUInt64 sceKernelGetTimerBaseWide(SceUID timerId);
SceInt32 sceKernelGetTimerTime(SceUID timerId, SceKernelSysClock *pClock);
SceUInt64 sceKernelGetTimerTimeWide(SceUID timerId);
SceInt32 sceKernelSetTimerTime(SceUID timerId, SceKernelSysClock *pClock);
SceUInt64 sceKernelSetTimerTimeWide(SceUID timerId, SceUInt64 clock);
SceInt32 sceKernelSetTimerEvent(SceUID timerId, SceInt32 type, SceKernelSysClock *pInterval,
				SceInt32 fRepeat);
SceInt32 sceKernelCancelTimer(SceUID timerId, SceUInt32 *pNumWaitThreads);
SceInt32 sceKernelGetTimerInfo(SceUID timerId, SceKernelTimerInfo *pInfo);
SceInt32 sceKernelGetTimerEventRemainingTime(SceUID timerId, SceKernelSysClock *pClock);
SceUID sceKernelCreateSimpleEvent(const char *pName, SceUInt32 attr, SceUInt32 initPattern,
				  const SceKernelSimpleEventOptParam *pOptParam);
SceInt32 sceKernelDeleteSimpleEvent(SceUID simpleEventId);
SceUID sceKernelOpenSimpleEvent(const char *pName);
SceInt32 sceKernelCloseSimpleEvent(SceUID simpleEventId);
SceUID sceKernelCreateMsgPipe(const char *pName, SceUInt32 type, SceUInt32 attr, SceSize bufSize,
			      const SceKernelMsgPipeOptParam *pOptParam);
SceInt32 sceKernelDeleteMsgPipe(SceUID msgPipeId);
SceUID sceKernelOpenMsgPipe(const char *pName);
SceInt32 sceKernelCloseMsgPipe(SceUID msgPipeId);
SceInt32 sceKernelSendMsgPipe(SceUID msgPipeId, const void *pSendBuf, SceSize sendSize,
			      SceUInt32 waitMode, SceSize *pResult, SceUInt32 *pTimeout);
SceInt32 sceKernelSendMsgPipeCB(SceUID msgPipeId, const void *pSendBuf, SceSize sendSize,
				SceUInt32 waitMode, SceSize *pResult, SceUInt32 *pTimeout);
SceInt32 sceKernelTrySendMsgPipe(SceUID msgPipeId, const void *pSendBuf, SceSize sendSize,
				 SceUInt32 waitMode, SceSize *pResult);
SceInt32 sceKernelSendMsgPipeVector(SceUID msgPipeId, const SceKernelMsgPipeVector *pVector,
				    SceUInt32 numVector, SceUInt32 waitMode, SceSize *pResult,
				    SceUInt32 *pTimeout);
SceInt32 sceKernelSendMsgPipeVectorCB(SceUID msgPipeId, const SceKernelMsgPipeVector *pVector,
				      SceUInt32 numVector, SceUInt32 waitMode, SceSize *pResult,
				      SceUInt32 *pTimeout);
SceInt32 sceKernelTrySendMsgPipeVector(SceUID msgPipeId, const SceKernelMsgPipeVector *pVector,
				       SceUInt32 numVector, SceUInt32 waitMode, SceSize *pResult);
SceInt32 sceKernelReceiveMsgPipe(SceUID msgPipeId, void *pRecvBuf, SceSize recvSize,
				 SceUInt32 waitMode, SceSize *pResult, SceUInt32 *pTimeout);
SceInt32 sceKernelReceiveMsgPipeCB(SceUID msgPipeId, void *pRecvBuf, SceSize recvSize,
				   SceUInt32 waitMode, SceSize *pResult, SceUInt32 *pTimeout);
SceInt32 sceKernelTryReceiveMsgPipe(SceUID msgPipeId, void *pRecvBuf, SceSize recvSize,
				    SceUInt32 waitMode, SceSize *pResult);
SceInt32 sceKernelReceiveMsgPipeVector(SceUID msgPipeId, const SceKernelMsgPipeVector *pVector,
				       SceUInt32 numVector, SceUInt32 waitMode, SceSize *pResult,
				       SceUInt32 *pTimeout);
SceInt32 sceKernelReceiveMsgPipeVectorCB(SceUID msgPipeId, const SceKernelMsgPipeVector *pVector,
					 SceUInt32 numVector, SceUInt32 waitMode, SceSize *pResult,
					 SceUInt32 *pTimeout);
SceInt32 sceKernelTryReceiveMsgPipeVector(SceUID msgPipeId, const SceKernelMsgPipeVector *pVector,
					  SceUInt32 numVector, SceUInt32 waitMode,
					  SceSize *pResult);
SceInt32 sceKernelCancelMsgPipe(SceUID msgPipeId, SceUInt32 *pNumSendWaitThreads,
				SceUInt32 *pNumReceiveWaitThreads);
SceInt32 sceKernelGetMsgPipeInfo(SceUID msgPipeId, SceKernelMsgPipeInfo *pInfo);
#endif

#ifdef __cplusplus
}
#endif
