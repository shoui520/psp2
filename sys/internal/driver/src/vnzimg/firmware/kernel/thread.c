/*
 * Threads: creation, the run queues, waiting and waking, event flags.
 *
 * A thread is changed under its core's run-queue semaphore (vnzKernelLockThread), with
 * dispatching disabled on the calling core and the core marked in the kernel, so
 * interrupt events wait (vnzKernelProcessEvents passes them on afterwards). Readying a
 * thread of a higher priority than the one its core will run next makes it the
 * next; when that core is another, its doorbell is rung (vnzKernelRingCore).
 *
 * A higher number is a higher priority; the idle thread, 0, runs when no other
 * can.
 */
#include "api.h"

/* the frame vnzKernelSwitchContext saves (kernel/switch.S) */
typedef struct VnzKernelSwitchFrame {
	uint32_t r5, r6, r7, r8, tp;
	void *lp;
} VnzKernelSwitchFrame;

/* the stand-in for an ending thread's context, which is not kept */
typedef struct VnzKernelScratchContext {
	uint32_t reserved[4];
	uint32_t savedDispatchDisable;
	uint32_t entry;
	uint32_t savedSp;
} VnzKernelScratchContext;

VnzKernelThread g_vnzKernelIdleThreads[VNZ_CORES] VNZ_KERNEL_SHARED;
VnzKernelQueue g_vnzKernelRunQueues[VNZ_CORES][VNZ_KERNEL_PRIORITIES] VNZ_KERNEL_SHARED;
uint32_t g_vnzKernelThreadCounts[VNZ_CORES] VNZ_KERNEL_SHARED;
VnzKernelThread *g_vnzKernelNextThreads[VNZ_CORES] VNZ_KERNEL_SHARED;
uint32_t g_vnzKernelExitedThreads VNZ_KERNEL_SHARED;
uint32_t g_vnzKernelExitedLimit VNZ_KERNEL_SHARED;

/* Take the run-queue semaphore of the thread's core. */
void vnzKernelLockThread(VnzKernelThread *thread)
{
	for (;;) {
		uint32_t core = thread->core;
		volatile uint32_t *semaphore = vnzKernelSemaphore(VNZ_KERNEL_SEMAPHORE_THREADS + core);
		uint32_t i;

		if (vnzKernelTrySemaphore(semaphore) == 0) {
			if (thread->core == core)
				return;
			vnzKernelGiveSemaphore(semaphore);
		}
		for (i = 0; i < 10; i++)
			;
	}
}

void vnzKernelUnlockThread(VnzKernelThread *thread)
{
	vnzKernelGiveSemaphore(vnzKernelSemaphore(VNZ_KERNEL_SEMAPHORE_THREADS + thread->core));
}

/* Switch to the core's next thread, if that is another. Interrupts are off. */
void vnzKernelReschedule(VnzKernelCore *core)
{
	VnzKernelThread *current = core->current;
	VnzKernelThread *next = g_vnzKernelNextThreads[core->index];

	if (next == current)
		return;
	if ((current->state & VNZ_KERNEL_STATE_MASK) == VNZ_KERNEL_STATE_RUNNING)
		current->state = (current->state & ~VNZ_KERNEL_STATE_MASK) | VNZ_KERNEL_STATE_READY;
	next->state = (next->state & ~VNZ_KERNEL_STATE_MASK) | VNZ_KERNEL_STATE_RUNNING;
	core->current = next;
	vnzKernelTrace(VNZ_KERNEL_TRACE_SWITCH, (uint32_t)current, (uint32_t)next,
		(uint32_t)next->entry,
		VNZ_KERNEL_TRACE_THREADS);
	vnzKernelSwitchContext(core, current, next);
}

/* Have a core reschedule: this one when dispatching allows, another by its doorbell. */
void vnzKernelRingCore(VnzKernelCore *core, uint32_t current, uint32_t target)
{
	if (target == current)
		core->reschedulePending = 1;
	else
		vnzRegister(VNZ_DOORBELLS + 0x20 * current + 0x10) = 1u << target;
}

/* the highest queued thread of a core at or below a priority, or NULL */
static VnzKernelThread *highestReady(uint32_t core, uint32_t priority)
{
	VnzKernelQueue *queues = g_vnzKernelRunQueues[core];
	int32_t i;

	for (i = (int32_t)priority - 1; i >= 0; i--)
		if (queues[i].head)
			return queues[i].head;
	return NULL;
}

/*
 * Create a thread of a core at a priority, its stack from a heap. It is dormant
 * until vnzKernelThreadStart. The calling thread is its parent, unless that is the idle
 * thread.
 */
