/* GPU heap objects and ordered allocation ranges. */
#include <stdint.h>
#include "gpu-heap.h"
#include "gpu-blockheap.h"
#include <sce/debug.h>

#undef __FILE_NAME__
#define __FILE_NAME__ "../../../../driver/src/gpu/memory/heap.c"

extern void *AllocProcessGpuMemory(unsigned size);
extern int FreeProcessGpuMemory(void *address);
extern void *memset(void *destination, int value, unsigned size);

/* SceSysclibForDriver A46CB7DE returns quotient/remainder in R0/R1. */
extern uint64_t GpuUnsignedDivideWithRemainder(unsigned numerator,
					      unsigned denominator)
	__asm__("__aeabi_uidivmod");

GpuHeapRange *FindGpuHeapGap(unsigned base, unsigned offset,
			    unsigned available, unsigned size,
			    unsigned alignment, unsigned boundary)
{
	GpuHeapRange *range;
	unsigned remainder, padding;

	remainder = GpuUnsignedDivideWithRemainder(base + offset, alignment) >> 32;
	padding = remainder ? alignment - remainder : 0;
	if (boundary && ((base + offset + padding) & (boundary - 1)) + size > boundary)
		return 0;
	if (padding + size > available)
		return 0;
	range = AllocProcessGpuMemory(sizeof(*range));
	if (!range)
		return 0;
	memset(range, 0, sizeof(*range));
	range->offset = offset + padding;
	range->size = size;
	return range;
}

int AllocateGpuHeapRange(GpuHeapSpan *span, unsigned *offsetOut,
			 unsigned size, unsigned alignment, unsigned boundary)
{
	GpuHeapRange *previous = span->allocations;
	GpuHeapRange *range;
	unsigned offset, limit;

	if (!previous) {
		range = FindGpuHeapGap(span->base, 0, span->size, size,
				       alignment, boundary);
		if (!range)
			return (int)0x804c0001;
		span->allocations = range;
		*offsetOut = range->offset;
		return 0;
	}
	for (; previous; previous = previous->next) {
		offset = previous->offset + previous->size;
		limit = previous->next ? previous->next->offset : span->size;
		range = FindGpuHeapGap(span->base, offset, limit - offset,
				       size, alignment, boundary);
		if (!range)
			continue;
		range->next = previous->next;
		previous->next = range;
		*offsetOut = range->offset;
		return 0;
	}
	return (int)0x804c0001;
}

/* Native heap factory; attributes come from DEVICE_MEMORY_HEAP_INFO. */
int CreateGpuHeap(unsigned base, unsigned size, unsigned lookupKey,
		  unsigned attributes, GpuHeap **output)
{
	GpuHeap *heap;

	if (!output)
		return (int)0x804c0000;
	if (!base)
		return (int)0x804c0000;
	if (!size)
		return (int)0x804c0000;
	heap = AllocProcessGpuMemory(sizeof(*heap));
	if (!heap)
		return (int)0x804c0001;
	memset(heap, 0, sizeof(*heap));
	heap->base = base;
	heap->size = size;
	heap->lookupKey = lookupKey;
	heap->attributes = attributes;
	*output = heap;
	return 0;
}

int DestroyGpuHeap(GpuHeap *heap)
{
	GpuHeapSpan *span, *nextSpan;
	GpuHeapRange *range, *nextRange;

	if (!heap)
		return (int)0x804c0000;
	for (span = heap->spans; span; span = nextSpan) {
		for (range = span->allocations; range; range = nextRange) {
			nextRange = range->next;
			FreeProcessGpuMemory(range);
		}
		nextSpan = span->next;
		FreeProcessGpuMemory(span);
	}
	FreeProcessGpuMemory(heap);
	return 0;
}

GpuMemBlockHeap *GetProcessMemBlockHeap(void);
unsigned sceKernelGetProcessId(void);
int AddSpanToHeap(GpuHeap *heap, unsigned gpuAddress, unsigned address,
		unsigned size, unsigned allocationType, unsigned flags,
		unsigned blockGroupId, unsigned useAlternateAddress,
		GpuHeapSpan **output);

