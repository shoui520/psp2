#ifndef LIBKERNEL_HEAP_STATE_H
#define LIBKERNEL_HEAP_STATE_H

#include <stddef.h>
#include <psp2common/kernel/threadmgr.h>

struct MspaceSegment {
	void *base;
	SceSize size;
	struct MspaceSegment *next;
	SceUInt32 flags;
};

/*
 * dlmalloc 2.8.3's struct malloc_state as SCE built it: upstream's fields
 * (contrib/dlmalloc-2.8.3/malloc.c), with the in-use counters SCE added.
 */
#define MSPACE_SMALL_BINS	32
#define MSPACE_TREE_BINS	32

/* flags: dlmalloc's USE_LOCK_BIT, and SCE's "the segments are its memblocks" */
#define MSPACE_USE_LOCK		2
#define MSPACE_OWN_SEGMENTS	8

/* MspaceSegment.flags: dlmalloc's IS_MMAPPED_BIT, a memblock of its own */
#define MSPACE_SEGMENT_MMAPPED	1

struct MspaceStateView {
	SceUInt32 smallmap;
	SceUInt32 treemap;
	SceSize dvsize;
	SceSize topsize;
	char *leastAddr;
	void *dv;
	void *top;
	SceSize trimCheck;
	SceSize magic;
	void *smallbins[(MSPACE_SMALL_BINS + 1) * 2];
	void *treebins[MSPACE_TREE_BINS];
	SceSize footprint;
	SceSize maxFootprint;
	SceSize currentInUse;
	SceSize peakInUse;
	SceUInt32 flags;
	SceKernelLwMutexWork mutex;
	struct MspaceSegment segment;
} __attribute__((may_alias));

struct MspaceStats {
	SceSize capacity;
	SceSize footprint;
	SceSize peakInUse;
	SceSize currentInUse;
};

_Static_assert(offsetof(struct MspaceStateView, top) == 0x18, "heap top offset");
_Static_assert(offsetof(struct MspaceStateView, footprint) == 0x1ac,
	       "heap footprint offset");
_Static_assert(offsetof(struct MspaceStateView, mutex) == 0x1c0,
	       "heap mutex offset");
_Static_assert(offsetof(struct MspaceStateView, segment) == 0x1e0,
	       "heap segment offset");

extern unsigned int heapInitialized;
extern struct MspaceStateView *primaryHeap;

#endif
