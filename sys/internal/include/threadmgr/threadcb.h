/*
 * ThreadCB: the kernel's thread control block, owned by SceKernelThreadMgr and
 * read by every module that looks at the current thread (TPIDRPRW holds the
 * running thread's ThreadCB): intrmgr, excpmgr, sysmem/sysroot, processmgr,
 * modulemgr, syslibtrace, the debugger modules.
 *
 * Names: ThreadCB, pRegisterContext, pVfpRegisterContext, currentCpuId,
 * callbackNest, pUIDEntryHeap and magic are in threadmgr's 3.6x/4.00 messages
 * ("pRunningThreadCB->currentCpuId", "pThreadCB->magic", ...). The other p-, init-
 * and current- names come from the 0.990.030-I internal system debugger prototype
 * firmware's ThreadCB debug info, matched by meaning. The rest are ours; unk* are unknown.
 *
 * Every module includes this header instead of defining its own view.
 */
#pragma once

#include <stdint.h>
#include <psp2kern/types.h>
#include <threadmgr/threadmgr.h>

/*
 * The registers of a thread that is not running: saved by an exception or
 * interrupt (full = 1) or by a call into the scheduler (full = 0).
 * Name unknown; ThreadCB.pRegisterContext points to it.
 */
typedef struct ThreadRegisterContext {
	uint32_t full;				/* 0x00: PLACEHOLDER: 1: pc/cpsr at 0xc0/0xc4; 0: saved by a call (pc = lr, cpsr at 0x40) */
	union {
		uint32_t r[15];			/* 0x04: r0-r14 */
		struct {
			uint32_t gpr[13];
			uint32_t sp;		/* 0x38 */
			uint32_t lr;		/* 0x3c */
		};
	};
	uint32_t status;			/* 0x40: the cpsr of a context saved by a call */
	uint32_t userDacr;			/* 0x44: DACR while in user mode */
	uint32_t kernelDacr;			/* 0x48: DACR while in the kernel */
	uint32_t cpacr;				/* 0x4c: CPACR */
	uint32_t tpidrurw;			/* 0x50: TPIDRURW */
	uint32_t tpidruro;			/* 0x54: TPIDRURO (the user TLS) */
	uint32_t ttbr1;				/* 0x58: TTBR1 */
	uint32_t contextidr;			/* 0x5c: CONTEXTIDR */
	uint32_t par;				/* 0x60: PAR */
	uint32_t pmcr;				/* 0x64: PMCR */
	uint32_t pmovsr;			/* 0x68: PMOVSR */
	uint32_t pmselr;			/* 0x6c: PMSELR */
	uint32_t pmuserenr;			/* 0x70: PMUSERENR */
	uint32_t pmccntr;			/* 0x74: PMCCNTR */
	struct {
		uint32_t eventType;		/* PMXEVTYPER */
		uint32_t count;			/* PMXEVCNTR */
	} pmevent[6];				/* 0x78 */
	uint32_t dfsr;				/* 0xa8: DFSR at an abort */
	uint32_t ifsr;				/* 0xac: IFSR */
	uint32_t dfar;				/* 0xb0: DFAR */
	uint32_t ifar;				/* 0xb4: IFAR */
	uint32_t teehbr;			/* 0xb8: TEEHBR */
	uint32_t priorityMask;			/* 0xbc: the GIC priority mask */
	uint32_t pc;				/* 0xc0 */
	uint32_t cpsr;				/* 0xc4 */
	uint32_t reserved0;			/* 0xc8: zeroed */
	uint8_t reserved1[0x24];
	uint32_t pmcntenset;			/* 0xf0: PMCNTENSET */
	uintptr_t callbackVfp;			/* 0xf4: callbackContext's VFP save area (| 1) */
	struct ThreadRegisterContext *callbackContext;	/* 0xf8: while in a callback */
	uint32_t syscallAux;			/* 0xfc */
} ThreadRegisterContext;

/*
 * The user registers saved on entry to a syscall, at the top of the kernel
 * stack (ThreadCB.userFrame while in one). Name unknown.
 */
