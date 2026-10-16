/*
 * Threads: creation, the run queues, waiting and waking, event flags.
 *
 * A thread is changed under its core's run-queue semaphore (vkLockThread), with
 * dispatching disabled on the calling core and the core marked in the kernel, so
 * interrupt events wait (vkProcessEvents passes them on afterwards). Readying a
 * thread of a higher priority than the one its core will run next makes it the
 * next; when that core is another, its doorbell is rung (vkRingCore).
 *
 * A higher number is a higher priority; the idle thread, 0, runs when no other
 * can.
 */
#include "kernel.h"

/* the frame vkSwitchContext saves (kernel/switch.S) */
typedef struct VkSwitchFrame {
	uint32_t r5, r6, r7, r8, tp;
	void *lp;
} VkSwitchFrame;

/* the stand-in for an ending thread's context, which is not kept */
typedef struct VkScratchContext {
	uint32_t reserved[4];
	uint32_t savedDispatchDisable;
	uint32_t entry;
	uint32_t savedSp;
} VkScratchContext;

uint32_t g_vkExitedThreads VK_SHARED;
uint32_t g_vkExitedLimit VK_SHARED;

/* Take the run-queue semaphore of the thread's core. */
void vkLockThread(VkThread *thread)
{
	for (;;) {
		uint32_t core = thread->core;
		volatile uint32_t *semaphore = vkSemaphore(VK_SEMAPHORE_THREADS + core);
		uint32_t i;

		if (vkTrySemaphore(semaphore) == 0) {
			if (thread->core == core)
				return;
			vkGiveSemaphore(semaphore);
		}
		for (i = 0; i < 10; i++)
			;
	}
}

void vkUnlockThread(VkThread *thread)
{
	vkGiveSemaphore(vkSemaphore(VK_SEMAPHORE_THREADS + thread->core));
}

/* Switch to the core's next thread, if that is another. Interrupts are off. */
void vkReschedule(VkCore *core)
{
	VkThread *current = core->current;
	VkThread *next = g_vkNextThreads[core->index];

	if (next == current)
		return;
	if ((current->state & VK_STATE_MASK) == VK_STATE_RUNNING)
		current->state = (current->state & ~VK_STATE_MASK) | VK_STATE_READY;
	next->state = (next->state & ~VK_STATE_MASK) | VK_STATE_RUNNING;
	core->current = next;
	vkTrace(VK_TRACE_SWITCH, (uint32_t)current, (uint32_t)next, (uint32_t)next->entry,
		VK_TRACE_THREADS);
	vkSwitchContext(core, current, next);
}

/* Have a core reschedule: this one when dispatching allows, another by its doorbell. */
void vkRingCore(VkCore *core, uint32_t current, uint32_t target)
{
	if (target == current)
		core->reschedulePending = 1;
	else
		vnzRegister(VNZ_DOORBELLS + 0x20 * current + 0x10) = 1u << target;
}

/* the highest queued thread of a core at or below a priority, or NULL */
static VkThread *highestReady(uint32_t core, uint32_t priority)
{
	VkQueue *queues = g_vkRunQueues[core];
	int32_t i;

	for (i = (int32_t)priority - 1; i >= 0; i--)
		if (queues[i].head)
			return queues[i].head;
	return NULL;
}

/*
 * Create a thread of a core at a priority, its stack from a heap. It is dormant
 * until vkThreadStart. The calling thread is its parent, unless that is the idle
 * thread.
 */
int32_t vkThreadCreate(void *entry, uint32_t coreId, uint32_t priority, uint32_t stackSize,
	VkHeap *heap)
{
	uint32_t index;
	VkCore *core;
	VkThread *thread;
	void *stack;

	vkDisableDispatch();
	core = vkCurrentCore(&index);
	thread = vkThreadAllocate();
	if (!thread) {
		vkEnableDispatch();
		return -1;
	}
	stackSize = (stackSize + 0x3f) & ~0x3fu;
	stack = vkHeapAllocateStack(heap, stackSize);
	if (!stack) {
		vkThreadFree(thread);
		vkEnableDispatch();
		return -1;
	}

	thread->physical = (VkThread *)vkPublicToPhysical((uint32_t)thread);
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
	thread->state = VK_STATE_DORMANT;
	thread->userWord = 0;
	thread->interrupts = 0;
	thread->local[2] = 0;
	thread->local[1] = 0;
	thread->local[0] = 0;
	thread->attributes = 0;
	thread->joining = 0;
	thread->children = 0;
	thread->entry = entry;
	thread->magic = VK_THREAD_MAGIC;

	core->inKernel = 1;
	if (thread->parent) {
		vkLockThread(thread->parent);
		thread->parent->children++;
		vkUnlockThread(thread->parent);
	}
	vkLockThread(thread);
	g_vkThreadCounts[thread->core]++;
	vkUnlockThread(thread);
	vkProcessEvents(core);
	vkTrace(VK_TRACE_CREATE, (uint32_t)thread, (uint32_t)entry, coreId, VK_TRACE_THREADS);
	vkEnableDispatch();
	return (int32_t)thread;
}

