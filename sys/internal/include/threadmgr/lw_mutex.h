/*
 * SceKernelThreadMgr: lightweight mutexes and condition variables. The info structures are the
 * SDK's (threadmgr/threadmgr.h); the work area, opaque in the SDK (SceKernelLwMutexWork), is
 * laid out here as the kernel reads it.
 */
#pragma once

#include <threadmgr/threadmgr.h>

typedef struct ThreadmgrLwMutexWork {
	SceUID owner;
	uint32_t reserved0;
	int lockCount;
	uint32_t attr;
	SceUID uid;
	uint32_t reserved1[3];
} ThreadmgrLwMutexWork;

#ifndef __cplusplus	/* C only; g++ 4.8 (C++98) has no static_assert */
_Static_assert(sizeof(ThreadmgrLwMutexWork) == 0x20, "lw mutex work size");
#endif

int ksceKernelGetLwMutexInfo(SceUID lwMutexId, SceKernelLwMutexInfo *info);
int sceKernelGetLwCondInfo(SceUID condId, SceKernelLwCondInfo *info);
int ksceKernelGetLwCondInfo(SceUID condId, SceKernelLwCondInfo *info);
