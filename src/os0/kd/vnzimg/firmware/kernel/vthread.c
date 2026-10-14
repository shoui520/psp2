/*
 * VThreads: work spread over the cores.
 *
 * Each enabled core runs a worker thread. A processing resource ("VTPR") is a
 * stack size and a space ("VTSP" contexts queue jobs into it); the resources
 * are in a ring, and the workers serve the ring's head, moving on when it has
 * no work. A context holds a fixed set of jobs: a job is given a function and
 * the number of signals it waits for, and becomes ready when they have come.
 * A broadcast runs a function once on every core.
 *
 * (VVNZRunner's names: vnzVThreadCreateContext is vkVThreadContextCreate,
 * vnzVThreadDeleteContext vkVThreadContextDelete.)
 */
#include "kernel.h"

#define VK_VTHREAD_WORKER_PRIORITY	1
#define VK_VTHREAD_WORKER_STACK		0x200
#define VK_VTHREAD_WAKE			0x1	/* the event flag the workers wait for */

#define VK_JOB_IDLE		0
#define VK_JOB_WAITING		1	/* for its signals */
#define VK_JOB_READY		2
#define VK_JOB_RUNNING		3

#define VK_BROADCAST_ENABLED	1	/* to the enabled cores; else to those with a worker */
#define VK_BROADCAST_WORKERS	2

#define VK_RESOURCE_MAGIC	0x56545052	/* "VTPR" */
#define VK_CONTEXT_MAGIC	0x56545350	/* "VTSP" */
#define VK_CORE_EVENT_MAGIC	0x4343454f	/* "CCEO" */

VkVThreadSystem g_vkVThreads VK_SHARED;

/* singly linked lists */
static void listAppend(VkList *list, VkListNode *node)
{
	node->next = NULL;
	if (list->head)
		list->tail->next = node;
	else
		list->head = node;
	list->tail = node;
}

static VkListNode *listTake(VkList *list)
{
	VkListNode *node = list->head;

	if (node) {
		list->head = node->next;
		node->next = NULL;
	}
	return node;
}

/* Move all of from's nodes to the end of to. */
static void listMove(VkList *from, VkList *to)
{
	if (to->head)
		to->tail->next = from->head;
	else
		to->head = from->head;
	to->tail = from->tail;
	from->head = NULL;
}

/* the workers waiting for work */
static void addWaiting(VkVThreadSystem *system, VkThread *thread)
{
	uint32_t i;

	for (i = 0; i < system->waitingCount; i++)
		if (system->waiting[i] == thread)
			return;
	system->waiting[system->waitingCount++] = thread;
}

static uint32_t takeWaiting(VkVThreadSystem *system, VkThread **threads)
{
	uint32_t count = system->waitingCount, i;

	system->waitingCount = 0;
	for (i = 0; i < count; i++)
		threads[i] = system->waiting[i];
	return count;
}

static uint32_t removeWaiting(VkVThreadSystem *system, VkThread *thread)
{
	uint32_t count = system->waitingCount, kept = 0, removed = 0, i;

	for (i = 0; i < count; i++) {
		if (system->waiting[i] == thread) {
			system->waitingCount--;
			removed = 1;
			continue;
		}
		system->waiting[kept++] = system->waiting[i];
	}
	return removed;
}

static void wakeAll(int32_t count, VkThread **threads)
{
	int32_t i;

	for (i = count - 1; i >= 0; i--)
		vkThreadSetFlags(threads[i], VK_VTHREAD_WAKE);
}

/* the threads told when the enabled cores have all started or stopped */
static void notifyCoreEvents(VkVThreadSystem *system)
{
	VkCoreEvent *event;

	for (event = system->coreEvents.head; event; event = event->next)
		vkThreadSetFlags(event->thread, event->flags);
}

/*
 * Contexts.
 */

static uint32_t contextSize(uint32_t count)
{
	return ((count * sizeof(VkJob) + 0x3f) & ~0x3f) +
		((count * sizeof(VkJobCounter) + 0x3f) & ~0x3f) + 0x80;
}

/* Create a context of count jobs that queues them into resource. Returns it, or NULL. */
VkVThreadContext *vkVThreadContextCreate(uint32_t count, VkVThreadResource *resource)
{
	VkVThreadContext *physical, *context;
	VkJobCounter *counters;
	VkJob *jobs;
	VkHeap *heap;
	uint32_t countersSize, jobsSize, i;

	if (count == 0)
		return NULL;
	heap = vkCurrentHeap();
	countersSize = (count * sizeof(VkJobCounter) + 0x3f) & ~0x3f;
	jobsSize = (count * sizeof(VkJob) + 0x3f) & ~0x3f;
	physical = vkHeapAllocate(heap, contextSize(count), 0x40);
	if (physical == NULL)
		return NULL;
	context = (VkVThreadContext *)vkPhysicalToPublic((uint32_t)physical);

	physical->lock = vkLockPageAllocate();
	if (physical->lock == NULL) {
		vnzCacheLine(VNZ_CACHE_PURGE, physical);
		vkHeapFree(heap, physical, contextSize(count), 0x40);
		return NULL;
	}
	physical->count = count;
	physical->resource = resource;
	physical->deleting = 0;
	physical->heap = heap;
	physical->space = resource->space;
	physical->counters = (VkJobCounter *)(context + 1);
	physical->magic = VK_CONTEXT_MAGIC;

	counters = (VkJobCounter *)vkPublicToPhysical((uint32_t)physical->counters);
	for (i = 0; i < count; i++) {
		counters[i].done = 0;
		counters[i].wanted = 0;
	}
	vkCacheWriteback((uint32_t)counters, countersSize);

	physical->jobs = (VkJob *)((uint8_t *)physical->counters + countersSize);
	jobs = (VkJob *)vkPublicToPhysical((uint32_t)physical->jobs);
	physical->jobsPhysical = jobs;
	for (i = 0; i < count; i++) {
		jobs[i].next = NULL;
		jobs[i].index = i;
		jobs[i].context = context;
		jobs[i].state = VK_JOB_IDLE;
		jobs[i].counter = &physical->counters[i];
		vnzCacheLine(VNZ_CACHE_WRITEBACK, &jobs[i]);
	}

	physical->ready = (VkJobList *)((uint8_t *)physical->jobs + jobsSize);
	physical->ready->count = 0;
	physical->ready->list.head = NULL;
	vnzCacheLine(VNZ_CACHE_WRITEBACK, physical->ready);
	vnzCacheLine(VNZ_CACHE_WRITEBACK, physical);
	mep_syncm();
	return context;
}

