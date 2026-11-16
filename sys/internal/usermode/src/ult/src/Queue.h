/*
 * Queue.h: the queue (SceUltQueue) and the pool of its data (SceUltQueueDataResourcePool).
 */
#ifndef QUEUE_H
#define QUEUE_H

#include "Semaphore.h"

#define QUEUE_ASSERT(line, text, cond) \
	ULT_ASSERT_AT("Queue.h", line, text, cond)
#define QUEUE_ASSERT_EQUAL(line, text, actual, expected) \
	ULT_ASSERT_EQUAL_AT("Queue.h", line, text, actual, expected)

/*
 * A queue data resource pool (SceUltQueueDataResourcePool from offset 48): the data's entries,
 * the queues' dummies, and a semaphore counting the free data.
 */
class QueueDataResourcePoolInternal {
public:
	NBQueue3 m_dataQueue;		/* free data, each entry pointing at its buffer */
	NBQueue3 m_queueQueue;		/* free dummies for the queues */
	NBQueueEntry2 m_dummy;		/* the semaphore's */
	Semaphore m_semaphore;		/* the free data */
	void *m_workArea;
	uint32_t m_dataSize;

	/*
	 * The work area: a dummy and numData entries, a dummy and numQueueObject entries, then
	 * the data.
	 */
	ULT_INLINE int initializeInline(const char *name, uint32_t numData, uint32_t dataSize,
		       uint32_t numQueueObject, WaitingQueueResourcePoolInternal *pool,
		       void *workArea, const SceUltQueueDataResourcePoolOptParam *optParam,
		       uint32_t buildVersion)
	{
		if (optParam != NULL) {
			int ret = checkOptParam(optParam);
			if (ret != SCE_OK)
				return ret;
		}
		if (numData == 0)
			return SCE_ULT_ERROR_INVALID;
		if (dataSize == 0)
			return SCE_ULT_ERROR_INVALID;
		if (numQueueObject == 0)
			return SCE_ULT_ERROR_INVALID;
		if (!isAligned8(workArea))
			return SCE_ULT_ERROR_ALIGNMENT;
		if (workArea == NULL)
			return SCE_ULT_ERROR_NULL;
		if (name == NULL)
			return SCE_ULT_ERROR_NULL;

		m_dataSize = dataSize;
		NBQueueEntry2 *entry = (NBQueueEntry2 *)workArea;
		m_dataQueue.initialize(entry++);
		char *data = (char *)workArea + sizeof(NBQueueEntry2) * (numData + numQueueObject) +
			     2 * sizeof(NBQueueEntry2);
		for (uint32_t i = numData; i != 0; i--) {
			entry->data = (uint32_t)data;
			__builtin_dmb();
			m_dataQueue.push(entry++);
			data += dataSize;
		}
		m_queueQueue.initialize(entry++);
		for (uint32_t i = numQueueObject; i != 0; i--)
			m_queueQueue.push(entry++);
		m_workArea = workArea;
		return m_semaphore.initialize(numData, pool, NULL, buildVersion, &m_dummy);
	}

	/* a user's pool, waiting in a user's waiting queue resource pool */
	__attribute__((noinline)) int initialize(const char *name, uint32_t numData, uint32_t dataSize,
		       uint32_t numQueueObject, SceUltWaitingQueueResourcePool *waitingQueueResourcePool,
		       void *workArea, const SceUltQueueDataResourcePoolOptParam *optParam,
		       uint32_t buildVersion)
	{
		return initializeInline(name, numData, dataSize, numQueueObject,
					internalOf(waitingQueueResourcePool), workArea, optParam,
					buildVersion);
	}

	/* an ulthread runtime's own pool */
	__attribute__((noinline)) int initialize(const char *name, uint32_t numData, uint32_t dataSize,
		       uint32_t numQueueObject, WaitingQueueResourcePoolInternal *waitingQueueResourcePool,
		       void *workArea, const SceUltQueueDataResourcePoolOptParam *optParam,
		       uint32_t buildVersion)
	{
		return initializeInline(name, numData, dataSize, numQueueObject,
					waitingQueueResourcePool, workArea, optParam, buildVersion);
	}

