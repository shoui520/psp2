/*
 * SceKernelThreadMgr (dispatch.c): choosing the thread a CPU runs next.
 * The CPU's own queues and the shared global queues are searched by
 * priority; a global thread this CPU may not run is left for a CPU that
 * may, which is asked to reschedule.
 */

#include "private.h"
#include "priority_change.h"
#include "scheduler.h"
#include <sysmem/cpu.h>
#include <sysmem/debug.h>
#include <string.h>
#include <kernel/error.h>
#include "fast_mutex.h"
#include <sysmem/sysmem.h>
#include "syscall_context.h"
#include <intrmgr/intrmgr.h>
#include "affinity.h"
#include <threadmgr/uid_waitable.h>
#include <threadmgr/threadmgr.h>

/* what both context selectors share */
#define DISPATCH_CPU_COUNT		4
#define DISPATCH_PRIORITY_SHARED_LOW	0x80	/* priorities 0x80-0xfb share the ready queues */
#define DISPATCH_PRIORITY_SHARED_RANGE	0x7c
#define DISPATCH_STATE_READY		0x00000002
#define DISPATCH_STATE_STANDBY		0x00000004
#define DISPATCH_STATE_HIGH_MASK	0xffff0000

/*
 * The preempted running thread of cpu goes back: to the ready queues, or to
 * the standby list of the CPU running the least urgent thread it may run on.
 * Sony inlined it into both context selectors; caller is theirs.
 */
static inline __attribute__((always_inline)) void distributePreemptedThread(
	struct ThreadMgrCpuCB *cpu,
	struct ThreadCB *thread, unsigned int cpuId, const void *caller)
{
	struct SceKernelThreadmgrPrivate *manager;
	uint32_t affinity, target = UINT32_MAX;
	unsigned int other;
	int worst = 1;

	SCE_DBG_ASSERT_CHECK_LR(thread->lock != 0, 253, caller);
	affinity = thread->currentCpuAffinityMask;
	if (affinity == 0)
		affinity = thread->process->cpuAffinity;
	affinity &= ~(1u << cpuId);
	SCE_DBG_ASSERT_CHECK_LR(thread->currentCpuId == (int)sceKernelCpuId(), 260, caller);
	manager = s_threadmgrRuntime.manager;
	if ((uint32_t)thread->currentPriority - DISPATCH_PRIORITY_SHARED_LOW < DISPATCH_PRIORITY_SHARED_RANGE) {
		thread->status = (thread->status & DISPATCH_STATE_HIGH_MASK) |
				     DISPATCH_STATE_READY;
		addReadyTop((struct ThreadCB *)thread);
		manager = s_threadmgrRuntime.manager;
		for (other = 0; other < DISPATCH_CPU_COUNT; other++) {
			if (((1u << other) & affinity) && worst < manager->cpuCB[other].priority) {
				worst = manager->cpuCB[other].priority;
				target = other;
			}
		}
		if (worst <= thread->currentPriority)
			target = UINT32_MAX;
	} else {
		for (other = 0; other < DISPATCH_CPU_COUNT; other++) {
			if (((1u << other) & affinity) && worst < manager->cpuCB[other].priority) {
				worst = manager->cpuCB[other].priority;
				target = other;
			}
		}
		if (thread->currentPriority < worst) {
			thread->currentCpuId = target;
			thread->status = (thread->status & DISPATCH_STATE_HIGH_MASK) |
					     DISPATCH_STATE_STANDBY;
			addStandby((struct ThreadMgrCpuCB *)&manager->cpuCB[target],
				   (struct ThreadCB *)thread);
		} else {
			thread->status = (thread->status & DISPATCH_STATE_HIGH_MASK) |
					     DISPATCH_STATE_READY;
			addReadyTop((struct ThreadCB *)thread);
			target = UINT32_MAX;
		}
	}
	thread = (struct ThreadCB *)cpu->runningThread;
	sceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
	if (target != UINT32_MAX)
		addIntrRequest(target, INTR_MASK_SCHEDULING);
}

