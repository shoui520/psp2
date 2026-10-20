/*
 * SceKernelThreadMgr's thread information for the debugger
 * (SceThreadmgrForDriver_555CA6BB, SceThreadmgrForKernel_C976719E/BFF892B4).
 */

#pragma once

#include <stdint.h>
#include <scetypes.h>
#include <threadmgr/threadcb.h>

/* the thread information for the debugger (0x128) */
typedef struct ThreadmgrThreadInfoForDebugger {
	SceSize size;
	SceUID threadId;
	SceUID processId;			/* 0x08 */
	char name[32];				/* 0x0c */
	uint32_t initAttr;			/* 0x2c */
	uint32_t attr;				/* 0x30 */
	uint32_t status;			/* 0x34: THREAD_STATE_* (level 5: the raw flags) */
	uintptr_t entry;			/* 0x38 */
	uintptr_t userStackBase;		/* 0x3c */
	SceSize userStackSize;
	SceSize userStackMaxUsed;		/* 0x44: with flag 1 */
	SceSize userStackUsed;			/* 0x48 */
	uintptr_t stackBase;			/* 0x4c: the kernel stack */
	SceSize stackSize;
	SceSize stackMaxUsed;			/* 0x54: with flag 1 */
	SceSize stackUsed;			/* 0x58 */
	uint32_t reserved0[4];			/* 0x5c: zeroed */
	uintptr_t kernelStackTop;		/* 0x6c */
	SceSize argSize;			/* 0x70 */
	uintptr_t pArgBlock;			/* 0x74 */
	int initPriority;			/* 0x78 */
	int currentPriority;			/* 0x7c */
	uint32_t initCpuAffinityMask;		/* 0x80 */
	uint32_t cpuAffinityMask;		/* 0x84 */
	uint32_t processCpuAffinityMask;	/* 0x88 */
	int cpuId;				/* 0x8c */
	int lastCpuId;				/* 0x90 */
	uint32_t waitType;			/* 0x94: | 0x80000000 in a wait that runs callbacks */
	SceUID waitId;				/* 0x98 */
	uint32_t runClocksLow;			/* 0x9c */
	uint32_t runClocksHigh;
	int exitStatus;				/* 0xa4: the thread's exit status */
	uint32_t debugSuspendReason;		/* 0xa8 */
	uint32_t intrPreemptCount;		/* 0xac */
	uint32_t threadPreemptCount;
	uint32_t threadReleaseCount;
	uint32_t changeCpuCount;		/* 0xb8 */
	uint32_t callbacksPending;		/* 0xbc */
	uintptr_t registers;			/* 0xc0 */
	uintptr_t vfpContext;			/* 0xc4 */
	uintptr_t userFrame;			/* 0xc8 */
	uint32_t reserved1;			/* 0xcc: zeroed */
	uintptr_t tls;				/* 0xd0 */
	ThreadTlsSlotMemory tlsSlotMemory;	/* 0xd4 */
	uint32_t timeout;			/* 0xf4 */
	uint32_t signalPending;			/* 0xf8: THREAD_FLAG_SIGNALED */
	int callbackNest;			/* 0xfc */
	uintptr_t context;			/* 0x100: the thread context */
	uint32_t flags;				/* 0x104: level 5: the thread's THREAD_FLAG_* */
	uint32_t waitDetail[4];			/* 0x108: by wait type */
	uint32_t reserved2[4];
} ThreadmgrThreadInfoForDebugger;

_Static_assert(sizeof(ThreadmgrThreadInfoForDebugger) == 0x128, "debugger thread info");

int SceThreadmgrForDriver_555CA6BB(SceUID threadId, uint32_t flags,
				   ThreadmgrThreadInfoForDebugger *info);
int SceThreadmgrForKernel_C976719E(SceUID threadId, uint32_t flags,
				   ThreadmgrThreadInfoForDebugger *info);
int SceThreadmgrForKernel_BFF892B4(SceUID threadId, uint32_t flags,
				   ThreadmgrThreadInfoForDebugger *info);
int SceThreadmgrForKernel_B011EA69(uint32_t flags, ThreadmgrThreadInfoForDebugger *infos,
				   SceSize size, SceSize *count);
int SceThreadmgrForKernel_6351A13A(SceUID pid, uint32_t flags,
				   ThreadmgrThreadInfoForDebugger *infos, SceSize size,
				   SceSize *count);
/* vitasdk: ksceKernelGetThreadInfoForDebugger */
int SceThreadmgrForDriver_DD8D9429(SceUID threadId, uint32_t flags,
				   ThreadmgrThreadInfoForDebugger *info);

/* Entries returned by sceKernelGetWaitingThreadList. */
typedef struct ThreadmgrWaitingThread {
	SceUID threadId;
	SceUID processId;
	SceUID userThreadId;
	uint32_t reserved;
} ThreadmgrWaitingThread;

_Static_assert(sizeof(ThreadmgrWaitingThread) == 0x10, "waiting thread entry size");

int sceKernelGetWaitingThreadList(SceUID waitId, ThreadmgrWaitingThread *list,
				  SceSize size, SceSize *count);
