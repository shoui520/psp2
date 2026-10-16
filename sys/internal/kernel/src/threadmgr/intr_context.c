/*
 * SceKernelThreadMgr (dispatch.c): switching threads at the end of an
 * interrupt. intrmgr asks setupNextContext_intr for the context to return
 * to: the running thread's, or the next thread's after the running one was
 * preempted. A user thread found to have overflowed its stack is
 * debug-suspended at once.
 */

#include "private.h"
#include "affinity.h"
#include "fast_mutex.h"
#include "priority_change.h"
#include "dispatch.h"
#include "scheduler.h"
#include "sce/cpu.h"
#include "sce/debug.h"
#include "sce/intrmgr.h"
#include "sce/sysmem.h"
#include "syscall_context.h"

#undef __FILE_NAME__
#define __FILE_NAME__ "dispatch.c"

#define FAST_MUTEX_MAGIC		0xa35db473
#define DEBUG_KIND_STACK_OVERFLOW	4
#define DEBUG_EVENT_SUSPEND		1
#define SWITCH_HOOK_INTR		1
#define SWITCH_HOOK_ARGC		5
#define SCHED_TRACE_OFF_CPU		2
#define SCHED_TRACE_ON_CPU		0

#define OVERFLOW_PANIC			0
#define OVERFLOW_SUSPEND_PROCESS	1
#define OVERFLOW_IGNORE			2

typedef void (*ThreadmgrProbe)(const struct ThreadmgrSchedProbeDescriptor *probe, ...);

extern void ksceKernelPrintfAssertLevel(SceUInt32 level, SceBool condition,
	const SceKernelDebugInfo *info, const void *lr, const char *format, ...);
extern int ksceKernelSetEventFlag(SceUID uid, uint32_t pattern);

#define SCHED_PROBE(symbol, providerName, probeName)				\
	static uint32_t symbol##ProbeId;						\
	volatile uintptr_t symbol##Handler;					\
	extern const char symbol##CodeSite[];					\
	const struct ThreadmgrSchedProbeDescriptor symbol##Descriptor = {	\
		.probeId = &symbol##ProbeId, .provider = providerName, .name = probeName, \
		.codeSite = symbol##CodeSite, .handler = &symbol##Handler,	\
	}

SCHED_PROBE(s_intrSchedRemainCpu, "__sched_", "remain__cpu");
SCHED_PROBE(s_intrSchedOnCpu, "__sched_", "on__cpu");
SCHED_PROBE(s_intrProcStart, "__proc_", "start");
SCHED_PROBE(s_intrProcLwpStart, "__proc_", "lwp__start");

/* setupNextContext_intr (SceThreadmgrForKernel_FEF07EC0): the register context to return to */
struct ThreadRegisterContext *setupNextContext_intr(uintptr_t unused, uint32_t reason)
{
	unsigned int cpuId = SceCpuId();
	struct ThreadMgrCpuCB *cpu = currentCpuState();
	struct ThreadCB *thread, *selected;
	struct ThreadmgrFastMutex *mutex;
	volatile SceDtraceThreadCB *dtrace;
	SceProcessObject *process;
	ThreadmgrDebugHandler overflowHook;
	ThreadmgrDebugHandler overflowHandler;
	ThreadmgrTraceHook hook;
	ThreadmgrOnCpuHook onCpu;
	ThreadmgrProbe probe;
	int initialPriority = cpu->priority, status;
	uint64_t ran;
	bool overflow = false;
	SceUID id;

	SCE_DBG_ASSERT_CHECK(ThreadmgrIntrDisabled(), 970);
	thread = (struct ThreadCB *)cpu->runningThread;
	if (thread != NULL)
		SCE_DBG_ASSERT_PRINTF(thread->currentCpuId == (int)cpuId &&
				      (thread->status & SCE_KERNEL_THREAD_STATUS_RUNNING), 980,
			" k(%d): setupNextContext_intr(), pRunningThreadCB: threadId=0x%08X, "
			"currentCpuId=0x%08X, status=0x%08X\n",
			cpuId, thread->threadId, thread->currentCpuId, thread->status);
	moveStandbyToReady();
	selected = (struct ThreadCB *)cpu->nextRunningThread;
	if (selected == NULL) {
		findNextRunningThread((struct ThreadCB *)cpu->runningThread);
		selected = (struct ThreadCB *)cpu->nextRunningThread;
	}
	thread = (struct ThreadCB *)cpu->runningThread;
	if (thread == selected) {
		cpu->remainCpuCount++;
		__asm__ volatile("mcr p15, 0, %0, c13, c0, 4" : : "r"(thread) : "memory");
		/* the DTrace probe site __sched_:remain__cpu */
		__asm__ volatile(".global s_intrSchedRemainCpuCodeSite\ns_intrSchedRemainCpuCodeSite:");
		probe = (ThreadmgrProbe)s_intrSchedRemainCpuHandler;
		if (probe != NULL)
			probe(&s_intrSchedRemainCpuDescriptor);
		goto deferred;
	}

