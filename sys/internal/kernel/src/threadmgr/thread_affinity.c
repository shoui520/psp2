/*
 * SceKernelThreadMgr (threadmgr.c): changing a thread's CPU affinity, alone
 * or with its priority. A thread queued on this CPU (or the global queues)
 * is requeued where it may run; one on another CPU's queues is changed by
 * that CPU (an interrupt request). A waiting thread is moved within the
 * priority-ordered queues it waits on.
 */

#include "private.h"
#include "affinity.h"
#include "event.h"
#include "priority_change.h"
#include "timer.h"
#include "uid_waitable.h"
#include "sce/cpu.h"
#include "sce/debug.h"
#include "sce/sysmem.h"
#include "syscall_context.h"
#include <psp2kern/kernel/sysmem/data_transfers.h>

#undef __FILE_NAME__
#define __FILE_NAME__ "threadmgr.c"

#define AFFINITY_MASK			0xf
#define AFFINITY_CURRENT_CPU		0x40000000	/* the mask: the calling CPU */
#define CPU_COUNT			4
#define CPU_ID_GLOBAL			0x7fff
#define PRIORITY_SHARED_LOW		0x80
#define PRIORITY_SHARED_RANGE		0x7c
#define WAIT_ID_MULTIPLE		((SceUID)0x80000000)	/* waiting on several events */
#define WAIT_ATTR_PRIORITY		0x00002000	/* waiters queue by priority */
#define SDK_VERSION_AFFINITY_CHECK	0x01800000
#define SDK_VERSION_DEFAULT_AFFINITY	0x02100000
#define USER_TLS_PRIORITY		0x7e0
#define USER_TLS_AFFINITY		0x7dc
#define PREEMPT_INTR			2
#define DEBUG_EVENT_PRIORITY_CHANGE	0x10
#define SCE_KERNEL_ERROR_ILLEGAL_THREAD_PARAM_COMBINATION	0x80028026
#define USER_ERROR_UNKNOWN_THREAD_ID	(SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID | SYSCALL_ERROR_BIT)

typedef void (*ThreadmgrPriorityEventHook)(unsigned int event, const uint32_t *values);

extern void notifyThreadPriorityChange(struct ThreadCB *thread, int oldPriority,
				       int newPriority);
extern int ksceGUIDReferObjectWithSubclass(SceUID uid, SceClass *cls, SceObjectBase **object);
extern void ksceKernelPrintfAssertLevel(SceUInt32 level, SceBool condition,
	const SceKernelDebugInfo *info, const void *lr, const char *format, ...);
extern int ksceGUIDGetObject(SceUID uid, SceObjectBase **object);

/* thread leaves the list it is on */
static inline void unlinkThread(struct ThreadCB *thread)
{
	((struct ThreadCB *)thread->next)->prev = thread->prev;
	((struct ThreadCB *)thread->prev)->next = thread->next;
}

/* the SDK version the caller's process was built with */
static inline uint32_t callerSdkVersion(void)
{
	uint32_t version;

	if (currentThreadContext() == NULL ||
	    currentThreadContext()->processId == THREADMGR_KERNEL_PID)
		return SDK_VERSION_KERNEL;
	if (SceSysrootForDriver_CAE820B2(currentThreadContext()->processId, &version) < 0)
		return 0;
	return version;
}

/* a thread's mask, the process default for none (older SDKs only, if old) */
static inline uint32_t effectiveAffinity(struct ThreadCB *thread,
					 uint32_t mask)
{
	return mask != 0 ? mask :
	       thread->process->cpuAffinity;
}

/* the mask the thread reports: its own, else (before SDK 2.10) its process's */
static inline uint32_t reportedAffinity(struct ThreadCB *thread,
					uint32_t version)
{
	uint32_t affinity = thread->currentCpuAffinityMask;

	if (affinity == 0 && version < SDK_VERSION_DEFAULT_AFFINITY)
		affinity = thread->process->cpuAffinity;
	return affinity;
}

