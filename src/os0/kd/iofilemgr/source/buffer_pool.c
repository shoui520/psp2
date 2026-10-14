/*
 * SceIofilemgr: pools of fixed size buffers, kept on two lock-free stacks of
 * nodes (file name unknown: no debug records).
 */

#include "iofilemgr_internal.h"
#include "../../../../common/sysclib/atomic.h"

/* push node onto a stack */
static inline __attribute__((always_inline)) void bufferPoolPush(BufferPoolNode *volatile *list,
								 BufferPoolNode *node)
{
	do {
		node->next = (BufferPoolNode *)AtomicLoadReserved32((volatile uint32_t *)list);
		AtomicFullBarrier();
	} while (AtomicStoreReserved32((volatile uint32_t *)list, (uint32_t)node));
}

/* num buffers of size bytes from base, nodes[] holding them */
int bufferPoolInitialize(BufferPool *pool, SceUInt32 num, void *base, SceSize size,
			 BufferPoolNode *nodes)
{
	int i;

	if (size == 0)
		return -1;
	pool->freeList = NULL;
	pool->base = base;
	pool->size = size;
	pool->num = num;
	pool->nodeList = NULL;
	for (i = num - 1; i >= 0; i--) {
		nodes[i].buf = (char *)base + i * size;
		bufferPoolPush(&pool->freeList, &nodes[i]);
	}
	return 0;
}

/* take a free buffer, NULL when none is left */
void *bufferPoolAlloc(BufferPool *pool)
{
	BufferPoolNode *node;
	void *buf;

	do {
		node = (BufferPoolNode *)AtomicLoadReserved32((volatile uint32_t *)&pool->freeList);
		if (node == NULL)
			return NULL;
		AtomicFullBarrier();
	} while (AtomicStoreReserved32((volatile uint32_t *)&pool->freeList,
				       (uint32_t)node->next));
	buf = node->buf;
	node->buf = NULL;
	bufferPoolPush(&pool->nodeList, node);
	return buf;
}

/* give a buffer back: -1 below the pool, -2 not a buffer's start, -3 no node */
int bufferPoolFree(BufferPool *pool, void *buf)
{
	BufferPoolNode *node;

	if ((SceInt32)(uintptr_t)buf < (SceInt32)(uintptr_t)pool->base)
		return -1;
	if ((SceSize)((char *)buf - (char *)pool->base) % pool->size != 0)
		return -2;
	do {
		node = (BufferPoolNode *)AtomicLoadReserved32((volatile uint32_t *)&pool->nodeList);
		if (node == NULL)
			return -3;
		AtomicFullBarrier();
	} while (AtomicStoreReserved32((volatile uint32_t *)&pool->nodeList,
				       (uint32_t)node->next));
	node->buf = buf;
	bufferPoolPush(&pool->freeList, node);
	return 0;
}
