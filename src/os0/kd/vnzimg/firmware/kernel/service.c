/*
 * Moving a thread to another core.
 *
 * Each core runs a service thread (priority 9). A move is queued to the
 * thread's core, whose service thread takes the thread off the core and stands
 * in for its interrupt handlers (vkInterruptsHook), then queues the move to the
 * target core, whose service thread puts the thread on that core and moves the
 * handlers there (vkInterruptsMove). The move semaphore keeps the moves in order.
 */
#include "kernel.h"

#define VK_SERVICE_OUTGOING	0x2	/* a move off this core is queued */
#define VK_SERVICE_INCOMING	0x1	/* a move to this core is queued */

VkMigrationQueue g_vkServiceQueues[VNZ_CORES][2] VK_SHARED;	/* outgoing, incoming */
VkThread *g_vkServiceThreads[VNZ_CORES] VK_SHARED;

static volatile uint32_t *const moveSemaphore = vkSemaphore(VK_SEMAPHORE_MOVES);

static void migrationAppend(VkMigrationQueue *queue, VkMigration *migration)
{
	if (queue->head)
		queue->tail->next = migration;
	else
		queue->head = migration;
	queue->tail = migration;
}

static VkMigration *migrationTake(VkMigrationQueue *queue)
{
	VkMigration *migration = queue->head;

	if (migration) {
		queue->head = migration->next;
		if (queue->head == NULL)
			queue->tail = NULL;
		migration->next = NULL;
	}
	return migration;
}

/*
 * Move a thread to the core coreId; notify is sent notifyFlags when it has
 * arrived. Returns 0, or -1 when the thread is already moving, its interrupts
 * are taken on that core, or there is no memory.
 */
int32_t vkThreadMigrate(VkThread *thread, uint32_t coreId, VkThread *notify, uint32_t notifyFlags)
{
	VkHeap *heap = vkSystemHeap();
	VkMigration *migration;
	VkCore *core;
	uint32_t index, from, to;

	migration = vkHeapAllocate(heap, sizeof(*migration), 0x40);
	if (migration == NULL)
		return -1;
	migration = (VkMigration *)vkPhysicalToPublic((uint32_t)migration);
	migration->lock = vkLockPageAllocate();
	if (migration->lock) {
		vkDisableDispatch();
		core = vkCurrentCore(&index);
		core->inKernel = 1;
		vkLockThread(thread);
		if (thread->attributes != VK_THREAD_MOVING) {
			from = thread->core;
			to = coreId - VNZ_FIRST_CORE_ID;
			if (vkInterruptsReserve(to, thread->interrupts) == 0) {
				thread->attributes = VK_THREAD_MOVING;
				migration->next = NULL;
				migration->thread = thread;
				migration->from = thread->core;
				migration->to = to;
				migration->notify = notify;
				migration->notifyFlags = notifyFlags;
				migrationAppend(&g_vkServiceQueues[from][0], migration);
				vkUnlockThread(thread);
				vkProcessEvents(core);
				if (g_vkServiceThreads[from])
					vkThreadSetFlags(g_vkServiceThreads[from], VK_SERVICE_OUTGOING);
				vkEnableDispatch();
				return 0;
			}
		}
		vkUnlockThread(thread);
		vkProcessEvents(core);
		vkEnableDispatch();
		vkLockPageFree(migration->lock);
	}
	vkHeapFree(heap, (void *)vkPublicToPhysical((uint32_t)migration), sizeof(*migration), 0x40);
	return -1;
}

