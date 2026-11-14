/*
 * Memory: pools and heaps.
 *
 * A pool is a free list of blocks in address order. A free block's header is
 * at its end: the next block and the block's size. Blocks are 0x40 or 0x100
 * aligned. The kernel has two pools under semaphores (pool 0, from the
 * VNZ_KERNEL_MEMORY_POOL1 region; pool 1, the system heap's).
 *
 * A heap is a pool behind an interface (create, delete, allocate, free); the
 * heaps are kept in a tree by address so that memory is freed to its own heap.
 * A thread allocates from its heap (vnzKernelCurrentHeap). An allocation is returned
 * in the window the caller asks for.
 */
#include "api.h"

#define VNZ_KERNEL_POOLS		2
#define VNZ_KERNEL_HEAP_POOL		1	/* the system heap's */

/* a free block's header, at its end */
typedef struct VnzKernelFreeBlock {
	struct VnzKernelFreeBlock *next;
	uint32_t size;
} VnzKernelFreeBlock;

#define blockStart(block)	((uint32_t)(block) + sizeof(VnzKernelFreeBlock) - (block)->size)
#define blockEnd(block)		((uint32_t)(block) + sizeof(VnzKernelFreeBlock))

VnzKernelPool g_vnzKernelPools[VNZ_KERNEL_POOLS] VNZ_KERNEL_SHARED;
VnzKernelHeap g_vnzKernelSystemHeap VNZ_KERNEL_SHARED;
VnzKernelHeap *g_vnzKernelHeapTree VNZ_KERNEL_SHARED;

static volatile uint32_t *const heapSemaphore = vnzKernelSemaphore(VNZ_KERNEL_SEMAPHORE_HEAPS);

/* Insert the free range [start, start + size), merging it with its neighbours. */
static void poolInsert(VnzKernelPool *pool, uint32_t start, uint32_t size)
{
	VnzKernelFreeBlock **link = &pool->head, **previousLink = NULL;
	VnzKernelFreeBlock *previous = NULL, *block = pool->head, *inserted;
	uint32_t end = start + size;

	while (block && blockStart(block) < start) {
		previousLink = link;
		previous = block;
		link = &block->next;
		block = block->next;
	}

	if (previous && blockEnd(previous) == start) {
		/* grows the previous block: its header moves to the new end */
		start = blockStart(previous);
		link = previousLink;
	}
	inserted = (VnzKernelFreeBlock *)(end - sizeof(VnzKernelFreeBlock));
	inserted->size = end - start;
	inserted->next = block;
	*link = inserted;

	if (block && blockStart(block) == end) {
		/* and the next block grows down over it */
		block->size += inserted->size;
		*link = block;
	}
}

/* Set a pool up on the range [start, end); the range's addresses become public ones. */
int32_t vnzKernelPoolInitialize(VnzKernelPool *pool, uint32_t start, uint32_t end)
{
	uint32_t last = end - 1;
	uint32_t first;

	if (vnzKernelToPublic(start) == 0 || vnzKernelToPublic(last) == 0)
		return -1;
	first = vnzKernelToPublic((start + 0x3f) & ~0x3f);
	last = vnzKernelToPublic((end & ~0x3f) - 1);
	if (last < first)
		return -1;
	vnzMemoryResume();
	pool->head = NULL;
	poolInsert(pool, first, last - first + 1);
	vnzMemorySuspend();
	return 0;
}

/* Take size bytes, 0x40 (mode 0) or 0x100 (mode 1) aligned. Returns the block, or 0. */
uint32_t vnzKernelPoolTake(VnzKernelPool *pool, uint32_t size, uint32_t mode)
{
	VnzKernelFreeBlock **link, *block, *gap;
	uint32_t start, result = 0, needed;

	if (size == 0)
		return 0;
	if (mode == 0)
		size = (size + 0x3f) & ~0x3f;
	else if (mode == 1)
		size = (size + 0xff) & ~0xff;
	else
		return 0;

	vnzMemoryResume();
	for (link = &pool->head; (block = *link) != NULL; link = &block->next) {
		start = blockStart(block);
		if (mode == 0) {
			if (block->size >= size) {
				result = start;
				break;
			}
		} else {
			result = (start + 0xff) & ~0xff;
			needed = result + size - start;
			if (block->size >= needed)
				break;
		}
	}
	if (block == NULL) {
		vnzMemorySuspend();
		return 0;
	}

	if (start < result) {
		/* the bytes before the aligned block stay free, as a block of their own */
		gap = (VnzKernelFreeBlock *)(result - sizeof(VnzKernelFreeBlock));
		gap->size = result - start;
		gap->next = block;
		*link = gap;
		block->size -= gap->size;
		link = &gap->next;
	}
	if (size < block->size)
		block->size -= size;
	else
		*link = block->next;
	vnzMemorySuspend();
	return result;
}