static void contextFree(VkVThreadContext *context)
{
	VkHeap *heap = context->heap;
	uint32_t size = contextSize(context->count);

	vkLockPageFree(context->lock);
	vkHeapFree(heap, (void *)vkPublicToPhysical((uint32_t)context), size, 0x40);
}

/* Run on every worker core while a context is deleted: drop its queued jobs. */
static void contextCancel(uint32_t argument, uint32_t argument2)
{
	VkVThreadContext *context = (VkVThreadContext *)argument;
	VkVThreadSpace *space = context->space;
	uint32_t index = vkCurrentCoreId() - VNZ_FIRST_CORE_ID;
	VkJob **slot = space->slot[index];
	VkVThreadSystem *system;
	VkList kept;
	VkJob *job;
	uint32_t first, removed;

	vkLockPageLock(context->lock);
	first = context->users < 1;
	context->users++;
	vkLockPageUnlock(context->lock);

	if (first) {
		kept.head = NULL;
		kept.tail = NULL;
		vkLockPageLock(space->lock);
		removed = 0;
		while ((job = (VkJob *)listTake(&space->ready)) != NULL) {
			if (job->context == context)
				removed++;
			else
				listAppend(&kept, (VkListNode *)job);
		}
		listMove(&kept, &space->ready);
		vkLockPageUnlock(space->lock);

		system = context->resource->system;
		vkLockPageLock(system->lock);
		system->jobsGiven -= removed;
		vkLockPageUnlock(system->lock);
	}

	if (*slot && (*slot)->context == context)
		*slot = NULL;
	(void)argument2;
}

/* The end of a context's cancelling broadcast: free it when its deletion is done. */
static void contextCancelled(uint32_t argument, uint32_t argument2)
{
	VkVThreadContext *context = (VkVThreadContext *)argument;
	volatile int8_t *deferred = (volatile int8_t *)&context->deferredFree;
	volatile int32_t i;

	(void)argument2;
	if (context->users < 1) {
		context->users = -1;
		return;
	}
	while (*deferred != 1)
		for (i = 0; i < 100; i++)
			;
	contextFree(context);
}

/* Delete a context: its queued jobs are dropped, its running jobs finish. */
void vkVThreadContextDelete(VkVThreadContext *context)
{
	VkVThreadSpace *space;
	VkJob *volatile *current;
	uint32_t running = 0, core;
	volatile int32_t i;

	vkDisableDispatch();
	vkLockPageLock(context->lock);
	context->deleting = 1;
	vkLockPageUnlock(context->lock);
	context->deferredFree = 0;
	context->users = 0;
	if (vkVThreadBroadcastTo(context->resource, contextCancel, (uint32_t)context, 0,
	    contextCancelled, (uint32_t)context, 0, VK_BROADCAST_WORKERS) != 0) {
		vkEnableDispatch();
		return;
	}

	space = context->space;
	vkLockPageLock(space->lock);
	for (core = 0; core < VNZ_CORES; core++) {
		if (space->current[core] && space->current[core]->context == context) {
			running = 1;
			break;
		}
	}
	vkLockPageUnlock(space->lock);
	vkEnableDispatch();

	if (running == 1) {
		for (core = 0; core < VNZ_CORES; core++) {
			current = (VkJob *volatile *)&context->space->current[core];
			while (*current && (*current)->context == context)
				for (i = 0; i < 100; i++)
					;
		}
	}

	context->magic = 0;
	if (context->users != -1)
		context->deferredFree = 1;
	else
		contextFree(context);
}

/* Lock a context for setting its jobs; returns it physically. */
VkVThreadContext *vkVThreadContextLock(VkVThreadContext *context)
{
	vkDisableDispatch();
	vkLockPageLock(context->lock);
	return (VkVThreadContext *)vkPublicToPhysical((uint32_t)context);
}

/* Hand the jobs that became ready to the workers. */
static int32_t contextFlush(VkVThreadContext *context, VkJobList *ready)
{
	uint32_t index = vkCurrentCoreId() - VNZ_FIRST_CORE_ID;
	VkVThreadSpace *space = context->space;
	VkVThreadSystem *system;
	VkThread *waiting[VNZ_CORES];
	VkJob **slot = space->slot[index];
	uint32_t count;

	if (space->current[index] && *slot == NULL) {
		/* this core is in a job: it takes the first itself */
		*slot = (VkJob *)listTake(&ready->list);
		ready->count--;
	}
	if (ready->list.head == NULL)
		return 0;

	system = context->resource->system;
	vkLockPageLock(space->lock);
	listMove(&ready->list, &space->ready);
	vkLockPageLock(system->lock);
	count = takeWaiting(system, waiting);
	system->jobsGiven += ready->count;
	ready->count = 0;
	vkLockPageUnlock(system->lock);
	vkLockPageUnlock(space->lock);
	wakeAll(count, waiting);
	return 0;
}