int32_t vnzKernelThreadCreate(void *entry, uint32_t coreId, uint32_t priority, uint32_t stackSize,
	VnzKernelHeap *heap)
{
	uint32_t index;
	VnzKernelCore *core;
	VnzKernelThread *thread;
	void *stack;

	vnzKernelDisableDispatch();
	core = vnzKernelCurrentCore(&index);
	thread = vnzKernelThreadAllocate();
	if (!thread) {
		vnzKernelEnableDispatch();
		return -1;
	}
	stackSize = (stackSize + 0x3f) & ~0x3fu;
	stack = vnzKernelHeapAllocateStack(heap, stackSize);
	if (!stack) {
		vnzKernelThreadFree(thread);
		vnzKernelEnableDispatch();
		return -1;
	}

	thread->physical = (VnzKernelThread *)vnzKernelPublicToPhysical((uint32_t)thread);
	thread->heap = heap;
	thread->stackSize = stackSize;
	thread->stack = stack;
	thread->core = coreId - VNZ_FIRST_CORE_ID;
	thread->priority = priority;
	thread->flags = 0;
	thread->waitFlags = 0;
	thread->exitNotify = NULL;
	thread->savedDispatchDisable = 0;
	thread->parent = core->idle == core->current ? NULL : core->current;
	thread->state = VNZ_KERNEL_STATE_DORMANT;
	thread->userWord = 0;
	thread->interrupts = 0;
	thread->local[2] = 0;
	thread->local[1] = 0;
	thread->local[0] = 0;
	thread->attributes = 0;
	thread->joining = 0;
	thread->children = 0;
	thread->entry = entry;
	thread->magic = VNZ_KERNEL_THREAD_MAGIC;

	core->inKernel = 1;
	if (thread->parent) {
		vnzKernelLockThread(thread->parent);
		thread->parent->children++;
		vnzKernelUnlockThread(thread->parent);
	}
	vnzKernelLockThread(thread);
	g_vnzKernelThreadCounts[thread->core]++;
	vnzKernelUnlockThread(thread);
	vnzKernelProcessEvents(core);
	vnzKernelTrace(VNZ_KERNEL_TRACE_CREATE, (uint32_t)thread, (uint32_t)entry, coreId,
		VNZ_KERNEL_TRACE_THREADS);
	vnzKernelEnableDispatch();
	return (int32_t)thread;
}

/* vnzKernelThreadCreate, the stack from the calling thread's heap */
int32_t vnzKernelThreadCreateHere(void *entry, uint32_t coreId, uint32_t priority,
	uint32_t stackSize)
{
	uint32_t index;
	VnzKernelHeap *heap;

	vnzKernelDisableDispatch();
	vnzKernelCurrentCore(&index);
	heap = g_vnzKernelCores[index].current->heap;
	vnzKernelEnableDispatch();
	return vnzKernelThreadCreate(entry, coreId, priority, stackSize, heap);
}

/* Start a dormant thread at its entry with two arguments. */
int32_t vnzKernelThreadStart(VnzKernelThread *thread, uint32_t argument, uint32_t argument2)
{
	uint32_t index, core;
	VnzKernelCore *self;
	VnzKernelThread *parent;
	uint32_t top;

	vnzKernelDisableDispatch();
	self = vnzKernelCurrentCore(&index);
	self->inKernel = 1;
	vnzKernelLockThread(thread);
	core = thread->core;
	if ((thread->state & VNZ_KERNEL_STATE_MASK) != VNZ_KERNEL_STATE_DORMANT) {
		vnzKernelUnlockThread(thread);
		vnzKernelProcessEvents(self);
		vnzKernelEnableDispatch();
		return -1;
	}

	vnzMemorySync((uint32_t)thread->stack, thread->stackSize, self->current);
	top = (uint32_t)thread->stack + thread->stackSize - 0x40;
	vnzCacheLine(VNZ_CACHE_INVALIDATE, top);
	thread->savedSp = vnzKernelInitialFrame(top, thread->entry, argument, argument2);
	vnzCacheLine(VNZ_CACHE_WRITEBACK, top);
	thread->state = (thread->state & ~VNZ_KERNEL_STATE_MASK) | VNZ_KERNEL_STATE_READY;
	vnzMemorySync((uint32_t)thread->stack, thread->stackSize, thread);
	if (core != index)
		mep_syncm();

	if (core != VNZ_KERNEL_NO_CORE) {
		uint32_t priority = thread->priority;

		vnzKernelQueueAppend(&g_vnzKernelRunQueues[core][priority - 1], thread);
		if (g_vnzKernelNextThreads[core]->priority < priority) {
			g_vnzKernelNextThreads[core] = thread;
			vnzKernelRingCore(self, index, core);
		}
	}
	vnzKernelUnlockThread(thread);

	parent = thread->parent;
	if (parent) {
		vnzKernelLockThread(parent);
		parent->joining++;
		vnzKernelUnlockThread(parent);
	}
	vnzKernelProcessEvents(self);
	vnzKernelEnableDispatch();
	return 0;
}

/*
 * Take the running thread off its core and pick the next one; on the idle
 * thread's stack, vnzKernelThreadFinish ends it (kernel/switch.S vnzKernelEndThread).
 */
static void leaveCore(VnzKernelCore *core, uint32_t index, VnzKernelThread *thread, uint32_t state)
{
	VnzKernelThread *next;

	vnzKernelQueueRemove(&g_vnzKernelRunQueues[index][thread->priority - 1], thread);
	thread->state = (thread->state & ~VNZ_KERNEL_STATE_MASK) | state;
	next = highestReady(index, g_vnzKernelNextThreads[index]->priority);
	g_vnzKernelNextThreads[index] = next ? next : core->idle;
	vnzKernelEndThread(thread, core, core->idle->savedSp);
}

/* End the calling thread; it goes dormant. */
void vnzKernelThreadExit(void)
{
	uint32_t index;
	VnzKernelCore *core;
	VnzKernelThread *thread;

	vnzKernelDisableDispatch();
	core = vnzKernelCurrentCore(&index);
	thread = core->current;
	core->inKernel = 1;
	vnzKernelLockThread(thread);
	leaveCore(core, index, thread, VNZ_KERNEL_STATE_DORMANT);
}

/*
 * End the calling thread with an exit status, kept until it is collected
 * (vnzKernelThreadExitStatus, vnzKernelThreadDelete). Too many uncollected ends stop the
 * kernel.
 */
