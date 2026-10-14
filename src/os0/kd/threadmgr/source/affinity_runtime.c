/*
 * SceKernelThreadMgr: what a process type allows of CPU affinity, the
 * per-CPU requests that move a thread's affinity or priority, and taking
 * the running thread off its CPU from an interrupt.
 */

#include <string.h>
#include "private.h"
#include "fast_mutex.h"
#include "priority_change.h"
#include "scheduler.h"
#include "sce/cpu.h"
#include "sce/debug.h"
#include "sce/sysmem.h"
#include "syscall_context.h"

#undef __FILE_NAME__
#define __FILE_NAME__ "threadmgr.c"

#define AFFINITY_MASK			0xf
#define AFFINITY_SYSTEM_CPU		0x8	/* CPU 3 */
#define THREAD_ATTR_UNLIMITED		0x00004000
#define TRACE_EVENT_AFFINITY		0x20
#define TRACE_EVENT_PRIORITY		0x21
#define FAST_MUTEX_MAGIC		0xa35db473
#define PREEMPT_WAIT			4
#define PREEMPT_EXCEPTION		8
#define PREEMPT_INTR			2
#define SWITCH_HOOK_OFF_CPU		2
#define SWITCH_HOOK_ARGC		5
#define SCHED_TRACE_OFF_CPU		2
#define PRIORITY_NONE			0xfe


/* the titles whose system-type processes may use any CPU */
#if FW_DEBUG
static inline int anyCpuTitle(SceProcessObject *process)
{
	return strncmp(process->TitleId, "NPXS10084", 31) == 0 ||
	       strncmp(process->TitleId, "NPXS10080", 31) == 0;
}
#else
#define anyCpuTitle(process) \
	(strncmp((process)->TitleId, "NPXS10084", 31) == 0 || \
	 strncmp((process)->TitleId, "NPXS10080", 31) == 0)
#endif

/* checkCpuAffinityMaskPhy: the CPUs of mask may run a thread of process (attr: its attributes) */
int checkCpuAffinityMaskPhy(uint32_t mask, uint32_t attributes, SceProcessObject *pProcess)
{
	uint32_t type;

	if (mask == 0)
		return 0;
	SCE_DBG_ASSERT_CHECK(!(mask & ~AFFINITY_MASK), 3050);
	type = pProcess->budget->type;
#if FW_DEBUG
	if (type == PROCESS_TYPE_MINI_APP) {
		if (!(mask & ~AFFINITY_SYSTEM_CPU) || anyCpuTitle(pProcess))
			return 0;
		SCE_DBG_PRINTF(0, 3069, " k: checkCpuAffinityMaskPhy(), pProcess->TitleId=[%s]\n",
			       pProcess->TitleId);
	} else if (type > PROCESS_TYPE_MINI_APP) {
		switch (type) {
		case PROCESS_TYPE_SHELL:
		case PROCESS_TYPE_KERNEL:
			break;
		default:
			goto invalidType;
		}
		if (!(mask >> 4))
			return 0;
	} else {
		if (type != PROCESS_TYPE_GAME)
			goto invalidType;
		if (!(mask >> 3) || (attributes & THREAD_ATTR_UNLIMITED))
			return 0;
	}
#else
	switch (type) {
	case PROCESS_TYPE_MINI_APP:
		if (!(mask & ~AFFINITY_SYSTEM_CPU))
			return 0;
		return anyCpuTitle(pProcess) ? 0 : SCE_KERNEL_ERROR_ILLEGAL_CPU_AFFINITY_MASK;
	case PROCESS_TYPE_GAME:
		return (mask & ~0x7u) && !(attributes & THREAD_ATTR_UNLIMITED) ?
			SCE_KERNEL_ERROR_ILLEGAL_CPU_AFFINITY_MASK : 0;
	case PROCESS_TYPE_SHELL:
	case PROCESS_TYPE_KERNEL:
		return (mask & ~AFFINITY_MASK) ? SCE_KERNEL_ERROR_ILLEGAL_CPU_AFFINITY_MASK : 0;
	default:
		goto invalidType;
	}
#endif
	SCE_DBG_PRINTF(0, 3090, "*** error: checkCpuAffinityMaskPhy(0x%08X, 0x%08X), type=0x%08X\n",
		       mask, attributes, pProcess->budget->type);
	return SCE_KERNEL_ERROR_ILLEGAL_CPU_AFFINITY_MASK;
invalidType:
	SCE_DBG_PRINTF(0, 3082, "error: processType=0x%08X\n", type);
	SCE_PANIC(SCE_LINE(3077, 3083));
}

