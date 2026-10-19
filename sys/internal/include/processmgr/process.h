/*
 * The process object (SceUIDProcessClass), owned by SceProcessmgr and read by
 * SceKernelThreadMgr (its threads, timers, message pipes and object counts).
 */

#pragma once

#include <scetypes.h>
#include <sysmem/spinlock.h>
#include <authmgr/sblauthmgr.h>
#include <threadmgr/fast_mutex_object.h>
#include <sysmem/list.h>

struct ThreadmgrException;

#define PROCESS_MAGIC			0x962e8d55
#define PROCESS_MAX			16
#define PROCESS_LOCAL_STORAGE_MAX	32

/* SceProcessObject.type, ProcessBudget.type */
#define PROCESS_TYPE_GAME		0x1000000
#define PROCESS_TYPE_MINI_APP		0x2000000
#define PROCESS_TYPE_SHELL		0x4000000
#define PROCESS_TYPE_KERNEL		0x5000000

/* SceProcessObject.status: low byte the state, the rest flags */
#define PROCESS_STATE_MASK		0xff
#define PROCESS_STATE_NONE		0
#define PROCESS_STATE_CREATED		1
#define PROCESS_STATE_RUNNING		2
#define PROCESS_STATE_SUSPENDED		3
#define PROCESS_STATE_EXITED		4
#define PROCESS_STATE_DEBUG		0x10	/* set in every debugger suspend kind */
#define PROCESS_STATE_DEBUG_SUSPENDED	0x11	/* 0x11..0x15: debugger suspend kinds */
#define PROCESS_FLAG_PRELOAD		0x00010000	/* preload pending (not started yet) */
#define PROCESS_FLAG_PRELOAD_SUSPEND	0x00020000
#define PROCESS_FLAG_SUSPENDING		0x00100000
#define PROCESS_FLAG_RESUMING		0x00200000
#define PROCESS_FLAG_DEBUG_SUSPENDING	0x00400000
#define PROCESS_FLAG_STARTING		0x00800000
#define PROCESS_FLAG_EXIT		0x01000000	/* exit/kill requested */
#define PROCESS_FLAG_FORCE_KILL		0x02000000
#define PROCESS_FLAG_FORCE_SUSPEND	0x04000000

/* one counted resource of a process (threads, ...) */
typedef struct ProcessResource {
	SceKernelSpinlock lock;
	SceUInt32 limit;
	SceUInt32 count;
	SceUInt32 peak;
} ProcessResource;

/* hardware breakpoint / watchpoint registers of a process (tool mode, dipsw 0xe4) */
typedef struct ProcessDebugRegPair {
	SceUInt32 value;
	SceUInt32 control;
} ProcessDebugRegPair;

typedef struct ProcessHwBreakpoints {
	SceKernelSpinlock lock;
	SceUInt32 reserved;
	ProcessDebugRegPair reg[5];
} ProcessHwBreakpoints;

typedef struct ProcessHwWatchpoints {
	SceKernelSpinlock lock;
	SceUInt32 reserved;
	ProcessDebugRegPair reg[4];
} ProcessHwWatchpoints;

/* one allocation for the three (tool mode) */
typedef struct ProcessDebugRegBlock {
	ProcessHwBreakpoints bp;
	ProcessHwWatchpoints wp;
	ProcessDebugRegPair contextBreakpoint;	/* breakpoint 5, the threads' THBP */
} ProcessDebugRegBlock;

_Static_assert(sizeof(ProcessDebugRegBlock) == 0x60, "ProcessDebugRegBlock size");

/* the process image information sceKernelLoadProcessImage fills in */
typedef struct ProcessImageInfo {
	SceUInt32 sdkVersion;
	char mainThreadName[0x20];
	SceInt32 mainThreadPriority;
	SceSize mainThreadStackSize;
	SceUInt32 mainThreadAttr;
	SceInt32 mainThreadCpuAffinityMask;
	SceUInt8 mainThreadOptParam[0x1c];	/* SceKernelThreadOptParam */
	SceUInt32 reserved;
	char processName[0x20];
	SceUInt32 processPreloadDisabled;
	void *processParam;
} ProcessImageInfo;

_Static_assert(sizeof(ProcessImageInfo) == 0x7c, "ProcessImageInfo size");

/* a process termination callback (sceKernelRegisterProcessTerminationCallback) */
typedef struct ProcessTerminationCallback {
	struct ProcessTerminationCallback *next;
	SceUID cb;
} ProcessTerminationCallback;

