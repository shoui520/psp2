/*
 * SceKernelThreadMgr: fast_mutex.c, the fast mutexes (SceUIDFastMutexClass).
 * A waiter first spins on the work area's lock (wait type 0x2000) and only
 * sleeps on the UID object's queue when the lock is free but still owned.
 */

#include "timer.h"
#include <sysmem/cpu.h>
#include <sysmem/debug.h>
#include <sysmem/sysmem.h>
#include "scheduler.h"
#include "fast_mutex.h"
#include "mutex.h"
#include "syscall_context.h"
#include <sysmem/sysclib.h>
#include <intrmgr/intrmgr.h>

#define FAST_MUTEX_ITEM_SIZE		0x80	/* the class's object allocation size */
#define FAST_MUTEX_ATTR_MASK		0x0000a00e
#define FAST_MUTEX_OPTION_MAX_SIZE	8

#define FAST_MUTEX_UID_OBJECT(queue) \
	((struct SceUIDFastMutexObject *)((uintptr_t)(queue) - offsetof(struct SceUIDFastMutexObject, next)))

/* SceUIDFastMutexClass's constructor */
static int uidFastMutex_Constructor(void *object)
{
	struct SceUIDFastMutexObject *uidMutex = object;

	uidMutex->workAreaBytes[0] = 0;
	uidMutex->workAreaBytes[1] = 0;
	uidMutex->workAreaBytes[2] = 0;
	uidMutex->workAreaBytes[3] = 0;
	return 0;
}

/* SceUIDFastMutexClass's destructor */
static int uidFastMutex_Destructor(void *object)
{
	struct SceUIDFastMutexObject *uidMutex = object;

	SCE_DBG_ASSERT_CHECK(ThreadmgrIntrDisabled(), 1536);
	uidMutex->workAreaBytes[0] = 0;
	uidMutex->workAreaBytes[1] = 0;
	uidMutex->workAreaBytes[2] = 0;
	uidMutex->workAreaBytes[3] = 0;
	return 0;
}

/* a work area's information, unlocked (the callers hold the UID object's lock) */
static int getFastMutexInfo(struct ThreadmgrFastMutex *mutex, ThreadmgrFastMutexInfo *info)
{
	struct ThreadmgrWaitQueue *queue;
	const char *name;

	if (mutex == NULL)
		return -1;
	if (mutex->magic != FAST_MUTEX_MAGIC)
		return SCE_KERNEL_ERROR_FAST_MUTEX_NOT_INITIALIZED;
	*info = (ThreadmgrFastMutexInfo){ .size = sizeof(*info) };
	name = SceSysmemForDriver_E655852F(mutex->uid);
	if (name != NULL)
		strncpy(info->name, name, sizeof(info->name) - 1);
	queue = mutex->queue;
	info->uid = mutex->uid;
	info->lockCount = mutex->lockCount;
	info->attr = queue->attributes;
	info->workArea = mutex;
	if (mutex->owner != NULL)
		info->currentOwnerId = mutex->owner->threadId;
	info->ceilingPriority = mutex->ceilingPriority;
	info->numWaitThreads = queue->waiting;
	info->numSpinThreads = mutex->spinWaitCount;
	return 0;
}

/* the ceiling of the first (highest-ceiling) fast mutex a thread holds */
int findFastMutexNextBoostPriority(struct ThreadCB *thread)
{
	struct ThreadmgrFastMutex *first;

	SCE_DBG_ASSERT_CHECK(thread->lock != 0, 110);
	first = (struct ThreadmgrFastMutex *)thread->ownedFastMutexNext;
	if (first != (struct ThreadmgrFastMutex *)&thread->ownedFastMutexNext &&
	    (first->queue->attributes & FAST_MUTEX_ATTR_CEILING))
		return first->ceilingPriority;
	return PRIORITY_NONE;
}

static int uidFastMutex_RevalidateMethod(void *cls, void *object);

/* SceUIDFastMutexClass */
void startFastMutex(void)
{
	struct SceKernelThreadmgrPrivate *manager = s_threadmgrRuntime.manager;
	struct ThreadmgrWaitableClass *cls;
	int ret;

	cls = sceKernelAllocHeapMemory(manager->heapId, 0x38);
	if (cls == NULL)
		SCE_PANIC(538);
	ret = uidWaitableClassInit(cls, "SceUIDFastMutexClass",
		s_threadmgrRuntime.manager->waitableClass, FAST_MUTEX_ITEM_SIZE,
		uidFastMutex_Constructor, (SceClassCallback)uidFastMutex_Destructor,
		uidDefaultCancelWait, uidFastMutex_RevalidateMethod,
		uidDefault_ReleaseMethod);
	if (ret < 0)
		SCE_PANIC_FUNC(SCE_LINE(1538, 1509), "uidFastMutexClassInit");
	s_threadmgrRuntime.manager->fastMutexClass = (SceClass *)cls;
	SCE_DBG_PRINTF_LEVEL(2, 0, 554, " sizeof(SceUIDFastMutexObject)=%d\n",
			     sizeof(struct SceUIDFastMutexObject));
}

