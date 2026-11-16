/*
 * Mutex.h: the mutex (SceUltMutex) and the condition variable (SceUltConditionVariable).
 *
 * The mutex is a CondWaiterQ whose head's count is 1 while it is locked. The owner keeps a
 * waiter entry of the pool while it holds the lock (ownerWaiter): the entry it queued with, or
 * the one the queue's head left when the lock was handed to it. A condition variable queues its
 * waiters on that entry, and hands each the entry its own queue's head leaves.
 */
#ifndef MUTEX_H
#define MUTEX_H

#include "ult_internal.h"
#include "NBQueue.h"

#define MUTEX_ASSERT(line, text, cond) \
	ULT_ASSERT_AT("Mutex.h", line, text, cond)
#define MUTEX_ASSERT_EQUAL(line, text, actual, expected) \
	ULT_ASSERT_EQUAL_AT("Mutex.h", line, text, actual, expected)

/* the head's count: 1 while locked */
struct MutexCallback {
	ULT_INLINE static int check(int32_t state, int32_t request)
	{
		if (state <= 0)
			return SCE_OK;
		return SCE_ULT_ERROR_AGAIN;
	}

	ULT_INLINE static int32_t update(int32_t state, int32_t request)
	{
		return 1;
	}

	ULT_INLINE static int release(int32_t state, int32_t request, int32_t &newState)
	{
		if (state == 0)
			return SCE_ULT_ERROR_STATE;
		newState = 0;
		return SCE_OK;
	}

	/* the waiter is the owner now: it takes the entry */
	template <class Queue>
	ULT_INLINE static void wakeup(Queue &q, NBQueueEntry2 *entry, Waiter *waiter, void *cause)
	{
		int ret = waiter->signal(entry, cause);
		MUTEX_ASSERT_EQUAL(154, "ret==0", ret, 0);
	}
};

/* a mutex's lock */
struct InternalMutex {
	volatile uint32_t ownerId;		/* the owner's thread ID (AbstractThread::getID2) */
	int32_t recursiveLockCount;
	CondWaiterQ<MutexCallback> waitQ;
	NBQueueEntry2 *ownerWaiter;		/* the owner's entry */
	uint32_t recursive;			/* SCE_ULT_MUTEX_ATTRIBUTE_RECURSIVE */
	BoundedCounter conditionVariables;	/* the condition variables on it */

	ULT_INLINE int initialize(WaitingQueueResourcePoolInternal *pool, uint32_t attribute)
	{
		int ret = waitQ.initialize(0, pool, NULL);
		if (ret != SCE_OK)
			return ret;
		ownerId = 0;
		recursiveLockCount = 0;
		ownerWaiter = NULL;
		recursive = attribute & SCE_ULT_MUTEX_ATTRIBUTE_RECURSIVE;
		/* a recursive mutex takes no condition variable */
		conditionVariables.initialize(recursive != 0 ? 0 : 0x7fffffff);
		return SCE_OK;
	}
};

/* an option parameter given: aligned, initialized, a known attribute, nothing reserved */
static inline int checkMutexOptParam(const SceUltMutexOptParam *optParam)
{
	if (!isAligned8(optParam))
		return SCE_ULT_ERROR_ALIGNMENT;
	int ret = checkOptParamHeader((const OptParamHeader *)optParam);
	if (ret != SCE_OK)
		return ret;
	if ((optParam->attribute & ~SCE_ULT_MUTEX_ATTRIBUTE_RECURSIVE) != 0)
		return SCE_ULT_ERROR_INVALID;
	if (optParam->__reserved__0 != 0)
		return SCE_ULT_ERROR_INVALID;
	for (uint32_t i = 0; i < sizeof(optParam->__reserved__) / sizeof(uint64_t); i++) {
		if (optParam->__reserved__[i] != 0)
			return SCE_ULT_ERROR_INVALID;
	}
	return SCE_OK;
}

/* SceUltMutex */
class Mutex {
public:
	UltObjectHeader header;
	uint32_t attribute;
	InternalMutex internalMutex;
	SceUltWaitingQueueResourcePool *waitingQueueResourcePool;
	uint32_t ulobjId;