	__attribute__((noinline)) int destroy()
	{
		int ret = m_semaphore.destroy();
		if (ret != SCE_OK)
			return ret;
		m_dataQueue.mDestroyed = true;
		__builtin_dmb();
		m_queueQueue.mDestroyed = true;
		__builtin_dmb();
		return SCE_OK;
	}

	/* a datum's entry back in the free list */
	inline __attribute__((noinline)) int freeData(NBQueueEntry2 *packet)
	{
		return m_dataQueue.push(packet);
	}
};

/* SceUltQueueDataResourcePool */
struct QueueDataResourcePoolObject {
	UltObjectHeader header;
	uint32_t numData;
	uint32_t numQueueObject;
	QueueDataResourcePoolInternal internal;
	SceUltWaitingQueueResourcePool *waitingQueueResourcePool;
	uint32_t ulobjId;
};

static inline QueueDataResourcePoolInternal *internalOf(SceUltQueueDataResourcePool *pool)
{
	if (pool == NULL)
		return NULL;
	return &((QueueDataResourcePoolObject *)pool)->internal;
}

/*
 * SceUltQueue: a semaphore counts the data queued, the pool's semaphore the free data; a push
 * takes a free datum, copies into it and queues it, a pop takes the first and frees it.
 */
class Queue {
public:
	UltObjectHeader header;
	uint32_t reserved;
	Semaphore m_semaphore;			/* the data queued */
	NBQueue3 m_queue;			/* the data, first to last */
	NBQueueEntry2 m_dummy;			/* the semaphore's */
	QueueDataResourcePoolInternal *m_queueDataResourcePool;
	WaitingQueueResourcePoolInternal *m_waitingQueueResourcePool;
	SceUltQueueDataResourcePool *queueDataResourcePool;	/* the user's, for the info */
	SceUltWaitingQueueResourcePool *waitingQueueResourcePool;
	uint32_t m_dataSize;
	uint32_t ulobjId;
	AtomicCounter m_numPushWaiters;		/* threads waiting in a push */

	/* an ulthread runtime's own queue (internal) is not registered with the debugger */
	__attribute__((noinline)) int initialize(const char *name, uint32_t dataSize,
						 WaitingQueueResourcePoolInternal *waitingQueueResourcePool,
						 QueueDataResourcePoolInternal *queueDataResourcePool,
						 const SceUltQueueOptParam *optParam, bool internal,
						 uint32_t buildVersion)
	{
		SceUltQueueOptParam defaultOptParam;

		if (this == NULL || name == NULL || queueDataResourcePool == NULL)
			return SCE_ULT_ERROR_NULL;
		if (!isAligned8(this) || !isAligned8(waitingQueueResourcePool) ||
		    !isAligned8(queueDataResourcePool))
			return SCE_ULT_ERROR_ALIGNMENT;
		if (dataSize == 0)
			return SCE_ULT_ERROR_INVALID;
		if (dataSize > queueDataResourcePool->m_dataSize)
			return SCE_ULT_ERROR_INVALID;
		QUEUE_ASSERT(583, "buildVersion >= 0x00910000", buildVersion >= ULT_BUILD_VERSION_MIN);
		if (optParam == NULL) {
			initializeOptParam(&defaultOptParam, ULT_VERSION);
			optParam = &defaultOptParam;
		} else {
			int ret = checkOptParam(optParam);
			if (ret != SCE_OK)
				return ret;
		}
		int ret = setName(header.name, name);
		if (ret != SCE_OK)
			return ret;
		header.version = ULT_OBJECT_VERSION;
		header.type = ULT_OBJECT_TYPE_QUEUE;
		m_dataSize = dataSize;
		m_queueDataResourcePool = queueDataResourcePool;
		this->queueDataResourcePool = NULL;
		NBQueueEntry2 *dummy = queueDataResourcePool->m_queueQueue.pop();
		if (dummy == NULL)
			return SCE_ULT_ERROR_BUSY;
		m_queue.initialize(dummy);
		m_waitingQueueResourcePool = waitingQueueResourcePool;
		this->waitingQueueResourcePool = NULL;
		ret = m_semaphore.initialize(0, waitingQueueResourcePool, NULL, buildVersion, &m_dummy);
		if (ret != SCE_OK) {
			int ret2 = m_queueDataResourcePool->m_queueQueue.push(dummy);
			QUEUE_ASSERT(629, "ret2 == SCE_OK", ret2 == SCE_OK);
			m_queue.mDestroyed = true;
			__builtin_dmb();
		}
		if (!internal && ret == SCE_OK)
			SceUlobjDbg_D7F0F610(this, ULOBJ_DBG_KIND_SYNC_OBJECT, &ulobjId);
		m_numPushWaiters.initialize();
		return ret;
	}

