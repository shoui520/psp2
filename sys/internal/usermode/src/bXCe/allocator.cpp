/*
 * ScebXCe's heap: the library's malloc and free. Requests are served by blocks of their own from
 * the allocator below; the heap's small pool, a first-fit list of blocks with boundary tags, is
 * used when such a block cannot be had, after the out-of-memory callback when it asks to retry.
 */
#include "bxce_internal.h"

SceUInt8 g_bxceHeapCreated;
BxceHeap *g_bxceHeap;

/* the pool: a sentinel, one free block of the rest, and an end tag */
__attribute__((noinline)) int bxceHeapInitializePool(BxceHeap *heap, void *memory, SceUInt32 size)
{
	BxceHeapBlock *sentinel = (BxceHeapBlock *)memory;
	BxceHeapBlock *block = sentinel + 1;
	BxceHeapBlock *end;

	if (size == 0)
		size = BXCE_HEAP_DEFAULT_SIZE;
	if (size < heap->headerSize + heap->overhead)
		return SCE_BXCE_ERROR_NO_MEMORY;

	heap->freeList = sentinel;
	sentinel->prevSize = 0;
	sentinel->size = 0;
	sentinel->next = sentinel;
	sentinel->prev = sentinel;
	block->prevSize = 0;
	block->next = sentinel;
	block->prev = sentinel;

	size = ((size - sizeof(BxceHeapBlock)) & ~3) - heap->headerSize;
	heap->freeList->prev = block;
	block->prev->next = block;
	block->size = size;
	end = (BxceHeapBlock *)((char *)block + size);
	end->prevSize = size;
	end->size = BXCE_HEAP_OWN_BLOCK;
	heap->initialized = 1;
	return 0;
}

/* makes the heap, once */
__attribute__((noinline)) int bxceHeapCreate(SceSize size, BxceHeapOutOfMemory outOfMemory)
{
	BxceHeap *heap;
	int ret;

	if (g_bxceHeapCreated)
		return SCE_BXCE_INFO_GENERIC_MESSAGE;

	heap = (BxceHeap *)bxceDefaultAllocate(sizeof(BxceHeap));
	if (heap == NULL) {
		bxceMspaceDestroy();
		return SCE_BXCE_ERROR_NO_MEMORY;
	}

	g_bxceHeap = heap;
	heap->memory = heap;
	heap->overhead = sizeof(BxceHeapBlock);
	heap->headerSize = sizeof(BxceHeapBlock);
	heap->outOfMemory = outOfMemory;
	ret = bxceHeapInitializePool(heap, heap->pool, BXCE_HEAP_POOL_SIZE);
	if (ret == 0)
		g_bxceHeapCreated = 1;
	else
		bxceDefaultDeallocate(heap);
	return ret;
}

int ScebXCe_EF4AF9F9(SceSize size, BxceHeapOutOfMemory outOfMemory)
{
	g_bxceHeapByApplication = 1;
	return bxceHeapCreate(size, outOfMemory);
}

/* deletes the heap the library made, once nothing is initialized */
void bxceHeapDelete(void)
{
	if (g_bxceHeap == NULL)
		return;
	if (g_bxceHeapByApplication)
		return;
	if (g_bxceInitializeCount > 0)
		return;
	if (!g_bxceHeapCreated)
		return;

	bxceDefaultDeallocate(g_bxceHeap->memory);
	bxceMspaceDestroy();
	g_bxceHeapCreated = 0;
}

/* deletes the heap the application made, once nothing is initialized */
void ScebXCe_2ECC8276(void)
{
	if (!g_bxceHeapByApplication)
		return;
	g_bxceHeapByApplication = 0;

	if (g_bxceHeap == NULL)
		return;
	if (g_bxceInitializeCount > 0)
		return;
	if (!g_bxceHeapCreated)
		return;

	bxceDefaultDeallocate(g_bxceHeap->memory);
	bxceMspaceDestroy();
	g_bxceHeapCreated = 0;
}

/* the first free block that fits: its tail when the rest can stay a block, else all of it */
void *bxceHeapAllocFromPool(BxceHeap *heap, SceSize size)
{
	BxceHeapBlock *sentinel = heap->freeList;
	BxceHeapBlock *block, *taken;
	int need, rest, length;

	need = (size + heap->overhead + 3) & ~3;
	for (block = sentinel->next; block != sentinel; block = block->next) {
		length = block->size;
		if (length < need)
			continue;

		rest = length - need;
		if ((SceUInt32)rest >= heap->headerSize + 4) {
			taken = (BxceHeapBlock *)((char *)block + rest);
			block->size = rest;
			*(int *)((char *)block + rest) = rest;
			*(int *)((char *)taken + need) = 0;
			taken->size = -need;
			return (char *)taken + heap->headerSize;
		}

		block->prev->next = block->next;
		block->next->prev = block->prev;
		block->size = -length;
		*(int *)((char *)block + length) = 0;
		return (char *)block + heap->headerSize;
	}
	return NULL;
}

