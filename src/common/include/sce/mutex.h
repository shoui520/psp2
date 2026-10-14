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

_Static_assert(sizeof(ThreadmgrMutexInfo) == 0x40, "kernel mutex info size");

int ksceKernelGetMutexInfo(SceUID mutexId, ThreadmgrMutexInfo *info);
