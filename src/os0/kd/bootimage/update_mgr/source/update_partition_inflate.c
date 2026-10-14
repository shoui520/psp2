#include <sce/zlib.h>
#include <psp2kern/kernel/threadmgr/misc.h>
#include "update_objects.h"

int SceZlibForDriver_B03E109B(SceZlibStream *stream, const char *version, int size);
int SceZlibForDriver_D4A85178(SceZlibStream *stream, int flush);
int SceZlibForDriver_9030BAE4(SceZlibStream *stream);
void *UpdateUtilHeapAllocate(SceUID heapId, SceSize count, SceSize size);
int UpdateUtilHeapFree(SceUID heapId, void *buffer);
int UpdateUtilUpdateNodeStatus(unsigned int nodeType,
		const UpdateUtilNodeStatus *status);

int UpdateUtilInflateWritePartition(const void *input, uint64_t size,
		uint64_t offset, const SceMbr *mbr, uint8_t partition,
		unsigned int nodeType, UpdateUtilNodeStatus *status, SceUID pid)
{
	SceUID heapId = updateHeapId;
	SceZlibStream stream;
	void *buffer = NULL;
	uint64_t remaining;
	SceSize rounded;
	unsigned int inflateResult;
	int result, permission;

	permission = ksceKernelSetPermission(128);
	if (permission < 0)
		return permission;
	if (!size || !input || !mbr) {
		ksceKernelSetPermission(permission);
		return (int)0x800f0216;
	}
	result = UpdateUtilAllocateBuffer(0x40000, &buffer, pid);
	if (result)
		goto finish;
	stream.opaque = (void *)(uintptr_t)heapId;
	stream.zalloc = (SceZlibAlloc)UpdateUtilHeapAllocate;
	stream.zfree = (SceZlibFree)UpdateUtilHeapFree;
	if (SceZlibForDriver_B03E109B(&stream, SCE_ZLIB_VERSION, sizeof(stream))) {
		result = (int)0x800f0216;
		goto finish;
	}
	stream.next_in = input;
	stream.avail_in = size;
	stream.total_out = 0;
	stream.next_out = buffer;
	stream.avail_out = 0x40000;
	remaining = size;
	for (;;) {
		inflateResult = SceZlibForDriver_D4A85178(&stream, Z_NO_FLUSH);
		if (inflateResult > Z_STREAM_END) {
			result = (int)0x800f0205;
			goto finish;
		}
		if (!stream.avail_out || !stream.avail_in) {
			rounded = ((0x40000 - stream.avail_out + 511) >> 9) << 9;
			result = UpdateUtilWritePartition(buffer, rounded, offset, mbr,
					partition, nodeType, NULL);
			if (result)
				goto finish;
			rounded = ((0x40000 - stream.avail_out + 511) >> 9) << 9;
			stream.avail_out = 0x40000;
			stream.next_out = buffer;
			offset += rounded;
			remaining -= rounded;
			if (status) {
				uint32_t ratio;

				status->progress = 3;
				ratio = remaining / size;
				status->seq_no++;
				status->written_rates = 100 - 100 * ratio;
				UpdateUtilUpdateNodeStatus(nodeType, status);
			}
		}
		if (inflateResult == Z_STREAM_END) {
			result = 0;
			goto finish;
		}
	}
finish:
	SceZlibForDriver_9030BAE4(&stream);
	if (buffer)
		UpdateUtilReleaseBuffer(buffer, pid);
	ksceKernelSetPermission(permission);
	return result;
}