/* Give back a block vnzKernelPoolTake gave. */
void vnzKernelPoolGive(VnzKernelPool *pool, uint32_t address, uint32_t size, uint32_t mode)
{
	uint32_t shift, start, end;

	if (size == 0)
		return;
	if (mode == 1)
		shift = 8;
	else if (mode == 0)
		shift = 6;
	else
		return;
	start = (address >> shift) << shift;
	end = ((address + size + (1 << shift) - 1) >> shift) << shift;
	vnzMemoryResume();
	poolInsert(pool, start, end - start);
	vnzMemorySuspend();
}

/* the kernel's pools, under their semaphores */
static int32_t kernelPoolInitialize(uint32_t pool, uint32_t start, uint32_t end)
{
	return vnzKernelPoolInitialize(&g_vnzKernelPools[pool], start, end);
}

uint32_t vnzKernelKernelPoolTake(uint32_t pool, uint32_t size, uint32_t mode)
{
	volatile uint32_t *semaphore = vnzKernelSemaphore(VNZ_KERNEL_SEMAPHORE_POOLS + pool);
	uint32_t block;

	vnzKernelDisableDispatch();
	vnzKernelTakeSemaphore(semaphore);
	block = vnzKernelPoolTake(&g_vnzKernelPools[pool], size, mode);
	vnzKernelGiveSemaphore(semaphore);
	vnzKernelEnableDispatch();
	return block;
}

void vnzKernelKernelPoolGive(uint32_t pool, uint32_t address, uint32_t size, uint32_t mode)
{
	volatile uint32_t *semaphore = vnzKernelSemaphore(VNZ_KERNEL_SEMAPHORE_POOLS + pool);

	vnzKernelDisableDispatch();
	vnzKernelTakeSemaphore(semaphore);
	vnzKernelPoolGive(&g_vnzKernelPools[pool], address, size, mode);
	vnzKernelGiveSemaphore(semaphore);
	vnzKernelEnableDispatch();
}

/* The pool mode of an alignment, and the size rounded to it. Returns -1 for others. */
static int32_t alignmentMode(uint32_t alignment, uint32_t *size, uint32_t *mode)
{
	if (alignment == 0x100) {
		*mode = 1;
		*size = (*size + 0xff) & ~0xff;
		return 0;
	}
	if (alignment == 0x40) {
		*mode = 0;
		*size = (*size + 0x3f) & ~0x3f;
		return 0;
	}
	return -1;
}

/* A block's address in the window asked for (physical as the pool gives it). */
static uint32_t toWindow(uint32_t physical, uint32_t size, int32_t window)
{
	switch (window) {
	case VNZ_KERNEL_WINDOW_RAW:
		return vnzMemoryLeave(physical, size);
	case VNZ_KERNEL_WINDOW_PHYSICAL:
		return vnzMemoryPhysicalToStack(physical, size);
	case VNZ_KERNEL_WINDOW_PUBLIC:
		return vnzMemoryPhysicalToPublic(physical, size);
	case VNZ_KERNEL_WINDOW_UNCACHED:
		if (size <= VNZ_KERNEL_L2_LINE_LIMIT)
			vnzKernelL2LineOperation(physical, size, VNZ_KERNEL_L2_INVALIDATE);
		else
			vnzKernelL2Operation(physical, size, VNZ_KERNEL_L2_INVALIDATE);
		return vnzMemoryPhysicalToUncached(physical, size);
	}
	return 0;
}

