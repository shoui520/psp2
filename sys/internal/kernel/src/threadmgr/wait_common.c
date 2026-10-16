/*
 * SceKernelThreadMgr: wait_common.c, the wait queues of waitable objects:
 * a thread waits in priority order, wakes up with a result and goes back to
 * a CPU; the waiting object's class revalidates or releases the wait.
 */

#include "timer.h"
#include "sce/cpu.h"
#include "sce/debug.h"
#include "sce/sysmem.h"
#include "scheduler.h"
#include "priority_change.h"
#include "uid_waitable.h"
#include "syscall_context.h"

#define WAIT_TYPE_LW_OBJECT		0x40000
typedef void (*ThreadmgrSchedProbeHandler)(const struct ThreadmgrSchedProbeDescriptor *probe,
					   SceUID threadId, SceUID processId);

#define WAIT_FLAG_TIMED			0x01	/* waitType */

extern int ksceGUIDReferObjectWithSubclass(SceUID uid, SceClass *cls, SceObjectBase **object);
extern int ksceKernelSpinlockLowTrylock(SceKernelSpinlock *lock);
extern int ksceKernelCopyToUserProc(SceUID pid, void *dst, const void *src, SceSize len);
extern void ksceKernelPrintfAssertLevel(SceUInt32 level, SceBool condition,
	const SceKernelDebugInfo *info, const void *lr, const char *format, ...);

/* All waitable subclasses place their common queue after the UID base. */
struct ThreadmgrQueuedObject {
	SceObjectBase base;
	uintptr_t cacheNext;
	uintptr_t cachePrev;
	struct ThreadmgrWaitQueue queue;
};

_Static_assert(offsetof(struct ThreadmgrQueuedObject, queue) == 0x10,
	       "ThreadMgr wait queue offset");

static inline struct ThreadmgrQueuedObject *WaitQueueObject(struct ThreadmgrWaitQueue *queue)
{
	return (struct ThreadmgrQueuedObject *)((char *)queue -
		offsetof(struct ThreadmgrQueuedObject, queue));
}

/* the waitable object a queue belongs to, and its class */
static inline struct ThreadmgrWaitableClass *WaitQueueClass(struct ThreadmgrWaitQueue *queue)
{
	return (struct ThreadmgrWaitableClass *)WaitQueueObject(queue)->base.sce_class;
}

/* the thread a queue node stands for */
static inline struct ThreadCB *WaitNodeThread(struct ThreadmgrWaitNode *node)
{
	if (node->owner & 1)
		return (struct ThreadCB *)node;
	return (struct ThreadCB *)(node->owner & ~3u);
}

/* a waiting thread queued after the waiters of its priority and higher */
void insertPriorityQueue(uintptr_t head, struct ThreadCB *thread)
{
	struct ThreadmgrWaitQueue *queue = (struct ThreadmgrWaitQueue *)head;
	struct ThreadmgrWaitNode *node, *prev, *self = (struct ThreadmgrWaitNode *)thread;
	struct ThreadCB *waiter;

#if FW_DEBUG
	if (ksceKernelGetAssertLevel() > 0) {
		SCE_DBG_ASSERT_LEVEL(1, ThreadmgrIntrDisabled(), 68);
		if (ksceKernelGetAssertLevel() > 0) {
			SCE_DBG_ASSERT_LEVEL(1, thread->lock != 0, 71);
			if (ksceKernelGetAssertLevel() > 0)
				SCE_DBG_ASSERT_LEVEL(1, queue->lock != 0, 74);
		}
	}
#endif
	for (node = queue->next; node != (struct ThreadmgrWaitNode *)queue; node = node->next) {
		waiter = WaitNodeThread(node);
		SCE_DBG_ASSERT_CHECK(waiter->magic == THREAD_CB_MAGIC, 84);
		if (thread->currentPriority < waiter->currentPriority)
			break;
	}
	if (thread->waitType == WAIT_TYPE_LW_OBJECT && queue->lwKeepFirst != 0) {
		prev = node->prev;
		if (prev != (struct ThreadmgrWaitNode *)queue)
			goto insert;
		node = node->next;
	}
	prev = node->prev;
insert:
	self->next = node;
	self->prev = prev;
	node->prev = self;
	self->prev->next = self;
}

