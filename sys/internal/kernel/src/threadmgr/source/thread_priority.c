/*
 * SceKernelThreadMgr (threadmgr.c): changing a thread's priority. A queued
 * thread is requeued (by another CPU's interrupt if it is on that CPU's
 * queues); a waiting one is moved within the priority-ordered queues it
 * waits on. The debugger's hook, the DTrace probe and the priority hook
 * hear of the change.
 */

#include "private.h"
#include "event.h"
#include "priority_change.h"
#include "scheduler.h"
#include "timer.h"
#include "uid_waitable.h"
#include "sce/cpu.h"
#include "sce/debug.h"
#include "sce/sysmem.h"
#include "syscall_context.h"
#include <psp2kern/kernel/sysmem/data_transfers.h>

#undef __FILE_NAME__
#define __FILE_NAME__ "threadmgr.c"

#define CPU_COUNT			4
#define CPU_ID_GLOBAL			0x7fff
#define PRIORITY_SHARED_LOW		0x80
#define PRIORITY_SHARED_RANGE		0x7c
#define WAIT_ID_MULTIPLE		((SceUID)0x80000000)
#define WAIT_ATTR_PRIORITY		0x00002000
#define USER_TLS_PRIORITY		0x7e0
#define TRACE_EVENT_PRIORITY		0x21
#define DEBUG_EVENT_PRIORITY_CHANGE	0x10

typedef void (*ThreadmgrPriorityEventHook)(unsigned int event, const uint32_t *values);
typedef void (*ThreadmgrChangePriorityProbe)(const struct ThreadmgrSchedProbeDescriptor *probe,
	SceUID threadId, SceUID processId, int oldPriority, int newPriority);

extern void ksceKernelPrintfAssertLevel(SceUInt32 level, SceBool condition,
	const SceKernelDebugInfo *info, const void *lr, const char *format, ...);
extern int ksceGUIDReferObjectWithSubclass(SceUID uid, SceClass *cls, SceObjectBase **object);

static inline void unlinkThread(struct ThreadCB *thread)
{
	((struct ThreadCB *)thread->next)->prev = thread->prev;
	((struct ThreadCB *)thread->prev)->next = thread->next;
}

/* cpuId changes threadId's priority */
void issueChangeThreadPriorityIntrRequest(unsigned int cpuId, SceUID threadId, int priority)
{
	struct ThreadmgrIntrRequest *request = allocIntrRequest(INTR_REQUEST_CHANGE_THREAD_PRIORITY);
	ThreadmgrTraceHook hook;

	hook = s_threadmgrRuntime.schedTraceHook;
	request->payload[0] = threadId;
	request->payload[1] = priority;
	if (hook != NULL)
		hook(TRACE_EVENT_PRIORITY, 2, threadId, priority);
	issueIntrRequest(cpuId, request);
}

/* the DTrace probe and the priority hook hear of a thread's new priority */
void notifyThreadPriorityChange(struct ThreadCB *thread,
			       int oldPriority, int newPriority)
{
	ThreadmgrChangePriorityProbe probe;
	ThreadmgrPriorityChangeHook hook;
	SceUID threadId;

	/* the DTrace probe site __sched_:change__pri */
	__asm__ volatile(".global s_schedChangePriorityCodeSite\ns_schedChangePriorityCodeSite:");
	probe = (ThreadmgrChangePriorityProbe)s_schedChangePriorityHandler;
	if (probe != NULL) {
		threadId = thread->userThreadId;
		if (threadId < 0)
			threadId = thread->threadId;
		probe(&s_schedChangePriorityDescriptor, threadId,
		      thread->process->pid, oldPriority, newPriority);
	}
	hook = s_threadmgrRuntime.priorityChangeHook;
	if (hook == NULL)
		return;
	threadId = thread->userThreadId;
	if (threadId < 0)
		threadId = thread->threadId;
	hook(thread->process->pid, threadId, newPriority,
	     oldPriority);
}

/* SceThreadmgrSGI: another CPU asks this one to change threadId's priority */
void changeThreadPrioritySGIHandler(SceUID threadId, int priority)
{
	struct ThreadmgrThreadObject *object;

	if (currentThreadContext() != NULL)
		SCE_PANIC(SCE_LINE(6152, 6158));
	if (ksceGUIDReferObjectWithClass(threadId,
	    s_threadmgrRuntime.manager->threadClass, (SceObjectBase **)&object) >= 0) {
		changeThreadPriority(&object->context, priority);
		ksceGUIDReleaseObject(threadId);
	}
}

