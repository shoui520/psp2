#include <iofilemgr/iofilemgr.h>
#include <psp2kern/kernel/threadmgr/misc.h>
#include "update_internal.h"

int StorageUtilGetPartitionPath(uint32_t partition, uint8_t active,
		char *path, SceSize size);

int StorageUtilReadMbr(SceMbr *mbr)
{
	SceUID fd;
	int permission, result;

	permission = sceKernelSetThreadAccessLevel(128);
	if (permission < 0)
		return permission;
	if (!mbr) {
		sceKernelSetThreadAccessLevel(permission);
		return (int)0x800f0216;
	}
	fd = StorageUtileGetPartitionFd(255, 1);
	if (fd < 0) {
		sceKernelSetThreadAccessLevel(permission);
		updateLastResult = 0x80100ff;
		return fd;
	}
	if (sceIoRead(fd, mbr, sizeof(*mbr)) == sizeof(*mbr)) {
		result = 0;
	} else {
		result = (int)0x800f0205;
		updateLastResult = 0x80100ff;
	}
	sceIoClose(fd);
	sceKernelSetThreadAccessLevel(permission);
	return result;
}

int StorageUtilWriteMbr(const SceMbr *mbr)
{
	char path[64];
	SceUID fd;
	int permission, result;

	permission = sceKernelSetThreadAccessLevel(128);
	if (permission < 0)
		return permission;
	if (!mbr) {
		sceKernelSetThreadAccessLevel(permission);
		return (int)0x800f0216;
	}
	fd = StorageUtileGetPartitionFd(255, 1);
	if (fd < 0) {
		sceKernelSetThreadAccessLevel(permission);
		updateLastResult = 0x80100ff;
		return fd;
	}
	if (sceIoWrite(fd, mbr, sizeof(*mbr)) != sizeof(*mbr)) {
		updateLastResult = 0x80100ff;
		return (int)0x800f0205;
	}
	result = StorageUtilGetPartitionPath(255, 1, path, sizeof(path));
	if (result) {
		updateLastResult = 0x80100ff;
		return result;
	}
	sceIoSync(path, 0);
	sceIoClose(fd);
	sceKernelSetThreadAccessLevel(permission);
	return 0;
}