/* And back. */
static uint32_t fromWindow(uint32_t address, uint32_t size, int32_t window)
{
	switch (window) {
	case VNZ_KERNEL_WINDOW_RAW:
		return vnzMemoryEnter(address, size);
	case VNZ_KERNEL_WINDOW_PHYSICAL:
		vnzKernelCachePurge(address, size);
		return vnzMemoryStackToPhysical(address, size);
	case VNZ_KERNEL_WINDOW_PUBLIC:
		return vnzMemoryPublicToPhysical(address, size);
	case VNZ_KERNEL_WINDOW_UNCACHED:
		return vnzMemoryUncachedToPhysical(address, size);
	}
	return (uint32_t)-1;
}

/*
 * The system heap: kernel pool 1.
 */
static int32_t systemHeapCreate(void *context, uint32_t base, uint32_t size)
{
	(void)context;
	vnzKernelGiveSemaphore(vnzKernelSemaphore(VNZ_KERNEL_SEMAPHORE_POOLS + VNZ_KERNEL_HEAP_POOL));
	g_vnzKernelPools[VNZ_KERNEL_HEAP_POOL].head = NULL;
	g_vnzKernelPools[VNZ_KERNEL_HEAP_POOL].reserved = 0;
	kernelPoolInitialize(VNZ_KERNEL_HEAP_POOL, base, base + size);
	fromWindow(base, size, VNZ_KERNEL_WINDOW_RAW);
	return 0;
}

static void systemHeapDelete(void *context, uint32_t base, uint32_t size)
{
	(void)context;
	(void)base;
	(void)size;
}

static void *systemHeapAllocate(void *context, uint32_t size, uint32_t alignment, int32_t window)
{
	uint32_t mode, block, address;

	(void)context;
	if (alignmentMode(alignment, &size, &mode) == -1)
		return NULL;
	block = vnzKernelKernelPoolTake(VNZ_KERNEL_HEAP_POOL, size, mode);
	if (block == 0)
		return NULL;
	address = toWindow(vnzKernelPublicToPhysical(block), size, window);
	if (address == 0)
		vnzKernelKernelPoolGive(VNZ_KERNEL_HEAP_POOL, block, size, mode);
	return (void *)address;
}

static void systemHeapFree(void *context, void *memory, uint32_t size, uint32_t alignment,
	int32_t window)
{
	uint32_t mode, physical;

	(void)context;
	if (alignmentMode(alignment, &size, &mode) == -1)
		return;
	physical = fromWindow((uint32_t)memory, size, window);
	if (window == VNZ_KERNEL_WINDOW_RAW)
		physical = (uint32_t)memory;
	vnzKernelKernelPoolGive(VNZ_KERNEL_HEAP_POOL, vnzKernelPhysicalToPublic(physical), size,
		mode);
}

/*
 * A heap of its own (vnzKernelHeapCreatePool): a pool under a lock page.
 */
int32_t vnzKernelPoolHeapCreate(void *context, uint32_t base, uint32_t size)
{
	VnzKernelPoolHeap *heap = context;

	heap->lock = vnzKernelLockPageAllocate();
	if (heap->lock == NULL)
		return -1;
	heap->pool.head = NULL;
	if (vnzKernelPoolInitialize(&heap->pool, base, base + size) != 0)
		return -1;
	fromWindow(base, size, VNZ_KERNEL_WINDOW_RAW);
	return 0;
}

void vnzKernelPoolHeapDelete(void *context, uint32_t base, uint32_t size)
{
	VnzKernelPoolHeap *heap = context;

	toWindow(base, size, VNZ_KERNEL_WINDOW_RAW);
	vnzKernelLockPageFree(heap->lock);
}

void *vnzKernelPoolHeapAllocate(void *context, uint32_t size, uint32_t alignment, int32_t window)
{
	VnzKernelPoolHeap *heap = context;
	uint32_t mode, block, address;

	if (alignmentMode(alignment, &size, &mode) == -1)
		return NULL;
	vnzKernelDisableDispatch();
	vnzKernelSpinLock(heap->lock);
	block = vnzKernelPoolTake(&heap->pool, size, mode);
	if (block == 0) {
		vnzKernelSpinUnlock(heap->lock);
		vnzKernelEnableDispatch();
		return NULL;
	}
	address = toWindow(vnzKernelPublicToPhysical(block), size, window);
	if (address == 0)
		vnzKernelPoolGive(&heap->pool, block, size, mode);
	vnzKernelSpinUnlock(heap->lock);
	vnzKernelEnableDispatch();
	return (void *)address;
}

