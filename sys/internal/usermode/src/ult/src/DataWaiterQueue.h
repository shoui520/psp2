/*
 * DataWaiterQueue.h: a non-blocking queue that holds either data or waiters, never both (an
 * ImplQueue's free buffers and its queued data). An entry's link carries the entry's type in
 * its count word; the head's flag says no datum is behind it. Who takes a datum when there is
 * none queues itself as a waiter, and who gives a datum to a queue of waiters hands it to the
 * first of them.
 */
#ifndef DATA_WAITER_QUEUE_H
#define DATA_WAITER_QUEUE_H

#include "FunctorNBQueue.h"

#define DWQ_ASSERT(line, text, cond) \
	ULT_ASSERT_AT("DataWaiterQueue.h", line, text, cond)
#define DWQ_ASSERT_EQUAL(line, text, actual, expected) \
	ULT_ASSERT_EQUAL_AT("DataWaiterQueue.h", line, text, actual, expected)

/* an entry's type, in the count word of the link to it */
enum {
	TYPE_DATA = 1,
	TYPE_WAITER = 3
};

/* why a pop found nothing */
enum {
	ERROR_EMPTY = -1,		/* nothing behind the head */
	ERROR_TYPE_UNMATCHED = -2,	/* the entries are of the other type */
	ERROR_NO_DATA = -3		/* the head's flag: no datum */
};

inline uint32_t getType(NBQueueID id)
{
	return (uint32_t)idCount(id) & 3;
}

struct DataWaiterFunctor {
};

class DataWaiterQueue : public FunctorNBQueue<DataWaiterFunctor> {
public:
	ULT_INLINE void initialize(NBQueueEntry2 *dummy)
	{
		initializeHead(dummy, 0);
		__builtin_dmb();
	}

	/* a datum is behind the head */
	ULT_INLINE bool hasData()
	{
		NBQueueID head = mHead;
		NBQueueID next = asPtr(mHead)->mNext;

		return !isFlagged(head) && !isInvalid(next) && getType(next) == TYPE_DATA;
	}

	/*
	 * Takes the first entry, of the given type: the old dummy carries it out (data), the
	 * entry is the new dummy.
	 */
	ULT_INLINE int pop(uint32_t type, NBQueueID &data)
	{
		HeadReservation r;
		NBQueueID newHead;
		uint32_t value;

		do {
			reserveHead(r);
			if (type == TYPE_DATA && isFlagged(r.head))
				return ERROR_NO_DATA;
			if (isInvalid(r.next))
				return ERROR_EMPTY;
			if (getType(r.next) != type)
				return ERROR_TYPE_UNMATCHED;
			DWQ_ASSERT(99, "!isInvalid(r.next)", !isInvalid(r.next));
			NBQueueEntry2 *n = asPtr(r.next);
			value = n->data;
			/* the flag: no datum behind the new head */
			if (getType(n->mNext) == TYPE_DATA && !isInvalid(n->mNext))
				newHead = clearFlag(r.next);
			else
				newHead = setFlag(r.next);
		} while (!commitHead(r, newHead));
		__builtin_dmb();
		DWQ_ASSERT(110, "getType(r.next) == type", getType(r.next) == type);
		NBQueueEntry2 *p = asPtr(r.head);
		p->data = value;
		p->mNext = INVALID_ID;
		__builtin_dmb();
		data = r.head;
		return SCE_OK;
	}

