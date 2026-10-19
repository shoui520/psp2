/*
 * SceKernelThreadMgr: the mutex object (SceUIDMutexObject), shared with the
 * condition variables that wait on it.
 */

#ifndef THREADMGR_MUTEX_H
#define THREADMGR_MUTEX_H

#include <stdint.h>
#include <scetypes.h>
#include <sysmem/spinlock.h>
#include <threadmgr/mutex.h>
#include "private.h"
#include <threadmgr/uid_waitable.h>
#include "priority_change.h"

#define MUTEX_ATTR_KERNEL		0x00000040	/* created by the kernel: no boost limits */
#define MUTEX_ATTR_UNLIMITED		0x00004000

/* the mutex state proper (0x30), cleared by the constructor; releaseMutex works on it */
typedef struct ThreadmgrMutexCore {
	uint32_t reserved0;			/* 0x28: zeroed */
	SceUID mutexId;				/* 0x2c: printed as the mutex ID; -1 or a process-type UID */
	ListNode ownerLink;	/* 0x30: in the owner's ownedMutex list */
	int initCount;				/* 0x38 */
	int lockCount;				/* 0x3c */
	struct ThreadCB *owner;	/* 0x40 */
	int ceilingPriority;			/* 0x44 */
	ListNode conds;		/* 0x48: the condition variables bound to it */
	SceUID puid;				/* 0x50: the creating process's PUID */
	uint32_t reserved1;
} ThreadmgrMutexCore;

/* SceUIDMutexObject (0x58) */
typedef struct ThreadmgrMutex {
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
	ThreadmgrMutexCore core;		/* 0x28 */
} ThreadmgrMutex;

/* SceUIDCondObject (0x40) */
typedef struct ThreadmgrCond {
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
	const void *option;			/* 0x28: from here on cleared by the constructor */
	SceUID mutexId;				/* 0x2c: -1 once the mutex is gone */
	ListNode mutexLink;	/* 0x30: in the mutex's conds list */
	SceUID waitCondId;			/* 0x38: PLACEHOLDER: -1, matched against waiters' */
	SceUID puid;				/* 0x3c */
} ThreadmgrCond;

#define COND_FROM_MUTEX_LINK(link) \
	((ThreadmgrCond *)((uintptr_t)(link) - offsetof(ThreadmgrCond, mutexLink)))
#define MUTEX_FROM_OWNER_LINK(link) \
	((ThreadmgrMutex *)((uintptr_t)(link) - offsetof(ThreadmgrMutex, core.ownerLink)))

_Static_assert(sizeof(ThreadmgrMutex) == 0x58, "SceUIDMutexObject size");

#define PRIORITY_USER_LOW		0x80	/* user priorities are 0x80..0xfb */
#define PRIORITY_USER_COUNT		0x7c
#define PRIORITY_USER_LOWEST		0xfc

typedef void (*ThreadmgrPriorityProbeHandler)(const struct ThreadmgrSchedProbeDescriptor *probe,
	SceUID threadId, SceUID pid, int oldPriority, int newPriority);
typedef void (*ThreadmgrPriorityHook)(SceUID pid, SceUID threadId, int newPriority,
	int oldPriority);

static inline unsigned int ThreadmgrCpuId(void)
{
	uint32_t mpidr;

	__asm__ volatile("mrc p15, 0, %0, c0, c0, 5" : "=r"(mpidr));
	return mpidr & 3;
}

static inline void ListLinkRemove(ListNode *link)
{
	link->next->prev = link->prev;
	link->prev->next = link->next;
}

/* tells the tracers and the hook that a thread's priority moved */
static inline void notifyPriorityChange(struct ThreadCB *thread, int oldPriority)
{
	ThreadmgrPriorityProbeHandler probe;
	ThreadmgrPriorityHook hook;
	SceUID threadId;

	if (s_threadmgrRuntime.manager->paHooks[1])
		issueCallPAPriorityHookIntrRequest(ThreadmgrCpuId(), thread->processId,
			thread->threadId, oldPriority, thread->currentPriority);
	probe = (ThreadmgrPriorityProbeHandler)s_schedChangePriorityHandler;
	if (probe != NULL) {
		threadId = thread->userThreadId;
		if (threadId < 0)
			threadId = thread->threadId;
		probe(&s_schedChangePriorityDescriptor, threadId,
		      thread->process->pid, oldPriority,
		      thread->currentPriority);
	}
	hook = (ThreadmgrPriorityHook)s_threadmgrRuntime.priorityChangeHook;
	if (hook != NULL) {
		threadId = thread->userThreadId;
		if (threadId < 0)
			threadId = thread->threadId;
		hook(thread->process->pid, threadId,
		     thread->currentPriority, oldPriority);
	}
}

int getMutexInfo(SceUID mutexId, SceKernelMutexInfo *info);
void acquireMutex(ThreadmgrMutex *mutex, struct ThreadCB *thread, int count);
int releaseMutex(ThreadmgrMutexCore *core);
int findNextCeilingPriority(struct ThreadCB *thread);
uint32_t releaseAllMutex(struct ThreadCB *thread);
int mutexProcessExit(SceProcessObject *process);
int lockMutexForCond(SceUID mutexId, int count, SceUInt32 *timeout, int callbacks, SceUID condId);
int unlockMutexForCond(SceUID mutexId, int count);

/* mutex.c */
void startMutex(void);

#endif