typedef struct ThreadSyscallFrame {
	uint32_t r[13];				/* 0x00: r0-r12 (r12: the syscall number) */
	uint32_t syscallAux;			/* 0x34: the register context's, kept here in a syscall */
	uintptr_t entry;			/* 0x38: the kernel function called ("Entry:" in sysmem's dump) */
	uint32_t fpscr;				/* 0x3c */
	uint32_t apiDeclare;			/* 0x40: sceKernelApiDeclare*: the result type, or 0x80000000 | an error message */
	uint32_t tpidrurw;			/* 0x44: the caller's TPIDRURW */
	uint8_t reserved[8];
	uint32_t sp;				/* 0x50 */
	uint32_t lr;				/* 0x54 */
	uint32_t pc;				/* 0x58 */
	uint32_t cpsr;				/* 0x5c */
} ThreadSyscallFrame;

/* a thread's memory of each driver TLS slot (SceThreadmgrForDriver_89CA5698); the debugger copies it whole */
typedef struct ThreadTlsSlotMemory {
	void *slot[8];
} ThreadTlsSlotMemory;

/*
 * syslibtrace's per-thread state (syslibtraceThreadCBInit allocates it). Packed:
 * Sony's code clears it byte by byte.
 */
typedef struct __attribute__((packed)) SyslibtraceThreadCB {
	uint8_t unk00[4];
	void *savedLr;				/* 0x04: the caller of the traced syscall */
} SyslibtraceThreadCB;

/* DTrace's per-thread state (dtraceThreadCBInit allocates it) */
typedef struct SceDtraceThreadCB {
	uint32_t unk00[5];
	uint32_t flags;				/* 0x14: DTRACE_THREAD_START when allocated */
	uint32_t unk18[4];
} SceDtraceThreadCB;

#include <processmgr/process.h>
struct ThreadmgrExtendedStack;
struct ThreadmgrCallback;
struct ThreadmgrCallbackCB;
struct ThreadmgrExceptionEntry;
struct ThreadmgrFastMutex;