	/*
	 * Links an entry (of the type given: its link's) behind the tail, unless the queue holds
	 * entries of the other type.
	 */
	ULT_INLINE int push(NBQueueID pushEntry, uint32_t type)
	{
		TailReservation r;

		asPtr(pushEntry)->mNext = INVALID_ID;
		__builtin_dmb();
		do {
			reserveTail(r);
			DWQ_ASSERT(141, "r.currentTail != pushEntry", r.currentTail != pushEntry);
			if (((idAddress(r.currentTail) ^ idAddress(mHead)) & ~1u) != 0 &&
			    getType(r.currentTail) != type)
				return ERROR_TYPE_UNMATCHED;
		} while (!commitTail(r, pushEntry));
		fixTail();
		__builtin_dmb();
		if (type == TYPE_DATA) {
			/* a datum is behind the head now */
			HeadReservation h;
			NBQueueID newHead;

			do {
				reserveHead(h);
				if (!isFlagged(h.head))
					break;
				newHead = h.head;
				if (getType(h.next) == TYPE_DATA && !isInvalid(h.next))
					newHead = clearFlag(h.head);
			} while (!commitHead(h, newHead));
		}
		return SCE_OK;
	}

	/* the first datum's entry, or ERROR_NO_DATA */
	ULT_INLINE int tryPop(NBQueueEntry2 *&entry)
	{
		for (;;) {
			NBQueueID data = INVALID_ID;
			int ret = pop(TYPE_DATA, data);

			if (ret == SCE_OK) {
				DWQ_ASSERT(213, "!isInvalid(data)", !isInvalid(data));
				entry = asPtr(data);
				return SCE_OK;
			}
			if (ret == ERROR_NO_DATA)
				return ret;
			DWQ_ASSERT(220, "(ret == ERROR_TYPE_UNMATCHED) || (ret == ERROR_EMPTY)",
				   (ret == ERROR_TYPE_UNMATCHED) || (ret == ERROR_EMPTY));
		}
	}

	/*
	 * The first datum's entry; when there is none, the waiter's entry is queued instead
	 * (NULL: the waiter is to wait for a datum to be handed to it).
	 */
	ULT_INLINE NBQueueEntry2 *popOrPushWaiter(NBQueueEntry2 *waiterEntry)
	{
		NBQueueID pushEntry = makeID(TYPE_WAITER, (uint32_t)waiterEntry);
		int ret;

		for (;;) {
			while (hasData()) {
				NBQueueID data = INVALID_ID;

				ret = pop(TYPE_DATA, data);
				if (ret == SCE_OK)
					return asPtr(data);
				DWQ_ASSERT(187, "(ret == ERROR_TYPE_UNMATCHED) || (ret == ERROR_EMPTY)|| (ret == ERROR_NO_DATA)",
					   (ret == ERROR_TYPE_UNMATCHED) || (ret == ERROR_EMPTY) ||
					   (ret == ERROR_NO_DATA));
			}
			ret = push(pushEntry, TYPE_WAITER);
			if (ret != ERROR_TYPE_UNMATCHED)
				break;
		}
		DWQ_ASSERT_EQUAL(197, "ret==0", ret, 0);
		return NULL;
	}

	/*
	 * Queues a datum's entry; when waiters are queued, the first of them is taken instead
	 * (its entry, for the caller to hand the datum to: NULL when the datum was queued).
	 */
	ULT_INLINE NBQueueEntry2 *pushOrPopWaiter(NBQueueEntry2 *entry)
	{
		NBQueueID pushEntry = makeID(TYPE_DATA, (uint32_t)entry);

		for (;;) {
			NBQueueID tail = mTail;
			NBQueueID head = mHead;

			/* empty, or data queued: the datum is queued too */
			if (((idAddress(head) ^ idAddress(tail)) & ~1u) == 0 ||
			    getType(tail) == TYPE_DATA) {
				if (push(pushEntry, TYPE_DATA) != ERROR_TYPE_UNMATCHED)
					return NULL;
				continue;
			}
			NBQueueID waiter = INVALID_ID;
			int ret = pop(TYPE_WAITER, waiter);
			if (ret == SCE_OK) {
				DWQ_ASSERT(245, "waiter != 0", waiter != 0);
				return asPtr(waiter);
			}
			DWQ_ASSERT(248, "(ret == ERROR_TYPE_UNMATCHED) || (ret == ERROR_EMPTY)",
				   (ret == ERROR_TYPE_UNMATCHED) || (ret == ERROR_EMPTY));
		}
	}
};

#endif