/* gives the work area to thread (both locked), boosting it to a ceiling */
void acquireFastMutex(struct ThreadmgrFastMutex *mutex, struct ThreadCB *thread)
{
	struct ThreadCB *current = currentThreadContext();
	struct ThreadmgrFastMutex *head = (struct ThreadmgrFastMutex *)&thread->ownedFastMutexNext;
	struct ThreadmgrFastMutex *link;
	int ceiling, priority, oldPriority;

	SCE_DBG_ASSERT_CHECK(ThreadmgrIntrDisabled(), 578);
	SCE_DBG_ASSERT_CHECK(mutex->queue->lock != 0, 581);
	SCE_DBG_ASSERT_CHECK(thread->lock != 0, 584);
	if (thread == current)
		SCE_DBG_ASSERT_CHECK(mutex->lock != 0, 588);
	mutex->owner = thread;
	mutex->lockCount++;
	if (!(mutex->queue->attributes & FAST_MUTEX_ATTR_CEILING)) {
		mutex->next = head;
		mutex->prev = head->prev;
		head->prev = mutex;
		mutex->prev->next = mutex;
		return;
	}
	ceiling = mutex->ceilingPriority;
	for (link = head->next; link != head && link->ceilingPriority <= ceiling; link = link->next)
		;
	mutex->next = link;
	mutex->prev = link->prev;
	link->prev = mutex;
	priority = thread->boostPriority <= ceiling ? thread->boostPriority : ceiling;
	mutex->prev->next = mutex;
	oldPriority = thread->currentPriority;
	if (ceiling < thread->boostPriority)
		thread->boostPriority = priority;
	if (oldPriority <= priority)
		return;
	if (!s_threadmgrRuntime.manager->testMode) {
		if ((uint32_t)oldPriority - PRIORITY_USER_LOW < PRIORITY_USER_COUNT) {
			if ((uint32_t)priority - PRIORITY_USER_LOW >= PRIORITY_USER_COUNT) {
				priority = PRIORITY_USER_LOW;
				thread->boostPriority = priority;
			}
		} else if ((uint32_t)priority - PRIORITY_USER_LOW < PRIORITY_USER_COUNT) {
			priority = PRIORITY_USER_LOWEST;
			thread->boostPriority = priority;
		}
	}
	thread->currentPriority = priority;
	if (thread == current)
		currentCpuState()->priority = thread->boostPriority;
	notifyPriorityChange(thread, oldPriority);
}

/* SceUIDFastMutexClass's revalidation: the first waiter gets a free mutex */
static int uidFastMutex_RevalidateMethod(void *cls, void *object)
{
	struct SceUIDFastMutexObject *uidMutex = object;
	struct ThreadmgrFastMutex *mutex = uidMutex->workArea;
	struct ThreadmgrWaitQueue *queue = (struct ThreadmgrWaitQueue *)&uidMutex->next;
	struct ThreadCB *first;

	SCE_DBG_ASSERT_CHECK(ThreadmgrIntrDisabled(), 1553);
	SCE_DBG_ASSERT_CHECK(uidMutex->lock != 0, 1556);
	first = (struct ThreadCB *)queue->next;
	if ((struct ThreadmgrWaitQueue *)first == queue)
		return 0;
	SCE_DBG_ASSERT_PRINTF(first->magic == THREAD_CB_MAGIC, 1563, "*** pWaitCB=%p, "
		"pFastMutex=%p, pThreadCB=%p, pFastMutex->magic=0x%08X, pThreadCB->magic=0x%08X, "
		"&gpThreadMgrCB->threadMgrCBLock=%p\n", queue, mutex, first, mutex->magic,
		first->magic, &s_threadmgrRuntime.manager->threadMgrCBLock);
	if (mutex->owner == NULL) {
		sceKernelSpinlockLowLock(THREAD_LOCK(first));
		acquireFastMutex(mutex, first);
		wakeupAndDistributeThread(first, (uintptr_t)queue, 0);
		sceKernelSpinlockLowUnlock(THREAD_LOCK(first));
	}
	return 0;
}

/*
 * An exiting thread gives up every fast mutex it holds and drops back to its
 * base priority.
 */
