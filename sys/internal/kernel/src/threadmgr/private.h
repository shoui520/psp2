/* SceKernelThreadMgr's private layouts */
#pragma once

#include <stddef.h>
#include <stdint.h>
#include <psp2kern/types.h>
#include <kernel/error.h>
#include <threadmgr/threadcb.h>
#include <threadmgr/threadmgr_debugger.h>
#include <threadmgr/threadmgr_vfp.h>
#include <threadmgr/threadmgr_callback.h>
#include <processmgr/process.h>
#include <sysmem/sysmem.h>
#include <systimer/timer_regs.h>
#include <excpmgr/excpmgr.h>

#include "private_errors.h"
#include <sysmem/sysroot.h>

/* a PUID attribute: the PUID came from an open, not a create */
#define PUID_ATTR_OPENED		0x00080000

/* The PMU access an SGI carries to the CPU the thread runs on. */
/*
 * The debugger's handlers (SceThreadmgrForDriver_6C2E3A49): group 0 is told
 * of an event, group 1 decides what happens to the thread.
 */
typedef int (*ThreadmgrDebugHandler)(SceSize size, void *message);

#define DEBUG_HANDLER_THREAD_EVENT	0
#define DEBUG_HANDLER_VFP_EXCEPTION	1
#define DEBUG_HANDLER_GPU_EXCEPTION	2
#define DEBUG_HANDLER_STACK_OVERFLOW	3	/* and DEBUG_SUSPEND_USER_STACK */
#define DEBUG_HANDLER_EXIT_CALLBACK	4
#define DEBUG_HANDLER_CRITICAL_ERROR	5
#define DEBUG_HANDLER_UNK80000		6
#define DEBUG_HANDLER_UNK100000		7

/* why a thread is debug-suspended (ThreadCB.debugSuspendReason) */
#define DEBUG_SUSPEND_PROCESS		0x00000002	/* PLACEHOLDER: name */
#define DEBUG_SUSPEND_VFP_EXCEPTION	0x00000010
#define DEBUG_SUSPEND_USER_STACK	0x00010000	/* PLACEHOLDER: name; every wait checks it */
#define DEBUG_SUSPEND_EXIT_CALLBACK	0x00020000	/* PLACEHOLDER: name */
#define DEBUG_SUSPEND_CRITICAL_ERROR	0x00040000
#define DEBUG_SUSPEND_UNK80000		0x00080000	/* PLACEHOLDER: name */
#define DEBUG_SUSPEND_UNK100000		0x00100000	/* PLACEHOLDER: name; the kind comes with it */

int debugSuspendThreadSelf(uint32_t reason, SceSize size, void *message);

#define SDK_VERSION_KERNEL_THREAD	0x04000000	/* kernel threads count as the newest SDK */

/* what the thread event handlers (DEBUG_HANDLER_THREAD_EVENT) hear */
struct ThreadmgrDebugThreadEvent {
	SceUID processId;
	SceUID threadId;			/* the user thread ID */
	uint32_t event;				/* DEBUG_THREAD_EVENT_* */
	uint32_t reserved;
	int status;				/* 0x10: DEBUG_THREAD_EVENT_EXIT only */
};

struct ThreadmgrPmonRequest {
	SceUID threadId;
	uint32_t counter;
	uint32_t value;			/* get: the uint32_t * to store into */
	int *result;
	SceKernelSpinlock *done;		/* held until the target CPU answers */
};

struct ThreadmgrCallback;

/* a stack sceKernelExtendKernelStackWide added (freed with the thread) */
struct ThreadmgrExtendedStack {
	struct ThreadmgrExtendedStack *next;
	uintptr_t stackBase;			/* the stack it replaced */
	SceUID memBlockId;			/* 0x08 */
	SceSize stackSize;
};
struct ThreadmgrWaitNode;
struct ThreadmgrException;
struct ThreadmgrExceptionEntry;

struct ThreadmgrFastMutex;