void vnzKernelThreadExitWithStatus(uint32_t status, uint32_t status2)
{
	uint32_t index;
	VnzKernelCore *core;
	VnzKernelThread *thread;
	uint32_t exited;

	vnzKernelDisableDispatch();
	core = vnzKernelCurrentCore(&index);
	thread = core->current;
	core->inKernel = 1;
	vnzKernelLockThread(thread);
	thread->exitStatus[0] = status;
	thread->exitStatus[1] = status2;

	vnzKernelTakeSemaphore(vnzKernelSemaphore(VNZ_KERNEL_SEMAPHORE_EXITS));
	exited = ++g_vnzKernelExitedThreads;
	if (g_vnzKernelExitedLimit != 0xffffffff && exited >= g_vnzKernelExitedLimit)
		vnzKernelExceptionTooManyExited();
	vnzKernelGiveSemaphore(vnzKernelSemaphore(VNZ_KERNEL_SEMAPHORE_EXITS));

	leaveCore(core, index, thread, VNZ_KERNEL_STATE_EXITED);
}

/* Free a thread's stack and the thread. */
void vnzKernelThreadRelease(VnzKernelThread *thread)
{
	void *stack = thread->stack;
	uint32_t size = thread->stackSize;

	vnzMemorySync((uint32_t)stack, size, vnzKernelThreadCurrent());
	vnzKernelHeapFreeStack(thread->heap, stack, size);
	vnzKernelThreadFree(thread);
}

/*
 * The rest of a thread's end, on the core's idle stack: tell whoever waits, free
 * what nobody can refer to any more, and switch to the next thread.
 */
void vnzKernelThreadFinish(VnzKernelThread *thread, VnzKernelCore *core)
{
	uint32_t index = core->index;
	uint32_t exited, notify = 0, freeStack, freeThread = 0;
	VnzKernelThread *parent, *next;
	VnzKernelScratchContext scratch;

	thread->savedDispatchDisable = core->dispatchDisable - 1;
	exited = (thread->state & VNZ_KERNEL_STATE_MASK) == VNZ_KERNEL_STATE_EXITED;
	if (exited)
		notify = thread->exitNotify != NULL;
	vnzKernelCachePurge((uint32_t)thread->stack, thread->stackSize);

	parent = thread->parent;
	if (parent || thread->children) {
		freeStack = 0;
	} else {
		freeStack = 1;
		if (!exited) {
			/* nothing refers to it: it goes */
			g_vnzKernelThreadCounts[index]--;
			freeThread = 1;
			thread->state &= ~VNZ_KERNEL_STATE_MASK;
		}
	}
	vnzKernelUnlockThread(thread);

	if (notify)
		vnzKernelThreadSetFlags(thread->exitNotify, thread->exitNotifyFlags);
	if (freeStack) {
		vnzMemorySync((uint32_t)thread->stack, thread->stackSize, vnzKernelThreadCurrent());
		vnzKernelHeapFreeStack(thread->heap, thread->stack, thread->stackSize);
	}
	if (freeThread)
		vnzKernelThreadFree(thread);

	if (parent) {
		vnzKernelLockThread(parent);
		mep_syncm();
		parent->joining--;
		if ((parent->state & (VNZ_KERNEL_WAIT_MASK | VNZ_KERNEL_STATE_MASK)) == (VNZ_KERNEL_WAIT_CHILDREN | VNZ_KERNEL_STATE_WAITING) &&
		    vnzKernelMakeReady(parent, 0))
			vnzKernelRingCore(core, index, parent->core);
		vnzKernelUnlockThread(parent);
	}
	vnzKernelProcessEvents(core);

	vnzDisableInterrupts();
	next = g_vnzKernelNextThreads[index];
	next->state = (next->state & ~VNZ_KERNEL_STATE_MASK) | VNZ_KERNEL_STATE_RUNNING;
	core->current = next;
	vnzKernelTrace(VNZ_KERNEL_TRACE_SWITCH, (uint32_t)thread, (uint32_t)next,
		(uint32_t)next->entry,
		VNZ_KERNEL_TRACE_THREADS);
	vnzKernelSwitchContext(core, (VnzKernelThread *)&scratch, next);
}

/*
 * Delete a thread that has ended and has no children of its own; an exited
 * thread's status is dropped. A thread serving interrupts keeps its memory.
 */
int32_t vnzKernelThreadDelete(VnzKernelThread *thread)
{
	uint32_t index;
	VnzKernelCore *core;
	uint32_t state, release = 0;
	int32_t result = -1;

	vnzKernelDisableDispatch();
	core = vnzKernelCurrentCore(&index);
	core->inKernel = 1;
	vnzKernelLockThread(thread);
	state = thread->state & VNZ_KERNEL_STATE_MASK;
	if ((state == VNZ_KERNEL_STATE_DORMANT || state == VNZ_KERNEL_STATE_EXITED) && !thread->children) {
		VnzKernelThread *parent;

		if ((int8_t)thread->state == VNZ_KERNEL_STATE_EXITED) {
			vnzKernelTakeSemaphore(vnzKernelSemaphore(VNZ_KERNEL_SEMAPHORE_EXITS));
			g_vnzKernelExitedThreads--;
			vnzKernelGiveSemaphore(vnzKernelSemaphore(VNZ_KERNEL_SEMAPHORE_EXITS));
		}
		thread->state &= ~VNZ_KERNEL_STATE_MASK;
		if (thread->attributes != VNZ_KERNEL_THREAD_MOVING) {
			g_vnzKernelThreadCounts[thread->core]--;
			release = 1;
		}
		parent = thread->parent;
		vnzKernelUnlockThread(thread);
		if (parent) {
			vnzKernelLockThread(parent);
			parent->children--;
			vnzKernelUnlockThread(parent);
		}
		if (release)
			vnzKernelThreadRelease(thread);
		result = 0;
	} else {
		vnzKernelUnlockThread(thread);
	}
	vnzKernelProcessEvents(core);
	vnzKernelEnableDispatch();
	return result;
}