	ULT_INLINE int create(const char *name, SceUltWaitingQueueResourcePool *pool,
			      const SceUltMutexOptParam *optParam, uint32_t buildVersion)
	{
		SceUltMutexOptParam defaultOptParam;

		if (this == NULL || name == NULL || pool == NULL)
			return SCE_ULT_ERROR_NULL;
		if (!isAligned8(this) || !isAligned8(optParam) || !isAligned8(pool))
			return SCE_ULT_ERROR_ALIGNMENT;
		MUTEX_ASSERT(216, "buildVersion >= 0x00910000", buildVersion >= ULT_BUILD_VERSION_MIN);
		if (optParam != NULL) {
			int ret = checkMutexOptParam(optParam);
			if (ret != SCE_OK)
				return ret;
		} else {
			_sceUltMutexOptParamInitialize(&defaultOptParam, ULT_VERSION);
			optParam = &defaultOptParam;
		}
		int ret = setName(header.name, name);
		if (ret != SCE_OK)
			return ret;
		header.version = ULT_OBJECT_VERSION;
		header.type = ULT_OBJECT_TYPE_MUTEX;
		attribute = optParam->attribute;
		waitingQueueResourcePool = pool;
		ret = internalMutex.initialize(internalOf(pool), optParam->attribute);
		if (ret != SCE_OK)
			return ret;
		SceUlobjDbg_D7F0F610(this, ULOBJ_DBG_KIND_SYNC_OBJECT, &ulobjId);
		__builtin_dmb();
		return SCE_OK;
	}

	/*
	 * Takes the lock with entry, the caller's: queued behind the owner when wait (entry's data
	 * is the caller's Waiter), else SCE_ULT_ERROR_AGAIN while it is held.
	 */
	__attribute__((noinline)) int lockInternal(NBQueueEntry2 *entry, bool wait)
	{
		if (wait) {
			Waiter *waiter = (Waiter *)entry->data;
			int ret = internalMutex.waitQ.acquire(0, makeID(0, (uint32_t)entry), this);
			if (ret == SCE_OK) {
				internalMutex.ownerWaiter = entry;
			} else {
				MUTEX_ASSERT_EQUAL(276, "ret==CondWaiterQ<MutexCallback>::WAIT", ret,
						   CondWaiterQ<MutexCallback>::WAIT);
				/* the entry the head left */
				NBQueueEntry2 *ownerWaiter = (NBQueueEntry2 *)waiter->wait(this);
				ownerWaiter->data = 0;
				internalMutex.ownerWaiter = ownerWaiter;
			}
		} else {
			int ret = internalMutex.waitQ.acquire(0, INVALID_ID, this);
			if (ret != SCE_OK)
				return ret;
			internalMutex.ownerWaiter = entry;
		}
		internalMutex.ownerId = AbstractThread::getID2();
		MUTEX_ASSERT_EQUAL(291, "internalMutex.recursiveLockCount==0",
				   internalMutex.recursiveLockCount, 0);
		internalMutex.recursiveLockCount = 1;
		return SCE_OK;
	}

	__attribute__((noinline)) int lock(bool wait)
	{
		if (wait && !AbstractThread::canWait())
			return SCE_ULT_ERROR_PERMISSION;
		if (this == NULL)
			return SCE_ULT_ERROR_NULL;
		if (!isAligned8(this))
			return SCE_ULT_ERROR_ALIGNMENT;
		if (internalMutex.ownerId == AbstractThread::getID2() && internalMutex.recursive == 0)
			return SCE_ULT_ERROR_STATE;
		if (internalMutex.recursive != 0 && internalMutex.ownerId == AbstractThread::getID2()) {
			internalMutex.recursiveLockCount++;
			return SCE_OK;
		}
		WaitingQueueResourcePoolInternal *pool = internalMutex.waitQ.m_pool;
		if (pool == NULL)
			return SCE_ULT_ERROR_BUSY;
		NBQueueEntry2 *entry = pool->allocateWaiterEntry();
		if (entry == NULL)
			return SCE_ULT_ERROR_BUSY;

		if (wait) {
			Waiter waiter;
			int ret = waiter.initialize(this);
			MUTEX_ASSERT_EQUAL(330, "ret==0", ret, 0);
			entry->data = (uint32_t)&waiter;
			__builtin_dmb();
			ret = lockInternal(entry, true);
			MUTEX_ASSERT_EQUAL(334, "ret==0", ret, 0);
			waiter.finalize();
			MUTEX_ASSERT(337, "ownerWaiter != NULL", internalMutex.ownerWaiter != NULL);
			return SCE_OK;
		}
		int ret = lockInternal(entry, false);
		if (ret != SCE_OK) {
			internalMutex.waitQ.freeEntry(entry);
			return ret;
		}
		MUTEX_ASSERT(342, "ownerWaiter != NULL", internalMutex.ownerWaiter != NULL);
		return SCE_OK;
	}

