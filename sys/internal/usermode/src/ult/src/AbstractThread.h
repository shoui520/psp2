/*
 * AbstractThread.h: what libult's synchronization objects know of a thread. A thread is a
 * kernel thread or an AbstractThread (an ulthread's, for one); the TLS slot ULT_TLS_KEY holds
 * the current thread's AbstractThread, or nothing on a kernel thread of its own.
 *
 * A thread is named by an ID: an AbstractThread's address (4-byte aligned), or a kernel
 * thread's ID shifted left with bit 0 set.
 */
#ifndef ABSTRACT_THREAD_H
#define ABSTRACT_THREAD_H

#include "ult_util.h"

#define ABSTRACT_THREAD_ASSERT(line, cond) \
	ULT_ASSERT_AT("AbstractThread.h", line, #cond, cond)

/* the TLS slot of the current AbstractThread */
#define ULT_TLS_KEY	67

class AbstractThread;

/* a thread that cannot wait itself waits on its parent */
#define ABSTRACT_THREAD_FLAG_WAIT_ON_PARENT	0x1

/* how a thread waits and is woken: its owner's callbacks */
struct AbstractThreadCallbacks {
	void *reserved[2];
	int (*waitSignal)(uint32_t id, void *cause);
	int (*sendSignal)(uint32_t id);
};

/* the thread is a kernel thread: its ID is the kernel's shifted left, with bit 0 set */
inline bool isKernelThread(uint32_t id)
{
	return id == 0 || (id & 1) != 0;
}

/* KernelThread.cpp: a kernel thread's side of the callbacks */
namespace KernelThread {
uint32_t getID();
void release(uint32_t tid);
int waitSignal(uint32_t tid);
int sendSignal(uint32_t tid);
}

class AbstractThread {
public:
	static const uint32_t INVALID_ID = 0xffffffff;

	uint32_t id;				/* what the callbacks are given */
	const AbstractThreadCallbacks *callbacks;
	AbstractThread *parent;			/* the thread it runs on, INVALID_ID if none */
	AbstractThread *previous;		/* the current thread it was switched from */
	uint32_t flags;				/* ABSTRACT_THREAD_FLAG_* */
	uint32_t reserved[3];

	/* the TLS slot of the current thread */
	static AbstractThread **getTLS();
	/* the current thread's ID */
	static uint32_t getID2();

	/* the ID of the current thread a waiter waits on: INVALID_ID if it cannot wait */
	ULT_INLINE static uint32_t getID()
	{
		AbstractThread *t = *getTLS();

		while (!isKernelThread((uint32_t)t)) {
			if (t->callbacks->waitSignal != NULL)
				return (uint32_t)t;
			if ((t->flags & ABSTRACT_THREAD_FLAG_WAIT_ON_PARENT) == 0)
				return INVALID_ID;
			t = t->parent;
			if (t == NULL)
				break;
		}
		return (KernelThread::getID() << 1) | 1;
	}

	/* the current thread can wait: getID would not give INVALID_ID */
	ULT_INLINE static bool canWait()
	{
		AbstractThread *t = *getTLS();

		while (!isKernelThread((uint32_t)t)) {
			if (t->callbacks->waitSignal != NULL)
				return true;
			if ((t->flags & ABSTRACT_THREAD_FLAG_WAIT_ON_PARENT) == 0)
				return false;
			t = t->parent;
			if (t == NULL)
				return true;
		}
		return true;
	}

	ULT_INLINE static int waitSignal(uint32_t id, void *cause)
	{
		if (isKernelThread(id))
			return KernelThread::waitSignal(id >> 1);
		AbstractThread *t = (AbstractThread *)id;
		ABSTRACT_THREAD_ASSERT(213, t->callbacks->waitSignal != NULL);
		return t->callbacks->waitSignal(t->id, cause);
	}

	ULT_INLINE static int sendSignal(uint32_t id)
	{
		if (isKernelThread(id))
			return KernelThread::sendSignal(id >> 1);
		AbstractThread *t = (AbstractThread *)id;
		/* Sony's check is of waitSignal, before it calls sendSignal */
		ABSTRACT_THREAD_ASSERT(200, t->callbacks->waitSignal != NULL);
		return t->callbacks->sendSignal(t->id);
	}

	ULT_INLINE static void releaseID(uint32_t id)
	{
		ABSTRACT_THREAD_ASSERT(187, id != 0);
		if (isKernelThread(id))
			KernelThread::release(id >> 1);
	}
};

#endif
