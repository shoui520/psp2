/*
 * A growing run of information elements in a pool buffer: the extra
 * elements the host sets for a join, then the response's. The filename is
 * inferred; no source filename survives.
 */
#include <stddef.h>
#include <stdint.h>
#include "robin/btrom.h"
#include "robin/buffer.h"
#include "robin/ie.h"
#include "robin/patch.h"
#include "robin/rom.h"
#include "robin/runtime.h"
#include "robin/threadx.h"

/* Override slots (0x55be0..): append, append a TLV, allocate. */
static ROBIN_PATCH_SLOT(wlanIeBufferAppendOverride);
static ROBIN_PATCH_SLOT(wlanIeBufferAppendTlvOverride);
static ROBIN_PATCH_SLOT(wlanIeBufferAllocOverride);

/* The pool the buffers come from and its blocks' size, set by wlanIeBufferInit(). */
struct OsaBufferPool *wlanIeBufferPool __attribute__((section(".itcm.rw")));	/* 0x54e68 */
uint32_t wlanIeBufferBlockSize __attribute__((section(".itcm.rw")));		/* 0x54e6c */

extern struct OsaBufferPool wlanPacketBufferPool;				/* 0x04000dcc */

/* Appends length bytes; returns how many were kept (0 when they do not fit). */
unsigned int wlanIeBufferAppend(struct WlanIeBuffer *ies, const void *data, int length)
{
	int result = 0;

	if (wlanIeBufferAppendOverride(ies, data, length, &result) == 0) {
		if (ies->size - ies->length > length) {
			__aeabi_memcpy((uint8_t *)ies->buffer + ies->buffer->dataOffset + ies->length,
				       data, length);
			ies->length += length;
			return (uint16_t)length;
		}
	}
	return (uint16_t)result;
}

/* Appends a TLV (type and length as little-endian halfwords, then the value) as a one-byte-type element. */
unsigned int wlanIeBufferAppendTlv(struct WlanIeBuffer *ies, const uint8_t *tlv)
{
	int result = 0;
	uint8_t header[2];
	int length;

	if (wlanIeBufferAppendTlvOverride(ies, tlv, &result) == 0) {
		length = (tlv[3] << 8) | tlv[2];
		if (ies->size - ies->length > length + 2) {
			header[0] = tlv[0];
			header[1] = tlv[2];
			result = wlanIeBufferAppend(ies, header, 2);
			length = wlanIeBufferAppend(ies, tlv + 4, (tlv[3] << 8) | tlv[2]);
			return (uint16_t)(length + result);
		}
	}
	return (uint16_t)result;
}

/* The elements kept; returns their length. */
unsigned int wlanIeBufferGet(struct WlanIeBuffer *ies, uint8_t **data)
{
	*data = (uint8_t *)ies->buffer + ies->buffer->dataOffset;
	return ies->length;
}

void wlanIeBufferReset(struct WlanIeBuffer *ies)
{
	ies->length = 0;
}

__attribute__((noinline)) int wlanIeBufferFree(struct WlanIeBuffer *ies)
{
	if (ies->buffer != NULL)
		osa_BufferFree(ies->buffer);
	ies->buffer = NULL;
	ies->length = 0;
	return 0;
}

/* Takes a buffer, waiting for one to be free. */
int wlanIeBufferAlloc(struct WlanIeBuffer *ies)
{
	int result = wlanIeBufferAllocOverride(ies);
	OsaBuffer *buffer;

	if (result == 0) {
		if (ies->buffer != NULL)
			wlanIeBufferFree(ies);
		for (;;) {
			buffer = osa_BufferAlloc(wlanIeBufferPool, (uint16_t)wlanIeBufferBlockSize, 0);
			if (buffer != NULL)
				break;
			tx_thread_sleep(1);
		}
		ies->buffer = buffer;
		ies->size = wlanIeBufferBlockSize;
		ies->length = 0;
		return 0;
	}
	return result;
}

/* Sets the pool and the block size the buffers have. */
void wlanIeBufferInit(void)
{
	wlanIeBufferPool = &wlanPacketBufferPool;
	wlanIeBufferBlockSize = 2000;
}