/* the error of a failed copy, marked for a syscall */
static inline int copyError(int result)
{
	return result | (ThreadmgrInSyscall() ? SYSCALL_ERROR_BIT : 0);
}

static inline void setPriorityAndAffinity(struct ThreadCB *thread,
					  int priority, uint32_t mask)
{
	thread->basePriority = priority;
	if (priority < thread->boostPriority)
		thread->currentPriority = priority;
	thread->currentCpuAffinityMask = mask & AFFINITY_MASK;
}

/* the running thread should give way to a more urgent one */
#define PREEMPT_SELF(cpu, line) do {						\
	SCE_DBG_ASSERT_LEVEL(0, (cpu) == currentCpuState(), (line));		\
	(cpu)->nextRunningThread = 0;							\
} while (0)

/*
 * changeThreadPriorityAndCpuAffinityMask: thread (its lock held) gets
 * priority and mask together; returns 1 if this CPU should switch.
 */
int changeThreadPriorityAndCpuAffinityMask(struct ThreadCB *thread,
	int priority, uint32_t mask, int *previousPriority, uint32_t *previousMask)
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
	uint32_t state, index, version, affinity;
	SceUID waitId;
	int oldPriority, status = 0, result, cpuId, highest, target;

	SCE_DBG_ASSERT_CHECK(context->lock != 0, 6457);
	SCE_DBG_ASSERT_CHECK(ThreadmgrIntrDisabled(), 6460);
	oldPriority = context->currentPriority;
	if (previousPriority != NULL)
		*previousPriority = oldPriority;
	version = SDK_VERSION_KERNEL;
	if (currentThreadContext() != NULL &&
	    currentThreadContext()->processId != THREADMGR_KERNEL_PID) {
		if (SceSysrootForDriver_CAE820B2(currentThreadContext()->processId,
					      &version) < 0)
			version = 0;
	}
	if (previousMask != NULL)
		*previousMask = reportedAffinity(context, version);