/* Unlock a context, starting the jobs that became ready. */
int32_t vkVThreadContextUnlock(VkVThreadContext *context)
{
	volatile uint8_t *lock = context->lock;
	int32_t result = 0;

	if (context->ready->list.head && context->deleting != 1)
		result = contextFlush(context, context->ready);
	vnzCacheLine(VNZ_CACHE_WRITEBACK, context->ready);
	vnzCacheLine(VNZ_CACHE_WRITEBACK, context);
	vkLockPageUnlock(lock);
	vkEnableDispatch();
	return result;
}

/*
 * Set a job (the context locked): it runs function once it has had wanted
 * signals. Returns 0, or the job's state when it is in use.
 */
uint32_t vkVThreadJobSet(VkVThreadContext *context, uint32_t index, uint32_t wanted,
	VkJobFunction function, uint32_t argument, uint32_t argument2, uint32_t argument3,
	uint32_t argument4)
{
	VkJob *job = &context->jobsPhysical[index];

	if (job->state) {
		vnzCacheLine(VNZ_CACHE_PURGE, job);
		return job->state;
	}
	job->counter->wanted = wanted;
	job->function = function;
	job->arguments[0] = argument;
	job->arguments[1] = argument2;
	job->arguments[2] = argument3;
	job->arguments[3] = argument4;
	job->notifyFlags = 0;
	job->notify = NULL;

	if (job->counter->done < wanted) {
		job->state = VK_JOB_WAITING;
		vnzCacheLine(VNZ_CACHE_WRITEBACK, job);
	} else {
		job->state = VK_JOB_READY;
		vnzCacheLine(VNZ_CACHE_WRITEBACK, job);
		listAppend(&context->ready->list, (VkListNode *)&context->jobs[index]);
		context->ready->count++;
	}
	vkTrace(VK_TRACE_JOB_SET, (uint32_t)context, index, wanted, VK_TRACE_JOBS);
	return 0;
}

/* The same, sending notify notifyFlags when the job has run. */
uint32_t vkVThreadJobSetNotify(VkVThreadContext *context, uint32_t index, uint32_t wanted,
	VkJobFunction function, uint32_t argument, uint32_t argument2, uint32_t argument3,
	uint32_t argument4, VkThread *notify, uint32_t notifyFlags)
{
	uint32_t result;
	VkJob *job;

	result = vkVThreadJobSet(context, index, wanted, function, argument, argument2,
		argument3, argument4);
	if (result == 0) {
		job = &context->jobsPhysical[index];
		job->notify = notify;
		job->notifyFlags = notifyFlags;
		vnzCacheLine(VNZ_CACHE_WRITEBACK, job);
	}
	return result;
}

/* Signal a job (the context locked); it becomes ready with its last signal. */
int32_t vkVThreadJobSignal(VkVThreadContext *context, uint32_t index)
{
	VkJobCounter *counter = &context->counters[index];
	uint32_t done = ++counter->done;
	VkJob *job;

	if (counter->wanted == done) {
		job = &context->jobsPhysical[index];
		job->state = VK_JOB_READY;
		vnzCacheLine(VNZ_CACHE_WRITEBACK, job);
		listAppend(&context->ready->list, (VkListNode *)&context->jobs[index]);
		context->ready->count++;
	}
	vkTrace(VK_TRACE_JOB_SIGNAL, (uint32_t)context, index, done, VK_TRACE_SIGNALS);
	return 0;
}

/* Reset a job that is idle or waiting. Returns 0, or its state. */
uint32_t vkVThreadJobClear(VkVThreadContext *context, uint32_t index)
{
	VkJob *job = &context->jobsPhysical[index];

	if (job->state != VK_JOB_IDLE && job->state != VK_JOB_WAITING) {
		vnzCacheLine(VNZ_CACHE_PURGE, job);
		return job->state;
	}
	job->state = VK_JOB_IDLE;
	job->counter->done = 0;
	job->counter->wanted = 0;
	job->notifyFlags = 0;
	job->notify = NULL;
	vnzCacheLine(VNZ_CACHE_WRITEBACK, job);
	return 0;
}

uint32_t vkVThreadJobState(VkVThreadContext *context, uint32_t index)
{
	return context->jobs[index].state;
}

uint32_t vkVThreadJobStateUnlocked(VkVThreadContext *context, uint32_t index)
{
	return context->jobs[index].state;
}

/*
 * Broadcasts.
 */

static VkBroadcast *broadcastCreate(VkBroadcastFunction function, uint32_t argument,
	uint32_t argument2, VkBroadcastFunction done, uint32_t doneArgument, uint32_t doneArgument2)
{
	VkHeap *heap = vkCurrentHeap();
	VkBroadcast *broadcast = vkHeapAllocate(heap, sizeof(*broadcast), 0x40);

	if (broadcast == NULL)
		return NULL;
	broadcast->argument2 = argument2;
	broadcast->argument = argument;
	broadcast->completed = 0;
	broadcast->function = function;
	broadcast->done = done;
	broadcast->doneArgument = doneArgument;
	broadcast->heap = heap;
	broadcast->doneArgument2 = doneArgument2;
	vkCacheWriteback((uint32_t)broadcast, 0x80);
	return (VkBroadcast *)vkPhysicalToPublic((uint32_t)broadcast);
}

