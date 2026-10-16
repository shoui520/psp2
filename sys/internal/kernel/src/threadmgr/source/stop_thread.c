/*
 * SceKernelThreadMgr (threadmgr.c): stopping another thread. A suspend or
 * exit request is marked on the thread; wherever it is (running, queued,
 * waiting, suspended, ...) it is moved so that it runs at the given priority
 * on any CPU until it reaches the point where it stops: the trap on the way
 * out of a syscall, or, when it is not in one, an exit set up in its saved
 * context. A suspension that can happen at once happens here.
 */

#include "private.h"
#include "sce/intrmgr.h"
#include "sce/cpu.h"
#include "sce/debug.h"
#include "sce/sysmem.h"
#include "event.h"
#include "scheduler.h"
#include "syscall_context.h"
#include "uid_waitable.h"

#undef __FILE_NAME__
#define __FILE_NAME__ "threadmgr.c"

#define THREAD_CPU_NONE			0x7fff
#define CPU_MASK_ALL			0x0000000f
#define WAIT_TYPE_THREAD_SUSPEND	0x00000008	/* PLACEHOLDER: woken when the thread is suspended */
#define WAIT_TYPE_COND			0x00000100
#define WAIT_TYPE_MUTEX_2		0x00000200
#define WAIT_TYPE_LW_COND		0x00200000
#define WAIT_TYPE_LW_MUTEX_2		0x00400000
#define WAIT_ATTR_UNINTERRUPTIBLE	0x00008000	/* PLACEHOLDER: name */
#define WAIT_ATTR_PRIORITY		0x00002000	/* waiters queue by priority */
#define EVENT_ATTR_PRIORITY		WAIT_ATTR_PRIORITY
#define EVENT_DELETED_PATTERN		0x00000010	/* PLACEHOLDER: the pattern a cut wait reports */

#define SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID	0x80028021
#define SCE_KERNEL_ERROR_ILLEGAL_THREAD_ID	0x80028022
#define SCE_KERNEL_ERROR_ILLEGAL_PRIORITY	0x80028023
#define SCE_KERNEL_ERROR_DORMANT		0x80028027
#define SCE_KERNEL_ERROR_DELETED		0x8002802a
#define SCE_KERNEL_ERROR_THREAD_STOPPED		0x8002802c
#define SCE_KERNEL_ERROR_THREAD_SUSPENDED	0x8002802d

extern int ksceKernelSpinlockLowTrylock(SceKernelSpinlock *lock);
extern int kscePUIDClose(SceUID pid, SceUID puid);
extern int ksceGUIDClose(SceUID guid);
extern int ksceGUIDReferObjectWithSubclass(SceUID uid, SceClass *cls, SceObjectBase **object);
void setupThreadExit(struct ThreadCB *thread, uint32_t request);
void insertPriorityQueue(uintptr_t head, struct ThreadCB *thread);
int revalidateWaitState(uintptr_t waitable);
void runningThreadPreemptByIntr(struct ThreadCB *thread, uint32_t reason);
void issueStopThreadIntrRequest(unsigned int cpuId, SceUID threadId, uint32_t arg1,
				uint32_t arg2);

/* the thread goes on the manager's suspended list (its lock held) */
static inline void listSuspended(struct ThreadCB *thread)
{
	struct ThreadmgrWaitNode *self = (struct ThreadmgrWaitNode *)thread, *head;

	ksceKernelSpinlockLowLock(&s_threadmgrRuntime.manager->threadMgrCBLock);
	head = (struct ThreadmgrWaitNode *)&s_threadmgrRuntime.manager->suspendedNext;
	self->next = head;
	self->prev = head->prev;
	head->prev = self;
	self->prev->next = self;
	ksceKernelSpinlockLowUnlock(&s_threadmgrRuntime.manager->threadMgrCBLock);
}

/* the thread leaves the manager list it is on */
static inline void unlistThread(struct ThreadCB *thread)
{
	struct ThreadmgrWaitNode *self = (struct ThreadmgrWaitNode *)thread;

	ksceKernelSpinlockLowLock(&s_threadmgrRuntime.manager->threadMgrCBLock);
	self->next->prev = self->prev;
	self->prev->next = self->next;
	ksceKernelSpinlockLowUnlock(&s_threadmgrRuntime.manager->threadMgrCBLock);
}