	/* the running thread is preempted */
	if (thread != NULL) {
		ksceKernelSpinlockLowLock(THREAD_LOCK(thread));
		thread = (struct ThreadCB *)cpu->runningThread;
		for (mutex = (struct ThreadmgrFastMutex *)thread->ownedFastMutexNext;
		     mutex != (struct ThreadmgrFastMutex *)&thread->ownedFastMutexNext;
		     mutex = (struct ThreadmgrFastMutex *)mutex->next) {
			SCE_DBG_ASSERT_CHECK(mutex->magic == FAST_MUTEX_MAGIC, 1007);
			ksceKernelSpinlockLowUnlock(&mutex->lock);
			thread = (struct ThreadCB *)cpu->runningThread;
		}
		ran = thread->runClock;
		thread->runClock = ran - cpu->lastThreadSwitchTime + SceSysrootForKernel_E20F6FC8();
		thread = (struct ThreadCB *)cpu->runningThread;
		thread->intrPreemptCount++;
		if (s_threadmgrRuntime.manager->paHooks[0] != 0)
			issueCallPAScheduleHookIntrRequest(SceCpuId(), thread->processId, thread->threadId,
				     SCHED_TRACE_OFF_CPU, thread->currentPriority);
		notifyOffCpuIntr(reason);
		thread = (struct ThreadCB *)cpu->runningThread;
		if ((int)thread->currentAttr < 0 && !(thread->flags & THREAD_FLAG_EXITING)) {
			status = checkThreadUserStack((struct ThreadCB *)thread);
			thread = (struct ThreadCB *)cpu->runningThread;
			if (status == 0) {
				overflow = true;
				SCE_DBG_PRINTF(0, 1034, " k: setupNextContext_intr(), User Stack Overflow "
					       "was detected. (threadId=0x%08X)\n", thread->threadId);
				thread = (struct ThreadCB *)cpu->runningThread;
				thread->debugSuspendReason |= DEBUG_SUSPEND_USER_STACK;
				if (thread->userFrame == NULL) {
					/* not in a syscall: it is debug-suspended now */
					thread->currentCpuId = -1;
					thread->status = ((thread->status & 0x7fff) << 16) |
						THREAD_STATE_DEBUG_SUSPENDED | THREAD_STATE_OFF_CPU;
					ksceKernelSpinlockLowLock(&s_threadmgrRuntime.manager->threadMgrCBLock);
					thread = (struct ThreadCB *)cpu->runningThread;
					thread->next = (uintptr_t)
						&s_threadmgrRuntime.manager->debugSuspendedNext;
					thread->prev = s_threadmgrRuntime.manager->debugSuspendedPrev;
					s_threadmgrRuntime.manager->debugSuspendedPrev = (uintptr_t)thread;
					*(uintptr_t *)thread->prev = (uintptr_t)thread;
					ksceKernelSpinlockLowUnlock(&s_threadmgrRuntime.manager->threadMgrCBLock);
					thread = (struct ThreadCB *)cpu->runningThread;
					ksceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
					goto overflowed;
				}
				thread->status |= THREAD_STATE_DEBUG_SUSPENDED;
			}
		}
		distributePreemptedThread(cpu, thread, cpuId, __builtin_return_address(0));
		if (overflow) {
overflowed:
			/* the debugger decides about the overflow */
			overflowHook = s_threadmgrRuntime.manager->
				debugHandlers[0][DEBUG_HANDLER_STACK_OVERFLOW];
			if (overflowHook != NULL)
				overflowHook(0, NULL);
			overflowHandler = s_threadmgrRuntime.manager->
				debugHandlers[1][DEBUG_HANDLER_STACK_OVERFLOW];
			status = overflowHandler != NULL ? overflowHandler(0, NULL) :
							   OVERFLOW_SUSPEND_PROCESS;
			if (status == OVERFLOW_SUSPEND_PROCESS) {
				thread = (struct ThreadCB *)cpu->runningThread;
				process = thread->process;
				if (!ksceKernelAtomicCompareAndSet32((SceInt32 *)&process->abortRequested,
								     0, 1)) {
					thread = (struct ThreadCB *)cpu->runningThread;
					process->abortCode = DEBUG_KIND_STACK_OVERFLOW;
					process->abortThread = thread->threadId;
					status = ksceKernelSetEventFlag(process->abortEventFlag,
									DEBUG_EVENT_SUSPEND);
					if (status < 0) {
						SCE_DBG_PRINTF(0, 1103, "*** error: debugSuspendThreadSelf() "
							"-> sceKernelSetEventFlag(0x%08X)=0x%08X\n",
							process->abortEventFlag, status);
						SCE_PANIC(1104);
					}
				}
			} else if (status != OVERFLOW_IGNORE) {
				if (status != OVERFLOW_PANIC)
					SCE_PANIC(1113);
				SCE_PANIC(1091);
			}
		}
		selected = (struct ThreadCB *)cpu->nextRunningThread;
	}

