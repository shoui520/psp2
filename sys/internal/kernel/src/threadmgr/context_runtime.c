/*
 * SceKernelThreadMgr (dispatch.c): switching threads from thread context.
 * switchThread saves the running thread's context and goes to the CPU's
 * switch stack, where switchThread2 picks the next thread
 * (setupNextContext_thread) and restores it. The preempted thread goes
 * back to a ready queue or to the standby list of a CPU it can preempt.
 */

#include "private.h"
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

#define PRIORITY_NONE			0xfe
#define FAST_MUTEX_MAGIC		0xa35db473
#define DEBUG_KIND_STACK_OVERFLOW	4
#define DEBUG_EVENT_SUSPEND		1
#define SWITCH_HOOK_THREAD		0	/* the switch hook's first argument */
#define SWITCH_HOOK_ARGC		5
#define SCHED_TRACE_OFF_CPU		2	/* issueCallPAScheduleHookIntrRequest's kinds */
#define SCHED_TRACE_WAIT		1
#define SCHED_TRACE_ON_CPU		0
#define WAIT_ID_NONE			((SceUID)0x80000000)

/* switchThread's reasons */
#define SWITCH_REASON_LATER		0x00000001	/* from an interrupt: request one */
#define SWITCH_REASON_PREEMPT		0x00000002
#define SWITCH_REASON_WAIT		0x00000004
#define SWITCH_MODE_UNLOCK		0x00000010	/* switchThread2: the old thread's lock is held */

/* what the debugger's stack overflow handler decides */
#define OVERFLOW_PANIC			0
#define OVERFLOW_SUSPEND_PROCESS	1
#define OVERFLOW_IGNORE			2

typedef void (*ThreadmgrProbe)(const struct ThreadmgrSchedProbeDescriptor *probe, ...);

extern void ksceKernelPrintfAssertLevel(SceUInt32 level, SceBool condition,
	const SceKernelDebugInfo *info, const void *lr, const char *format, ...);
extern int SceIntrmgrForKernel_71C5F42F(struct ThreadRegisterContext *context, uintptr_t vfp);
extern uint32_t SceIntrmgrForKernel_08FDB0CE(struct ThreadRegisterContext *context,
					     uint32_t flags, uintptr_t vfp);
extern int ksceKernelSetEventFlag(SceUID uid, uint32_t pattern);
/* context_runtime.S */
extern void callSwitchThread2OnStack(uintptr_t stackTop, uint32_t mode);

#define SCHED_PROBE(symbol, providerName, probeName)				\
	static uint32_t symbol##ProbeId;						\
	volatile uintptr_t symbol##Handler;					\
	extern const char symbol##CodeSite[];					\
	const struct ThreadmgrSchedProbeDescriptor symbol##Descriptor = {	\
		.probeId = &symbol##ProbeId, .provider = providerName, .name = probeName, \
		.codeSite = symbol##CodeSite, .handler = &symbol##Handler,	\
	}

SCHED_PROBE(s_schedRemainCpu, "__sched_", "remain__cpu");
SCHED_PROBE(s_schedOnCpu, "__sched_", "on__cpu");
SCHED_PROBE(s_procStart, "__proc_", "start");
SCHED_PROBE(s_procLwpStart, "__proc_", "lwp__start");

/*
 * Picks the thread cpu runs next, from thread context, and makes it the
 * running one; current was running. Returns the thread to restore.
 */