uint32_t releaseAllFastMutex(struct ThreadCB *thread)
{
	struct ThreadmgrFastMutex *head = (struct ThreadmgrFastMutex *)&thread->ownedFastMutexNext;
	struct ThreadmgrFastMutex *mutex;
	volatile int delay;
	uint32_t wake = 0;
	int oldPriority;

	SCE_DBG_ASSERT_CHECK(ThreadmgrIntrDisabled(), 669);
	for (;;) {
		sceKernelSpinlockLowLock(THREAD_LOCK(thread));
		for (mutex = head->next; mutex != head; mutex = head->next) {
			if (SceCpuForDriver_5AC9D394(&mutex->queue->lock) < 0)
				break;
			SCE_DBG_ASSERT_CHECK(mutex->lock != 0, 691);
			sceKernelSpinlockLowUnlock(&mutex->lock);
			mutex->lockCount = 0;
			mutex->owner = NULL;
			mutex->next->prev = mutex->prev;
			mutex->prev->next = mutex->next;
			sceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
			if (mutex->spinWaitCount == 0)
				wake |= revalidateWaitState((uintptr_t)mutex->queue);
			sceKernelSpinlockLowUnlock(&mutex->queue->lock);
			sceKernelSpinlockLowLock(THREAD_LOCK(thread));
		}
		if (mutex == head)
			break;
		/* the queue holds its lock while it takes ours: back off */
		sceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
		for (delay = 10; --delay != 0;)
			;
	}
	sceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
	sceKernelSpinlockLowLock(THREAD_LOCK(thread));
	oldPriority = thread->currentPriority;
	thread->boostPriority = PRIORITY_NONE;
	if (oldPriority != thread->basePriority) {
		thread->currentPriority = thread->basePriority;
		wake |= 1;
		currentCpuState()->priority = thread->basePriority;
		notifyPriorityChange(thread, oldPriority);
	}
	sceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
	return wake;
}

int sceKernelInitializeFastMutex(struct ThreadmgrFastMutex *mutex, const char *name,
				 SceUInt32 attr, const ThreadmgrFastMutexOptParam *option)
{
	struct SceUIDFastMutexObject *uidMutex;
	struct ThreadCB *thread;
	uint32_t saved;
	int ceiling, intr, ret;

	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	if (currentThreadContext() == NULL) {
		SCE_DBG_ASSERT_PRINTF_FUNC(0, 774, "sceKernelInitializeFastMutex", "*** error: illegal context. pFastMutex=%p[%s]\n",
				      mutex, SceSysmemForDriver_E655852F(mutex->uid));
		ret = SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
		goto restore;
	}
	if (attr & ~FAST_MUTEX_ATTR_MASK) {
		SCE_DBG_PRINTF_FUNC(0, 786, "sceKernelInitializeFastMutex", " k: error: sceKernelInitializeFastMutex(%s), attr=0x%08X\n",
			       name, attr);
		ret = SCE_KERNEL_ERROR_ILLEGAL_ATTR;
		goto restore;
	}
	if (option != NULL && option->size > FAST_MUTEX_OPTION_MAX_SIZE) {
		ret = SCE_KERNEL_ERROR_ILLEGAL_SIZE;
		goto restore;
	}
	if (mutex->magic == FAST_MUTEX_MAGIC) {
		SCE_DBG_ASSERT_CHECK_FUNC(0, 798, "sceKernelInitializeFastMutex");
		ret = SCE_KERNEL_ERROR_FAST_MUTEX_ALREADY_INITIALIZED;
		goto restore;
	}
	if (!(attr & FAST_MUTEX_ATTR_CEILING)) {
		intr = SceCpuForDriver_821FC0EE();
		thread = currentThreadContext();
		ceiling = PRIORITY_NONE;
	} else {
		if (option == NULL) {
			ret = SCE_KERNEL_ERROR_INVALID_ARGUMENT;
			goto restore;
		}
		intr = SceCpuForDriver_821FC0EE();
		thread = currentThreadContext();
		ceiling = option->ceilingPriority;
		if (ceiling == 0)
			ceiling = thread->currentPriority;
		if ((uint32_t)ceiling - 1 >= PRIORITY_NONE ||
		    ((attr & FAST_MUTEX_ATTR_LOCKED) && ceiling > thread->basePriority)) {
			ret = SCE_KERNEL_ERROR_ILLEGAL_PRIORITY;
			goto resume;
		}
	}
	ret = sceGUIDKernelCreate(s_threadmgrRuntime.manager->fastMutexClass, name,
				   (SceObjectBase **)&uidMutex);
	if (ret < 0)
		goto resume;
	uidMutex->ownerPid = SCE_KERNEL_PROCESS_ID;
	uidMutex->attr = attr;
	mutex->ceilingPriority = ceiling;
	mutex->uid = ret;
	mutex->next = mutex;
	mutex->prev = mutex;
	mutex->queue = (struct ThreadmgrWaitQueue *)&uidMutex->next;
	mutex->lockCount = 0;
	mutex->owner = NULL;
	mutex->lock = 0;
	mutex->spinWaitCount = 0;
	uidMutex->workArea = mutex;
	if (attr & FAST_MUTEX_ATTR_LOCKED) {
		sceKernelSpinlockLowLock(&mutex->lock);
		sceKernelSpinlockLowLock(THREAD_LOCK(thread));
		acquireFastMutex(mutex, thread);
		sceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
	}
	ret = 0;
	mutex->magic = FAST_MUTEX_MAGIC;
	sceKernelSpinlockLowUnlock(&uidMutex->lock);	/* the class hands it over locked */
resume:
	sceKernelCpuResumeIntr(intr);
restore:
	ThreadmgrSetUserRoWord(saved);
	return ret;
}

