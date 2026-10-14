/* Extended ThreadMgr UID classes; callback roles follow native class use. */
#pragma once

#include <stdint.h>
#include <psp2kern/kernel/sysmem/uid_class.h>

struct ThreadCB;

/* the methods an extended class adds after SceClass; each defaults to its parent's */
typedef int (*ThreadmgrCancelWaitMethod)(void *cls, void *object, uint32_t a2);
typedef int (*ThreadmgrRevalidateMethod)(void *cls, void *object);
typedef int (*ThreadmgrReleaseMethod)(void *cls, void *object, struct ThreadCB *thread);

struct ThreadmgrWaitableClass {
	SceClass base;
	ThreadmgrCancelWaitMethod cancelWait;
	ThreadmgrRevalidateMethod revalidate;
	ThreadmgrReleaseMethod release;
};

/* the waiters of a waitable object (its +0x10): threads, or nodes pointing at one */
struct ThreadmgrWaitNode {
	struct ThreadmgrWaitNode *next;
	struct ThreadmgrWaitNode *prev;
	uintptr_t owner;			/* a thread's own UID (odd), else its context | flags */
};

struct ThreadmgrWaitQueue {
	struct ThreadmgrWaitNode *next;
	struct ThreadmgrWaitNode *prev;
	SceKernelSpinlock lock;			/* 0x08 */
	uint32_t waiting;			/* 0x0c */
	uint32_t attributes;			/* 0x10 */
	uint32_t reserved0;			/* 0x14: the objects' own fields from here, unused here */
	uint32_t pattern;			/* 0x18: event flags: the current pattern */
	uint8_t reserved1[0x14];
	uint32_t readWaiters;			/* 0x30: rw locks */
	uint32_t writeWaiters;			/* 0x34 */
	uint32_t reserved2;
	uint32_t lwKeepFirst;			/* 0x3c: lw objects wait after the first waiter */
	uint8_t reserved3[0xc];
	uint32_t lwBusy;			/* 0x4c: lw objects */
	uint32_t lwWaiters;			/* 0x50 */
};

struct ThreadmgrWaitableObject {
	SceUID ownerPid;			/* the object base */
	SceClass *cls;
	void *uidEntry;				/* 0x08: the GUID entry */
	void *cacheNext;			/* 0x0c: the next cached object; &uidEntry while in use */
	uintptr_t next;				/* 0x10: the waiters */
	uintptr_t prev;
	SceKernelSpinlock lock;			/* 0x18 */
	uint32_t waiting;			/* 0x1c */
	uint32_t attributes;			/* 0x20 */
	uint32_t reserved;			/* 0x24: zeroed */
};

int uidCacheConstructor(void *object);
int uidCacheDestructor(void *object);
int uidWaitableConstructor(void *object);
int uidWaitableDestructor(void *object);
int uidWaitable_RevalidateMethod(void *cls, void *object);
int uidWaitable_ReleaseMethod(void *cls, void *object, struct ThreadCB *thread);
int uidExtendedClassInit(struct ThreadmgrWaitableClass *cls, const char *name,
			SceClass *parent, SceSize itemsize,
			SceClassCallback create, SceClassCallback destroy,
			ThreadmgrCancelWaitMethod cancelWait, ThreadmgrRevalidateMethod revalidate,
			ThreadmgrReleaseMethod release);

/* Default methods preserve native callback forwarding; a2 is unresolved. */
int uidDefaultCancelWait(void *cls, void *object, uint32_t a2);
int uidDefault_RevalidateMethod(void *cls, void *object);
int uidDefault_ReleaseMethod(void *cls, void *object, struct ThreadCB *thread);
