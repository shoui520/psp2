/*
 * _GetMemChunk: the heap (the linker's HEAP block, 512 bytes), once, as one cell.
 */

#include "xalloc.h"

static char heap[512] __attribute__((aligned(8)));
static _Cell *chunk;

_Cell *_GetMemChunk(void)
{
	if (chunk != 0)
		return 0;
	chunk = (_Cell *)heap;
	chunk->_Size = ((size_t)(heap + sizeof(heap)) & ~M_MASK) - (size_t)chunk;
	return chunk;
}