/* vkThreadCreate, the stack from the calling thread's heap */
int32_t vkThreadCreateHere(void *entry, uint32_t coreId, uint32_t priority, uint32_t stackSize)
{
	uint32_t index;
	VkHeap *heap;

	vkDisableDispatch();
	vkCurrentCore(&index);
	heap = g_vkCores[index].current->heap;
	vkEnableDispatch();
	return vkThreadCreate(entry, coreId, priority, stackSize, heap);
}

/* Start a dormant thread at its entry with two arguments. */
int32_t vkThreadStart(VkThread *thread, uint32_t argument, uint32_t argument2)
{
	uint32_t index, core;
	VkCore *self;
	VkThread *parent;
	uint32_t top;

	vkDisableDispatch();
	self = vkCurrentCore(&index);
	self->inKernel = 1;
	vkLockThread(thread);
	core = thread->core;
	if ((thread->state & VK_STATE_MASK) != VK_STATE_DORMANT) {
		vkUnlockThread(thread);
		vkProcessEvents(self);
		vkEnableDispatch();
		return -1;
	}

	top = (uint32_t)thread->stack + thread->stackSize - 0x40;
	vnzCacheLine(VNZ_CACHE_INVALIDATE, top);
	thread->savedSp = vkInitialFrame(top, thread->entry, argument, argument2);
	vnzCacheLine(VNZ_CACHE_WRITEBACK, top);
	thread->state = (thread->state & ~VK_STATE_MASK) | VK_STATE_READY;
	if (core != index)
		mep_syncm();

	if (core != VK_NO_CORE) {
		uint32_t priority = thread->priority;

		vkQueueAppend(&g_vkRunQueues[core][priority - 1], thread);
		if (g_vkNextThreads[core]->priority < priority) {
			g_vkNextThreads[core] = thread;
			vkRingCore(self, index, core);
		}
	}
	vkUnlockThread(thread);

	parent = thread->parent;
	if (parent) {
		vkLockThread(parent);
		parent->joining++;
		vkUnlockThread(parent);
	}
	vkProcessEvents(self);
	vkEnableDispatch();
	return 0;
}

/*
 * Take the running thread off its core and pick the next one; on the idle
 * thread's stack, vkThreadFinish ends it (kernel/switch.S vkEndThread).
 */
static void leaveCore(VkCore *core, uint32_t index, VkThread *thread, uint32_t state)
{
	VkThread *next;

	vkQueueRemove(&g_vkRunQueues[index][thread->priority - 1], thread);
	thread->state = (thread->state & ~VK_STATE_MASK) | state;
	next = highestReady(index, g_vkNextThreads[index]->priority);
	g_vkNextThreads[index] = next ? next : core->idle;
	vkEndThread(thread, core, core->idle->savedSp);
}

/* End the calling thread; it goes dormant. */
void vkThreadExit(void)
{
	uint32_t index;
	VkCore *core;
	VkThread *thread;

	vkDisableDispatch();
	core = vkCurrentCore(&index);
	thread = core->current;
	core->inKernel = 1;
	vkLockThread(thread);
	leaveCore(core, index, thread, VK_STATE_DORMANT);
}

/*
 * End the calling thread with an exit status, kept until it is collected
 * (vkThreadExitStatus, vkThreadDelete). Too many uncollected ends stop the
 * kernel.
 */
void vkThreadExitWithStatus(uint32_t status, uint32_t status2)
{
	uint32_t index;
	VkCore *core;
	VkThread *thread;
	uint32_t exited;

	vkDisableDispatch();
	core = vkCurrentCore(&index);
	thread = core->current;
	core->inKernel = 1;
	vkLockThread(thread);
	thread->exitStatus[0] = status;
	thread->exitStatus[1] = status2;

	vkTakeSemaphore(vkSemaphore(VK_SEMAPHORE_EXITS));
	exited = ++g_vkExitedThreads;
	if (g_vkExitedLimit != 0xffffffff && exited >= g_vkExitedLimit)
		vkExceptionTooManyExited();
	vkGiveSemaphore(vkSemaphore(VK_SEMAPHORE_EXITS));

	leaveCore(core, index, thread, VK_STATE_EXITED);
}

