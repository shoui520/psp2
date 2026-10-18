/*
 * kernel/threadmgr.h: the thread manager's constants and types, under Sony's names and layouts
 * (SDK Release 03.570.011, target/include/kernel/threadmgr.h; the kernel's DWARF names the header
 * at sys/sdk/target/include/kernel/threadmgr.h): what the kernel fills for its callers and takes
 * from them.
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

#ifdef __cplusplus
}
#endif