/* Change a thread's priority; a ready or running one moves in its run queue. */
void vnzKernelThreadSetPriority(VnzKernelThread *thread, uint32_t priority)
{
	uint32_t index, core, state, old, nextPriority;
	VnzKernelCore *self;
	VnzKernelQueue *queues;
	VnzKernelThread *next;

	vnzKernelDisableDispatch();
	self = vnzKernelCurrentCore(&index);
	self->inKernel = 1;
	vnzKernelLockThread(thread);
	core = thread->core;
	old = thread->priority;
	if (old == priority)
		goto done;
	state = thread->state & VNZ_KERNEL_STATE_MASK;
	if (state == VNZ_KERNEL_STATE_EXITED)
		goto done;
	if (state == VNZ_KERNEL_STATE_WAITING || state == VNZ_KERNEL_STATE_DORMANT ||
	    thread->attributes == VNZ_KERNEL_THREAD_MOVING) {
		thread->priority = priority;
		goto done;
	}

	/* ready or running: requeue it */
	queues = g_vnzKernelRunQueues[core];
	next = g_vnzKernelNextThreads[core];
	nextPriority = next->priority;
	vnzKernelQueueRemove(&queues[old - 1], thread);
	thread->priority = priority;
	vnzKernelQueueAppend(&queues[priority - 1], thread);
	if (g_vnzKernelNextThreads[core]->priority < priority) {
		g_vnzKernelNextThreads[core] = thread;
	} else {
		int32_t i;

		if (g_vnzKernelNextThreads[core] != thread || priority >= nextPriority)
			goto done;
		/* it was to run next and drops: the highest above its new priority */
		for (i = nextPriority - 1; i >= (int32_t)priority && !queues[i].head; i--)
			;
		g_vnzKernelNextThreads[core] = queues[i].head;
		if (queues[i].head == thread)
			goto done;
		thread->state = (thread->state & ~VNZ_KERNEL_STATE_MASK) | VNZ_KERNEL_STATE_READY;
	}
	vnzKernelRingCore(self, index, core);
done:
	vnzKernelUnlockThread(thread);
	vnzKernelProcessEvents(self);
	vnzKernelEnableDispatch();
}

/* Give the core to the next thread of the same priority. */
void vnzKernelThreadYield(void)
{
	uint32_t index, priority, reschedule;
	VnzKernelCore *core;
	VnzKernelThread *thread;
	VnzKernelQueue *queue;

	vnzKernelDisableDispatch();
	core = vnzKernelCurrentCore(&index);
	thread = core->current;
	core->inKernel = 1;
	vnzKernelLockThread(thread);
	priority = thread->priority;
	queue = &g_vnzKernelRunQueues[index][priority - 1];
	vnzKernelQueueRemove(queue, thread);
	vnzKernelQueueAppend(queue, thread);
	if (priority < g_vnzKernelNextThreads[index]->priority) {
		reschedule = 1;
	} else if (queue->head == thread) {
		reschedule = 0;
	} else {
		thread->state = (thread->state & ~VNZ_KERNEL_STATE_MASK) | VNZ_KERNEL_STATE_READY;
		g_vnzKernelNextThreads[index] = queue->head;
		reschedule = 1;
	}
	vnzKernelUnlockThread(thread);
	vnzKernelProcessEvents(core);
	if (reschedule) {
		vnzDisableInterrupts();
		vnzKernelReschedule(core);
		vnzEnableInterrupts();
	}
	vnzKernelEnableDispatch();
}

/* Make a locked thread wait, and pick the next thread of its core. */
static void blockThread(VnzKernelThread *thread, uint32_t wait)
{
	uint32_t core = thread->core;
	VnzKernelThread *next;

	thread->state = (thread->state & ~VNZ_KERNEL_STATE_MASK) | wait | VNZ_KERNEL_STATE_WAITING;
	vnzKernelQueueRemove(&g_vnzKernelRunQueues[core][thread->priority - 1], thread);
	next = highestReady(core, g_vnzKernelNextThreads[core]->priority);
	g_vnzKernelNextThreads[core] = next ? next : g_vnzKernelCores[core].idle;
}

/* Wait until vnzKernelThreadWakeup; the value it was given. */
uint32_t vnzKernelThreadSleep(void)
{
	uint32_t index;
	VnzKernelCore *core;
	VnzKernelThread *thread;

	vnzKernelDisableDispatch();
	core = vnzKernelCurrentCore(&index);
	thread = core->current;
	core->inKernel = 1;
	vnzKernelLockThread(thread);
	blockThread(thread, VNZ_KERNEL_WAIT_SLEEP);
	vnzKernelUnlockThread(thread);
	vnzKernelProcessEvents(core);
	vnzDisableInterrupts();
	vnzKernelReschedule(core);
	vnzEnableInterrupts();
	vnzKernelEnableDispatch();
	return thread->waitResult;
}

/* Make a thread wait for event flag 0 during a suspend (kernel/power.c). */
void vnzKernelThreadHoldForResume(VnzKernelThread *thread)
{
	vnzKernelLockThread(thread);
	blockThread(thread, VNZ_KERNEL_WAIT_FLAGS);
	thread->waitFlags = 1;
	vnzKernelUnlockThread(thread);
}

/*
 * Ready a locked waiting thread with a result; nonzero when it is now the next
 * thread of its core (which then needs telling).
 */