/* sceKernelGetDefaultCpuAffinityMask: the CPUs a process type's threads use by default */
uint32_t sceKernelGetDefaultCpuAffinityMask(ProcessBudget *budget)
{
	uint32_t type = budget->type;

	switch (type) {
	case PROCESS_TYPE_GAME:
		return 0x7;
	case PROCESS_TYPE_MINI_APP:
	case PROCESS_TYPE_SHELL:
		return 0x8;
	case PROCESS_TYPE_KERNEL:
		return 0xf;
	default:
		SCE_DBG_PRINTF(0, 8634, "*** warning: unknown process type(0x%08X)\n", type);
		SCE_PANIC(SCE_LINE(8630, 8635));
	}
}

/* cpuId sets threadId's affinity */
void issueChangeThreadCpuAffinityIntrRequest(unsigned int cpuId, SceUID threadId, uint32_t mask)
{
	struct ThreadmgrIntrRequest *request =
		allocIntrRequest(INTR_REQUEST_CHANGE_THREAD_CPU_AFFINITY);
	ThreadmgrTraceHook hook;

	hook = s_threadmgrRuntime.schedTraceHook;
	request->payload[0] = threadId;
	request->payload[1] = mask;
	if (hook != NULL)
		hook(TRACE_EVENT_AFFINITY, 3, threadId, mask, 0);
	issueIntrRequest(cpuId, request);
}

/* cpuId sets threadId's priority and affinity */
void issueChangeThreadPriorityAndCpuAffinityIntrRequest(unsigned int cpuId, SceUID threadId,
				       int priority, uint32_t mask)
{
	struct ThreadmgrIntrRequest *request =
		allocIntrRequest(INTR_REQUEST_CHANGE_THREAD_PRIORITY_AFFINITY);
	ThreadmgrTraceHook hook;

	hook = s_threadmgrRuntime.schedTraceHook;
	request->payload[2] = mask;
	request->payload[0] = threadId;
	request->payload[1] = priority;
	if (hook != NULL)
		hook(TRACE_EVENT_PRIORITY, 2, threadId, priority, mask);
	issueIntrRequest(cpuId, request);
}

/* the rest is Sony's dispatch.c */
#undef __FILE_NAME__
#define __FILE_NAME__ "dispatch.c"

#define INTR_SCHED_PROBE(symbol, label) \
	static uint32_t symbol##ProbeId; \
	volatile uintptr_t symbol##Handler; \
	extern const char symbol##CodeSite[]; \
	const struct ThreadmgrSchedProbeDescriptor symbol##Descriptor = { \
		.probeId = &symbol##ProbeId, .provider = "__sched_", .name = label, \
		.codeSite = symbol##CodeSite, .handler = &symbol##Handler, \
	}

INTR_SCHED_PROBE(s_schedIntrWait, "wait");
INTR_SCHED_PROBE(s_schedIntrPreempt, "preempt");
INTR_SCHED_PROBE(s_schedIntrOffCpu, "off__cpu");

typedef void (*ThreadmgrOffCpuProbe)(const struct ThreadmgrSchedProbeDescriptor *probe, ...);

