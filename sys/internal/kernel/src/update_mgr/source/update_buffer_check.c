#include <sce/sysmem.h>
#include <psp2kern/kernel/threadmgr/fast_mutex.h>
#include "update_objects.h"

int UpdateUtilCheckBuffer(SceSize size, void *buffer)
{
	UpdateUtilBufferEntry *entry;
	int result, bufferResult;

	if (!buffer)
		return (int)0x800f0216;
	result = ksceKernelLockFastMutex((SceKernelFastMutex *)&updateTableMutex);
	if (result)
		return result;
	bufferResult = (int)0x800f0202;
	for (entry = updateBufferTable; entry != updateBufferTable + 4; entry++) {
		if (entry->kernelBuffer == buffer && entry->size == size &&
				entry->allocated == 1) {
			bufferResult = 0;
			continue;
		}
		if (entry->kernelBuffer && !entry->allocated && !entry->inUse) {
			ksceKernelFreeMemBlock(entry->kernelMemBlock);
			if (entry->userMemBlock > 0)
				ksceKernelFreeMemBlock(entry->userMemBlock);
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
	ksceKernelUnlockFastMutex((SceKernelFastMutex *)&updateTableMutex);
	return bufferResult;
}