int sceKernelFinalizeFastMutex(struct ThreadmgrFastMutex *mutex)
{
	uint32_t saved;
	int intr, ret;

	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	if (mutex->magic != FAST_MUTEX_MAGIC) {
		SCE_DBG_ASSERT_PRINTF_FUNC(0, 904, "sceKernelFinalizeFastMutex", "*** error: not initialized. pFastMutex=%p\n", mutex);
		ret = SCE_KERNEL_ERROR_FAST_MUTEX_NOT_INITIALIZED;
		goto restore;
	}
	if (SceIntrmgrForDriver_182EE3E3()) {
		SCE_DBG_ASSERT_PRINTF_FUNC(0, 910, "sceKernelFinalizeFastMutex", "*** error: illegal context. pFastMutex=%p[%s]\n",
				      mutex, SceSysmemForDriver_E655852F(mutex->uid));
		ret = SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
		goto restore;
	}
	intr = SceCpuForDriver_821FC0EE();
	sceKernelSpinlockLowLock(&mutex->queue->lock);
	if (mutex->owner == NULL) {
		sceKernelSpinlockLowUnlock(&mutex->queue->lock);
		ret = sceGUIDClose(mutex->uid);
		if (ret < 0)
			SCE_DBG_ASSERT_LEVEL_FUNC(0, 0, 936, "sceKernelFinalizeFastMutex");
		else
			memset(mutex, 0, sizeof(*mutex));
	} else {
		SCE_DBG_PRINTF_FUNC(0, 921, "sceKernelFinalizeFastMutex", " k: error: sceKernelFinalizeFastMutex %s (%p) still owned\n",
			       SceSysmemForDriver_E655852F(mutex->uid), mutex);
		SCE_DBG_ASSERT_CHECK_FUNC(0, 923, "sceKernelFinalizeFastMutex");
		ret = SCE_KERNEL_ERROR_FAST_MUTEX_OWNED;
		sceKernelSpinlockLowUnlock(&mutex->queue->lock);
	}
	sceKernelCpuResumeIntr(intr);
restore:
	ThreadmgrSetUserRoWord(saved);
	return ret;
}

/*
 * The lock is free but still owned (the owner is handing it over): sleep on
 * the UID object's queue until it is ours.
 */
#define FAST_MUTEX_SLEEP(panicLine, assertLine) do {					\
	if (sceGUIDReferObjectWithClass(mutex->uid,					\
			s_threadmgrRuntime.manager->fastMutexClass, NULL) < 0)	\
		SCE_PANIC_FUNC(panicLine, "lockFastMutex");				\
	sceKernelSpinlockLowLock(THREAD_LOCK(thread));					\
	thread->waitMode = (int)mutex;							\
	ret = waitWaitableObject(mutex->uid, (uintptr_t)mutex->queue, SCE_KERNEL_WAITTYPE_FAST_MUTEX,	\
				 NULL, NULL, NULL, &locked);				\
	if (ret >= 0)									\
		SCE_DBG_ASSERT_LEVEL_FUNC(0, thread == mutex->owner && mutex->lock != 0,	\
				     assertLine, "lockFastMutex");			\
	SceCpuForDriver_4AE1BCC0((SceInt32 *)&thread->flags, THREAD_FLAG_WAKE_PENDING);	\
	sceKernelSpinlockLowUnlock(THREAD_LOCK(thread));				\
	if (locked) {									\
		sceKernelSpinlockLowUnlock(&mutex->queue->lock);			\
		sceGUIDReleaseObject(mutex->uid);					\
		locked = 0;								\
	}										\
} while (0)

