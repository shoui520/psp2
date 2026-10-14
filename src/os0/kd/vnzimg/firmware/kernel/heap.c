/*
 * Memory: pools and heaps.
 *
 * A pool is a free list of blocks in address order. A free block's header is
 * at its end: the next block and the block's size. Blocks are 0x40 or 0x100
 * aligned. The kernel has two pools under semaphores (pool 0, from the
 * VK_MEMORY_POOL1 region; pool 1, the system heap's).
 *
 * A heap is a pool behind an interface (create, delete, allocate, free); the
 * heaps are kept in a tree by address so that memory is freed to its own heap.
 * A thread allocates from its heap (vkCurrentHeap). An allocation is returned
 * in the window the caller asks for.
 */
#include "kernel.h"

#define VK_POOLS		2
#define VK_HEAP_POOL		1	/* the system heap's */

/* a free block's header, at its end */
typedef struct VkFreeBlock {
	struct VkFreeBlock *next;
	uint32_t size;
} VkFreeBlock;

#define blockStart(block)	((uint32_t)(block) + sizeof(VkFreeBlock) - (block)->size)
#define blockEnd(block)		((uint32_t)(block) + sizeof(VkFreeBlock))

VkPool g_vkPools[VK_POOLS] VK_SHARED;
VkHeap g_vkSystemHeap VK_SHARED;
VkHeap *g_vkHeapTree VK_SHARED;

static volatile uint32_t *const heapSemaphore = vkSemaphore(VK_SEMAPHORE_HEAPS);

/* the pool's lock: nothing (the callers lock) */
static void poolLock(void)
{
}

static void poolUnlock(void)
{
}

/* Insert the free range [start, start + size), merging it with its neighbours. */
static void poolInsert(VkPool *pool, uint32_t start, uint32_t size)
{
	VkFreeBlock **link = &pool->head, **previousLink = NULL;
	VkFreeBlock *previous = NULL, *block = pool->head, *inserted;
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
	inserted = (VkFreeBlock *)(end - sizeof(VkFreeBlock));
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
int32_t vkPoolInitialize(VkPool *pool, uint32_t start, uint32_t end)
{
	uint32_t last = end - 1;
	uint32_t first;

	if (vkToPublic(start) == 0 || vkToPublic(last) == 0)
		return -1;
	first = vkToPublic((start + 0x3f) & ~0x3f);
	last = vkToPublic((end & ~0x3f) - 1);
	if (last < first)
		return -1;
	poolLock();
	pool->head = NULL;
	poolInsert(pool, first, last - first + 1);
	poolUnlock();
	return 0;
}

/* Take size bytes, 0x40 (mode 0) or 0x100 (mode 1) aligned. Returns the block, or 0. */
uint32_t vkPoolTake(VkPool *pool, uint32_t size, uint32_t mode)
{
	VkFreeBlock **link, *block, *gap;
	uint32_t start, result = 0, needed;

	if (size == 0)
		return 0;
	if (mode == 0)
		size = (size + 0x3f) & ~0x3f;
	else if (mode == 1)
		size = (size + 0xff) & ~0xff;
	else
		return 0;

	poolLock();
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
		poolUnlock();
		return 0;
	}

	if (start < result) {
		/* the bytes before the aligned block stay free, as a block of their own */
		gap = (VkFreeBlock *)(result - sizeof(VkFreeBlock));
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
	poolUnlock();
	return result;
}

/* Give back a block vkPoolTake gave. */
void vkPoolGive(VkPool *pool, uint32_t address, uint32_t size, uint32_t mode)
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
	poolLock();
	poolInsert(pool, start, end - start);
	poolUnlock();
}

/* the kernel's pools, under their semaphores */
static int32_t kernelPoolInitialize(uint32_t pool, uint32_t start, uint32_t end)
{
	return vkPoolInitialize(&g_vkPools[pool], start, end);
}

uint32_t vkKernelPoolTake(uint32_t pool, uint32_t size, uint32_t mode)
{
	volatile uint32_t *semaphore = vkSemaphore(VK_SEMAPHORE_POOLS + pool);
	uint32_t block;

	vkDisableDispatch();
	vkTakeSemaphore(semaphore);
	block = vkPoolTake(&g_vkPools[pool], size, mode);
	vkGiveSemaphore(semaphore);
	vkEnableDispatch();
	return block;
}