#define PRIORITY_NO_THREAD			0x100	/* above every priority */
#define PRIORITY_GLOBAL_FIRST_WORD	4	/* the global queues: bitmap words 4-7 of the whole */
#define PRIORITY_CPU_LAST_WORD		7	/* priorities 0xe0-0xff: the CPU's own again */

typedef void (*ThreadmgrOffCpuProbe)(const struct ThreadmgrSchedProbeDescriptor *probe, ...);

#define SCHED_PROBE(symbol, providerName, probeName)				\
	static uint32_t symbol##ProbeId;						\
	volatile uintptr_t symbol##Handler;					\
	extern const char symbol##CodeSite[];					\
	const struct ThreadmgrSchedProbeDescriptor symbol##Descriptor = {	\
		.probeId = &symbol##ProbeId, .provider = providerName, .name = probeName, \
		.codeSite = symbol##CodeSite, .handler = &symbol##Handler,	\
	}

SCHED_PROBE(s_schedWait, "__sched_", "wait");
SCHED_PROBE(s_schedPreempt, "__sched_", "preempt");
SCHED_PROBE(s_schedOffCpu, "__sched_", "off__cpu");

/* the most urgent priority with a ready thread for this CPU (PRIORITY_NO_THREAD: none) */
uint32_t searchHighestPriority(void)
{
	struct ThreadMgrCpuCB *cpu = currentCpuState();
	uint32_t bitmap, priority = PRIORITY_NO_THREAD;
	unsigned int word;

	for (word = 0; word < PRIORITY_GLOBAL_FIRST_WORD; word++) {
		bitmap = cpu->readyBitmap[word];
		if (bitmap != 0) {
			priority = __builtin_clz(bitmap) + word * 32;
			goto found;
		}
	}
	sceKernelSpinlockLowLock(&s_threadmgrRuntime.manager->globalReadyLock);
	for (word = 0; word < 4; word++) {
		bitmap = s_threadmgrRuntime.manager->globalReadyBitmap[word];
		if (bitmap != 0) {
			priority = __builtin_clz(bitmap) + (word + PRIORITY_GLOBAL_FIRST_WORD) * 32;
			break;
		}
	}
	sceKernelSpinlockLowUnlock(
		&s_threadmgrRuntime.manager->globalReadyLock);
	if (priority == PRIORITY_NO_THREAD) {
		bitmap = cpu->readyBitmap[PRIORITY_CPU_LAST_WORD];
		if (bitmap != 0)
			priority = __builtin_clz(bitmap) + PRIORITY_CPU_LAST_WORD * 32;
	}
found:
	SCE_DBG_ASSERT_PRINTF(priority <= PRIORITY_NO_THREAD, 1597,
			      " fatal error: searchHighestPriority() = %d\n", priority);
	return priority;
}

/*
 * PLACEHOLDER: name. The running thread leaves the CPU (reason & 4: to
 * wait, else preempted): the DTrace probes and the off-CPU hook hear of it.
 */
