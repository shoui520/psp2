#include "heap_state.h"

unsigned int heapInitialized;
struct MspaceStateView *primaryHeap;

extern int sceKernelLockLwMutex(SceKernelLwMutexWork *work, int count,
		SceUInt *timeout);
extern int sceKernelUnlockLwMutex(SceKernelLwMutexWork *work, int count);
extern void abort(void) __attribute__((noreturn));

/* SCE: inspect accounting maintained by the allocator, without traversing bins. */
int sceClibMspaceMallocStatsFast(void *mspace, struct MspaceStats *stats)
{
	struct MspaceStateView *space = mspace;
	struct MspaceStats value = { 0 };
	unsigned int locked = space->flags & MSPACE_USE_LOCK;

	if (locked) {
		if (sceKernelLockLwMutex(&space->mutex, 1, NULL) < 0)
			abort();
		locked = space->flags & MSPACE_USE_LOCK;
	}
	if (space->top || heapInitialized) {
		value.capacity = space->maxFootprint;
		value.footprint = space->footprint;
		value.peakInUse = space->peakInUse;
		value.currentInUse = space->currentInUse;
	}
	if (locked && sceKernelUnlockLwMutex(&space->mutex, 1) < 0)
		abort();
	if (stats)
		*stats = value;
	return 0;
}

/* the bytes an empty heap reports in use: the primary heap 0 or 0x40, another its malloc_state */
#define PRIMARY_HEAP_EMPTY_IN_USE	0x40
#define MSPACE_EMPTY_IN_USE		0x280

SceBool sceClibMspaceIsHeapEmpty(void *mspace)
{
	struct MspaceStateView *space = mspace;
	SceSize current = 0;
	unsigned int locked = space->flags & MSPACE_USE_LOCK;

	if (locked) {
		if (sceKernelLockLwMutex(&space->mutex, 1, NULL) < 0)
			abort();
		locked = space->flags & MSPACE_USE_LOCK;
	}
	if (space->top || heapInitialized)
		current = space->currentInUse;
	if (locked && sceKernelUnlockLwMutex(&space->mutex, 1) < 0)
		abort();
	if (space == primaryHeap)
		return (current & ~PRIMARY_HEAP_EMPTY_IN_USE) == 0;
	return current == MSPACE_EMPTY_IN_USE;
}