struct ThreadCB *setupNextContext_thread(struct ThreadCB *current,
	uint32_t reason, SceUID oldThreadId, int oldPriority)
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

	SCE_DBG_ASSERT_CHECK(ThreadmgrIntrDisabled(), 543);
	if (cpu->disabledDispatch) {
		SCE_DBG_ASSERT_LEVEL(0, (reason >> 1) & 1, 548);
		return current;
	}
	thread = (struct ThreadCB *)cpu->runningThread;
	if (thread != NULL)
		SCE_DBG_ASSERT_PRINTF(thread->currentCpuId == (int)cpuId &&
				      (thread->status & SCE_KERNEL_THREAD_STATUS_RUNNING), 556,
			" k(%d): setupNextContext_thread(), pRunningThreadCB: threadId=0x%08X, "
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
		__asm__ volatile(".global s_schedRemainCpuCodeSite\ns_schedRemainCpuCodeSite:");
		probe = (ThreadmgrProbe)s_schedRemainCpuHandler;
		if (probe != NULL)
			probe(&s_schedRemainCpuDescriptor);
		goto notify;
	}

	/* the running thread lets go of the CPU */
	if (thread != NULL) {
		SCE_DBG_ASSERT_LEVEL(0, !(reason & SWITCH_REASON_WAIT), 574);
		ksceKernelSpinlockLowLock(THREAD_LOCK(thread));
		thread = (struct ThreadCB *)cpu->runningThread;
		for (mutex = (struct ThreadmgrFastMutex *)thread->ownedFastMutexNext;
		     mutex != (struct ThreadmgrFastMutex *)&thread->ownedFastMutexNext;
		     mutex = (struct ThreadmgrFastMutex *)mutex->next) {
			SCE_DBG_ASSERT_CHECK(mutex->magic == FAST_MUTEX_MAGIC, 583);
			ksceKernelSpinlockLowUnlock(&mutex->lock);
			thread = (struct ThreadCB *)cpu->runningThread;
		}
		ran = thread->runClock;
		thread->runClock = ran - cpu->lastThreadSwitchTime + SceSysrootForKernel_E20F6FC8();
		thread = (struct ThreadCB *)cpu->runningThread;
		thread->threadPreemptCount++;
		if (s_threadmgrRuntime.manager->paHooks[0] != 0)
			issueCallPAScheduleHookIntrRequest(SceCpuId(), thread->processId, thread->threadId,
				     SCHED_TRACE_OFF_CPU, thread->currentPriority);
		notifyOffCpu(reason);
		thread = (struct ThreadCB *)cpu->runningThread;
		if ((int)thread->currentAttr < 0 && !(thread->flags & THREAD_FLAG_EXITING)) {
			status = checkThreadUserStack((struct ThreadCB *)thread);
			thread = (struct ThreadCB *)cpu->runningThread;
			if (status == 0) {
				overflow = true;
				SCE_DBG_PRINTF(0, 611, " k: setupNextContext_thread(), User Stack Overflow "
					       "was detected. (threadId=0x%08X)\n", thread->threadId);
				thread = (struct ThreadCB *)cpu->runningThread;
				thread->status |= THREAD_STATE_DEBUG_SUSPENDED;
				thread->debugSuspendReason |= DEBUG_SUSPEND_USER_STACK;
			}
		}
		distributePreemptedThread(cpu, thread, cpuId, __builtin_return_address(0));
		if (overflow) {
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
						SCE_DBG_PRINTF(0, 665, "*** error: debugSuspendThreadSelf() "
							"-> sceKernelSetEventFlag(0x%08X)=0x%08X\n",
							process->abortEventFlag, status);
						SCE_PANIC(666);
					}
				}
			} else if (status != OVERFLOW_IGNORE) {
				if (status != OVERFLOW_PANIC)
					SCE_PANIC(675);
				SCE_PANIC(653);
			}
		}
		selected = (struct ThreadCB *)cpu->nextRunningThread;
	}

	/* the selected thread takes it */
	ksceKernelSpinlockLowLock(THREAD_LOCK(selected));
	for (mutex = (struct ThreadmgrFastMutex *)selected->ownedFastMutexNext;
	     mutex != (struct ThreadmgrFastMutex *)&selected->ownedFastMutexNext;
	     mutex = (struct ThreadmgrFastMutex *)mutex->next) {
		SCE_DBG_ASSERT_CHECK(mutex->magic == FAST_MUTEX_MAGIC, 689);
		ksceKernelSpinlockLowLock(&mutex->lock);
	}
	ksceKernelSpinlockLowUnlock(THREAD_LOCK(selected));
	cpu->threadSwitchCount++;
	cpu->lastThreadSwitchTime = SceSysrootForKernel_E20F6FC8();
	if (s_threadmgrRuntime.manager->paHooks[0] != 0)
		issueCallPAScheduleHookIntrRequest(SceCpuId(), selected->processId, selected->threadId,
			     SCHED_TRACE_ON_CPU, selected->currentPriority);
	cpu->runningThread = (uintptr_t)selected;
	cpu->runningProcessId = selected->processId;
	cpu->runningThreadId = selected->threadId;
	__asm__ volatile("mcr p15, 0, %0, c13, c0, 4" : : "r"(selected) : "memory");
	dtrace = selected->dtraceState;
	if (dtrace != NULL && (dtrace->flags & DTRACE_THREAD_START)) {
		process = selected->process;
		if (process->dtraceFlags & DTRACE_PROCESS_START) {
			/* the DTrace probe site __proc_:start */
			__asm__ volatile(".global s_procStartCodeSite\ns_procStartCodeSite:");
			probe = (ThreadmgrProbe)s_procStartHandler;
			if (probe != NULL) {
				probe(&s_procStartDescriptor);
				process = selected->process;
			}
			process->dtraceFlags &= ~DTRACE_PROCESS_START;
		}
		id = selected->userThreadId;
		if (id < 0)
			id = selected->threadId;
		/* the DTrace probe site __proc_:lwp__start */
		__asm__ volatile(".global s_procLwpStartCodeSite\ns_procLwpStartCodeSite:");
		probe = (ThreadmgrProbe)s_procLwpStartHandler;
		if (probe != NULL)
			probe(&s_procLwpStartDescriptor, id, process->pid);
		dtrace->flags &= ~DTRACE_THREAD_START;
	}
	/* the DTrace probe site __sched_:on__cpu */
	__asm__ volatile(".global s_schedOnCpuCodeSite\ns_schedOnCpuCodeSite:");
	probe = (ThreadmgrProbe)s_schedOnCpuHandler;
	if (probe != NULL)
		probe(&s_schedOnCpuDescriptor);
	onCpu = s_threadmgrRuntime.onCpuHook;
	if (onCpu != NULL) {
		id = selected->userThreadId;
		if (id < 0)
			id = selected->threadId;
		onCpu(selected->process->pid, id,
		      selected->currentPriority, selected->currentCpuAffinityMask);
	}
	thread = selected;
