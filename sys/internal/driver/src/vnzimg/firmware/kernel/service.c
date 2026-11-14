/*
 * Moving a thread to another core.
 *
 * Each core runs a service thread (priority 9). A move is queued to the
 * thread's core, whose service thread takes the thread off the core and stands
 * in for its interrupt handlers (vnzKernelInterruptsHook), then queues the move to the
 * target core, whose service thread puts the thread on that core and moves the
 * handlers there (vnzKernelInterruptsMove). The move semaphore keeps the moves in order.
 */
#include "kernel.h"

#define VNZ_KERNEL_SERVICE_OUTGOING	0x2	/* a move off this core is queued */
#define VNZ_KERNEL_SERVICE_INCOMING	0x1	/* a move to this core is queued */

/* outgoing, incoming */
VnzKernelMigrationQueue g_vnzKernelServiceQueues[VNZ_CORES][2] VNZ_KERNEL_SHARED;
VnzKernelThread *g_vnzKernelServiceThreads[VNZ_CORES] VNZ_KERNEL_SHARED;

static volatile uint32_t *const moveSemaphore = vnzKernelSemaphore(VNZ_KERNEL_SEMAPHORE_MOVES);

static void migrationAppend(VnzKernelMigrationQueue *queue, VnzKernelMigration *migration)
{
	if (queue->head)
		queue->tail->next = migration;
	else
		queue->head = migration;
	queue->tail = migration;
}

