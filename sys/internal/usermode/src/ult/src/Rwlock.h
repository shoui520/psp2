/*
 * Rwlock.h: the reader-writer lock (SceUltReaderWriterLock) over a CondWaiterQ. The head's
 * count holds the lock: its low byte is set while a writer has it, its high half counts the
 * readers. A waiter's count is its operation.
 */
#ifndef RWLOCK_H
#define RWLOCK_H

#include "ult_internal.h"

#define RWLOCK_ASSERT(line, text, cond) \
	ULT_ASSERT_AT("Rwlock.h", line, text, cond)
#define RWLOCK_ASSERT_EQUAL(line, text, actual, expected) \
	ULT_ASSERT_EQUAL_AT("Rwlock.h", line, text, actual, expected)

/* a waiter's operation */
#define RWLOCK_OPERATION_READ		SCE_ULT_THREAD_OPERATION_READER_LOCK
#define RWLOCK_OPERATION_WRITE		SCE_ULT_THREAD_OPERATION_WRITER_LOCK

/* the lock's state */
#define RWLOCK_STATE_WRITER		0x000000ff
#define RWLOCK_STATE_READERS_SHIFT	16
#define RWLOCK_STATE_MAX_READERS	0xffff

struct RwlockCallback {
	ULT_INLINE static uint32_t writer(int32_t state)
	{
		return (uint8_t)state;
	}

	ULT_INLINE static int32_t readers(int32_t state)
	{
		return (uint32_t)state >> RWLOCK_STATE_READERS_SHIFT;
	}

	ULT_INLINE static int check(int32_t state, int32_t operation)
	{
		if (operation == RWLOCK_OPERATION_READ) {
			int ret = SCE_OK;
			if (writer(state) != 0 || readers(state) >= RWLOCK_STATE_MAX_READERS)
				ret = SCE_ULT_ERROR_AGAIN;
			return ret;
		}
		if (operation == RWLOCK_OPERATION_WRITE) {
			int ret = SCE_OK;
			if (readers(state) > 0 || writer(state) != 0)
				ret = SCE_ULT_ERROR_AGAIN;
			return ret;
		}
		return SCE_ULT_ERROR_AGAIN;
	}

	ULT_INLINE static int32_t update(int32_t state, int32_t operation)
	{
		if (operation == RWLOCK_OPERATION_READ)
			return (uint16_t)state |
			       ((uint16_t)(readers(state) + 1) << RWLOCK_STATE_READERS_SHIFT);
		if (operation == RWLOCK_OPERATION_WRITE)
			return (state & ~RWLOCK_STATE_WRITER) | 1;
		return -1;
	}

	ULT_INLINE static int release(int32_t state, int32_t operation, int32_t &newState)
	{
		if (operation == RWLOCK_OPERATION_READ) {
			if (writer(state) != 0 || readers(state) == 0)
				return SCE_ULT_ERROR_STATE;
		} else if (operation == RWLOCK_OPERATION_WRITE) {
			if (writer(state) == 0 || readers(state) != 0)
				return SCE_ULT_ERROR_STATE;
		} else {
			return -1;
		}
		if (operation == RWLOCK_OPERATION_READ)
			newState = (uint16_t)state |
				   ((uint16_t)(readers(state) - 1) << RWLOCK_STATE_READERS_SHIFT);
		else
			newState = state & ~RWLOCK_STATE_WRITER;
		return SCE_OK;
	}

	/* the entry goes back to the pool */
	template <class Queue>
	ULT_INLINE static void wakeup(Queue &q, NBQueueEntry2 *entry, Waiter *waiter, void *cause)
	{
		q.freeEntry(entry);
		RWLOCK_ASSERT(138, "waiter != NULL", waiter != NULL);
		int ret = waiter->signal(NULL, cause);
		RWLOCK_ASSERT_EQUAL(140, "ret==0", ret, 0);
	}
};

/* the lock's state, the head's count, as the infos read it */
struct RwlockState {
	uint8_t writer;
	uint8_t reserved;
	uint16_t readers;
};

/* SceUltReaderWriterLock */
class Rwlock {
public:
	UltObjectHeader header;
	uint32_t m_writeOwner;		/* the writer's thread ID (AbstractThread::getID2) */
	CondWaiterQ<RwlockCallback> m_q;
	NBQueueEntry2 m_dummy;
	SceUltWaitingQueueResourcePool *m_waitingQueueResourcePool;
	uint32_t ulobjId;