/* a block's tags for a block of its own; its memory past the header */
static inline void *bxceHeapOwnBlock(BxceHeap *heap, BxceHeapBlock *block, int size)
{
	block->size = size;
	block->prevSize = BXCE_HEAP_OWN_BLOCK;
	return (char *)block + heap->headerSize;
}

void *bxceHeapAlloc(BxceHeap *heap, SceSize size)
{
	BxceHeapBlock *block;
	void *ptr;
	int total;

	if (size == 0)
		size = 1;

	total = (size + heap->overhead * 2 + 3) & ~3;
	if (total >= 0) {
		block = (BxceHeapBlock *)bxceDefaultAllocate(total);
		if (block == NULL)
			return NULL;
		ptr = bxceHeapOwnBlock(heap, block, total);
		if (ptr != NULL)
			return ptr;
	}

	ptr = bxceHeapAllocFromPool(heap, size);
	if (ptr != NULL)
		return ptr;

	if (heap->outOfMemory != NULL && heap->outOfMemory(size) == SCE_BXCE_INFO_RETRY) {
		ptr = bxceHeapAllocFromPool(heap, size);
		if (ptr != NULL)
			return ptr;
	}

	block = (BxceHeapBlock *)bxceMspaceMalloc(total);
	if (block == NULL)
		return NULL;
	ptr = bxceHeapOwnBlock(heap, block, total);
	if (ptr != NULL)
		return ptr;
	return NULL;
}

void *ScebXCe_B8DDE741(SceSize size)
{
	return bxceHeapAlloc(g_bxceHeap, size);
}

/* gives a block back: a block of its own to the allocator, a pool block to the free list */
void bxceHeapFree(BxceHeap *heap, void *ptr)
{
	BxceHeapBlock *block = (BxceHeapBlock *)((char *)ptr - heap->headerSize);
	BxceHeapBlock *free, *next;
	int size, length;

	size = block->size;
	if (size > 0 && block->prevSize == BXCE_HEAP_OWN_BLOCK) {
		bxceDefaultDeallocate(block);
		return;
	}
	if (size >= 0)
		return;

	length = -size;
	if (block->prevSize != 0) {
		/* into the free block before it */
		free = (BxceHeapBlock *)((char *)block - block->prevSize);
		block->size = length;
		free->size = free->size - size;
		length = free->size;
	} else {
		/* a free block of its own, at the end of the list */
		block->next = heap->freeList;
		block->prev = heap->freeList->prev;
		heap->freeList->prev = block;
		block->prev->next = block;
		block->size = length;
		free = block;
	}

	next = (BxceHeapBlock *)((char *)free + length);
	if (next->size > 0) {
		length += next->size;
		next->prev->next = next->next;
		next->next->prev = next->prev;
		free->size = length;
	}
	*(int *)((char *)free + length) = length;
}

void ScebXCe_EC560647(void *ptr)
{
	bxceHeapFree(g_bxceHeap, ptr);
}

/*
 * A block extra bytes larger than the one at ptr (a new one when ptr is NULL), cleared and
 * holding the old contents; the old block is freed, also when no new one could be had.
 */
void *bxceHeapExtend(BxceHeap *heap, void *ptr, SceSize extra)
{
	BxceHeapBlock *block = (BxceHeapBlock *)((char *)ptr - heap->headerSize);
	BxceHeapBlock *own;
	SceUInt32 size, length, copy;
	void *p;

	size = block->size;
	if ((int)size > 0 && block->prevSize == BXCE_HEAP_OWN_BLOCK) {
		length = size + extra;
		own = (BxceHeapBlock *)bxceDefaultAllocate(length + heap->headerSize);
		if (own == NULL) {
			if (ptr != NULL)
				bxceHeapFree(heap, ptr);
			return NULL;
		}
		own->size = length + heap->headerSize;
		own->prevSize = BXCE_HEAP_OWN_BLOCK;
		p = (char *)own + heap->headerSize;
		if (ptr == NULL)
			return p;
		bxceMemset(p, 0, length);
		copy = size;
		if (copy > extra)
			copy = extra;
		bxceMemcpy(p, ptr, copy);
		bxceDefaultDeallocate(block);
		return p;
	}

	size = -size - heap->headerSize;
	length = size + extra;
	p = bxceHeapAlloc(heap, length);
	if (p == NULL) {
		if (ptr != NULL)
			bxceHeapFree(heap, ptr);
		return NULL;
	}
	if (ptr == NULL)
		return p;
	bxceMemset(p, 0, length);
	copy = size;
	if (copy > extra)
		copy = extra;
	bxceMemcpy(p, ptr, copy);
	bxceHeapFree(heap, ptr);
	return p;
}
