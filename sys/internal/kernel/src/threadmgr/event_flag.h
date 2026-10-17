/*
 * SceKernelThreadMgr: the event flag object (SceUIDEventFlagObject).
 */

#ifndef THREADMGR_EVENT_FLAG_H
#define THREADMGR_EVENT_FLAG_H

#include <stdint.h>
#include <psp2kern/types.h>
#include "private.h"
#include <threadmgr/uid_waitable.h>
#include "mutex.h"

#define EVF_ATTR_LW			0x00000080
#define EVF_ATTR_MULTI			0x00001000	/* several threads may wait at once */
#define EVF_ATTR_PRIORITY		0x00002000
#define EVF_ATTR_UNLIMITED		0x00004000
#define EVF_WAITMODE_OR			0x00000001
#define EVF_WAITMODE_CLEAR_ALL		0x00000002
#define EVF_WAITMODE_CLEAR_PAT		0x00000004

/* the event flag state proper (0x18), cleared by the constructor */
typedef struct ThreadmgrEventFlagCore {
	uint32_t pattern;			/* 0x28 */
	uint32_t initPattern;			/* 0x2c */
	struct ThreadCB *holder;	/* 0x30: the one waiter of a non-multi flag */
	int holdCount;				/* 0x34 */
	ListNode holderLink;	/* 0x38: in the holder's heldEventFlag list */
} ThreadmgrEventFlagCore;

/* SceUIDEventFlagObject (0x40) */
typedef struct ThreadmgrEventFlag {
	SceUID ownerPid;			/* 0x00: the object base's first word */
	SceClass *cls;
	void *uidEntry;				/* 0x08: the GUID entry */
	void *cacheNext;			/* 0x0c: the next cached object; &uidEntry while in use */
	struct ThreadmgrWaitQueue *next;	/* 0x10: the waiters (ThreadmgrWaitQueue from here) */
	struct ThreadmgrWaitQueue *prev;
	SceKernelSpinlock lock;			/* 0x18 */
	uint32_t waiting;			/* 0x1c */
	uint32_t attr;				/* 0x20 */
	uint32_t reserved;			/* 0x24: zeroed */
	ThreadmgrEventFlagCore core;		/* 0x28 */
} ThreadmgrEventFlag;

_Static_assert(sizeof(ThreadmgrEventFlag) == 0x40, "SceUIDEventFlagObject size");

#define EVENT_FLAG_FROM_HOLDER_LINK(link) \
	((ThreadmgrEventFlag *)((uintptr_t)(link) - offsetof(ThreadmgrEventFlag, core.holderLink)))

/* ksceKernelGetEventFlagInfo (0x38) */
typedef struct ThreadmgrEventFlagInfo {
	SceSize size;
	SceUID evfId;
	char name[32];
	uint32_t attr;				/* 0x28 */
	uint32_t initPattern;
	uint32_t currentPattern;
	int numWaitThreads;			/* 0x34 */
} ThreadmgrEventFlagInfo;

int getEventFlagInfo(SceUID evfId, ThreadmgrEventFlagInfo *info);
int waitEventFlag(SceUID evfId, uint32_t pattern, uint32_t mode, uint32_t *outPattern,
		  SceUInt32 *timeout, int callbacks);
void releaseAllEventFlag(struct ThreadCB *thread);
int eventFlagProcessExit(SceProcessObject *process);
int ksceKernelDeleteEventFlag(SceUID evfId);
int ksceKernelPollEventFlag(SceUID evfId, uint32_t pattern, uint32_t mode, uint32_t *outPattern);
int ksceKernelSetEventFlag(SceUID evfId, uint32_t pattern);
int ksceKernelClearEventFlag(SceUID evfId, uint32_t pattern);
int sceKernelCancelEventFlag(SceUID evfId, uint32_t setPattern, int *numWaitThreads);
int ksceKernelGetEventFlagInfo(SceUID evfId, ThreadmgrEventFlagInfo *info);

#endif