	const volatile RwlockState *state() const
	{
		return (const volatile RwlockState *)&m_q.mHead;
	}

	ULT_INLINE int create(const char *name, SceUltWaitingQueueResourcePool *waitingQueueResourcePool,
			      const SceUltReaderWriterLockOptParam *optParam, uint32_t buildVersion)
	{
		SceUltReaderWriterLockOptParam defaultOptParam;

		if (this == NULL || name == NULL)
			return SCE_ULT_ERROR_NULL;
		if (!isAligned8(this) || !isAligned8(waitingQueueResourcePool))
			return SCE_ULT_ERROR_ALIGNMENT;
		RWLOCK_ASSERT(219, "buildVersion >= 0x00910000", buildVersion >= ULT_BUILD_VERSION_MIN);
		if (optParam == NULL) {
			_sceUltReaderWriterLockOptParamInitialize(&defaultOptParam, ULT_VERSION);
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
		header.type = ULT_OBJECT_TYPE_READER_WRITER_LOCK;
		m_waitingQueueResourcePool = waitingQueueResourcePool;
		ret = m_q.initialize(0, internalOf(waitingQueueResourcePool), &m_dummy);
		if (ret != SCE_OK)
			return ret;
		m_writeOwner = 0;
		SceUlobjDbg_D7F0F610(this, ULOBJ_DBG_KIND_SYNC_OBJECT, &ulobjId);
		__builtin_dmb();
		return SCE_OK;
	}

	__attribute__((noinline)) int tryLock(int32_t operation)
	{
		if (this == NULL)
			return SCE_ULT_ERROR_NULL;
		if (!isAligned8(this))
			return SCE_ULT_ERROR_ALIGNMENT;
		int ret = m_q.acquire(operation, INVALID_ID, this);
		if (ret != SCE_OK)
			return ret;
		if (operation == RWLOCK_OPERATION_WRITE)
			m_writeOwner = AbstractThread::getID2();
		return SCE_OK;
	}

	__attribute__((noinline)) int lock(int32_t operation)
	{
		if (!AbstractThread::canWait())
			return SCE_ULT_ERROR_PERMISSION;
		int ret = tryLock(operation);
		if (ret != SCE_ULT_ERROR_AGAIN)
			return ret;
		WaitingQueueResourcePoolInternal *pool = m_q.m_pool;
		if (pool == NULL)
			return SCE_ULT_ERROR_BUSY;
		NBQueueEntry2 *entry = pool->allocateWaiterEntry();
		if (entry == NULL)
			return SCE_ULT_ERROR_BUSY;

		Waiter waiter;
		ret = waiter.initialize(this);
		RWLOCK_ASSERT_EQUAL(300, "ret==0", ret, 0);
		entry->data = (uint32_t)&waiter;
		__builtin_dmb();
		ret = m_q.acquire(operation, makeID(operation, (uint32_t)entry), this);
		if (ret == SCE_OK || ret == SCE_ULT_ERROR_STATE) {
			entry->data = 0;
			waiter.finalize();
			m_q.freeEntry(entry);
			if (ret != SCE_OK)
				return ret;
		} else {
			RWLOCK_ASSERT_EQUAL(319, "ret==CondWaiterQ<RwlockCallback>::WAIT", ret,
					    CondWaiterQ<RwlockCallback>::WAIT);
			void *dret = waiter.wait(this);
			RWLOCK_ASSERT_EQUAL(321, "dret==NULL", (int)dret, 0);
			waiter.finalize();
		}
		if (operation == RWLOCK_OPERATION_WRITE)
			m_writeOwner = AbstractThread::getID2();
		return SCE_OK;
	}

	__attribute__((noinline)) int unlock(int32_t operation)
	{
		if (this == NULL)
			return SCE_ULT_ERROR_NULL;
		if (!isAligned8(this))
			return SCE_ULT_ERROR_ALIGNMENT;
		int ret = m_q.release(operation, this);
		if (ret != SCE_OK)
			return ret;
		if (operation == RWLOCK_OPERATION_WRITE)
			m_writeOwner = 0;
		return SCE_OK;
	}

	ULT_INLINE int destroy()
	{
		if (this == NULL)
			return SCE_ULT_ERROR_NULL;
		if (!isAligned8(this))
			return SCE_ULT_ERROR_ALIGNMENT;
		int ret = m_q.destroy();
		if (ret != SCE_OK)
			return ret;
		SceUlobjDbg_F9C0F5DA(ulobjId);
		return SCE_OK;
	}
};

#endif