/* PLACEHOLDER: name. notifyOffCpu, from an interrupt (its own probe sites) */
void notifyOffCpuIntr(uint32_t reason)
{
	struct ThreadMgrCpuCB *cpu = currentCpuState();
	struct ThreadCB *thread;
	ThreadmgrOffCpuProbe probe;
	ThreadmgrOffCpuHook hook;
	SceUID id;

	if (reason & 4) {
		/* the DTrace probe site __sched_:wait */
		__asm__ volatile(".global s_schedIntrWaitCodeSite\ns_schedIntrWaitCodeSite:");
		probe = (ThreadmgrOffCpuProbe)s_schedIntrWaitHandler;
		if (probe != NULL)
			probe(&s_schedIntrWaitDescriptor);
	} else {
		/* the DTrace probe site __sched_:preempt */
		__asm__ volatile(".global s_schedIntrPreemptCodeSite\ns_schedIntrPreemptCodeSite:");
		probe = (ThreadmgrOffCpuProbe)s_schedIntrPreemptHandler;
		if (probe != NULL)
			probe(&s_schedIntrPreemptDescriptor);
	}
	/* the DTrace probe site __sched_:off__cpu */
	__asm__ volatile(".global s_schedIntrOffCpuCodeSite\ns_schedIntrOffCpuCodeSite:");
	probe = (ThreadmgrOffCpuProbe)s_schedIntrOffCpuHandler;
	if (probe != NULL) {
		thread = (struct ThreadCB *)cpu->runningThread;
		id = thread->userThreadId;
		if (id < 0)
			id = thread->threadId;
		probe(&s_schedIntrOffCpuDescriptor, id,
		      thread->process->pid);
	}
	hook = s_threadmgrRuntime.offCpuHook;
	if (hook != NULL) {
		thread = (struct ThreadCB *)cpu->runningThread;
		id = thread->userThreadId;
		if (id < 0)
			id = thread->threadId;
		hook(thread->process->pid, id, (reason & 4) ? 4 : 2);
	}
}

/*
 * From an interrupt: the running thread (its lock held) leaves the CPU
 * for reason PREEMPT_*; nothing runs on the CPU until a thread is chosen.
 */
void runningThreadPreemptByIntr(struct ThreadCB *context, uint32_t reason)
{
	struct ThreadCB *thread = context;
	struct ThreadMgrCpuCB *cpu = currentCpuState();
	struct ThreadmgrFastMutex *mutex;
	ThreadmgrTraceHook hook;
	uint64_t ran;

	SCE_DBG_ASSERT_CHECK(ThreadmgrIntrDisabled(), 1253);
	SCE_DBG_ASSERT_CHECK(thread->lock != 0, 1256);
	SCE_DBG_ASSERT_CHECK(currentThreadContext() == NULL, 1258);
	for (mutex = (struct ThreadmgrFastMutex *)thread->ownedFastMutexNext;
	     mutex != (struct ThreadmgrFastMutex *)&thread->ownedFastMutexNext;
	     mutex = (struct ThreadmgrFastMutex *)mutex->next) {
		SCE_DBG_ASSERT_CHECK(mutex->magic == FAST_MUTEX_MAGIC, 1266);
		ksceKernelSpinlockLowUnlock(&mutex->lock);
	}
	ran = thread->runClock;
	thread->runClock = ran - cpu->lastThreadSwitchTime + SceSysrootForKernel_E20F6FC8();
	if (reason == PREEMPT_WAIT)
		thread->threadReleaseCount++;
	else if (reason == PREEMPT_EXCEPTION)
		thread->threadPreemptCount++;
	else if (reason == PREEMPT_INTR)
		thread->intrPreemptCount++;
	else
		SCE_PANIC(1288);
	hook = s_threadmgrRuntime.switchHook;
	if (hook != NULL)
		hook(SWITCH_HOOK_OFF_CPU, SWITCH_HOOK_ARGC, thread->threadId, thread->currentPriority,
		     -1, -1, reason);
	if (s_threadmgrRuntime.manager->paHooks[0] != 0)
		issueCallPAScheduleHookIntrRequest(SceCpuId(), thread->processId, thread->threadId,
					   SCHED_TRACE_OFF_CPU, thread->currentPriority);
	notifyOffCpuIntr(reason);
	cpu->processId = -1;
	cpu->threadId = -1;
	cpu->priority = PRIORITY_NONE;
	cpu->runningThread = 0;
	SCE_DBG_ASSERT_LEVEL(0, cpu == currentCpuState(), 1313);
	cpu->nextRunningThread = 0;
}