	ULT_INLINE int destroy()
	{
		if (this == NULL)
			return SCE_ULT_ERROR_NULL;
		if (!isAligned8(this))
			return SCE_ULT_ERROR_ALIGNMENT;
		if (m_numPushWaiters.m_count > 0)
			return SCE_ULT_ERROR_STATE;
		int ret = m_semaphore.destroy();
		if (ret != SCE_OK)
			return ret;
		/* the data still queued go back to the pool */
		for (;;) {
			NBQueueEntry2 *packet = m_queue.pop();
			if (packet == NULL)
				break;
			ret = m_queueDataResourcePool->m_dataQueue.push(packet);
			if (ret == SCE_ULT_ERROR_STATE)
				break;
			QUEUE_ASSERT_EQUAL(698, "ret==SCE_OK", ret, SCE_OK);
			__builtin_dmb();
			ret = m_queueDataResourcePool->m_semaphore.release(1, this);
			QUEUE_ASSERT_EQUAL(703, "ret==SCE_OK", ret, SCE_OK);
		}
		__builtin_dmb();
		ret = m_queueDataResourcePool->m_queueQueue.push(asPtr(m_queue.mHead));
		if (ret != SCE_OK)
			QUEUE_ASSERT(711, "ret == SCE_ULT_ERROR_STATE", ret == SCE_ULT_ERROR_STATE);
		m_queue.mDestroyed = true;
		__builtin_dmb();
		return SCE_OK;
	}

	/* waits for a free datum in the pool */
	ULT_INLINE int acquireFreeData()
	{
		if (m_queueDataResourcePool->m_semaphore.m_pool == NULL)
			return SCE_ULT_ERROR_STATE;
		return m_queueDataResourcePool->m_semaphore.acquire(1, this);
	}

	/* waits for a datum in the queue */
	ULT_INLINE int acquireData()
	{
		if (m_semaphore.m_pool == NULL)
			return SCE_ULT_ERROR_STATE;
		return m_semaphore.acquire(1, this);
	}

	__attribute__((noinline)) int push(const void *data)
	{
		if (m_queueDataResourcePool->m_semaphore.m_pool == NULL)
			return SCE_ULT_ERROR_STATE;
		if (!AbstractThread::canWait())
			return SCE_ULT_ERROR_PERMISSION;
		int ret = tryPush(data);
		if (ret != SCE_ULT_ERROR_AGAIN)
			return ret;
		m_numPushWaiters.increment();
		ret = acquireFreeData();
		if (ret != SCE_OK) {
			m_numPushWaiters.decrement();
			return ret;
		}
		NBQueueEntry2 *packet = m_queueDataResourcePool->m_dataQueue.pop();
		QUEUE_ASSERT(751, "packet != NULL", packet != NULL);
		sceClibMemcpy_safe((void *)packet->data, data, m_dataSize);
		ret = m_queue.push(packet);
		if (ret != SCE_OK) {
			QUEUE_ASSERT_EQUAL(756, "ret==SCE_ULT_ERROR_STATE", ret, SCE_ULT_ERROR_STATE);
			int ret2 = m_queueDataResourcePool->freeData(packet);
			QUEUE_ASSERT_EQUAL(759, "ret2==SCE_OK", ret2, SCE_OK);
			ret2 = m_queueDataResourcePool->m_semaphore.release(1, this);
			QUEUE_ASSERT_EQUAL(762, "ret2==SCE_OK", ret2, SCE_OK);
			m_numPushWaiters.decrement();
			return ret;
		}
		__builtin_dmb();
		ret = m_semaphore.release(1, this);
		QUEUE_ASSERT_EQUAL(771, "ret==SCE_OK", ret, SCE_OK);
		m_numPushWaiters.decrement();
		return SCE_OK;
	}