/* Free a thread's stack and the thread. */
void vkThreadRelease(VkThread *thread)
{
	void *stack = thread->stack;
	uint32_t size = thread->stackSize;

	vkHeapFreeStack(thread->heap, stack, size);
	vkThreadFree(thread);
}

/*
 * The rest of a thread's end, on the core's idle stack: tell whoever waits, free
 * what nobody can refer to any more, and switch to the next thread.
 */
void vkThreadFinish(VkThread *thread, VkCore *core)
{
	uint32_t index = core->index;
	uint32_t exited, notify = 0, freeStack, freeThread = 0;
	VkThread *parent, *next;
	VkScratchContext scratch;

	thread->savedDispatchDisable = core->dispatchDisable - 1;
	exited = (thread->state & VK_STATE_MASK) == VK_STATE_EXITED;
	if (exited)
		notify = thread->exitNotify != NULL;
	vkCachePurge((uint32_t)thread->stack, thread->stackSize);

	parent = thread->parent;
	if (parent || thread->children) {
		freeStack = 0;
	} else {
		freeStack = 1;
		if (!exited) {
			/* nothing refers to it: it goes */
			g_vkThreadCounts[index]--;
			freeThread = 1;
			thread->state &= ~VK_STATE_MASK;
		}
	}
	vkUnlockThread(thread);

	if (notify)
		vkThreadSetFlags(thread->exitNotify, thread->exitNotifyFlags);
	if (freeStack)
		vkHeapFreeStack(thread->heap, thread->stack, thread->stackSize);
	if (freeThread)
		vkThreadFree(thread);

	if (parent) {
		vkLockThread(parent);
		mep_syncm();
		parent->joining--;
		if ((parent->state & (VK_WAIT_MASK | VK_STATE_MASK)) == (VK_WAIT_CHILDREN | VK_STATE_WAITING) &&
		    vkMakeReady(parent, 0))
			vkRingCore(core, index, parent->core);
		vkUnlockThread(parent);
	}
	vkProcessEvents(core);

	vnzDisableInterrupts();
	next = g_vkNextThreads[index];
	next->state = (next->state & ~VK_STATE_MASK) | VK_STATE_RUNNING;
	core->current = next;
	vkTrace(VK_TRACE_SWITCH, (uint32_t)thread, (uint32_t)next, (uint32_t)next->entry,
		VK_TRACE_THREADS);
	vkSwitchContext(core, (VkThread *)&scratch, next);
}

/*
 * Delete a thread that has ended and has no children of its own; an exited
 * thread's status is dropped. A thread serving interrupts keeps its memory.
 */
int32_t vkThreadDelete(VkThread *thread)
{
	uint32_t index;
	VkCore *core;
	uint32_t state, release = 0;
	int32_t result = -1;

	vkDisableDispatch();
	core = vkCurrentCore(&index);
	core->inKernel = 1;
	vkLockThread(thread);
	state = thread->state & VK_STATE_MASK;
	if ((state == VK_STATE_DORMANT || state == VK_STATE_EXITED) && !thread->children) {
		VkThread *parent;

		if ((int8_t)thread->state == VK_STATE_EXITED) {
			vkTakeSemaphore(vkSemaphore(VK_SEMAPHORE_EXITS));
			g_vkExitedThreads--;
			vkGiveSemaphore(vkSemaphore(VK_SEMAPHORE_EXITS));
		}
		thread->state &= ~VK_STATE_MASK;
		if (thread->attributes != VK_THREAD_MOVING) {
			g_vkThreadCounts[thread->core]--;
			release = 1;
		}
		parent = thread->parent;
		vkUnlockThread(thread);
		if (parent) {
			vkLockThread(parent);
			parent->children--;
			vkUnlockThread(parent);
		}
		if (release)
			vkThreadRelease(thread);
		result = 0;
	} else {
		vkUnlockThread(thread);
	}
	vkProcessEvents(core);
	vkEnableDispatch();
	return result;
}