void vnzKernelPoolHeapFree(void *context, void *memory, uint32_t size, uint32_t alignment,
	int32_t window)
{
	VnzKernelPoolHeap *heap = context;
	uint32_t mode, physical;

	if (alignmentMode(alignment, &size, &mode) == -1)
		return;
	vnzKernelDisableDispatch();
	vnzKernelSpinLock(heap->lock);
	physical = fromWindow((uint32_t)memory, size, window);
	if (window == VNZ_KERNEL_WINDOW_RAW)
		physical = (uint32_t)memory;
	vnzKernelPoolGive(&heap->pool, vnzKernelPhysicalToPublic(physical), size, mode);
	vnzKernelSpinUnlock(heap->lock);
	vnzKernelEnableDispatch();
}

/*
 * The heap tree, by address.
 */

/* the heap holding [address, address + size) */
VnzKernelHeap *vnzKernelHeapFind(uint32_t address, uint32_t size)
{
	uint32_t end = address + size;
	VnzKernelHeap *heap;

	if (size == 0)
		return NULL;
	vnzKernelDisableDispatch();
	vnzKernelTakeSemaphore(heapSemaphore);
	heap = g_vnzKernelHeapTree;
	while (heap) {
		if (address >= heap->base && heap->base + heap->size >= end)
			break;
		if (address < heap->base)
			heap = heap->left;
		else if (heap->base < address)
			heap = heap->right;
		else
			heap = NULL;
	}
	vnzKernelGiveSemaphore(heapSemaphore);
	vnzKernelEnableDispatch();
	return heap;
}

/* Add a heap for [base, base + size); returns it, or NULL when it overlaps one. */
static VnzKernelHeap *heapInsert(uint32_t base, uint32_t size, VnzKernelHeap *heap)
{
	VnzKernelHeap **link = &g_vnzKernelHeapTree, *at;
	VnzKernelHeap *result = heap;

	vnzKernelDisableDispatch();
	vnzKernelTakeSemaphore(heapSemaphore);
	while ((at = *link) != NULL) {
		if (base < at->base) {
			if (at->base < base + size) {
				result = NULL;
				goto out;
			}
			link = &at->left;
		} else {
			if (base < at->base + at->size || base == at->base) {
				result = NULL;
				goto out;
			}
			link = &at->right;
		}
	}
	heap->size = size;
	heap->base = base;
	heap->right = NULL;
	heap->left = NULL;
	*link = heap;
out:
	vnzKernelGiveSemaphore(heapSemaphore);
	vnzKernelEnableDispatch();
	return result;
}

/* Take a heap out of the tree. Returns 0, or -1 when it is not there. */
static int32_t heapRemove(uint32_t base, uint32_t size, VnzKernelHeap *heap)
{
	VnzKernelHeap **link = &g_vnzKernelHeapTree, *at, *rightmost;
	int32_t result = -1;

	vnzKernelDisableDispatch();
	vnzKernelTakeSemaphore(heapSemaphore);
	while ((at = *link) != NULL && at != heap) {
		if (base < at->base)
			link = &at->left;
		else if (at->base < base)
			link = &at->right;
		else
			break;
	}
	if (at == heap && at && at->size == size) {
		if (at->left == NULL) {
			*link = at->right;
		} else if (at->right == NULL) {
			*link = at->left;
		} else {
			rightmost = at->left;
			while (rightmost->right)
				rightmost = rightmost->right;
			rightmost->right = at->right;
			*link = at->left;
		}
		result = 0;
	}
	vnzKernelGiveSemaphore(heapSemaphore);
	vnzKernelEnableDispatch();
	return result;
}

