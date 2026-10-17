/* SceKernelThreadMgr: the fast-mutex work area (SceKernelFastMutex) a caller owns. */
#pragma once

#include <stdint.h>
#include <psp2kern/types.h>

struct ThreadCB;
struct ThreadmgrWaitQueue;

struct ThreadmgrFastMutex {
	struct ThreadmgrFastMutex *next;	/* 0x00: owner's fast-mutex list */
	struct ThreadmgrFastMutex *prev;
	SceUID uid;
	struct ThreadmgrWaitQueue *queue;
	SceKernelSpinlock lock;
	uint32_t spinWaitCount;
	int lockCount;
	struct ThreadCB *owner;
	int ceilingPriority;
	uint32_t magic;
	uint8_t reserved[0x18];
};

_Static_assert(sizeof(struct ThreadmgrFastMutex) == 0x40,
	       "fast-mutex work area size");