void vkKernelPoolGive(uint32_t pool, uint32_t address, uint32_t size, uint32_t mode)
{
	volatile uint32_t *semaphore = vkSemaphore(VK_SEMAPHORE_POOLS + pool);

	vkDisableDispatch();
	vkTakeSemaphore(semaphore);
	vkPoolGive(&g_vkPools[pool], address, size, mode);
	vkGiveSemaphore(semaphore);
	vkEnableDispatch();
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
	case VK_WINDOW_RAW:
	case VK_WINDOW_PHYSICAL:
		return physical;
	case VK_WINDOW_PUBLIC:
		return vkPhysicalToPublic(physical);
	case VK_WINDOW_UNCACHED:
		if (size <= VK_L2_LINE_LIMIT)
			vkL2LineOperation(physical, size, VK_L2_INVALIDATE);
		else
			vkL2Operation(physical, size, VK_L2_INVALIDATE);
		return vkPhysicalToUncached(physical);
	}
	return 0;
}

/* And back. */
static uint32_t fromWindow(uint32_t address, uint32_t size, int32_t window)
{
	switch (window) {
	case VK_WINDOW_RAW:
		return address;
	case VK_WINDOW_PHYSICAL:
		vkCachePurge(address, size);
		return address;
	case VK_WINDOW_PUBLIC:
		return vkPublicToPhysical(address);
	case VK_WINDOW_UNCACHED:
		return vkUncachedToPhysical(address);
	}
	return (uint32_t)-1;
}

/*
 * The system heap: kernel pool 1.
 */
static int32_t systemHeapCreate(void *context, uint32_t base, uint32_t size)
{
	(void)context;
	vkGiveSemaphore(vkSemaphore(VK_SEMAPHORE_POOLS + VK_HEAP_POOL));
	g_vkPools[VK_HEAP_POOL].head = NULL;
	g_vkPools[VK_HEAP_POOL].reserved = 0;
	kernelPoolInitialize(VK_HEAP_POOL, base, base + size);
	fromWindow(base, size, VK_WINDOW_RAW);
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
	block = vkKernelPoolTake(VK_HEAP_POOL, size, mode);
	if (block == 0)
		return NULL;
	address = toWindow(vkPublicToPhysical(block), size, window);
	if (address == 0)
		vkKernelPoolGive(VK_HEAP_POOL, block, size, mode);
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
	if (window == VK_WINDOW_RAW)
		physical = (uint32_t)memory;
	vkKernelPoolGive(VK_HEAP_POOL, vkPhysicalToPublic(physical), size, mode);
}

/*
 * A heap of its own (vkHeapCreatePool): a pool under a lock page.
 */
static int32_t poolHeapCreate(void *context, uint32_t base, uint32_t size)
{
	VkPoolHeap *heap = context;

	heap->lock = vkLockPageAllocate();
	if (heap->lock == NULL)
		return -1;
	heap->pool.head = NULL;
	if (vkPoolInitialize(&heap->pool, base, base + size) != 0)
		return -1;
	fromWindow(base, size, VK_WINDOW_RAW);
	return 0;
}

static void poolHeapDelete(void *context, uint32_t base, uint32_t size)
{
	VkPoolHeap *heap = context;

	toWindow(base, size, VK_WINDOW_RAW);
	vkLockPageFree(heap->lock);
}

static void *poolHeapAllocate(void *context, uint32_t size, uint32_t alignment, int32_t window)
{
	VkPoolHeap *heap = context;
	uint32_t mode, block, address;

	if (alignmentMode(alignment, &size, &mode) == -1)
		return NULL;
	vkDisableDispatch();
	vkSpinLock(heap->lock);
	block = vkPoolTake(&heap->pool, size, mode);
	if (block == 0) {
		vkSpinUnlock(heap->lock);
		vkEnableDispatch();
		return NULL;
	}
	address = toWindow(vkPublicToPhysical(block), size, window);
	if (address == 0)
		vkPoolGive(&heap->pool, block, size, mode);
	vkSpinUnlock(heap->lock);
	vkEnableDispatch();
	return (void *)address;
}