retry:
	state = context->status;
	if (!(state & THREAD_STATE_QUEUED)) {
		if (!(state & SCE_KERNEL_THREAD_STATUS_WAITING)) {
			setPriorityAndAffinity(context, priority, mask);
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
					SCE_PANIC(SCE_LINE(6651, 6657));
				if (ksceKernelSpinlockLowTrylock(&event->lock) < 0)
					goto retry_wait;
				state = event->attr;
				setPriorityAndAffinity(context, priority, mask);
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
								     6690);
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
			SCE_DBG_ASSERT_PRINTF(context->delayId != -1, 6731,
				"*** error: sceGUIDReferObjectWithSubclass(0x%08X)=0x%08X, delayId=0x%08X\n",
				waitId, result, context->delayId);
			setPriorityAndAffinity(context, priority, mask);
			goto finish;
		}
		if (ksceKernelSpinlockLowTrylock(&waitable->lock) < 0)
			goto retry_wait;
		state = waitable->attributes;
		setPriorityAndAffinity(context, priority, mask);
		if (state & WAIT_ATTR_PRIORITY) {
			unlinkThread(context);
			insertPriorityQueue((uintptr_t)&waitable->next, thread);
		}
		ksceKernelSpinlockLowUnlock(THREAD_LOCK(context));
		status = revalidateWaitState((uintptr_t)&waitable->next);
		ksceKernelSpinlockLowUnlock(&waitable->lock);
		ksceGUIDReleaseObject(waitId);
		ksceKernelSpinlockLowLock(THREAD_LOCK(context));
		goto finish;
	}

	/* queued: on another CPU, that CPU does it */
	cpuId = context->currentCpuId;
	if (cpuId != (int)currentCpuId && cpuId != CPU_ID_GLOBAL)
		goto remote;
	if (!((1u << (currentCpuId & 0xff)) & AFFINITY_MASK & effectiveAffinity(context, mask))) {
		/* this CPU may no longer run it */
		if (state & SCE_KERNEL_THREAD_STATUS_RUNNING) {
			if (currentThreadContext() != NULL)
				goto remote;
			setPriorityAndAffinity(context, priority, mask);
			if (cpu->disabledDispatch)
				goto finish;
			runningThreadPreemptByIntr(thread, PREEMPT_INTR);
		} else {
			if (state & SCE_KERNEL_THREAD_STATUS_READY) {
				removeReady(thread, context->currentPriority);
			} else {
				ksceKernelSpinlockLowLock(&cpu->standbyLock);
				unlinkThread(context);
				ksceKernelSpinlockLowUnlock(&cpu->standbyLock);
			}
			setPriorityAndAffinity(context, priority, mask);
		}
		goto distribute;
	}
	context->currentCpuAffinityMask = mask & AFFINITY_MASK;
	context->basePriority = priority;
	if (priority >= context->boostPriority)
		goto finish;
	if (state & SCE_KERNEL_THREAD_STATUS_RUNNING) {
		state = cpu->disabledDispatch;
		context->currentPriority = priority;
		cpu->priority = priority;
		if (!state && (int)searchHighestPriority() < priority) {
			PREEMPT_SELF(cpu, 6507);
			status = 1;
		}
	} else if (state & SCE_KERNEL_THREAD_STATUS_READY) {
		removeReady(thread, context->currentPriority);
		context->currentPriority = priority;
		addReady(thread);
		if ((uint32_t)context->currentPriority - PRIORITY_SHARED_LOW < PRIORITY_SHARED_RANGE) {
			struct ThreadMgrCpuCB *targetCpu = s_threadmgrRuntime.manager->cpuCB;

			/* the CPU running the least urgent thread reschedules */
			highest = 1;
			target = -1;
			for (index = 0; index < CPU_COUNT; index++, targetCpu++) {
				int runningPriority = targetCpu->priority;

				if (highest < runningPriority) {
					highest = runningPriority;
					target = index;
				}
			}
			if (priority < highest) {
				if (target == (int)currentCpuId) {
					PREEMPT_SELF(cpu, 6527);
					status = 1;
				} else {
					addIntrRequest(target, INTR_MASK_SCHEDULING);
				}
			}
		} else if (priority < cpu->priority) {
			PREEMPT_SELF(cpu, 6536);
			status = 1;
		}
	} else {
		context->currentPriority = priority;
		if ((uint32_t)priority - PRIORITY_SHARED_LOW < PRIORITY_SHARED_RANGE) {
			ksceKernelSpinlockLowLock(&cpu->standbyLock);
			unlinkThread(context);
			ksceKernelSpinlockLowUnlock(&cpu->standbyLock);
			goto distribute;
		} else if (priority < cpu->priority) {
			PREEMPT_SELF(cpu, 6555);
			status = 1;
		}
	}
finish:
	if (oldPriority > 0) {
		if ((int32_t)context->currentAttr < 0) {
			/* a user thread's TLS reports both */
			affinity = reportedAffinity(context, version) << 16;
			result = ksceKernelCopyToUserProc(context->processId,
				(void *)(context->tls - USER_TLS_PRIORITY),
				(void *)&context->currentPriority, sizeof(int));
			if (result < 0)
				status = copyError(result);
			result = ksceKernelCopyToUserProc(context->processId,
				(void *)(context->tls - USER_TLS_AFFINITY), &affinity,
				sizeof(affinity));
			if (result < 0)
				status = copyError(result);
		}
		if (status >= 0) {
			hook = (ThreadmgrPriorityEventHook)s_threadmgrRuntime.manager->paHooks[1];
			if (hook != NULL) {
				uint32_t values[] = { context->processId, context->threadId,
						      oldPriority, context->currentPriority };

				hook(DEBUG_EVENT_PRIORITY_CHANGE, values);
			}
			notifyThreadPriorityChange(thread, oldPriority, priority);
		}
	}
	return status;