/* the thread leaves its CPU's standby list */
static inline void unlistStandby(struct ThreadMgrCpuCB *cpu,
				 struct ThreadCB *thread)
{
	struct ThreadmgrWaitNode *self = (struct ThreadmgrWaitNode *)thread;

	ksceKernelSpinlockLowLock(&cpu->standbyLock);
	self->next->prev = self->prev;
	self->prev->next = self->next;
	ksceKernelSpinlockLowUnlock(&cpu->standbyLock);
}

/* until it stops, the thread runs at priority on any CPU */
static inline void boostToStop(struct ThreadCB *thread, int priority)
{
	thread->savedCpuAffinityMask = thread->currentCpuAffinityMask;
	thread->currentPriority = priority;
	thread->currentCpuAffinityMask |= CPU_MASK_ALL;
}

/* a priority only this CPU's queues hold (not the shared 0x80-0xfb) */
static inline int cpuLocalPriority(int priority)
{
	return (unsigned int)(priority - 0x80) > 0x7b;
}

/*
 * Stops the thread threadId: request is a THREAD_FLAG_REQUEST_* bit, the
 * thread runs at priority until it gets there. A thread on another CPU is
 * reached with SGI kind 9.
 */
int stopThread(SceUID threadId, int priority, uint32_t request)
{
	struct ThreadMgrCpuCB *cpu;
	struct ThreadmgrThreadObject *object;
	struct ThreadCB *thread;
	struct ThreadmgrWaitNode *head, *node, *next;
	struct ThreadmgrWaitQueue *queue;
	struct ThreadmgrExceptionEntry *entry;
	ThreadmgrWaitMultipleEvents *wme;
	ThreadmgrEventWaitNode *eventNode;
	struct ThreadCB *waiter;
	ThreadmgrEvent *event;
	SceUID waitId, eventId;
	uint32_t state, status, suspend, i, left;
	int intr, ret, error, cpuId, target;
	volatile int spin;
	uint32_t wake;

	if (threadId == 0 ||
	    (currentThreadContext() != NULL && currentThreadContext()->threadId == threadId))
		return SCE_KERNEL_ERROR_ILLEGAL_THREAD_ID;
	if ((unsigned int)(priority - 1) > 0xfd)
		return SCE_KERNEL_ERROR_ILLEGAL_PRIORITY;
	intr = ksceKernelCpuSuspendIntr();
	cpuId = ksceKernelCpuId();
	cpu = currentCpuState();
	ret = ksceGUIDReferObjectWithClass(threadId,
		s_threadmgrRuntime.manager->threadClass, (SceObjectBase **)&object);
	if (ret < 0) {
		ret = SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID;
		goto resume;
	}
	thread = &object->context;
	wake = 0;
	suspend = request & THREAD_FLAG_REQUEST_SUSPEND;
retry:
	ksceKernelSpinlockLowLock(THREAD_LOCK(thread));
	if (thread->flags & THREAD_FLAG_PENDING_STOP)
		goto unlock;
	state = thread->status;
	if (state & THREAD_STATE_QUEUED) {
		/* on a CPU: another CPU's thread is stopped there */
		if (thread->currentCpuId != cpuId && thread->currentCpuId != THREAD_CPU_NONE) {
			issueStopThreadIntrRequest(thread->currentCpuId, threadId, priority, request);
			goto unlock;
		}
		ksceKernelAtomicOrAndGet32((SceInt32 *)&thread->flags, request);
		if (thread->status & SCE_KERNEL_THREAD_STATUS_RUNNING) {
			/* it is the thread this SGI interrupted */
			SCE_DBG_ASSERT_CHECK(ksceKernelIsIntrAllowedInCurrentContext() != 0, 8809);
			SCE_DBG_ASSERT_CHECK(!(thread->flags & THREAD_FLAG_SELF_STOPPED), 8810);
			if (!(request & THREAD_FLAG_REQUEST_SUSPEND)) {
				if (thread->userFrame == NULL)
					setupThreadExit(thread, request);
				thread->currentPriority = priority;
				cpu->priority = priority;
				cpu->disabledDispatch = 0;
				if (searchHighestPriority() < priority) {
					SCE_DBG_ASSERT_LEVEL(0, cpu == currentCpuState(), 8848);
					wake = 1;
					cpu->nextRunningThread = 0;
				}
				goto unlock;
			}
			ksceKernelAtomicOrAndGet32((SceInt32 *)&thread->flags,
						   THREAD_FLAG_SUSPEND_COUNTED);
			if (thread->userFrame == NULL) {
				runningThreadPreemptByIntr(thread, 2);
				ksceKernelAtomicClearMask32((SceInt32 *)&thread->flags,
							    THREAD_FLAG_REQUEST_MASK);
				thread->status = (thread->status >> 16) << 16 |
						     SCE_KERNEL_THREAD_STATUS_SUSPENDED;
				SCE_DBG_ASSERT_CHECK(thread->currentCpuId == (int)ksceKernelCpuId(), 8832);
				thread->currentCpuId = -1;
				wake = 1;
				goto suspended;
			}
			boostToStop(thread, priority);
			cpu->priority = priority;
			if (searchHighestPriority() < priority) {
				SCE_DBG_ASSERT_LEVEL(0, cpu == currentCpuState(), 8820);
				wake = 1;
				cpu->nextRunningThread = 0;
			}
			goto unlock;
		}
		if (thread->status & SCE_KERNEL_THREAD_STATUS_READY) {
			if (!(request & THREAD_FLAG_REQUEST_SUSPEND)) {
				if (thread->userFrame == NULL)
					setupThreadExit(thread, request);
				removeReady(thread, thread->currentPriority);
				thread->currentPriority = priority;
				if (!cpuLocalPriority(priority))
					goto distribute;
				addReady(thread);
				if (priority < cpu->priority) {
					SCE_DBG_ASSERT_LEVEL(0, cpu == currentCpuState(), 8898);
					wake = 1;
					cpu->nextRunningThread = 0;
				}
				goto unlock;
			}
			ksceKernelAtomicOrAndGet32((SceInt32 *)&thread->flags,
						   THREAD_FLAG_SUSPEND_COUNTED);
			if (thread->userFrame == NULL || (thread->flags & THREAD_FLAG_SELF_STOPPED)) {
				ksceKernelAtomicClearMask32((SceInt32 *)&thread->flags,
							    THREAD_FLAG_REQUEST_MASK);
				removeReady(thread, thread->currentPriority);
				thread->status = (thread->status >> 16) << 16 |
						     SCE_KERNEL_THREAD_STATUS_SUSPENDED;
				SCE_DBG_ASSERT_CHECK(thread->currentCpuId == (int)ksceKernelCpuId() ||
						     thread->currentCpuId == THREAD_CPU_NONE, 8880);
				goto offCpu;
			}
			removeReady(thread, thread->currentPriority);
			thread->currentPriority = priority;
			thread->savedCpuAffinityMask = thread->currentCpuAffinityMask;
			thread->currentCpuAffinityMask |= CPU_MASK_ALL;
			if (!cpuLocalPriority(priority))
				goto distribute;
			addReady(thread);
			if (priority < cpu->priority) {
				SCE_DBG_ASSERT_LEVEL(0, cpu == currentCpuState(), 8869);
				wake = 1;
				cpu->nextRunningThread = 0;
			}
			goto unlock;
		}
		/* on standby */
		if (request & THREAD_FLAG_REQUEST_SUSPEND) {
			ksceKernelAtomicOrAndGet32((SceInt32 *)&thread->flags,
						   THREAD_FLAG_SUSPEND_COUNTED);
			if (thread->userFrame == NULL || (thread->flags & THREAD_FLAG_SELF_STOPPED)) {
				ksceKernelAtomicClearMask32((SceInt32 *)&thread->flags,
							    THREAD_FLAG_REQUEST_MASK);
				unlistStandby(cpu, thread);
				thread->status = (thread->status >> 16) << 16 |
						     SCE_KERNEL_THREAD_STATUS_SUSPENDED;
				SCE_DBG_ASSERT_CHECK(thread->currentCpuId == (int)ksceKernelCpuId(), 8936);
				goto offCpu;
			}
			thread->currentPriority = priority;
			thread->savedCpuAffinityMask = thread->currentCpuAffinityMask;
			thread->currentCpuAffinityMask |= CPU_MASK_ALL;
			if (cpuLocalPriority(priority)) {
				if (priority < cpu->priority) {
					SCE_DBG_ASSERT_LEVEL(0, cpu == currentCpuState(), 8921);
					wake = 1;
					cpu->nextRunningThread = 0;
				}
				goto unlock;
			}
		} else {
			if (thread->userFrame == NULL)
				setupThreadExit(thread, request);
			thread->currentPriority = priority;
			if (cpuLocalPriority(priority)) {
				if (priority < cpu->priority) {
					SCE_DBG_ASSERT_LEVEL(0, cpu == currentCpuState(), 8956);
					wake = 1;
					cpu->nextRunningThread = 0;
				}
				goto unlock;
			}
		}
		unlistStandby(cpu, thread);
		goto distribute;
	}
	if (state & SCE_KERNEL_THREAD_STATUS_STAGNANT) {
		ksceKernelAtomicOrAndGet32((SceInt32 *)&thread->flags, request);
		unlistThread(thread);
		if (!(request & THREAD_FLAG_REQUEST_SUSPEND))
			goto boostAndRun;
		ksceKernelAtomicOrAndGet32((SceInt32 *)&thread->flags,
					   THREAD_FLAG_SUSPEND_COUNTED);
		if (thread->userFrame == NULL || (thread->flags & THREAD_FLAG_SELF_STOPPED)) {
			ksceKernelAtomicClearMask32((SceInt32 *)&thread->flags,
						    THREAD_FLAG_REQUEST_MASK);
			SCE_DBG_ASSERT_CHECK(thread->currentCpuId == -1, 8990);
			thread->status = (thread->status >> 16) << 16 | SCE_KERNEL_THREAD_STATUS_SUSPENDED;
			goto suspended;
		}
		boostToStop(thread, priority);
		target = selectExecCpu(thread);
		if ((unsigned int)target > 3) {
			SCE_DBG_PRINTF(0, 8983, " k: can not select cpu(0x%08X)\n", target);
			SCE_PANIC(SCE_LINE(8979, 8984));
		}
		wake = distributeThread(target, thread);
		goto unlock;
	}
	if (state & THREAD_STATE_OFF_CPU) {
		ksceKernelAtomicOrAndGet32((SceInt32 *)&thread->flags, request);
		unlistThread(thread);
		if (!(request & THREAD_FLAG_REQUEST_SUSPEND))
			goto boostAndRun;
		ksceKernelAtomicOrAndGet32((SceInt32 *)&thread->flags,
					   THREAD_FLAG_SUSPEND_COUNTED);
		if (thread->userFrame == NULL || (thread->flags & THREAD_FLAG_SELF_STOPPED)) {
			ksceKernelAtomicClearMask32((SceInt32 *)&thread->flags,
						    THREAD_FLAG_REQUEST_MASK);
			thread->status &= THREAD_STATE_DEBUG_SUSPENDED | 0xffff;
			SCE_DBG_ASSERT_CHECK(thread->currentCpuId == -1, 9043);
			thread->status = (thread->status & 0xffff0000) | SCE_KERNEL_THREAD_STATUS_SUSPENDED;
			listSuspended(thread);
			goto wakeSuspendWaiters;
		}
		boostToStop(thread, priority);
		wake = distributeThread(selectExecCpu(thread), thread);
		goto unlock;
	}
	if (state & SCE_KERNEL_THREAD_STATUS_WAITING) {
		ksceKernelAtomicOrAndGet32((SceInt32 *)&thread->flags, request);
		if (suspend) {
			ksceKernelAtomicOrAndGet32((SceInt32 *)&thread->flags,
						   THREAD_FLAG_SUSPEND_COUNTED);
			error = SCE_KERNEL_ERROR_THREAD_SUSPENDED;
		} else {
			error = SCE_KERNEL_ERROR_THREAD_STOPPED;
		}
		waitId = thread->waitId;
		if (waitId == WAIT_ID_MULTIPLE_EVENTS) {
			if (thread->flags & THREAD_FLAG_WAIT_MARK) {
				/* in a syscall: the wait ends with the error */
				boostToStop(thread, priority);
				ksceKernelAtomicOrAndGet32((SceInt32 *)&thread->flags,
							   THREAD_FLAG_SIGNAL_DEFERRED);
				ksceKernelAtomicClearMask32((SceInt32 *)&thread->flags,
							    THREAD_FLAG_RESUMED_WAIT);
				wake |= wakeupAndDistributeThread(thread, 0, error);
				goto unlock;
			}
			/* each event of the WME: cut its node, or requeue it by the new priority */
			wme = (ThreadmgrWaitMultipleEvents *)thread->next;
			thread->currentPriority = priority;
			for (i = 0; i < wme->count; i++) {
				if (wme->nodes[i].next == &wme->nodes[i])
					continue;
				eventNode = (ThreadmgrEventWaitNode *)((uintptr_t)wme->nodes[i].next -
					offsetof(ThreadmgrEventWaitNode, wmeLink));
				eventId = eventNode->eventId;
				if (ksceGUIDReferObjectWithSubclass(eventId,
					s_threadmgrRuntime.manager->eventClass,
					(SceObjectBase **)&event) < 0)
					SCE_PANIC(SCE_LINE(9110, 9115));
				if (ksceKernelSpinlockLowTrylock(&event->lock) < 0) {
					ksceKernelAtomicClearMask32((SceInt32 *)&thread->flags,
						request | THREAD_FLAG_SUSPEND_COUNTED);
					thread->currentPriority =
						thread->basePriority < thread->boostPriority ?
						thread->basePriority : thread->boostPriority;
					ksceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
					ksceGUIDReleaseObject(eventId);
					goto retry;
				}
				if (event->attr & EVENT_ATTR_UNINTERRUPTIBLE) {
					left = unlinkEventWaitNode(eventNode, wme);
					if (wme->busy == 0 && thread->waitId != WAIT_ID_MULTIPLE_EVENTS) {
						ret = releaseEventWaiter(event, eventNode, thread);
					} else {
						addEventWaitResult(wme, eventNode->eventId, error,
								   EVENT_DELETED_PATTERN, 0);
						markEventReady(wme, eventNode->index);
						if (thread->waitId == WAIT_ID_MULTIPLE_EVENTS &&
						    (left == 0 || wme->mode == WME_MODE_ANY))
							wake |= wakeupAndDistributeThread(thread,
								EVENT_WAIT_QUEUE(event), 0);
						else
							ret = releaseEventWaiter(event, eventNode, thread);
					}
					if (freeEventWaitNode(thread->processId, eventNode) < 0)
						SCE_PANIC(SCE_LINE(9169, 9174));
				} else if (event->attr & EVENT_ATTR_PRIORITY) {
					/* requeued in front of the first waiter of a lower priority */
					eventNode->next->prev = eventNode->prev;
					eventNode->prev->next = eventNode->next;
					head = (struct ThreadmgrWaitNode *)EVENT_WAIT_QUEUE(event);
					for (node = head->next; node != head; node = node->next) {
						waiter = EventWaiterIsThread(node) ?
							(struct ThreadCB *)node :
							(struct ThreadCB *)(node->owner & ~3);
						SCE_DBG_ASSERT_CHECK(waiter->magic == THREAD_CB_MAGIC,
								     9190);
						if (thread->currentPriority < waiter->currentPriority)
							break;
					}
					eventNode->next = node;
					eventNode->prev = node->prev;
					node->prev = (struct ThreadmgrWaitNode *)eventNode;
					eventNode->prev->next = (struct ThreadmgrWaitNode *)eventNode;
				}
				ksceKernelSpinlockLowUnlock(&event->lock);
				ksceGUIDReleaseObject(eventNode->eventId);
			}
			thread->savedCpuAffinityMask = thread->currentCpuAffinityMask;
			thread->currentCpuAffinityMask |= CPU_MASK_ALL;
			goto unlock;
		}
		ret = ksceGUIDReferObjectWithSubclass(waitId,
			s_threadmgrRuntime.manager->waitableClass,
			(SceObjectBase **)&queue);
		if (ret < 0) {
			/* only a delay */
			SCE_DBG_ASSERT_CHECK(thread->delayId != -1, 9232);
			waitId = -1;
			queue = NULL;
			ret = 0;
		} else {
			queue = (struct ThreadmgrWaitQueue *)((uintptr_t)queue + 0x10);
			if (ksceKernelSpinlockLowTrylock(&queue->lock) < 0) {
				ksceKernelAtomicClearMask32((SceInt32 *)&thread->flags,
					request | THREAD_FLAG_SUSPEND_COUNTED);
				ksceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
				ksceGUIDReleaseObject(waitId);
				goto retry;
			}
		}
		status = thread->flags;
		boostToStop(thread, priority);
		if ((status & THREAD_FLAG_WAIT_MARK) ||
		    (queue != NULL && (queue->attributes & WAIT_ATTR_UNINTERRUPTIBLE))) {
			/* the wait ends with the error */
			SCE_DBG_ASSERT_CHECK(!(status & THREAD_FLAG_COND_RETRY_MASK), 9242);
			if (suspend) {
				switch (thread->waitType) {
				case WAIT_TYPE_COND:
					ksceKernelAtomicOrAndGet32((SceInt32 *)&thread->flags,
								   THREAD_FLAG_COND_KEEP_MUTEX);
					break;
				case WAIT_TYPE_MUTEX_2:
					ksceKernelAtomicOrAndGet32((SceInt32 *)&thread->flags,
								   THREAD_FLAG_COND_RELOCK);
					break;
				case WAIT_TYPE_LW_COND:
					ksceKernelAtomicOrAndGet32((SceInt32 *)&thread->flags,
								   THREAD_FLAG_LW_COND_KEEP_MUTEX);
					break;
				case WAIT_TYPE_LW_MUTEX_2:
					ksceKernelAtomicOrAndGet32((SceInt32 *)&thread->flags,
								   THREAD_FLAG_LW_COND_RELOCK);
					break;
				}
			}
			ksceKernelAtomicOrAndGet32((SceInt32 *)&thread->flags,
						   THREAD_FLAG_SIGNAL_DEFERRED);
			ksceKernelAtomicClearMask32((SceInt32 *)&thread->flags,
						    THREAD_FLAG_RESUMED_WAIT);
			wake |= wakeupAndDistributeThread(thread, (uintptr_t)queue, error);
		} else if (queue != NULL && (queue->attributes & WAIT_ATTR_PRIORITY)) {
			((struct ThreadmgrWaitNode *)thread)->next->prev =
				((struct ThreadmgrWaitNode *)thread)->prev;
			((struct ThreadmgrWaitNode *)thread)->prev->next =
				((struct ThreadmgrWaitNode *)thread)->next;
			insertPriorityQueue((uintptr_t)queue, thread);
		}
		ksceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
		if (waitId != -1) {
			wake = revalidateWaitState((uintptr_t)queue);
			ksceKernelSpinlockLowUnlock(&queue->lock);
			ksceGUIDReleaseObject(waitId);
		}
		goto release;
	}
	if (state & SCE_KERNEL_THREAD_STATUS_DORMANT) {
		if (!(request & THREAD_FLAG_REQUEST_EXIT_DELETE)) {
			ret = SCE_KERNEL_ERROR_DORMANT;
			goto unlock;
		}
		/* deleting a dormant thread: its UIDs close now */
		SCE_DBG_ASSERT_CHECK(thread->currentCpuId == -1, 9288);
		thread->currentCpuId = -1;
		thread->status = (thread->status >> 16) << 16 | SCE_KERNEL_THREAD_STATUS_DELETED;
		ksceKernelSpinlockLowLock(
			&s_threadmgrRuntime.manager->threadMgrCBLock);
		((struct ThreadmgrWaitNode *)thread)->next->prev =
			((struct ThreadmgrWaitNode *)thread)->prev;
		((struct ThreadmgrWaitNode *)thread)->prev->next =
			((struct ThreadmgrWaitNode *)thread)->next;
		head = (struct ThreadmgrWaitNode *)&s_threadmgrRuntime.manager->closedNext;
		((struct ThreadmgrWaitNode *)thread)->next = head;
		((struct ThreadmgrWaitNode *)thread)->prev = head->prev;
		head->prev = (struct ThreadmgrWaitNode *)thread;
		((struct ThreadmgrWaitNode *)thread)->prev->next = (struct ThreadmgrWaitNode *)thread;
		ksceKernelSpinlockLowUnlock(
			&s_threadmgrRuntime.manager->threadMgrCBLock);
		if (thread->userThreadId != -1) {
			ret = kscePUIDClose(thread->processId, thread->userThreadId);
			if (ret < 0) {
				SCE_DBG_ASSERT_LEVEL(0, 0, 9306);
				goto unlock;
			}
		}
		ret = ksceGUIDClose(thread->threadId);
		goto unlock;
	}
	if (state & SCE_KERNEL_THREAD_STATUS_DELETED) {
		ret = SCE_KERNEL_ERROR_DELETED;
		goto unlock;
	}
	if (!(state & SCE_KERNEL_THREAD_STATUS_SUSPENDED)) {
		SCE_DBG_PRINTF(0, 9375, "error: unexpected thread status(%p, 0x%08x)\n", thread,
			       state);
		SCE_PANIC(SCE_LINE(9371, 9376));
	}
	/* suspended */
	if (suspend) {
		if (thread->flags & THREAD_FLAG_SUSPEND_COUNTED) {
			ret = SCE_KERNEL_ERROR_THREAD_SUSPENDED;
		} else {
			ret = 0;
			ksceKernelAtomicOrAndGet32((SceInt32 *)&thread->flags,
						   THREAD_FLAG_SUSPEND_COUNTED);
		}
		goto unlock;
	}
	if (ksceKernelSpinlockLowTrylock(
		&thread->process->exception->lock) < 0) {
		ksceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
		for (spin = 10; spin != 0; spin--)
			;
		goto retry;
	}
	if (thread->flags & THREAD_FLAG_EXCEPTION) {
		entry = thread->exceptionEntry;
		entry->next->prev = entry->prev;
		entry->prev->next = entry->next;
		ksceKernelAtomicClearMask32((SceInt32 *)&thread->flags, THREAD_FLAG_EXCEPTION);
	}
	ksceKernelSpinlockLowUnlock(&thread->process->exception->lock);
	ksceKernelAtomicOrAndGet32((SceInt32 *)&thread->flags, request);
	unlistThread(thread);
	boostToStop(thread, priority);
	thread->status &= 0xffff;
	if (thread->userFrame == NULL)
		setupThreadExit(thread, request);
	goto distribute;

boostAndRun:
	boostToStop(thread, priority);
	if (thread->userFrame == NULL)
		setupThreadExit(thread, request);
distribute:
	wake = distributeThread(selectExecCpu(thread), thread);
	goto unlock;

offCpu:
	thread->currentCpuId = -1;
suspended:
	listSuspended(thread);
wakeSuspendWaiters:
	/* the threads waiting for it to be suspended */
	ksceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
	ksceKernelSpinlockLowLock(&object->waitLock);
	head = (struct ThreadmgrWaitNode *)&object->waitNext;
	for (node = head->next; node != head; node = next) {
		next = node->next;
		SCE_DBG_ASSERT_CHECK(((struct ThreadCB *)node)->magic ==
				     THREAD_CB_MAGIC, 9391);
		ksceKernelSpinlockLowLock(THREAD_LOCK((struct ThreadCB *)node));
		if (((struct ThreadCB *)node)->waitType == WAIT_TYPE_THREAD_SUSPEND)
			wake |= wakeupAndDistributeThread((struct ThreadCB *)node,
							  (uintptr_t)head, 0);
		ksceKernelSpinlockLowUnlock(THREAD_LOCK((struct ThreadCB *)node));
	}
	ksceKernelSpinlockLowUnlock(&object->waitLock);
	goto release;

unlock:
	ksceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
release:
	ksceGUIDReleaseObject(threadId);
	if ((wake & 1) && currentThreadContext() != NULL)
		switchThread((intr >> 7) & 1);
resume:
	ksceKernelCpuResumeIntr(intr);
	return ret;
}
