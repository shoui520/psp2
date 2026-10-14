/*
 * SceKernelThreadMgr (threadmgr.c): the ready queues. Priorities 0x80-0xfb
 * share the manager's global queues (any CPU may take them); the others
 * are per CPU. A thread for another CPU goes on that CPU's standby list
 * until the CPU moves it to its queues.
 */

#include "private.h"
#include "priority_change.h"
#include "scheduler.h"
#include "sce/cpu.h"
#include "sce/debug.h"
#include "syscall_context.h"

#undef __FILE_NAME__
#define __FILE_NAME__ "threadmgr.c"

#define CPU_COUNT			4
#define CPU_MASK_ALL			0xf
#define PRIORITY_SHARED_LOW		0x80	/* priorities 0x80-0xfb share the global queues */
#define PRIORITY_SHARED_RANGE		0x7c
#define CPU_ID_GLOBAL			0x7fff	/* thread->cpuId: on the global queues */
#define STANDBY_RETRY_DELAY		10

typedef void (*ThreadmgrQueueProbe)(const struct ThreadmgrSchedProbeDescriptor *probe,
				    SceUID threadId, SceUID processId, unsigned int cpu);

#define SCHED_PROBE(symbol, probeName)						\
	static uint32_t symbol##ProbeId;						\
	volatile uintptr_t symbol##Handler;					\
	extern const char symbol##CodeSite[];					\
	const struct ThreadmgrSchedProbeDescriptor symbol##Descriptor = {	\
		.probeId = &symbol##ProbeId, .provider = "__sched_", .name = probeName, \
		.codeSite = symbol##CodeSite, .handler = &symbol##Handler,	\
	}

SCHED_PROBE(s_schedGlobalEnqueue, "enqueue");
SCHED_PROBE(s_schedCpuEnqueue, "enqueue");
SCHED_PROBE(s_schedGlobalDequeue, "dequeue");
SCHED_PROBE(s_schedCpuDequeue, "dequeue");
SCHED_PROBE(s_schedGlobalEnqueueTop, "enqueue");
SCHED_PROBE(s_schedCpuEnqueueTop, "enqueue");

/* the ID the probes report: the thread's UID in its process, if it has one */
static inline SceUID probeThreadId(volatile struct ThreadCB *thread)
{
	SceUID userThreadId = thread->userThreadId;

	return userThreadId < 0 ? thread->threadId : userThreadId;
}

static inline unsigned int cpuIndex(struct ThreadMgrCpuCB *cpu)
{
	return -(s_threadmgrRuntime.manager->cpuCB - cpu);
}

/* CLZ: 32 for 0 */
static inline uint32_t leadingZeros(uint32_t value)
{
	return value != 0 ? (uint32_t)__builtin_clz(value) : 32;
}

/* ROR by a register: the amount modulo 32 */
static inline uint32_t rotateRight(uint32_t value, uint32_t shift)
{
	shift &= 31;
	return (value >> shift) | (value << ((-shift) & 31));
}

/* LSR by a register: the low byte of the amount, 0 from 32 on */
static inline uint32_t shiftRight(uint32_t value, uint32_t shift)
{
	shift &= 0xff;
	return shift < 32 ? value >> shift : 0;
}

/* thread goes before node (at the end of the queue node heads, or in front of node) */
static inline void insertBefore(uintptr_t node, struct ThreadCB *thread)
{
	struct ThreadCB *head = (struct ThreadCB *)node;

	thread->next = node;
	thread->prev = head->prev;
	head->prev = (uintptr_t)thread;
	((struct ThreadCB *)thread->prev)->next = (uintptr_t)thread;
}

static inline void unlinkThread(struct ThreadCB *thread)
{
	uintptr_t next = thread->next;

	((struct ThreadCB *)next)->prev = thread->prev;
	((struct ThreadCB *)thread->prev)->next = next;
}

/* the manager's list at head takes thread */
#define listManagerThread(head, thread) do {					\
	ksceKernelSpinlockLowLock(						\
		&s_threadmgrRuntime.manager->threadMgrCBLock); \
	insertBefore((head), (thread));						\
	ksceKernelSpinlockLowUnlock(						\
		&s_threadmgrRuntime.manager->threadMgrCBLock); \
} while (0)

