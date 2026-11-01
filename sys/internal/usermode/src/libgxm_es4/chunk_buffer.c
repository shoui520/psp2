/*
 * SceGxm: chunk buffers.
 *
 * Commands and data are written into a chunk of memory; when it runs out
 * a deferred context asks its callback for the next chunk.
 */

#include <sceerror.h>
#include "gxm_internal.h"

void *gxmChunkBufferReserveSlow(GxmChunkBuffer *buffer, SceSize size, SceSize alignment)
{
	SceUInt32 block;

	buffer->refill(buffer, size, alignment);
	block = (buffer->cur - 1 + alignment) & -alignment;
	if (buffer->end < block + size)
		return NULL;
#if SCE_DBG_ASSERTS_ENABLED
	buffer->reserved = block + size;
#endif
	buffer->cur = block;
	return (void *)block;
}

static void gxmDeferredBufferRefill(GxmChunkBuffer *buffer, SceSize size, SceSize alignment)
{
	GxmDeferredBuffer *chunkBuf = buffer->cookie;
	SceSize minSize, chunkSize;
	SceUInt32 memory, block, reserve;

	GXM_DBG_ASSERT(16, chunkBuf);
	minSize = size + alignment;
	if (minSize < 0x400)
		minSize = 0x400;
	chunkSize = 0;
	memory = (SceUInt32)chunkBuf->callback(chunkBuf->userData, minSize, &chunkSize);
	if (memory == 0 || chunkSize < minSize)
		return;
#if SCE_DBG_ASSERTS_ENABLED
	if (gxmMemoryCheckCallbackRegion((void *)memory, chunkSize,
					 "SceGxmDeferredContextCallback") != SCE_OK) {
		chunkBuf->invalidCallback = SCE_TRUE;
		chunkBuf->invalidAddress = (void *)memory;
		chunkBuf->invalidSize = chunkSize;
	}
#endif
	block = (memory + alignment - 1) & -alignment;
	reserve = 0;
	if (chunkBuf->linked) {
		/* the old chunk ends in a jump to the new one */
		if (buffer->cur != 0)
			*(SceUInt32 *)buffer->cur = block >> 2;
		reserve = GXM_CHUNK_LINK_RESERVE;
	}
	buffer->cur = block;
#if SCE_DBG_ASSERTS_ENABLED
	buffer->reserved = block;
#endif
	buffer->end = memory + chunkSize - reserve;
	buffer->gpuEnd = 0;
}

void gxmChunkBufferInitDeferred(GxmDeferredBuffer *chunkBuffer, GxmChunkBuffer *writeBuffer,
				void *memory, SceSize size, GxmDeferredBufferCallback *callback,
				void *userData, SceUInt8 linked)
{
	SceSize reservedBytes;

	if (memory == NULL) {
		size = 0;
	} else {
		reservedBytes = linked ? GXM_CHUNK_LINK_RESERVE : 0;
		GXM_DBG_ASSERT(75, size > reservedBytes);
		size -= reservedBytes;
	}
	chunkBuffer->callback = callback;
	chunkBuffer->userData = userData;
	chunkBuffer->linked = linked;
#if SCE_DBG_ASSERTS_ENABLED
	chunkBuffer->invalidCallback = SCE_FALSE;
	chunkBuffer->invalidAddress = NULL;
	chunkBuffer->invalidSize = 0;
#endif
	writeBuffer->cur = (SceUInt32)memory;
	writeBuffer->end = (SceUInt32)memory + size;
	writeBuffer->cookie = chunkBuffer;
#if SCE_DBG_ASSERTS_ENABLED
	writeBuffer->reserved = (SceUInt32)memory;
#endif
	writeBuffer->refill = gxmDeferredBufferRefill;
	writeBuffer->gpuEnd = 0;
}

void gxmChunkBufferReset(const GxmDeferredBuffer *chunkBuffer, GxmChunkBuffer *writeBuffer,
			 void *memory, SceSize size)
{
	SceSize reservedBytes;

	GXM_DBG_ASSERT(107, writeBuffer->cookie == chunkBuffer);
	if (memory == NULL) {
		size = 0;
	} else {
		reservedBytes = chunkBuffer->linked ? GXM_CHUNK_LINK_RESERVE : 0;
		GXM_DBG_ASSERT(112, size > reservedBytes);
		size -= reservedBytes;
	}
#if SCE_DBG_ASSERTS_ENABLED
	writeBuffer->reserved = (SceUInt32)memory;
#endif
	writeBuffer->cur = (SceUInt32)memory;
	writeBuffer->end = (SceUInt32)memory + size;
}

#if SCE_DBG_ASSERTS_ENABLED
/* report memory the callback returned that GXM cannot use, once */
int gxmDeferredBufferCheckCallback(GxmDeferredBuffer *chunkBuffer, const char *bufferName)
{
	if (!chunkBuffer->invalidCallback)
		return SCE_OK;
	GXM_DBG_ERROR(131, "The SceGxmDeferredContextCallback for the %s buffer returned an address "
		      "of 0x%p with a size of %d bytes, which does not refer to a valid region of "
		      "memory.", bufferName, chunkBuffer->invalidAddress, chunkBuffer->invalidSize);
	chunkBuffer->invalidCallback = SCE_FALSE;
	chunkBuffer->invalidAddress = NULL;
	chunkBuffer->invalidSize = 0;
	return SCE_GXM_ERROR_INVALID_POINTER;
}
#endif