/* SceUIDExceptionObject (0x38) */
struct ThreadmgrException {
	SceUID ownerPid;
	void *cls;
	void *uidEntry;				/* 0x08: the GUID entry */
	void *cacheNext;			/* 0x0c: the next cached object; &uidEntry while in use */
	struct ThreadmgrWaitNode *next;		/* 0x10: its waiter */
	struct ThreadmgrWaitNode *prev;
	SceKernelSpinlock lock;			/* 0x18 */
	uint32_t waiting;			/* 0x1c */
	uint32_t attr;				/* 0x20 */
	uint32_t reserved;			/* 0x24: zeroed */
	struct ThreadmgrExceptionEntry *pendingNext;	/* 0x28: the exceptions raised */
	struct ThreadmgrExceptionEntry *pendingPrev;
	SceUID owner;				/* 0x30: the thread waiting on it, or -1 */
	int waiters;				/* 0x34: its waits in progress */
};

/* an exception a thread raised, until the VM takes it */
struct ThreadmgrExceptionEntry {
	struct ThreadmgrExceptionEntry *next;
	struct ThreadmgrExceptionEntry *prev;
	struct ThreadCB *thread;	/* 0x08 */
	uint32_t reserved0[2];
	uint32_t value;				/* 0x14 */
	uint32_t kind;				/* 0x18: matched against the wait's mask */
	uint32_t reserved1;
	uint32_t value2;			/* 0x20: with kind 1 */
};

/* waitException's report (0x18) */
typedef struct ThreadmgrExceptionInfo {
	SceSize size;
	uint32_t value;
	uint32_t kind;
	uint32_t reserved0;
	uint32_t value2;
	uint32_t reserved1;
} ThreadmgrExceptionInfo;

#define DTRACE_THREAD_START		0x00000004	/* __proc_:lwp__start pending */
#define DTRACE_PROCESS_START		0x00000004	/* __proc_:start pending */

/* a thread context's spinlock */
#define THREAD_LOCK(thread)	(&(thread)->lock)

/* a callback's control block (SceUIDCallbackObject +8), in its thread's callbacks */
struct ThreadmgrCallbackCB {
	struct ThreadmgrCallbackCB *next;
	struct ThreadmgrCallbackCB *prev;
	SceUID threadId;			/* 0x08 */
	SceUID guid;				/* 0x0c */
	SceUID puid;				/* 0x10: -1 for a kernel callback */
	uint32_t attr;				/* 0x14: THREAD_ATTR_UNLIMITED_OBJECTS of its creator */
	uint32_t deleted;			/* 0x18 */
	uint32_t user;				/* 0x1c: runs in user mode */
	uintptr_t entry;			/* 0x20 */
	uint32_t notifyId;			/* 0x24 */
	uint32_t notifyCount;			/* 0x28 */
	uint32_t notifyArg;			/* 0x2c */
	void *common;				/* 0x30 */
	int running;				/* 0x34: calls in progress */
};

/* SceUIDCallbackObject (0x40) */
struct ThreadmgrCallback {
	SceUID ownerPid;			/* 0x00: the object base's first word */
	void *cls;
	struct ThreadmgrCallbackCB cb;		/* 0x08 */
};

/* Intr-request payload layout is known only from individual consumers. */
struct ThreadmgrIntrRequest {
	struct ThreadmgrIntrRequest *volatile next;
	volatile uint32_t kind;
	volatile uint32_t fromPool;		/* 0: from the heap, freed when done */
	uint32_t reserved;
	uint32_t payload[4];
};