uint32_t vnzKernelMakeReady(VnzKernelThread *thread, uint32_t result)
{
	uint32_t core = thread->core;

	thread->waitResult = result;
	thread->state = (thread->state & ~(VNZ_KERNEL_WAIT_MASK | VNZ_KERNEL_STATE_MASK)) | VNZ_KERNEL_STATE_READY;
	if (core == VNZ_KERNEL_NO_CORE)
		return 0;
	vnzKernelQueueAppend(&g_vnzKernelRunQueues[core][thread->priority - 1], thread);
	if (g_vnzKernelNextThreads[core]->priority < thread->priority) {
		g_vnzKernelNextThreads[core] = thread;
		return 1;
	}
	return 0;
}

/* Wake a sleeping thread with a value. */
int32_t vnzKernelThreadWakeup(VnzKernelThread *thread, uint32_t value)
{
	uint32_t index, core;
	VnzKernelCore *self;
	int32_t result = -1;

	vnzKernelDisableDispatch();
	self = vnzKernelCurrentCore(&index);
	self->inKernel = 1;
	vnzKernelLockThread(thread);
	core = thread->core;
	if ((thread->state & (VNZ_KERNEL_WAIT_MASK | VNZ_KERNEL_STATE_MASK)) == (VNZ_KERNEL_WAIT_SLEEP | VNZ_KERNEL_STATE_WAITING)) {
		if (core != index)
			mep_syncm();
		if (vnzKernelMakeReady(thread, value))
			vnzKernelRingCore(self, index, core);
		result = 0;
	}
	vnzKernelUnlockThread(thread);
	vnzKernelProcessEvents(self);
	vnzKernelEnableDispatch();
	return result;
}

/* Release a thread held by vnzKernelThreadHoldForResume. */
void vnzKernelThreadReleaseFromResume(VnzKernelThread *thread)
{
	vnzKernelLockThread(thread);
	if ((thread->state & (VNZ_KERNEL_WAIT_MASK | VNZ_KERNEL_STATE_MASK)) == (VNZ_KERNEL_WAIT_FLAGS | VNZ_KERNEL_STATE_WAITING)) {
		thread->flags = 0;
		thread->waitFlags = 0;
		vnzKernelMakeReady(thread, 0);
	}
	vnzKernelUnlockThread(thread);
}

/* Wait until the started children have ended; how many are still running. */
uint32_t vnzKernelThreadWaitChildren(void)
{
	uint32_t index, reschedule;
	VnzKernelCore *core;
	VnzKernelThread *thread;

	vnzKernelDisableDispatch();
	core = vnzKernelCurrentCore(&index);
	thread = core->current;
	core->inKernel = 1;
	vnzKernelLockThread(thread);
	if (thread->joining == 0) {
		reschedule = g_vnzKernelNextThreads[index] != thread;
	} else {
		blockThread(thread, VNZ_KERNEL_WAIT_CHILDREN);
		reschedule = 1;
	}
	vnzKernelUnlockThread(thread);
	vnzKernelProcessEvents(core);
	if (reschedule) {
		vnzDisableInterrupts();
		vnzKernelReschedule(core);
		vnzEnableInterrupts();
	}
	vnzKernelEnableDispatch();
	return thread->joining;
}

/*
 * Detach a thread from its parent; a dormant one without children of its own
 * goes. A parent waiting for it stops waiting for it.
 */
void vnzKernelThreadDetach(VnzKernelThread *thread)
{
	uint32_t index, core, state, release = 0;
	VnzKernelCore *self;
	VnzKernelThread *parent;

	vnzKernelDisableDispatch();
	self = vnzKernelCurrentCore(&index);
	self->inKernel = 1;
	vnzKernelLockThread(thread);
	core = thread->core;
	parent = thread->parent;
	thread->parent = NULL;
	state = thread->state & VNZ_KERNEL_STATE_MASK;
	if (state == VNZ_KERNEL_STATE_DORMANT && !(int16_t)thread->children) {
		thread->state &= ~VNZ_KERNEL_STATE_MASK;
		if (thread->attributes != VNZ_KERNEL_THREAD_MOVING) {
			g_vnzKernelThreadCounts[core]--;
			release = 1;
		}
	}
	vnzKernelUnlockThread(thread);

	if (parent) {
		vnzKernelLockThread(parent);
		parent->children--;
		if (state != VNZ_KERNEL_STATE_DORMANT && state != VNZ_KERNEL_STATE_EXITED) {
			parent->joining--;
			if ((parent->state & (VNZ_KERNEL_WAIT_MASK | VNZ_KERNEL_STATE_MASK)) ==
			    (VNZ_KERNEL_WAIT_CHILDREN | VNZ_KERNEL_STATE_WAITING) && vnzKernelMakeReady(parent, 0))
				vnzKernelRingCore(self, index, parent->core);
		}
		vnzKernelUnlockThread(parent);
	}
	if (release)
		vnzKernelThreadRelease(thread);
	vnzKernelProcessEvents(self);
	vnzKernelEnableDispatch();
}

/* Set event flags of a thread, waking it when it waits for one of them. */
void vnzKernelThreadSetFlags(VnzKernelThread *thread, uint32_t flags)
{
	uint32_t index, core, set;
	VnzKernelCore *self;

	vnzKernelDisableDispatch();
	self = vnzKernelCurrentCore(&index);
	self->inKernel = 1;
	vnzKernelLockThread(thread);
	core = thread->core;
	if (core != index)
		mep_syncm();
	set = thread->flags | flags;
	if ((thread->state & (VNZ_KERNEL_WAIT_MASK | VNZ_KERNEL_STATE_MASK)) == (VNZ_KERNEL_WAIT_FLAGS | VNZ_KERNEL_STATE_WAITING)) {
		uint32_t wanted = thread->waitFlags;

		if (set & wanted) {
			if (vnzKernelMakeReady(thread, set))
				vnzKernelRingCore(self, index, core);
			thread->waitFlags = 0;
			set &= ~wanted;
		}
	}
	thread->flags = set;
	vnzKernelUnlockThread(thread);
	vnzKernelProcessEvents(self);
	vnzKernelEnableDispatch();
}

