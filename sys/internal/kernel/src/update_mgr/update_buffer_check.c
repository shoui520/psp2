#include <sysmem/sysmem.h>
#include <threadmgr/threadmgr.h>
#include <threadmgr/fast_mutex_object.h>
#include "update_objects.h"

int UpdateUtilCheckBuffer(SceSize size, void *buffer)
{
	UpdateUtilBufferEntry *entry;
	int index, result, bufferResult;

	if (!buffer)
		return (int)0x800f0216;
	result = lockFastMutex((SceKernelFastMutex *)&updateTableMutex);
	if (result)
		return result;
	bufferResult = (int)0x800f0202;
	for (index = 0; index < 4; index++) {
		entry = &updateBufferTable[index];
		if (entry->kernelBuffer == buffer && entry->size == size &&
				entry->allocated == 1) {
			bufferResult = 0;
			continue;
		}
		if (entry->kernelBuffer && !entry->allocated && !entry->inUse) {
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
	return bufferResult;
}
