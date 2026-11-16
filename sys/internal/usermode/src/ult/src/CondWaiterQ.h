/*
 * CondWaiterQ.h: the waiting queue every synchronization object is built on, and the pool its
 * entries come from.
 *
 * A CondWaiterQ is a FunctorNBQueue whose head carries the object's state in its count (a
 * semaphore's resources, a lock's owners) and whose other entries are waiting threads, each
 * with its request in its count. The Callback decides whether a request can be granted on a
 * state (check: SCE_OK or SCE_ULT_ERROR_AGAIN), the state it leaves (update), the state a
 * release leaves (release), and wakes a waiter whose request was granted, given the entry the
 * queue's head leaves (wakeup). The head's flag set says no waiter can be granted: a thread that
 * clears it owes the queue a wakeup.
 */
#ifndef COND_WAITER_Q_H
#define COND_WAITER_Q_H

#include "NBQueue.h"
#include "Waiter.h"

#define CWQ_ASSERT(line, text, cond) \
	ULT_ASSERT_AT("CondWaiterQ.h", line, text, cond)
#define CWQ_ASSERT_EQUAL(line, text, actual, expected) \
	ULT_ASSERT_EQUAL_AT("CondWaiterQ.h", line, text, actual, expected)

/*
 * A waiting queue resource pool (SceUltWaitingQueueResourcePool from offset 48): the entries
 * waiting threads queue on, counted, and the dummies the objects' queues start with.
 */
class WaitingQueueResourcePoolInternal {
public:
	NBQueue3 m_waiterQueue;		/* free entries for waiting threads */
	NBQueue3 m_syncObjectQueue;	/* free dummies for the objects' queues */
	void *m_workArea;
	uint32_t reserved;
	int32_t m_numFreeWaiters;	/* -1 once destroyed */
	int32_t m_maxNumWaiters;

	/* the work area: a dummy and numThreads entries, then a dummy and numSyncObjects ones */
	ULT_INLINE int initialize(uint32_t numThreads, uint32_t numSyncObjects, void *workArea)
	{
		if (numThreads == 0)
			return SCE_ULT_ERROR_INVALID;
		if (numSyncObjects == 0)
			return SCE_ULT_ERROR_INVALID;
		if (!isAligned8(workArea))
			return SCE_ULT_ERROR_ALIGNMENT;
		if (workArea == NULL)
			return SCE_ULT_ERROR_NULL;

		NBQueueEntry2 *entry = (NBQueueEntry2 *)workArea;
		m_waiterQueue.initialize(entry++);
		for (uint32_t i = numThreads; i != 0; i--)
			m_waiterQueue.push(entry++);
		m_syncObjectQueue.initialize(entry++);
		for (uint32_t i = numSyncObjects; i != 0; i--)
			m_syncObjectQueue.push(entry++);
		m_workArea = workArea;
		m_numFreeWaiters = numThreads;
		m_maxNumWaiters = numThreads;
		return SCE_OK;
	}

	/* SCE_ULT_ERROR_STATE while a waiter's entry is out */
	ULT_INLINE int destroy()
	{
		volatile int32_t *count = &m_numFreeWaiters;

		do {
			if (__ldrex(count) != m_maxNumWaiters)
				return SCE_ULT_ERROR_STATE;
		} while (__strex(-1, count) != 0);
		m_waiterQueue.mDestroyed = true;
		__builtin_dmb();
		m_syncObjectQueue.mDestroyed = true;
		__builtin_dmb();
		return SCE_OK;
	}

	/* a waiter's entry back in the count; SCE_ULT_ERROR_AGAIN when the count is full */
	ULT_INLINE int incrementFreeWaiters()
	{
		volatile int32_t *count = &m_numFreeWaiters;
		int32_t n;

		do {
			n = __ldrex(count);
			if (n < 0)
				return SCE_ULT_ERROR_STATE;
			if ((uint32_t)n >= (uint32_t)m_maxNumWaiters)
				return SCE_ULT_ERROR_AGAIN;
		} while (__strex(n + 1, count) != 0);
		return SCE_OK;
	}

	/* an entry for a waiting thread, NULL when none is left */
	ULT_INLINE NBQueueEntry2 *allocateWaiterEntry()
	{
		volatile int32_t *count = &m_numFreeWaiters;
		int32_t n;

		do {
			n = __ldrex(count);
			if (n <= 0)
				return NULL;
		} while (__strex(n - 1, count) != 0);
		NBQueueEntry2 *entry = m_waiterQueue.pop();
		if (entry == NULL) {
			int ret = incrementFreeWaiters();
			CWQ_ASSERT_EQUAL(144, "ret==SCE_OK", ret, SCE_OK);
		}
		return entry;
	}

	ULT_INLINE int freeWaiterEntry(NBQueueEntry2 *entry)
	{
		int ret = m_waiterQueue.push(entry);
		if (ret == SCE_OK) {
			int ret2 = incrementFreeWaiters();
			CWQ_ASSERT_EQUAL(155, "ret2==SCE_OK", ret2, SCE_OK);
		}
		return ret;
	}
};

template <class Callback>
class CondWaiterQ : public FunctorNBQueue<Callback> {
public:
	enum {
		WAIT = 1		/* the request is queued: the thread waits */
	};

	WaitingQueueResourcePoolInternal *m_pool;

