/* Native GPU heap span insertion, address lookup and range release. */
#include "gpu-blockheap.h"
#include "sce/debug.h"

#undef __FILE_NAME__
#define __FILE_NAME__ "../../../../driver/src/gpu/memory/heap.c"

extern void *AllocProcessGpuMemory(unsigned size);
extern int FreeProcessGpuMemory(void *address);
extern void *memset(void *destination, int value, unsigned size);
extern unsigned sceKernelGetProcessId(void);

int AddSpanToHeap(GpuHeap *heap, unsigned gpuAddress, unsigned address,
		  unsigned size, unsigned allocationType, unsigned flags,
		  unsigned blockGroupId, unsigned useAlternateAddress,
		  GpuHeapSpan **output)
{
	volatile GpuHeap *gpuHeap = heap;
	volatile GpuHeapSpan *span, *current, *previous;
	GpuHeapSpan *next;
	unsigned pid;

	if (!heap)
		return (int)0x804c0000;
	span = AllocProcessGpuMemory(sizeof(*span));
	if (!span)
		return (int)0x804c0001;
	memset((void *)span, 0, sizeof(*span));
	pid = blockGroupId == 1 ? 0x10005 : sceKernelGetProcessId();
	span->pid = pid;
	span->blockGroupId = blockGroupId;
	span->base = gpuAddress;
	if (useAlternateAddress) {
		span->primaryAddress = 0;
		span->alternateAddress = address;
	} else {
		span->primaryAddress = address;
		span->alternateAddress = 0;
	}
	span->size = size;
	span->allocationType = allocationType;
	span->flags = flags;
	current = gpuHeap->spans;
	previous = 0;
	if (!current) {
		gpuHeap->spans = (GpuHeapSpan *)span;
	} else {
		for (;;) {
			SCE_DBG_ASSERT(span->base != current->base, 36);
			if (span->base < current->base) {
				span->next = (GpuHeapSpan *)current;
				if (previous) {
					previous->next = (GpuHeapSpan *)span;
				} else {
					SCE_DBG_ASSERT(gpuHeap->spans == current, 49);
					gpuHeap->spans = (GpuHeapSpan *)span;
				}
				break;
			}
			next = current->next;
			if (!next) {
				SCE_DBG_ASSERT(1, 44);
				current->next = (GpuHeapSpan *)span;
				break;
			}
			previous = current;
			current = next;
		}
	}
	if (output)
		*output = (GpuHeapSpan *)span;
	return 0;
}

/* Native private name unavailable; this searches unsigned half-open spans. */
int FindGpuHeapSpanForAddress(GpuHeap *heap, unsigned address,
			     GpuHeapSpan **output, GpuHeapSpan **previousOut)
{
	volatile GpuHeapSpan *span;
	GpuHeapSpan *previous = 0;
	unsigned base;

	if (!heap)
		return (int)0x804c0000;
	if (!address)
		return (int)0x804c0000;
	if (!output)
		return (int)0x804c0000;
	if (!previousOut)
		return (int)0x804c0000;
	for (span = heap->spans; span; span = span->next) {
		base = span->base;
		if (address >= base && address < base + span->size) {
			*output = (GpuHeapSpan *)span;
			*previousOut = previous;
			return 0;
		}
		previous = (GpuHeapSpan *)span;
	}
	return (int)0x804c0000;
}

int sceGpuHeapFreeByGpuVA(GpuHeap *heap, unsigned address,
			GpuHeapAllocation *output)
{
	GpuHeapSpan *found, *previousSpan;
	volatile GpuHeapSpan *span;
	volatile GpuHeapRange *range, *previous;
	volatile GpuHeapAllocation *allocation = output;
	unsigned offset, backingAddress;
	int ret;

	ret = FindGpuHeapSpanForAddress(heap, address, &found, &previousSpan);
	if (ret < 0)
		return ret;
	if (!heap || !found || !address)
		return (int)0x804c0000;
	span = found;
	offset = address - span->base;
	previous = 0;
	for (range = span->allocations; range; range = range->next) {
		if (range->offset != offset) {
			previous = range;
			continue;
		}
		if (previous)
			previous->next = range->next;
		else
			span->allocations = range->next;
		FreeProcessGpuMemory((void *)range);
		if (span->allocations) {
			allocation->releasedBackingAddress = 0;
			allocation->releasedSpanBase = 0;
			allocation->releasedSpanSize = 0;
			return 0;
		}
		backingAddress = span->alternateAddress;
		if (!backingAddress)
			backingAddress = span->primaryAddress;
		allocation->releasedBackingAddress = backingAddress;
		allocation->releasedSpanBase = span->base;
		allocation->releasedSpanSize = span->size;
		if (span->allocationType != 7) {
			ret = sceGpuMemBlockHeapFree(span->blockHeap,
						   span->primaryAddress, span->size);
			if (ret < 0) {
				SCE_DBG_PRINTF(0, 530,
					"Failed to free with error %08x for %p %08x\n",
					ret, (void *)span->primaryAddress, span->size);
			}
		}
		if (previousSpan)
			previousSpan->next = span->next;
		else
			heap->spans = span->next;
		FreeProcessGpuMemory((void *)span);
		return 0;
	}
	return (int)0x804c0005;
}
