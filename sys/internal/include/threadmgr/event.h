/* SceKernelThreadMgr event object, shared with inherited event classes. */
#pragma once

#include <stdint.h>
#include <scetypes.h>
#include <sysmem/spinlock.h>
#include <sysmem/sysmem.h>

struct ThreadmgrWaitNode;
struct ThreadmgrWaitableClass;		/* threadmgr/uid_waitable.h */

typedef struct ThreadmgrEventCallback {
	struct ThreadmgrEventCallback *next;
	struct ThreadmgrEventCallback *prev;
	SceUID callbackId;
	uint32_t notifyId;
	SceUID threadId;
	uint32_t notifiedCpus;
} ThreadmgrEventCallback;

/* SceUIDEventObject; subclasses begin immediately after userData. */
typedef struct ThreadmgrEvent {
	SceUID ownerPid;
	SceClass *cls;
	struct UIDEntry *uidEntry;
	void *cacheNext;
	struct ThreadmgrWaitNode *next;
	struct ThreadmgrWaitNode *prev;
	SceKernelSpinlock lock;
	uint32_t waiting;
	uint32_t attr;
	uint32_t pattern;
	ThreadmgrEventCallback *callbackNext;
	ThreadmgrEventCallback *callbackPrev;
	uint64_t userData __attribute__((packed, aligned(4)));
} ThreadmgrEvent;

_Static_assert(sizeof(ThreadmgrEvent) == 0x38, "SceUIDEventObject size");

/* event.c: a subclass of SceUIDEventClass, and a new object of one (it comes locked) */
int sceKernelUIDInheritedEventClassInit(struct ThreadmgrWaitableClass *cls, const char *name,
				   SceSize size, SceClassCallback create, SceClassCallback destroy);
int sceKernelEventObjectInit(SceUID eventId, uint32_t attr, uint64_t userData, uint32_t pattern);