retry_wait:
	ksceKernelSpinlockLowUnlock(THREAD_LOCK(context));
	ksceGUIDReleaseObject(waitId);
	ksceKernelSpinlockLowLock(THREAD_LOCK(context));
	goto retry;
remote:
	issueChangeThreadPriorityAndCpuAffinityIntrRequest(cpuId, context->threadId, priority, mask);
	return 0;
distribute:
	status = distributeThread(selectExecCpu(thread), thread);
	goto finish;
}

/* SceThreadmgrSGI: another CPU asks this one to change threadId */
void changeThreadPriorityAndCpuAffinityMaskSGIHandler(SceUID threadId, int priority,
						    uint32_t mask)
{
	struct ThreadmgrThreadObject *object;

	if (currentThreadContext() != NULL)
		SCE_PANIC(SCE_LINE(6823, 6829));
	if (ksceGUIDReferObjectWithClass(threadId,
	    s_threadmgrRuntime.manager->threadClass, (SceObjectBase **)&object) >= 0) {
		ksceKernelSpinlockLowLock(THREAD_LOCK(&object->context));
		changeThreadPriorityAndCpuAffinityMask(&object->context, priority, mask, NULL, NULL);
		ksceKernelSpinlockLowUnlock(THREAD_LOCK(&object->context));
		ksceGUIDReleaseObject(threadId);
	}
}