typedef struct ThreadCB {
	uintptr_t next;				/* 0x00: Sony: link */
	uintptr_t prev;
	SceUID threadId;
	ThreadRegisterContext *pRegisterContext;	/* 0x0c */
	uintptr_t pVfpRegisterContext;		/* 0x10: its VFP save area (0x120) */
	uint32_t vfpException;
	struct SceProcessObject *process;	/* 0x18 */
	uint32_t thbp[2];			/* 0x1c: sceKernelSetTHBP's words (context breakpoint value, control) */
	uint32_t flags;				/* 0x24: THREAD_STATUS_* bits the interrupt return path checks */
	int initPermission;			/* 0x28: permission restored after a syscall */
	int permission;
	uintptr_t kernelStackTop;		/* 0x30: where a syscall frame goes */
	uintptr_t tls;				/* 0x34: TPIDRURO */
	SceUID processId;
	SceKernelSpinlock lock;			/* 0x3c: THREAD_LOCK */
	uint32_t waitType;			/* 0x40: what it waits on (0x40000: a user lw object) */
	ThreadSyscallFrame *userFrame;		/* 0x44: in a syscall, the caller's registers */
	ThreadTlsSlotMemory tlsSlotMemory;	/* 0x48 */
	uintptr_t ownedFastMutexNext;
	uintptr_t ownedFastMutexPrev;
	uint32_t debugSuspendReason;		/* 0x70: the debugger's suspend reasons */
	uintptr_t pKStack;			/* 0x74: its kernel stack */
	SceSize kStackSize;			/* 0x78 */
	void *restartWaitMultipleEvents;	/* 0x7c: kept for a restarted WME syscall */
	uint8_t unk80[0x10];
	ListNode processLink;		/* 0x90: in the process's thread list (SceProcessObject.threads) */
	SceUID userThreadId;
	uint32_t status;			/* 0x9c */
	int exitStatus;				/* 0xa0: sceKernelExitThread's status */
	uint32_t initAttr;			/* 0xa4: the attributes a start restores */
	uint32_t currentAttr;			/* 0xa8: bit 31: a user thread */
	int initPriority;
	int basePriority;
	int boostPriority;
	int currentPriority;
	uint32_t initCpuAffinityMask;		/* 0xbc */
	uint32_t currentCpuAffinityMask;
	int currentCpuId;
	uint32_t lastExecutedCpuId;
	uintptr_t initSp;			/* 0xcc: a kernel thread's first sp */
	uintptr_t pUStack;			/* 0xd0 */
	SceSize uStackSize;			/* 0xd4 */
	SceUID uStackMemblkId;			/* 0xd8 */
	SceUID kStackMemblkId;			/* 0xdc */
	uintptr_t entry;			/* 0xe0: the entry point */
	SceSize argSize;			/* 0xe4: the start argument */
	uintptr_t pArgBlock;			/* 0xe8 */
	uint8_t unkEC[8];
	void *addressSpace;			/* 0xf4: its process's address space object */
	void *pUIDEntryHeap;			/* 0xf8: its process's PUID entry heap */
	SceUID tlsProcessId;
	int ioFlags;				/* 0x100: per-thread I/O mode (driver get/set) */
	SceUID userTlsMemBlockId;		/* 0x104: "uTLS"; freed with the thread */
	uint32_t stackMemBlockType;		/* 0x108 */
	struct ThreadmgrExtendedStack *extendedStacks;	/* 0x10c: sceKernelExtendKernelStackWide's */
	SceUID waitId;
	int waitResult;
	SceUID delayId;
	uintptr_t timeoutOutput;
	uint32_t remainingTimeLow;
	uint32_t remainingTimeHigh;
	union {
		uint32_t waitArg0;		/* 0x128: as the debugger reports it */
		int waitMode;			/* 0x128: rw lock: 1 = waits to write */
		uint32_t waitEventPattern;	/* event: the bits waited for */
		SceSize msgPipeRemaining;	/* msg pipe: the bytes still to move */
		void *waitMultipleEvents;	/* WME: its ThreadmgrWaitMultipleEvents */
		struct ThreadmgrFastMutex *spinMutex;	/* fast mutex: the one it spins on */
	};
	union {
		uint32_t waitArg1;		/* 0x12c */
		SceUID waitCondId;		/* 0x12c: cond: the condition it waited on */
		uint32_t waitEventMode;		/* event flag: the wait mode */
		uint32_t fastMutexHandoff;	/* fast mutex: set when the spin ends */
		uint32_t *waitEventPatternOutput;	/* event: the bits seen */
		SceSize msgPipeSize;		/* msg pipe: the bytes asked for */
	};
	union {
		uint32_t waitArg2;		/* 0x130 */
		uint32_t *waitPatternOutput;	/* 0x130: event flag: the pattern seen */
		SceUID waitMutexId;		/* cond: the mutex it relocks */
		uint64_t *waitEventDataOutput;	/* event: the user data seen */
		SceSize *msgPipeResult;		/* msg pipe: where the bytes moved go */
	};
	uint32_t waitOptions;			/* 0x134: msg pipe: its MSG_PIPE_MODE_* */
	const void *msgPipeVector;		/* 0x138: msg pipe: the caller's buffers */
	int msgPipeVectorIndex;			/* 0x13c */
	SceSize msgPipeVectorOffset;		/* 0x140 */
	SceUID timeoutProcessId;		/* 0x144: the process the wait's pointers are in */
	uintptr_t heldEventFlagNext;		/* 0x148: the event flags it holds */
	uintptr_t heldEventFlagPrev;
	uintptr_t ownedMutexNext;		/* 0x150: the mutexes it holds (ThreadmgrMutex.ownerLink) */
	uintptr_t ownedMutexPrev;
	uintptr_t ownedRWLockNext;		/* 0x158: the rw locks it holds for writing */
	uintptr_t ownedRWLockPrev;
	int callbackNest;			/* 0x160: callbacks running on it */
	struct ThreadmgrCallback *currentCallback;	/* 0x164 */
	struct ThreadmgrCallbackCB *callbackNext;	/* 0x168: its callbacks */
	struct ThreadmgrCallbackCB *callbackPrev;
	uint32_t threadEventMask;		/* 0x170: the thread events being delivered */
	void *threadEventHandler;		/* 0x174: the handler running */
	struct ThreadmgrExceptionEntry *exceptionEntry;	/* 0x178: its pending exception (status 0x200) */
	uint32_t handedExceptions;		/* 0x17c: exceptions handed to the VM (its option's) */
	int vmSuspendCount;			/* 0x180: sceKernelSuspendThreadForVM's nesting */
	uint32_t intrPreemptCount;		/* 0x184 */
	uint32_t threadPreemptCount;
	uint32_t threadReleaseCount;
	uint64_t runClock;			/* 0x190: time on a CPU */
	uint32_t changeCpuCount;		/* 0x198 */
	uint32_t savedCpuAffinityMask;		/* 0x19c: put back when a suspension ends */
	struct SyslibtraceThreadCB *syslibtraceState;	/* 0x1a0: syslibtraceThreadCBInit's (8) */
	struct SceDtraceThreadCB *dtraceState;	/* 0x1a4: dtraceThreadCBInit's (0x28) */
	SceKernelSpinlock dtraceLock;		/* 0x1a8 */
	uint32_t magic;				/* 0x1ac: THREAD_CB_MAGIC */
} ThreadCB;

