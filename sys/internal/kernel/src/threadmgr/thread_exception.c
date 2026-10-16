/*
 * SceKernelThreadMgr (threadmgr.c): CPU exceptions of threads. The
 * exception handlers (excp_handlers.S) save the faulting thread's registers
 * and call these. A user thread whose process takes exceptions has the
 * exception handed to its process's exception object (the VM's monitor
 * waits there) and is suspended; anything else is debug-suspended.
 */

#include <string.h>
#include "private.h"
#include "affinity.h"
#include "sce/cpu.h"
#include "sce/debug.h"
#include "sce/intrmgr.h"
#include "syscall_context.h"

#undef __FILE_NAME__
#define __FILE_NAME__ "threadmgr.c"

/* what the handler is told about the exception */
#define EXCP_KIND_PREFETCH_ABORT	0x100
#define EXCP_KIND_DATA_ABORT		0x200
#define EXCP_KIND_UNDEFINED		0x400

/* what the exception handler does next */
#define EXCP_ACTION_DEFAULT		1	/* PLACEHOLDER: names; the excpmgr chain's codes */
#define EXCP_ACTION_KERNEL		3
#define EXCP_ACTION_SUSPENDED		4

/* the kinds the VM is told of (ThreadmgrExceptionInfo.kind) */
#define VM_EXCEPTION_DATA_ABORT		0x001
#define VM_EXCEPTION_PREFETCH_ABORT	0x100
#define VM_EXCEPTION_DEBUG		0x200

/* thread->handedExceptions: the kinds this thread hands over (SceKernelThreadOptParam data_0x18) */
#define THREAD_EXCP_DATA_ABORT		0x001
#define THREAD_EXCP_PREFETCH_ABORT	0x100
#define THREAD_EXCP_DEBUG		0x200

#define CPSR_MODE_MASK			0x1f
#define CPSR_MODE_USR			0x10
#define CPSR_MODE_SVC			0x13
#define FSR_STATUS_MASK			0x140f
#define DFSR_FIRST			5	/* translation, domain and permission faults */
#define DFSR_COUNT			11
#define DFSR_HANDED			0x555
#define IFSR_FIRST			2	/* a debug event, then as for data */
#define IFSR_COUNT			14
#define IFSR_HANDED			0x2aa8
#define IFSR_DEBUG			0x0001

#define THREAD_ATTR_EXCEPTION_LIST	0x04000000	/* PLACEHOLDER: name; its process takes its exceptions */
#define THREAD_ATTR_IN_SUSPENDED	0x00800000	/* PLACEHOLDER: name */
#define WAIT_TYPE_THREAD_SUSPEND	0x00000008
#define PREEMPT_REASON_EXCEPTION	8	/* PLACEHOLDER: name */

#define CPU_STOP_REQUEST		4	/* the other CPUs wait while it is set */
#define CPU_STOP_ALL_STOPPED		3	/* each of the other CPUs counted itself in */
#define SGI_STOP_CPUS			7
#define CPU_MASK_ALL			0xf
#define PROCESS_DEBUG_SUSPEND_EXCEPTION	0x11	/* PLACEHOLDER: SceSysrootForKernel_1247A825's reason */
#define PROCESS_DEBUG_SUSPEND_CPU	0	/* the processDebugSuspendHook kind */
#define SCE_KERNEL_ERROR_NOT_DEBUG_SUSPENDED	0x80028030	/* PLACEHOLDER: name */

typedef void (*ThreadmgrProcessDebugSuspendHook)(SceUID pid, SceUID threadId, int kind,
						 uintptr_t arg);

extern void ksceKernelPrintfAssertLevel(SceUInt32 level, SceBool condition,
	const SceKernelDebugInfo *info, const void *lr, const char *format, ...);
extern int SceSysrootForKernel_1247A825(SceUID pid, uint32_t reason);
extern int SceSysrootForDriver_DABD4A47(ThreadmgrThreadInfoForDebugger *info);
int ksceKernelIsThreadDebugSuspended(SceUID threadId);
int ksceKernelDebugResumeThread(SceUID threadId, uint32_t reason);
int ksceKernelGetThreadInfoForDebugger(SceUID threadId, uint32_t flags,
				       ThreadmgrThreadInfoForDebugger *info);
bool wakeupAndDistributeThread(struct ThreadCB *thread, uintptr_t waitable,
			       int result);