static uint32_t s_schedSleepProbeId;
volatile uintptr_t s_schedSleepHandler;
extern const char s_schedSleepCodeSite[];

const struct ThreadmgrSchedProbeDescriptor s_schedSleepDescriptor = {
	.probeId = &s_schedSleepProbeId,
	.provider = "__sched_",
	.name = "sleep",
	.codeSite = s_schedSleepCodeSite,
	.handler = &s_schedSleepHandler,
};

typedef void (*ThreadmgrSchedSleepHandler)(const struct ThreadmgrSchedProbeDescriptor *probe);

#define WAIT_ATTR_PRIORITY		0x00002000
#define WAIT_ATTR_UNINTERRUPTIBLE	0x00008000	/* PLACEHOLDER: name */

#define WAIT_TYPE_THREAD_END		0x00000002
#define WAIT_TYPE_THREAD_END_2		0x00000008
#define WAIT_TYPE_EVENT_FLAG		0x00000010
#define WAIT_TYPE_SEMAPHORE		0x00000020
#define WAIT_TYPE_MUTEX			0x00000040
#define WAIT_TYPE_RW_LOCK		0x00000080
#define WAIT_TYPE_COND			0x00000100
#define WAIT_TYPE_MUTEX_2		0x00000200
#define WAIT_TYPE_FAST_MUTEX		0x00001000
#define WAIT_TYPE_EVENT			0x00010000
#define WAIT_TYPE_LW_MUTEX		0x00100000
#define WAIT_TYPE_LW_COND		0x00200000
#define WAIT_TYPE_LW_MUTEX_2		0x00400000
#define WAIT_TYPE_EXCEPTION		0x01000000
#define WAIT_TYPE_WORK_QUEUE		0x04000000
#define WAIT_TYPE_WORK_TASK		0x08000000

#define THREADMGR_ERROR_WAIT_CANCEL	0x800280a2	/* PLACEHOLDER: name */
#define SCE_KERNEL_ERROR_WAIT_DELETE	0x80028006

void cancelDelayTimer(SceUID uid, struct ThreadCB *thread);
int uidDelay_AllocObject(const uint64_t *wakeup, uintptr_t callback);
extern int SceSysrootForDriver_CAE820B2(SceUID pid, SceUInt32 *sdkVersion);

/* the class a wait type's object belongs to */
static inline SceClass *WaitTypeClass(uint32_t waitType)
{
	struct SceKernelThreadmgrPrivate *manager = s_threadmgrRuntime.manager;

	switch (waitType) {
	case WAIT_TYPE_THREAD_END:
	case WAIT_TYPE_THREAD_END_2:
		return manager->threadClass;
	case WAIT_TYPE_EVENT_FLAG:
		return manager->eventFlagClass;
	case WAIT_TYPE_SEMAPHORE:
		return manager->semaphoreClass;
	case WAIT_TYPE_MUTEX:
	case WAIT_TYPE_MUTEX_2:
		return manager->mutexClass;
	case WAIT_TYPE_RW_LOCK:
		return manager->rwLockClass;
	case WAIT_TYPE_COND:
		return manager->condClass;
	case WAIT_TYPE_FAST_MUTEX:
		return manager->fastMutexClass;
	case WAIT_TYPE_EVENT:
		return manager->eventClass;
	case WAIT_TYPE_LW_OBJECT:
		return manager->msgPipeClass;
	case WAIT_TYPE_LW_MUTEX:
	case WAIT_TYPE_LW_MUTEX_2:
		return manager->lwMutexClass;
	case WAIT_TYPE_LW_COND:
		return manager->lwCondClass;
	case WAIT_TYPE_EXCEPTION:
		return manager->exceptionClass;
	case WAIT_TYPE_WORK_QUEUE:
		return manager->workQueueClass;
	case WAIT_TYPE_WORK_TASK:
		return manager->workTaskClass;
	default:
		SCE_PANIC_FUNC(365, "waitWaitableObject");
	}
}

