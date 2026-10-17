/* SceKernelThreadMgr: complete kernel mutex information. */
#pragma once

#include <stdint.h>
#include <psp2kern/types.h>

/* ksceKernelGetMutexInfo (0x40) */
typedef struct ThreadmgrMutexInfo {
	SceSize size;
	SceUID mutexId;
	char name[32];
	uint32_t attr;				/* 0x28 */
	int initCount;
	int currentCount;
	SceUID currentOwnerId;			/* 0x34 */
	int numWaitThreads;
	int ceilingPriority;			/* 0x3c */
} ThreadmgrMutexInfo;

#ifndef __cplusplus	/* C only; g++ 4.8 (C++98) has no static_assert */
_Static_assert(sizeof(ThreadmgrMutexInfo) == 0x40, "kernel mutex info size");
#endif

int ksceKernelGetMutexInfo(SceUID mutexId, ThreadmgrMutexInfo *info);
