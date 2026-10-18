/* SceKernelThreadMgr: the fast-mutex work area (SceKernelFastMutex) a caller owns. */
#pragma once

#include <stdint.h>
#include <scetypes.h>
#include <sysmem/spinlock.h>

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

#ifndef __cplusplus
_Static_assert(sizeof(struct ThreadmgrFastMutex) == 0x40,
	       "fast-mutex work area size");
#endif

/*
 * The work area as its callers declare it (vitasdk's name, nothing of Sony's names it): opaque,
 * the size of struct ThreadmgrFastMutex.
 */
typedef struct SceKernelFastMutex {
	SceUInt8 data[0x40];
} SceKernelFastMutex;

#ifndef __cplusplus
_Static_assert(sizeof(SceKernelFastMutex) == sizeof(struct ThreadmgrFastMutex),
	       "SceKernelFastMutex size");
#endif