/*
 * Wait until one of the event flags is set; the flags that were set. Those
 * waited for are cleared.
 */
uint32_t vnzKernelThreadWaitFlags(uint32_t flags)
{
	uint32_t index, set, reschedule;
	VnzKernelCore *core;
	VnzKernelThread *thread;

	vnzKernelDisableDispatch();
	core = vnzKernelCurrentCore(&index);
	thread = core->current;
	core->inKernel = 1;
	vnzKernelLockThread(thread);
	set = thread->flags;
	if (set & flags) {
		thread->waitResult = set;
		thread->waitFlags = 0;
		thread->flags = set & ~flags;
		reschedule = g_vnzKernelNextThreads[index] != thread;
	} else {
		blockThread(thread, VNZ_KERNEL_WAIT_FLAGS);
		thread->waitFlags = flags;
		reschedule = 1;
	}
	vnzKernelUnlockThread(thread);
	vnzKernelProcessEvents(core);
	if (reschedule) {
		vnzDisableInterrupts();
		vnzKernelReschedule(core);
		vnzEnableInterrupts();
	}
	vnzKernelEnableDispatch();
	return thread->waitResult;
}

/* Clear event flags of the calling thread; the flags that were set. */
uint32_t vnzKernelThreadClearFlags(uint32_t flags)
{
	uint32_t index, set;
	VnzKernelCore *core;
	VnzKernelThread *thread;

	vnzKernelDisableDispatch();
	core = vnzKernelCurrentCore(&index);
	thread = core->current;
	core->inKernel = 1;
	vnzKernelLockThread(thread);
	set = thread->flags;
	thread->flags = set & ~flags;
	vnzKernelUnlockThread(thread);
	vnzKernelProcessEvents(core);
	vnzKernelEnableDispatch();
	return set;
}

VnzKernelThread *vnzKernelThreadCurrent(void)
{
	VnzKernelThread *thread;

	vnzKernelDisableDispatch();
	thread = g_vnzKernelCores[vnzCoreIndex()].current;
	vnzKernelEnableDispatch();
	return thread;
}

/* the start of an inquiry about a thread, and its end */
static VnzKernelCore *beginInquiry(VnzKernelThread *thread, uint32_t *index)
{
	VnzKernelCore *core;

	vnzKernelDisableDispatch();
	core = vnzKernelCurrentCore(index);
	core->inKernel = 1;
	vnzKernelLockThread(thread);
	return core;
}

static void endInquiry(VnzKernelCore *core, VnzKernelThread *thread)
{
	vnzKernelUnlockThread(thread);
	vnzKernelProcessEvents(core);
	vnzKernelEnableDispatch();
}

uint32_t vnzKernelThreadPriority(VnzKernelThread *thread)
{
	uint32_t index, priority;
	VnzKernelCore *core = beginInquiry(thread, &index);

	priority = thread->priority;
	endInquiry(core, thread);
	return priority;
}

/* nonzero while the thread has not ended */
uint32_t vnzKernelThreadIsAlive(VnzKernelThread *thread)
{
	uint32_t index, state;
	VnzKernelCore *core = beginInquiry(thread, &index);

	state = thread->state & VNZ_KERNEL_STATE_MASK;
	endInquiry(core, thread);
	return state != VNZ_KERNEL_STATE_DORMANT && state != VNZ_KERNEL_STATE_EXITED;
}

uint32_t vnzKernelThreadHasExited(VnzKernelThread *thread)
{
	uint32_t index, exited;
	VnzKernelCore *core = beginInquiry(thread, &index);

	exited = (thread->state & VNZ_KERNEL_STATE_MASK) == VNZ_KERNEL_STATE_EXITED;
	endInquiry(core, thread);
	return exited;
}

/*
 * Have the calling thread's event flags set when a thread exits (at once when
 * it has). One thread may ask.
 */
int32_t vnzKernelThreadNotifyExit(VnzKernelThread *thread, uint32_t flags)
{
	uint32_t index, now = 0;
	VnzKernelCore *core = beginInquiry(thread, &index);
	VnzKernelThread *current = core->current;
	int32_t result = -1;

	if (!thread->exitNotify) {
		thread->exitNotify = current;
		thread->exitNotifyFlags = flags;
		if ((int8_t)thread->state == VNZ_KERNEL_STATE_EXITED)
			now = 1;
		result = 0;
	}
	endInquiry(core, thread);
	if (now == 1)
		vnzKernelThreadSetFlags(current, flags);
	return result;
}

int32_t vnzKernelThreadCancelExitNotify(VnzKernelThread *thread)
{
	uint32_t index;
	VnzKernelCore *core = beginInquiry(thread, &index);
	int32_t result = -1;

	if (thread->exitNotify) {
		thread->exitNotifyFlags = 0;
		thread->exitNotify = NULL;
		result = 0;
	}
	endInquiry(core, thread);
	return result;
}

int32_t vnzKernelThreadExitStatus(VnzKernelThread *thread, uint32_t *status, uint32_t *status2)
{
	uint32_t index;
	VnzKernelCore *core = beginInquiry(thread, &index);
	int32_t result = -1;

	if ((thread->state & VNZ_KERNEL_STATE_MASK) == VNZ_KERNEL_STATE_EXITED) {
		*status = thread->exitStatus[0];
		*status2 = thread->exitStatus[1];
		result = 0;
	}
	endInquiry(core, thread);
	return result;
}

uint32_t vnzKernelExitedThreads(void)
{
	return g_vnzKernelExitedThreads;
}