	/* 1 while a recursive lock is still held, else the lock goes to the next waiter */
	__attribute__((noinline)) int unlockInternal()
	{
		if (internalMutex.ownerId != AbstractThread::getID2())
			return SCE_ULT_ERROR_PERMISSION;
		MUTEX_ASSERT(357, "ownerWaiter != NULL", internalMutex.ownerWaiter != NULL);
		if (internalMutex.recursive != 0 && internalMutex.recursiveLockCount > 1) {
			internalMutex.recursiveLockCount--;
			return 1;
		}
		MUTEX_ASSERT_EQUAL(365, "internalMutex.recursiveLockCount==1",
				   internalMutex.recursiveLockCount, 1);
		__builtin_dmb();
		internalMutex.ownerId = 0;
		internalMutex.recursiveLockCount = 0;
		internalMutex.ownerWaiter = NULL;
		__builtin_dmb();
		return internalMutex.waitQ.release(0, this);
	}

	ULT_INLINE int unlock()
	{
		if (this == NULL)
			return SCE_ULT_ERROR_NULL;
		if (!isAligned8(this))
			return SCE_ULT_ERROR_ALIGNMENT;
		__builtin_dmb();
		NBQueueEntry2 *waiter = internalMutex.ownerWaiter;
		int ret = unlockInternal();
		if (ret < 0)
			return ret;
		if (ret != 1) {
			MUTEX_ASSERT(396, "waiter != NULL", waiter != NULL);
			internalMutex.waitQ.freeEntry(waiter);
		}
		return SCE_OK;
	}

	ULT_INLINE int destroy()
	{
		if (this == NULL)
			return SCE_ULT_ERROR_NULL;
		if (!isAligned8(this))
			return SCE_ULT_ERROR_ALIGNMENT;
		if (internalMutex.ownerId != 0)
			return SCE_ULT_ERROR_STATE;
		int ret = internalMutex.conditionVariables.close();
		if (ret != SCE_OK)
			return ret;
		ret = internalMutex.waitQ.destroy();
		if (ret != SCE_OK)
			return ret;
		SceUlobjDbg_F9C0F5DA(ulobjId);
		return SCE_OK;
	}
};

/* SceUltConditionVariable: its waiters queue on their mutex's entries */
class ConditionVariable {
public:
	UltObjectHeader header;
	uint32_t reserved;
	NBQueue3 waiters;
	Mutex *mutex;
	uint32_t ulobjId;

	ULT_INLINE int create(const char *name, Mutex *mutex,
			      const SceUltConditionVariableOptParam *optParam, uint32_t buildVersion)
	{
		SceUltConditionVariableOptParam defaultOptParam;

		if (this == NULL || name == NULL || mutex == NULL)
			return SCE_ULT_ERROR_NULL;
		if (!isAligned8(this) || !isAligned8(mutex))
			return SCE_ULT_ERROR_ALIGNMENT;
		if (mutex->internalMutex.recursive != 0)
			return SCE_ULT_ERROR_PERMISSION;
		MUTEX_ASSERT(518, "buildVersion >= 0x00910000", buildVersion >= ULT_BUILD_VERSION_MIN);
		if (optParam != NULL) {
			int ret = checkOptParam(optParam);
			if (ret != SCE_OK)
				return ret;
		} else {
			_sceUltConditionVariableOptParamInitialize(&defaultOptParam, ULT_VERSION);
			optParam = &defaultOptParam;
		}
		int ret = setName(header.name, name);
		if (ret != SCE_OK)
			return ret;
		header.version = ULT_OBJECT_VERSION;
		header.type = ULT_OBJECT_TYPE_CONDITION_VARIABLE;
		WaitingQueueResourcePoolInternal *pool = mutex->internalMutex.waitQ.m_pool;
		if (pool == NULL)
			return SCE_ULT_ERROR_INVALID;
		NBQueueEntry2 *dummy = pool->m_syncObjectQueue.pop();
		if (dummy == NULL)
			return SCE_ULT_ERROR_BUSY;
		waiters.initialize(dummy);
		ret = mutex->internalMutex.conditionVariables.increment();
		MUTEX_ASSERT(557, "ret == SCE_OK", ret == SCE_OK);
		this->mutex = mutex;
		SceUlobjDbg_D7F0F610(this, ULOBJ_DBG_KIND_SYNC_OBJECT, &ulobjId);
		__builtin_dmb();
		return SCE_OK;
	}

