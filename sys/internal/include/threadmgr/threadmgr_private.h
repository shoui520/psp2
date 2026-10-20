/* The thread manager's own state (SceThreadmgrForKernel_19440C83), read by the debugger. */
#pragma once

#include <stdint.h>
#include <scetypes.h>
#include <sysmem/spinlock.h>
#include <sysmem/uid_class.h>
#include <processmgr/process.h>
#include <systimer/timer_regs.h>

struct IntrRequest;
struct ThreadCB;
struct ThreadmgrTimer;
struct SceUIDWaitableObject;

/*
 * The debugger's handlers (SceThreadmgrForDriver_6C2E3A49): group 0 is told
 * of an event, group 1 decides what happens to the thread.
 */
typedef int (*ThreadmgrDebugHandler)(SceSize size, void *message);

/* Per-CPU scheduler state begins at the VBAR+0x2c indirection. */
struct ThreadMgrCpuCB {
	uintptr_t pRunningThreadCB;		/* 0x00 */
	uintptr_t pNextRunningThreadCB;		/* 0x04: 0: to be chosen */
	uint32_t disabledDispatch;		/* 0x08: nonzero: dispatching is disabled */
	volatile SceUID runningProcessId;	/* 0x0c */
	volatile SceUID runningThreadId;	/* 0x10 */
	SceUID processId;
	SceUID threadId;
	int priority;
	uintptr_t standbyNext;
	uintptr_t standbyPrev;
	SceKernelSpinlock standbyListLock;
	uint32_t threadSwitchCount;		/* 0x2c: reported by SceThreadmgrForDriver_7E280B69 */
	uint32_t remainCpuCount;		/* 0x30: switches that kept the running thread (__sched_:remain__cpu) */
	struct ThreadCB *pIdleThreadCB;	/* 0x34: never debug-suspended */
	uint64_t lastThreadSwitchTime;		/* 0x38: when the running thread got the CPU */
	struct IntrRequest *intrRequestHead;
	uint32_t intrRequestMask;
	uintptr_t switchStackTop;		/* 0x48: its page of the switch stack block */
	uint32_t releasingWaitObject;		/* 0x4c: a switch is releasing the old wait's UID */
	uint8_t reserved[0x10];
	uint32_t readySummary[8];		/* 0x60: a bit per non-empty ready queue */
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

/* Recovered prefix of Sony's SceKernelThreadmgrPrivate (native size 0x28c8). */
struct SceKernelThreadmgrPrivate {
	struct ThreadMgrCpuCB cpuCB[4];
	SceKernelSpinlock threadMgrCBLock;
	uint32_t activeCpuMask;
	uint32_t cpuSelectCounter;
	SceKernelSpinlock delayCacheLock;	/* 0x220c: free delay objects (timer.c) */
	int delayFreeCount;			/* 0x2210 */
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
	struct ThreadCB *pCleanerThreadCB;	/* 0x2260: never debug-suspended either */
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
	struct SceUIDWaitableObject *signalObject;	/* 0x2398: its waitable (queue at +0x10) */
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
	struct IntrRequest *freeIntrRequests;
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
