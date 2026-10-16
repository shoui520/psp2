/*
 * SceKernelThreadMgr (dispatch.c): choosing the thread a CPU runs next.
 * The CPU's own queues and the shared global queues are searched by
 * priority; a global thread this CPU may not run is left for a CPU that
 * may, which is asked to reschedule.
 */

#include "private.h"
#include "priority_change.h"
#include "scheduler.h"
#include "sce/cpu.h"
#include "sce/debug.h"

#undef __FILE_NAME__
#define __FILE_NAME__ "dispatch.c"

#define CPU_COUNT			4
#define CPU_MASK_ALL			0xf
#define PRIORITY_NONE			0x100	/* above every priority */
#define PRIORITY_GLOBAL_FIRST_WORD	4	/* the global queues: bitmap words 4-7 of the whole */
#define PRIORITY_CPU_LAST_WORD		7	/* priorities 0xe0-0xff: the CPU's own again */
#define CPU_ID_GLOBAL			0x7fff
#define TRACE_EVENT_SCHED		0x2c
#define LOCKED_RETRY_DELAY		10

typedef void (*ThreadmgrOffCpuProbe)(const struct ThreadmgrSchedProbeDescriptor *probe, ...);

extern void ksceKernelPrintfAssertLevel(SceUInt32 level, SceBool condition,
	const SceKernelDebugInfo *info, const void *lr, const char *format, ...);

#define SCHED_PROBE(symbol, probeName)						\
	static uint32_t symbol##ProbeId;						\
	volatile uintptr_t symbol##Handler;					\
	extern const char symbol##CodeSite[];					\
	const struct ThreadmgrSchedProbeDescriptor symbol##Descriptor = {	\
		.probeId = &symbol##ProbeId, .provider = "__sched_", .name = probeName, \
		.codeSite = symbol##CodeSite, .handler = &symbol##Handler,	\
	}

SCHED_PROBE(s_schedWait, "wait");
SCHED_PROBE(s_schedPreempt, "preempt");
SCHED_PROBE(s_schedOffCpu, "off__cpu");

/* the most urgent priority with a ready thread for this CPU (PRIORITY_NONE: none) */
uint32_t searchHighestPriority(void)
{
	struct ThreadMgrCpuCB *cpu = currentCpuState();
	uint32_t bitmap, priority = PRIORITY_NONE;
	unsigned int word;

	for (word = 0; word < PRIORITY_GLOBAL_FIRST_WORD; word++) {
		bitmap = cpu->readyBitmap[word];
		if (bitmap != 0) {
			priority = __builtin_clz(bitmap) + word * 32;
			goto found;
		}
	}
	ksceKernelSpinlockLowLock(&s_threadmgrRuntime.manager->globalReadyLock);
	for (word = 0; word < 4; word++) {
		bitmap = s_threadmgrRuntime.manager->globalReadyBitmap[word];
		if (bitmap != 0) {
			priority = __builtin_clz(bitmap) + (word + PRIORITY_GLOBAL_FIRST_WORD) * 32;
			break;
		}
	}
	ksceKernelSpinlockLowUnlock(
		&s_threadmgrRuntime.manager->globalReadyLock);
	if (priority == PRIORITY_NONE) {
		bitmap = cpu->readyBitmap[PRIORITY_CPU_LAST_WORD];
		if (bitmap != 0)
			priority = __builtin_clz(bitmap) + PRIORITY_CPU_LAST_WORD * 32;
	}
found:
	SCE_DBG_ASSERT_PRINTF(priority <= PRIORITY_NONE, 1597,
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
	uint32_t cpuId = ksceKernelCpuId(), mask, bitmap, affinity, target;
	unsigned int word, other;
	int currentPriority, priority, worst;
	volatile unsigned int delay;

	cpu = currentCpuState();
	currentPriority = thread != NULL ? thread->currentPriority : PRIORITY_NONE;

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
		ksceKernelSpinlockLowLock(THREAD_LOCK(thread));
		removeReady((struct ThreadCB *)thread, priority);
		SCE_DBG_ASSERT_CHECK(thread->currentCpuId == (int)ksceKernelCpuId(), 353);
		goto take;
	}

	/* the global queues: the first thread this CPU may run */
	mask = (1u << cpuId) & CPU_MASK_ALL;
retry:
	ksceKernelSpinlockLowLock(&s_threadmgrRuntime.manager->globalReadyLock);
	for (word = 0; word < 4; word++) {
		bitmap = s_threadmgrRuntime.manager->globalReadyBitmap[word];
		while (bitmap != 0) {
			priority = __builtin_clz(bitmap) + (word + PRIORITY_GLOBAL_FIRST_WORD) * 32;
			if (priority >= currentPriority) {
				ksceKernelSpinlockLowUnlock(&s_threadmgrRuntime.manager->globalReadyLock);
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
				if (ksceKernelSpinlockLowTrylock(THREAD_LOCK(thread)) < 0) {
					/* held elsewhere: start over shortly */
					ksceKernelSpinlockLowUnlock(&s_threadmgrRuntime.manager->globalReadyLock);
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
					ksceKernelSpinlockLowUnlock(&s_threadmgrRuntime.manager->globalReadyLock);
					takeThread(cpu, thread, cpuId);
					SCE_DBG_ASSERT_CHECK(thread->currentCpuId == CPU_ID_GLOBAL, 424);
					thread->currentCpuId = cpuId;
					ksceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
					goto selected;
				}
				/* not for this CPU: one that may run it reschedules if it should */
				ksceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
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
	ksceKernelSpinlockLowUnlock(&s_threadmgrRuntime.manager->globalReadyLock);

	/* the least urgent of this CPU's own */
	bitmap = cpu->readyBitmap[PRIORITY_CPU_LAST_WORD];
	if (bitmap == 0)
		goto selected;
	priority = __builtin_clz(bitmap) + PRIORITY_CPU_LAST_WORD * 32;
	if (priority >= currentPriority)
		goto selected;
	thread = (struct ThreadCB *)cpu->readyQueue[priority].next;
	SCE_DBG_ASSERT_CHECK(thread->magic == THREAD_CB_MAGIC, 477);
	ksceKernelSpinlockLowLock(THREAD_LOCK(thread));
	removeReady((struct ThreadCB *)thread, priority);
	SCE_DBG_ASSERT_CHECK(thread->currentCpuId == (int)ksceKernelCpuId(), 481);
take:
	takeThread(cpu, thread, cpuId);
	cpu->processId = thread->processId;
	cpu->threadId = thread->threadId;
	cpu->priority = thread->currentPriority;
	ksceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
selected:
	SCE_DBG_ASSERT_LEVEL(0, thread != NULL, 502);
	cpu->nextRunningThread = (uintptr_t)thread;
}

/* PLACEHOLDER: name. Tells cpuId of a scheduling event (with the trace hook) */
void issueCallPAScheduleHookIntrRequest(unsigned int cpuId, SceUID processId, SceUID threadId,
				uint32_t event, int priority)
{
	struct ThreadmgrIntrRequest *request = allocIntrRequest(INTR_REQUEST_CALL_PA_SCHEDULE_HOOK);
	ThreadmgrTraceHook hook;

	hook = s_threadmgrRuntime.schedTraceHook;
	request->payload[0] = processId;
	request->payload[1] = threadId;
	request->payload[2] = event;
	request->payload[3] = priority;
	if (hook != NULL)
		hook(TRACE_EVENT_SCHED, 4, processId, threadId, event, priority);
	issueIntrRequest(cpuId, request);
}