static void poolHeapFree(void *context, void *memory, uint32_t size, uint32_t alignment,
	int32_t window)
{
	VkPoolHeap *heap = context;
	uint32_t mode, physical;

	if (alignmentMode(alignment, &size, &mode) == -1)
		return;
	vkDisableDispatch();
	vkSpinLock(heap->lock);
	physical = fromWindow((uint32_t)memory, size, window);
	if (window == VK_WINDOW_RAW)
		physical = (uint32_t)memory;
	vkPoolGive(&heap->pool, vkPhysicalToPublic(physical), size, mode);
	vkSpinUnlock(heap->lock);
	vkEnableDispatch();
}

/*
 * The heap tree, by address.
 */

/* the heap holding [address, address + size) */
VkHeap *vkHeapFind(uint32_t address, uint32_t size)
{
	uint32_t end = address + size;
	VkHeap *heap;

	if (size == 0)
		return NULL;
	vkDisableDispatch();
	vkTakeSemaphore(heapSemaphore);
	heap = g_vkHeapTree;
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
	vkGiveSemaphore(heapSemaphore);
	vkEnableDispatch();
	return heap;
}

/* Add a heap for [base, base + size); returns it, or NULL when it overlaps one. */
static VkHeap *heapInsert(uint32_t base, uint32_t size, VkHeap *heap)
{
	VkHeap **link = &g_vkHeapTree, *at;
	VkHeap *result = heap;

	vkDisableDispatch();
	vkTakeSemaphore(heapSemaphore);
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
	vkGiveSemaphore(heapSemaphore);
	vkEnableDispatch();
	return result;
}

/* Take a heap out of the tree. Returns 0, or -1 when it is not there. */
static int32_t heapRemove(uint32_t base, uint32_t size, VkHeap *heap)
{
	VkHeap **link = &g_vkHeapTree, *at, *rightmost;
	int32_t result = -1;

	vkDisableDispatch();
	vkTakeSemaphore(heapSemaphore);
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
	vkGiveSemaphore(heapSemaphore);
	vkEnableDispatch();
	return result;
}

/* a heap's header (0x40 bytes) and its context (0x100), from the system heap */
static VkHeap *heapHeaderAllocate(void)
{
	VkHeap *system = vkSystemHeap();
	VkHeap *heap;
	void *context;

	heap = vkHeapAllocate(system, 0x40, 0x40);
	if (heap == NULL)
		return NULL;
	context = vkHeapAllocate(system, 0x100, 0x100);
	if (context == NULL) {
		vkHeapFree(system, heap, 0x40, 0x40);
		return NULL;
	}
	heap = (VkHeap *)vkPhysicalToPublic((uint32_t)heap);
	heap->context = (void *)vkPhysicalToPublic((uint32_t)context);
	return heap;
}

static void heapHeaderFree(VkHeap *heap)
{
	VkHeap *system = vkSystemHeap();

	vkHeapFree(system, (void *)vkPublicToPhysical((uint32_t)heap->context), 0x100, 0x100);
	vkHeapFree(system, (void *)vkPublicToPhysical((uint32_t)heap), 0x40, 0x40);
}

