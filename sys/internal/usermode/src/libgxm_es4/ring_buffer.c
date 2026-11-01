/*
 * SceGxm: an immediate context's ring buffers.
 *
 * Each chunk buffer of an immediate context writes into a ring the GPU
 * reads behind it; the GPU's read offset sits in the context's slot.
 */

#include "gxm_internal.h"

/* the next free stretch of the ring, waiting for the GPU while there is none */
static void gxmRingBufferRefill(GxmChunkBuffer *writeBuffer, SceSize size, SceSize alignment)
{
	GxmRingBuffer *ringBuffer = writeBuffer->ring;
	SceUInt32 cur, end, aligned, offset, used;

	GXM_DBG_ASSERT(93, SCE_GXM_IS_POW2(alignment));
	GXM_DBG_ASSERT(94, size + alignment < ringBuffer->commitLimit);
	for (;;) {
		SceBool wait;

		cur = writeBuffer->cur;
		end = writeBuffer->end;
		do {
			SceUInt32 read = *ringBuffer->readOffset;
			SceUInt32 endOffset = end - ringBuffer->memory;
			SceUInt32 curOffset = cur - ringBuffer->memory;
			SceSize available;

			if (read == 0) {
				available = ringBuffer->usableSize;
				wait = available == endOffset;
			} else {
				available = read - 1;
				if (endOffset < available) {
					wait = SCE_TRUE;
				} else if (endOffset == ringBuffer->usableSize) {
					/* back to the start, behind the GPU */
					if (ringBuffer->linked) {
						GXM_DBG_ASSERT(62, (writeBuffer->cur & 0x3) == 0);
						*(SceUInt32 *)writeBuffer->cur = ringBuffer->memory >> 2;
					}
					curOffset = 0;
					wait = SCE_TRUE;
				} else if (curOffset >= read) {
					available = ringBuffer->usableSize;
					wait = SCE_FALSE;
				} else {
					available = endOffset;
					wait = SCE_TRUE;
				}
			}
			cur = ringBuffer->memory + curOffset;
			writeBuffer->cur = cur;
			aligned = (cur + alignment - 1) & -alignment;
			end = ringBuffer->memory + available;
			writeBuffer->end = end;
			writeBuffer->gpuEnd = available + ringBuffer->gpuOffset;
			if (end >= aligned + size)
				goto found;
		} while (!wait);
		sceGxmWaitEvent();
	}

found:
	/* the next kick is due half a ring on */
	offset = cur - ringBuffer->memory;
	used = offset - ringBuffer->lastOffset +
	       (offset < ringBuffer->lastOffset ? ringBuffer->size : 0);
	if (used < ringBuffer->commitLimit)
		cur += ringBuffer->commitLimit - used;
	ringBuffer->threshold = cur;
	writeBuffer->cur = aligned;
}

void gxmRingBufferInit(GxmRingBuffer *ringBuffer, GxmChunkBuffer *writeBuffer, void *memory,
		       SceUInt32 gpuOffset, SceSize allocSize, SceUInt8 linked,
		       volatile SceUInt32 *readOffset)
{
	SceSize reservedBytes = linked ? GXM_CHUNK_LINK_RESERVE : 0;
	SceSize usableSize;

	GXM_DBG_ASSERT(150, allocSize > reservedBytes);
	usableSize = allocSize - reservedBytes;
	ringBuffer->memory = (SceUInt32)memory;
	ringBuffer->gpuOffset = gpuOffset;
	ringBuffer->linked = linked;
	ringBuffer->usableSize = usableSize;
	ringBuffer->size = allocSize;
	ringBuffer->commitLimit = usableSize / 2;
	ringBuffer->threshold = (SceUInt32)memory + usableSize / 2;
	ringBuffer->readOffset = readOffset;
	ringBuffer->lastOffset = 0;
	writeBuffer->end = (SceUInt32)memory + usableSize;
	writeBuffer->gpuEnd = usableSize + gpuOffset;
	writeBuffer->cur = (SceUInt32)memory;
	writeBuffer->ring = ringBuffer;
#if SCE_DBG_ASSERTS_ENABLED
	writeBuffer->reserved = (SceUInt32)memory;
#endif
	writeBuffer->refill = gxmRingBufferRefill;
	*readOffset = 0;
}

/* a kick sent the ring up to the buffer's position: the next one is due half a ring on */
void gxmRingBufferKicked(GxmRingBuffer *ringBuffer, GxmChunkBuffer *writeBuffer)
{
	SceUInt32 cur;
	SceUInt32 offset;

	GXM_DBG_ASSERT(186, writeBuffer->ring == ringBuffer);
	cur = writeBuffer->cur;
	offset = cur - ringBuffer->memory;

	/* at the end of the ringBuffer: wait for the GPU to leave the start */
	if (offset == ringBuffer->usableSize) {
		for (;;) {
			SceUInt32 end = writeBuffer->end;
			SceBool wait;

			do {
				SceUInt32 read = *ringBuffer->readOffset;
				SceUInt32 endOffset = end - ringBuffer->memory;
				SceUInt32 curOffset = cur - ringBuffer->memory;
				SceSize available;

				if (read == 0) {
					available = ringBuffer->usableSize;
					wait = endOffset == available;
				} else {
					available = read - 1;
					if (endOffset < available) {
						wait = SCE_TRUE;
					} else if (endOffset == ringBuffer->usableSize) {
						if (ringBuffer->linked) {
							GXM_DBG_ASSERT(62, (cur & 0x3) == 0);
							*(SceUInt32 *)cur = ringBuffer->memory >> 2;
						}
						curOffset = 0;
						wait = SCE_TRUE;
					} else if (curOffset >= read) {
						available = ringBuffer->usableSize;
						wait = SCE_FALSE;
					} else {
						available = endOffset;
						wait = SCE_TRUE;
					}
				}
				cur = ringBuffer->memory + curOffset;
				writeBuffer->cur = cur;
				end = ringBuffer->memory + available;
				writeBuffer->end = end;
				writeBuffer->gpuEnd = available + ringBuffer->gpuOffset;
				offset = cur - ringBuffer->memory;
				if (offset != ringBuffer->usableSize)
					goto done;
			} while (!wait);
			sceGxmWaitEvent();
			cur = writeBuffer->cur;
			offset = cur - ringBuffer->memory;
		}
	}

done:
	ringBuffer->lastOffset = offset;
	ringBuffer->threshold = cur + ringBuffer->commitLimit;
}
