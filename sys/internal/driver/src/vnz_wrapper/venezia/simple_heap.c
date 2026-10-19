/* Sony venezia/simple_heap.c: fixed-block communication-memory heap. */
#include <sysmem/debug.h>
#include "../venezia_internal.h"

void _sceVeneziaSimpleHeapInit(void *base, SceSize size, VeneziaSimpleHeap *heap,
			     SceSize workSize, SceSize blockSize)
{
	unsigned int numBlocks;
#if FW_DEBUG
	int valid;
#endif

#if FW_DEBUG
	valid = blockSize;
	if (blockSize)
		valid = ((blockSize - 1) & blockSize) == 0;
	sceKernelAssert(valid,
		SCE_DBGINFO_FUNC(29, "_sceVeneziaSimpleHeapInit"), __builtin_return_address(0));
	numBlocks = size / blockSize;
	sceKernelAssert(numBlocks - 1 <= 127, SCE_DBGINFO_FUNC(31, "_sceVeneziaSimpleHeapInit"),
		__builtin_return_address(0));
	sceKernelAssert(workSize >= sizeof(VeneziaSimpleHeap),
		SCE_DBGINFO_FUNC(32, "_sceVeneziaSimpleHeapInit"), __builtin_return_address(0));
	sceKernelAssert(base != NULL &&
		((uintptr_t)base & (blockSize - 1)) == 0,
		SCE_DBGINFO_FUNC(33, "_sceVeneziaSimpleHeapInit"), __builtin_return_address(0));
	sceKernelAssert(heap != NULL, SCE_DBGINFO_FUNC(34, "_sceVeneziaSimpleHeapInit"),
		__builtin_return_address(0));
#endif
	heap->marker = (uintptr_t)heap - 1;
	heap->base = base;
	heap->size = size;
	heap->blockSize = blockSize;
	heap->lastFreeBlock = 0;
#if !FW_DEBUG
	numBlocks = size / blockSize;
#endif
	heap->numBlocks = numBlocks;
	heap->lock = 0;
	heap->blocks[0].next = 0;
	heap->blocks[0].size = size;
}

void *_sceVeneziaSimpleHeapAllocMemory(VeneziaSimpleHeap *heap, SceSize size)
{
	unsigned int previous, current, allocated, next;
	SceSize alignedSize, availableSize;
	void *memory;
	int intr;

#if FW_DEBUG
	sceKernelAssert(heap != NULL,
		SCE_DBGINFO_FUNC(55, "_sceVeneziaSimpleHeapAllocMemory"), __builtin_return_address(0));
	sceKernelAssert(heap->marker == (uintptr_t)heap - 1,
		SCE_DBGINFO_FUNC(56, "_sceVeneziaSimpleHeapAllocMemory"), __builtin_return_address(0));
	sceKernelAssert(size != 0, SCE_DBGINFO_FUNC(57, "_sceVeneziaSimpleHeapAllocMemory"),
		__builtin_return_address(0));
#endif
	intr = sceKernelSpinlockLowLockCpuSuspendIntr(&heap->lock);
	previous = heap->lastFreeBlock;
	alignedSize = (heap->blockSize - 1 + size) & -heap->blockSize;
	current = heap->blocks[previous].next;
#if FW_DEBUG
	sceKernelAssert(previous < heap->numBlocks,
		SCE_DBGINFO_FUNC(69, "_sceVeneziaSimpleHeapAllocMemory"), __builtin_return_address(0));
#endif
	for (;;) {
#if FW_DEBUG
		sceKernelAssert(current < heap->numBlocks,
			SCE_DBGINFO_FUNC(71, "_sceVeneziaSimpleHeapAllocMemory"), __builtin_return_address(0));
#endif
		availableSize = heap->blocks[current].size;
		if (alignedSize <= availableSize)
			break;
		previous = current;
		if (heap->lastFreeBlock == current) {
			memory = NULL;
			goto unlock;
		}
		current = heap->blocks[current].next;
	}
	allocated = (availableSize - alignedSize) / heap->blockSize + current;
	next = heap->blocks[current].next;
#if FW_DEBUG
	sceKernelAssert(allocated < heap->numBlocks,
		SCE_DBGINFO_FUNC(76, "_sceVeneziaSimpleHeapAllocMemory"), __builtin_return_address(0));
#endif
	heap->blocks[allocated].next = ~0U;
	heap->blocks[allocated].size = alignedSize;
	if (allocated > current) {
		heap->blocks[current].size -= alignedSize;
	} else {
#if FW_DEBUG
		sceKernelAssert(current == allocated,
			SCE_DBGINFO_FUNC(84, "_sceVeneziaSimpleHeapAllocMemory"), __builtin_return_address(0));
#endif
		heap->blocks[previous].next = next;
	}
	heap->lastFreeBlock = previous;
	memory = (void *)((uintptr_t)heap->base + heap->blockSize * allocated);
unlock:
	sceKernelSpinlockLowUnlockCpuResumeIntr(&heap->lock, intr);
	return memory;
}

