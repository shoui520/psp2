/*
 * SceGxm: chunk buffers.
 *
 * Commands and data are written into a chunk of memory; when it runs out
 * a deferred context asks its callback for the next chunk.
 */

#include "gxm_internal.h"

void *gxmChunkBufferReserveSlow(GxmChunkBuffer *buffer, SceSize size, SceSize alignment)
{
	SceUInt32 block;

	buffer->refill(buffer, size, alignment);
	block = (buffer->cur - 1 + alignment) & -alignment;
	if (buffer->end < block + size)
		return NULL;
	buffer->cur = block;
	return (void *)block;
}

static void gxmDeferredBufferRefill(GxmChunkBuffer *buffer, SceSize size, SceSize alignment)
{
	GxmDeferredBuffer *source = buffer->source;
	SceSize minSize = size + alignment, chunkSize;
	SceUInt32 memory, block, reserve;

	if (minSize < 0x400)
		minSize = 0x400;
	chunkSize = 0;
	memory = (SceUInt32)source->callback(source->userData, minSize, &chunkSize);
	if (memory == 0 || chunkSize < minSize)
		return;
	block = (memory + alignment - 1) & -alignment;
	reserve = 0;
	if (source->linked) {
		/* the old chunk ends in a jump to the new one */
		if (buffer->cur != 0)
			*(SceUInt32 *)buffer->cur = block >> 2;
		reserve = GXM_CHUNK_LINK_RESERVE;
	}
	buffer->cur = block;
	buffer->end = memory + chunkSize - reserve;
	buffer->gpuEnd = 0;
}

void gxmChunkBufferInitDeferred(GxmDeferredBuffer *source, GxmChunkBuffer *buffer, void *memory,
				SceSize size, GxmDeferredBufferCallback *callback, void *userData,
				SceUInt8 linked)
{
	if (memory == NULL)
		size = 0;
	else
		size -= linked ? GXM_CHUNK_LINK_RESERVE : 0;
	source->callback = callback;
	source->userData = userData;
	source->linked = linked;
	buffer->cur = (SceUInt32)memory;
	buffer->end = (SceUInt32)memory + size;
	buffer->source = source;
	buffer->refill = gxmDeferredBufferRefill;
	buffer->gpuEnd = 0;
}

void gxmChunkBufferReset(const GxmDeferredBuffer *source, GxmChunkBuffer *buffer, void *memory,
			 SceSize size)
{
	if (memory == NULL)
		size = 0;
	else
		size -= source->linked ? GXM_CHUNK_LINK_RESERVE : 0;
	buffer->cur = (SceUInt32)memory;
	buffer->end = (SceUInt32)memory + size;
}
