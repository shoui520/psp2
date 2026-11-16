/*
 * Waiter.h: a thread waiting on a synchronization object. It lives on the waiting thread's
 * stack; the queue entry the object's waiting queue holds points at it. The thread that
 * wakes it leaves a value for it and the object it was woken by (its cause).
 */
#ifndef WAITER_H
#define WAITER_H

#include "AbstractThread.h"

#define WAITER_ASSERT_EQUAL(line, text, actual, expected) \
	ULT_ASSERT_EQUAL_AT("Waiter.h", line, text, actual, expected)

class Waiter {
public:
	uint32_t m_id;			/* the waiting thread (AbstractThread::getID) */
	void *m_value;			/* what the waking thread left */
	void *volatile m_cause;		/* the object it waits on */

	ULT_INLINE int initialize(void *cause)
	{
		m_cause = cause;
		m_id = AbstractThread::getID();
		if (m_id == 0)
			return -1;
		__builtin_dmb();
		return 0;
	}

	ULT_INLINE void finalize()
	{
		AbstractThread::releaseID(m_id);
		m_id = AbstractThread::INVALID_ID;
		__builtin_dmb();
	}

	/* sleeps until signal; what the waking thread left */
	ULT_INLINE void *wait(void *cause)
	{
		int iret = AbstractThread::waitSignal(m_id, cause);
		WAITER_ASSERT_EQUAL(61, "iret==0", iret, 0);
		__builtin_dmb();
		WAITER_ASSERT_EQUAL(63, "m_cause==cause", (int)m_cause, (int)cause);
		m_cause = NULL;
		__builtin_dmb();
		return m_value;
	}

	ULT_INLINE int signal(void *value, void *cause)
	{
		WAITER_ASSERT_EQUAL(51, "m_cause==cause", (int)m_cause, (int)cause);
		m_value = value;
		__builtin_dmb();
		return AbstractThread::sendSignal(m_id);
	}
};

#endif
