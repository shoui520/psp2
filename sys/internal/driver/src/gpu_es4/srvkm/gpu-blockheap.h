/* Native backing-memory ranges and GPU allocation release descriptors. */
#ifndef GPU_BLOCKHEAP_H
#define GPU_BLOCKHEAP_H

#include <gpu/gpu-heap.h>

typedef struct GpuMemBlockRange {
	unsigned primaryAddress;
	unsigned alternateAddress;
	unsigned size;
	unsigned allocated;
	struct GpuMemBlockRange *next;
} GpuMemBlockRange;

typedef struct GpuMemBlock {
	unsigned inputUid;
	int referencedUid;
	int mappingUid;
	unsigned restoreMapping;
	unsigned groupId;
	unsigned attributes;
	unsigned allocatedRangeCount;
	unsigned primaryBase;
	unsigned size;
	GpuMemBlockRange *ranges;
	struct GpuMemBlock *next;
} GpuMemBlock;

typedef struct GpuMemBlockHeap {
	GpuMemBlock *blocks;
} GpuMemBlockHeap;

/* Both address domains are still unnamed; the native callers choose between
 * them and return the selected backing address when an entire span is released.
 */
typedef struct GpuHeapAllocation {
	unsigned gpuAddress;
	unsigned primaryAddress;
	unsigned alternateAddress;
	GpuHeap *heap;
	unsigned releasedBackingAddress;
	unsigned releasedSpanBase;
	unsigned releasedSpanSize;
} GpuHeapAllocation;

_Static_assert(sizeof(GpuMemBlockRange) == 0x14, "native backing range");
_Static_assert(sizeof(GpuMemBlock) == 0x2c, "native backing block");
_Static_assert(offsetof(GpuMemBlock, ranges) == 0x24, "native range head");
_Static_assert(sizeof(GpuMemBlockHeap) == 4, "native backing heap");
_Static_assert(sizeof(GpuHeapAllocation) == 0x1c, "native allocation result");

int sceGpuMemBlockHeapAlloc(GpuMemBlockHeap *heap, unsigned groupId,
			   unsigned attributes, unsigned size,
			   unsigned *primaryOut, unsigned *alternateOut);
int sceGpuMemBlockHeapFree(GpuMemBlockHeap *heap, unsigned address,
			  unsigned size);
GpuMemBlockHeap *GetKernelMemBlockHeap(void);
GpuMemBlock *sceGpuMemBlockNewBlock(int uid, unsigned *groupId,
				 unsigned restoreMapping);
int sceGpuMemBlockAddBlock(GpuMemBlockHeap *heap, int uid,
			  unsigned *groupId, unsigned restoreMapping);
int sceGpuMemBlockDelBlock(GpuMemBlockHeap *heap, int uid, int *referencedOut);
int DestroyMemBlockHeap(GpuMemBlockHeap *heap);

#endif