int lockFastMutex(struct ThreadmgrFastMutex *mutex)
{
	struct ThreadCB *thread;
	SceKernelSpinlock *queueLock;
	uint32_t saved;
	int intr, ret, haveLock, locked = 0;

	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	SCE_DBG_ASSERT_LEVEL_FUNC(0, 1, 200, "lockFastMutex");
	if (mutex->magic != FAST_MUTEX_MAGIC) {
		SCE_DBG_ASSERT_PRINTF_FUNC(0, 203, "lockFastMutex", "*** error: not initialized. pFastMutex=%p\n", mutex);
		ret = SCE_KERNEL_ERROR_FAST_MUTEX_NOT_INITIALIZED;
		goto restore;
	}
	if (SceIntrmgrForDriver_182EE3E3()) {
		SCE_DBG_ASSERT_PRINTF_FUNC(0, 210, "lockFastMutex", "*** error: illegal context. pFastMutex=%p[%s]\n",
				      mutex, SceSysmemForDriver_E655852F(mutex->uid));
		ret = SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
		goto restore;
	}
	intr = SceCpuForDriver_821FC0EE();
	if (intr & 0x80) {
		SCE_DBG_ASSERT_PRINTF_FUNC(0, 218, "lockFastMutex", "*** error: irq suspended. pFastMutex=%p[%s]\n",
				      mutex, SceSysmemForDriver_E655852F(mutex->uid));
		ret = SCE_KERNEL_ERROR_CAN_NOT_WAIT;
		goto resume;
	}
	thread = currentThreadContext();
	if (currentCpuState()->disabledDispatch != 0) {
		ret = SCE_KERNEL_ERROR_CAN_NOT_WAIT;
		goto resume;
	}
	if (mutex->owner == thread) {
		if (mutex->queue->attributes & FAST_MUTEX_ATTR_RECURSIVE) {
			if (mutex->lockCount + 1 < 0) {
				ret = SCE_KERNEL_ERROR_FAST_MUTEX_LOCK_OVF;
			} else {
				mutex->lockCount++;
				ret = 0;
			}
		} else {
			ret = SCE_KERNEL_ERROR_FAST_MUTEX_RECURSIVE;
		}
		goto resume;
	}
	queueLock = &mutex->queue->lock;
	sceKernelSpinlockLowLock(queueLock);
	if (SceCpuForDriver_5AC9D394(&mutex->lock) >= 0) {
		if (mutex->owner == NULL)
			goto take;
		sceKernelSpinlockLowUnlock(&mutex->lock);
		FAST_MUTEX_SLEEP(287, 301);
		goto resume;
	}
	/* the owner holds the lock: spin on it */
	thread->waitMode = (int)mutex;
	thread->fastMutexHandoff = 0;
	thread->waitType = SCE_KERNEL_WAITTYPE_FAST_MUTEX_SPIN;
	SCE_DBG_ASSERT_LEVEL_FUNC(0, mutex->spinWaitCount < 0x7fffffff, 330, "lockFastMutex");
	mutex->spinWaitCount++;
	sceKernelSpinlockLowUnlock(queueLock);
	sceKernelCpuResumeIntr(intr);
	for (;;) {
		intr = sceKernelSpinlockLowTrylockCpuSuspendIntr(&mutex->lock);
		if (intr < 0) {
			intr = SceCpuForDriver_821FC0EE();
			haveLock = 0;
			if (thread->fastMutexHandoff == 0)
				goto again;
		} else {
			haveLock = 1;
			thread->waitMode = 0;
			thread->waitType = 0;
		}
		SCE_DBG_ASSERT_CHECK_FUNC(ThreadmgrIntrDisabled(), 381, "lockFastMutex");
		if (mutex->owner == thread) {
			/* the unlocker handed it to us */
			SCE_DBG_ASSERT_LEVEL_FUNC(0, thread->fastMutexHandoff != 0, 390, "lockFastMutex");
			SCE_DBG_ASSERT_LEVEL_FUNC(0, thread->waitType != SCE_KERNEL_WAITTYPE_FAST_MUTEX_SPIN, 391, "lockFastMutex");
			SCE_DBG_ASSERT_CHECK_FUNC(mutex->lock != 0, 393, "lockFastMutex");
			thread->fastMutexHandoff = 0;
			SceCpuForDriver_4AE1BCC0((SceInt32 *)&thread->flags,
						    THREAD_FLAG_WAKE_PENDING);
			sceKernelCpuResumeIntr(intr);
			ret = 0;
			goto restore;
		}
		if (thread->fastMutexHandoff != 0) {
			/* woken out of the spin with an error */
			SCE_DBG_ASSERT_LEVEL_FUNC(0, (thread->flags & THREAD_FLAG_REQUEST_MASK) != 0, 406, "lockFastMutex");
			SCE_DBG_ASSERT_LEVEL_FUNC(0, thread->waitResult != 0, 407, "lockFastMutex");
			SCE_DBG_ASSERT_LEVEL_FUNC(0, thread->waitType != SCE_KERNEL_WAITTYPE_FAST_MUTEX_SPIN, 408, "lockFastMutex");
			thread->fastMutexHandoff = 0;
			SceCpuForDriver_4AE1BCC0((SceInt32 *)&thread->flags,
						    THREAD_FLAG_WAKE_PENDING);
			ret = thread->waitResult;
			if (haveLock)
				sceKernelSpinlockLowUnlock(&mutex->lock);
			goto resume;
		}
		SCE_DBG_ASSERT_LEVEL_FUNC(0, 1, 424, "lockFastMutex");
		if (SceCpuForDriver_5AC9D394(queueLock) >= 0)
			break;
		sceKernelSpinlockLowUnlock(&mutex->lock);
again:
		sceKernelCpuResumeIntr(intr);
	}
	SCE_DBG_ASSERT_LEVEL_FUNC(0, mutex->owner != thread, 443, "lockFastMutex");
	SCE_DBG_ASSERT_CHECK_FUNC(thread->fastMutexHandoff == 0, 444, "lockFastMutex");
	SceCpuForDriver_4AE1BCC0((SceInt32 *)&thread->flags, THREAD_FLAG_WAKE_PENDING);
	SCE_DBG_ASSERT_PRINTF_FUNC(mutex->spinWaitCount != 0, 453, "lockFastMutex", " k(%d): spinWaitCount = 0, "
		"threadId=0x%08X(flags=0x%08X), waitId=0x%08X, pCurrentOwnerId=0x%08X\n",
		sceKernelCpuId(), thread->threadId, thread->flags, mutex->uid,
		mutex->owner != NULL ? mutex->owner->threadId : 0);
	mutex->spinWaitCount--;
	if (mutex->owner != NULL) {
		sceKernelSpinlockLowUnlock(&mutex->lock);
		FAST_MUTEX_SLEEP(477, 490);
		goto resume;
	}
	thread->waitMode = 0;
	thread->waitType = 0;
take:
	sceKernelSpinlockLowLock(THREAD_LOCK(thread));
	acquireFastMutex(mutex, thread);
	sceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
	sceKernelSpinlockLowUnlock(queueLock);
	ret = 0;
resume:
	sceKernelCpuResumeIntr(intr);
restore:
	ThreadmgrSetUserRoWord(saved);
	return ret;
}

