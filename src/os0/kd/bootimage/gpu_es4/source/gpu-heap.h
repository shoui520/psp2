/* Native GPU heap and allocation-span owners. */
#ifndef GPU_HEAP_H
#define GPU_HEAP_H

#include <stddef.h>

struct GpuMemoryContext;
struct GpuMemBlockHeap;

typedef struct GpuHeapRange {
	unsigned offset;
	unsigned size;
	struct GpuHeapRange *next;
} GpuHeapRange;

typedef struct GpuHeapSpan {
	unsigned base;
	unsigned reserved;
	unsigned primaryAddress;
	unsigned alternateAddress;
	unsigned size;
	unsigned allocationType;
	unsigned flags;
	unsigned pid;
	unsigned blockGroupId;
	struct GpuMemBlockHeap *blockHeap;
	GpuHeapRange *allocations;
	struct GpuHeapSpan *next;
} GpuHeapSpan;

typedef struct GpuHeap {
	unsigned base;
	unsigned attributes;
	unsigned size;
	unsigned lookupKey;
	unsigned shared;
	GpuHeapSpan *spans;
	struct GpuMemoryContext *memoryContext;
	struct GpuHeap *previous;
	struct GpuHeap *next;
} GpuHeap;

_Static_assert(sizeof(GpuHeapRange) == 12, "native range size");
_Static_assert(sizeof(GpuHeapSpan) == 0x30, "native span size");
_Static_assert(sizeof(GpuHeap) == 0x24, "native heap size");
_Static_assert(offsetof(GpuHeapSpan, allocations) == 0x28,
	       "native allocation list");
_Static_assert(offsetof(GpuHeap, spans) == 0x14, "native span list");

#endif
