#include <psp2kern/kernel/threadmgr/fast_mutex.h>
#include "update_objects.h"

int UpdateUtilKA2UA(const void *kernelBuffer, SceUID pid, void **userBuffer)
{
	UpdateUtilBufferEntry *entry;
	int result, index;

	if (!userBuffer || !kernelBuffer || pid <= 0)
		return (int)0x800f0216;
	result = lockFastMutex((SceKernelFastMutex *)&updateTableMutex);
	if (result)
		return result;
	index = 0;
	entry = updateBufferTable;
	while (entry->kernelBuffer != kernelBuffer || entry->pid != pid ||
			entry->allocated != 1) {
		index++;
		entry++;
		if (index == 4) {
			result = (int)0x800f0202;
			goto unlock;
		}
	}
	*userBuffer = updateBufferTable[index].userBuffer;
	result = 0;
unlock:
	sceKernelUnlockFastMutex((SceKernelFastMutex *)&updateTableMutex);
	return result;
}

int UpdateUtilUA2KA(const void *userBuffer, SceUID pid, void **kernelBuffer)
{
	UpdateUtilBufferEntry *entry;
	int result, index;

	if (!userBuffer || !kernelBuffer || pid <= 0)
		return (int)0x800f0216;
	result = lockFastMutex((SceKernelFastMutex *)&updateTableMutex);
	if (result)
		return result;
	index = 0;
	entry = updateBufferTable;
	while (entry->userBuffer != userBuffer || entry->pid != pid ||
			entry->allocated != 1) {
		index++;
		entry++;
		if (index == 4) {
			result = (int)0x800f0202;
			goto unlock;
		}
	}
	*kernelBuffer = updateBufferTable[index].kernelBuffer;
	result = 0;
unlock:
	sceKernelUnlockFastMutex((SceKernelFastMutex *)&updateTableMutex);
	return result;
}