	ULT_INLINE int signal()
	{
		if (this == NULL)
			return SCE_ULT_ERROR_NULL;
		if (!isAligned8(this))
			return SCE_ULT_ERROR_ALIGNMENT;
		NBQueueEntry2 *entry = waiters.pop();
		if (entry != NULL) {
			Waiter *wi = (Waiter *)entry->data;
			MUTEX_ASSERT(578, "wi != NULL", wi != NULL);
			wi->signal(entry, this);
		}
		return SCE_OK;
	}

	ULT_INLINE int signalAll()
	{
		if (this == NULL)
			return SCE_ULT_ERROR_NULL;
		if (!isAligned8(this))
			return SCE_ULT_ERROR_ALIGNMENT;
		for (;;) {
			NBQueueEntry2 *entry = waiters.pop();
			if (entry == NULL)
				break;
			Waiter *wi = (Waiter *)entry->data;
			MUTEX_ASSERT(601, "wi != NULL", wi != NULL);
			wi->signal(entry, this);
		}
		return SCE_OK;
	}

	/* unlocks the mutex, waits for a signal and locks it again */
	ULT_INLINE int wait()
	{
		if (this == NULL)
			return SCE_ULT_ERROR_NULL;
		if (!isAligned8(this))
			return SCE_ULT_ERROR_ALIGNMENT;
		if (mutex->internalMutex.ownerId != AbstractThread::getID2())
			return SCE_ULT_ERROR_PERMISSION;
		NBQueueEntry2 *waiterForMutex = mutex->internalMutex.ownerWaiter;
		MUTEX_ASSERT(624, "waiterForMutex !=NULL", waiterForMutex != NULL);

		Waiter waiter;
		int ret = waiter.initialize(this);
		MUTEX_ASSERT_EQUAL(628, "ret==0", ret, 0);
		waiterForMutex->data = (uint32_t)&waiter;
		__builtin_dmb();
		ret = waiters.push(waiterForMutex);
		if (ret == SCE_ULT_ERROR_STATE) {
			waiterForMutex->data = 0;
			waiter.finalize();
			return SCE_ULT_ERROR_STATE;
		}
		MUTEX_ASSERT_EQUAL(638, "ret==0", ret, 0);
		ret = mutex->unlockInternal();
		MUTEX_ASSERT(640, "ret >= 0", ret >= 0);
		/* the entry the signal gave */
		NBQueueEntry2 *waiterNext = (NBQueueEntry2 *)waiter.wait(this);
		MUTEX_ASSERT(643, "waiterNext != NULL", waiterNext != NULL);
		waiter.finalize();

		ret = waiter.initialize(mutex);
		MUTEX_ASSERT_EQUAL(648, "ret==0", ret, 0);
		waiterNext->data = (uint32_t)&waiter;
		__builtin_dmb();
		ret = mutex->lockInternal(waiterNext, true);
		MUTEX_ASSERT_EQUAL(652, "ret==0", ret, 0);
		waiter.finalize();
		return SCE_OK;
	}

	ULT_INLINE int destroy()
	{
		if (this == NULL)
			return SCE_ULT_ERROR_NULL;
		if (!isAligned8(this))
			return SCE_ULT_ERROR_ALIGNMENT;
		int ret = waiters.destroy();
		if (ret != SCE_OK)
			return ret;
		ret = mutex->internalMutex.waitQ.m_pool->m_syncObjectQueue.push(asPtr(waiters.mHead));
		MUTEX_ASSERT_EQUAL(675, "ret==0", ret, 0);
		mutex->internalMutex.conditionVariables.decrement();
		SceUlobjDbg_F9C0F5DA(ulobjId);
		return SCE_OK;
	}
};

#endif