notify:
	hook = s_threadmgrRuntime.switchHook;
	if (hook != NULL)
		hook(SWITCH_HOOK_THREAD, SWITCH_HOOK_ARGC, oldThreadId, oldPriority, thread->threadId,
		     thread->currentPriority, reason);
	if (initialPriority < cpu->priority) {
		moveStandbyToReady();
		if (cpu->nextRunningThread == 0)
			addIntrRequest(cpuId, INTR_MASK_SCHEDULING);
	}
	return (struct ThreadCB *)thread;
}

/*
 * On the CPU's switch stack: a waiting thread is taken off the CPU, then
 * the next thread is set up and restored (until a restore sticks).
 */
void switchThread2(uint32_t reason)
{
	struct ThreadMgrCpuCB *cpu = currentCpuState();
	struct ThreadCB *thread = currentThreadContext();
	struct ThreadmgrFastMutex *mutex;
	struct ThreadCB *selected;
	SceUID oldThreadId = cpu->threadId, waitId;
	int oldPriority = cpu->priority;
	uint32_t mode = reason;
	uint64_t ran;

	if (reason & SWITCH_REASON_WAIT) {
		for (mutex = (struct ThreadmgrFastMutex *)thread->ownedFastMutexNext;
		     mutex != (struct ThreadmgrFastMutex *)&thread->ownedFastMutexNext;
		     mutex = (struct ThreadmgrFastMutex *)mutex->next) {
			SCE_DBG_ASSERT_CHECK(mutex->magic == FAST_MUTEX_MAGIC, 183);
			ksceKernelSpinlockLowUnlock(&mutex->lock);
		}
		ran = thread->runClock;
		thread->runClock = ran - cpu->lastThreadSwitchTime + SceSysrootForKernel_E20F6FC8();
		thread->threadReleaseCount++;
		if (s_threadmgrRuntime.manager->paHooks[0] != 0)
			issueCallPAScheduleHookIntrRequest(SceCpuId(), thread->processId, thread->threadId,
				     SCHED_TRACE_WAIT, thread->currentPriority);
		notifyOffCpu(reason);
		waitId = thread->waitId;
		cpu->processId = -1;
		cpu->threadId = -1;
		cpu->priority = PRIORITY_NONE;
		cpu->runningThread = 0;
		ksceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
		if (waitId != WAIT_ID_NONE && waitId != -1 &&
		    s_threadmgrRuntime.manager->signalId != waitId) {
			cpu->releasingWaitObject = 1;
			ksceGUIDReleaseObject(waitId);
			cpu->releasingWaitObject = 0;
		}
		SCE_DBG_ASSERT_LEVEL(0, cpu == currentCpuState(), 226);
		cpu->nextRunningThread = 0;
		mode = 0;
	}
	for (;;) {
		if (mode & SWITCH_MODE_UNLOCK) {
			ksceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
			SCE_DBG_ASSERT_CHECK(cpu->runningThread == 0, 231);
		}
		selected = setupNextContext_thread((struct ThreadCB *)cpu->runningThread,
						   reason, oldThreadId, oldPriority);
		mode = SceIntrmgrForKernel_08FDB0CE(selected->pRegisterContext, 0, selected->pVfpRegisterContext);
	}
}