/*
 * The current thread waits on a waitable object (its queue locked by the
 * caller) of waitType, until woken or, with a wakeup time, timed out
 * (remaining time to timeoutOutput). *locked says whether the queue is still
 * locked on return.
 */
int waitWaitableObject(SceUID waitId, uintptr_t waitable, uint32_t waitType,
		       const uint64_t *wakeup, void *timeoutOutput, int *waitCount, int *locked)
{
	struct ThreadmgrWaitQueue *queue = (struct ThreadmgrWaitQueue *)waitable;
	struct ThreadCB *thread =
		currentThreadContext();
	struct ThreadmgrWaitNode *self = (struct ThreadmgrWaitNode *)thread;
	ThreadmgrSchedSleepHandler sleep;
	uint32_t status, value;	/* value: the SDK version, then a zero to copy out */
	SceUID delayId;
	int ret;

#if FW_DEBUG
	if (ksceKernelGetAssertLevel() > 0) {
		SCE_DBG_ASSERT_LEVEL(1, ThreadmgrIntrDisabled(), 128);
		if (ksceKernelGetAssertLevel() > 0) {
			SCE_DBG_ASSERT_LEVEL(1, queue->lock != 0, 131);
			if (ksceKernelGetAssertLevel() > 0)
				SCE_DBG_ASSERT_LEVEL(1, thread->lock != 0, 134);
		}
	}
#endif
	*locked = 1;
	status = thread->flags;
	if ((status & THREAD_FLAG_REQUEST_MASK) != 0 &&
	    (!(status & THREAD_FLAG_REQUEST_VM_SUSPEND) && (queue->attributes & WAIT_ATTR_UNINTERRUPTIBLE)
	     ? 1 : (ThreadmgrUserRoWord() & USERRO_IN_SYSCALL) != 0)) {
		/* a pending suspend or exit request ends the wait before it starts */
		ksceKernelAtomicOrAndGet32((SceInt32 *)&thread->flags, 0x4000);
		ksceKernelAtomicClearMask32((SceInt32 *)&thread->flags, 0x8001);
		switch (thread->flags & THREAD_FLAG_REQUEST_MASK) {
		case THREAD_FLAG_REQUEST_VM_SUSPEND:
		case THREAD_FLAG_REQUEST_SUSPEND:
			thread->waitResult = SCE_KERNEL_ERROR_THREAD_SUSPENDED;
			if (waitType == WAIT_TYPE_MUTEX_2)
				ksceKernelAtomicOrAndGet32((SceInt32 *)&thread->flags, 0x800000);
			else if (waitType == WAIT_TYPE_COND)
				ksceKernelAtomicOrAndGet32((SceInt32 *)&thread->flags, 0x400000);
			else if (waitType == WAIT_TYPE_LW_COND)
				ksceKernelAtomicOrAndGet32((SceInt32 *)&thread->flags, 0x100000);
			else if (waitType == WAIT_TYPE_LW_MUTEX_2)
				ksceKernelAtomicOrAndGet32((SceInt32 *)&thread->flags, 0x200000);
			else
				goto abandon;
			return thread->waitResult;
		case THREAD_FLAG_REQUEST_EXIT_DELETE:
		case THREAD_FLAG_REQUEST_EXIT:
			thread->waitResult = SCE_KERNEL_ERROR_THREAD_STOPPED;
			goto abandon;
		default:
			SCE_PANIC(168);
		}
	}
#if FW_DEBUG
	if (ksceKernelGetAssertLevel() > 0) {
		SCE_DBG_ASSERT_LEVEL(1, thread->delayId == -1, 187);
		if (ksceKernelGetAssertLevel() > 0)
			SCE_DBG_ASSERT_LEVEL(1, thread->timeoutOutput == 0, 188);
	}
#endif
	if (timeoutOutput == NULL) {
		thread->delayId = -1;
		thread->timeoutOutput = 0;
	} else {
		if (waitType == WAIT_TYPE_LW_OBJECT && (thread->waitOptions & 0x10) &&
		    (thread->timeoutProcessId == 0x10005 ||
		     (SceSysrootForDriver_CAE820B2(thread->timeoutProcessId, &value) >= 0 &&
		      value > 0x20fffff)))
			timeoutOutput = NULL;
		delayId = uidDelay_AllocObject(wakeup, (uintptr_t)waitTimeoutHandler);
		if (delayId < 0) {
			SCE_DBG_ASSERT_CHECK(delayId == (SceUID)SCE_KERNEL_ERROR_WAIT_TIMEOUT, 207);
			thread->waitResult = delayId;
			ksceKernelAtomicClearMask32((SceInt32 *)&thread->flags, 1);
			if (waitType == WAIT_TYPE_RW_LOCK) {
				if (thread->waitMode == 1)
					queue->writeWaiters--;
				else
					queue->readWaiters--;
			} else if (waitType == WAIT_TYPE_LW_OBJECT) {
				if (--queue->lwWaiters == 0)
					queue->lwBusy = 0;
				if (timeoutOutput != NULL) {
					if (thread->timeoutProcessId == 0x10005) {
						*(uint32_t *)timeoutOutput = 0;
					} else {
						value = 0;
						ret = ksceKernelCopyToUserProc(thread->timeoutProcessId,
									       timeoutOutput, &value, 4);
						if (ret < 0) {
							SCE_DBG_PRINTF_LEVEL(1, 0, 235,
								"error: waitWaitableObject(): sceKernelCopyToUserProc()=0x%08X\n",
								ret);
							SCE_DBG_ASSERT_FAIL(236);
						}
					}
				}
				return thread->waitResult;
			}
			if (timeoutOutput != NULL)
				*(uint32_t *)timeoutOutput = 0;
			if (waitType == WAIT_TYPE_EVENT_FLAG && thread->waitPatternOutput != NULL) {
				*thread->waitPatternOutput = queue->pattern;
				return thread->waitResult;
			}
			return thread->waitResult;
		}
		thread->delayId = delayId;
		thread->timeoutOutput = (uintptr_t)timeoutOutput;
		thread->remainingTimeLow = ((const uint32_t *)wakeup)[0];
		thread->remainingTimeHigh = ((const uint32_t *)wakeup)[1];
		if ((thread->flags & THREAD_FLAG_CALLBACK_WAKEUP) == THREAD_FLAG_CALLBACK_WAKEUP) {
			cancelDelayTimer(delayId, NULL);
			thread->delayId = -1;
			thread->timeoutOutput = 0;
			goto cancelled;
		}
	}
	if ((thread->flags & THREAD_FLAG_CALLBACK_WAKEUP) == THREAD_FLAG_CALLBACK_WAKEUP) {
cancelled:
		thread->waitResult = THREADMGR_ERROR_WAIT_CANCEL;
		ksceKernelAtomicClearMask32((SceInt32 *)&thread->flags, 1);
		goto abandon;
	}
	thread->waitType = waitType;
	thread->waitId = waitId;
	thread->waitResult = 0;
	SCE_DBG_ASSERT_PRINTF(thread->currentCpuId == ksceKernelCpuId(), 286,
		"pRunningThreadCB->currentCpuId=0x%08X\n", thread->currentCpuId);
	thread->currentCpuId = -1;
	if (ThreadmgrUserRoWord() & USERRO_IN_SYSCALL)
		ksceKernelAtomicOrAndGet32((SceInt32 *)&thread->flags, THREAD_FLAG_WAIT_MARK);
	queue->waiting++;
	SCE_DBG_ASSERT_CHECK(queue->waiting != 0, 294);
	thread->status = (thread->status & 0xffff0000) | SCE_KERNEL_THREAD_STATUS_WAITING;
	if (queue->attributes & WAIT_ATTR_PRIORITY) {
		insertPriorityQueue(waitable, thread);
	} else {
		self->next = (struct ThreadmgrWaitNode *)queue;
		self->prev = queue->prev;
		queue->prev = self;
		self->prev->next = self;
	}
	if (waitCount != NULL)
		(*waitCount)++;
	/* the DTrace probe site __sched_:sleep */
	__asm__ volatile(".global s_schedSleepCodeSite\ns_schedSleepCodeSite:");
	*locked = 0;
	ksceKernelSpinlockLowUnlock(&queue->lock);
	sleep = (ThreadmgrSchedSleepHandler)s_schedSleepHandler;
	if (sleep != NULL)
		sleep(&s_schedSleepDescriptor);
	currentCpuState()->nextRunningThread = 0;
	switchThread(2);
	if (waitType == WAIT_TYPE_EVENT)
		ret = ksceGUIDReferObjectWithSubclass(waitId, WaitTypeClass(waitType), NULL);
	else
		ret = ksceGUIDReferObjectWithClass(waitId, WaitTypeClass(waitType), NULL);
	if (ret < 0) {
		ksceKernelSpinlockLowLock(&thread->lock);
		if (thread->waitResult == (int)THREADMGR_ERROR_WAIT_CANCEL)
			thread->waitResult = SCE_KERNEL_ERROR_WAIT_DELETE;
		return thread->waitResult;
	}
	*locked = 1;
	ksceKernelSpinlockLowLock(&queue->lock);
	ksceKernelSpinlockLowLock(&thread->lock);
	return thread->waitResult;

abandon:
	/* the wait ends before it started */
	if (waitType == WAIT_TYPE_RW_LOCK) {
		if (thread->waitMode == 1)
			queue->writeWaiters--;
		else
			queue->readWaiters--;
	} else if (waitType == WAIT_TYPE_LW_OBJECT) {
		if (--queue->lwWaiters == 0)
			queue->lwBusy = 0;
	}
	return thread->waitResult;
}