	/* the selected thread takes the CPU */
	ksceKernelSpinlockLowLock(THREAD_LOCK(selected));
	for (mutex = (struct ThreadmgrFastMutex *)selected->ownedFastMutexNext;
	     mutex != (struct ThreadmgrFastMutex *)&selected->ownedFastMutexNext;
	     mutex = (struct ThreadmgrFastMutex *)mutex->next) {
		SCE_DBG_ASSERT_CHECK(mutex->magic == FAST_MUTEX_MAGIC, 1127);
		ksceKernelSpinlockLowLock(&mutex->lock);
	}
	ksceKernelSpinlockLowUnlock(THREAD_LOCK(selected));
	cpu->threadSwitchCount++;
	cpu->lastThreadSwitchTime = SceSysrootForKernel_E20F6FC8();
	if (s_threadmgrRuntime.manager->paHooks[0] != 0)
		issueCallPAScheduleHookIntrRequest(SceCpuId(), selected->processId, selected->threadId,
			     SCHED_TRACE_ON_CPU, selected->currentPriority);
	hook = s_threadmgrRuntime.switchHook;
	if (hook != NULL)
		hook(SWITCH_HOOK_INTR, SWITCH_HOOK_ARGC, cpu->runningThreadId, initialPriority,
		     selected->threadId, selected->currentPriority, reason);
	cpu->runningThread = (uintptr_t)selected;
	cpu->runningProcessId = selected->processId;
	cpu->runningThreadId = selected->threadId;
	__asm__ volatile("mcr p15, 0, %0, c13, c0, 4" : : "r"(selected) : "memory");
	dtrace = selected->dtraceState;
	if (dtrace != NULL && (dtrace->flags & DTRACE_THREAD_START)) {
		process = selected->process;
		if (process->dtraceFlags & DTRACE_PROCESS_START) {
			/* the DTrace probe site __proc_:start */
			__asm__ volatile(".global s_intrProcStartCodeSite\ns_intrProcStartCodeSite:");
			probe = (ThreadmgrProbe)s_intrProcStartHandler;
			if (probe != NULL) {
				probe(&s_intrProcStartDescriptor);
				process = selected->process;
			}
			process->dtraceFlags &= ~DTRACE_PROCESS_START;
		}
		id = selected->userThreadId;
		if (id < 0)
			id = selected->threadId;
		/* the DTrace probe site __proc_:lwp__start */
		__asm__ volatile(".global s_intrProcLwpStartCodeSite\ns_intrProcLwpStartCodeSite:");
		probe = (ThreadmgrProbe)s_intrProcLwpStartHandler;
		if (probe != NULL)
			probe(&s_intrProcLwpStartDescriptor, id, process->pid);
		dtrace->flags &= ~DTRACE_THREAD_START;
	}
	/* the DTrace probe site __sched_:on__cpu */
	__asm__ volatile(".global s_intrSchedOnCpuCodeSite\ns_intrSchedOnCpuCodeSite:");
	probe = (ThreadmgrProbe)s_intrSchedOnCpuHandler;
	if (probe != NULL)
		probe(&s_intrSchedOnCpuDescriptor);
	onCpu = s_threadmgrRuntime.onCpuHook;
	if (onCpu != NULL) {
		id = selected->userThreadId;
		if (id < 0)
			id = selected->threadId;
		onCpu(selected->process->pid, id,
		      selected->currentPriority, selected->currentCpuAffinityMask);
	}
	thread = selected;
deferred:
	if (initialPriority < cpu->priority) {
		moveStandbyToReady();
		if (cpu->nextRunningThread == 0)
			addIntrRequest(cpuId, INTR_MASK_SCHEDULING);
	}
	return thread->pRegisterContext;
}