/* ThreadmgrIntrRequest.kind: one per issue*IntrRequest (names from the 0.990 prototype's) */
enum ThreadmgrIntrRequestKind {
	INTR_REQUEST_CHANGE_THREAD_CPU_AFFINITY		= 0x00,
	INTR_REQUEST_CHANGE_THREAD_PRIORITY		= 0x01,
	INTR_REQUEST_CHANGE_THREAD_PRIORITY_AFFINITY	= 0x02,
	INTR_REQUEST_ACKNOWLEDGE			= 0x03,	/* issueSimpleIntrRequestToOtherCpuAndWait */
	INTR_REQUEST_DEBUG_SUSPEND_THREAD		= 0x08,
	INTR_REQUEST_STOP_THREAD			= 0x09,
	INTR_REQUEST_SUSPEND_THREAD_FOR_VM		= 0x0a,
	INTR_REQUEST_SET_CPU_REGISTER			= 0x0b,
	INTR_REQUEST_GET_CPU_REGISTER			= 0x0c,
	INTR_REQUEST_SET_VFP_REGISTER			= 0x0d,
	INTR_REQUEST_GET_VFP_REGISTER			= 0x0e,
	INTR_REQUEST_PMON_THREAD_CHANGE_COUNTER_ENABLE	= 0x10,
	INTR_REQUEST_PMON_THREAD_SET_COUNTER		= 0x11,
	INTR_REQUEST_PMON_THREAD_GET_COUNTER		= 0x12,
	INTR_REQUEST_CALL_PA_SCHEDULE_HOOK		= 0x13,
	INTR_REQUEST_CALL_PA_PRIORITY_HOOK		= 0x14,
	INTR_REQUEST_DTRACE_ASYNC_XCALL			= 0x18,
	INTR_REQUEST_DTRACE_SYNC_XCALL			= 0x19,
};

/* ThreadMgrCpuCB.intrRequestMask: requests without a payload (addIntrRequest) */
#define INTR_MASK_SIMPLE		0x00000001	/* just interrupt the CPU (SceThreadmgrForKernel_8D53CA9B) */
#define INTR_MASK_SCHEDULING		0x00000002	/* issueSchedulingIntrRequest: choose again */
#define INTR_MASK_SALVAGE		0x00000010	/* a CPU came up: take the stagnant threads it may run */
#define INTR_MASK_DEACTIVATE		0x00000020	/* issueDeactivateCpuIntrRequest */

/* Per-CPU scheduler state begins at the VBAR+0x2c indirection. */
struct ThreadMgrCpuCB {
	uintptr_t runningThread;		/* 0x00: pRunningThreadCB (Sony's dump names) */
	uintptr_t nextRunningThread;		/* 0x04: pNextRunningThreadCB, 0: to be chosen */
	uint32_t disabledDispatch;		/* 0x08: nonzero: dispatching is disabled */
	volatile SceUID runningProcessId;	/* 0x0c */
	volatile SceUID runningThreadId;	/* 0x10 */
	SceUID processId;
	SceUID threadId;
	int priority;
	uintptr_t standbyNext;
	uintptr_t standbyPrev;
	SceKernelSpinlock standbyLock;
	uint32_t threadSwitchCount;		/* 0x2c: reported by SceThreadmgrForDriver_7E280B69 */
	uint32_t remainCpuCount;		/* 0x30: switches that kept the running thread (__sched_:remain__cpu) */
	struct ThreadCB *idleThread;	/* 0x34: PLACEHOLDER: name; never debug-suspended */
	uint64_t lastThreadSwitchTime;		/* 0x38: when the running thread got the CPU */
	struct ThreadmgrIntrRequest *intrRequestHead;
	uint32_t intrRequestMask;
	uintptr_t switchStackTop;		/* 0x48: its page of the switch stack block */
	uint32_t releasingWaitObject;		/* 0x4c: a switch is releasing the old wait's UID */
	uint8_t reserved[0x10];
	uint32_t readyBitmap[8];		/* 0x60: a bit per non-empty ready queue */
	struct {
		uintptr_t next;
		uintptr_t prev;
	} readyQueue[0x100];			/* 0x80: per priority (0x80-0xfb are shared) */
};

/* a kernel TLS slot (SceThreadmgrForDriver_B645C7EF) */
struct ThreadmgrTlsSlot {
	uint32_t active;
	uint32_t liveCount;			/* threads with memory for it */
	SceSize size;
	void (*destructor)(void *memory, uintptr_t argument);
	uintptr_t destructorArgument;
};

struct ThreadmgrTimer;