	/* an empty queue with the state count: its dummy from the pool, or the object's own */
	ULT_INLINE int initialize(int32_t count, WaitingQueueResourcePoolInternal *pool,
		       NBQueueEntry2 *dummyEntry_for_freeQ)
	{
		m_pool = pool;
		if (pool != NULL) {
			NBQueueEntry2 *dummy = pool->m_syncObjectQueue.pop();
			if (dummy == NULL)
				return SCE_ULT_ERROR_BUSY;
			this->initializeHead(dummy, count);
		} else {
			CWQ_ASSERT(300, "dummyEntry_for_freeQ != NULL", dummyEntry_for_freeQ != NULL);
			this->initializeHead(dummyEntry_for_freeQ, count);
		}
		this->mDestroyed = false;
		return SCE_OK;
	}

	/* ends the queue: SCE_ULT_ERROR_STATE while a thread waits on it */
	ULT_INLINE int destroy()
	{
		int ret = FunctorNBQueue<Callback>::destroy();
		if (ret != SCE_OK)
			return ret;
		if (m_pool != NULL)
			m_pool->m_syncObjectQueue.push(asPtr(this->mHead));
		return SCE_OK;
	}

	/* an entry a waiter has left goes back to the pool */
	ULT_INLINE void freeEntry(NBQueueEntry2 *entry)
	{
		if (m_pool != NULL) {
			int ret = m_pool->freeWaiterEntry(entry);
			CWQ_ASSERT_EQUAL(339, "ret==0", ret, 0);
		} else {
			CWQ_ASSERT(341, "0", 0);
		}
	}

	/* the head after a state change: flagged unless the first waiter can be granted */
	ULT_INLINE static NBQueueID nextHead(int32_t state, uint32_t head, NBQueueID next)
	{
		if (!isInvalid(next) && Callback::check(state, idCount(next)) == SCE_OK)
			return makeID(state, head & ~1u);
		return makeID(state, head | 1);
	}

	/*
	 * Grants request on the state, or queues newEnt (when valid) and returns WAIT. Without an
	 * entry, SCE_ULT_ERROR_AGAIN when the request cannot be granted now.
	 */
	ULT_INLINE int acquire(int32_t request, NBQueueID newEnt, void *cause)
	{
		NBQueueID head;
		HeadReservation r;
		bool flagged;

		for (;;) {
			for (;;) {
				if (this->mDestroyed)
					return SCE_ULT_ERROR_STATE;
				head = this->mHead;
				flagged = isFlagged(head);
				if (!flagged || Callback::check(idCount(head), request) != SCE_OK)
					break;
				this->reserveHead(r);
				if (isFlagged(r.head) &&
				    Callback::check(idCount(r.head), request) == SCE_OK) {
					NBQueueID newHead = nextHead(Callback::update(idCount(r.head), request),
								     idAddress(r.head), r.next);
					if (this->commitHead(r, newHead))
						return SCE_OK;
				}
			}
			if (!isInvalid(newEnt))
				break;
			if (flagged)
				return SCE_ULT_ERROR_AGAIN;
			wakeup(cause);
		}

		TailReservation t;
		do {
			this->reserveTail(t);
			if (this->mDestroyed || t.next == DESTROYED_ID)
				return SCE_ULT_ERROR_STATE;
			CWQ_ASSERT(419, "r.currentTail != newEnt", t.currentTail != newEnt);
		} while (!this->commitTail(t, newEnt));
		this->fixTail();

		NBQueueID newHead;
		do {
			this->reserveHead(r);
			newHead = nextHead(idCount(r.head), idAddress(r.head), r.next);
		} while (!this->commitHead(r, newHead));
		wakeup(cause);
		return WAIT;
	}

	/* gives back what request had: the state changes as Callback::release says */
	ULT_INLINE int release(int32_t request, void *cause)
	{
		HeadReservation r;
		NBQueueID newHead;

		do {
			if (this->mDestroyed)
				return SCE_ULT_ERROR_STATE;
			this->reserveHead(r);
			int32_t state;
			int ret = Callback::release(idCount(r.head), request, state);
			if (ret != SCE_OK)
				return ret;
			newHead = nextHead(state, idAddress(r.head), r.next);
		} while (!this->commitHead(r, newHead));
		wakeup(cause);
		return SCE_OK;
	}

	/* grants every waiter the state allows, first to last */
	void wakeup(void *cause);
};

template <class Callback>
void CondWaiterQ<Callback>::wakeup(void *cause)
{
	HeadReservation r;

	for (;;) {
		this->reserveHead(r);
		if (isFlagged(r.head) || r.next == DESTROYED_ID)
			return;
		if (isInvalid(r.next) || Callback::check(idCount(r.head), idCount(r.next)) != SCE_OK)
			continue;
		NBQueueEntry2 *n = asPtr(r.next);
		int32_t state = Callback::update(idCount(r.head), idCount(r.next));
		uint32_t data = n->data;
		/* the waiter's link is read even when the state grants nobody behind it */
		NBQueueID newHead = nextHead(state, idAddress(r.next),
					     *(volatile NBQueueID *)&n->mNext);
		if (!this->commitHead(r, newHead))
			continue;
		NBQueueEntry2 *old = asPtr(r.head);
		old->data = data;
		Waiter *waiter = (Waiter *)*(volatile uint32_t *)&old->data;
		Callback::wakeup(*this, old, waiter, cause);
	}
}

#endif
