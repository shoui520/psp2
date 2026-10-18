#include <sysmem/sysmem.h>
#include <iofilemgr/iofilemgr.h>
#include <psp2kern/kernel/sysclib.h>
#include <psp2kern/kernel/threadmgr/misc.h>
#include "update_objects.h"

int StorageUtilGetPartitionPath(uint32_t partition, uint8_t active,
		char *path, SceSize size);
int UpdateUtilUpdateNodeStatus(unsigned int nodeType,
		const UpdateUtilNodeStatus *status);

int UpdateUtilWritePartition(const void *buffer, uint64_t size, uint64_t offset,
		const SceMbr *mbr, uint8_t partition,
		int nodeType, UpdateUtilNodeStatus *status)
{
	SceUID heapId = updateHeapId, fd;
	const uint8_t *cursor = buffer;
	SceAllocOpt option;
	void *readBuffer;
	char path[64];
	uint64_t remaining;
	int64_t fileSize;
	SceSize chunk, rounded;
	int permission, result, written;
	unsigned int error;

	permission = sceKernelSetThreadAccessLevel(128);
	if (permission < 0)
		return permission;
	if (!size || !buffer || !mbr) {
		sceKernelSetThreadAccessLevel(permission);
		return (int)0x800f0216;
	}
	result = StorageUtilGetPartitionPath(partition, 0, path, sizeof(path));
	if (result) {
		updateLastResult = partition | 0x0d010002;
		return result;
	}
	fd = StorageUtileGetPartitionFd(partition, 0);
	if (fd < 0) {
		result = (int)0x800f0205;
		goto restorePermission;
	}
	fileSize = sceIoLseek(fd, 0, SCE_SEEK_END);
	if (fileSize < 0) {
		updateLastResult = partition | 0x0d010003;
		result = (int)0x800f0205;
		goto close;
	}
	option.mmapKind = 0;
	option.mmapBase = NULL;
	option.mmapSize = 0;
	option.size = sizeof(option);
	option.align = 4096;
	readBuffer = SceSysmemForDriver_49D4DD9B(heapId, 0x40000, &option);
	if (!readBuffer) {
		result = (int)0x800f020c;
		goto close;
	}
	if ((uint64_t)fileSize < size) {
		result = (int)0x800f0216;
		goto free;
	}
	remaining = size;
	do {
		if (remaining > 0x40000) {
			chunk = 0x40000;
			rounded = 512;
		} else {
			chunk = remaining;
			rounded = (chunk + 511) >> 9;
		}
		if (sceIoLseek(fd, offset, SCE_SEEK_SET) < 0) {
			error = partition | 3;
			goto ioError;
		}
		rounded <<= 9;
		written = sceIoWrite(fd, cursor, rounded);
		if (written != rounded) {
			error = partition | 5;
			goto ioError;
		}
		sceIoSync(path, 0);
		memset(readBuffer, 255, 0x40000);
		if (sceIoLseek(fd, offset, SCE_SEEK_SET) < 0) {
			error = partition | 3;
			goto ioError;
		}
		if (sceIoRead(fd, readBuffer, written) != written) {
			error = partition | 4;
			goto ioError;
		}
		if (timingsafe_memcmp(cursor, readBuffer, written)) {
			error = partition | 6;
			goto ioError;
		}
		if (status) {
			uint32_t ratio;

			status->progress = 3;
			ratio = remaining / size;
			status->seq_no++;
			status->written_rates = 100 - 100 * ratio;
			UpdateUtilUpdateNodeStatus(nodeType, status);
		}
		remaining -= chunk;
		cursor += chunk;
		offset += rounded;
	} while (remaining);
	result = 0;
	goto free;

ioError:
	result = (int)0x800f0205;
	updateLastResult = error | 0x0d010000;
free:
	sceKernelFreeHeapMemory(heapId, readBuffer);
close:
	sceIoClose(fd);
restorePermission:
	sceKernelSetThreadAccessLevel(permission);
	return result;
}
