/*
 * Semaphore.h: a counting semaphore over a CondWaiterQ: the head's count is the number of
 * resources, a waiter's count the resources it asks for, negated.
 */
#ifndef SEMAPHORE_H
#define SEMAPHORE_H

#include "ult_internal.h"

#define SEMAPHORE_ASSERT(line, text, cond) \
	ULT_ASSERT_AT("Semaphore.h", line, text, cond)
#define SEMAPHORE_ASSERT_EQUAL(line, text, actual, expected) \
	ULT_ASSERT_EQUAL_AT("Semaphore.h", line, text, actual, expected)

struct SemaphoreCallback {
	ULT_INLINE static int check(int32_t state, int32_t request)
	{
		if (state + request < 0)
			return SCE_ULT_ERROR_AGAIN;
		return SCE_OK;
	}

	ULT_INLINE static int32_t update(int32_t state, int32_t request)
	{
		return state + request;
	}

	/* the count stays positive */
	ULT_INLINE static int release(int32_t state, int32_t request, int32_t &newState)
	{
		if (state > 0 && (int32_t)((uint32_t)state + request) < 0)
			return SCE_ULT_ERROR_STATE;
		newState = state + request;
		return SCE_OK;
	}

	/* the entry goes back to the pool */
	template <class Queue>
	ULT_INLINE static void wakeup(Queue &q, NBQueueEntry2 *entry, Waiter *waiter, void *cause)
	{
		q.freeEntry(entry);
		SEMAPHORE_ASSERT(94, "waiter != NULL", waiter != NULL);
		int ret = waiter->signal(NULL, cause);
		SEMAPHORE_ASSERT_EQUAL(96, "ret==0", ret, 0);
	}
};

class Semaphore : public CondWaiterQ<SemaphoreCallback> {
public:
	ULT_INLINE int initialize(int32_t count, WaitingQueueResourcePoolInternal *pool,
		       const SceUltSemaphoreOptParam *optParam, uint32_t buildVersion,
		       NBQueueEntry2 *dummyEntry_for_freeQ)
	{
		SceUltSemaphoreOptParam defaultOptParam;

		if (this == NULL)
			return SCE_ULT_ERROR_NULL;
		if (!isAligned8(this) || !isAligned8(pool))
			return SCE_ULT_ERROR_ALIGNMENT;
		SEMAPHORE_ASSERT(141, "buildVersion >= 0x00910000", buildVersion >= ULT_BUILD_VERSION_MIN);
		if (optParam == NULL) {
			_sceUltSemaphoreOptParamInitialize(&defaultOptParam, ULT_VERSION);
			optParam = &defaultOptParam;
		} else {
			int ret = checkOptParam(optParam);
			if (ret != SCE_OK)
				return ret;
		}
		int ret = CondWaiterQ<SemaphoreCallback>::initialize(count, pool, dummyEntry_for_freeQ);
		if (ret != SCE_OK)
			return ret;
		__builtin_dmb();
		return SCE_OK;
	}

	ULT_INLINE int tryAcquire(int32_t numResource, void *cause)
	{
		if (this == NULL)
			return SCE_ULT_ERROR_NULL;
		if (!isAligned8(this))
			return SCE_ULT_ERROR_ALIGNMENT;
		if (numResource <= 0)
			return SCE_ULT_ERROR_RANGE;
		return CondWaiterQ<SemaphoreCallback>::acquire(-numResource, INVALID_ID, cause);
	}

	ULT_INLINE int acquire(int32_t numResource, void *cause)
	{
		if (m_pool != NULL && !AbstractThread::canWait())
			return SCE_ULT_ERROR_PERMISSION;
		int ret = tryAcquire(numResource, cause);
		if (ret != SCE_ULT_ERROR_AGAIN)
			return ret;
		if (m_pool == NULL)
			return SCE_ULT_ERROR_BUSY;
		NBQueueEntry2 *entry = m_pool->allocateWaiterEntry();
		if (entry == NULL)
			return SCE_ULT_ERROR_BUSY;

		Waiter waiter;
		ret = waiter.initialize(cause);
		SEMAPHORE_ASSERT_EQUAL(216, "ret==0", ret, 0);
		entry->data = (uint32_t)&waiter;
		__builtin_dmb();
		ret = CondWaiterQ<SemaphoreCallback>::acquire(-numResource,
							      makeID(-numResource, (uint32_t)entry), cause);
		if (ret != SCE_OK && ret != SCE_ULT_ERROR_STATE) {
			SEMAPHORE_ASSERT_EQUAL(228, "ret==CondWaiterQ<SemaphoreCallback>::WAIT", ret, WAIT);
			void *dret = waiter.wait(cause);
			SEMAPHORE_ASSERT_EQUAL(230, "dret==NULL", (int)dret, 0);
			waiter.finalize();
			return SCE_OK;
		}
		entry->data = 0;
		waiter.finalize();
		freeEntry(entry);
		return ret;
	}

	ULT_INLINE int release(int32_t numResource, void *cause)
	{
		if (this == NULL)
			return SCE_ULT_ERROR_NULL;
		if (!isAligned8(this))
			return SCE_ULT_ERROR_ALIGNMENT;
		if (numResource <= 0)
			return SCE_ULT_ERROR_RANGE;
		return CondWaiterQ<SemaphoreCallback>::release(numResource, cause);
	}

	ULT_INLINE int destroy()
	{
		if (this == NULL)
			return SCE_ULT_ERROR_NULL;
		if (!isAligned8(this))
			return SCE_ULT_ERROR_ALIGNMENT;
		return CondWaiterQ<SemaphoreCallback>::destroy();
	}
};

#endif
