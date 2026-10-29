/*
 * SceGxm: an immediate context's ring buffers.
 *
 * Each chunk buffer of an immediate context writes into a ring the GPU
 * reads behind it; the GPU's read offset sits in the context's slot.
 */

#include "gxm_internal.h"

/* the next free stretch of the ring, waiting for the GPU while there is none */
static void gxmRingBufferRefill(GxmChunkBuffer *buffer, SceSize size, SceSize alignment)
{
	GxmRingBuffer *ring = buffer->ring;
	SceUInt32 cur, end, aligned, offset, used;

	for (;;) {
		SceBool wait;

		cur = buffer->cur;
		end = buffer->end;
		do {
			SceUInt32 read = *ring->readOffset;
			SceUInt32 endOffset = end - ring->memory;
			SceUInt32 curOffset = cur - ring->memory;
			SceSize available;

			if (read == 0) {
				available = ring->usableSize;
				wait = available == endOffset;
			} else {
				available = read - 1;
				if (endOffset < available) {
					wait = SCE_TRUE;
				} else if (endOffset == ring->usableSize) {
					/* back to the start, behind the GPU */
					if (ring->linked)
						*(SceUInt32 *)cur = ring->memory >> 2;
					curOffset = 0;
					wait = SCE_TRUE;
				} else if (curOffset >= read) {
					available = ring->usableSize;
					wait = SCE_FALSE;
				} else {
					available = endOffset;
					wait = SCE_TRUE;
				}
			}
			cur = ring->memory + curOffset;
			buffer->cur = cur;
			aligned = (cur + alignment - 1) & -alignment;
			end = ring->memory + available;
			buffer->end = end;
			buffer->gpuEnd = available + ring->gpuOffset;
			if (end >= aligned + size)
				goto found;
		} while (!wait);
		sceGxmWaitEvent();
	}

found:
	/* the next kick is due half a ring on */
	offset = cur - ring->memory;
	used = offset - ring->lastOffset + (offset < ring->lastOffset ? ring->size : 0);
	if (used < ring->halfSize)
		cur += ring->halfSize - used;
	ring->threshold = cur;
	buffer->cur = aligned;
}

void gxmRingBufferInit(GxmRingBuffer *ring, GxmChunkBuffer *buffer, void *memory,
		       SceUInt32 gpuOffset, SceSize size, SceUInt8 linked,
		       volatile SceUInt32 *readOffset)
{
	SceSize usableSize = size - (linked ? GXM_CHUNK_LINK_RESERVE : 0);

	ring->memory = (SceUInt32)memory;
	ring->gpuOffset = gpuOffset;
	ring->linked = linked;
	ring->usableSize = usableSize;
	ring->size = size;
	ring->halfSize = usableSize / 2;
	ring->threshold = (SceUInt32)memory + usableSize / 2;
	ring->readOffset = readOffset;
	ring->lastOffset = 0;
	buffer->end = (SceUInt32)memory + usableSize;
	buffer->gpuEnd = usableSize + gpuOffset;
	buffer->cur = (SceUInt32)memory;
	buffer->ring = ring;
	buffer->refill = gxmRingBufferRefill;
	*readOffset = 0;
}
