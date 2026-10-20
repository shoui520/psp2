/*
 * SceKernelThreadMgr: fast mutexes. The mutex lives in the caller's memory
 * (the work area, SceKernelFastMutex); a small UID object carries its wait
 * queue. An uncontended lock only takes the work area's spinlock.
 */

#ifndef THREADMGR_FAST_MUTEX_H
#define THREADMGR_FAST_MUTEX_H

#include "private.h"
#include <threadmgr/uid_waitable.h>
#include <threadmgr/fast_mutex.h>

#define FAST_MUTEX_ATTR_RECURSIVE	0x00000002
#define FAST_MUTEX_ATTR_CEILING		0x00000004
#define FAST_MUTEX_ATTR_LOCKED		0x00000008	/* created locked by the caller */

/* SceUIDFastMutexObject (0x30) */
struct SceUIDFastMutexObject {
	SceUID ownerPid;			/* 0x00 */
	SceClass *cls;
	struct UIDEntry *uidEntry;		/* 0x08: the GUID entry */
	void *cacheNext;			/* 0x0c: the next cached object; &uidEntry while in use */
	struct ThreadmgrWaitQueue *next;	/* 0x10: the waiters (ThreadmgrWaitQueue from here) */
	struct ThreadmgrWaitQueue *prev;
	SceKernelSpinlock lock;			/* 0x18 */
	uint32_t waiting;			/* 0x1c */
	uint32_t attr;				/* 0x20 */
	uint32_t reserved0;			/* 0x24: zeroed */
	uint32_t reserved1;
	/* 0x2c: the work area; Sony clears it byte by byte */
	union {
		struct ThreadmgrFastMutex *workArea;
		uint8_t workAreaBytes[4];
	};
};

int findFastMutexNextBoostPriority(struct ThreadCB *thread);
void acquireFastMutex(struct ThreadmgrFastMutex *mutex, struct ThreadCB *thread);

_Static_assert(sizeof(struct SceUIDFastMutexObject) == 0x30, "SceUIDFastMutexObject size");
_Static_assert(sizeof(struct ThreadmgrFastMutex) == 0x40, "fast-mutex work area size");
_Static_assert(offsetof(struct ThreadmgrFastMutex, spinWaitCount) == 0x14,
	       "fast-mutex spin waiter count");
_Static_assert(offsetof(struct ThreadCB, ownedFastMutexNext) == 0x68,
	       "thread owned fast-mutex list");

/* fast_mutex.c */
void startFastMutex(void);
uint32_t releaseAllFastMutex(struct ThreadCB *thread);

#endif