/* changeThreadCpuAffinityMask: threadId's mask; previousMask gets the old one */
int changeThreadCpuAffinityMask(SceUID threadId, uint32_t mask, uint32_t *previousMask)
{
	unsigned int cpuId = ksceKernelCpuId();
	struct ThreadMgrCpuCB *cpu = currentCpuState();
	struct ThreadCB *thread;
	struct ThreadmgrThreadObject *object;
	ProcessBudget *type;
	uint32_t version, state, affinity;
	int status, result, count;

	SCE_DBG_ASSERT_CHECK(ThreadmgrIntrDisabled(), 5530);
	if (ksceGUIDReferObjectWithClass(threadId,
	    s_threadmgrRuntime.manager->threadClass, (SceObjectBase **)&object) < 0)
		return USER_ERROR_UNKNOWN_THREAD_ID;
	thread = &object->context;
	if (mask == AFFINITY_CURRENT_CPU) {
		mask = (1u << (cpuId & 0xff)) & AFFINITY_MASK;
	} else {
		status = checkCpuAffinityMaskPhy(mask, thread->currentAttr, thread->process);
		if (status < 0)
			goto release;
	}
	ksceKernelSpinlockLowLock(THREAD_LOCK(thread));
	version = callerSdkVersion();
	if (version < SDK_VERSION_AFFINITY_CHECK) {
		/* older SDKs: the mask must suit the priority */
		if (!s_threadmgrRuntime.manager->testMode &&
		    (uint32_t)thread->currentPriority - PRIORITY_SHARED_LOW < PRIORITY_SHARED_RANGE &&
		    mask != 0 &&
		    ksceGUIDGetObject(thread->process->budgetId,
				      (SceObjectBase **)&type) >= 0 &&
		    (sceKernelGetDefaultCpuAffinityMask(type) & mask) !=
			 sceKernelGetDefaultCpuAffinityMask(type)) {
			ksceKernelPrintfWithInfo(0, SCE_DBGINFO(SCE_LINE(5562, 5568)),
				"*** error: illegal CPU affinity mask change. threadId=0x%08X[%s]"
				"(priority=%d), 0x%08X -> 0x%08X\n",
				threadId, ksceGUIDName(thread->threadId), thread->currentPriority,
				thread->currentCpuAffinityMask, mask);
			status = SCE_KERNEL_ERROR_ILLEGAL_THREAD_PARAM_COMBINATION;
			goto unlock;
		}
		if (version < SDK_VERSION_AFFINITY_CHECK && !s_threadmgrRuntime.manager->testMode &&
		    (uint32_t)thread->currentPriority - PRIORITY_SHARED_LOW >= PRIORITY_SHARED_RANGE &&
		    mask != 0) {
			count = (mask & 1) + ((mask >> 1) & 1) + ((mask >> 2) & 1) + ((mask >> 3) & 1);
			if (count > 1) {
				ksceKernelPrintfWithInfo(0, SCE_DBGINFO(SCE_LINE(5582, 5588)),
					"*** error: illegal CPU affinity mask change. threadId=0x%08X[%s]"
					"(priority=%d), 0x%08X -> 0x%08X\n",
					threadId, ksceGUIDName(thread->threadId),
					thread->currentPriority, thread->currentCpuAffinityMask, mask);
				status = SCE_KERNEL_ERROR_ILLEGAL_THREAD_PARAM_COMBINATION;
				goto unlock;
			}
		}
	}
	if (previousMask != NULL)
		*previousMask = reportedAffinity(thread, version);
	state = thread->status;
	status = 0;
	if (!(state & THREAD_STATE_QUEUED)) {
		thread->currentCpuAffinityMask = mask & AFFINITY_MASK;
		if ((state & SCE_KERNEL_THREAD_STATUS_STAGNANT) &&
		    thread->process->threadsStopped == 0 &&
		    (effectiveAffinity(thread, mask) & s_threadmgrRuntime.manager->activeCpuMask)) {
			/* a CPU that may run it is active again */
			ksceKernelSpinlockLowLock(
				&s_threadmgrRuntime.manager->threadMgrCBLock);
			unlinkThread(thread);
			ksceKernelSpinlockLowUnlock(
				&s_threadmgrRuntime.manager->threadMgrCBLock);
			goto distribute;
		}
	} else {
		if (thread->currentCpuId != (int)cpuId && thread->currentCpuId != CPU_ID_GLOBAL) {
			issueChangeThreadCpuAffinityIntrRequest(thread->currentCpuId, threadId, mask);
			goto unlock;
		}
		if ((1u << (cpuId & 0xff)) & AFFINITY_MASK & effectiveAffinity(thread, mask)) {
			thread->currentCpuAffinityMask = mask & AFFINITY_MASK;
		} else if (state & SCE_KERNEL_THREAD_STATUS_RUNNING) {
			if (currentThreadContext() != NULL) {
				issueChangeThreadCpuAffinityIntrRequest(cpuId, threadId, mask);
				goto unlock;
			}
			thread->currentCpuAffinityMask = mask & AFFINITY_MASK;
			if (!cpu->disabledDispatch) {
				runningThreadPreemptByIntr((struct ThreadCB *)thread,
							   PREEMPT_INTR);
				goto distribute;
			}
		} else {
			if (state & SCE_KERNEL_THREAD_STATUS_READY) {
				removeReady((struct ThreadCB *)thread,
					    thread->currentPriority);
			} else {
				ksceKernelSpinlockLowLock(&cpu->standbyLock);
				unlinkThread(thread);
				ksceKernelSpinlockLowUnlock(&cpu->standbyLock);
			}
			thread->currentCpuAffinityMask = mask & AFFINITY_MASK;
			goto distribute;
		}
	}
copy:
	if ((int)thread->currentAttr < 0) {
		/* a user thread's TLS reports its mask */
		affinity = reportedAffinity(thread, version) << 16;
		result = ksceKernelCopyToUserProc(thread->processId,
			(void *)(thread->tls - USER_TLS_AFFINITY), &affinity, sizeof(affinity));
		if (result < 0 && copyError(result) < 0)
			status = 0;
	}
unlock:
	ksceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
release:
	ksceGUIDReleaseObject(threadId);
	return status;
distribute:
	status = distributeThread(selectExecCpu((struct ThreadCB *)thread),
				  (struct ThreadCB *)thread) == 1;
	goto copy;
}