/* Recovered prefix of Sony's SceKernelThreadmgrPrivate (native size 0x28c8). */
struct SceKernelThreadmgrPrivate {
	struct ThreadMgrCpuCB cpuCB[4];
	SceKernelSpinlock threadMgrCBLock;
	uint32_t activeCpuMask;
	uint32_t lastSelectedCpu;
	SceKernelSpinlock delayCacheLock;	/* 0x220c: free delay objects (timer.c) */
	int delayCacheCount;			/* 0x2210 */
	void *delayCache;			/* 0x2214: linked through cacheNext */
	uintptr_t deletedNext;			/* 0x2218: threads ended and deleted, for the cleaner */
	uintptr_t deletedPrev;
	uint32_t reserved0;			/* 0x2220: zeroed when the cleaner starts */
	SceKernelSpinlock timedWaitLock;	/* 0x2224: the threads waiting with a timeout */
	uintptr_t timedWaitNext;		/* 0x2228: delayed threads */
	uintptr_t timedWaitPrev;
	uintptr_t dormantNext;			/* 0x2230: threads ended, not deleted */
	uintptr_t dormantPrev;
	uintptr_t closedNext;			/* 0x2238: dormant threads deleted, their UIDs closed */
	uintptr_t closedPrev;
	uintptr_t reservedListNext;		/* 0x2240: a list set up empty; no other user found */
	uintptr_t reservedListPrev;
	uintptr_t stagnantNext;			/* 0x2248: threads no active CPU may run */
	uintptr_t stagnantPrev;
	uintptr_t debugSuspendedNext;		/* 0x2250: threads the debugger took off the CPUs */
	uintptr_t debugSuspendedPrev;
	uintptr_t suspendedNext;		/* 0x2258: threads suspended (by a request or a VM) */
	uintptr_t suspendedPrev;
	struct ThreadCB *cleanerThread;	/* 0x2260: never debug-suspended either */
	struct ThreadmgrTlsSlot tlsSlots[8];	/* 0x2264: the kernel TLS slots (2-7 for drivers) */
	SceUID heapId;
	SceUID timerMemBlockId;			/* 0x2308: "SceLT5" */
	volatile SceLongTimerRegisters *systemTime;	/* 0x230c: the LT5 timer */
	SceKernelSpinlock timerLock;		/* 0x2310: the timer queue */
	int timerCpuId;				/* 0x2314: the CPU running a timer's handler, or -1 */
	SceUID timerHandlingId;			/* 0x2318: the timer being handled, or -1 */
	struct ThreadmgrTimer *timerNextToHandle;	/* 0x231c: the handler's cursor */
	struct ThreadmgrTimer *timerQueueNext;	/* 0x2320: the queue, by expiry */
	struct ThreadmgrTimer *timerQueuePrev;
	uint64_t timerCompare;			/* 0x2328: the expiry programmed into the LT5 */
	SceClass *threadClass;			/* 0x2330: "SceUIDThreadClass" */
	SceClass *semaphoreClass;		/* 0x2334 */
	SceClass *eventFlagClass;		/* 0x2338 */
	SceClass *mutexClass;			/* 0x233c */
	SceClass *condClass;			/* 0x2340 */
	SceClass *timerClass;			/* 0x2344: "SceUIDTimerClass" */
	SceClass *msgPipeClass;			/* 0x2348 */
	SceClass *callbackClass;		/* 0x234c */
	SceClass *threadEventClass;		/* 0x2350 */
	SceClass *lwMutexClass;			/* 0x2354 */
	SceClass *lwCondClass;			/* 0x2358 */
	SceClass *rwLockClass;			/* 0x235c */
	SceClass *simpleEventClass;		/* 0x2360 */
	SceClass *exceptionClass;		/* 0x2364 */
	SceClass *fastMutexClass;		/* 0x2368 */
	SceClass *delayClass;			/* 0x236c: "SceUIDDelayClass" */
	SceClass *alarmClass;			/* 0x2370: "SceUIDAlarmClass" */
	SceClass *workQueueClass;
	SceClass *workTaskClass;		/* 0x2378 */
	SceClass *cacheClass;			/* 0x237c: "SceUIDCacheClass" */
	SceClass *waitableClass;		/* 0x2380: "SceUIDWaitableClass" */
	SceClass *registerCallbackClass;	/* 0x2384 */
	SceClass *eventClass;			/* 0x2388: parent of message pipes and CPU timers */
	SceClass *cpuTimerClass;		/* 0x238c: parent of delays, alarms and timers */
	SceClass *processClass;			/* 0x2390: processmgr's (SceThreadmgrForKernel_59D1D5B0) */
	SceUID signalId;			/* 0x2394: the wait ID of a thread in waitSignal */
	struct ThreadmgrWaitableObject *signalObject;	/* 0x2398: its waitable (queue at +0x10) */
	ThreadmgrDebugHandler debugHandlers[2][8];		/* 0x239c: SceThreadmgrForDriver_6C2E3A49 */
	uintptr_t paHooks[3];			/* 0x23dc: SceThreadmgrForDriver_1378F6EF; [0] schedule, [1] priority */
	uintptr_t processDebugSuspendHook;	/* 0x23e8: (pid, threadId, kind, arg) after an exception */
	uintptr_t processDebugSuspendArg;	/* 0x23ec */
	SceUID cbBlockId;			/* 0x23f0: "SceThreadMgrCBBlock", this structure */
	SceUID initialThreadId[4];		/* 0x23f4: "SceKernelInitialThread_%d" */
	SceProcessObject *kernelProcess;	/* 0x2404 */
	void *kernelAddressSpace;		/* 0x2408 */
	void *kernelUidEntryHeap;		/* 0x240c */
	SceUID switchStackBlockId;		/* 0x2410: "SceThreadMgrSwitchStackBlock", a page per CPU */
	struct ThreadmgrIntrRequest *freeIntrRequests;
	uint32_t reserved1;			/* 0x2418 */
	uint32_t reserved2;			/* 0x241c..0x2427: zeroed at boot */
	uint32_t reserved3;
	uint32_t reserved4;
	uintptr_t threadEventHandlerNext;	/* 0x2428 */
	uintptr_t threadEventHandlerPrev;
	SceKernelSpinlock threadEventLock;	/* 0x2430 */
	uint32_t testMode;
	uint32_t syscallRestartSlot;		/* 0x2438: the TLS slot a restarted syscall's state is in */
	SceKernelSpinlock globalReadyLock;	/* 0x243c: the shared ready queues */
	uint32_t globalReadyBitmap[4];		/* 0x2440: a bit per non-empty shared queue */
	struct {
		uintptr_t next;
		uintptr_t prev;
	} globalReadyQueue[0x7c];		/* 0x2450: priorities 0x80-0xfb, any CPU */
	SceUID msgPipeAsyncThreadId[8];		/* 0x2830: "SceKernelMsgPipeAsync%d" */
	SceUID msgPipeAsyncMutex;		/* 0x2850: guards the requests */
	SceUID msgPipeAsyncCond;		/* 0x2854: a request is queued */
	SceUID msgPipeAsyncPid[8];		/* 0x2858: the process each thread serves, or -1 */
	SceUID msgPipeAsyncDoneCond[8];		/* 0x2878: "SceKernelMsgPipeAsyncReqComp" */
	uintptr_t msgPipeAsyncNext;		/* 0x2898: the queued requests */
	uintptr_t msgPipeAsyncPrev;
	uint8_t reserved5[8];			/* 0x28a0 */
	int syslibtraceThreadCBEnable;		/* 0x28a8: SceThreadmgrForKernel_60798313 reads it */
	int dtraceThreadCBEnable;		/* 0x28ac */
	SceUID dtraceHeapId;			/* 0x28b0: "SceDtraceThreadCB" */
	uint8_t reserved6[0xc];			/* 0x28b4 */
	volatile uint32_t cpuStopState;		/* 0x28c0: CPU_STOP_*, then the CPUs stopped */
	uint32_t cpuStopOwner;			/* 0x28c4: set while a CPU stops the others */
};