/* the end of a thread's wait: the result, the timeout gone, the object told */
int releaseThreadWaitState(struct ThreadCB *thread, uintptr_t waitable,
			   int result)
{
	struct ThreadmgrWaitQueue *queue = (struct ThreadmgrWaitQueue *)waitable;
	struct ThreadmgrWaitableClass *cls;
	SceUID delayId;
	int ret;

#if FW_DEBUG
	if (ksceKernelGetAssertLevel() > 0) {
		SCE_DBG_ASSERT_LEVEL(1, ThreadmgrIntrDisabled(), 525);
		if (ksceKernelGetAssertLevel() > 0)
			SCE_DBG_ASSERT_LEVEL(1, thread->lock != 0, 528);
	}
#endif
	if (queue != NULL)
		SCE_DBG_ASSERT_CHECK(queue->lock != 0, 532);
	ksceKernelAtomicClearMask32((SceInt32 *)&thread->flags, THREAD_FLAG_WAIT_MARK);
	delayId = thread->delayId;
	thread->waitResult = result;
	if (delayId != -1) {
#if FW_DEBUG
		if (ksceKernelGetAssertLevel() > 0) {
			SCE_DBG_ASSERT_PRINTF_LEVEL(1, delayId > 0, 542,
				"error: delayId=0x%08X, threadId=0x%08X\n", delayId, thread->threadId);
			delayId = thread->delayId;
		}
#endif
		cancelDelayTimer(delayId, thread);
		thread->delayId = -1;
		thread->timeoutOutput = 0;
	}
	ret = 0;
	if (queue != NULL) {
		cls = WaitQueueClass(queue);
		ret = cls->release(
			cls, WaitQueueObject(queue), thread);
	}
	thread->waitId = -1;
	thread->waitType = 0;
	return ret;
}