/* A thread's state, for the debugger (VnzKernelThreadInformation). */
void vnzKernelThreadInformation(VnzKernelThread *thread, VnzKernelThreadInformation *information)
{
	uint32_t index;
	VnzKernelCore *core = beginInquiry(thread, &index);

	information->state = thread->state | thread->attributes;
	information->priority = (int8_t)thread->priority;
	information->savedSp = thread->savedSp;
	information->stack = thread->stack;
	information->stackSize = thread->stackSize;
	information->flags = thread->flags;
	information->waitFlags = thread->waitFlags;
	if (thread == g_vnzKernelCores[index].current)
		information->dispatchDisable = g_vnzKernelCores[index].dispatchDisable - 1;
	else
		information->dispatchDisable = thread->savedDispatchDisable;
	information->parent = thread->parent;
	information->joining = (int16_t)thread->joining;
	information->reserved10 = 0;
	information->reserved14 = 0;
	information->reserved2c[0] = 0;
	information->reserved2c[1] = 0;
	information->reserved2c[2] = 0;
	information->reserved2c[3] = 0;
	information->reserved2c[4] = 0;
	information->children = (int16_t)thread->children;
	endInquiry(core, thread);
}

/* the started children of the calling thread that have not ended */
uint32_t vnzKernelThreadJoining(void)
{
	uint32_t joining;

	vnzKernelDisableDispatch();
	joining = g_vnzKernelCores[vnzCoreIndex()].current->joining;
	vnzKernelEnableDispatch();
	return joining;
}

void vnzKernelThreadSetUserWord(uint32_t word)
{
	g_vnzKernelCores[vnzCoreIndex()].current->userWord = word;
}

/* the calling thread's own words: Sony's kernel has one function per word */
static inline __attribute__((always_inline)) VnzKernelThread *currentThread(void)
{
	uint32_t core;
	VnzKernelThread *thread;

	vnzKernelDisableDispatch();
	vnzKernelCurrentCore(&core);
	thread = g_vnzKernelCores[core].current;
	vnzKernelEnableDispatch();
	return thread;
}

uint32_t *vnzKernelThreadLocal0(void)
{
	return &currentThread()->local[0];
}

uint32_t *vnzKernelThreadLocal1(void)
{
	return &currentThread()->local[1];
}

uint32_t *vnzKernelThreadLocal2(void)
{
	return &currentThread()->local[2];
}

uint32_t vnzKernelThreadAttributes(VnzKernelThread *thread)
{
	uint32_t index, attributes;
	VnzKernelCore *core = beginInquiry(thread, &index);

	attributes = thread->attributes;
	endInquiry(core, thread);
	return attributes;
}

/*
 * Put a thread back in its run queue at the head, after it served an interrupt
 * on its core.
 */
/* a thread's control block, from kernel pool 0 */
VnzKernelThread *vnzKernelThreadAllocate(void)
{
	return (VnzKernelThread *)vnzKernelKernelPoolTake(0, sizeof(VnzKernelThread), 0);
}

void vnzKernelThreadFree(VnzKernelThread *thread)
{
	vnzKernelTrace(VNZ_KERNEL_TRACE_DELETE, (uint32_t)thread, (uint32_t)thread->entry,
		thread->core + VNZ_FIRST_CORE_ID, VNZ_KERNEL_TRACE_THREAD_ENDS);
	thread->magic = 0;
	vnzKernelKernelPoolGive(0, (uint32_t)thread, sizeof(VnzKernelThread), 0);
}

void vnzKernelThreadResume(VnzKernelThread *thread)
{
	uint32_t index, core, priority, state;
	VnzKernelCore *self;
	VnzKernelQueue *queue;

	vnzKernelDisableDispatch();
	self = vnzKernelCurrentCore(&index);
	self->inKernel = 1;
	vnzKernelLockThread(thread);
	state = thread->state & VNZ_KERNEL_STATE_MASK;
	if (state == VNZ_KERNEL_STATE_READY || state == VNZ_KERNEL_STATE_RUNNING) {
		core = thread->core;
		priority = thread->priority;
		queue = &g_vnzKernelRunQueues[core][priority - 1];
		vnzKernelQueueRemove(queue, thread);
		vnzKernelQueuePrepend(queue, thread);
		if (thread != g_vnzKernelNextThreads[core] && g_vnzKernelNextThreads[core]->priority <= priority)
			vnzKernelRingCore(self, index, core);
	}
	vnzKernelUnlockThread(thread);
	vnzKernelProcessEvents(self);
	vnzKernelEnableDispatch();
}

/* Mark interrupts as the calling thread's; all it has. */
uint32_t vnzKernelThreadSetInterrupts(uint32_t interrupts)
{
	uint32_t index, set;
	VnzKernelCore *core;
	VnzKernelThread *thread;

	vnzKernelDisableDispatch();
	core = vnzKernelCurrentCore(&index);
	thread = core->current;
	core->inKernel = 1;
	vnzKernelLockThread(thread);
	set = thread->interrupts | interrupts;
	thread->interrupts = set;
	vnzKernelUnlockThread(thread);
	vnzKernelProcessEvents(core);
	vnzKernelEnableDispatch();
	return set;
}

uint32_t vnzKernelThreadClearInterrupts(uint32_t interrupts)
{
	uint32_t index, set;
	VnzKernelCore *core;
	VnzKernelThread *thread;

	vnzKernelDisableDispatch();
	core = vnzKernelCurrentCore(&index);
	thread = core->current;
	core->inKernel = 1;
	vnzKernelLockThread(thread);
	set = thread->interrupts & ~interrupts;
	thread->interrupts = set;
	vnzKernelUnlockThread(thread);
	vnzKernelProcessEvents(core);
	vnzKernelEnableDispatch();
	return set;
}

