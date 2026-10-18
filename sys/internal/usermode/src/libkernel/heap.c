#include <libkernel/libkernel.h>

struct HeapChunkHeader {
	SceSize previousSize;
	SceSize sizeAndFlags;
};

enum {
	HEAP_CHUNK_IN_USE = 2,
	HEAP_CHUNK_EXTRA_OVERHEAD = 4,
	HEAP_CHUNK_FLAGS = 7,
};

/* SCE: overhead comes from head bit2, unlike the current dlmalloc candidates. */
SceSize sceClibMspaceMallocUsableSize(void *pointer)
{
	const struct HeapChunkHeader *chunk;
	SceSize head, overhead;

	if (!pointer)
		return 0;
	chunk = (const struct HeapChunkHeader *)pointer - 1;
	head = chunk->sizeAndFlags;
	if (!(head & HEAP_CHUNK_IN_USE))
		return 0;
	overhead = head & HEAP_CHUNK_EXTRA_OVERHEAD ? 8 : 4;
	return (head & ~HEAP_CHUNK_FLAGS) - overhead;
}
