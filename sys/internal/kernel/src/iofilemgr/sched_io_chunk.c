/* SceIofilemgr's request chunk pool (Sony's sched_io_chunk.c). */
#include "iofilemgr_internal.h"

static inline __attribute__((always_inline)) int isIoChunkInPool(IoChunkPool *pool,
							       IoChunk *chunk)
{
	SceUIntPtr base = (SceUIntPtr)pool->base;
	SceUIntPtr address = (SceUIntPtr)chunk;

	return address >= base && address <= base + pool->size - sizeof(*chunk) &&
	       address - base == ((address - base) / sizeof(*chunk)) * sizeof(*chunk);
}

#if FW_VERSION >= 0x04000000
int sceIoChunkIsInPool(IoChunkPool *pool, IoChunk *chunk)
{
	return isIoChunkInPool(pool, chunk);
}
#endif

int sceIoChunkCreateIoChunkPool(IoChunk *chunks, int chunkNum, IoChunkPool *pool)
{
	IoChunk *chunk;
	SceUInt32 count;
	int i;

	if (pool != NULL && chunks != NULL && chunkNum != 0) {
		pool->base = chunks;
		pool->size = sizeof(*chunks) * chunkNum;
		chunk = chunks;
		for (i = 0; i != chunkNum - 1; i++) {
			chunk++;
			chunk[-1].next = chunk;
		}
		chunks[chunkNum - 1].next = NULL;
		pool->freeList = chunks;
		pool->freeNum = chunkNum;
		count = 0;
		for (chunk = chunks->next; chunk != NULL; chunk = chunk->next)
			count++;
		SCE_DBG_ASSERT(count == chunkNum - 1, 70);
	}
	return 0;
}

IoChunk *sceIoChunkCreateIoChunk(IoChunkPool *pool, IoRequest *req, SceUIntPtr data,
				       SceSize offset, SceSize size)
{
	IoChunk *chunk;

	if (pool->freeNum == 0)
		return NULL;
	chunk = pool->freeList;
	pool->freeNum--;
	pool->freeList = chunk->next;
	SCE_DBG_ASSERT(isIoChunkInPool(pool, chunk), 103);
	memset(chunk, 0, sizeof(*chunk));
	chunk->request = req;
	chunk->data = data + offset;
	chunk->offset = offset;
	chunk->size = size;
	return chunk;
}

void sceIoChunkDestroyIoChunk(IoChunkPool *pool, IoChunk *chunk)
{
	SCE_DBG_ASSERT(isIoChunkInPool(pool, chunk), 118);
	memset(chunk, 0, sizeof(*chunk));
	chunk->next = pool->freeList;
	pool->freeList = chunk;
	pool->freeNum++;
}
