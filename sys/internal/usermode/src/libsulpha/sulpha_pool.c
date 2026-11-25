/*
 * SceSulpha lock-free fixed-size pool and coalescing heap allocator.
 */

#include "sulpha_internal.h"

SceInt32 sulphaPoolGetNeededMemory(SceInt32 count, SceUInt32 elemSize,
				   SceUInt32 align, SceUInt32 *outSize)
{
	SceUInt32 stride = (elemSize + align + 3U) & ~(align - 1U);
	*outSize = (SceUInt32)count * stride;
	return 0;
}

SceInt32 sulphaPoolInit(SulphaFixedPool *pool, void *buf, SceInt32 count,
			SceUInt32 elemSize, SceUInt32 align)
{
	SceUInt32 stride = (elemSize + align + 3U) & ~(align - 1U);

	pool->head.ptr = NULL;
	pool->head.tag = 0;
	pool->base = buf;
	pool->capacity = count;
	pool->stride = stride;
	pool->align = align;
	pool->usedCount = 0;

	if (buf != NULL && count > 0) {
		SceUInt8 *slot = (SceUInt8 *)buf;
		for (SceInt32 i = 0; i < count; i++) {
			*(void **)slot = pool->head.ptr;
			pool->head.ptr = slot;
			slot += stride;
		}
	}
	return 0;
}

SceInt32 sulphaPoolShutdown(SulphaFixedPool *pool)
{
	pool->base = NULL;
	return 0;
}

SceInt32 sulphaPoolIsValid(const SulphaFixedPool *pool)
{
	return (pool->base != NULL) ? 0 : SULPHA_ERROR_INTERNAL;
}

void *sulphaPoolAlloc(SulphaFixedPool *pool)
{
	for (;;) {
		void *headPtr = pool->head.ptr;
		if (headPtr == NULL)
			return NULL;

		void *nextPtr = *(void **)headPtr;
		SceUInt32 tag = pool->head.tag;
		SceUInt64 expected = sulphaPackTaggedPtr(headPtr, tag);
		SceUInt64 replacement = sulphaPackTaggedPtr(nextPtr, tag + 1);
		SceUInt64 observed;

		do {
			observed = sulphaAtomicLoad64(&pool->head);
		} while (observed == expected && sulphaAtomicStore64(replacement, &pool->head) != 0);

		if (observed == expected) {
			sulphaAtomicInc32(&pool->usedCount);
			return (SceUInt8 *)headPtr + pool->align;
		}
	}
}

void sulphaPoolFree(SulphaFixedPool *pool, void *ptr)
{
	SceUIntPtr slotAddr = (SceUIntPtr)ptr - pool->align;
	SceUIntPtr baseAddr = (SceUIntPtr)pool->base;
	SceUIntPtr endAddr = baseAddr + (SceUIntPtr)pool->capacity * pool->stride;

	if (slotAddr < baseAddr || slotAddr >= endAddr)
		return;

	void **slot = (void **)slotAddr;
	for (;;) {
		void *oldHead = pool->head.ptr;
		*slot = oldHead;
		void *observed;
		do {
			observed = (void *)(SceUIntPtr)sulphaAtomicLoad32(&pool->head.ptr);
		} while (observed == oldHead && sulphaAtomicStore32((SceUInt32)slotAddr, &pool->head.ptr) != 0);

		if (observed == oldHead)
			break;
	}

	sulphaAtomicDec32(&pool->usedCount);
}

SceUInt32 sulphaHeapGetNeededMemory(SceUInt32 size, SceUInt32 *outSize)
{
	SceUInt32 aligned = (size + 15U) & ~15U;
	*outSize = aligned + 16U;
	return size + 15U;
}

void sulphaHeapInit(SulphaHeap *heap, void *buf, SceUInt32 size)
{
	SceUInt32 totalBlocks = (size + 15U) >> 4;
	SulphaHeapBlock *blocks = (SulphaHeapBlock *)buf;

	heap->base = blocks;
	heap->freeHead = NULL;
	heap->reserved0 = 0;
	heap->totalBlocks = totalBlocks;
	heap->allocCount = 0;
	heap->reserved1 = 0;

	if (blocks != NULL) {
		SulphaHeapBlock *sentinel = (SulphaHeapBlock *)((SceUInt8 *)blocks + (totalBlocks << 4));
		heap->freeHead = sentinel;
		blocks->blockCount = totalBlocks;
		blocks->next = NULL;
		sentinel->blockCount = 0;
		sentinel->next = blocks;
	}
}

void *sulphaHeapAlloc(SulphaHeap *heap, SceUInt32 size)
{
	SceUInt32 needBlocks = (size + 19U) >> 4;
	SulphaHeapBlock *prev = heap->freeHead;
	SulphaHeapBlock *cur = prev->next;

	while (cur != NULL && needBlocks > cur->blockCount) {
		prev = cur;
		cur = cur->next;
	}
	if (cur == NULL)
		return NULL;

	if (needBlocks < cur->blockCount) {
		SulphaHeapBlock *rem = (SulphaHeapBlock *)((SceUInt8 *)cur + (needBlocks << 4));
		rem->blockCount = cur->blockCount - needBlocks;
		rem->next = cur->next;
		cur->blockCount = needBlocks;
		cur->next = rem;
	}

	heap->allocCount++;
	prev->next = cur->next;
	return (SceUInt32 *)cur + 1;
}

void sulphaHeapFree(SulphaHeap *heap, void *ptr)
{
	if (ptr == NULL)
		return;

	SulphaHeapBlock *block = (SulphaHeapBlock *)((SceUInt32 *)ptr - 1);
	if (block == NULL)
		return;

	SulphaHeapBlock *prev = heap->freeHead;
	SulphaHeapBlock *next = prev->next;
	while (next != NULL && block > next) {
		prev = next;
		next = next->next;
	}

	if (next != NULL) {
		SulphaHeapBlock *blockEnd = (SulphaHeapBlock *)((SceUInt8 *)block + (block->blockCount << 4));
		if (blockEnd == next) {
			block->blockCount += next->blockCount;
			next = next->next;
		}
	}

	block->next = next;

	SulphaHeapBlock *prevEnd = (SulphaHeapBlock *)((SceUInt8 *)prev + (prev->blockCount << 4));
	if (prevEnd == block) {
		prev->blockCount += block->blockCount;
		prev->next = next;
	} else {
		prev->next = block;
	}

	heap->allocCount--;
}