static void broadcastFree(VkBroadcast *broadcast)
{
	VkHeap *heap = broadcast->heap;

	vkHeapFree(heap, (void *)vkPublicToPhysical((uint32_t)broadcast), sizeof(*broadcast), 0x40);
}

/* Queue a broadcast to each core's list in the space. */
static void broadcastQueue(VkVThreadResource *resource, VkVThreadSpace *space,
	VkBroadcast *broadcast, uint32_t mode)
{
	VkVThreadSystem *system = resource->system;
	volatile uint8_t *spaceLock = space->lock;
	volatile uint8_t *systemLock = system->lock;
	VkThread *waiting[VNZ_CORES];
	uint32_t core, eligible, count = 0, woken;

	vkDisableDispatch();
	vkLockPageLock(spaceLock);
	vkLockPageLock(systemLock);
	for (core = 0; core < VNZ_CORES; core++) {
		if (mode == VK_BROADCAST_ENABLED)
			eligible = system->enabledCores & (1 << core);
		else
			eligible = system->workers[core] != NULL;
		if (eligible) {
			listAppend(&space->broadcasts[core], &broadcast->next[core]);
			system->broadcastsQueued[core]++;
			count++;
		} else {
			broadcast->next[core].next = NULL;
		}
	}
	broadcast->total = count;

	if ((int32_t)count >= 1) {
		woken = takeWaiting(resource->system, waiting);
		vkLockPageUnlock(systemLock);
		vkLockPageUnlock(spaceLock);
		wakeAll(woken, waiting);
	} else {
		vkLockPageUnlock(systemLock);
		vkLockPageUnlock(spaceLock);
		if (broadcast->done)
			broadcast->done(broadcast->doneArgument, broadcast->doneArgument2);
		broadcastFree(broadcast);
	}
	vkEnableDispatch();
}

/*
 * Run function(argument, argument2) once on each core the mode names; done
 * runs after the last. Returns 0, or -1 when there is no memory.
 */
int32_t vkVThreadBroadcastTo(VkVThreadResource *resource, VkBroadcastFunction function,
	uint32_t argument, uint32_t argument2, VkBroadcastFunction done,
	uint32_t doneArgument, uint32_t doneArgument2, uint32_t mode)
{
	VkBroadcast *broadcast;

	broadcast = broadcastCreate(function, argument, argument2, done, doneArgument,
		doneArgument2);
	if (broadcast == NULL)
		return -1;
	broadcastQueue(resource, resource->space, broadcast, mode);
	return 0;
}

int32_t vkVThreadBroadcast(VkVThreadResource *resource, VkBroadcastFunction function,
	uint32_t argument, uint32_t argument2)
{
	return vkVThreadBroadcastTo(resource, function, argument, argument2, NULL, 0, 0,
		VK_BROADCAST_ENABLED);
}

int32_t vkVThreadBroadcastDone(VkVThreadResource *resource, VkBroadcastFunction function,
	uint32_t argument, uint32_t argument2, VkBroadcastFunction done,
	uint32_t doneArgument, uint32_t doneArgument2)
{
	return vkVThreadBroadcastTo(resource, function, argument, argument2, done,
		doneArgument, doneArgument2, VK_BROADCAST_ENABLED);
}

/* A space's per-core job: run the broadcasts queued for this core. */
static void runBroadcasts(uint32_t argument, uint32_t argument2, uint32_t argument3,
	uint32_t argument4)
{
	VkVThreadSpace *space = (VkVThreadSpace *)argument;
	VkVThreadSystem *system = (VkVThreadSystem *)argument2;
	uint32_t index = vkCurrentCoreId() - VNZ_FIRST_CORE_ID;
	volatile uint8_t *lock = space->lock;
	VkList *list = &space->broadcasts[index];
	VkBroadcast *broadcast;
	uint32_t ran = 0, completed;

	(void)argument3;
	(void)argument4;
	for (;;) {
		vkLockPageLock(lock);
		broadcast = (VkBroadcast *)((uint32_t)listTake(list) & ~0x3f);
		vkLockPageUnlock(lock);
		if (broadcast == NULL)
			break;
		broadcast->function(broadcast->argument, broadcast->argument2);

		vkLockPageLock(lock);
		completed = ++broadcast->completed;
		vkLockPageUnlock(lock);
		if (broadcast->total == completed) {
			if (broadcast->done)
				broadcast->done(broadcast->doneArgument, broadcast->doneArgument2);
			broadcastFree(broadcast);
		}
		ran++;
	}

	vkLockPageLock(system->lock);
	system->broadcastsRun[index] += ran;
	vkLockPageUnlock(system->lock);
}

/*
 * Resources.
 */