/*
 * threadMgrExcpHandlerCfunc_0: an abort or undefined instruction of the
 * running thread (excpKind EXCP_KIND_*), its registers saved. Returns the
 * handler's next step.
 */
int threadMgrExcpHandlerCfunc_0(struct ThreadCB *thread, uint32_t excpKind)
{
	struct ThreadmgrThreadObject *object = THREAD_OBJECT(thread);
	struct ThreadCB *waiter, *node, *next;
	struct ThreadmgrExceptionEntry *entry;
	struct ThreadmgrException *exception;
	ThreadmgrExceptionInfo *info;
	uint32_t kind, status, far;
	int action;
	SceUID uid;

	SCE_DBG_ASSERT_CHECK(ThreadmgrIntrDisabled(), 10817);
	SCE_DBG_ASSERT_CHECK(thread != NULL, 10819);
	ksceKernelSpinlockLowLock(THREAD_LOCK(thread));
	SCE_DBG_ASSERT_CHECK(thread->threadId != currentCpuState()->idleThread->threadId, 10825);
	SCE_DBG_ASSERT_PRINTF((thread->status &
			       (SCE_KERNEL_THREAD_STATUS_RUNNING | THREAD_STATE_OFF_CPU)) != 0,
			      10832, "status=0x%08X\n", thread->status);

	/* is it the process's to handle? */
	if ((thread->pRegisterContext->cpsr & CPSR_MODE_MASK) != CPSR_MODE_USR) {
		action = (thread->pRegisterContext->cpsr & CPSR_MODE_MASK) == CPSR_MODE_SVC ?
			EXCP_ACTION_DEFAULT : EXCP_ACTION_KERNEL;
		goto suspend;
	}
	action = EXCP_ACTION_DEFAULT;
	if ((thread->currentAttr & THREAD_ATTR_IN_SUSPENDED) ||
	    !(thread->currentAttr & THREAD_ATTR_EXCEPTION_LIST))
		goto suspend;
	switch (excpKind) {
	case EXCP_KIND_DATA_ABORT:
		status = (thread->pRegisterContext->dfsr & FSR_STATUS_MASK) - DFSR_FIRST;
		if (status < DFSR_COUNT && ((1 << status) & DFSR_HANDED) &&
		    (thread->handedExceptions & THREAD_EXCP_DATA_ABORT)) {
			kind = VM_EXCEPTION_DATA_ABORT;
			goto handed;
		}
		goto suspend;
	case EXCP_KIND_UNDEFINED:
		goto suspend;
	case EXCP_KIND_PREFETCH_ABORT:
		break;
	default:
		SCE_PANIC(SCE_LINE(10897, 10902));
	}
	status = (thread->pRegisterContext->ifsr & FSR_STATUS_MASK) - IFSR_FIRST;
	if (status >= IFSR_COUNT)
		goto suspend;
	if ((1 << status) & IFSR_HANDED) {
		if (!(thread->handedExceptions & THREAD_EXCP_PREFETCH_ABORT))
			goto suspend;
		kind = VM_EXCEPTION_PREFETCH_ABORT;
	} else if (((1 << status) & IFSR_DEBUG) && (thread->handedExceptions & THREAD_EXCP_DEBUG)) {
		kind = VM_EXCEPTION_DEBUG;
	} else {
		goto suspend;
	}
handed:
	runningThreadPreemptByIntr(thread, PREEMPT_REASON_EXCEPTION);
	if (!(thread->currentAttr & THREAD_ATTR_IN_SUSPENDED)) {
		ksceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
		goto deliver;
	}
	action = EXCP_ACTION_SUSPENDED;
	goto debug_suspend;
suspend:
	kind = 0;
	runningThreadPreemptByIntr(thread, PREEMPT_REASON_EXCEPTION);
debug_suspend:
	if (thread->debugSuspendReason == 0) {
		thread->debugSuspendReason = excpKind;
		thread->currentCpuId = -1;
		thread->status = ((thread->status & 0x7fff) << 16) |
				     THREAD_STATE_DEBUG_SUSPENDED | THREAD_STATE_OFF_CPU;
		ksceKernelSpinlockLowLock(
			&s_threadmgrRuntime.manager->threadMgrCBLock);
		thread->next = (uintptr_t)&s_threadmgrRuntime.manager->debugSuspendedNext;
		thread->prev = s_threadmgrRuntime.manager->debugSuspendedPrev;
		s_threadmgrRuntime.manager->debugSuspendedPrev = (uintptr_t)thread;
		*(uintptr_t *)thread->prev = (uintptr_t)thread;
		ksceKernelSpinlockLowUnlock(
			&s_threadmgrRuntime.manager->threadMgrCBLock);
	}
	ksceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
	if (action != EXCP_ACTION_SUSPENDED)
		return action;

deliver:
	if (thread->currentAttr & THREAD_ATTR_IN_SUSPENDED)
		return EXCP_ACTION_SUSPENDED;
	/* VM-suspended, the exception goes to the process's exception object */
	ksceKernelSpinlockLowLock(&thread->process->exception->lock);
	ksceKernelSpinlockLowLock(THREAD_LOCK(thread));
	uid = thread->userThreadId;
	if (thread->debugSuspendReason != 0) {
		thread->status &= THREAD_STATE_KEEP_SUSPEND;
		ksceKernelSpinlockLowLock(
			&s_threadmgrRuntime.manager->threadMgrCBLock);
		((struct ThreadCB *)thread->next)->prev = thread->prev;
		((struct ThreadCB *)thread->prev)->next = thread->next;
		ksceKernelSpinlockLowUnlock(
			&s_threadmgrRuntime.manager->threadMgrCBLock);
	}
	SCE_DBG_ASSERT_CHECK(thread->currentCpuId == ksceKernelCpuId(), 10967);
	thread->currentCpuId = -1;
	thread->vmSuspendCount++;
	thread->status = (thread->status & ~SCE_KERNEL_THREAD_STATUS_MASK) |
			     SCE_KERNEL_THREAD_STATUS_SUSPENDED;
	ksceKernelSpinlockLowLock(&s_threadmgrRuntime.manager->threadMgrCBLock);
	thread->next = (uintptr_t)&s_threadmgrRuntime.manager->suspendedNext;
	thread->prev = s_threadmgrRuntime.manager->suspendedPrev;
	s_threadmgrRuntime.manager->suspendedPrev = (uintptr_t)thread;
	*(uintptr_t *)thread->prev = (uintptr_t)thread;
	ksceKernelSpinlockLowUnlock(&s_threadmgrRuntime.manager->threadMgrCBLock);
	exception = thread->process->exception;
	waiter = (struct ThreadCB *)exception->next;
	if (waiter == (struct ThreadCB *)&exception->next ||
	    !(kind & waiter->waitArg0)) {
		/* nobody waits for it: it is left pending */
		entry = thread->exceptionEntry;
		entry->value = uid;
		SCE_DBG_ASSERT_CHECK(kind != 0, 10988);
		entry->kind = kind;
		if (kind == VM_EXCEPTION_DATA_ABORT)
			entry->value2 = thread->pRegisterContext->dfar;
		SCE_DBG_ASSERT_CHECK(!(thread->flags & THREAD_FLAG_EXCEPTION), 10998);
		exception = thread->process->exception;
		entry->prev = exception->pendingPrev;
		entry->next = (struct ThreadmgrExceptionEntry *)&exception->pendingNext;
		exception->pendingPrev = entry;
		entry->prev->next = entry;
		ksceKernelAtomicOrAndGet32((SceInt32 *)&thread->flags, THREAD_FLAG_EXCEPTION);
		ksceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
	} else {
		/* the waiter takes it */
		SCE_DBG_ASSERT_CHECK(waiter->magic == THREAD_CB_MAGIC, 11004);
		far = thread->pRegisterContext->dfar;
		ksceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
		SCE_DBG_ASSERT_CHECK(waiter->magic == THREAD_CB_MAGIC, 11015);
		ksceKernelSpinlockLowLock(THREAD_LOCK(waiter));
		info = (ThreadmgrExceptionInfo *)waiter->waitArg1;
		info->value = uid;
		info->kind = kind;
		if (kind == VM_EXCEPTION_DATA_ABORT)
			info->value2 = far;
		wakeupAndDistributeThread(waiter,
			(uintptr_t)&thread->process->exception->next, 0);
		ksceKernelSpinlockLowUnlock(THREAD_LOCK(waiter));
	}
	ksceKernelSpinlockLowUnlock(&thread->process->exception->lock);

	/* the threads waiting for it to be suspended */
	ksceKernelSpinlockLowLock(&object->waitLock);
	for (node = (struct ThreadCB *)object->waitNext;
	     node != (struct ThreadCB *)&object->waitNext; node = next) {
		next = (struct ThreadCB *)node->next;
		SCE_DBG_ASSERT_CHECK(node->magic == THREAD_CB_MAGIC, 11042);
		ksceKernelSpinlockLowLock(THREAD_LOCK(node));
		if (node->waitType == WAIT_TYPE_THREAD_SUSPEND)
			wakeupAndDistributeThread(node, (uintptr_t)&object->waitNext, 0);
		ksceKernelSpinlockLowUnlock(THREAD_LOCK(node));
	}
	ksceKernelSpinlockLowUnlock(&object->waitLock);
	return EXCP_ACTION_SUSPENDED;
}

