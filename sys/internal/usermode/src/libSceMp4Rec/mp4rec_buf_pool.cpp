/*
 * SceLibMp4Recorder aligned write-block buffer pool.
 */
#include "mp4rec_internal.h"

int32_t BufferPoolFreeBlocks(BufferPool *pool, int32_t count)
{
	MemAllocator *alloc = pool->allocator;
	BufferQueueTerm(&pool->queue);
	if (pool->entries != NULL) {
		for (int32_t i = 0; i < count; i++) {
			BufferPoolBlock *blk = (BufferPoolBlock *)pool->entries[i].data;
			if (blk != NULL) {
				if (blk->data != NULL) {
					alloc->Free(blk->data);
					blk->data = NULL;
				}
				alloc->Free(blk);
				pool->entries[i].data = NULL;
			}
		}
		alloc->Free(pool->entries);
		pool->entries = NULL;
	}
	return 0;
}

int32_t BufferPoolInit(BufferPool *pool, MemAllocator *allocator, uint32_t blockSize,
	uint32_t blockCount, const char *mutexName, BufferPoolCallbackList *cbList)
{
	memset(pool, 0, sizeof(*pool));
	pool->allocator = allocator;

	pool->entries = (BufferQueueEntry *)allocator->Malloc(blockCount * sizeof(BufferQueueEntry));
	if (pool->entries == NULL)
		return (int32_t)SCE_MP4REC_ERROR_OUT_OF_MEMORY;

	memset(pool->entries, 0, blockCount * sizeof(BufferQueueEntry));

	for (uint32_t i = 0; i < blockCount; i++) {
		BufferPoolBlock *blk = (BufferPoolBlock *)allocator->Malloc(sizeof(BufferPoolBlock));
		if (blk == NULL) {
			BufferPoolFreeBlocks(pool, (int32_t)i);
			return (int32_t)SCE_MP4REC_ERROR_OUT_OF_MEMORY;
		}
		memset(blk, 0, sizeof(BufferPoolBlock));
		pool->entries[i].data = blk;
		pool->entries[i].size = sizeof(BufferPoolBlock);

		void *mem = allocator->Memalign(64, blockSize);
		if (mem == NULL) {
			BufferPoolFreeBlocks(pool, (int32_t)i);
			return (int32_t)SCE_MP4REC_ERROR_OUT_OF_MEMORY;
		}
		blk->data = mem;
		blk->usedSize = 0;
		blk->capacity = blockSize;
	}

	int32_t ret = BufferQueueInit(&pool->queue, blockSize, blockCount, pool->entries, mutexName);
	if (ret < 0)
		BufferPoolFreeBlocks(pool, (int32_t)blockCount);
	pool->cbList = cbList;
	return ret;
}

int32_t BufferPoolTerm(BufferPool *pool)
{
	int32_t cap = BufferQueueGetCapacity(&pool->queue);
	if (cap >= 0)
		return BufferPoolFreeBlocks(pool, cap);
	return cap;
}

int32_t BufferPoolGetReadBlock(BufferPool *pool, BufferPoolBlock **outBlock)
{
	BufferQueueEntry *entry = NULL;
	int32_t ret = BufferQueuePeekRead(&pool->queue, 0, &entry);
	*outBlock = (ret >= 0) ? (BufferPoolBlock *)entry->data : NULL;
	return ret;
}

int32_t BufferPoolReleaseReadBlock(BufferPool *pool)
{
	int32_t ret = BufferQueueCommitRead(&pool->queue, 1);
	int32_t used = BufferQueueGetUsedCount(&pool->queue);
	int32_t cap = BufferQueueGetCapacity(&pool->queue);
	if (used >= 0 && cap > 0)
		BufferPoolNotifyCallbacks(pool->cbList, 4, used, cap);
	return ret;
}

int32_t BufferPoolGetWriteBlock(BufferPool *pool, BufferPoolBlock **outBlock)
{
	BufferQueueEntry *entry = NULL;
	int32_t ret = BufferQueuePeekWrite(&pool->queue, 0, &entry);
	*outBlock = (ret >= 0) ? (BufferPoolBlock *)entry->data : NULL;
	return ret;
}

int32_t BufferPoolCommitWriteBlock(BufferPool *pool)
{
	int32_t ret = BufferQueueCommitWrite(&pool->queue, 1);
	int32_t used = BufferQueueGetUsedCount(&pool->queue);
	int32_t cap = BufferQueueGetCapacity(&pool->queue);
	if (used >= 0 && cap > 0)
		BufferPoolNotifyCallbacks(pool->cbList, 2, used, cap);
	return ret;
}

int32_t BufferPoolGetQueuedCount(BufferPool *pool)
{
	return BufferQueueGetUsedCount(&pool->queue);
}