/* Create a heap on [base, base + size) with its own functions. Returns it, or NULL. */
VkHeap *vkHeapCreate(uint32_t base, uint32_t size, VkHeapCreate create, VkHeapDelete delete,
	VkHeapAllocate allocate, VkHeapFree free)
{
	VkHeap *heap = heapHeaderAllocate();

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

void vkHeapDelete(VkHeap *heap)
{
	heap->delete(heap->context, heap->base, heap->size);
	heapRemove(heap->base, heap->size, heap);
	heapHeaderFree(heap);
}

/* A heap on [base, base + size) of the kernel's own kind. */
VkHeap *vkHeapCreatePool(uint32_t base, uint32_t size)
{
	return vkHeapCreate(base, size, poolHeapCreate, poolHeapDelete, poolHeapAllocate,
		poolHeapFree);
}

void *vkHeapAllocateStack(VkHeap *heap, uint32_t size)
{
	return heap->allocate(heap->context, size, 0x40, VK_WINDOW_PHYSICAL);
}

void *vkHeapAllocatePublic(VkHeap *heap, uint32_t size)
{
	return heap->allocate(heap->context, size, 0x40, VK_WINDOW_PUBLIC);
}

void *vkHeapAllocateUncached(VkHeap *heap, uint32_t size)
{
	return heap->allocate(heap->context, size, 0x100, VK_WINDOW_UNCACHED);
}

void *vkHeapAllocate(VkHeap *heap, uint32_t size, uint32_t alignment)
{
	return heap->allocate(heap->context, size, alignment, VK_WINDOW_RAW);
}

/* the heap that holds memory: heap itself, or the one the tree finds */
static VkHeap *heapOwner(VkHeap *heap, uint32_t address, uint32_t size)
{
	if (address >= heap->base && heap->base + heap->size >= address + size)
		return heap;
	return vkHeapFind(address, size);
}

void vkHeapFreeStack(VkHeap *heap, void *memory, uint32_t size)
{
	VkHeap *owner = heapOwner(heap, (uint32_t)memory, size);

	if (owner)
		owner->free(owner->context, memory, size, 0x40, VK_WINDOW_PHYSICAL);
}

void vkHeapFreePublic(VkHeap *heap, void *memory, uint32_t size)
{
	VkHeap *owner = heapOwner(heap, vkPublicToPhysical((uint32_t)memory), size);

	if (owner)
		owner->free(owner->context, memory, size, 0x40, VK_WINDOW_PUBLIC);
}

void vkHeapFreeUncached(VkHeap *heap, void *memory, uint32_t size)
{
	VkHeap *owner = heapOwner(heap, vkUncachedToPhysical((uint32_t)memory), size);

	if (owner)
		owner->free(owner->context, memory, size, 0x100, VK_WINDOW_UNCACHED);
}

void vkHeapFree(VkHeap *heap, void *memory, uint32_t size, uint32_t alignment)
{
	VkHeap *owner = heapOwner(heap, (uint32_t)memory, size);

	if (owner)
		owner->free(owner->context, memory, size, alignment, VK_WINDOW_RAW);
}

VkHeap *vkSystemHeap(void)
{
	return &g_vkSystemHeap;
}

/* the current thread's heap */
VkHeap *vkCurrentHeap(void)
{
	VkHeap *heap;

	vkDisableDispatch();
	heap = g_vkCores[vnzCoreIndex()].current->heap;
	vkEnableDispatch();
	return heap;
}

VkHeap *vkThreadHeap(VkThread *thread)
{
	return thread->heap;
}

/* The pools and the system heap (the primary core, at boot). */
void vkHeapInitialize(uint32_t primary)
{
	uint32_t base, size;

	if (!primary)
		return;
	g_vkHeapTree = NULL;
	vkGiveSemaphore(heapSemaphore);

	base = vkMemoryRegion(VK_MEMORY_POOL1, &size);
	vkGiveSemaphore(vkSemaphore(VK_SEMAPHORE_POOLS));
	g_vkPools[0].reserved = 0;
	g_vkPools[0].head = NULL;
	kernelPoolInitialize(0, base, base + size);

	base = vkMemoryRegion(VK_MEMORY_POOL0, &size);
	g_vkSystemHeap.allocate = systemHeapAllocate;
	g_vkSystemHeap.free = systemHeapFree;
	g_vkSystemHeap.delete = systemHeapDelete;
	g_vkSystemHeap.create = systemHeapCreate;
	heapInsert(base, size, &g_vkSystemHeap);
	systemHeapCreate(NULL, base, size);
}

/*
 * The memory map.
 */

/* the index of id in the map's sorted regions, or -1 */
static int32_t regionIndex(const VkMemoryRegion *regions, uint32_t count, uint32_t id)
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
uint32_t vkMemoryRegion(uint32_t id, uint32_t *size)
{
	const VkMemoryMap *map = VK_CONFIGURATION->memory;
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