int sceKernelTryLockFastMutex(struct ThreadmgrFastMutex *mutex)
{
	struct ThreadCB *thread;
	uint32_t saved;
	int intr, ret, count;

	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	if (mutex->magic != FAST_MUTEX_MAGIC) {
		SCE_DBG_ASSERT_PRINTF_FUNC(0, 999, "sceKernelTryLockFastMutex", "*** error: not initialized. pFastMutex=%p\n", mutex);
		ret = SCE_KERNEL_ERROR_FAST_MUTEX_NOT_INITIALIZED;
		goto restore;
	}
	if (SceIntrmgrForDriver_182EE3E3()) {
		SCE_DBG_ASSERT_PRINTF_FUNC(0, 1006, "sceKernelTryLockFastMutex", "*** error: illegal context. pFastMutex=%p[%s]\n",
				      mutex, SceSysmemForDriver_E655852F(mutex->uid));
		ret = SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
		goto restore;
	}
	intr = SceCpuForDriver_821FC0EE();
	thread = currentThreadContext();
	if (mutex->owner == thread) {
		if (mutex->queue->attributes & FAST_MUTEX_ATTR_RECURSIVE) {
			count = mutex->lockCount + 1;
			if (count < 0) {
				ret = SCE_KERNEL_ERROR_FAST_MUTEX_LOCK_OVF;
			} else {
				mutex->lockCount = count;
				ret = 0;
			}
		} else {
			ret = SCE_KERNEL_ERROR_FAST_MUTEX_RECURSIVE;
		}
	} else {
		sceKernelSpinlockLowLock(&mutex->queue->lock);
		if (SceCpuForDriver_5AC9D394(&mutex->lock) < 0) {
			ret = SCE_KERNEL_ERROR_FAST_MUTEX_FAILED_TO_OWN;
		} else if (mutex->owner == NULL) {
			sceKernelSpinlockLowLock(THREAD_LOCK(thread));
			acquireFastMutex(mutex, thread);
			sceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
			ret = 0;
		} else {
			ret = SCE_KERNEL_ERROR_FAST_MUTEX_FAILED_TO_OWN;
			sceKernelSpinlockLowUnlock(&mutex->lock);
		}
		sceKernelSpinlockLowUnlock(&mutex->queue->lock);
	}
	sceKernelCpuResumeIntr(intr);
restore:
	ThreadmgrSetUserRoWord(saved);
	return ret;
}

/* takes the work area from its owner and drops the owner's boost */
static inline __attribute__((always_inline)) uint32_t releaseFastMutex(
	struct ThreadmgrFastMutex *mutex, struct ThreadCB *owner)
{
	int priority;