#define THREAD_CB_MAGIC		0xe38b17a9

/*
 * ThreadCB.status: the state in the low half, Sony's SCE_KERNEL_THREAD_STATUS_*
 * (threadmgr/threadmgr.h). A debug-suspended thread keeps its old state << 16.
 */
#define THREAD_STATE_QUEUED		0x00000007	/* running, ready or standby */
#define THREAD_STATE_GONE		0x00000060	/* deleted or dead */
#define THREAD_STATE_ENDED		0x00000070	/* dormant, deleted or dead */
#define THREAD_STATE_OFF_CPU		0x00008000	/* debug-suspended and taken off the CPUs */
#define THREAD_STATE_HIGH_MASK		0xffff0000
#define THREAD_STATE_DEBUG_SUSPENDED	0x80000000
#define THREAD_STATE_KEEP_SUSPEND	0x8000ffff

/* ThreadCB.flags (Sony: "flags") */
#define THREAD_FLAG_WAKE_PENDING	0x00000001	/* in a wait that runs callbacks */
#define THREAD_FLAG_CALLBACKS		0x00000002	/* callbacks pending */
#define THREAD_FLAG_CALLBACK_WAKEUP	0x00000003	/* both: the wait ends to run them */
#define THREAD_FLAG_CALLBACK_RESULT	0x00000004	/* a callback's result replaces a deleted wait's */
#define THREAD_FLAG_SIGNALED		0x00000010	/* a signal is pending */
#define THREAD_FLAG_WAIT_MARK		0x00000020	/* waiting inside a syscall */
#define THREAD_FLAG_EXCEPTION		0x00000200	/* an exception is pending (exceptionEntry) */
#define THREAD_FLAG_EXCEPTION_WAITER	0x00000400	/* it owns its process's exception object */
#define THREAD_FLAG_FRAME_CHANGED	0x00002000	/* the debugger wrote the user frame */
#define THREAD_FLAG_SIGNAL_DEFERRED	0x00004000	/* a request broke a wait */
#define THREAD_FLAG_RESUMED_WAIT	0x00008000	/* a restarted syscall's wait */
#define THREAD_FLAG_WAIT_CLEAR		0x00008001
#define THREAD_FLAG_CALLBACK_SAVED	0x0000a000	/* the bits a callback runs without */
#define THREAD_FLAG_IN_CALLBACK		0x00010000	/* the registers are a callback's */
#define THREAD_FLAG_SELF_STOPPED	0x00020000	/* it took itself off its CPU in a trap */
#define THREAD_FLAG_LW_COND_KEEP_MUTEX	0x00100000	/* retried, mutex not held */
#define THREAD_FLAG_LW_COND_RELOCK	0x00200000	/* retried after the wait */
#define THREAD_FLAG_COND_KEEP_MUTEX	0x00400000	/* retried, mutex not held */
#define THREAD_FLAG_COND_RELOCK		0x00800000	/* retried after the wait */
#define THREAD_FLAG_COND_RETRY_MASK	0x00f00000
#define THREAD_FLAG_SUSPEND_COUNTED	0x08000000	/* a suspension is in place */
#define THREAD_FLAG_REQUEST_VM_SUSPEND	0x10000000	/* the requests, served in a trap */
#define THREAD_FLAG_REQUEST_SUSPEND	0x20000000
#define THREAD_FLAG_REQUEST_EXIT_DELETE	0x40000000
#define THREAD_FLAG_REQUEST_EXIT	0x80000000
#define THREAD_FLAG_EXITING		0xc0000000
#define THREAD_FLAG_PENDING_STOP	0xe0000000
#define THREAD_FLAG_REQUEST_MASK	0xf0000000
#define THREAD_FLAG_RESUME_CLEAR	0xf8000000
#define THREAD_FLAG_REQUEST_SHIFT	28