static int32_t spaceInitialize(VkVThreadSpace *space, VkHeap *heap, VkVThreadSystem *system)
{
	VkJob *job;
	uint32_t i;

	space->lock = vkLockPageAllocate();
	if (space->lock == NULL)
		return -1;
	space->ready.head = NULL;

	space->jobs = vkHeapAllocate(heap, VNZ_CORES * sizeof(VkJob), 0x40);
	space->jobs = (VkJob *)vkPhysicalToPublic((uint32_t)space->jobs);
	if (space->jobs == NULL)
		goto noJobs;
	space->counters = vkHeapAllocate(heap, VNZ_CORES * sizeof(VkJobCounter), 0x40);
	space->counters = (VkJobCounter *)vkPhysicalToPublic((uint32_t)space->counters);
	if (space->counters == NULL)
		goto noCounters;

	for (i = 0; i < VNZ_CORES; i++) {
		space->queue[i] = NULL;
		space->slot[i] = (VkJob **)&space->slot[i];
		space->current[i] = NULL;
		space->broadcasts[i].head = NULL;
		space->counters[i].wanted = 0;
		space->counters[i].done = 0;

		job = &space->jobs[i];
		job->notifyFlags = 0;
		job->notify = NULL;
		job->arguments[3] = 0;
		job->arguments[2] = 0;
		job->arguments[1] = (uint32_t)system;
		job->arguments[0] = (uint32_t)space;
		job->function = runBroadcasts;
		job->state = VK_JOB_IDLE;
		job->counter = &space->counters[i];
		job->context = NULL;
		job->index = i;
		job->next = NULL;
	}
	return 0;

noCounters:
	vkHeapFree(heap, (void *)vkPublicToPhysical((uint32_t)space->jobs),
		VNZ_CORES * sizeof(VkJob), 0x40);
noJobs:
	vkLockPageFree(space->lock);
	return -1;
}

static void spaceRelease(VkVThreadSpace *space, VkHeap *heap)
{
	vkHeapFree(heap, (void *)vkPublicToPhysical((uint32_t)space->counters),
		VNZ_CORES * sizeof(VkJobCounter), 0x40);
	vkHeapFree(heap, (void *)vkPublicToPhysical((uint32_t)space->jobs),
		VNZ_CORES * sizeof(VkJob), 0x40);
	vkLockPageFree(space->lock);
}

static int32_t resourceInitialize(VkVThreadResource *resource, VkVThreadSystem *system,
	VkVThreadSpace *space, uint32_t stackSize)
{
	resource->lock = vkLockPageAllocate();
	if (resource->lock == NULL)
		return -1;
	resource->stackSize = stackSize;
	resource->space = space;
	resource->system = system;
	resource->owner = NULL;
	resource->users = 0;
	resource->next = NULL;
	resource->magic = VK_RESOURCE_MAGIC;
	return 0;
}

static void resourceRelease(VkVThreadResource *resource)
{
	resource->magic = 0;
	vkLockPageFree(resource->lock);
}

/* Take a resource out of the ring. */
static void resourceRemove(VkVThreadSystem *system, VkVThreadResource *resource)
{
	VkVThreadResource *previous, *at;

	if (system->head == system->tail) {
		if (system->head == resource) {
			system->head = NULL;
			system->tail = NULL;
		}
	} else if (system->head == resource) {
		system->head = resource->next;
		system->tail->next = resource->next;
	} else {
		previous = NULL;
		at = system->head;
		do {
			if (at->next == resource) {
				previous = at;
				break;
			}
			at = at->next;
		} while (at != system->head);
		if (previous) {
			previous->next = resource->next;
			if (system->tail == resource)
				system->tail = previous;
		}
	}
	resource->next = NULL;
}

/* Create a processing resource whose jobs run on stacks of stackSize. Returns it, or NULL. */
VkVThreadResource *vkVThreadResourceCreate(uint32_t stackSize)
{
	VkVThreadSystem *system = &g_vkVThreads;
	VkHeap *heap = vkCurrentHeap();
	VkVThreadResource *resource;
	VkVThreadSpace *space;
	void *memory;

	memory = vkHeapAllocate(heap, sizeof(VkVThreadSpace), 0x40);
	if (memory == NULL)
		return NULL;
	space = (VkVThreadSpace *)vkPhysicalToPublic((uint32_t)memory);
	if (spaceInitialize(space, heap, system) != 0)
		goto noSpace;

	memory = vkHeapAllocate(heap, sizeof(VkVThreadResource), 0x40);
	if (memory == NULL)
		goto noResource;
	resource = (VkVThreadResource *)vkPhysicalToPublic((uint32_t)memory);
	if (resourceInitialize(resource, system, space, stackSize) != 0) {
		vkHeapFree(heap, (void *)vkPublicToPhysical((uint32_t)resource),
			sizeof(VkVThreadResource), 0x40);
		goto noResource;
	}

	vkDisableDispatch();
	vkLockPageLock(system->lock);
	if (system->head) {
		resource->next = system->head;
		system->tail->next = resource;
	} else {
		system->head = resource;
		resource->next = resource;
	}
	system->tail = resource;
	vkLockPageUnlock(system->lock);
	vkEnableDispatch();
	return resource;

noResource:
	spaceRelease(space, heap);
noSpace:
	vkHeapFree(heap, (void *)vkPublicToPhysical((uint32_t)space), sizeof(VkVThreadSpace), 0x40);
	return NULL;
}

/* Delete a resource once its work is done; waits for it. */
void vkVThreadResourceDelete(VkVThreadResource *resource)
{
	VkVThreadSystem *system = &g_vkVThreads;
	volatile uint8_t *lock;
	VkThread *waiting[VNZ_CORES];
	uint32_t count = 0, core, busy;
	VkHeap *heap;

	if (resource == NULL)
		return;
	lock = system->lock;
	vkDisableDispatch();
	for (;;) {
		vkLockPageLock(lock);
		busy = resource->users != 0;
		for (core = 0; !busy && core < VNZ_CORES; core++)
			busy = resource->space->broadcasts[core].head != NULL;
		if (!busy)
			break;
		resource->owner = vkThreadCurrent();
		vkLockPageUnlock(lock);
		vkThreadSleep();
	}
	resourceRemove(system, resource);
	if (system->head == NULL)
		count = takeWaiting(system, waiting);
	vkLockPageUnlock(lock);
	vkEnableDispatch();
	if (count)
		wakeAll(count, waiting);

	heap = vkCurrentHeap();
	spaceRelease(resource->space, heap);
	vkHeapFree(heap, (void *)vkPublicToPhysical((uint32_t)resource->space),
		sizeof(VkVThreadSpace), 0x40);
	resourceRelease(resource);
	vkHeapFree(heap, (void *)vkPublicToPhysical((uint32_t)resource),
		sizeof(VkVThreadResource), 0x40);
}

