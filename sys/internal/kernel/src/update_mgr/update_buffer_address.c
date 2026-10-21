#include <threadmgr/threadmgr.h>
#include <threadmgr/fast_mutex_object.h>
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
	for (index = 0; index < 4; index++) {
		entry = &updateBufferTable[index];
		if (entry->kernelBuffer == kernelBuffer && entry->pid == pid && entry->allocated == 1)
			break;
	}
	if (index == 4) {
		result = (int)0x800f0202;
	} else {
		*userBuffer = updateBufferTable[index].userBuffer;
		result = 0;
	}
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
	for (index = 0; index < 4; index++) {
		entry = &updateBufferTable[index];
		if (entry->userBuffer == userBuffer && entry->pid == pid && entry->allocated == 1)
			break;
	}
	if (index == 4) {
		result = (int)0x800f0202;
	} else {
		*kernelBuffer = updateBufferTable[index].kernelBuffer;
		result = 0;
	}
	sceKernelUnlockFastMutex((SceKernelFastMutex *)&updateTableMutex);
	return result;
}