_Static_assert(sizeof(struct SceKernelThreadmgrPrivate) == 0x28c8, "threadmgr private size");

/* the tracers' hooks: an event, the count of the values that follow, the values */
typedef void (*ThreadmgrTraceHook)(uint32_t event, uint32_t argc, ...);
typedef void (*ThreadmgrOnCpuHook)(SceUID processId, SceUID threadId, int priority,
				   uint32_t cpuAffinityMask);
typedef void (*ThreadmgrOffCpuHook)(SceUID processId, SceUID threadId, uint32_t why);
typedef void (*ThreadmgrPriorityChangeHook)(SceUID processId, SceUID threadId, int newPriority,
					    int oldPriority);

struct ThreadmgrRuntime {
	struct SceKernelThreadmgrPrivate *manager;
	void (*idleEntry)(void);
	ThreadmgrOffCpuHook offCpuHook;
	ThreadmgrTraceHook schedTraceHook;	/* 0x0c: SceThreadmgrForKernel_F55FDEFB */
	ThreadmgrOnCpuHook onCpuHook;
	ThreadmgrPriorityChangeHook priorityChangeHook;
	ThreadmgrTraceHook switchHook;		/* 0x18: SceThreadmgrForKernel_8C07C8EC */
	ThreadmgrTraceHook threadTraceHook;	/* 0x1c: SceThreadmgrForKernel_E2DACBC2 */
	ThreadmgrTraceHook alarmTraceHook;	/* 0x20: SceThreadmgrForKernel_ABBE9BA5 */
	ThreadmgrTraceHook vfpExceptionHook;	/* 0x24: SceThreadmgrForKernel_10853CCD */
};