static uint32_t s_schedWakeupProbeId;
volatile uintptr_t s_schedWakeupHandler;
extern const char s_schedWakeupCodeSite[];

const struct ThreadmgrSchedProbeDescriptor s_schedWakeupDescriptor = {
	.probeId = &s_schedWakeupProbeId,
	.provider = "__sched_",
	.name = "wakeup",
	.codeSite = s_schedWakeupCodeSite,
	.handler = &s_schedWakeupHandler,
};

/* a thread leaves the queue it waits in (or the timed-wait list) and is made ready */
bool wakeupAndDistributeThread(struct ThreadCB *thread, uintptr_t waitable,
			       int result)
{
	struct ThreadmgrWaitQueue *queue = (struct ThreadmgrWaitQueue *)waitable;
	struct ThreadmgrWaitNode *self = (struct ThreadmgrWaitNode *)thread;
	ThreadmgrSchedProbeHandler handler;
	SceUID probeId;
#if FW_DEBUG
	bool timedOnly;
#endif

#if FW_DEBUG
	if (ksceKernelGetAssertLevel() > 0) {
		SCE_DBG_ASSERT_LEVEL(1, ThreadmgrIntrDisabled(), 416);
		if (ksceKernelGetAssertLevel() > 0)
			SCE_DBG_ASSERT_LEVEL(1, thread->lock != 0, 419);
	}
#endif
	if (queue != NULL)
		SCE_DBG_ASSERT_CHECK(queue->lock != 0, 423);
	/* the DTrace probe site __sched_:wakeup */
	__asm__ volatile(".global s_schedWakeupCodeSite\ns_schedWakeupCodeSite:");
	handler = (ThreadmgrSchedProbeHandler)s_schedWakeupHandler;
	if (handler != NULL) {
		probeId = thread->userThreadId;
		if (probeId < 0)
			probeId = thread->threadId;
		handler(&s_schedWakeupDescriptor, probeId,
			thread->process->pid);
	}
	if (thread->waitType & WAIT_FLAG_TIMED) {
#if FW_DEBUG
		if (ksceKernelGetAssertLevel() > 0) {
			if (thread->delayId == -1)
				timedOnly = false;
			else if (thread->waitId == -1)
				timedOnly = queue == NULL;
			else
				timedOnly = false;
			SCE_DBG_ASSERT_PRINTF_LEVEL(1, timedOnly, 444,
				" k: wakeupAndDistributeThread(), delayId=0x%08X, waitId=0x%08X, pWaitCB=%p\n",
				thread->delayId, thread->waitId, queue);
		}
#endif
		ksceKernelSpinlockLowLock(&s_threadmgrRuntime.manager->timedWaitLock);
		self->next->prev = self->prev;
		self->prev->next = self->next;
		ksceKernelSpinlockLowUnlock(&s_threadmgrRuntime.manager->timedWaitLock);
	} else {
		self->next->prev = self->prev;
		self->prev->next = self->next;
	}
	releaseThreadWaitState(thread, waitable, result);
	return distributeThread(selectExecCpu(thread), thread);
}