	SCE_DBG_ASSERT_CHECK_FUNC(ThreadmgrIntrDisabled(), 145, "releaseFastMutex");
	SCE_DBG_ASSERT_CHECK_FUNC(mutex->queue->lock != 0, 148, "releaseFastMutex");
	SCE_DBG_ASSERT_CHECK_FUNC(mutex->lock != 0, 151, "releaseFastMutex");
	mutex->lockCount = 0;
	mutex->owner = NULL;
	if (owner == NULL)
		return 0;
	sceKernelSpinlockLowLock(THREAD_LOCK(owner));
	mutex->next->prev = mutex->prev;
	mutex->prev->next = mutex->next;
	priority = findNextCeilingPriority(owner);
	if (priority == owner->currentPriority) {
		sceKernelSpinlockLowUnlock(&mutex->lock);
		sceKernelSpinlockLowUnlock(THREAD_LOCK(owner));
		return 0;
	}
	sceKernelSpinlockLowUnlock(&mutex->lock);
	sceKernelSpinlockLowUnlock(THREAD_LOCK(owner));
	return changeThreadPriority(owner, priority);
}

/* SceThreadmgrForDriver_DB395782: when no thread spins, the first sleeper gets the mutex */
int sceKernelUnlockFastMutex(struct ThreadmgrFastMutex *mutex)
{
	struct ThreadCB *owner, *first;
	struct ThreadmgrWaitQueue *queue;
	uint32_t saved, wake;
	int intr, ret;

	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	if (mutex->magic != FAST_MUTEX_MAGIC) {
		SCE_DBG_ASSERT_PRINTF_FUNC(0, 1085, "sceKernelUnlockFastMutex", "*** error: not initialized. pFastMutex=%p\n", mutex);
		ret = SCE_KERNEL_ERROR_FAST_MUTEX_NOT_INITIALIZED;
		goto restore;
	}
	if (SceIntrmgrForDriver_182EE3E3()) {
		SCE_DBG_ASSERT_PRINTF_FUNC(0, 1092, "sceKernelUnlockFastMutex", "*** error: illegal context. pFastMutex=%p[%s]\n",
				      mutex, SceSysmemForDriver_E655852F(mutex->uid));
		ret = SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
		goto restore;
	}
	intr = SceCpuForDriver_821FC0EE();
	sceKernelSpinlockLowLock(&mutex->queue->lock);
	owner = mutex->owner;
	if (owner != currentThreadContext()) {
		SCE_DBG_PRINTF_FUNC(0, 1109, "sceKernelUnlockFastMutex", " k: error: sceKernelUnlockFastMutex %s (%p) not owned\n",
			       SceSysmemForDriver_E655852F(mutex->uid), mutex);
		SCE_DBG_ASSERT_CHECK_FUNC(0, 1111, "sceKernelUnlockFastMutex");
		ret = SCE_KERNEL_ERROR_FAST_MUTEX_NOT_OWNED;
		goto unlock;
	}
	if (--mutex->lockCount > 0) {
		ret = 0;
		goto unlock;
	}
	wake = releaseFastMutex(mutex, owner);
	queue = mutex->queue;
	first = (struct ThreadCB *)queue->next;
	if (mutex->spinWaitCount == 0 && (struct ThreadmgrWaitQueue *)first != queue) {
		SCE_DBG_ASSERT_CHECK_FUNC(first->magic == THREAD_CB_MAGIC, 1139, "sceKernelUnlockFastMutex");
		sceKernelSpinlockLowLock(THREAD_LOCK(first));
		acquireFastMutex(mutex, first);
		wake |= wakeupAndDistributeThread(first, (uintptr_t)queue, 0);
		sceKernelSpinlockLowUnlock(THREAD_LOCK(first));
	}
	sceKernelSpinlockLowUnlock(&mutex->queue->lock);
	ret = 0;
	if (wake & 1)
		switchThread((intr >> 7) & 1);
	goto resume;
unlock:
	sceKernelSpinlockLowUnlock(&mutex->queue->lock);
resume:
	sceKernelCpuResumeIntr(intr);
restore:
	ThreadmgrSetUserRoWord(saved);
	return ret;
}

int sceKernelGetFastMutexInfo(struct ThreadmgrFastMutex *mutex, ThreadmgrFastMutexInfo *info)
{
	ThreadmgrFastMutexInfo local;
	const char *name;
	uint32_t saved;
	SceSize size;
	int intr, ret;

	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	if (mutex->magic != FAST_MUTEX_MAGIC) {
		ret = SCE_KERNEL_ERROR_FAST_MUTEX_NOT_INITIALIZED;
	} else if (info == NULL) {
		ret = SCE_KERNEL_ERROR_ILLEGAL_ADDR;
	} else if (info->size > sizeof(local)) {
		ret = SCE_KERNEL_ERROR_ILLEGAL_SIZE;
	} else {
		local = (ThreadmgrFastMutexInfo){ .size = sizeof(local) };
		name = SceSysmemForDriver_E655852F(mutex->uid);
		if (name != NULL)
			strncpy(local.name, name, sizeof(local.name) - 1);
		intr = SceCpuForDriver_821FC0EE();
		sceKernelSpinlockLowLock(&mutex->queue->lock);
		local.uid = mutex->uid;
		local.lockCount = mutex->lockCount;
		local.attr = mutex->queue->attributes;
		local.workArea = mutex;
		if (mutex->owner != NULL)
			local.currentOwnerId = mutex->owner->threadId;
		local.ceilingPriority = mutex->ceilingPriority;
		local.numWaitThreads = mutex->queue->waiting;
		local.numSpinThreads = mutex->spinWaitCount;
		sceKernelSpinlockLowUnlock(&mutex->queue->lock);
		sceKernelCpuResumeIntr(intr);
		size = info->size;
		if (size > sizeof(local))
			size = sizeof(local);
		memcpy(info, &local, size);
		ret = 0;
	}
	ThreadmgrSetUserRoWord(saved);
	return ret;
}