/*
 * The workers.
 */

/* Pick this core's next job in a resource's space. */
static VkJob *fetchJob(VkVThreadResource *resource, VkVThreadSpace *space, VkJob **slot)
{
	VkVThreadSystem *system = resource->system;
	uint32_t index = vkCurrentCoreId() - VNZ_FIRST_CORE_ID;
	VkList *queue = space->queue[index];
	volatile uint8_t *spaceLock = space->lock;
	volatile uint8_t *systemLock;
	VkVThreadResource *next;
	VkJob *job;
	uint32_t core;

	vkDisableDispatch();
	vkLockPageLock(spaceLock);
	if (space->broadcasts[index].head) {
		job = &space->jobs[index];
	} else if (*slot) {
		job = *slot;
		*slot = NULL;
	} else {
		job = (VkJob *)listTake(queue);
		systemLock = system->lock;
		vkLockPageLock(systemLock);
		if (job) {
			system->jobsTaken++;
		} else {
			if (queue != &system->idleQueue && system->head == resource &&
			    resource->next != resource) {
				/* this resource has run dry: the next one has the cores */
				next = resource->next;
				system->head = next;
				system->tail = resource;
				for (core = 0; core < VNZ_CORES; core++) {
					if (system->workers[core] == NULL ||
					    !(system->enabledCores & (1 << core)))
						continue;
					resource->space->queue[core] = &system->idleQueue;
					resource->space->slot[core] =
						(VkJob **)&resource->space->slot[core];
				}
			}
			addWaiting(system, vkThreadCurrent());
		}
		vkLockPageUnlock(systemLock);
	}
	space->current[index] = job;
	vkLockPageUnlock(spaceLock);
	vkEnableDispatch();
	return job;
}

static void runJob(uint32_t index, VkVThreadResource *resource, VkJob *job)
{
	VkJob *physical;
	VkThread *notify;

	job->state = VK_JOB_RUNNING;
	physical = (VkJob *)vkPublicToPhysical((uint32_t)job);
	vkTrace(VK_TRACE_JOB_START, (uint32_t)physical->context, physical->index,
		(uint32_t)physical->function, VK_TRACE_JOB_RUNS);
	vkThreadSetUserWord((uint32_t)physical->function);
	physical->function(physical->arguments[0], physical->arguments[1],
		physical->arguments[2], physical->arguments[3]);
	vkThreadSetUserWord(0);
	vkTrace(VK_TRACE_JOB_END, (uint32_t)physical->context, physical->index,
		(uint32_t)physical->function, VK_TRACE_JOB_RUNS);

	notify = NULL;
	if (physical->notifyFlags) {
		notify = physical->notify;
		physical->notify = NULL;
		physical->notifyFlags = 0;
	}
	physical->state = VK_JOB_IDLE;
	physical->counter->done = 0;
	physical->counter->wanted = 0;
	resource->space->current[index] = NULL;
	vnzCacheLine(VNZ_CACHE_WRITEBACK, physical);
	if (notify)
		vkThreadSetFlags(notify, physical->notifyFlags);
}

/* Run a resource's jobs until it has none for this core (on the resource's stack). */
void vkVThreadServe(VkVThreadSystem *system, VkVThreadResource *resource)
{
	uint32_t index = vkCurrentCoreId() - VNZ_FIRST_CORE_ID;
	VkJob *(*fetch)(VkVThreadResource *, VkVThreadSpace *, VkJob **) = system->fetch;
	VkVThreadSpace *space = resource->space;
	VkJob **slot = &system->stacks[index]->slot;
	VkJob *job;

	*slot = NULL;
	for (;;) {
		vkEnableDispatch();
		vkDisableDispatch();
		job = fetch(resource, space, slot);
		if (job == NULL)
			return;
		runJob(index, resource, job);
	}
}

/* Serve a resource on a stack of its size. */
static void serveResource(VkVThreadSystem *system, VkVThreadResource *resource)
{
	uint32_t index = vkCurrentCoreId() - VNZ_FIRST_CORE_ID;
	VkWorkerStack *stack = system->stacks[index];
	uint32_t size = stack->size;
	VkHeap *heap;

	if (size < resource->stackSize) {
		heap = vkCurrentHeap();
		if (size)
			vkHeapFreeStack(heap, stack->base, size);
		size = resource->stackSize;
		stack->base = vkHeapAllocateStack(heap, size);
		if (stack->base == NULL) {
			stack->base = NULL;
			stack->size = 0;
			return;
		}
		stack->size = size;
	}
	vkDisableDispatch();
	vkCallOnStack(system, resource, (uint8_t *)stack->base + size);
	vkEnableDispatch();
}

/* A worker leaves: its core no longer serves. */
static void workerExit(uint32_t index, VkVThreadSystem *system)
{
	removeWaiting(system, vkThreadCurrent());
	system->workers[index] = NULL;
	system->workerCount--;
	system->runningCores &= ~(1 << index);
	if (system->enabledCores == system->runningCores) {
		system->settled = 1;
		notifyCoreEvents(system);
	}
}

