/* SceKernelThreadMgr: caller-owned fast-mutex work area. */
#pragma once

#include <stdint.h>
#include <psp2kern/types.h>

#include <threadmgr/fast_mutex_object.h>
#include <sysmem/cpu.h>

/* what ksceKernelGetFastMutexInfo gives (0x44); lockCount is Sony's name ("inf.lockCount") */
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
_Static_assert(sizeof(ThreadmgrFastMutexInfo) == 0x44, "fast-mutex info size");
