/*
 * memory.c: the pool libnet allocates from (sceNetInit's memory). Blocks carry a 16-byte
 * header; the used and the free blocks are each on a list in address order, and a freed block
 * merges with its free neighbours. Allocations stop at the reserve unless they may use it.
 */
#include <libkernel/libkernel.h>
#include "libnet_internal.h"

/* bits of libnet_malloc's flags */
#define LIBNET_MALLOC_ZERO		0x2	/* clear the block */
#define LIBNET_MALLOC_RESERVE		0x4	/* the reserve may be used */

/* what libnet_free leaves in a freed block */
#define LIBNET_FREE_FILL		0xdd

typedef struct LibnetBlock {
	struct LibnetBlock	*next;
	struct LibnetBlock	*prev;
	int			size;		/* the bytes after the header */
	int			reserved;
} LibnetBlock;

typedef struct LibnetMemory {
	LibnetBlock	*usedHead;
	LibnetBlock	*usedTail;
	LibnetBlock	*freeHead;
	LibnetBlock	*freeTail;
	char		*poolStart;
	char		*poolEnd;
	int		freeSize;
	int		freeMinimum;		/* the least freeSize has been */
	int		reserve;
	LibnetSync	sync;
} LibnetMemory;

static LibnetMemory libnet_memory;

int libnet_memory_free_size(int minimum)
{
	if (minimum)
		return libnet_memory.freeMinimum;
	return libnet_memory.freeSize;
}

void *libnet_malloc(int size, int flags)
{
	LibnetBlock *block, *rest, *next;
	int need, left;

	if (size <= 0)
		return NULL;
	need = (size + 7) & ~7;
	libnet_sync_wait(&libnet_memory.sync, NULL);
	if ((!(flags & LIBNET_MALLOC_RESERVE) && libnet_memory.freeSize < libnet_memory.reserve) ||
	    (block = libnet_memory.freeHead) == NULL)
		goto fail;
	while (need > block->size) {
		block = block->next;
		if (block == NULL)
			goto fail;
	}
	left = block->size - need - sizeof(LibnetBlock);
	if (left <= 0) {
		/* all of it */
		if (block->next != NULL)
			block->next->prev = block->prev;
		else
			libnet_memory.freeTail = block->prev;
		if (block->prev != NULL)
			block->prev->next = block->next;
		else
			libnet_memory.freeHead = block->next;
		rest = block;
		left = block->size;
	} else {
		/* its start; the rest stays free in its place */
		rest = (LibnetBlock *)((char *)block + need + sizeof(LibnetBlock));
		block->size = need;
		rest->size = left;
		rest->next = block->next;
		if (block->next != NULL)
			block->next->prev = rest;
		else
			libnet_memory.freeTail = rest;
		rest->prev = block->prev;
		if (block->prev != NULL)
			block->prev->next = rest;
		else
			libnet_memory.freeHead = rest;
	}
	next = (LibnetBlock *)((char *)rest + left + sizeof(LibnetBlock));
	if (libnet_memory.usedHead == NULL) {
		block->next = NULL;
		block->prev = NULL;
		libnet_memory.usedHead = block;
		libnet_memory.usedTail = block;
	} else if ((char *)next < libnet_memory.poolEnd) {
		/* before the used block that follows it */
		block->prev = next->prev;
		if (next->prev != NULL)
			next->prev->next = block;
		else
			libnet_memory.usedHead = block;
		block->next = next;
		next->prev = block;
	} else {
		block->next = NULL;
		block->prev = libnet_memory.usedTail;
		libnet_memory.usedTail->next = block;
		libnet_memory.usedTail = block;
	}
	left = libnet_memory.freeSize - sizeof(LibnetBlock) - block->size;
	if (left < libnet_memory.freeMinimum)
		libnet_memory.freeMinimum = left;
	libnet_memory.freeSize = left;
	libnet_sync_signal(&libnet_memory.sync, 1);
	if (flags & LIBNET_MALLOC_ZERO)
		sceClibMemset(block + 1, 0, size);
	return block + 1;
fail:
	libnet_sync_signal(&libnet_memory.sync, 1);
	return NULL;
}

int libnet_free(void *ptr)
{
	LibnetBlock *block, *scan, *next, *prev;
	int size, length;

	if (ptr == NULL)
		return 0;
	block = (LibnetBlock *)ptr - 1;
	if (block->size > 0)
		sceClibMemset(ptr, LIBNET_FREE_FILL, block->size);
	libnet_sync_wait(&libnet_memory.sync, NULL);
	if (block->next != NULL)
		block->next->prev = block->prev;
	else
		libnet_memory.usedTail = block->prev;
	if (block->prev != NULL)
		block->prev->next = block->next;
	else
		libnet_memory.usedHead = block->next;
	size = block->size;
	libnet_memory.freeSize += size + sizeof(LibnetBlock);

	/* the first free block after it: past the used blocks that follow it directly */
	scan = block;
	length = size + sizeof(LibnetBlock);
	for (;;) {
		next = (LibnetBlock *)((char *)scan + length);
		if (scan->next != next)
			break;
		scan = scan->next;
		length = scan->size + sizeof(LibnetBlock);
	}
	if ((char *)next < libnet_memory.poolEnd) {
		block->prev = next->prev;
		if (next->prev != NULL)
			next->prev->next = block;
		else
			libnet_memory.freeHead = block;
		block->next = next;
		next->prev = block;
	} else {
		block->next = NULL;
		block->prev = libnet_memory.freeTail;
		if (libnet_memory.freeTail != NULL)
			libnet_memory.freeTail->next = block;
		else
			libnet_memory.freeHead = block;
		libnet_memory.freeTail = block;
	}

	prev = block->prev;
	if (prev != NULL && (char *)block == (char *)prev + prev->size + sizeof(LibnetBlock)) {
		prev->size += size + sizeof(LibnetBlock);
		prev->next = block->next;
		if (prev->next != NULL)
			prev->next->prev = prev;
		else
			libnet_memory.freeTail = prev;
		block = prev;
	}
	next = block->next;
	if (next != NULL && (char *)next == (char *)block + block->size + sizeof(LibnetBlock)) {
		block->next = next->next;
		block->size += next->size + sizeof(LibnetBlock);
		if (block->next != NULL)
			block->next->prev = block;
		else
			libnet_memory.freeTail = block;
	}
	libnet_sync_signal(&libnet_memory.sync, 1);
	return 0;
}

int libnet_memory_init(void *pool, int size, int reserve)
{
	LibnetBlock *block;
	char *start = pool;
	int adjust;

	adjust = (char *)(((SceUIntPtr)start + 7) & ~7) - start;
	if (adjust != 0) {
		start += adjust;
		size -= adjust;
	}
	block = (LibnetBlock *)start;
	libnet_memory.freeHead = block;
	libnet_memory.freeTail = block;
	block->next = NULL;
	size = size / 7 * 7;
	block->prev = NULL;
	block->size = size - sizeof(LibnetBlock);
	libnet_memory.freeSize = size;
	libnet_memory.freeMinimum = size;
	libnet_memory.poolStart = start;
	libnet_memory.poolEnd = start + size;
	libnet_memory.reserve = reserve;
	return libnet_sync_create(&libnet_memory.sync, 1, "SceNetMemSync");
}

int libnet_memory_term(void)
{
	libnet_sync_destroy(&libnet_memory.sync);
	return 0;
}