/* SCE heap allocation reuses compatible spans before acquiring backing memory. */
int sceGpuHeapAlloc(GpuHeap *heap, unsigned size, unsigned alignment,
		unsigned allocationType, unsigned flags, unsigned blockGroupId,
		GpuHeapAllocation *output, unsigned boundary)
{
	GpuHeapSpan *span, *next;
	GpuMemBlockHeap *blockHeap;
	unsigned pid, offset, spanSize, gpuAddress, boundaryMask;
	unsigned primaryAddress, alternateAddress, attributes;
	int ret;

	SCE_DBG_ASSERT((flags & 0x400000) || allocationType == 7, 339);
	if (!heap || !size || (size & 3) || !output || allocationType - 1 > 6)
		return (int)0x804c0000;
	if (size > heap->size)
		return (int)0x804c0001;
	if (size + alignment < size || size + alignment < alignment)
		return (int)0x804c0000;
	if (!alignment)
		alignment = 1;
	span = heap->spans;
	pid = flags & 0x200000 ? 0x10005 : sceKernelGetProcessId();
	for (; span; span = span->next) {
		if (span->allocationType != allocationType || span->flags != flags ||
		    span->pid != pid || span->blockGroupId != blockGroupId)
			continue;
		ret = AllocateGpuHeapRange(span, &offset, size, alignment, boundary);
		if (ret != 0)
			continue;
		output->primaryAddress = span->primaryAddress + offset;
		output->alternateAddress = span->alternateAddress + offset;
		output->heap = heap;
		output->releasedBackingAddress = 0;
		output->releasedSpanBase = 0;
		output->releasedSpanSize = 0;
		output->gpuAddress = span->base + offset;
		return 0;
	}
	spanSize = (size + 4095) & ~4095u;
	if (boundary && spanSize > boundary)
		return (int)0x804c0001;
	span = heap->spans;
	boundaryMask = boundary - 1;
	if (!span) {
		gpuAddress = (heap->base - 1 + alignment) & -alignment;
		if (boundary && (gpuAddress & boundaryMask) + spanSize > boundary)
			gpuAddress = (gpuAddress + boundaryMask) & ~boundaryMask;
		if (gpuAddress < heap->base ||
		    gpuAddress + spanSize > heap->base + heap->size)
			return (int)0x804c0001;
	} else {
		for (;;) {
			next = span->next;
			gpuAddress = (span->base + span->size - 1 + alignment) & -alignment;
			if (boundary && (gpuAddress & boundaryMask) + spanSize > boundary)
				gpuAddress = (gpuAddress + boundaryMask) & ~boundaryMask;
			if (gpuAddress < heap->base ||
			    gpuAddress + spanSize > heap->base + heap->size)
				return (int)0x804c0001;
			if (!next || gpuAddress + spanSize <= next->base)
				break;
			span = next;
		}
	}
	if (allocationType == 7) {
		ret = AddSpanToHeap(heap, gpuAddress, 0xaa000000, spanSize,
				allocationType, flags, blockGroupId, 0, &span);
		if (ret < 0)
			return ret;
	} else {
		blockHeap = blockGroupId == 1 ? GetKernelMemBlockHeap() :
				GetProcessMemBlockHeap();
		if (!blockHeap)
			return (int)0x804c0001;
		SCE_DBG_ASSERT(!(flags & 0x1000), 420);
		attributes = flags & 0x100000 ? 0x408000 : 0x208000;
		ret = sceGpuMemBlockHeapAlloc(blockHeap, blockGroupId, attributes,
				spanSize, &primaryAddress, &alternateAddress);
		if (ret != 0)
			return (int)0x804c0001;
		ret = AddSpanToHeap(heap, gpuAddress, primaryAddress, spanSize,
				allocationType, flags, blockGroupId, 0, &span);
		if (ret < 0) {
			sceGpuMemBlockHeapFree(blockHeap, primaryAddress, spanSize);
			return ret;
		}
		span->alternateAddress = alternateAddress;
		span->blockHeap = blockHeap;
	}
	ret = AllocateGpuHeapRange(span, &offset, size, alignment, boundary);
	if (ret < 0)
		return ret;
	output->primaryAddress = span->primaryAddress + offset;
	output->alternateAddress = span->alternateAddress + offset;
	output->heap = heap;
	output->gpuAddress = span->base + offset;
	output->releasedBackingAddress = span->alternateAddress ?
			span->alternateAddress : span->primaryAddress;
	output->releasedSpanBase = span->base;
	output->releasedSpanSize = spanSize;
	return 0;
}