/* a heap's header (0x40 bytes) and its context (0x100), from the system heap */
static VnzKernelHeap *heapHeaderAllocate(void)
{
	VnzKernelHeap *system = vnzKernelSystemHeap();
	VnzKernelHeap *heap;
	void *context;

	heap = vnzKernelHeapAllocate(system, 0x40, 0x40);
	if (heap == NULL)
		return NULL;
	context = vnzKernelHeapAllocate(system, 0x100, 0x100);
	if (context == NULL) {
		vnzKernelHeapFree(system, heap, 0x40, 0x40);
		return NULL;
	}
	heap = (VnzKernelHeap *)vnzKernelPhysicalToPublic((uint32_t)heap);
	heap->context = (void *)vnzKernelPhysicalToPublic((uint32_t)context);
	return heap;
}

static void heapHeaderFree(VnzKernelHeap *heap)
{
	VnzKernelHeap *system = vnzKernelSystemHeap();

	vnzKernelHeapFree(system, (void *)vnzKernelPublicToPhysical((uint32_t)heap->context), 0x100,
		0x100);
	vnzKernelHeapFree(system, (void *)vnzKernelPublicToPhysical((uint32_t)heap), 0x40, 0x40);
}

/* Create a heap on [base, base + size) with its own functions. Returns it, or NULL. */
VnzKernelHeap *vnzKernelHeapCreate(uint32_t base, uint32_t size, VnzKernelHeapCreate create,
	VnzKernelHeapDelete delete,
	VnzKernelHeapAllocate allocate, VnzKernelHeapFree free)
{
	VnzKernelHeap *heap = heapHeaderAllocate();

	if (heap == NULL)
		return NULL;
	heap->create = create;
	heap->delete = delete;
	heap->allocate = allocate;
	heap->base = base;
	heap->size = size;
	heap->free = free;
	if (heapInsert(base, size, heap) == NULL)
		goto fail;
	if (heap->create(heap->context, base, size) != 0) {
		heapRemove(base, size, heap);
		goto fail;
	}
	return heap;
fail:
	heapHeaderFree(heap);
	return NULL;
}

void vnzKernelHeapDelete(VnzKernelHeap *heap)
{
	heap->delete(heap->context, heap->base, heap->size);
	heapRemove(heap->base, heap->size, heap);
	heapHeaderFree(heap);
}

void *vnzKernelHeapContext(VnzKernelHeap *heap)
{
	return heap->context;
}

/* A heap on [base, base + size) of the kernel's own kind. */
VnzKernelHeap *vnzKernelHeapCreatePool(uint32_t base, uint32_t size)
{
	return vnzKernelHeapCreate(base, size, vnzKernelPoolHeapCreate, vnzKernelPoolHeapDelete,
		vnzKernelPoolHeapAllocate,
		vnzKernelPoolHeapFree);
}

void vnzKernelHeapDeletePool(VnzKernelHeap *heap)
{
	vnzKernelHeapDelete(heap);
}

void *vnzKernelHeapAllocateStack(VnzKernelHeap *heap, uint32_t size)
{
	return heap->allocate(heap->context, size, 0x40, VNZ_KERNEL_WINDOW_PHYSICAL);
}

void *vnzKernelHeapAllocatePublic(VnzKernelHeap *heap, uint32_t size)
{
	return heap->allocate(heap->context, size, 0x40, VNZ_KERNEL_WINDOW_PUBLIC);
}

void *vnzKernelHeapAllocateUncached(VnzKernelHeap *heap, uint32_t size)
{
	return heap->allocate(heap->context, size, 0x100, VNZ_KERNEL_WINDOW_UNCACHED);
}

void *vnzKernelHeapAllocate(VnzKernelHeap *heap, uint32_t size, uint32_t alignment)
{
	return heap->allocate(heap->context, size, alignment, VNZ_KERNEL_WINDOW_RAW);
}

/* the heap that holds memory: heap itself, or the one the tree finds */
static VnzKernelHeap *heapOwner(VnzKernelHeap *heap, uint32_t address, uint32_t size)
{
	if (address >= heap->base && heap->base + heap->size >= address + size)
		return heap;
	return vnzKernelHeapFind(address, size);
}