/* thread waits on cpu's standby list for cpu to take it */
void addStandby(struct ThreadMgrCpuCB *state, struct ThreadCB *thread)
{
	struct ThreadMgrCpuCB *cpu = state;

	SCE_DBG_ASSERT_CHECK(ThreadmgrIntrDisabled(), 1300);
	SCE_DBG_ASSERT_CHECK(thread->lock != 0, 1303);
	ksceKernelSpinlockLowLock(&cpu->standbyLock);
	insertBefore((uintptr_t)&cpu->standbyNext, thread);
	ksceKernelSpinlockLowUnlock(&cpu->standbyLock);
}

/* thread joins the end of its priority's ready queue */
void addReady(struct ThreadCB *thread)
{
	struct ThreadCB *context = thread;
	struct ThreadMgrCpuCB *cpu;
	ThreadmgrQueueProbe probe;
	int priority, shared;

	SCE_DBG_ASSERT_CHECK(context->lock != 0, 1384);
	priority = context->currentPriority;
	if ((uint32_t)priority - PRIORITY_SHARED_LOW < PRIORITY_SHARED_RANGE) {
		/* the DTrace probe site __sched_:enqueue */
		__asm__ volatile(".global s_schedGlobalEnqueueCodeSite\ns_schedGlobalEnqueueCodeSite:");
		probe = (ThreadmgrQueueProbe)s_schedGlobalEnqueueHandler;
		if (probe != NULL) {
			probe(&s_schedGlobalEnqueueDescriptor, probeThreadId(context),
			      context->process->pid, CPU_ID_GLOBAL);
			priority = context->currentPriority;
		}
		shared = priority - PRIORITY_SHARED_LOW;
		ksceKernelSpinlockLowLock(&s_threadmgrRuntime.manager->globalReadyLock);
		s_threadmgrRuntime.manager->globalReadyBitmap[shared / 32] |=
			0x80000000u >> (shared % 32);
		insertBefore((uintptr_t)&s_threadmgrRuntime.manager->globalReadyQueue[shared], context);
		ksceKernelSpinlockLowUnlock(&s_threadmgrRuntime.manager->globalReadyLock);
		context->currentCpuId = CPU_ID_GLOBAL;
		return;
	}
	/* the DTrace probe site __sched_:enqueue */
	__asm__ volatile(".global s_schedCpuEnqueueCodeSite\ns_schedCpuEnqueueCodeSite:");
	cpu = currentCpuState();
	probe = (ThreadmgrQueueProbe)s_schedCpuEnqueueHandler;
	if (probe != NULL)
		probe(&s_schedCpuEnqueueDescriptor, probeThreadId(context),
		      context->process->pid, cpuIndex(cpu));
	priority = context->currentPriority;
	cpu->readyBitmap[priority / 32] |= 0x80000000u >> (priority % 32);
	insertBefore((uintptr_t)&cpu->readyQueue[priority], context);
	context->currentCpuId = ksceKernelCpuId();
}

/* this CPU takes the threads on its standby list into its queues; returns how many */
unsigned int moveStandbyToReady(void)
{
	struct ThreadMgrCpuCB *cpu = currentCpuState();
	struct ThreadCB *thread;
	volatile unsigned int delay;
	unsigned int count = 0;

	SCE_DBG_ASSERT_CHECK(ThreadmgrIntrDisabled(), 1330);
	for (;;) {
		ksceKernelSpinlockLowLock(&cpu->standbyLock);
		thread = (struct ThreadCB *)cpu->standbyNext;
		while (thread != (struct ThreadCB *)&cpu->standbyNext) {
			SCE_DBG_ASSERT_CHECK(thread->magic == THREAD_CB_MAGIC, 1343);
			if (ksceKernelSpinlockLowTrylock(THREAD_LOCK(thread)) < 0)
				break;
			unlinkThread(thread);
			ksceKernelSpinlockLowUnlock(&cpu->standbyLock);
			SCE_DBG_ASSERT_CHECK(thread->currentCpuId == (int)ksceKernelCpuId(), 1356);
			thread->status = (thread->status & THREAD_STATE_HIGH_MASK) |
					     SCE_KERNEL_THREAD_STATUS_READY;
			addReady((struct ThreadCB *)thread);
			if (thread->currentPriority < cpu->priority) {
				SCE_DBG_ASSERT_LEVEL(0, cpu == currentCpuState(), 1363);
				cpu->nextRunningThread = 0;
			}
			count++;
			ksceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
			ksceKernelSpinlockLowLock(&cpu->standbyLock);
			thread = (struct ThreadCB *)cpu->standbyNext;
		}
		ksceKernelSpinlockLowUnlock(&cpu->standbyLock);
		if (thread == (struct ThreadCB *)&cpu->standbyNext)
			return count;
		/* a thread was locked by someone: again shortly */
		for (delay = STANDBY_RETRY_DELAY; delay != 0; delay--)
			;
	}
}