/* threadMgrExcpHandlerCfunc_2: the debugger lets the thread go on */
void threadMgrExcpHandlerCfunc_2(SceUID threadId)
{
	int reason, ret;

	reason = ksceKernelIsThreadDebugSuspended(threadId);
	if (reason <= 0)
		return;
	ret = ksceKernelDebugResumeThread(threadId, reason);
	if (ret < 0 && ret != (int)SCE_KERNEL_ERROR_NOT_DEBUG_SUSPENDED)
		SCE_PANIC(SCE_LINE(11066, 11071));
}

/*
 * threadMgrExcpHandlerCfunc_3: the debugger stops the thread's process.
 * The first exception of a process debug-suspends it and reports its
 * threads, the other CPUs held still meanwhile.
 */
void threadMgrExcpHandlerCfunc_3(struct ThreadCB *thread)
{
	struct ThreadCB *other;
	ThreadmgrThreadInfoForDebugger info;
	ThreadmgrProcessDebugSuspendHook hook;
	ListNode *link;
	int stopped, ret;

	SCE_DBG_ASSERT_FAIL(11113);
	if (ksceKernelAtomicCompareAndSet32(
		(SceInt32 *)&thread->process->abortRequested, 0, 1) != 0)
		return;
	stopped = ksceKernelAtomicCompareAndSet32(
		(SceInt32 *)&s_threadmgrRuntime.manager->cpuStopOwner, 0, 1);
	if (stopped == 0) {
		s_threadmgrRuntime.manager->cpuStopState = CPU_STOP_REQUEST;
		ksceKernelTriggerSGI(SGI_STOP_CPUS, 0,
				     ~(1 << ksceKernelCpuId()) & CPU_MASK_ALL);
	}
	ksceKernelCpuResumeIntr(0);
	ret = SceSysrootForKernel_1247A825(thread->processId, PROCESS_DEBUG_SUSPEND_EXCEPTION);
	if (ret < 0)
		SCE_DBG_PRINTF(0xf, 11135, "(): sceKernelProcessDebugSuspend(0x%08X)=0x%08X\n",
			       thread->processId, ret);
	ksceKernelSpinlockLowLockCpuSuspendIntr(
		&thread->process->lock);
	for (link = thread->process->threads.next; link != &thread->process->threads;
	     link = link->next) {
		other = THREAD_FROM_PROCESS_LINK(link);
		memset(&info, 0, sizeof(info));
		info.size = sizeof(info);
		ret = ksceKernelGetThreadInfoForDebugger(other->threadId, 0, &info);
		if (ret < 0) {
			SCE_DBG_ASSERT_FAIL(11148);
			continue;
		}
		ret = SceSysrootForDriver_DABD4A47(&info);
		if (ret < 0)
			SCE_DBG_ASSERT_FAIL(11156);
	}
	ksceKernelSpinlockLowUnlock(&thread->process->lock);
	if (stopped == 0) {
		/* the other CPUs go on once all of them stopped */
		s_threadmgrRuntime.manager->cpuStopState = 0;
		__asm__ volatile("dsb" : : : "memory");
		while (s_threadmgrRuntime.manager->cpuStopState != CPU_STOP_ALL_STOPPED)
			__asm__ volatile("wfe");
		s_threadmgrRuntime.manager->cpuStopState = 0;
		s_threadmgrRuntime.manager->cpuStopOwner = 0;
	}
	hook = (ThreadmgrProcessDebugSuspendHook)s_threadmgrRuntime.manager->processDebugSuspendHook;
	if (ret >= 0 && hook != NULL)
		hook(thread->processId, thread->threadId, PROCESS_DEBUG_SUSPEND_CPU,
		     s_threadmgrRuntime.manager->processDebugSuspendArg);
}