void vnzKernelHeapFreeStack(VnzKernelHeap *heap, void *memory, uint32_t size)
{
	VnzKernelHeap *owner = heapOwner(heap, (uint32_t)memory, size);

	if (owner)
		owner->free(owner->context, memory, size, 0x40, VNZ_KERNEL_WINDOW_PHYSICAL);
}

void vnzKernelHeapFreePublic(VnzKernelHeap *heap, void *memory, uint32_t size)
{
	VnzKernelHeap *owner = heapOwner(heap, vnzKernelPublicToPhysical((uint32_t)memory), size);

	if (owner)
		owner->free(owner->context, memory, size, 0x40, VNZ_KERNEL_WINDOW_PUBLIC);
}

void vnzKernelHeapFreeUncached(VnzKernelHeap *heap, void *memory, uint32_t size)
{
	VnzKernelHeap *owner = heapOwner(heap, vnzKernelUncachedToPhysical((uint32_t)memory), size);

	if (owner)
		owner->free(owner->context, memory, size, 0x100, VNZ_KERNEL_WINDOW_UNCACHED);
}

void vnzKernelHeapFree(VnzKernelHeap *heap, void *memory, uint32_t size, uint32_t alignment)
{
	VnzKernelHeap *owner = heapOwner(heap, (uint32_t)memory, size);

	if (owner)
		owner->free(owner->context, memory, size, alignment, VNZ_KERNEL_WINDOW_RAW);
}

VnzKernelHeap *vnzKernelSystemHeap(void)
{
	return &g_vnzKernelSystemHeap;
}

/* the current thread's heap */
VnzKernelHeap *vnzKernelCurrentHeap(void)
{
	VnzKernelHeap *heap;

	vnzKernelDisableDispatch();
	heap = g_vnzKernelCores[vnzCoreIndex()].current->heap;
	vnzKernelEnableDispatch();
	return heap;
}

VnzKernelHeap *vnzKernelThreadHeap(VnzKernelThread *thread)
{
	return thread->heap;
}

/* The pools and the system heap (the primary core, at boot). */
void vnzKernelHeapInitialize(uint32_t primary)
{
	uint32_t base, size;

	if (!primary)
		return;
	g_vnzKernelHeapTree = NULL;
	vnzKernelGiveSemaphore(heapSemaphore);

	base = vnzKernelMemoryRegion(VNZ_KERNEL_MEMORY_POOL1, &size);
	vnzKernelGiveSemaphore(vnzKernelSemaphore(VNZ_KERNEL_SEMAPHORE_POOLS));
	g_vnzKernelPools[0].reserved = 0;
	g_vnzKernelPools[0].head = NULL;
	kernelPoolInitialize(0, base, base + size);

	base = vnzKernelMemoryRegion(VNZ_KERNEL_MEMORY_POOL0, &size);
	g_vnzKernelSystemHeap.allocate = systemHeapAllocate;
	g_vnzKernelSystemHeap.free = systemHeapFree;
	g_vnzKernelSystemHeap.delete = systemHeapDelete;
	g_vnzKernelSystemHeap.create = systemHeapCreate;
	heapInsert(base, size, &g_vnzKernelSystemHeap);
	systemHeapCreate(NULL, base, size);
}

/*
 * The memory map.
 */

/* the index of id in the map's sorted regions, or -1 */
static int32_t regionIndex(const VnzKernelMemoryRegion *regions, uint32_t count, uint32_t id)
{
	int32_t low = 0, high = count - 1, middle;

	if (count == 0)
		return -1;
	while (low <= high) {
		middle = low + ((high - low) >> 1);
		if (id < regions[middle].id)
			high = middle - 1;
		else if (regions[middle].id < id)
			low = middle + 1;
		else
			return middle;
	}
	return -1;
}

/* A memory region's base, and its size; 0 when the map has no such region. */
uint32_t vnzKernelMemoryRegion(uint32_t id, uint32_t *size)
{
	const VnzKernelMemoryMap *map = VNZ_KERNEL_CONFIGURATION->memory;
	int32_t index = regionIndex(map->regions, map->count, id);

	if (index == -1) {
		if (size)
			*size = 0;
		return 0;
	}
	if (size)
		*size = map->regions[index].size;
	return map->regions[index].base;
}
