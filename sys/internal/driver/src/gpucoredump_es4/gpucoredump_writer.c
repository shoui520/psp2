/*
 * SceGpuEs4CoreDump: the writer of the dump file (Sony's file name
 * unknown). Segments with a key are DES-encrypted.
 */

#include <scetypes.h>
#include <iofilemgr/iofilemgr.h>
#include <string.h>
#include "gpucoredump.h"

/* the file of the dump: a header, then each segment with its type and size */
#define GPU_COREDUMP_FILE_MAGIC		0x01727a72
#define GPU_COREDUMP_FILE_BODY		0x01304000

static void gpuCoreDumpFlush(GpuCoreDumpWriter *w)
{
	if (sceIoWrite(w->fd, w->buf, w->length) != (int)w->length)
		w->error = 1;
	w->length = 0;
}

static __attribute__((noinline)) void gpuCoreDumpEncryptBlock(GpuCoreDumpWriter *w)
{
	union {
		SceUInt64 value;
		SceUInt8 bytes[8];
	} block;
	SceUInt8 *output;
	unsigned int i;

	if (w->length + sizeof(w->block) > sizeof(w->buf))
		gpuCoreDumpFlush(w);
	output = w->buf + w->length;
	/* The kernel cipher takes the block as a big-endian integer. */
	for (i = 0; i != sizeof(w->block); i++)
		block.bytes[7 - i] = w->block[i] ^ w->iv[i];
	block.value = SceKernelUtilsForDriver_A0D45D25(&w->cipher, block.value);
	for (i = 0; i != sizeof(w->block); i++) {
		output[i] = block.bytes[7 - i];
		w->iv[i] = block.bytes[7 - i];
	}
	w->length += sizeof(w->block);
	w->blockLength = 0;
}

static __attribute__((noinline)) void gpuCoreDumpFinish(GpuCoreDumpWriter *w)
{
	SceSize i = w->blockLength;
	SceUInt8 padding = sizeof(w->block) - i;

	if (i != 0) {
		for (; i < sizeof(w->block); i++)
			w->block[i] = padding;
		gpuCoreDumpEncryptBlock(w);
	}
	w->encrypt = 0;
}

static void gpuCoreDumpAppend(GpuCoreDumpWriter *w, const SceUInt8 *data, SceSize size)
{
	SceSize len;

	for (; size != 0; size -= len) {
		len = sizeof(w->buf) - w->length;
		if (len == 0) {
			gpuCoreDumpFlush(w);
			len = sizeof(w->buf) - w->length;
		}
		if (size <= len)
			len = size;
		memcpy(w->buf + w->length, data, len);
		w->length += len;
		data += len;
	}
}

/* plain, or collected into blocks while a segment is encrypted */
static void gpuCoreDumpWrite(GpuCoreDumpWriter *w, const SceUInt8 *data, SceSize size)
{
	SceSize len;

	if (w->error)
		return;
	if (!w->encrypt) {
		gpuCoreDumpAppend(w, data, size);
		return;
	}
	for (; size != 0; size -= len) {
		len = sizeof(w->block) - w->blockLength;
		if (size < len)
			len = size;
		memcpy(w->block + w->blockLength, data, len);
		w->blockLength += len;
		if (w->blockLength >= sizeof(w->block))
			gpuCoreDumpEncryptBlock(w);
		data += len;
	}
}

int gpuCoreDumpWriteSegments(SceUID fd, const GpuCoreDumpSegment *seg, int count)
{
	GpuCoreDumpWriter w;
	SceUInt32 hdr[2];
	SceSize total = 0;
	int i, k;

	for (i = 0; i != count; i++)
		total += ((seg[i].size + 7) & ~7) + 8;
	hdr[0] = GPU_COREDUMP_FILE_MAGIC;
	hdr[1] = total + 8;
	w.fd = fd;
	w.length = 0;
	w.encrypt = 0;
	w.error = 0;
	gpuCoreDumpAppend(&w, (const SceUInt8 *)hdr, sizeof(hdr));
	hdr[0] = GPU_COREDUMP_FILE_BODY;
	hdr[1] = total;
	gpuCoreDumpWrite(&w, (const SceUInt8 *)hdr, sizeof(hdr));
	for (i = 0; i != count; i++, seg++) {
		hdr[0] = seg->type;
		hdr[1] = (seg->size + 7) & ~7;
		gpuCoreDumpWrite(&w, (const SceUInt8 *)hdr, sizeof(hdr));
		if (seg->iv != NULL) {
			w.blockLength = 0;
			if (SceKernelUtilsForDriver_AE6C45FB(&w.cipher, seg->key) >= 0)
				for (k = 0; k < 8; k++)
					w.iv[k] = seg->iv[k];
			w.encrypt = 1;
		}
		gpuCoreDumpWrite(&w, seg->data, seg->size);
		if (seg->iv != NULL)
			gpuCoreDumpFinish(&w);
	}
	if (w.encrypt)
		gpuCoreDumpFinish(&w);
	gpuCoreDumpFlush(&w);
	w.blockLength = 0;
	w.length = 0;
	w.fd = -1;
	if (w.error)
		return -1;
	return sceIoClose(fd);
}