	__attribute__((noinline)) int tryPush(const void *data)
	{
		if (data == NULL)
			return SCE_ULT_ERROR_NULL;
		int ret = m_queueDataResourcePool->m_semaphore.tryAcquire(1, this);
		if (ret != SCE_OK)
			return ret;
		NBQueueEntry2 *packet = m_queueDataResourcePool->m_dataQueue.pop();
		QUEUE_ASSERT(789, "packet != NULL", packet != NULL);
		sceClibMemcpy_safe((void *)packet->data, data, m_dataSize);
		ret = m_queue.push(packet);
		if (ret != SCE_OK) {
			QUEUE_ASSERT_EQUAL(794, "ret==SCE_ULT_ERROR_STATE", ret, SCE_ULT_ERROR_STATE);
			int ret2 = m_queueDataResourcePool->m_dataQueue.push(packet);
			QUEUE_ASSERT_EQUAL(797, "ret2==SCE_OK", ret2, SCE_OK);
			ret2 = m_queueDataResourcePool->m_semaphore.release(1, this);
			QUEUE_ASSERT_EQUAL(800, "ret2==SCE_OK", ret2, SCE_OK);
			return ret;
		}
		__builtin_dmb();
		ret = m_semaphore.release(1, this);
		QUEUE_ASSERT_EQUAL(808, "ret==SCE_OK", ret, SCE_OK);
		return SCE_OK;
	}

	__attribute__((noinline)) int pop(void *data)
	{
		if (data == NULL)
			return SCE_ULT_ERROR_NULL;
		int ret = acquireData();
		if (ret != SCE_OK)
			return ret;
		NBQueueEntry2 *packet = m_queue.pop();
		QUEUE_ASSERT(825, "packet != NULL", packet != NULL);
		sceClibMemcpy_safe(data, (const void *)packet->data, m_dataSize);
		ret = m_queueDataResourcePool->m_dataQueue.push(packet);
		QUEUE_ASSERT_EQUAL(830, "ret==SCE_OK", ret, SCE_OK);
		__builtin_dmb();
		ret = m_queueDataResourcePool->m_semaphore.release(1, this);
		QUEUE_ASSERT_EQUAL(835, "ret==SCE_OK", ret, SCE_OK);
		return SCE_OK;
	}

	/* tryPop, for the one caller Sony's build calls it out of line from */
	__attribute__((noinline)) int tryPopOutOfLine(void *data);

	ULT_INLINE int tryPop(void *data)
	{
		if (data == NULL)
			return SCE_ULT_ERROR_NULL;
		int ret = m_semaphore.tryAcquire(1, this);
		if (ret != SCE_OK)
			return ret;
		NBQueueEntry2 *packet = m_queue.pop();
		QUEUE_ASSERT(853, "packet != NULL", packet != NULL);
		sceClibMemcpy_safe(data, (const void *)packet->data, m_dataSize);
		ret = m_queueDataResourcePool->m_dataQueue.push(packet);
		QUEUE_ASSERT_EQUAL(858, "ret==SCE_OK", ret, SCE_OK);
		__builtin_dmb();
		ret = m_queueDataResourcePool->m_semaphore.release(1, this);
		QUEUE_ASSERT_EQUAL(863, "ret==SCE_OK", ret, SCE_OK);
		return SCE_OK;
	}
};

inline int Queue::tryPopOutOfLine(void *data)
{
	return tryPop(data);
}

#endif
