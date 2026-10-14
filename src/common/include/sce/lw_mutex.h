/* SceKernelThreadMgr: complete lightweight synchronization information. */
#pragma once

#include <stdint.h>
#include <psp2kern/types.h>

typedef struct ThreadmgrLwMutexWork {
	SceUID owner;
	uint32_t reserved0;
	int lockCount;
	uint32_t attr;
	SceUID uid;
	uint32_t reserved1[3];
} ThreadmgrLwMutexWork;

typedef struct ThreadmgrLwMutexInfo {
	SceSize size;
	SceUID uid;
	char name[32];
	uint32_t attr;
	ThreadmgrLwMutexWork *work;
	int initCount;
	int currentCount;
	SceUID currentOwnerId;
	int numWaitThreads;
} ThreadmgrLwMutexInfo;

typedef struct ThreadmgrLwCondInfo {
	SceSize size;
	SceUID uid;
	char name[32];
	uint32_t attr;
	void *work;
	ThreadmgrLwMutexWork *lwMutexWork;
	int numWaitThreads;
} ThreadmgrLwCondInfo;

_Static_assert(sizeof(ThreadmgrLwMutexWork) == 0x20, "lw mutex work size");
_Static_assert(sizeof(ThreadmgrLwMutexInfo) == 0x40, "lw mutex info size");
_Static_assert(sizeof(ThreadmgrLwCondInfo) == 0x38, "lw condition info size");

int ksceKernelGetLwMutexInfo(SceUID lwMutexId, ThreadmgrLwMutexInfo *info);
int sceKernelGetLwCondInfo(SceUID condId, ThreadmgrLwCondInfo *info);
int ksceKernelGetLwCondInfo(SceUID condId, ThreadmgrLwCondInfo *info);
