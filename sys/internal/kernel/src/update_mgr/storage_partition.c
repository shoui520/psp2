#include <iofilemgr/iofilemgr.h>
#include "update_internal.h"

int StorageUtilGetPartitionPath(uint32_t partition, uint8_t active,
		char *path, SceSize size);

SceUID StorageUtileGetPartitionFd(uint32_t partition, uint8_t active)
{
	char path[64];
	int result;

	result = StorageUtilGetPartitionPath(partition, active, path, sizeof(path));
	if (!result)
		result = sceIoOpen(path, 0x4000003, 0);
	return result;
}
