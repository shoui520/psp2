/* Native backing-range allocation and adjacent-free-range coalescing. */
#include "gpu-blockheap.h"
#include "sce/debug.h"

#undef __FILE_NAME__
#define __FILE_NAME__ "../../../../driver/src/gpu/memory/blockmgr.c"

extern void *AllocProcessGpuMemory(unsigned size);
extern int FreeProcessGpuMemory(void *address);
extern void *memset(void *destination, int value, unsigned size);

int sceGpuMemBlockHeapAlloc(GpuMemBlockHeap *heap, unsigned groupId,
			   unsigned attributes, unsigned size,
			   unsigned *primaryOut, unsigned *alternateOut)
{
	volatile GpuMemBlockHeap *blockHeap = heap;
	volatile GpuMemBlock *block;
	volatile GpuMemBlockRange *range;
	GpuMemBlockRange *tail, *next;
	unsigned available, primary, alternate, remaining;

	SCE_DBG_ASSERT(!(size & 0xfff), 336);
	if (!heap)
		return (int)0x804c0000;
	if (!primaryOut)
		return (int)0x804c0000;
	if (!alternateOut)
		return (int)0x804c0000;
	if (!groupId)
		return (int)0x804c0000;
	for (block = blockHeap->blocks; block; block = block->next) {
		if (block->attributes != attributes ||
		    block->groupId != groupId)
			continue;
		range = block->ranges;
		SCE_DBG_ASSERT(range != 0, 348);
		for (; range; range = range->next) {
			if (range->allocated)
				continue;
			available = range->size;
			if (size > available)
				continue;
			SCE_DBG_ASSERT(1, 366);
#if FW_DEBUG
			/* The native debug build reloads this after its assertion. */
			available = range->size;
#endif
			if (available != size) {
				primary = range->primaryAddress + size;
				alternate = range->alternateAddress + size;
				remaining = available - size;
				tail = AllocProcessGpuMemory(sizeof(*tail));
				if (!tail)
					return (int)0x804c0001;
				memset(tail, 0, sizeof(*tail));
				tail->primaryAddress = primary;
				tail->alternateAddress = alternate;
				tail->size = remaining;
				next = range->next;
				range->size = size;
				tail->next = next;
				range->next = tail;
			}
			range->allocated = 1;
			*primaryOut = range->primaryAddress;
			*alternateOut = range->alternateAddress;
			block->allocatedRangeCount++;
			return 0;
		}
	}
	return (int)0x804c0001;
}

int sceGpuMemBlockHeapFree(GpuMemBlockHeap *heap, unsigned address,
			  unsigned size)
{
	volatile GpuMemBlockHeap *blockHeap = heap;
	volatile GpuMemBlock *block;
	volatile GpuMemBlockRange *range, *previous;
	GpuMemBlockRange *next;

	if (!heap || !address)
		return (int)0x804c0000;
	for (block = blockHeap->blocks; block; block = block->next) {
		range = block->ranges;
		previous = 0;
		SCE_DBG_ASSERT(range != 0, 405);
		for (; range; previous = range, range = range->next) {
			if (range->allocated != 1 || range->primaryAddress != address ||
			    range->size != size)
				continue;
			SCE_DBG_ASSERT(1, 425);
			range->allocated = 0;
			block->allocatedRangeCount--;
			next = range->next;
			if (next && !next->allocated) {
				range->size += next->size;
				range->next = next->next;
				FreeProcessGpuMemory(next);
			}
			if (previous && !previous->allocated) {
				previous->size += range->size;
				previous->next = range->next;
				FreeProcessGpuMemory((void *)range);
			}
			return 0;
		}
	}
	return (int)0x804c0005;
}