/* Change a thread's priority; a ready or running one moves in its run queue. */
void vkThreadSetPriority(VkThread *thread, uint32_t priority)
{
	uint32_t index, core, state, old, nextPriority;
	VkCore *self;
	VkQueue *queues;
	VkThread *next;

	vkDisableDispatch();
	self = vkCurrentCore(&index);
	self->inKernel = 1;
	vkLockThread(thread);
	core = thread->core;
	old = thread->priority;
	if (old == priority)
		goto done;
	state = thread->state & VK_STATE_MASK;
	if (state == VK_STATE_EXITED)
		goto done;
	if (state == VK_STATE_WAITING || state == VK_STATE_DORMANT ||
	    thread->attributes == VK_THREAD_MOVING) {
		thread->priority = priority;
		goto done;
	}

	/* ready or running: requeue it */
	queues = g_vkRunQueues[core];
	next = g_vkNextThreads[core];
	nextPriority = next->priority;
	vkQueueRemove(&queues[old - 1], thread);
	thread->priority = priority;
	vkQueueAppend(&queues[priority - 1], thread);
	if (g_vkNextThreads[core]->priority < priority) {
		g_vkNextThreads[core] = thread;
	} else {
		int32_t i;

		if (g_vkNextThreads[core] != thread || priority >= nextPriority)
			goto done;
		/* it was to run next and drops: the highest above its new priority */
		for (i = nextPriority - 1; i >= (int32_t)priority && !queues[i].head; i--)
			;
		g_vkNextThreads[core] = queues[i].head;
		if (queues[i].head == thread)
			goto done;
		thread->state = (thread->state & ~VK_STATE_MASK) | VK_STATE_READY;
	}
	vkRingCore(self, index, core);
done:
	vkUnlockThread(thread);
	vkProcessEvents(self);
	vkEnableDispatch();
}

/* Give the core to the next thread of the same priority. */
void vkThreadYield(void)
{
	uint32_t index, priority, reschedule;
	VkCore *core;
	VkThread *thread;
	VkQueue *queue;

	vkDisableDispatch();
	core = vkCurrentCore(&index);
	thread = core->current;
	core->inKernel = 1;
	vkLockThread(thread);
	priority = thread->priority;
	queue = &g_vkRunQueues[index][priority - 1];
	vkQueueRemove(queue, thread);
	vkQueueAppend(queue, thread);
	if (priority < g_vkNextThreads[index]->priority) {
		reschedule = 1;
	} else if (queue->head == thread) {
		reschedule = 0;
	} else {
		thread->state = (thread->state & ~VK_STATE_MASK) | VK_STATE_READY;
		g_vkNextThreads[index] = queue->head;
		reschedule = 1;
	}
	vkUnlockThread(thread);
	vkProcessEvents(core);
	if (reschedule) {
		vnzDisableInterrupts();
		vkReschedule(core);
		vnzEnableInterrupts();
	}
	vkEnableDispatch();
}

/* Make a locked thread wait, and pick the next thread of its core. */
static void blockThread(VkThread *thread, uint32_t wait)
{
	uint32_t core = thread->core;
	VkThread *next;

	thread->state = (thread->state & ~VK_STATE_MASK) | wait | VK_STATE_WAITING;
	vkQueueRemove(&g_vkRunQueues[core][thread->priority - 1], thread);
	next = highestReady(core, g_vkNextThreads[core]->priority);
	g_vkNextThreads[core] = next ? next : g_vkCores[core].idle;
}

/* Wait until vkThreadWakeup; the value it was given. */
uint32_t vkThreadSleep(void)
{
	uint32_t index;
	VkCore *core;
	VkThread *thread;

	vkDisableDispatch();
	core = vkCurrentCore(&index);
	thread = core->current;
	core->inKernel = 1;
	vkLockThread(thread);
	blockThread(thread, VK_WAIT_SLEEP);
	vkUnlockThread(thread);
	vkProcessEvents(core);
	vnzDisableInterrupts();
	vkReschedule(core);
	vnzEnableInterrupts();
	vkEnableDispatch();
	return thread->waitResult;
}

/* Make a thread wait for event flag 0 during a suspend (kernel/power.c). */
void vkThreadHoldForResume(VkThread *thread)
{
	vkLockThread(thread);
	blockThread(thread, VK_WAIT_FLAGS);
	thread->waitFlags = 1;
	vkUnlockThread(thread);
}

/*
 * Ready a locked waiting thread with a result; nonzero when it is now the next
 * thread of its core (which then needs telling).
 */
uint32_t vkMakeReady(VkThread *thread, uint32_t result)
{
	uint32_t core = thread->core;

	thread->waitResult = result;
	thread->state = (thread->state & ~(VK_WAIT_MASK | VK_STATE_MASK)) | VK_STATE_READY;
	if (core == VK_NO_CORE)
		return 0;
	vkQueueAppend(&g_vkRunQueues[core][thread->priority - 1], thread);
	if (g_vkNextThreads[core]->priority < thread->priority) {
		g_vkNextThreads[core] = thread;
		return 1;
	}
	return 0;
}