/* thread goes to the front of its priority's ready queue (it was preempted) */
void addReadyTop(struct ThreadCB *thread)
{
	struct ThreadCB *context = thread;
	struct ThreadMgrCpuCB *cpu;
	ThreadmgrQueueProbe probe;
	int priority, shared;

	SCE_DBG_ASSERT_CHECK(context->lock != 0, 1430);
	priority = context->currentPriority;
	if ((uint32_t)priority - PRIORITY_SHARED_LOW < PRIORITY_SHARED_RANGE) {
		/* the DTrace probe site __sched_:enqueue */
		__asm__ volatile(".global s_schedGlobalEnqueueTopCodeSite\n"
				 "s_schedGlobalEnqueueTopCodeSite:");
		probe = (ThreadmgrQueueProbe)s_schedGlobalEnqueueTopHandler;
		if (probe != NULL) {
			probe(&s_schedGlobalEnqueueTopDescriptor, probeThreadId(context),
			      context->process->pid, CPU_ID_GLOBAL);
			priority = context->currentPriority;
		}
		shared = priority - PRIORITY_SHARED_LOW;
		ksceKernelSpinlockLowLock(&s_threadmgrRuntime.manager->globalReadyLock);
		s_threadmgrRuntime.manager->globalReadyBitmap[shared / 32] |=
			0x80000000u >> (shared % 32);
		insertBefore(s_threadmgrRuntime.manager->globalReadyQueue[shared].next, context);
		ksceKernelSpinlockLowUnlock(&s_threadmgrRuntime.manager->globalReadyLock);
		context->currentCpuId = CPU_ID_GLOBAL;
		return;
	}
	/* the DTrace probe site __sched_:enqueue */
	__asm__ volatile(".global s_schedCpuEnqueueTopCodeSite\ns_schedCpuEnqueueTopCodeSite:");
	cpu = currentCpuState();
	probe = (ThreadmgrQueueProbe)s_schedCpuEnqueueTopHandler;
	if (probe != NULL)
		probe(&s_schedCpuEnqueueTopDescriptor, probeThreadId(context),
		      context->process->pid, cpuIndex(cpu));
	priority = context->currentPriority;
	cpu->readyBitmap[priority / 32] |= 0x80000000u >> (priority % 32);
	insertBefore(cpu->readyQueue[priority].next, context);
	context->currentCpuId = ksceKernelCpuId();
}

/* thread leaves the global queue of priority (the global lock held) */
void removeCommonRaw(struct ThreadCB *thread, int priority)
{
	struct ThreadCB *context = thread;
	struct ThreadMgrCpuCB *cpu;
	ThreadmgrQueueProbe probe;
	int shared;

	SCE_DBG_ASSERT_CHECK(context->lock != 0, 1471);
	SCE_DBG_ASSERT_CHECK(s_threadmgrRuntime.manager->globalReadyLock != 0, 1474);
	if (context->next == context->prev && context->next != (uintptr_t)context) {
		/* the last of its queue */
		shared = priority - PRIORITY_SHARED_LOW;
		s_threadmgrRuntime.manager->globalReadyBitmap[shared / 32] &=
			~(0x80000000u >> (shared % 32));
	}
	unlinkThread(context);
	/* the DTrace probe site __sched_:dequeue */
	__asm__ volatile(".global s_schedGlobalDequeueCodeSite\ns_schedGlobalDequeueCodeSite:");
	probe = (ThreadmgrQueueProbe)s_schedGlobalDequeueHandler;
	if (probe != NULL) {
		cpu = currentCpuState();
		probe(&s_schedGlobalDequeueDescriptor, probeThreadId(context),
		      context->process->pid, cpuIndex(cpu));
	}
}