extern struct ThreadmgrRuntime s_threadmgrRuntime;

void issueIntrRequest(unsigned int cpuId, struct ThreadmgrIntrRequest *request);
struct ThreadmgrIntrRequest *allocIntrRequest(uint32_t kind);
void addIntrRequest(unsigned int cpuId, uint32_t mask);
void SceThreadmgrForKernel_8D53CA9B(unsigned int cpuId);

struct ThreadmgrExtendedStackCache {
	uint32_t count;
	uint32_t capacity;
	SceUID memblockIds[3];
	uintptr_t mappedBases[3];
};

extern volatile struct ThreadmgrExtendedStackCache s_extendedStackCaches[4];
SceUID popExtendedStackCache(void **base);
int pushExtendedStackCache(SceUID uid, void *base);

_Static_assert(sizeof(struct ThreadmgrExtendedStackCache) == 0x20,
	       "per-CPU extended stack cache layout");

/* Native Sysroot time contracts; shared declarations await header integration. */
/* SceKernelSysClock */
typedef struct ThreadmgrSysClock {
	uint32_t low;
	uint32_t high;
} ThreadmgrSysClock;

_Static_assert(offsetof(SceProcessObject, threads) == 0x2d0 &&
	       offsetof(SceProcessObject, threadEventCount) == 0x374 &&
	       offsetof(SceProcessObject, lwMutexCacheLock) == 0x340 &&
	       offsetof(SceProcessObject, lwMutexCount) == 0x394 &&
	       offsetof(SceProcessObject, eventFlagCount) == 0x384 &&
	       offsetof(SceProcessObject, mutexCount) == 0x3a4 &&
	       offsetof(SceProcessObject, semaCount) == 0x3b4 &&
	       offsetof(SceProcessObject, uidLwCount) == 0x3e4,
	       "threadmgr process resource counts");
_Static_assert(offsetof(struct ThreadRegisterContext, pmcntenset) == 0xf0 &&
	       offsetof(struct ThreadCB, processLink) == 0x90,
	       "thread register context and process link");
_Static_assert(offsetof(struct ThreadCB, magic) == 0x1ac,
	       "threadmgr thread magic offset");
_Static_assert(offsetof(struct ThreadCB, waitType) == 0x40,
	       "threadmgr wait type offset");
_Static_assert(offsetof(struct SceKernelThreadmgrPrivate, semaphoreClass) == 0x2334,
	       "threadmgr semaphore class offset");
_Static_assert(offsetof(struct SceKernelThreadmgrPrivate, cpuTimerClass) == 0x238c,
	       "threadmgr CPU timer class offset");