/* Take a moving thread off this core and pass it on. */
static void sendThread(uint32_t index, VkCore *core, VkMigration *migration)
{
	VkThread *thread = migration->thread;
	uint32_t from = thread->core;
	uint32_t to;

	vkInterruptsHook(migration);
	thread->core = VK_NO_CORE;
	if ((thread->state & VK_STATE_MASK) == VK_STATE_READY)
		vkQueueRemove(&g_vkRunQueues[from][thread->priority - 1], thread);
	g_vkThreadCounts[index]--;
	vkGiveSemaphore(vkSemaphore(VK_SEMAPHORE_THREADS + index));

	to = migration->to;
	vkTakeSemaphore(vkSemaphore(VK_SEMAPHORE_THREADS + to));
	migrationAppend(&g_vkServiceQueues[to][1], migration);
	vkGiveSemaphore(vkSemaphore(VK_SEMAPHORE_THREADS + to));
	vkGiveSemaphore(moveSemaphore);
	vkProcessEvents(core);
	if (g_vkServiceThreads[to])
		vkThreadSetFlags(g_vkServiceThreads[to], VK_SERVICE_INCOMING);
}

/* Put a thread that moved here on this core. */
static void receiveThread(uint32_t index, VkCore *core, VkHeap *heap, VkMigration *migration)
{
	VkThread *thread = migration->thread;
	uint32_t ended = 1;

	thread->core = index;
	if ((thread->state & VK_STATE_MASK) == VK_STATE_READY)
		vkQueueAppend(&g_vkRunQueues[index][thread->priority - 1], thread);
	if (thread->state & VK_STATE_MASK) {
		g_vkThreadCounts[index]++;
		ended = 0;
	}
	thread->attributes = 0;
	vkInterruptsMove(migration);
	vkGiveSemaphore(vkSemaphore(VK_SEMAPHORE_THREADS + index));
	vkGiveSemaphore(moveSemaphore);
	vkProcessEvents(core);

	if (migration->notifyFlags)
		vkThreadSetFlags(migration->notify, migration->notifyFlags);
	if (ended)		/* it was deleted while it moved */
		vkThreadRelease(thread);
	vkLockPageFree(migration->lock);
	vkHeapFree(heap, (void *)vkPublicToPhysical((uint32_t)migration), sizeof(*migration), 0x40);
}

/* Take the next move from one of this core's queues, holding the semaphores if there is one. */
static VkMigration *nextMove(uint32_t index, VkCore *core, uint32_t incoming)
{
	VkMigration *migration;

	core->inKernel = 1;
	vkTakeSemaphore(moveSemaphore);
	vkTakeSemaphore(vkSemaphore(VK_SEMAPHORE_THREADS + index));
	migration = migrationTake(&g_vkServiceQueues[index][incoming]);
	if (migration == NULL) {
		vkGiveSemaphore(vkSemaphore(VK_SEMAPHORE_THREADS + index));
		vkGiveSemaphore(moveSemaphore);
		vkProcessEvents(core);
	}
	return migration;
}

/* The service thread (runs with dispatching disabled, except while it waits). */
static void serviceThread(void)
{
	VkMigration *migration;
	uint32_t index, flags;
	VkCore *core;
	VkHeap *heap;

	vkDisableDispatch();
	core = vkCurrentCore(&index);
	heap = vkSystemHeap();
	for (;;) {
		flags = vkThreadWaitFlags(VK_SERVICE_OUTGOING | VK_SERVICE_INCOMING);
		if (flags & VK_SERVICE_OUTGOING) {
			vkCacheWritebackAll();
			mep_syncm();
			while ((migration = nextMove(index, core, 0)) != NULL)
				sendThread(index, core, migration);
		}
		if (flags & VK_SERVICE_INCOMING)
			while ((migration = nextMove(index, core, 1)) != NULL)
				receiveThread(index, core, heap, migration);
	}
}

void vkServiceStart(void)
{
	int32_t thread;

	thread = vkThreadCreateHere(serviceThread, vkCurrentCoreId(), 9, 0x200);
	if (thread == -1)
		return;
	vkThreadSetFlags((VkThread *)thread, VK_SERVICE_OUTGOING | VK_SERVICE_INCOMING);
	g_vkServiceThreads[vnzCoreIndex()] = (VkThread *)thread;
	vkThreadStart((VkThread *)thread, 0, 0);
}
