#include <sysmem/sysmem.h>
#include <threadmgr/threadmgr.h>
#include <threadmgr/fast_mutex_object.h>
#include "update_objects.h"

int UpdateUtilReleaseAllBuffers(SceUID pid)
{
	UpdateUtilBufferEntry *entry;
	int result;

	result = lockFastMutex((SceKernelFastMutex *)&updateTableMutex);
	if (result)
		return result;
	for (entry = updateBufferTable; entry != updateBufferTable + 4; entry++) {
		if (entry->pid == pid && entry->kernelBuffer &&
				entry->allocated == 1 && !entry->inUse) {
			sceKernelFreeMemBlock(entry->kernelMemBlock);
			if (entry->userMemBlock > 0)
				sceKernelFreeMemBlock(entry->userMemBlock);
			entry->kernelMemBlock = 0;
			entry->userMemBlock = 0;
			entry->kernelBuffer = NULL;
			entry->userBuffer = NULL;
			entry->size = 0;
			entry->allocated = 0;
			entry->inUse = 0;
			entry->pid = 0;
		}
	}
	sceKernelUnlockFastMutex((SceKernelFastMutex *)&updateTableMutex);
	return result;
}