/* Wake a sleeping thread with a value. */
int32_t vkThreadWakeup(VkThread *thread, uint32_t value)
{
	uint32_t index, core;
	VkCore *self;
	int32_t result = -1;

	vkDisableDispatch();
	self = vkCurrentCore(&index);
	self->inKernel = 1;
	vkLockThread(thread);
	core = thread->core;
	if ((thread->state & (VK_WAIT_MASK | VK_STATE_MASK)) == (VK_WAIT_SLEEP | VK_STATE_WAITING)) {
		if (core != index)
			mep_syncm();
		if (vkMakeReady(thread, value))
			vkRingCore(self, index, core);
		result = 0;
	}
	vkUnlockThread(thread);
	vkProcessEvents(self);
	vkEnableDispatch();
	return result;
}

/* Release a thread held by vkThreadHoldForResume. */
void vkThreadReleaseFromResume(VkThread *thread)
{
	vkLockThread(thread);
	if ((thread->state & (VK_WAIT_MASK | VK_STATE_MASK)) == (VK_WAIT_FLAGS | VK_STATE_WAITING)) {
		thread->flags = 0;
		thread->waitFlags = 0;
		vkMakeReady(thread, 0);
	}
	vkUnlockThread(thread);
}

/* Wait until the started children have ended; how many are still running. */
uint32_t vkThreadWaitChildren(void)
{
	uint32_t index, reschedule;
	VkCore *core;
	VkThread *thread;

	vkDisableDispatch();
	core = vkCurrentCore(&index);
	thread = core->current;
	core->inKernel = 1;
	vkLockThread(thread);
	if (thread->joining == 0) {
		reschedule = g_vkNextThreads[index] != thread;
	} else {
		blockThread(thread, VK_WAIT_CHILDREN);
		reschedule = 1;
	}
	vkUnlockThread(thread);
	vkProcessEvents(core);
	if (reschedule) {
		vnzDisableInterrupts();
		vkReschedule(core);
		vnzEnableInterrupts();
	}
	vkEnableDispatch();
	return thread->joining;
}

/*
 * Detach a thread from its parent; a dormant one without children of its own
 * goes. A parent waiting for it stops waiting for it.
 */
void vkThreadDetach(VkThread *thread)
{
	uint32_t index, core, state, release = 0;
	VkCore *self;
	VkThread *parent;

	vkDisableDispatch();
	self = vkCurrentCore(&index);
	self->inKernel = 1;
	vkLockThread(thread);
	core = thread->core;
	parent = thread->parent;
	thread->parent = NULL;
	state = thread->state & VK_STATE_MASK;
	if (state == VK_STATE_DORMANT && !(int16_t)thread->children) {
		thread->state &= ~VK_STATE_MASK;
		if (thread->attributes != VK_THREAD_MOVING) {
			g_vkThreadCounts[core]--;
			release = 1;
		}
	}
	vkUnlockThread(thread);

	if (parent) {
		vkLockThread(parent);
		parent->children--;
		if (state != VK_STATE_DORMANT && state != VK_STATE_EXITED) {
			parent->joining--;
			if ((parent->state & (VK_WAIT_MASK | VK_STATE_MASK)) ==
			    (VK_WAIT_CHILDREN | VK_STATE_WAITING) && vkMakeReady(parent, 0))
				vkRingCore(self, index, parent->core);
		}
		vkUnlockThread(parent);
	}
	if (release)
		vkThreadRelease(thread);
	vkProcessEvents(self);
	vkEnableDispatch();
}

/* Set event flags of a thread, waking it when it waits for one of them. */
void vkThreadSetFlags(VkThread *thread, uint32_t flags)
{
	uint32_t index, core, set;
	VkCore *self;

	vkDisableDispatch();
	self = vkCurrentCore(&index);
	self->inKernel = 1;
	vkLockThread(thread);
	core = thread->core;
	if (core != index)
		mep_syncm();
	set = thread->flags | flags;
	if ((thread->state & (VK_WAIT_MASK | VK_STATE_MASK)) == (VK_WAIT_FLAGS | VK_STATE_WAITING)) {
		uint32_t wanted = thread->waitFlags;

		if (set & wanted) {
			if (vkMakeReady(thread, set))
				vkRingCore(self, index, core);
			thread->waitFlags = 0;
			set &= ~wanted;
		}
	}
	thread->flags = set;
	vkUnlockThread(thread);
	vkProcessEvents(self);
	vkEnableDispatch();
}

