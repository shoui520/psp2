/* Native UID cache/waitable construction and class method foundations. */
#include <string.h>
#include "private.h"
#include "uid_waitable.h"
#include "sce/debug.h"
#include "syscall_context.h"

#undef __FILE_NAME__
#define __FILE_NAME__ "threadmgr.c"

/* UID-cache objects carry a link at +8, before the waitable base exists. */
struct ThreadmgrCacheObject {
	SceObjectBase base;
	uintptr_t next;
	uintptr_t prev;
};

_Static_assert(offsetof(struct ThreadmgrCacheObject, next) == 8,
	       "ThreadMgr cache link offset");

_Static_assert(sizeof(struct ThreadmgrWaitableClass) == 0x38,
	       "ThreadMgr extended class size");
_Static_assert(sizeof(struct ThreadmgrWaitableObject) == 0x28,
	       "ThreadMgr waitable object size");

/* the class a class extends */
static inline struct ThreadmgrWaitableClass *parentClass(void *cls)
{
	return (struct ThreadmgrWaitableClass *)((SceClass *)cls)->uidclass;
}

int uidCacheConstructor(void *object)
{
	volatile struct ThreadmgrCacheObject *cache = object;

	cache->next = 0;
	cache->prev = (uintptr_t)&cache->next;
	return 0;
}

int uidCacheDestructor(void *object)
{
	(void)object;
	return 0;
}

int uidWaitableConstructor(void *object)
{
	volatile struct ThreadmgrWaitableObject *waitable = object;

	waitable->lock = 1;
	waitable->next = (uintptr_t)&waitable->next;
	waitable->prev = (uintptr_t)&waitable->next;
	waitable->attributes = 0;
	waitable->reserved = 0;
	waitable->waiting = 0;
	return 0;
}

/* the default methods: the parent class's */
int uidDefaultCancelWait(void *cls, void *object, uint32_t a2)
{
	struct ThreadmgrWaitableClass *parent = parentClass(cls);

	return parent->cancelWait(parent, object, a2);
}

int uidWaitable_ReleaseMethod(void *cls, void *object, struct ThreadCB *thread)
{
	volatile struct ThreadmgrWaitableObject *waitable = object;
#if FW_DEBUG
	if (ksceKernelGetAssertLevel() > 0) {
		SCE_DBG_ASSERT_LEVEL(1, ThreadmgrIntrDisabled(), 11612);
		if (ksceKernelGetAssertLevel() > 0) {
			SCE_DBG_ASSERT_LEVEL(1, waitable->lock != 0, 11615);
			if (ksceKernelGetAssertLevel() > 0)
				SCE_DBG_ASSERT_LEVEL(1, waitable->waiting != 0, 11618);
		}
	}
#endif
	(void)cls;
	/* Its wider meaning remains unknown; native release decrements this word. */
	waitable->waiting--;
	return 0;
}

int uidDefault_RevalidateMethod(void *cls, void *object)
{
	struct ThreadmgrWaitableObject *waitable = object;
	struct ThreadmgrWaitableClass *parent;

	SCE_DBG_ASSERT_CHECK(ThreadmgrIntrDisabled(), 11638);
	SCE_DBG_ASSERT_CHECK(waitable->lock != 0, 11641);
	parent = parentClass(cls);
	return parent->revalidate(parent, object);
}

int uidDefault_ReleaseMethod(void *cls, void *object, struct ThreadCB *thread)
{
	struct ThreadmgrWaitableObject *waitable = object;
	struct ThreadmgrWaitableClass *parent;

	SCE_DBG_ASSERT_CHECK(ThreadmgrIntrDisabled(), 11655);
	SCE_DBG_ASSERT_CHECK(waitable->lock != 0, 11658);
	parent = parentClass(cls);
	return parent->release(parent, object, thread);
}

/* SceUIDWaitableClass: the waitable's queue and state cleared */
int uidWaitableDestructor(void *object)
{
	struct ThreadmgrWaitableObject *waitable = object;

	memset(&waitable->next, 0,
	       sizeof(*waitable) - offsetof(struct ThreadmgrWaitableObject, next));
	return 0;
}

int uidWaitable_RevalidateMethod(void *cls, void *object)
{
#if FW_DEBUG
	volatile struct ThreadmgrWaitableObject *waitable = object;
	if (ksceKernelGetAssertLevel() > 0) {
		SCE_DBG_ASSERT_LEVEL(1, ThreadmgrIntrDisabled(), 11595);
		if (ksceKernelGetAssertLevel() > 0)
			SCE_DBG_ASSERT_LEVEL(1, waitable->lock != 0, 11598);
	}
#else
	(void)object;
#endif
	(void)cls;
	return 0;
}

__attribute__((noinline, noreturn))
void uidCacheClassInit(void)
{
	SCE_PANIC(THREADMGR_UID_CACHE_CLASS_INIT_LINE);
}

int uidExtendedClassInit(struct ThreadmgrWaitableClass *cls, const char *name,
			SceClass *parent, SceSize itemsize,
			SceClassCallback create, SceClassCallback destroy,
			ThreadmgrCancelWaitMethod cancelWait, ThreadmgrRevalidateMethod revalidate,
			ThreadmgrReleaseMethod release)
{
	int result;

	result = ksceUIDClassInitClass(&cls->base, name, parent, itemsize,
				     create, destroy);
	if (result < 0)
		uidCacheClassInit();
	cls->cancelWait = cancelWait;
	cls->revalidate = revalidate;
	cls->release = release;
	return result;
}
