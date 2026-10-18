#include <iofilemgr/iofilemgr.h>
#include <threadmgr/threadmgr.h>
#include <string.h>
#include "update_internal.h"

int UpdateUtilIoRead(SceUID fd, int64_t offset, uint64_t size, void *buffer)
{
	void *readBuffer = NULL;
	uint8_t *readBase;
	uint64_t remaining, position;
	int64_t readOffset;
	SceSize chunk;
	int result, allocated;

	if (offset & 511) {
		remaining = (offset & 511) + size;
		if (remaining > UINT32_MAX)
			return (int)0x800f0216;
		result = UpdateUtilAllocateBuffer(remaining, &readBuffer,
				sceKernelGetProcessId());
		if (result)
			return result;
		allocated = 1;
		readOffset = offset - offset % 512;
	} else {
		remaining = size;
		readBuffer = buffer;
		allocated = 0;
		readOffset = offset;
	}
	if (sceIoLseek(fd, readOffset, SCE_SEEK_SET) < 0) {
		result = (int)0x800f0205;
		goto release;
	}
	readBase = readBuffer;
	position = 0;
	while (remaining) {
		chunk = remaining > 0x40000 ? 0x40000 : remaining;
		if (sceIoRead(fd, readBase + position, chunk) != chunk) {
			result = (int)0x800f0205;
			goto release;
		}
		remaining -= chunk;
		position += chunk;
	}
	result = 0;
release:
	if (allocated) {
		void *releaseBuffer = readBuffer;

		if (!result)
			memcpy(buffer, (uint8_t *)releaseBuffer + offset % 512, size);
		if (releaseBuffer)
			UpdateUtilReleaseBuffer(releaseBuffer, sceKernelGetProcessId());
	}
	return result;
}