/* thread leaves its ready queue (priority: the queue it is on) */
void removeReady(struct ThreadCB *thread, int priority)
{
	struct ThreadCB *context = thread;
	struct ThreadMgrCpuCB *cpu;
	ThreadmgrQueueProbe probe;

	SCE_DBG_ASSERT_CHECK(context->lock != 0, 1500);
	if ((uint32_t)context->currentPriority - PRIORITY_SHARED_LOW < PRIORITY_SHARED_RANGE) {
		ksceKernelSpinlockLowLock(&s_threadmgrRuntime.manager->globalReadyLock);
		removeCommonRaw(thread, priority);
		ksceKernelSpinlockLowUnlock(&s_threadmgrRuntime.manager->globalReadyLock);
		return;
	}
	cpu = currentCpuState();
	if (context->next == context->prev && context->next != (uintptr_t)context)
		cpu->readyBitmap[priority / 32] &= ~(0x80000000u >> (priority % 32));
	unlinkThread(context);
	/* the DTrace probe site __sched_:dequeue */
	__asm__ volatile(".global s_schedCpuDequeueCodeSite\ns_schedCpuDequeueCodeSite:");
	probe = (ThreadmgrQueueProbe)s_schedCpuDequeueHandler;
	if (probe != NULL)
		probe(&s_schedCpuDequeueDescriptor, probeThreadId(context),
		      context->process->pid, cpuIndex(cpu));
}

/* the CPU thread should run on, of the ones it may (UINT32_MAX: none for now) */
int selectExecCpu(struct ThreadCB *thread)
{
	struct SceKernelThreadmgrPrivate *manager = s_threadmgrRuntime.manager;
	struct ThreadMgrCpuCB *state;
	SceProcessObject *process;
	uint32_t affinity, mask, preferred, sameProcess, lastCpu, rotation, selected;
	int bestPriority, priority;
	unsigned int cpu;

	SCE_DBG_ASSERT_CHECK(thread->lock != 0, 1547);
	process = thread->process;
	if (process->threadsStopped != 0 && !(thread->flags >> THREAD_FLAG_REQUEST_SHIFT))
		return -1;
	affinity = thread->currentCpuAffinityMask;
	if (affinity == 0)
		affinity = process->cpuAffinity;
	mask = s_threadmgrRuntime.manager->activeCpuMask & affinity & CPU_MASK_ALL;
	if (((mask - 1) & mask) == 0)
		return 31 - leadingZeros(mask);

	/* the CPUs running the least urgent threads */
	preferred = 0;
	bestPriority = thread->currentPriority + 1;
	for (cpu = 0, state = manager->cpuCB; cpu < CPU_COUNT; cpu++, state++) {
		if (!(mask & (1u << cpu)))
			continue;
		priority = state->priority;
		if (bestPriority > priority)
			continue;
		if (bestPriority != priority)
			preferred = 0;
		preferred += 1u << cpu;
		bestPriority = priority;
	}
	if (preferred != 0) {
		if (((preferred - 1) & preferred) == 0)
			return 31 - leadingZeros(preferred);
		/* of those, the ones running its process */
		sameProcess = preferred;
		for (cpu = 0, state = manager->cpuCB; cpu < CPU_COUNT; cpu++, state++) {
			if (state->processId != thread->processId)
				sameProcess &= ~(1u << cpu);
		}
		if (((sameProcess - 1) & sameProcess) == 0) {
			if (sameProcess != 0)
				return 31 - leadingZeros(sameProcess);
			sameProcess = preferred;
		}
		mask = sameProcess;
	}
	lastCpu = thread->lastExecutedCpuId;
	if (lastCpu != UINT32_MAX && ((mask >> lastCpu) & 1))
		return lastCpu;
	/* round robin */
	rotation = s_threadmgrRuntime.manager->lastSelectedCpu;
	selected = (rotation - 1 - __builtin_clz(rotateRight(mask, rotation))) & (CPU_COUNT - 1);
	s_threadmgrRuntime.manager->lastSelectedCpu = selected;
	return selected;
}