/* SceThreadmgrForDriver_AF302193: a fast mutex's information by its UID */
int SceThreadmgrForDriver_AF302193(SceUID uid, ThreadmgrFastMutexInfo *info)
{
	struct SceUIDFastMutexObject *uidMutex;
	SceObjectBase *pObject;
	struct ThreadmgrFastMutex *mutex;
	ThreadmgrFastMutexInfo local;
	uint32_t saved;
	SceSize size;
	int intr, ret;

	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	if (info == NULL) {
		ret = SCE_KERNEL_ERROR_ILLEGAL_ADDR;
	} else if (info->size > sizeof(local)) {
		ret = SCE_KERNEL_ERROR_ILLEGAL_SIZE;
	} else if (sceGUIDReferObjectWithClass(uid,
			s_threadmgrRuntime.manager->fastMutexClass,
			&pObject) < 0) {
		ret = SCE_KERNEL_ERROR_UNKNOWN_FAST_MUTEX_ID;
	} else {
		uidMutex = (struct SceUIDFastMutexObject *)pObject;
		mutex = uidMutex->workArea;
		intr = sceKernelSpinlockLowLockCpuSuspendIntr(&uidMutex->lock);
		ret = getFastMutexInfo(mutex, &local);
		sceKernelSpinlockLowUnlockCpuResumeIntr(&uidMutex->lock, intr);
		sceGUIDReleaseObject(uid);
		if (ret == 0) {
			size = info->size;
			if (size > sizeof(local))
				size = sizeof(local);
			memcpy(info, &local, size);
		}
	}
	ThreadmgrSetUserRoWord(saved);
	return ret;
}

/* SceThreadmgrForDriver_6D4C1EB7: the information of every fast mutex visible at level */
int SceThreadmgrForDriver_6D4C1EB7(int level, ThreadmgrFastMutexInfo *infos, SceSize size,
				   SceSize *count)
{
	SceSize got = 0;
	SceKernelSpinlock *lock;
	SceUID id;
	SceObjectBase *pObject;
	ThreadmgrFastMutexInfo info;
	SceUID *vector;
	uint32_t saved;
	int total, i, ret, intr, err;

	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	if (size >= sizeof(info)) {
		if (infos == NULL) {
			ret = SCE_KERNEL_ERROR_INVALID_ARGUMENT;
			goto restore;
		}
	}
	ret = sceGUIDGetUIDVectorByClass(s_threadmgrRuntime.manager->fastMutexClass,
					  level, NULL, 0, NULL);
	if (ret <= 0)
		goto restore;
	while (1) {
		vector = sceKernelAllocHeapMemory(s_threadmgrRuntime.manager->heapId,
						   ret * sizeof(SceUID));
		if (vector == NULL) {
			ret = SCE_KERNEL_ERROR_NO_MEMORY;
			goto restore;
		}
		total = sceGUIDGetUIDVectorByClass(
			s_threadmgrRuntime.manager->fastMutexClass, level, vector, ret, &got);
		if (total > (int)got) {
			ret = total;
			sceKernelFreeHeapMemory(s_threadmgrRuntime.manager->heapId, vector);
			continue;
		}
		break;
	}
	ret = 0;
	got = 0;
	for (i = 0; i < total; i++) {
		id = vector[i];
		if (sceGUIDReferObjectWithClass(id,
				s_threadmgrRuntime.manager->fastMutexClass,
				&pObject) < 0)
			continue;
		lock = &((struct SceUIDFastMutexObject *)pObject)->lock;
		intr = sceKernelSpinlockLowLockCpuSuspendIntr(lock);
		err = getFastMutexInfo(((struct SceUIDFastMutexObject *)pObject)->workArea, &info);
		sceKernelSpinlockLowUnlockCpuResumeIntr(lock, intr);
		sceGUIDReleaseObject(id);
		if (err != 0)
			continue;
		ret++;
		if (size < ret * sizeof(info)) {
			ret += total - i - 1;
			break;
		}
		memcpy(&infos[got++], &info, sizeof(info));
	}
	if (count != NULL)
		*count = got;
	sceKernelFreeHeapMemory(s_threadmgrRuntime.manager->heapId, vector);
restore:
	ThreadmgrSetUserRoWord(saved);
	return ret;
}