/*
 * Leaves the CPU for another thread (reason SWITCH_REASON_*): the running
 * thread's context is saved here and the switch goes on on the switch stack;
 * it returns when the thread is restored.
 */
void switchThread(uint32_t reason)
{
	struct ThreadMgrCpuCB *cpu;
	struct ThreadCB *thread;
	uint32_t mode;

	SCE_DBG_ASSERT_CHECK(ThreadmgrIntrDisabled(), 104);
	SCE_DBG_ASSERT_LEVEL(0, !ksceKernelIsIntrAllowedInCurrentContext(), 107);
	cpu = currentCpuState();
	thread = currentThreadContext();
	if (reason & (SWITCH_REASON_PREEMPT | SWITCH_REASON_WAIT))
		SCE_DBG_ASSERT_CHECK(thread->lock != 0, 114);
	if (cpu->releasingWaitObject)
		return;
	mode = (reason & SWITCH_REASON_PREEMPT) ? SWITCH_REASON_WAIT :
	       (reason & SWITCH_REASON_WAIT) ? SWITCH_MODE_UNLOCK : SWITCH_REASON_PREEMPT;
	if (reason & SWITCH_REASON_LATER) {
		SCE_DBG_ASSERT_CHECK(!(reason & SWITCH_REASON_PREEMPT), 132);
		addIntrRequest(SceCpuId(), INTR_MASK_SCHEDULING);
		return;
	}
	if (cpu->disabledDispatch) {
		SCE_DBG_ASSERT_CHECK(!(reason & SWITCH_REASON_PREEMPT), 139);
		SCE_DBG_PRINTF(0xb, 140, "disabled dispatch(0x%08X)\n", cpu->disabledDispatch);
		return;
	}
	if (!SceIntrmgrForKernel_71C5F42F(thread->pRegisterContext, thread->pVfpRegisterContext)) {
		callSwitchThread2OnStack(cpu->switchStackTop, mode);
		/* the restore comes back to the save above */
		__asm__ volatile("" ::: "memory");
	}
}
