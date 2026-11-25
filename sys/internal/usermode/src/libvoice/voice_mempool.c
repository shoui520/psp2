/*
 * 128-byte aligned block memory pool allocator for SceVoice.
 */

#include "voice_internal.h"

void voiceMemPoolInit(VoiceMemPool *pool, void *baseAddr, uint32_t totalSize,
                      VoiceMemBlock *blockTable, int blockCount, uint8_t isPrivate, const char *name)
{
	pool->freeBytes = totalSize;
	pool->name = name;
	pool->initialized = 1;
	pool->isPrivate = isPrivate;
	pool->allocCount = 0;
	pool->freeCount = 0;
	pool->usedBytes = 0;

	voiceListInit(&pool->availBlocks);
	voiceListInit(&pool->usedBlocks);
	voiceListInit(&pool->freeBlocks);

	voiceMemset(blockTable, 0, (uint32_t)blockCount * sizeof(VoiceMemBlock));
	for (int i = 0; i < blockCount; i++) {
		voiceListPushBack(&pool->availBlocks, &blockTable[i].node);
	}

	VoiceMemBlock *initial = (VoiceMemBlock *)voiceListPopFront(&pool->availBlocks);
	initial->address = baseAddr;
	initial->size = totalSize;
	initial->inUse = 1;
	voiceListPushFront(&pool->freeBlocks, &initial->node);
}

void voiceMemPoolTerm(VoiceMemPool *pool)
{
	while (pool->availBlocks.count != 0) {
		VoiceListNode *node = voiceListPopFront(&pool->availBlocks);
		if (node == NULL) {
			break;
		}
	}
	voiceListPopFront(&pool->freeBlocks);
}

void *voiceMemPoolAlloc(VoiceMemPool *pool, uint32_t size)
{
	uint32_t alignedSize = (size + 127u) & ~127u;
	VoiceMemBlock *candidate = (VoiceMemBlock *)pool->freeBlocks.head;

	while (candidate != NULL && candidate->size < alignedSize) {
		candidate = (VoiceMemBlock *)candidate->node.next;
	}
	if (candidate == NULL) {
		return NULL;
	}

	VoiceMemBlock *allocated;
	if (candidate->size == alignedSize) {
		voiceListRemove(&pool->freeBlocks, &candidate->node);
		allocated = candidate;
	} else {
		allocated = (VoiceMemBlock *)voiceListPopFront(&pool->availBlocks);
		allocated->inUse = 1;
		allocated->size = alignedSize;
		allocated->address = candidate->address;
		candidate->address = (uint8_t *)candidate->address + alignedSize;
		candidate->size -= alignedSize;
	}

	voiceListPushFront(&pool->usedBlocks, &allocated->node);
	pool->usedBytes += alignedSize;
	pool->freeBytes -= alignedSize;
	pool->allocCount++;
	return allocated->address;
}

void voiceMemPoolCoalesce(VoiceMemBlock *target, VoiceList *freeList,
                          VoiceList *availList, VoiceMemBlock *neighbor)
{
	target->size += neighbor->size;
	target->address = neighbor->address;
	voiceListRemove(freeList, &neighbor->node);
	neighbor->size = 0;
	neighbor->inUse = 0;
	neighbor->address = NULL;
	voiceListPushFront(availList, &neighbor->node);
}

void voiceMemPoolInsertSorted(VoiceMemBlock *block, VoiceList *freeList)
{
	VoiceMemBlock *cursor = (VoiceMemBlock *)freeList->head;
	if (cursor == NULL) {
		voiceListPushFront(freeList, &block->node);
		return;
	}

	while (cursor != NULL && (uintptr_t)cursor->address <= (uintptr_t)block->address) {
		cursor = (VoiceMemBlock *)cursor->node.next;
	}

	if (cursor == NULL) {
		voiceListPushBack(freeList, &block->node);
		return;
	}

	VoiceListNode *prev = cursor->node.prev;
	block->node.prev = prev;
	block->node.next = &cursor->node;
	cursor->node.prev = &block->node;
	if (prev != NULL) {
		prev->next = &block->node;
	} else {
		freeList->head = &block->node;
	}
	block->node.owner = freeList;
	freeList->count++;
}

void voiceMemPoolFree(VoiceMemPool *pool, void *addr)
{
	VoiceMemBlock *block = (VoiceMemBlock *)pool->usedBlocks.head;
	while (block != NULL && block->address != addr) {
		block = (VoiceMemBlock *)block->node.next;
	}
	if (block == NULL) {
		return;
	}

	pool->freeBytes += block->size;
	pool->usedBytes -= block->size;
	voiceListRemove(&pool->usedBlocks, &block->node);
	voiceMemPoolInsertSorted(block, &pool->freeBlocks);

	VoiceMemBlock *prevBlock = (VoiceMemBlock *)block->node.prev;
	if (prevBlock != NULL &&
	    (uint8_t *)prevBlock->address + prevBlock->size == (uint8_t *)block->address) {
		voiceMemPoolCoalesce(block, &pool->freeBlocks, &pool->availBlocks, prevBlock);
	}

	VoiceMemBlock *nextBlock = (VoiceMemBlock *)block->node.next;
	if (nextBlock != NULL &&
	    (uint8_t *)block->address + block->size == (uint8_t *)nextBlock->address) {
		voiceMemPoolCoalesce(nextBlock, &pool->freeBlocks, &pool->availBlocks, block);
	}

	pool->freeCount++;
}