void notifyOffCpu(uint32_t reason)
{
	struct ThreadMgrCpuCB *cpu = currentCpuState();
	struct ThreadCB *thread;
	ThreadmgrOffCpuProbe probe;
	ThreadmgrOffCpuHook hook;
	SceUID id;

	if (reason & 4) {
		/* the DTrace probe site __sched_:wait */
		__asm__ volatile(".global s_schedWaitCodeSite\ns_schedWaitCodeSite:");
		probe = (ThreadmgrOffCpuProbe)s_schedWaitHandler;
		if (probe != NULL)
			probe(&s_schedWaitDescriptor);
	} else {
		/* the DTrace probe site __sched_:preempt */
		__asm__ volatile(".global s_schedPreemptCodeSite\ns_schedPreemptCodeSite:");
		probe = (ThreadmgrOffCpuProbe)s_schedPreemptHandler;
		if (probe != NULL)
			probe(&s_schedPreemptDescriptor);
	}
	/* the DTrace probe site __sched_:off__cpu */
	__asm__ volatile(".global s_schedOffCpuCodeSite\ns_schedOffCpuCodeSite:");
	probe = (ThreadmgrOffCpuProbe)s_schedOffCpuHandler;
	if (probe != NULL) {
		thread = (struct ThreadCB *)cpu->runningThread;
		id = thread->userThreadId;
		if (id < 0)
			id = thread->threadId;
		probe(&s_schedOffCpuDescriptor, id, thread->process->pid);
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

/* the thread taken off a queue runs on this CPU now */
static inline void takeThread(struct ThreadMgrCpuCB *cpu,
			      struct ThreadCB *thread, uint32_t cpuId)
{
	if (thread->lastExecutedCpuId != cpuId) {
		thread->lastExecutedCpuId = cpuId;
		thread->changeCpuCount++;
	}
	thread->status = (thread->status & THREAD_STATE_HIGH_MASK) | SCE_KERNEL_THREAD_STATUS_RUNNING;
}

/*
 * Picks the thread this CPU runs next into cpu->nextRunningThread: the most urgent
 * ready one more urgent than current (which stays otherwise).
 */
void findNextRunningThread(struct ThreadCB *current)
{
	struct ThreadCB *thread = current, *next;
	struct ThreadMgrCpuCB *cpu;
	uint32_t cpuId = sceKernelCpuId(), mask, bitmap, affinity, target;
	unsigned int word, other;
	int currentPriority, priority, worst;
	volatile unsigned int delay;

	cpu = currentCpuState();
	currentPriority = thread != NULL ? thread->currentPriority : PRIORITY_NO_THREAD;

	/* this CPU's own queues first */
	for (word = 0; word < PRIORITY_GLOBAL_FIRST_WORD; word++) {
		bitmap = cpu->readyBitmap[word];
		if (bitmap == 0)
			continue;
		priority = __builtin_clz(bitmap) + word * 32;
		if (priority >= currentPriority)
			goto selected;
		thread = (struct ThreadCB *)cpu->readyQueue[priority].next;
		SCE_DBG_ASSERT_CHECK(thread->magic == THREAD_CB_MAGIC, 348);
		sceKernelSpinlockLowLock(THREAD_LOCK(thread));
		removeReady((struct ThreadCB *)thread, priority);
		SCE_DBG_ASSERT_CHECK(thread->currentCpuId == (int)sceKernelCpuId(), 353);
		goto take;
	}

	/* the global queues: the first thread this CPU may run */
	mask = (1u << cpuId) & CPU_MASK_ALL;
retry:
	sceKernelSpinlockLowLock(&s_threadmgrRuntime.manager->globalReadyLock);
	for (word = 0; word < 4; word++) {
		bitmap = s_threadmgrRuntime.manager->globalReadyBitmap[word];
		while (bitmap != 0) {
			priority = __builtin_clz(bitmap) + (word + PRIORITY_GLOBAL_FIRST_WORD) * 32;
			if (priority >= currentPriority) {
				sceKernelSpinlockLowUnlock(&s_threadmgrRuntime.manager->globalReadyLock);
				SCE_DBG_ASSERT_LEVEL(0, thread != NULL, 457);
				goto selected;
			}
			for (thread = (struct ThreadCB *)s_threadmgrRuntime.
				     manager->globalReadyQueue[priority - 0x80].next;
			     thread != (struct ThreadCB *)
				     &s_threadmgrRuntime.manager->globalReadyQueue[priority - 0x80];
			     thread = next) {
				next = (struct ThreadCB *)thread->next;
				SCE_DBG_ASSERT_CHECK(s_threadmgrRuntime.manager->globalReadyLock != 0, 387);
				SCE_DBG_ASSERT_PRINTF(thread->magic == THREAD_CB_MAGIC, 389,
					"*** fatal error: pNewRunningThreadCB=%p, priority=%d\n",
					thread, priority);
				if (SceCpuForDriver_5AC9D394(THREAD_LOCK(thread)) < 0) {
					/* held elsewhere: start over shortly */
					sceKernelSpinlockLowUnlock(&s_threadmgrRuntime.manager->globalReadyLock);
					for (delay = LOCKED_RETRY_DELAY; delay != 0; delay--)
						;
					thread = current;
					goto retry;
				}
				affinity = thread->currentCpuAffinityMask;
				if (affinity == 0)
					affinity = thread->process->
						cpuAffinity;
				if (mask & affinity) {
					removeCommonRaw((struct ThreadCB *)thread, priority);
					cpu->processId = thread->processId;
					cpu->threadId = thread->threadId;
					cpu->priority = thread->currentPriority;
					sceKernelSpinlockLowUnlock(&s_threadmgrRuntime.manager->globalReadyLock);
					takeThread(cpu, thread, cpuId);
					SCE_DBG_ASSERT_CHECK(thread->currentCpuId == THREAD_CPU_NONE, 424);
					thread->currentCpuId = cpuId;
					sceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
					goto selected;
				}
				/* not for this CPU: one that may run it reschedules if it should */
				sceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
				target = UINT32_MAX;
				worst = 1;
				for (other = 0; other < CPU_COUNT; other++) {
					if (((1u << other) & ~mask & affinity) &&
					    worst < s_threadmgrRuntime.manager->cpuCB[other].priority) {
						target = other;
						worst = s_threadmgrRuntime.manager->cpuCB[other].priority;
					}
				}
				if (priority < worst)
					addIntrRequest(target, INTR_MASK_SCHEDULING);
			}
			thread = current;
			bitmap &= ~(0x80000000u >> (priority % 32));
		}
	}
	sceKernelSpinlockLowUnlock(&s_threadmgrRuntime.manager->globalReadyLock);

	/* the least urgent of this CPU's own */
	bitmap = cpu->readyBitmap[PRIORITY_CPU_LAST_WORD];
	if (bitmap == 0)
		goto selected;
	priority = __builtin_clz(bitmap) + PRIORITY_CPU_LAST_WORD * 32;
	if (priority >= currentPriority)
		goto selected;
	thread = (struct ThreadCB *)cpu->readyQueue[priority].next;
	SCE_DBG_ASSERT_CHECK(thread->magic == THREAD_CB_MAGIC, 477);
	sceKernelSpinlockLowLock(THREAD_LOCK(thread));
	removeReady((struct ThreadCB *)thread, priority);
	SCE_DBG_ASSERT_CHECK(thread->currentCpuId == (int)sceKernelCpuId(), 481);
take:
	takeThread(cpu, thread, cpuId);
	cpu->processId = thread->processId;
	cpu->threadId = thread->threadId;
	cpu->priority = thread->currentPriority;
	sceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
selected:
	SCE_DBG_ASSERT_LEVEL(0, thread != NULL, 502);
	cpu->nextRunningThread = (uintptr_t)thread;
}

void SceThreadmgrForKernel_8C07C8EC(ThreadmgrTraceHook hook)
{
	s_threadmgrRuntime.switchHook = hook;
}

void SceThreadmgrForKernel_F55FDEFB(ThreadmgrTraceHook hook)
{
	s_threadmgrRuntime.schedTraceHook = hook;
}

void SceThreadmgrForKernel_E2DACBC2(ThreadmgrTraceHook hook)
{
	s_threadmgrRuntime.threadTraceHook = hook;
}

void SceThreadmgrForKernel_A3975A5A(ThreadmgrOnCpuHook hook)
{
	s_threadmgrRuntime.onCpuHook = hook;
}

void SceThreadmgrForKernel_F885ECCA(ThreadmgrOffCpuHook hook)
{
	s_threadmgrRuntime.offCpuHook = hook;
}

void SceThreadmgrForKernel_9605DF7D(ThreadmgrPriorityChangeHook hook)
{
	s_threadmgrRuntime.priorityChangeHook = hook;
}

int SceThreadmgrForKernel_BB56D42F(void)
{
	struct ThreadMgrCpuCB *cpu = currentCpuState();
	uintptr_t current;

	if (cpu->disabledDispatch)
		return 0;
	current = cpu->runningThread;
	if (current)
		return current != cpu->nextRunningThread;
	return 1;
}

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
		sceKernelSpinlockLowUnlock(&mutex->lock);
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

/*
 * SceKernelThreadMgr (dispatch.c): switching threads from thread context.
 * switchThread saves the running thread's context and goes to the CPU's
 * switch stack, where switchThread2 picks the next thread
 * (setupNextContext_thread) and restores it. The preempted thread goes
 * back to a ready queue or to the standby list of a CPU it can preempt.
 */

#define DEBUG_KIND_STACK_OVERFLOW	4
#define DEBUG_EVENT_SUSPEND		1
#define SWITCH_HOOK_THREAD		0	/* the switch hook's first argument */
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
/* context_runtime.S */
extern void callSwitchThread2OnStack(uintptr_t stackTop, uint32_t mode);

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
		sceKernelSpinlockLowLock(THREAD_LOCK(thread));
		thread = (struct ThreadCB *)cpu->runningThread;
		for (mutex = (struct ThreadmgrFastMutex *)thread->ownedFastMutexNext;
		     mutex != (struct ThreadmgrFastMutex *)&thread->ownedFastMutexNext;
		     mutex = (struct ThreadmgrFastMutex *)mutex->next) {
			SCE_DBG_ASSERT_CHECK(mutex->magic == FAST_MUTEX_MAGIC, 583);
			sceKernelSpinlockLowUnlock(&mutex->lock);
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
				if (!SceCpuForDriver_CDA96E81((SceInt32 *)&process->abortRequested,
								     0, 1)) {
					thread = (struct ThreadCB *)cpu->runningThread;
					process->abortCode = DEBUG_KIND_STACK_OVERFLOW;
					process->abortThread = thread->threadId;
					status = sceKernelSetEventFlag(process->abortEventFlag,
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
	sceKernelSpinlockLowLock(THREAD_LOCK(selected));
	for (mutex = (struct ThreadmgrFastMutex *)selected->ownedFastMutexNext;
	     mutex != (struct ThreadmgrFastMutex *)&selected->ownedFastMutexNext;
	     mutex = (struct ThreadmgrFastMutex *)mutex->next) {
		SCE_DBG_ASSERT_CHECK(mutex->magic == FAST_MUTEX_MAGIC, 689);
		sceKernelSpinlockLowLock(&mutex->lock);
	}
	sceKernelSpinlockLowUnlock(THREAD_LOCK(selected));
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
			sceKernelSpinlockLowUnlock(&mutex->lock);
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
		sceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
		if (waitId != WAIT_ID_NONE && waitId != -1 &&
		    s_threadmgrRuntime.manager->signalId != waitId) {
			cpu->releasingWaitObject = 1;
			sceGUIDReleaseObject(waitId);
			cpu->releasingWaitObject = 0;
		}
		SCE_DBG_ASSERT_LEVEL(0, cpu == currentCpuState(), 226);
		cpu->nextRunningThread = 0;
		mode = 0;
	}
	for (;;) {
		if (mode & SWITCH_MODE_UNLOCK) {
			sceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
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
	SCE_DBG_ASSERT_LEVEL(0, !SceIntrmgrForDriver_182EE3E3(), 107);
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

/*
 * SceKernelThreadMgr (dispatch.c): switching threads at the end of an
 * interrupt. intrmgr asks setupNextContext_intr for the context to return
 * to: the running thread's, or the next thread's after the running one was
 * preempted. A user thread found to have overflowed its stack is
 * debug-suspended at once.
 */

#define SWITCH_HOOK_INTR		1

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
		sceKernelSpinlockLowLock(THREAD_LOCK(thread));
		thread = (struct ThreadCB *)cpu->runningThread;
		for (mutex = (struct ThreadmgrFastMutex *)thread->ownedFastMutexNext;
		     mutex != (struct ThreadmgrFastMutex *)&thread->ownedFastMutexNext;
		     mutex = (struct ThreadmgrFastMutex *)mutex->next) {
			SCE_DBG_ASSERT_CHECK(mutex->magic == FAST_MUTEX_MAGIC, 1007);
			sceKernelSpinlockLowUnlock(&mutex->lock);
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
					sceKernelSpinlockLowLock(&s_threadmgrRuntime.manager->threadMgrCBLock);
					thread = (struct ThreadCB *)cpu->runningThread;
					thread->next = (uintptr_t)
						&s_threadmgrRuntime.manager->debugSuspendedNext;
					thread->prev = s_threadmgrRuntime.manager->debugSuspendedPrev;
					s_threadmgrRuntime.manager->debugSuspendedPrev = (uintptr_t)thread;
					*(uintptr_t *)thread->prev = (uintptr_t)thread;
					sceKernelSpinlockLowUnlock(&s_threadmgrRuntime.manager->threadMgrCBLock);
					thread = (struct ThreadCB *)cpu->runningThread;
					sceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
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
				if (!SceCpuForDriver_CDA96E81((SceInt32 *)&process->abortRequested,
								     0, 1)) {
					thread = (struct ThreadCB *)cpu->runningThread;
					process->abortCode = DEBUG_KIND_STACK_OVERFLOW;
					process->abortThread = thread->threadId;
					status = sceKernelSetEventFlag(process->abortEventFlag,
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
	sceKernelSpinlockLowLock(THREAD_LOCK(selected));
	for (mutex = (struct ThreadmgrFastMutex *)selected->ownedFastMutexNext;
	     mutex != (struct ThreadmgrFastMutex *)&selected->ownedFastMutexNext;
	     mutex = (struct ThreadmgrFastMutex *)mutex->next) {
		SCE_DBG_ASSERT_CHECK(mutex->magic == FAST_MUTEX_MAGIC, 1127);
		sceKernelSpinlockLowLock(&mutex->lock);
	}
	sceKernelSpinlockLowUnlock(THREAD_LOCK(selected));
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

/*
 * SceKernelThreadMgr (dispatch.c): an interrupt arriving while the running
 * thread spins on a fast mutex. The spin ends: the thread takes the mutex
 * if it is free, else it waits on the mutex's UID object (or, with a stop
 * or exit request pending, gives up the wait).
 */

#define SPIN_RETRY_DELAY		10

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
	sceKernelSpinlockLowLock(THREAD_LOCK(thread));
	for (;;) {
		SCE_DBG_ASSERT_CHECK(thread->waitType == SCE_KERNEL_WAITTYPE_FAST_MUTEX_SPIN, 821);
		SCE_DBG_ASSERT_CHECK((thread->status & SCE_KERNEL_THREAD_STATUS_RUNNING) &&
				     thread->currentCpuId == (int)sceKernelCpuId(), 823);
		SCE_DBG_ASSERT_CHECK(thread->spinMutex->magic == FAST_MUTEX_MAGIC, 829);
		mutex = thread->spinMutex;
		if (SceCpuForDriver_5AC9D394(&mutex->queue->lock) >= 0)
			break;
		sceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
		for (delay = SPIN_RETRY_DELAY; delay != 0; delay--)
			;
		sceKernelSpinlockLowLock(THREAD_LOCK(thread));
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
		sceKernelSpinlockLowLock(&mutex->lock);
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
		thread->waitType = SCE_KERNEL_WAITTYPE_FAST_MUTEX;
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
		SceCpuForDriver_BC248C30((SceInt32 *)&thread->flags, THREAD_FLAG_SIGNAL_DEFERRED);
		SceCpuForDriver_4AE1BCC0((SceInt32 *)&thread->flags, THREAD_FLAG_WAIT_CLEAR);
		request = thread->flags & THREAD_FLAG_REQUEST_MASK;
		if (request == THREAD_FLAG_REQUEST_EXIT_DELETE || request == THREAD_FLAG_REQUEST_EXIT)
			thread->waitResult = SCE_KERNEL_ERROR_THREAD_STOPPED;
		else if (request == THREAD_FLAG_REQUEST_SUSPEND)
			thread->waitResult = SCE_KERNEL_ERROR_THREAD_SUSPENDED;
		else
			SCE_DBG_ASSERT_LEVEL(0, 0, 865);
		thread->spinMutex = NULL;
		thread->waitType = 0;
	}
unlock:
	sceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
	sceKernelSpinlockLowUnlock(&mutex->queue->lock);
}