/* every waiter woken with result; returns whether a reschedule is due */
uint32_t wakeupWaitObjectAll(uintptr_t waitable, int result, uint32_t *count)
{
	struct ThreadmgrWaitQueue *queue = (struct ThreadmgrWaitQueue *)waitable;
	struct ThreadmgrWaitNode *node, *next;
	struct ThreadCB *thread;
	uint32_t woken = 0, wake = 0;

#if FW_DEBUG
	if (ksceKernelGetAssertLevel() > 0) {
		SCE_DBG_ASSERT_LEVEL(1, ThreadmgrIntrDisabled(), 480);
		if (ksceKernelGetAssertLevel() > 0)
			SCE_DBG_ASSERT_LEVEL(1, queue->lock != 0, 483);
	}
#endif
	for (node = queue->next; node != (struct ThreadmgrWaitNode *)queue; node = next) {
		thread = (struct ThreadCB *)node;
		next = node->next;
		woken++;
		SCE_DBG_ASSERT_CHECK(thread->magic == THREAD_CB_MAGIC, 493);
		ksceKernelSpinlockLowLock(&thread->lock);
		wake |= wakeupAndDistributeThread(thread, waitable, result);
		ksceKernelSpinlockLowUnlock(&thread->lock);
	}
	if (count != NULL)
		*count = woken;
	return wake;
}