/* A core's worker thread. */
static void workerThread(uint32_t argument)
{
	VkVThreadSystem *system = (VkVThreadSystem *)argument;
	uint32_t index = vkCurrentCoreId() - VNZ_FIRST_CORE_ID;
	uint32_t bit = 1 << index;
	volatile uint8_t *lock = system->lock;
	VkThread *waiting[VNZ_CORES];
	VkVThreadResource *resource;
	VkWorkerStack *stack;
	uint32_t idle, mine, count;
	VkHeap *heap;

	stack = vkHeapAllocateStack(vkCurrentHeap(), sizeof(*stack));
	if (stack == NULL)
		return;
	stack->base = NULL;
	stack->size = 0;
	stack->slot = NULL;
	system->stacks[index] = stack;
	system->workerStart(system);

	vkDisableDispatch();
	vkLockPageLock(lock);
	resource = system->head;
	if (!(system->enabledCores & bit) && resource == NULL)
		goto leave;

	for (;;) {
		/* serve the ring from resource on */
		while (resource) {
			resource->users++;
			if (system->enabledCores & bit) {
				resource->space->queue[index] = &resource->space->ready;
				resource->space->slot[index] = &stack->slot;
			} else {
				resource->space->queue[index] = &system->idleQueue;
				resource->space->slot[index] =
					(VkJob **)&resource->space->slot[index];
			}
			vkLockPageUnlock(lock);
			vkEnableDispatch();
			serveResource(system, resource);
			vkDisableDispatch();
			vkLockPageLock(lock);

			idle = system->jobsGiven == system->jobsTaken;
			mine = system->broadcastsQueued[index] == system->broadcastsRun[index];
			if (!(idle && mine)) {
				removeWaiting(system, vkThreadCurrent());
				vkThreadClearFlags(VK_VTHREAD_WAKE);
				if (!idle) {
					count = takeWaiting(system, waiting);
					if ((int32_t)count >= 1)
						wakeAll(count, waiting);
				}
				idle = 0;
			}

			if (resource->users == 1 && resource->owner) {
				for (count = 0; count < VNZ_CORES; count++)
					if (resource->space->broadcasts[count].head)
						break;
				if (count == VNZ_CORES) {
					while (vkThreadWakeup(resource->owner, 0) != 0)
						;
					resourceRemove(system, resource);
				}
			}
			resource->users--;

			while (system->enabledCores & bit) {
				if (!idle)
					goto restart;
				vkLockPageUnlock(lock);
				vkThreadWaitFlags(VK_VTHREAD_WAKE);
				vkLockPageLock(lock);
				mine = system->broadcastsQueued[index] == system->broadcastsRun[index];
				idle = 0;
			}
			if (mine)
				goto leave;
			resource = resource->next;
		}

		/* nothing to serve: wait */
		addWaiting(system, vkThreadCurrent());
		vkLockPageUnlock(lock);
		if (stack->base) {
			vkHeapFreeStack(vkCurrentHeap(), stack->base, stack->size);
			stack->size = 0;
			stack->base = NULL;
		}
		vkThreadWaitFlags(VK_VTHREAD_WAKE);
		vkLockPageLock(lock);
restart:
		resource = system->head;
		if (!(system->enabledCores & bit) && resource == NULL)
			break;
	}

leave:
	workerExit(index, system);
	vkLockPageUnlock(lock);
	vkEnableDispatch();
	heap = vkCurrentHeap();
	if (stack->base) {
		vkHeapFreeStack(heap, stack->base, stack->size);
		stack->size = 0;
		stack->base = NULL;
	}
	vkHeapFreeStack(heap, stack, sizeof(*stack));
	system->workerEnd(system);
}

/* Start a worker on core coreId. Returns 0, or -1. */
static int32_t workerCreate(VkVThreadSystem *system, uint32_t coreId)
{
	uint32_t index = coreId - VNZ_FIRST_CORE_ID;
	int32_t thread;

	if (system->workers[index])
		return 0;
	thread = vkThreadCreateHere(workerThread, coreId, VK_VTHREAD_WORKER_PRIORITY,
		VK_VTHREAD_WORKER_STACK);
	if (thread == -1) {
		system->workers[index] = NULL;
		return -1;
	}
	system->workers[index] = (VkThread *)thread;
	vkThreadStart((VkThread *)thread, (uint32_t)system, 0);
	vkThreadDetach((VkThread *)thread);
	system->workerCount++;
	return 0;
}

static void workerHookNothing(VkVThreadSystem *system)
{
	(void)system;
}

/* Set up the workers' state (the primary core, at boot). */
void vkVThreadInitialize(void)
{
	VkVThreadSystem *system = &g_vkVThreads;
	uint32_t core;

	system->lock = vkLockPageAllocate();
	system->workerCount = 0;
	system->waitingCount = 0;
	system->jobsGiven = 0;
	system->jobsTaken = 0;
	for (core = 0; core < VNZ_CORES; core++) {
		system->workers[core] = NULL;
		system->waiting[core] = NULL;
		system->stacks[core] = NULL;
		system->broadcastsQueued[core] = 0;
		system->broadcastsRun[core] = 0;
	}
	system->enabledCores = 0;
	system->previousCores = 0;
	system->runningCores = 0;
	system->fetch = fetchJob;
	system->workerEnd = workerHookNothing;
	system->workerStart = workerHookNothing;
	system->tail = NULL;
	system->head = NULL;

	for (core = 0; core < VNZ_CORES; core++)
		if (vkCoreType(VNZ_FIRST_CORE_ID + core) & VK_CORE_VTHREADS)
			system->enabledCores |= 1 << core;
	system->settled = system->enabledCores == 0;

	system->idleQueue.head = NULL;
	system->coreEvents.tail = NULL;
	system->coreEvents.head = NULL;
}