_Static_assert(sizeof(ThreadRegisterContext) == 0x100, "ThreadRegisterContext size");
_Static_assert(__builtin_offsetof(ThreadRegisterContext, pmcntenset) == 0xf0, "pmcntenset");
_Static_assert(sizeof(ThreadSyscallFrame) == 0x60, "ThreadSyscallFrame size");
_Static_assert(__builtin_offsetof(ThreadSyscallFrame, apiDeclare) == 0x40, "apiDeclare");
_Static_assert(__builtin_offsetof(ThreadCB, userFrame) == 0x44, "ThreadCB.userFrame");
_Static_assert(__builtin_offsetof(ThreadCB, status) == 0x9c, "ThreadCB.status");
_Static_assert(__builtin_offsetof(ThreadCB, currentAttr) == 0xa8, "ThreadCB.currentAttr");
_Static_assert(__builtin_offsetof(ThreadCB, processLink) == 0x90, "ThreadCB.processLink");

/* the thread a SceProcessObject.threads link belongs to */
#define THREAD_FROM_PROCESS_LINK(n)	((ThreadCB *)((uintptr_t)(n) - __builtin_offsetof(ThreadCB, processLink)))
_Static_assert(__builtin_offsetof(ThreadCB, addressSpace) == 0xf4, "ThreadCB.addressSpace");
_Static_assert(__builtin_offsetof(ThreadCB, syslibtraceState) == 0x1a0, "ThreadCB.syslibtraceState");
_Static_assert(sizeof(SyslibtraceThreadCB) == 8, "SyslibtraceThreadCB size");
_Static_assert(sizeof(SceDtraceThreadCB) == 0x28, "SceDtraceThreadCB size");
_Static_assert(sizeof(ThreadCB) == 0x1b0, "ThreadCB size");

struct ThreadmgrWaitNode;

/* SceUIDThreadObject: the object base, then the thread context */
struct ThreadmgrThreadObject {
	SceUID ownerPid;			/* the object base */
	void *cls;
	void *uidEntry;				/* 0x08: the GUID entry */
	void *cacheNext;			/* 0x0c: the next cached object; &uidEntry while in use */
	struct ThreadmgrWaitNode *waitNext;	/* 0x10: the threads waiting on this thread */
	struct ThreadmgrWaitNode *waitPrev;
	SceKernelSpinlock waitLock;		/* 0x18 */
	uint32_t waiting;
	uint32_t attributes;			/* 0x20 */
	uint32_t reserved;			/* 0x24: zeroed */
	struct ThreadCB context;	/* 0x28 */
};

/* the thread object a thread context is part of */
#define THREAD_OBJECT(thread)	((struct ThreadmgrThreadObject *)((uintptr_t)(thread) -	\
				 __builtin_offsetof(struct ThreadmgrThreadObject, context)))

/* TPIDRPRW: the running thread's ThreadCB, NULL before the thread manager runs */
static inline ThreadCB *getCurrentThreadCB(void)
{
	ThreadCB *thread;

	__asm__ volatile("mrc p15, 0, %0, c13, c0, 4" : "=r"(thread));
	return thread;
}

static inline void setCurrentThreadCB(ThreadCB *thread)
{
	__asm__ volatile("mcr p15, 0, %0, c13, c0, 4" : : "r"(thread) : "memory");
}