/*
 * Wait until one of the event flags is set; the flags that were set. Those
 * waited for are cleared.
 */
uint32_t vkThreadWaitFlags(uint32_t flags)
{
	uint32_t index, set, reschedule;
	VkCore *core;
	VkThread *thread;

	vkDisableDispatch();
	core = vkCurrentCore(&index);
	thread = core->current;
	core->inKernel = 1;
	vkLockThread(thread);
	set = thread->flags;
	if (set & flags) {
		thread->waitResult = set;
		thread->waitFlags = 0;
		thread->flags = set & ~flags;
		reschedule = g_vkNextThreads[index] != thread;
	} else {
		blockThread(thread, VK_WAIT_FLAGS);
		thread->waitFlags = flags;
		reschedule = 1;
	}
	vkUnlockThread(thread);
	vkProcessEvents(core);
	if (reschedule) {
		vnzDisableInterrupts();
		vkReschedule(core);
		vnzEnableInterrupts();
	}
	vkEnableDispatch();
	return thread->waitResult;
}

/* Clear event flags of the calling thread; the flags that were set. */
uint32_t vkThreadClearFlags(uint32_t flags)
{
	uint32_t index, set;
	VkCore *core;
	VkThread *thread;

	vkDisableDispatch();
	core = vkCurrentCore(&index);
	thread = core->current;
	core->inKernel = 1;
	vkLockThread(thread);
	set = thread->flags;
	thread->flags = set & ~flags;
	vkUnlockThread(thread);
	vkProcessEvents(core);
	vkEnableDispatch();
	return set;
}

VkThread *vkThreadCurrent(void)
{
	VkThread *thread;

	vkDisableDispatch();
	thread = g_vkCores[vnzCoreIndex()].current;
	vkEnableDispatch();
	return thread;
}

/* the start of an inquiry about a thread, and its end */
static VkCore *beginInquiry(VkThread *thread, uint32_t *index)
{
	VkCore *core;

	vkDisableDispatch();
	core = vkCurrentCore(index);
	core->inKernel = 1;
	vkLockThread(thread);
	return core;
}

static void endInquiry(VkCore *core, VkThread *thread)
{
	vkUnlockThread(thread);
	vkProcessEvents(core);
	vkEnableDispatch();
}

uint32_t vkThreadPriority(VkThread *thread)
{
	uint32_t index, priority;
	VkCore *core = beginInquiry(thread, &index);

	priority = thread->priority;
	endInquiry(core, thread);
	return priority;
}

/* nonzero while the thread has not ended */
uint32_t vkThreadIsAlive(VkThread *thread)
{
	uint32_t index, state;
	VkCore *core = beginInquiry(thread, &index);

	state = thread->state & VK_STATE_MASK;
	endInquiry(core, thread);
	return state != VK_STATE_DORMANT && state != VK_STATE_EXITED;
}

uint32_t vkThreadHasExited(VkThread *thread)
{
	uint32_t index, exited;
	VkCore *core = beginInquiry(thread, &index);

	exited = (thread->state & VK_STATE_MASK) == VK_STATE_EXITED;
	endInquiry(core, thread);
	return exited;
}

/*
 * Have the calling thread's event flags set when a thread exits (at once when
 * it has). One thread may ask.
 */
int32_t vkThreadNotifyExit(VkThread *thread, uint32_t flags)
{
	uint32_t index, now = 0;
	VkCore *core = beginInquiry(thread, &index);
	VkThread *current = core->current;
	int32_t result = -1;

	if (!thread->exitNotify) {
		thread->exitNotify = current;
		thread->exitNotifyFlags = flags;
		if ((int8_t)thread->state == VK_STATE_EXITED)
			now = 1;
		result = 0;
	}
	endInquiry(core, thread);
	if (now == 1)
		vkThreadSetFlags(current, flags);
	return result;
}

int32_t vkThreadCancelExitNotify(VkThread *thread)
{
	uint32_t index;
	VkCore *core = beginInquiry(thread, &index);
	int32_t result = -1;

	if (thread->exitNotify) {
		thread->exitNotifyFlags = 0;
		thread->exitNotify = NULL;
		result = 0;
	}
	endInquiry(core, thread);
	return result;
}