/*
 * Set event flags of a thread from an interrupt event, its core's run queue
 * already taken; nonzero when the thread is now the next of the core. The
 * thread is changed through its physical address.
 */
uint32_t vnzKernelWakeFromInterrupt(uint32_t coreIndex, VnzKernelThread *thread,
	VnzKernelThread *physical,
	uint32_t flags)
{
	uint32_t set = physical->flags | flags, result = 0;

	if ((physical->state & (VNZ_KERNEL_WAIT_MASK | VNZ_KERNEL_STATE_MASK)) == (VNZ_KERNEL_WAIT_FLAGS | VNZ_KERNEL_STATE_WAITING)) {
		uint32_t wanted = physical->waitFlags;

		if (set & wanted) {
			VnzKernelQueue *queue = &g_vnzKernelRunQueues[coreIndex][physical->priority - 1];

			physical->waitResult = set;
			physical->state = (physical->state & ~(VNZ_KERNEL_WAIT_MASK | VNZ_KERNEL_STATE_MASK)) | VNZ_KERNEL_STATE_READY;
			physical->next = NULL;
			if (!queue->head) {
				physical->previous = NULL;
				queue->head = thread;
			} else {
				physical->previous = queue->tail;
				queue->tail->next = thread;
			}
			queue->tail = thread;
			if (g_vnzKernelNextThreads[coreIndex]->priority < physical->priority) {
				g_vnzKernelNextThreads[coreIndex] = thread;
				result = 1;
			}
			physical->waitFlags = 0;
			set &= ~wanted;
		}
	}
	physical->flags = set;
	return result;
}

/*
 * At start: on the primary core, clear every core's run queues and set up its
 * idle thread; on each core, nothing pending.
 */
void vnzKernelThreadsInitialize(uint32_t primary)
{
	uint32_t core, i;

	if (primary) {
		for (core = 0; core < VNZ_CORES; core++) {
			VnzKernelThread *idle = &g_vnzKernelIdleThreads[core];

			*vnzKernelSemaphore(VNZ_KERNEL_SEMAPHORE_THREADS + core) = 0;
			g_vnzKernelThreadCounts[core] = 0;
			for (i = 0; i < VNZ_KERNEL_PRIORITIES; i++) {
				g_vnzKernelRunQueues[core][i].head = NULL;
				g_vnzKernelRunQueues[core][i].tail = NULL;
			}
			g_vnzKernelServiceQueues[core][0].head = NULL;
			g_vnzKernelServiceQueues[core][0].tail = NULL;
			g_vnzKernelServiceQueues[core][1].head = NULL;
			g_vnzKernelServiceQueues[core][1].tail = NULL;
			g_vnzKernelServiceThreads[core] = NULL;
			idle->physical = (VnzKernelThread *)vnzKernelPublicToPhysical((uint32_t)idle);
			idle->core = core;
			idle->state = VNZ_KERNEL_STATE_READY;
			idle->entry = (void *)vnzKernelIdleEntry;
			idle->savedDispatchDisable = 0;
			idle->priority = VNZ_KERNEL_IDLE_PRIORITY;
			idle->heap = &g_vnzKernelSystemHeap;
			g_vnzKernelNextThreads[core] = idle;
		}
	}
	g_vnzKernelCores[vnzCoreIndex()].reschedulePending = 0;
}

/* Start the image's threads of this core (VnzKernelConfiguration.startThreads). */
void vnzKernelStartThreads(uint32_t coreId, const VnzKernelConfiguration *configuration)
{
	const VnzKernelStartTable *table = configuration->startThreads;
	uint32_t i;

	if (!table)
		return;
	for (i = 0; i < table->count; i++) {
		const VnzKernelStartThread *start = &table->threads[i];
		int32_t thread;

		if (start->coreId != coreId)
			continue;
		thread = vnzKernelThreadCreateHere(start->module->entry, coreId, start->priority,
			start->stackSize);
		if (thread != -1)
			vnzKernelThreadStart((VnzKernelThread *)thread, start->argument,
				start->argument2);
	}
}

/*
 * The idle thread on a core, from the kernel's start: on the first start, start
 * the core's kernel threads; start or wake its power thread; on the first start,
 * the image's threads for the core. Then idle.
 */
void vnzKernelIdleEntry(uint32_t coreId, const VnzKernelConfiguration *configuration,
	uint32_t primary,
	uint32_t firstStart)
{
	uint32_t index = coreId - VNZ_FIRST_CORE_ID;

	vnzKernelDisableDispatch();
	vnzKernelEnableDoorbell();
	if (firstStart) {
		vnzKernelCacheStartWorkers(coreId);
		vnzKernelLocalCodeStartWorker(coreId);
		vnzKernelVThreadStartWorker();
		vnzKernelServiceStart();
	}
	vnzKernelPowerStart(primary, firstStart);
	if (firstStart)
		vnzKernelStartThreads(coreId, configuration);
	g_vnzKernelCores[index].reschedulePending = 1;
	vnzKernelEnableDispatch();
	for (;;)
		__asm__ volatile("swi 2");
}

/* Make this core's idle thread the running one. */
void vnzKernelIdleInitialize(uint32_t primary, uint32_t start, uint32_t index)
{
	VnzKernelCore *core = &g_vnzKernelCores[index];
	VnzKernelThread *idle = &g_vnzKernelIdleThreads[index];

	(void)primary;
	(void)start;
	core->idle = idle;
	core->dispatchDisable = 0;
	idle->state = VNZ_KERNEL_STATE_RUNNING;
	core->current = idle;
}

