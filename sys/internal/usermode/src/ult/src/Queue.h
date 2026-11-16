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
	__attribute__((noinline)) int initialize(const char *name, uint32_t numData, uint32_t dataSize,
		       uint32_t numQueueObject, SceUltWaitingQueueResourcePool *waitingQueueResourcePool,
		       void *workArea, const SceUltQueueDataResourcePoolOptParam *optParam,
		       uint32_t buildVersion)
	{
		WaitingQueueResourcePoolInternal *pool = internalOf(waitingQueueResourcePool);

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
};

#endif
