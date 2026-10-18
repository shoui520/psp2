#include <sysmem/sysmem.h>
#include <psp2kern/kernel/threadmgr/fast_mutex.h>
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
			SceSysmemForDriver_009E1C61(entry->kernelMemBlock);
			if (entry->userMemBlock > 0)
				SceSysmemForDriver_009E1C61(entry->userMemBlock);
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