/* the waiting object's class checks its waiters again */
int revalidateWaitState(uintptr_t waitable)
{
	struct ThreadmgrWaitQueue *queue = (struct ThreadmgrWaitQueue *)waitable;
	struct ThreadmgrWaitableClass *cls;

#if FW_DEBUG
	if (ksceKernelGetAssertLevel() > 0) {
		SCE_DBG_ASSERT_LEVEL(1, ThreadmgrIntrDisabled(), 575);
		if (ksceKernelGetAssertLevel() > 0)
			SCE_DBG_ASSERT_LEVEL(1, queue->lock != 0, 578);
	}
#endif
	cls = WaitQueueClass(queue);
	return cls->revalidate(cls, WaitQueueObject(queue));
}

/* a wait timed out: the thread wakes with SCE_KERNEL_ERROR_WAIT_TIMEOUT */
void waitTimeoutHandler(struct ThreadCB *thread)
{
	struct ThreadmgrWaitQueue *queue;
	SceObjectBase *object;
	uint32_t zero;
	SceUID waitId;
	int ret;

	SCE_DBG_ASSERT_LEVEL_CHECK(1, ThreadmgrIntrDisabled(), 601);
	for (;;) {
		ksceKernelSpinlockLowLock(&thread->lock);
		waitId = thread->waitId;
		ret = ksceGUIDReferObjectWithSubclass(waitId,
			s_threadmgrRuntime.manager->waitableClass, &object);
		if (ret < 0) {
			SCE_DBG_ASSERT_PRINTF(waitId == -1 || waitId == (SceUID)0x80000000, 624,
				"*** error: sceGUIDReferObjectWithSubclass(0x%08X)=0x%08X\n",
				waitId, ret);
			queue = NULL;
			object = NULL;
			break;
		}
		queue = &((struct ThreadmgrQueuedObject *)object)->queue;
		if (ksceKernelSpinlockLowTrylock(&queue->lock) >= 0)
			break;
		ksceKernelSpinlockLowUnlock(&thread->lock);
		ksceGUIDReleaseObject(waitId);
	}
	if (thread->timeoutOutput != 0) {
		if (thread->waitType == WAIT_TYPE_LW_OBJECT && thread->timeoutProcessId != 0x10005) {
			zero = 0;
			ret = ksceKernelCopyToUserProc(thread->timeoutProcessId,
						       (void *)thread->timeoutOutput, &zero, 4);
			if (ret < 0)
				SCE_DBG_PRINTF_LEVEL(1, 0, 640,
					"warning: waitTimeoutHandler(): sceKernelCopyToUserProc()=0x%08X\n",
					ret);
		} else {
			*(uint32_t *)thread->timeoutOutput = 0;
		}
		thread->timeoutOutput = 0;
	}
	if (thread->status & SCE_KERNEL_THREAD_STATUS_WAITING)
		wakeupAndDistributeThread(thread, (uintptr_t)queue, SCE_KERNEL_ERROR_WAIT_TIMEOUT);
	ksceKernelSpinlockLowUnlock(&thread->lock);
	if (queue != NULL) {
		revalidateWaitState((uintptr_t)queue);
		ksceKernelSpinlockLowUnlock(&queue->lock);
		ksceGUIDReleaseObject(waitId);
	}
}
