/*
 * NBQueue.h: the plain non-blocking queue (NBQueue3), the free lists of the resource pools
 * and of a runtime's threads. Its functor's flag says the queue is empty.
 */
#ifndef NB_QUEUE_H
#define NB_QUEUE_H

#include "FunctorNBQueue.h"

#define NBQ_ASSERT(line, cond)	ULT_ASSERT_AT("NBQueue.h", line, #cond, cond)

struct NBQueueFunctor {
};

class NBQueue3 : public FunctorNBQueue<NBQueueFunctor> {
public:
	ULT_INLINE void initialize(NBQueueEntry2 *dummy)
	{
		initializeHead(dummy, 0);
		mDestroyed = false;
	}

	int push(NBQueueEntry2 *entry);
	NBQueueEntry2 *pop();
};

inline __attribute__((noinline)) int NBQueue3::push(NBQueueEntry2 *entry)
{
	NBQueueID pushEntry = makeID(0, (uint32_t)entry);
	HeadReservation r;
	int ret;

	NBQ_ASSERT(99, ! isInvalid(pushEntry));
	asPtr(pushEntry)->mNext = INVALID_ID;
	__builtin_dmb();
	ret = pushTail(pushEntry);
	if (ret != SCE_OK)
		return ret;
	/* the queue is not empty any more */
	do {
		reserveHead(r);
		if (!isFlagged(r.head))
			break;
		NBQueueID newHead = r.head;
		if (!isInvalid(r.next))
			newHead = clearFlag(newHead);
		if (commitHead(r, newHead))
			break;
	} while (1);
	return SCE_OK;
}

inline __attribute__((noinline)) NBQueueEntry2 *NBQueue3::pop()
{
	HeadReservation r;
	uint32_t data;

	for (;;) {
		reserveHead(r);
		if (isInvalid(r.next)) {
			if (isFlagged(r.head))
				return NULL;
			continue;
		}
		NBQ_ASSERT(75, r.head != r.tail);
		NBQueueEntry2 *n = asPtr(r.next);
		NBQueueID newHead = r.next;
		data = n->data;
		if (isInvalid(n->mNext))
			newHead = setFlag(r.next);
		if (commitHead(r, newHead))
			break;
	}
	__builtin_dmb();
	NBQueueEntry2 *p = asPtr(r.head);
	NBQ_ASSERT(87, p->mNext != INVALID_ID);
	p->mNext = INVALID_ID;
	p->data = data;
	__builtin_dmb();
	return p;
}

#endif