/* Start this core's worker when the core is enabled (each core, at its first start). */
void vkVThreadStartWorker(void)
{
	VkVThreadSystem *system = &g_vkVThreads;
	uint32_t coreId = vkCurrentCoreId();
	uint32_t bit = 1 << (coreId - VNZ_FIRST_CORE_ID);
	volatile uint8_t *lock;

	if (system->settled || !(system->enabledCores & bit))
		return;
	lock = system->lock;
	vkDisableDispatch();
	vkLockPageLock(lock);
	if (workerCreate(system, coreId) == 0)
		system->runningCores |= bit;
	if (system->enabledCores == system->runningCores) {
		system->settled = 1;
		notifyCoreEvents(system);
	}
	vkLockPageUnlock(lock);
	vkEnableDispatch();
}

/*
 * Change the cores the workers run on, from expected to cores. Returns 0, or
 * -1 when the cores are not expected or a change is under way.
 */
int32_t vkVThreadSetCores(uint32_t expected, uint32_t cores)
{
	VkVThreadSystem *system = &g_vkVThreads;
	volatile uint8_t *lock = system->lock;
	uint32_t running, bit, core;
	VkThread *worker;
	int32_t result;

	vkDisableDispatch();
	vkLockPageLock(lock);
	if (system->enabledCores != expected || system->settled != 1) {
		result = -1;
		goto out;
	}
	system->settled = 0;
	if (expected != cores) {
		system->enabledCores = cores;
		system->previousCores = expected;
		running = system->runningCores;
		for (core = 0; core < VNZ_CORES; core++) {
			bit = 1 << core;
			if ((running & bit) == (cores & bit))
				continue;
			if (cores & bit) {
				if (workerCreate(system, VNZ_FIRST_CORE_ID + core) == 0)
					system->runningCores |= bit;
				continue;
			}
			/* stop: the head resource's queue for the core runs dry */
			if (system->head) {
				system->head->space->queue[core] = &system->idleQueue;
				system->head->space->slot[core] =
					(VkJob **)&system->head->space->slot[core];
			}
			worker = system->workers[core];
			if (removeWaiting(system, worker))
				vkThreadSetFlags(worker, VK_VTHREAD_WAKE);
		}
	}
	if (system->runningCores == cores) {
		system->settled = 1;
		notifyCoreEvents(system);
	}
	result = 0;
out:
	vkLockPageUnlock(lock);
	vkEnableDispatch();
	return result;
}

uint32_t vkVThreadCores(void)
{
	VkVThreadSystem *system = &g_vkVThreads;
	uint32_t cores;

	vkDisableDispatch();
	vkLockPageLock(system->lock);
	cores = system->runningCores;
	vkLockPageUnlock(system->lock);
	vkEnableDispatch();
	return cores;
}

/* An event a thread gets (flags) when a change of cores has settled. */
VkCoreEvent *vkVThreadCoreEventCreate(VkThread *thread, uint32_t flags)
{
	VkCoreEvent *event = vkHeapAllocate(vkCurrentHeap(), sizeof(*event), 0x40);

	if (event == NULL)
		return NULL;
	event->thread = thread;
	event->flags = flags;
	event->next = NULL;
	event->previous = NULL;
	event->magic = VK_CORE_EVENT_MAGIC;
	vkCacheWriteback((uint32_t)event, 0x40);
	return (VkCoreEvent *)vkPhysicalToPublic((uint32_t)event);
}

static uint32_t coreEventListed(VkCoreEventList *list, VkCoreEvent *event)
{
	return event->previous || event->next || list->head == event;
}

void vkVThreadCoreEventRegister(VkCoreEvent *event)
{
	VkVThreadSystem *system = &g_vkVThreads;
	volatile uint8_t *lock = system->lock;
	VkCoreEventList *list = &system->coreEvents;

	vkDisableDispatch();
	vkLockPageLock(lock);
	if (!coreEventListed(list, event)) {
		if (list->head) {
			list->tail->next = event;
			event->previous = list->tail;
		} else {
			list->head = event;
		}
		list->tail = event;
	}
	vkLockPageUnlock(lock);
	vkEnableDispatch();
}

void vkVThreadCoreEventUnregister(VkCoreEvent *event)
{
	VkVThreadSystem *system = &g_vkVThreads;
	volatile uint8_t *lock = system->lock;
	VkCoreEventList *list = &system->coreEvents;

	vkDisableDispatch();
	vkLockPageLock(lock);
	if (coreEventListed(list, event)) {
		if (event->previous)
			event->previous->next = event->next;
		else
			list->head = event->next;
		if (event->next)
			event->next->previous = event->previous;
		else
			list->tail = event->previous;
		event->next = NULL;
		event->previous = NULL;
	}
	vkLockPageUnlock(lock);
	vkEnableDispatch();
}

void vkVThreadCoreEventDelete(VkCoreEvent *event)
{
	VkHeap *heap;

	vkVThreadCoreEventUnregister(event);
	event->magic = 0;
	heap = vkCurrentHeap();
	vkHeapFree(heap, (void *)vkPublicToPhysical((uint32_t)event), sizeof(*event), 0x40);
}
