#include <post_ss_mgr/zlib.h>
#include <threadmgr/threadmgr.h>
#include "update_objects.h"

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

	permission = sceKernelSetThreadAccessLevel(128);
	if (permission < 0)
		return permission;
	if (!size || !input || !mbr) {
		sceKernelSetThreadAccessLevel(permission);
		return (int)0x800f0216;
	}
	result = UpdateUtilAllocateBuffer(0x40000, &buffer, pid);
	if (result)
		goto finish;
	stream.opaque = (void *)(uintptr_t)heapId;
	stream.zalloc = (SceZlibAlloc)UpdateUtilHeapAllocate;
	stream.zfree = (SceZlibFree)UpdateUtilHeapFree;
	if (SceZlibForDriver_inflateInitNocheck_(&stream, SCE_ZLIB_VERSION, sizeof(stream))) {
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
		inflateResult = inflate(&stream, Z_NO_FLUSH);
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
	SceZlibForDriver_inflateEnd(&stream);
	if (buffer)
		UpdateUtilReleaseBuffer(buffer, pid);
	sceKernelSetThreadAccessLevel(permission);
	return result;
}
