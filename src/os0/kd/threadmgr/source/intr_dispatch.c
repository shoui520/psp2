/*
 * SceKernelThreadMgr (dispatch.c): an interrupt arriving while the running
 * thread spins on a fast mutex. The spin ends: the thread takes the mutex
 * if it is free, else it waits on the mutex's UID object (or, with a stop
 * or exit request pending, gives up the wait).
 */

#include "private.h"
#include "affinity.h"
#include "fast_mutex.h"
#include "uid_waitable.h"
#include "sce/cpu.h"
#include "sce/debug.h"
#include "syscall_context.h"
#include <psp2kern/kernel/cpu/atomic.h>

#undef __FILE_NAME__
#define __FILE_NAME__ "dispatch.c"

#define FAST_MUTEX_MAGIC		0xa35db473
#define WAIT_TYPE_FAST_MUTEX		0x1000
#define WAIT_TYPE_FAST_MUTEX_SPIN	0x2000
#define WAIT_ATTR_UNINTERRUPTIBLE	0x00008000
#define PREEMPT_WAIT			4
#define SPIN_RETRY_DELAY		10
#define SCE_KERNEL_ERROR_WAIT_EXITING	0x8002802c	/* PLACEHOLDER: names */
#define SCE_KERNEL_ERROR_WAIT_SUSPENDED	0x8002802d

extern void ksceKernelPrintfAssertLevel(SceUInt32 level, SceBool condition,
	const SceKernelDebugInfo *info, const void *lr, const char *format, ...);

/* callThreadmgrBeforeIntrHandler: thread (running here) was spinning on a fast mutex */
void callThreadmgrBeforeIntrHandler(uintptr_t unused, struct ThreadCB *context)
{
	struct ThreadCB *thread = context;
	struct ThreadmgrFastMutex *mutex;
	struct ThreadmgrWaitQueue *queue;
	uint32_t flags, request;
	int count;
	volatile unsigned int delay;

	SCE_DBG_ASSERT_CHECK(thread != NULL, 814);
	ksceKernelSpinlockLowLock(THREAD_LOCK(thread));
	for (;;) {
		SCE_DBG_ASSERT_CHECK(thread->waitType == WAIT_TYPE_FAST_MUTEX_SPIN, 821);
		SCE_DBG_ASSERT_CHECK((thread->status & SCE_KERNEL_THREAD_STATUS_RUNNING) &&
				     thread->currentCpuId == (int)ksceKernelCpuId(), 823);
		SCE_DBG_ASSERT_CHECK(thread->spinMutex->magic == FAST_MUTEX_MAGIC, 829);
		mutex = thread->spinMutex;
		if (ksceKernelSpinlockLowTrylock(&mutex->queue->lock) >= 0)
			break;
		ksceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
		for (delay = SPIN_RETRY_DELAY; delay != 0; delay--)
			;
		ksceKernelSpinlockLowLock(THREAD_LOCK(thread));
	}
	SCE_DBG_ASSERT_PRINTF_LEVEL(0, thread->fastMutexHandoff == 0, 839,
				     "fSpinInterrupted=0x%08X\n", thread->fastMutexHandoff);
	thread->fastMutexHandoff = 1;
	count = mutex->spinWaitCount;
	SCE_DBG_ASSERT_PRINTF_LEVEL(0, count != 0, 842, "error: spinWaitCount=0x%08X\n", count);
	mutex->spinWaitCount--;
	if (mutex->owner == NULL) {
		/* free: it is the thread's */
		thread->spinMutex = NULL;
		thread->waitType = 0;
		ksceKernelSpinlockLowLock(&mutex->lock);
		acquireFastMutex(mutex, context);
		goto unlock;
	}
	flags = thread->flags;
	if (!(flags >> THREAD_FLAG_REQUEST_SHIFT) ||
	    (((flags & THREAD_FLAG_REQUEST_VM_SUSPEND) ||
	      !(mutex->queue->attributes & WAIT_ATTR_UNINTERRUPTIBLE)) && !ThreadmgrInSyscall())) {
		/* it waits on the mutex */
		runningThreadPreemptByIntr(context, PREEMPT_WAIT);
		queue = mutex->queue;
		thread->currentCpuId = -1;
		thread->waitId = mutex->uid;
		thread->waitType = WAIT_TYPE_FAST_MUTEX;
		count = ++queue->waiting;
		SCE_DBG_ASSERT_CHECK(count != 0, 882);
		queue = mutex->queue;
		thread->next = (uintptr_t)queue;
		thread->prev = (uintptr_t)queue->prev;
		thread->status = (thread->status & THREAD_STATE_HIGH_MASK) | SCE_KERNEL_THREAD_STATUS_WAITING;
		queue->prev = (struct ThreadmgrWaitNode *)thread;
		((struct ThreadmgrWaitNode *)thread->prev)->next = (struct ThreadmgrWaitNode *)thread;
	} else {
		/* a stop or exit request: no wait */
		ksceKernelAtomicOrAndGet32((SceInt32 *)&thread->flags, THREAD_FLAG_SIGNAL_DEFERRED);
		ksceKernelAtomicClearMask32((SceInt32 *)&thread->flags, THREAD_FLAG_WAIT_CLEAR);
		request = thread->flags & THREAD_FLAG_REQUEST_MASK;
		if (request == THREAD_FLAG_REQUEST_EXIT_DELETE || request == THREAD_FLAG_REQUEST_EXIT)
			thread->waitResult = SCE_KERNEL_ERROR_WAIT_EXITING;
		else if (request == THREAD_FLAG_REQUEST_SUSPEND)
			thread->waitResult = SCE_KERNEL_ERROR_WAIT_SUSPENDED;
		else
			SCE_DBG_ASSERT_LEVEL(0, 0, 865);
		thread->spinMutex = NULL;
		thread->waitType = 0;
	}
unlock:
	ksceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
	ksceKernelSpinlockLowUnlock(&mutex->queue->lock);
}