static VnzKernelMigration *migrationTake(VnzKernelMigrationQueue *queue)
{
	VnzKernelMigration *migration = queue->head;

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
int32_t vnzKernelThreadMigrate(VnzKernelThread *thread, uint32_t coreId, VnzKernelThread *notify,
	uint32_t notifyFlags)
{
	VnzKernelHeap *heap = vnzKernelSystemHeap();
	VnzKernelMigration *migration;
	VnzKernelCore *core;
	uint32_t index, from, to;

	migration = vnzKernelHeapAllocate(heap, sizeof(*migration), 0x40);
	if (migration == NULL)
		return -1;
	migration = (VnzKernelMigration *)vnzKernelPhysicalToPublic((uint32_t)migration);
	migration->lock = vnzKernelLockPageAllocate();
	if (migration->lock) {
		vnzKernelDisableDispatch();
		core = vnzKernelCurrentCore(&index);
		core->inKernel = 1;
		vnzKernelLockThread(thread);
		if (thread->attributes != VNZ_KERNEL_THREAD_MOVING) {
			from = thread->core;
			to = coreId - VNZ_FIRST_CORE_ID;
			if (vnzKernelInterruptsReserve(to, thread->interrupts) == 0) {
				thread->attributes = VNZ_KERNEL_THREAD_MOVING;
				migration->next = NULL;
				migration->thread = thread;
				migration->from = thread->core;
				migration->to = to;
				migration->notify = notify;
				migration->notifyFlags = notifyFlags;
				migrationAppend(&g_vnzKernelServiceQueues[from][0], migration);
				vnzKernelUnlockThread(thread);
				vnzKernelProcessEvents(core);
				if (g_vnzKernelServiceThreads[from])
					vnzKernelThreadSetFlags(g_vnzKernelServiceThreads[from],
						VNZ_KERNEL_SERVICE_OUTGOING);
				vnzKernelEnableDispatch();
				return 0;
			}
		}
		vnzKernelUnlockThread(thread);
		vnzKernelProcessEvents(core);
		vnzKernelEnableDispatch();
		vnzKernelLockPageFree(migration->lock);
	}
	vnzKernelHeapFree(heap, (void *)vnzKernelPublicToPhysical((uint32_t)migration),
		sizeof(*migration), 0x40);
	return -1;
}

/* Take a moving thread off this core and pass it on. */
static void sendThread(uint32_t index, VnzKernelCore *core, VnzKernelMigration *migration)
{
	VnzKernelThread *thread = migration->thread;
	uint32_t from = thread->core;
	uint32_t to;

	vnzKernelInterruptsHook(migration);
	thread->core = VNZ_KERNEL_NO_CORE;
	if ((thread->state & VNZ_KERNEL_STATE_MASK) == VNZ_KERNEL_STATE_READY)
		vnzKernelQueueRemove(&g_vnzKernelRunQueues[from][thread->priority - 1], thread);
	g_vnzKernelThreadCounts[index]--;
	vnzKernelGiveSemaphore(vnzKernelSemaphore(VNZ_KERNEL_SEMAPHORE_THREADS + index));

	to = migration->to;
	vnzKernelTakeSemaphore(vnzKernelSemaphore(VNZ_KERNEL_SEMAPHORE_THREADS + to));
	migrationAppend(&g_vnzKernelServiceQueues[to][1], migration);
	vnzKernelGiveSemaphore(vnzKernelSemaphore(VNZ_KERNEL_SEMAPHORE_THREADS + to));
	vnzKernelGiveSemaphore(moveSemaphore);
	vnzKernelProcessEvents(core);
	if (g_vnzKernelServiceThreads[to])
		vnzKernelThreadSetFlags(g_vnzKernelServiceThreads[to], VNZ_KERNEL_SERVICE_INCOMING);
}

/* Put a thread that moved here on this core. */
static void receiveThread(uint32_t index, VnzKernelCore *core, VnzKernelHeap *heap,
	VnzKernelMigration *migration)
{
	VnzKernelThread *thread = migration->thread;
	uint32_t ended = 1;

	thread->core = index;
	if ((thread->state & VNZ_KERNEL_STATE_MASK) == VNZ_KERNEL_STATE_READY)
		vnzKernelQueueAppend(&g_vnzKernelRunQueues[index][thread->priority - 1], thread);
	if (thread->state & VNZ_KERNEL_STATE_MASK) {
		g_vnzKernelThreadCounts[index]++;
		ended = 0;
	}
	thread->attributes = 0;
	vnzKernelInterruptsMove(migration);
	vnzKernelGiveSemaphore(vnzKernelSemaphore(VNZ_KERNEL_SEMAPHORE_THREADS + index));
	vnzKernelGiveSemaphore(moveSemaphore);
	vnzKernelProcessEvents(core);

	if (migration->notifyFlags)
		vnzKernelThreadSetFlags(migration->notify, migration->notifyFlags);
	if (ended)		/* it was deleted while it moved */
		vnzKernelThreadRelease(thread);
	vnzKernelLockPageFree(migration->lock);
	vnzKernelHeapFree(heap, (void *)vnzKernelPublicToPhysical((uint32_t)migration),
		sizeof(*migration), 0x40);
}

/* Take the next move from one of this core's queues, holding the semaphores if there is one. */
static VnzKernelMigration *nextMove(uint32_t index, VnzKernelCore *core, uint32_t incoming)
{
	VnzKernelMigration *migration;

	core->inKernel = 1;
	vnzKernelTakeSemaphore(moveSemaphore);
	vnzKernelTakeSemaphore(vnzKernelSemaphore(VNZ_KERNEL_SEMAPHORE_THREADS + index));
	migration = migrationTake(&g_vnzKernelServiceQueues[index][incoming]);
	if (migration == NULL) {
		vnzKernelGiveSemaphore(vnzKernelSemaphore(VNZ_KERNEL_SEMAPHORE_THREADS + index));
		vnzKernelGiveSemaphore(moveSemaphore);
		vnzKernelProcessEvents(core);
	}
	return migration;
}

/* The service thread (runs with dispatching disabled, except while it waits). */
static void serviceThread(void)
{
	VnzKernelMigration *migration;
	uint32_t index, flags;
	VnzKernelCore *core;
	VnzKernelHeap *heap;

	vnzKernelDisableDispatch();
	core = vnzKernelCurrentCore(&index);
	heap = vnzKernelSystemHeap();
	for (;;) {
		flags = vnzKernelThreadWaitFlags(VNZ_KERNEL_SERVICE_OUTGOING | VNZ_KERNEL_SERVICE_INCOMING);
		if (flags & VNZ_KERNEL_SERVICE_OUTGOING) {
			vnzKernelCacheWritebackAll();
			mep_syncm();
			while ((migration = nextMove(index, core, 0)) != NULL)
				sendThread(index, core, migration);
		}
		if (flags & VNZ_KERNEL_SERVICE_INCOMING)
			while ((migration = nextMove(index, core, 1)) != NULL)
				receiveThread(index, core, heap, migration);
	}
}

void vnzKernelServiceStart(void)
{
	int32_t thread;

	thread = vnzKernelThreadCreateHere(serviceThread, vnzKernelCurrentCoreId(), 9, 0x200);
	if (thread == -1)
		return;
	vnzKernelThreadSetFlags((VnzKernelThread *)thread,
		VNZ_KERNEL_SERVICE_OUTGOING | VNZ_KERNEL_SERVICE_INCOMING);
	g_vnzKernelServiceThreads[vnzCoreIndex()] = (VnzKernelThread *)thread;
	vnzKernelThreadStart((VnzKernelThread *)thread, 0, 0);
}