_Static_assert(offsetof(struct SceKernelThreadmgrPrivate, systemTime) == 0x230c,
	       "shared system time pointer layout");

_Static_assert(offsetof(struct ThreadCB, permission) == 0x2c,
	       "thread permission layout");
_Static_assert(offsetof(struct ThreadCB, vfpException) == 0x14,
	       "thread VFP exception layout");
_Static_assert(sizeof(struct ThreadmgrCallback) == 0x40 &&
	       offsetof(struct ThreadmgrCallback, cb.notifyCount) == 0x30,
	       "SceUIDCallbackObject layout");
_Static_assert(offsetof(struct SceKernelThreadmgrPrivate, debugSuspendedNext) == 0x2250 &&
	       offsetof(struct SceKernelThreadmgrPrivate, debugHandlers) == 0x239c &&
	       offsetof(struct ThreadCB, pUStack) == 0xd0 &&
	       offsetof(struct ThreadCB, intrPreemptCount) == 0x184 &&
	       offsetof(struct ThreadCB, vmSuspendCount) == 0x180 &&
	       offsetof(SceProcessObject, TitleId) == 0x4c0 &&
	       offsetof(struct ThreadMgrCpuCB, idleThread) == 0x34,
	       "debugger state layout");
_Static_assert(sizeof(struct ThreadRegisterContext) == 0x100,
	       "thread register context size");
_Static_assert(offsetof(struct ThreadCB, kStackMemblkId) == 0xdc &&
	       offsetof(struct ThreadCB, stackMemBlockType) == 0x108 &&
	       offsetof(struct ThreadCB, callbackNest) == 0x160 &&
	       offsetof(struct ThreadCB, restartWaitMultipleEvents) == 0x7c,
	       "thread callback state layout");
_Static_assert(offsetof(struct SceKernelThreadmgrPrivate, callbackClass) == 0x234c,
	       "callback class layout");
_Static_assert(offsetof(struct ThreadCB, currentAttr) == 0xa8,
	       "thread state layout");
_Static_assert(offsetof(struct ThreadCB, tlsProcessId) == 0xfc,
	       "thread TLS layout");
_Static_assert(sizeof(struct ThreadMgrCpuCB) == 0x880, "CPU state stride");
_Static_assert(offsetof(struct SceKernelThreadmgrPrivate, dtraceHeapId) == 0x28b0,
	       "threadmgr dtrace state");
_Static_assert(offsetof(struct SceKernelThreadmgrPrivate, testMode) == 0x2434,
	       "test mode layout");

static inline struct ThreadCB *currentThreadContext(void)
{
	struct ThreadCB *thread;

	__asm__ volatile("mrc p15, 0, %0, c13, c0, 4" : "=r"(thread) : : "memory");
	return thread;
}

/* this core's VFP exception mask word, through its exception vector bank */
static inline volatile uint32_t *currentVfpException(void)
{
	volatile ExcpVectorBank *bank;

	__asm__ volatile("mrc p15, 0, %0, c12, c0, 0"
			 : "=r"(bank) : : "memory");
	return &bank->trace->vfpException;
}

static inline struct ThreadMgrCpuCB *currentCpuState(void)
{
	uintptr_t vbar;

	__asm__ volatile("mrc p15, 0, %0, c12, c0, 0"
			 : "=r"(vbar) : : "memory");
	return *(struct ThreadMgrCpuCB *volatile *)(vbar + 0x2c);
}

_Static_assert(sizeof(struct ThreadmgrIntrRequest) == 0x20,
	       "interrupt request allocation size");
_Static_assert(offsetof(struct ThreadMgrCpuCB, intrRequestHead) == 0x40,
	       "interrupt request head layout");
_Static_assert(offsetof(struct ThreadMgrCpuCB, intrRequestMask) == 0x44,
	       "interrupt request mask layout");
_Static_assert(offsetof(struct SceKernelThreadmgrPrivate, heapId) == 0x2304,
	       "interrupt request heap layout");
_Static_assert(offsetof(struct SceKernelThreadmgrPrivate, freeIntrRequests) == 0x2414,
	       "interrupt request free-list layout");