int32_t vkThreadExitStatus(VkThread *thread, uint32_t *status, uint32_t *status2)
{
	uint32_t index;
	VkCore *core = beginInquiry(thread, &index);
	int32_t result = -1;

	if ((thread->state & VK_STATE_MASK) == VK_STATE_EXITED) {
		*status = thread->exitStatus[0];
		*status2 = thread->exitStatus[1];
		result = 0;
	}
	endInquiry(core, thread);
	return result;
}

uint32_t vkExitedThreads(void)
{
	return g_vkExitedThreads;
}

/* A thread's state, for the debugger (VkThreadInformation). */
void vkThreadInformation(VkThread *thread, VkThreadInformation *information)
{
	uint32_t index;
	VkCore *core = beginInquiry(thread, &index);

	information->state = thread->state | thread->attributes;
	information->priority = (int8_t)thread->priority;
	information->savedSp = thread->savedSp;
	information->stack = thread->stack;
	information->stackSize = thread->stackSize;
	information->flags = thread->flags;
	information->waitFlags = thread->waitFlags;
	if (thread == g_vkCores[index].current)
		information->dispatchDisable = g_vkCores[index].dispatchDisable - 1;
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
uint32_t vkThreadJoining(void)
{
	uint32_t joining;

	vkDisableDispatch();
	joining = g_vkCores[vnzCoreIndex()].current->joining;
	vkEnableDispatch();
	return joining;
}

void vkThreadSetUserWord(uint32_t word)
{
	g_vkCores[vnzCoreIndex()].current->userWord = word;
}

/* the calling thread's own words */
uint32_t *vkThreadLocal(uint32_t index)
{
	uint32_t core;
	VkThread *thread;

	vkDisableDispatch();
	vkCurrentCore(&core);
	thread = g_vkCores[core].current;
	vkEnableDispatch();
	return &thread->local[index];
}

uint32_t vkThreadAttributes(VkThread *thread)
{
	uint32_t index, attributes;
	VkCore *core = beginInquiry(thread, &index);

	attributes = thread->attributes;
	endInquiry(core, thread);
	return attributes;
}

/*
 * Put a thread back in its run queue at the head, after it served an interrupt
 * on its core.
 */
/* a thread's control block, from kernel pool 0 */
VkThread *vkThreadAllocate(void)
{
	return (VkThread *)vkKernelPoolTake(0, sizeof(VkThread), 0);
}

void vkThreadFree(VkThread *thread)
{
	vkTrace(VK_TRACE_DELETE, (uint32_t)thread, (uint32_t)thread->entry,
		thread->core + VNZ_FIRST_CORE_ID, VK_TRACE_THREAD_ENDS);
	thread->magic = 0;
	vkKernelPoolGive(0, (uint32_t)thread, sizeof(VkThread), 0);
}

void vkThreadResume(VkThread *thread)
{
	uint32_t index, core, priority, state;
	VkCore *self;
	VkQueue *queue;

	vkDisableDispatch();
	self = vkCurrentCore(&index);
	self->inKernel = 1;
	vkLockThread(thread);
	state = thread->state & VK_STATE_MASK;
	if (state == VK_STATE_READY || state == VK_STATE_RUNNING) {
		core = thread->core;
		priority = thread->priority;
		queue = &g_vkRunQueues[core][priority - 1];
		vkQueueRemove(queue, thread);
		vkQueuePrepend(queue, thread);
		if (thread != g_vkNextThreads[core] && g_vkNextThreads[core]->priority <= priority)
			vkRingCore(self, index, core);
	}
	vkUnlockThread(thread);
	vkProcessEvents(self);
	vkEnableDispatch();
}

/* Mark interrupts as the calling thread's; all it has. */
uint32_t vkThreadSetInterrupts(uint32_t interrupts)
{
	uint32_t index, set;
	VkCore *core;
	VkThread *thread;

	vkDisableDispatch();
	core = vkCurrentCore(&index);
	thread = core->current;
	core->inKernel = 1;
	vkLockThread(thread);
	set = thread->interrupts | interrupts;
	thread->interrupts = set;
	vkUnlockThread(thread);
	vkProcessEvents(core);
	vkEnableDispatch();
	return set;
}

uint32_t vkThreadClearInterrupts(uint32_t interrupts)
{
	uint32_t index, set;
	VkCore *core;
	VkThread *thread;

	vkDisableDispatch();
	core = vkCurrentCore(&index);
	thread = core->current;
	core->inKernel = 1;
	vkLockThread(thread);
	set = thread->interrupts & ~interrupts;
	thread->interrupts = set;
	vkUnlockThread(thread);
	vkProcessEvents(core);
	vkEnableDispatch();
	return set;
}

/*
 * Set event flags of a thread from an interrupt event, its core's run queue
 * already taken; nonzero when the thread is now the next of the core. The
 * thread is changed through its physical address.
 */
uint32_t vkWakeFromInterrupt(uint32_t coreIndex, VkThread *thread, VkThread *physical,
	uint32_t flags)
{
	uint32_t set = physical->flags | flags, result = 0;

	if ((physical->state & (VK_WAIT_MASK | VK_STATE_MASK)) == (VK_WAIT_FLAGS | VK_STATE_WAITING)) {
		uint32_t wanted = physical->waitFlags;

		if (set & wanted) {
			VkQueue *queue = &g_vkRunQueues[coreIndex][physical->priority - 1];

			physical->waitResult = set;
			physical->state = (physical->state & ~(VK_WAIT_MASK | VK_STATE_MASK)) | VK_STATE_READY;
			physical->next = NULL;
			if (!queue->head) {
				physical->previous = NULL;
				queue->head = thread;
			} else {
				physical->previous = queue->tail;
				queue->tail->next = thread;
			}
			queue->tail = thread;
			if (g_vkNextThreads[coreIndex]->priority < physical->priority) {
				g_vkNextThreads[coreIndex] = thread;
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
void vkThreadsInitialize(uint32_t primary)
{
	uint32_t core, i;

	if (primary) {
		for (core = 0; core < VNZ_CORES; core++) {
			VkThread *idle = &g_vkIdleThreads[core];

			*vkSemaphore(VK_SEMAPHORE_THREADS + core) = 0;
			g_vkThreadCounts[core] = 0;
			for (i = 0; i < VK_PRIORITIES; i++) {
				g_vkRunQueues[core][i].head = NULL;
				g_vkRunQueues[core][i].tail = NULL;
			}
			g_vkServiceQueues[core][0].head = NULL;
			g_vkServiceQueues[core][0].tail = NULL;
			g_vkServiceQueues[core][1].head = NULL;
			g_vkServiceQueues[core][1].tail = NULL;
			g_vkServiceThreads[core] = NULL;
			idle->physical = (VkThread *)vkPublicToPhysical((uint32_t)idle);
			idle->core = core;
			idle->state = VK_STATE_READY;
			idle->entry = (void *)vkIdleEntry;
			idle->savedDispatchDisable = 0;
			idle->priority = VK_IDLE_PRIORITY;
			idle->heap = &g_vkSystemHeap;
			g_vkNextThreads[core] = idle;
		}
	}
	g_vkCores[vnzCoreIndex()].reschedulePending = 0;
}

/* Start the image's threads of this core (VkConfiguration.startThreads). */
void vkStartThreads(uint32_t coreId, const VkConfiguration *configuration)
{
	const VkStartTable *table = configuration->startThreads;
	uint32_t i;

	if (!table)
		return;
	for (i = 0; i < table->count; i++) {
		const VkStartThread *start = &table->threads[i];
		int32_t thread;

		if (start->coreId != coreId)
			continue;
		thread = vkThreadCreateHere(start->module->entry, coreId, start->priority,
			start->stackSize);
		if (thread != -1)
			vkThreadStart((VkThread *)thread, start->argument, start->argument2);
	}
}

/*
 * The idle thread on a core, from the kernel's start: on the first start, start
 * the core's kernel threads; start or wake its power thread; on the first start,
 * the image's threads for the core. Then idle.
 */
void vkIdleEntry(uint32_t coreId, const VkConfiguration *configuration, uint32_t primary,
	uint32_t firstStart)
{
	uint32_t index = coreId - VNZ_FIRST_CORE_ID;

	vkDisableDispatch();
	vkEnableDoorbell();
	if (firstStart) {
		vkCacheStartWorkers(coreId);
		vkLocalCodeStartWorker(coreId);
		vkVThreadStartWorker();
		vkServiceStart();
	}
	vkPowerStart(primary, firstStart);
	if (firstStart)
		vkStartThreads(coreId, configuration);
	g_vkCores[index].reschedulePending = 1;
	vkEnableDispatch();
	for (;;)
		__asm__ volatile("swi 2");
}

/* Make this core's idle thread the running one. */
void vkIdleInitialize(uint32_t primary, uint32_t start, uint32_t index)
{
	VkCore *core = &g_vkCores[index];
	VkThread *idle = &g_vkIdleThreads[index];

	(void)primary;
	(void)start;
	core->idle = idle;
	core->dispatchDisable = 0;
	idle->state = VK_STATE_RUNNING;
	core->current = idle;
}