/*
 * thread is ready to run on cpuId (selectExecCpu's choice): queued here, on
 * the global queues or that CPU's standby list; a debug-suspended thread
 * and one no CPU may run are listed aside. Returns whether this CPU should
 * switch now.
 */
bool distributeThread(int cpuId, struct ThreadCB *thread)
{
	struct ThreadCB *context = thread;
	struct ThreadMgrCpuCB *current = currentCpuState();
	struct SceKernelThreadmgrPrivate *manager = s_threadmgrRuntime.manager;
	struct ThreadMgrCpuCB *target;
	bool reschedule = false;

	SCE_DBG_ASSERT_CHECK(ThreadmgrIntrDisabled(), 1677);
	SCE_DBG_ASSERT_CHECK(context->lock != 0, 1680);
	SCE_DBG_ASSERT_CHECK(!(context->flags & THREAD_FLAG_EXCEPTION), 1682);
	if (cpuId == -1) {
		/* stagnant: no active CPU may run it */
		context->currentCpuId = -1;
		context->status = (context->status & THREAD_STATE_HIGH_MASK) |
				      SCE_KERNEL_THREAD_STATUS_STAGNANT;
		listManagerThread((uintptr_t)&s_threadmgrRuntime.manager->stagnantNext, context);
		goto out;
	}
	if ((int32_t)context->status < 0 && !(context->flags & THREAD_FLAG_EXITING) &&
	    (context->userFrame == NULL || (context->flags & THREAD_FLAG_SELF_STOPPED))) {
		/* debug-suspended */
		context->status = ((context->status << 16) & 0x7fffffff) |
				      THREAD_STATE_DEBUG_SUSPENDED | THREAD_STATE_OFF_CPU;
		listManagerThread((uintptr_t)&s_threadmgrRuntime.manager->debugSuspendedNext,
				  context);
		context->currentCpuId = -1;
		goto out;
	}
	if ((int)ksceKernelCpuId() == cpuId) {
		context->status = (context->status & THREAD_STATE_HIGH_MASK) |
				      SCE_KERNEL_THREAD_STATUS_READY;
		addReady(thread);
		reschedule = context->currentPriority < current->priority;
		if (reschedule) {
			SCE_DBG_ASSERT_LEVEL(0, current == currentCpuState(), 1719);
			current->nextRunningThread = 0;
		}
		SCE_DBG_ASSERT_CHECK(context->currentCpuId == (int)ksceKernelCpuId() ||
				     context->currentCpuId == CPU_ID_GLOBAL, 1725);
		goto out;
	}
	target = &manager->cpuCB[cpuId];
	if ((uint32_t)context->currentPriority - PRIORITY_SHARED_LOW < PRIORITY_SHARED_RANGE) {
		context->status = (context->status & THREAD_STATE_HIGH_MASK) |
				      SCE_KERNEL_THREAD_STATUS_READY;
		addReady(thread);
		if (context->currentPriority < target->priority)
			addIntrRequest(cpuId, INTR_MASK_SCHEDULING);
		SCE_DBG_ASSERT_CHECK(context->currentCpuId == CPU_ID_GLOBAL, 1734);
	} else {
		context->status = (context->status & THREAD_STATE_HIGH_MASK) |
				      SCE_KERNEL_THREAD_STATUS_STANDBY;
		addStandby((struct ThreadMgrCpuCB *)target, thread);
		if (context->currentPriority < target->priority)
			addIntrRequest(cpuId, INTR_MASK_SCHEDULING);
		context->currentCpuId = cpuId;
	}
out:
	return reschedule;
}
