/*
 * FunctorNBQueue.h: the non-blocking queue every libult object is built on (a Michael-Scott
 * queue of 32-byte entries with a dummy at its head).
 *
 * The head and the tail, and each entry's link to the next, are 64-bit IDs changed with
 * LDREXD/STREXD: the high word is the entry's address, the low word a count the queue's
 * functor keeps (a semaphore's resources, the reservations a waiter asks for). Bit 0 of the
 * head's address is the functor's flag: set, the entries behind the head have nothing for
 * the functor to do. An entry whose link is all ones (DESTROYED_ID) ends a destroyed queue.
 */
#ifndef FUNCTOR_NB_QUEUE_H
#define FUNCTOR_NB_QUEUE_H

#include "ult_util.h"

#define FNBQ_ASSERT(line, cond)	ULT_ASSERT_AT("FunctorNBQueue.h", line, #cond, cond)

typedef uint64_t NBQueueID;

const NBQueueID INVALID_ID = 0;
const NBQueueID DESTROYED_ID = ~(NBQueueID)0;

inline uint32_t idAddress(NBQueueID id)
{
	return (uint32_t)(id >> 32);
}

inline int32_t idCount(NBQueueID id)
{
	return (int32_t)id;
}

inline NBQueueID makeID(int32_t count, uint32_t address)
{
	return ((NBQueueID)address << 32) | (uint32_t)count;
}

/* the functor's flag, bit 0 of the address */
inline bool isFlagged(NBQueueID id)
{
	return (idAddress(id) & 1) != 0;
}

inline bool isInvalid(NBQueueID id)
{
	return (idAddress(id) & ~1u) == 0;
}

struct NBQueueEntry2 {
	NBQueueID mNext;
	uint32_t data;			/* what the entry carries (a waiter, a queue's data) */
	uint32_t reserved[5];
};

/* the address word of an entry's link, read alone (the objects' infos) */
inline uint32_t nextAddress(const NBQueueEntry2 *entry)
{
	return ((const uint32_t *)&entry->mNext)[1];
}

inline NBQueueEntry2 *asPtr(NBQueueID id)
{
	return (NBQueueEntry2 *)(idAddress(id) & ~1u);
}

inline NBQueueID setFlag(NBQueueID id)
{
	return makeID(idCount(id), idAddress(id) | 1);
}

inline NBQueueID clearFlag(NBQueueID id)
{
	return makeID(idCount(id), idAddress(id) & ~1u);
}

/* what a pass over the head saw */
struct HeadReservation {
	NBQueueID head;
	NBQueueID next;			/* the head's link */
	NBQueueID tail;
};

/* what a pass over the tail saw */
struct TailReservation {
	NBQueueID currentTail;
	NBQueueID next;			/* the tail's link */
};

template <class Functor>
class FunctorNBQueue {
public:
	volatile NBQueueID mHead;
	volatile NBQueueID mTail;
	uint32_t reserved;
	bool mDestroyed;		/* no more pushes */

	/* the head's words, read alone (the objects' infos) */
	uint32_t headAddress() const
	{
		return ((const volatile uint32_t *)&mHead)[1];
	}

	int32_t headCount() const
	{
		return ((const volatile int32_t *)&mHead)[0];
	}

	/* points the tail at the last entry, which a push may not have done yet */
	ULT_INLINE void fixTail()
	{
		for (;;) {
			NBQueueID tail = __ldrexd((volatile long long *)&mTail);
			__builtin_dmb();
			FNBQ_ASSERT(359, ! isInvalid(tail));
			NBQueueID next = asPtr(tail)->mNext;
			FNBQ_ASSERT(362, tail != next);
			if (isInvalid(next))
				return;
			__strexd(next, (volatile long long *)&mTail);
		}
	}

	/* reads the head and its link, the tail first fixed when it is behind the head */
	ULT_INLINE void reserveHead(HeadReservation &r)
	{
		for (;;) {
			__builtin_dmb();
			NBQueueID head = __ldrexd((volatile long long *)&mHead);
			r.head = head;
			FNBQ_ASSERT(265, ! isInvalid(head));
			__builtin_dmb();
			r.next = asPtr(head)->mNext;
			r.tail = mTail;
			if (r.next == DESTROYED_ID ||
			    ((idAddress(r.head) ^ idAddress(r.tail)) & ~1u) != 0 ||
			    isInvalid(r.next))
				return;
			fixTail();
		}
	}

	/* ends a reservation of the head with the new head */
	ULT_INLINE bool commitHead(HeadReservation &r, NBQueueID newHead)
	{
		FNBQ_ASSERT(285, !isInvalid(newHead));
		return __strexd(newHead, (volatile long long *)&mHead) == 0;
	}

	/* reads the tail and reserves its link */
	ULT_INLINE void reserveTail(TailReservation &r)
	{
		for (;;) {
			r.currentTail = mTail;
			__builtin_dmb();
			r.next = __ldrexd((volatile long long *)&asPtr(r.currentTail)->mNext);
			__builtin_dmb();
			if (r.next == DESTROYED_ID)
				return;
			if (r.currentTail != mTail)
				continue;
			if (isInvalid(r.next))
				return;
			fixTail();
		}
	}

	/* links an entry behind the reserved tail */
	ULT_INLINE bool commitTail(TailReservation &reservation, NBQueueID newTail)
	{
		FNBQ_ASSERT(332, reservation.currentTail != newTail);
		FNBQ_ASSERT(333, !isInvalid(newTail));
		FNBQ_ASSERT(334, isInvalid( asPtr(newTail)->mNext));
		__builtin_dmb();
		return __strexd(newTail,
				(volatile long long *)&asPtr(reservation.currentTail)->mNext) == 0;
	}

	/* links an entry behind the tail; SCE_ULT_ERROR_STATE once the queue is destroyed */
	ULT_INLINE int pushTail(NBQueueID newTail)
	{
		TailReservation reservation;

		do {
			reserveTail(reservation);
			if (mDestroyed || reservation.next == DESTROYED_ID)
				return SCE_ULT_ERROR_STATE;
		} while (!commitTail(reservation, newTail));
		fixTail();
		return SCE_OK;
	}

	/* ends the queue: SCE_ULT_ERROR_STATE while an entry is in it */
	ULT_INLINE int destroy()
	{
		NBQueueID head;
		NBQueueID next;

		do {
			do {
				head = mHead;
				__builtin_dmb();
				next = __ldrexd((volatile long long *)&asPtr(head)->mNext);
				__builtin_dmb();
			} while (head != mHead);
			if (next != INVALID_ID)
				return SCE_ULT_ERROR_STATE;
		} while (__strexd(DESTROYED_ID, (volatile long long *)&asPtr(head)->mNext) != 0);
		mDestroyed = true;
		__builtin_dmb();
		return SCE_OK;
	}

	/* an empty queue: the dummy at its head, with the functor's count */
	ULT_INLINE void initializeHead(NBQueueEntry2 *dummy, int32_t count)
	{
		mHead = makeID(count, (uint32_t)dummy | 1);
		mTail = makeID(count, (uint32_t)dummy);
		asPtr(mHead)->mNext = INVALID_ID;
		__builtin_dmb();
	}
};

#endif
