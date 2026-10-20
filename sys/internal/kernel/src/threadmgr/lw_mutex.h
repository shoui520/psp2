/*
 * SceKernelThreadMgr: lightweight mutexes. The lock word lives in the user's
 * work area (SceKernelLwMutexWork); the kernel object only queues waiters.
 */

#ifndef THREADMGR_LW_MUTEX_H
#define THREADMGR_LW_MUTEX_H

#include "private.h"
#include "mutex.h"
#include <threadmgr/lw_mutex.h>

#define LW_MUTEX_ATTR_UNLIMITED		0x00004000

/* the user's SceKernelLwCondWork (0x10) */
typedef struct ThreadmgrLwCondWork {
	SceUID uid;				/* the lw cond's PUID */
	SceUID lwMutexId;			/* the lw mutex's PUID */
	uint32_t reserved[2];			/* zeroed */
} ThreadmgrLwCondWork;

/* SceUIDLwMutexObject (0x60); freed ones wait in their process's cache */
typedef struct SceUIDLwMutexObject {
	SceUID ownerPid;			/* 0x00: the object base's first word */
	SceClass *cls;
	struct UIDEntry *uidEntry;		/* 0x08: the GUID entry */
	void *cacheNext;			/* 0x0c: the next cached object; &uidEntry while in use */
	struct ThreadmgrWaitQueue *next;	/* 0x10: the waiters (ThreadmgrWaitQueue from here) */
	struct ThreadmgrWaitQueue *prev;
	SceKernelSpinlock lock;			/* 0x18 */
	uint32_t waiting;			/* 0x1c */
	uint32_t attr;				/* 0x20 */
	uint32_t reserved;			/* 0x24: zeroed */
	SceUID guid;				/* 0x28: from here on cleared by the destructor */
	int initCount;
	ThreadmgrLwMutexWork *work;		/* 0x30: user address */
	SceUID pid;				/* 0x34 */
	ListNode conds;		/* 0x38: the lw conds bound to it */
	char name[32];				/* 0x40 */
} SceUIDLwMutexObject;

_Static_assert(sizeof(SceUIDLwMutexObject) == 0x60, "SceUIDLwMutexObject size");

/* SceUIDLwCondObject (0x48) */
typedef struct SceUIDLwCondObject {
	SceUID ownerPid;			/* 0x00: the object base's first word */
	SceClass *cls;
	struct UIDEntry *uidEntry;		/* 0x08: the GUID entry */
	void *cacheNext;			/* 0x0c: the next cached object; &uidEntry while in use */
	struct ThreadmgrWaitQueue *next;	/* 0x10: the waiters (ThreadmgrWaitQueue from here) */
	struct ThreadmgrWaitQueue *prev;
	SceKernelSpinlock lock;			/* 0x18 */
	uint32_t waiting;			/* 0x1c */
	uint32_t attr;				/* 0x20 */
	uint32_t reserved0;			/* 0x24: zeroed */
	const void *option;			/* 0x28: from here on cleared by the constructor */
	SceUID lwMutexId;			/* 0x2c: -1 once the lw mutex is gone */
	void *work;				/* 0x30: the user's SceKernelLwCondWork */
	ThreadmgrLwMutexWork *lwMutexWork;	/* 0x34 */
	ListNode lwMutexLink;	/* 0x38: in the lw mutex's conds list */
	SceUID guid;				/* 0x40 */
	uint32_t reserved1;
} SceUIDLwCondObject;

_Static_assert(sizeof(SceUIDLwCondObject) == 0x48, "SceUIDLwCondObject size");

#define LW_COND_FROM_MUTEX_LINK(link) \
	((SceUIDLwCondObject *)((uintptr_t)(link) - offsetof(SceUIDLwCondObject, lwMutexLink)))

int lockLwMutexForLwCond(SceUID lwMutexId, int count, SceUInt32 *timeout, int callbacks,
			 SceUID condId);
int unlockLwMutexForLwCond(SceUID lwMutexId, int count);

/* lw_mutex.c */
void startLwMutex(void);
int lwMutexProcessExit(SceProcessObject *process);
int _sceKernelCreateLwMutexForUser(ThreadmgrLwMutexWork *work, const char *name, SceUInt32 attr,
				   int initCount, const SceKernelLwMutexOptParam *option);
int sceKernelDeleteLwMutexForUser(ThreadmgrLwMutexWork *work);
int _sceKernelLockLwMutex(ThreadmgrLwMutexWork *work, int count, SceUInt32 *timeout, int callbacks);
int _sceKernelUnlockLwMutex(ThreadmgrLwMutexWork *work, int count);
int uidLwMutex_DeleteCache(SceProcessObject *process, int target);
int uidLwMutex_ManageCache(SceProcessObject *process, int high, int low, int target);

#endif