/* the SceUIDProcBudgetClass object (0x74 bytes) */
typedef struct ProcessBudget {
	SceUInt32 base[2];
	SceUInt32 reserved;
	SceUID mutex;
	SceUID uid;
	SceUInt32 type;				/* PROCESS_TYPE_* */
	void *mainPartition;
	void *cdialogPartition;
	void *phyContPartition;
	void *cdramPartition;
	SceUInt32 processCount;
	SceUInt32 activeCount;
	SceUInt32 mainSize;
	SceUID process[PROCESS_MAX];
} ProcessBudget;

_Static_assert(sizeof(ProcessBudget) == 0x74, "ProcessBudget size");

/* the SceUIDProcessClass object (0x4e0 bytes) */
typedef struct SceProcessObject {
	SceUInt32 base[2];			/* SceObjectBase */
	SceUInt32 reserved0;
	ProcessHwBreakpoints *phbp;
	ProcessHwWatchpoints *phwp;
	SceUID lockSema;			/* "SceKernelProcessmgrProcLock" */
	SceUID eventFlag;			/* "SceKernelProcessmgrProc" */
	SceUID eventSema;			/* "SceKernelProcessmgrEventLock" */
	SceUInt64 time;				/* start time, then the run time; off its timers' clock */
	SceKernelSpinlock timeLock;
	SceUInt32 timeRunning;
	ListNode timers;			/* its timers (ThreadmgrUidTimer.processLink) */
	SceKernelSpinlock timerLock;
	SceUInt32 status;			/* PROCESS_STATE_*, PROCESS_FLAG_* */
	SceUInt32 type;				/* PROCESS_TYPE_* */
	SceUInt32 attr;
	void *entryHeapObject;			/* the UID entry heap object */
	SceUID entryHeap;
	SceUID app;
	SceUInt32 cpuAffinity;			/* its threads' default CPU affinity mask */
	SceUInt32 slot;				/* process private memory slot */
	SceUID addressSpace;
	void *addressSpaceCB;
	SceUID pid;
	SceUID parentPid;
	SceUID mainThread;
	SceUID exitThread;			/* the thread that called exit: __proc_:exit when it ends */
	ProcessBudget *budget;
	SceUID budgetId;
	SceUInt32 pendingRequests;
	SceUID uniqueHeap;
	SceUID kplsHeap;
	SceUID cdialogHeap;
	SceUID toolHeap;
	SceUID kplsBlock;
	int exitStatus;
	SceUInt32 infoParam;			/* reported by sceKernelGetProcessInfo; 0x7f once created */
	void *libdb;
	SceUInt32 threadPermission;		/* its new threads' permission */
	SceUInt8 random[0x10];			/* SceKernelUtils random state */
	SceKernelSpinlock randomLock;
	SceSelfAuthInfo authInfo;
	ProcessImageInfo image;
	SceUID stdioFd[4];			/* stdin, stdout, stderr, tty7 */
	SceUID stdioPuid[4];
	void *libkernelAddr[6];			/* _sceKernelRegisterLibkernelAddresses */
	SceUInt32 syscallTraceFlag;
	SceUInt32 reserved1;
	SceKernelSpinlock localStorageLock;
	void *localStorage[PROCESS_LOCAL_STORAGE_MAX];
	ProcessDebugRegPair *contextBreakpoint;	/* tool: breakpoint 5, copied to new threads' THBP */
	SceUInt32 defaultPmuserenr;		/* its threads' PMUSERENR */
	SceUInt32 defaultPmcr;			/* its threads' PMCR */
	SceUInt32 prevState;
	SceUInt32 reserved2[3];
	SceUInt32 dtraceFlags;			/* DTRACE_PROCESS_START: __proc_:start pending */
	ListNode recoveryNode;		/* resource recovery queue */
	SceKernelSpinlock lock;			/* its status and its thread list */
	SceUInt32 powerLockCount;
	SceKernelSpinlock powerLock;
	SceUInt32 threadsStopped;		/* set while a suspend scans its threads: none is scheduled */
	SceUInt32 stackGuard;			/* random; copied to its threads' TLS */
	SceKernelSpinlock callbackLock;
	ProcessTerminationCallback *callbacks;
	SceUID preloadThread;
	ListNode threads;		/* ThreadCB.processThreadNext */
	SceULong threadCount;			/* long: Sony's updates do not alias the SceUID fields */
	SceULong threadPeak;
	SceULong unlimitedThreadCount;		/* threads with attribute 0x4000 */
	SceULong unlimitedThreadPeak;
	ListNode msgPipes;		/* the msg pipes whose buffers it holds */
	struct ThreadmgrFastMutex msgPipeLock;	/* "SceKernelMsgPipeListLock" */
	SceUID workerThread;			/* "tworker: %s", processWorkerThread */
	SceUID abortEventFlag;			/* set when a thread asks for the process's abort/suspension */
	SceUID abortThread;			/* that thread */
	SceUInt32 abortCode;			/* why */
	SceKernelSpinlock lwMutexCacheLock;	/* free SceKernelLwMutexCache objects */
	void *lwMutexCache;
	int lwMutexCacheCount;
	SceUInt32 abortRequested;		/* set once, atomically, by the first abort */
	SceUID exceptionId;			/* "excp: %s", virtual_machine.c */
	struct ThreadmgrException *exception;
	SceKernelSpinlock resourceLock;		/* guards the object counts */
	SceUInt32 uidCount;			/* UID objects of all kinds, at most 0x800 */
	SceUInt32 uidPeak;
	SceUInt32 uidUnlimitedCount;		/* those of threads with attr 0x4000 */
	SceUInt32 uidUnlimitedPeak;
	SceUInt32 callbackCount;
	SceUInt32 callbackPeak;
	SceUInt32 threadEventCount;
	SceUInt32 threadEventPeak;
	SceUInt32 condCount;
	SceUInt32 condPeak;
	SceUInt32 eventFlagCount;
	SceUInt32 eventFlagPeak;
	SceUInt32 lwCondCount;
	SceUInt32 lwCondPeak;
	SceUInt32 lwMutexCount;
	SceUInt32 lwMutexPeak;
	SceUInt32 msgPipeCount;
	SceUInt32 msgPipePeak;
	SceUInt32 mutexCount;
	SceUInt32 mutexPeak;
	SceUInt32 rwLockCount;
	SceUInt32 rwLockPeak;
	SceUInt32 semaCount;
	SceUInt32 semaPeak;
	SceUInt32 simpleEventCount;
	SceUInt32 simpleEventPeak;
	SceUInt32 timerCount;
	SceUInt32 timerPeak;
	SceUInt32 reserved3[6];
	SceUInt32 uidOpenableCount;		/* openable objects, at most 0x100 */
	SceUInt32 uidOpenablePeak;
	SceKernelSpinlock coredumpLock;
	void *coredumpHandler;
	SceSize coredumpStackSize;
	void *coredumpArg;
	SceUID coredumpStack;
	SceInt32 msgPipeAsyncCount;		/* its queued async requests, at most 0x100 */
	SceSize msgPipeMemory;			/* its msg pipes' buffers, at most 32 MiB */
	SceUInt16 resolutionFlags;		/* the display resolutions allowed (SceKernelProcessOpt) */
	SceUInt16 powerConfigFlags;		/* the power configurations allowed (SceKernelProcessOpt) */
	SceUInt16 moduleCount[2];		/* modules, kernel modules */
	SceUInt32 magic;			/* PROCESS_MAGIC */
	SceUInt32 cpuAffinityOverride;
	void *uniqueHeapCB;			/* the unique heap's control block, in the PPM */
	void *cdialogHeapCB;			/* the CDialog heap's */
	void *toolMemory;
	void *objectHeap[2];
	SceUInt32 limit[2];
	SceUInt32 threadActionCursor;		/* sceKernelProcessDebugResume */
	ListNode node;			/* all processes */
	ProcessResource resource[8];
	char TitleId[0x20];			/* Sony: "pProcess->TitleId" */
} SceProcessObject;

_Static_assert(sizeof(SceProcessObject) == 0x4e0, "SceProcessObject size");
_Static_assert(__builtin_offsetof(SceProcessObject, contextBreakpoint) == 0x288 &&
	       __builtin_offsetof(SceProcessObject, threads) == 0x2d0 &&
	       __builtin_offsetof(SceProcessObject, msgPipeLock) == 0x2f0 &&
	       __builtin_offsetof(SceProcessObject, exceptionId) == 0x350 &&
	       __builtin_offsetof(SceProcessObject, uidOpenableCount) == 0x3e4 &&
	       __builtin_offsetof(SceProcessObject, coredumpLock) == 0x3ec &&
	       __builtin_offsetof(SceProcessObject, node) == 0x438 &&
	       __builtin_offsetof(SceProcessObject, TitleId) == 0x4c0, "SceProcessObject layout");

#define PROCESS_FROM_NODE(n)		((SceProcessObject *)((char *)(n) - __builtin_offsetof(SceProcessObject, node)))
#define PROCESS_FROM_RECOVERY(n)	((SceProcessObject *)((char *)(n) - __builtin_offsetof(SceProcessObject, recoveryNode)))
