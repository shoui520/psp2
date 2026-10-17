/* SceKernelThreadMgr event object, shared with inherited event classes. */
#pragma once

#include <stdint.h>
#include <psp2kern/types.h>
#include <sysmem/sysmem.h>

struct ThreadmgrWaitNode;

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
	void *uidEntry;
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