static inline __attribute__((always_inline)) void mergeVeneziaSimpleHeapBlocks(
	VeneziaSimpleHeap *heap, unsigned int first, unsigned int second)
{
	SceSize size = heap->blocks[first].size;

	if (second == first + size / heap->blockSize) {
		heap->blocks[first].size = size + heap->blocks[second].size;
		heap->blocks[first].next = heap->blocks[second].next;
		heap->blocks[second].next = 0;
		heap->blocks[second].size = 0;
	}
}

int _sceVeneziaSimpleHeapFreeMemory(VeneziaSimpleHeap *heap, void *memory)
{
	uintptr_t address = (uintptr_t)memory;
	unsigned int allocated, current, next;
	int intr, ret;

#if FW_DEBUG
	sceKernelAssert(heap != NULL, SCE_DBGINFO_FUNC(135, "_sceVeneziaSimpleHeapFreeMemory"),
		__builtin_return_address(0));
	sceKernelAssert(heap->marker == (uintptr_t)heap - 1,
		SCE_DBGINFO_FUNC(136, "_sceVeneziaSimpleHeapFreeMemory"), __builtin_return_address(0));
	sceKernelAssert(address && !(address & (heap->blockSize - 1)),
		SCE_DBGINFO_FUNC(137, "_sceVeneziaSimpleHeapFreeMemory"), __builtin_return_address(0));
	sceKernelAssert(address >= (uintptr_t)heap->base &&
		address < (uintptr_t)heap->base + heap->blockSize * heap->numBlocks,
		SCE_DBGINFO_FUNC(138, "_sceVeneziaSimpleHeapFreeMemory"), __builtin_return_address(0));
#endif
	intr = sceKernelSpinlockLowLockCpuSuspendIntr(&heap->lock);
	allocated = (address - (uintptr_t)heap->base) / heap->blockSize;
#if FW_DEBUG
	sceKernelAssert(heap->blocks[allocated].size != 0,
		SCE_DBGINFO_FUNC(146, "_sceVeneziaSimpleHeapFreeMemory"), __builtin_return_address(0));
	sceKernelAssert(heap->blocks[allocated].next == ~0U,
		SCE_DBGINFO_FUNC(147, "_sceVeneziaSimpleHeapFreeMemory"), __builtin_return_address(0));
#endif
	current = heap->blocks[heap->lastFreeBlock].next;
#if FW_DEBUG
	sceKernelAssert(heap->lastFreeBlock < heap->numBlocks,
		SCE_DBGINFO_FUNC(152, "_sceVeneziaSimpleHeapFreeMemory"), __builtin_return_address(0));
	sceKernelAssert(current < heap->numBlocks,
		SCE_DBGINFO_FUNC(153, "_sceVeneziaSimpleHeapFreeMemory"), __builtin_return_address(0));
#endif
	for (;;) {
		next = heap->blocks[current].next;
#if FW_DEBUG
		sceKernelAssert(next < heap->numBlocks,
			SCE_DBGINFO_FUNC(156, "_sceVeneziaSimpleHeapFreeMemory"), __builtin_return_address(0));
#endif
		if (next == current) {
			if (allocated > current) {
				heap->blocks[allocated].next = current;
				heap->blocks[current].next = allocated;
				mergeVeneziaSimpleHeapBlocks(heap, current, allocated);
				heap->lastFreeBlock = next;
				ret = 0;
				break;
			}
		} else if (current > next) {
			if (allocated < next || allocated > current) {
				heap->blocks[allocated].next = next;
				heap->blocks[current].next = allocated;
				if (allocated < next) {
					mergeVeneziaSimpleHeapBlocks(heap, allocated, next);
					heap->lastFreeBlock = allocated;
				} else {
					mergeVeneziaSimpleHeapBlocks(heap, current, allocated);
					heap->lastFreeBlock = current;
				}
				ret = 0;
				break;
			}
		} else if (allocated > current && allocated < next) {
			heap->blocks[allocated].next = next;
			heap->blocks[current].next = allocated;
			mergeVeneziaSimpleHeapBlocks(heap, allocated, next);
			mergeVeneziaSimpleHeapBlocks(heap, current, allocated);
			heap->lastFreeBlock = current;
			ret = 0;
			break;
		}
		if (heap->lastFreeBlock == current) {
			ret = (int)0x80600000;
#if FW_DEBUG
			sceKernelAssert(0, SCE_DBGINFO_FUNC(201, "_sceVeneziaSimpleHeapFreeMemory"),
				__builtin_return_address(0));
#endif
			break;
		}
		current = next;
	}
	sceKernelSpinlockLowUnlockCpuResumeIntr(&heap->lock, intr);
	return ret;
}