/* the running thread should give way to a more urgent one */
#define PREEMPT_SELF(cpu, line) do {						\
	SCE_DBG_ASSERT_LEVEL(0, (cpu) == currentCpuState(), (line));		\
	(cpu)->nextRunningThread = 0;							\
} while (0)

/* changeThreadPriority: thread's current priority; returns 1 if this CPU should switch */
int changeThreadPriority(struct ThreadCB *thread, int priority)
{
	struct ThreadCB *context = thread;
	unsigned int currentCpuId = ksceKernelCpuId();
	struct ThreadMgrCpuCB *cpu = currentCpuState();
	ThreadmgrWaitMultipleEvents *wme;
	ThreadmgrEventWaitNode *node;
	ThreadmgrEvent *event;
	struct ThreadmgrWaitableObject *waitable;
	struct ThreadmgrWaitNode *next;
	struct ThreadCB *waiter;
	ThreadmgrPriorityEventHook hook;
	uint32_t state, index;
	SceUID waitId;
	int oldPriority, status = 0, result, cpuId, highest, target;
	bool notify;

	SCE_DBG_ASSERT_CHECK(ThreadmgrIntrDisabled(), 5907);
	ksceKernelSpinlockLowLock(THREAD_LOCK(context));
	oldPriority = context->currentPriority;
retry:
	state = context->status;
	if (!(state & THREAD_STATE_QUEUED)) {
		if (!(state & SCE_KERNEL_THREAD_STATUS_WAITING)) {
			context->currentPriority = priority;
			goto finish;
		}
		waitId = context->waitId;
		if (waitId == WAIT_ID_MULTIPLE) {
			/* each event it waits on requeues its node by the new priority */
			wme = (ThreadmgrWaitMultipleEvents *)context->next;
			for (index = 0; index < wme->count; index++) {
				if (wme->nodes[index].next == &wme->nodes[index])
					continue;
				node = (ThreadmgrEventWaitNode *)((uintptr_t)wme->nodes[index].next -
					offsetof(ThreadmgrEventWaitNode, wmeLink));
				waitId = node->eventId;
				result = ksceGUIDReferObjectWithSubclass(waitId,
					s_threadmgrRuntime.manager->eventClass,
					(SceObjectBase **)&event);
				if (result < 0)
					SCE_PANIC(SCE_LINE(6005, 6011));
				if (ksceKernelSpinlockLowTrylock(&event->lock) < 0)
					goto retry_wait;
				state = event->attr;
				context->currentPriority = priority;
				if (state & WAIT_ATTR_PRIORITY) {
					node->next->prev = node->prev;
					node->prev->next = node->next;
					for (next = event->next;
					     next != (struct ThreadmgrWaitNode *)&event->next;
					     next = next->next) {
						waiter = EventWaiterIsThread(next) ?
							(struct ThreadCB *)next :
							(struct ThreadCB *)(next->owner & ~3u);
						SCE_DBG_ASSERT_CHECK(waiter->magic == THREAD_CB_MAGIC,
								     6038);
						if (context->currentPriority < waiter->currentPriority)
							break;
					}
					node->next = next;
					node->prev = next->prev;
					next->prev = (struct ThreadmgrWaitNode *)node;
					node->prev->next = (struct ThreadmgrWaitNode *)node;
				}
				ksceKernelSpinlockLowUnlock(&event->lock);
				ksceGUIDReleaseObject(node->eventId);
			}
			goto finish;
		}
		result = ksceGUIDReferObjectWithSubclass(waitId,
			s_threadmgrRuntime.manager->waitableClass,
			(SceObjectBase **)&waitable);
		if (result < 0) {
			SCE_DBG_ASSERT_PRINTF(context->delayId != -1, 6081,
				"*** error: sceGUIDReferObjectWithSubclass(0x%08X)=0x%08X, delayId=0x%08X\n",
				waitId, result, context->delayId);
			context->currentPriority = priority;
			goto finish;
		}
		if (ksceKernelSpinlockLowTrylock(&waitable->lock) < 0)
			goto retry_wait;
		state = waitable->attributes;
		context->currentPriority = priority;
		if (state & WAIT_ATTR_PRIORITY) {
			unlinkThread(context);
			insertPriorityQueue((uintptr_t)&waitable->next, thread);
		}
		if (oldPriority > 0 && (int32_t)context->currentAttr < 0 &&
		    ksceKernelCopyToUserProc(context->processId,
					     (void *)(context->tls - USER_TLS_PRIORITY),
					     (void *)&context->currentPriority, sizeof(int)) < 0)
			SCE_PANIC(SCE_LINE(6093, 6099));
		ksceKernelSpinlockLowUnlock(THREAD_LOCK(context));
		status = revalidateWaitState((uintptr_t)&waitable->next);
		ksceKernelSpinlockLowUnlock(&waitable->lock);
		ksceGUIDReleaseObject(waitId);
		notify = oldPriority > 0 && status >= 0;
		goto notify_change;
	}

	/* queued: on another CPU, that CPU does it */
	cpuId = context->currentCpuId;
	if (cpuId != (int)currentCpuId && cpuId != CPU_ID_GLOBAL) {
		issueChangeThreadPriorityIntrRequest(cpuId, context->threadId, priority);
		oldPriority = -1;
		goto unlock;
	}
	if (state & SCE_KERNEL_THREAD_STATUS_RUNNING) {
		state = cpu->disabledDispatch;
		context->currentPriority = priority;
		cpu->priority = priority;
		if (!state && (int)searchHighestPriority() < priority) {
			PREEMPT_SELF(cpu, 5929);
			status = 1;
		}
	} else if (state & SCE_KERNEL_THREAD_STATUS_READY) {
		removeReady(thread, context->currentPriority);
		context->currentPriority = priority;
		addReady(thread);
		if ((uint32_t)context->currentPriority - PRIORITY_SHARED_LOW < PRIORITY_SHARED_RANGE) {
			/* the CPU running the least urgent thread reschedules */
			highest = 1;
			target = -1;
			for (index = 0; index < CPU_COUNT; index++) {
				if (highest < s_threadmgrRuntime.manager->cpuCB[index].priority) {
					highest = s_threadmgrRuntime.manager->cpuCB[index].priority;
					target = index;
				}
			}
			if (priority < highest) {
				if (target == (int)currentCpuId) {
					PREEMPT_SELF(cpu, 5949);
					status = 1;
				} else {
					addIntrRequest(target, INTR_MASK_SCHEDULING);
				}
			}
		} else if (priority < cpu->priority) {
			PREEMPT_SELF(cpu, 5958);
			status = 1;
		}
	} else {
		context->currentPriority = priority;
		if ((uint32_t)priority - PRIORITY_SHARED_LOW < PRIORITY_SHARED_RANGE) {
			ksceKernelSpinlockLowLock(&cpu->standbyLock);
			unlinkThread(context);
			ksceKernelSpinlockLowUnlock(&cpu->standbyLock);
			status = distributeThread(selectExecCpu(thread), thread);
		} else if (priority < cpu->priority) {
			PREEMPT_SELF(cpu, 5977);
			status = 1;
		}
	}
finish:
	notify = false;
	if (oldPriority > 0) {
		if ((int32_t)context->currentAttr < 0) {
			/* a user thread's TLS reports it */
			result = ksceKernelCopyToUserProc(context->processId,
				(void *)(context->tls - USER_TLS_PRIORITY),
				(void *)&context->currentPriority, sizeof(int));
			if (result < 0) {
				status = result | (ThreadmgrInSyscall() ? SYSCALL_ERROR_BIT : 0);
				goto unlock;
			}
		}
		notify = status >= 0;
	}
	ksceKernelSpinlockLowUnlock(THREAD_LOCK(context));
notify_change:
	if (notify) {
		hook = (ThreadmgrPriorityEventHook)s_threadmgrRuntime.manager->paHooks[1];
		if (hook != NULL) {
			uint32_t values[] = { context->processId, context->threadId,
					      oldPriority, context->currentPriority };

			hook(DEBUG_EVENT_PRIORITY_CHANGE, values);
		}
		notifyThreadPriorityChange(thread, oldPriority, priority);
	}
	return status;
retry_wait:
	ksceKernelSpinlockLowUnlock(THREAD_LOCK(context));
	ksceGUIDReleaseObject(waitId);
	ksceKernelSpinlockLowLock(THREAD_LOCK(context));
	goto retry;
unlock:
	ksceKernelSpinlockLowUnlock(THREAD_LOCK(context));
	return status;
}
