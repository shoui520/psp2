#include <sysmem/sysmem.h>
#include <threadmgr/threadmgr.h>
#include <threadmgr/fast_mutex_object.h>
#include <string.h>
#include "update_objects.h"
#include <acmgr/acmgr.h>

static const char *const updateUserBufferNames[] = {
	"SceSblUsUser0", "SceSblUsUser1", "SceSblUsUser2", "SceSblUsUser3"
};

static const char *const updateKernelBufferNames[] = {
	"SceSblUsKernel0", "SceSblUsKernel1", "SceSblUsKernel2", "SceSblUsKernel3"
};

int UpdateUtilAllocateBuffer(SceSize size, void **buffer, SceUID pid)
{
	SceKernelAllocMemBlockKernelOpt kernelOption, userOption;
	void *kernelBuffer = NULL, *userBuffer = NULL;
	SceUID kernelMemBlock, userMemBlock;
	UpdateUtilBufferEntry *entry;
	int result, index;

	if (!buffer || !size)
		return (int)0x800f0216;
	result = lockFastMutex((SceKernelFastMutex *)&updateTableMutex);
	if (result)
		return result;
	index = 0;
	entry = updateBufferTable;
	while (entry->kernelBuffer || entry->allocated || entry->inUse) {
		index++;
		entry++;
		if (index == 4)
			goto failed;
	}
	if (SceSblACMgrForKernel_sceSblACMgrIsKernelProgram(pid)) {
		memset(&kernelOption, 0, sizeof(kernelOption));
		kernelOption.size = sizeof(kernelOption);
		kernelOption.alignment = 4096;
		kernelMemBlock = SceSysmemForDriver_C94850C9(updateKernelBufferNames[index],
				SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_ROOT_GAME_RW,
				((size + 4095) >> 12) << 12, &kernelOption);
		if (kernelMemBlock < 0)
			goto failed;
		userMemBlock = SceSysmemForDriver_A841EDDA(kernelMemBlock, &kernelBuffer);
		if (userMemBlock)
			goto failed;
	} else {
		memset(&userOption, 0, sizeof(userOption));
		userOption.size = sizeof(userOption);
		userOption.pid = pid;
		userOption.attr = SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_HAS_PID;
		userOption.alignment = 4096;
		userMemBlock = SceSysmemForDriver_C94850C9(updateUserBufferNames[index],
				SCE_KERNEL_MEMBLOCK_TYPE_USER_MAIN_RW,
				((size + 4095) >> 12) << 12, &userOption);
		if (userMemBlock < 0 || SceSysmemForDriver_A841EDDA(userMemBlock, &userBuffer))
			goto failed;
		memset(&kernelOption, 0, sizeof(kernelOption));
		kernelOption.size = sizeof(kernelOption);
		kernelOption.mirror_blockid = userMemBlock;
		kernelOption.alignment = 4096;
		kernelOption.attr = ALLOC_ATTR_SHARE_PHYPAGE |
				SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_HAS_MIRROR_BLOCKID;
		kernelMemBlock = SceSysmemForDriver_C94850C9(updateKernelBufferNames[index],
				SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_TMP_RW, 0, &kernelOption);
		if (kernelMemBlock < 0 || SceSysmemForDriver_A841EDDA(kernelMemBlock, &kernelBuffer))
			goto failed;
	}
	if (!kernelBuffer)
		goto failed;
	entry = &updateBufferTable[index];
	entry->kernelBuffer = kernelBuffer;
	entry->size = size;
	entry->pid = pid;
	entry->userBuffer = userBuffer;
	entry->allocated = 1;
	entry->inUse = 0;
	*buffer = kernelBuffer;
	entry->kernelMemBlock = kernelMemBlock;
	entry->userMemBlock = userMemBlock;
	result = 0;
	goto unlock;

failed:
	result = (int)0x800f020c;
unlock:
	sceKernelUnlockFastMutex((SceKernelFastMutex *)&updateTableMutex);
	return result;
}

int UpdateUtilReleaseBuffer(void *buffer, SceUID pid)
{
	UpdateUtilBufferEntry *entry;
	int result, freeResult = 0, userFreeResult = 0, kernelFreeResult;

	if (!buffer)
		return (int)0x800f0216;
	result = lockFastMutex((SceKernelFastMutex *)&updateTableMutex);
	if (result)
		return result;
	result = (int)0x800f0202;
	for (entry = updateBufferTable; entry != updateBufferTable + 4; entry++) {
		if (buffer == entry->kernelBuffer && pid == entry->pid &&
				entry->allocated == 1) {
			result = 0;
			entry->allocated = 0;
		}
		if (entry->kernelBuffer && !entry->allocated && !entry->inUse) {
			kernelFreeResult = SceSysmemForDriver_009E1C61(entry->kernelMemBlock);
			if (entry->userMemBlock > 0)
				userFreeResult = SceSysmemForDriver_009E1C61(entry->userMemBlock);
			entry->kernelMemBlock = 0;
			entry->userMemBlock = 0;
			if (userFreeResult < 0 && buffer == entry->kernelBuffer)
				freeResult = userFreeResult;
			if (kernelFreeResult < 0 && buffer == entry->kernelBuffer)
				freeResult = kernelFreeResult;
			entry->kernelBuffer = NULL;
			entry->userBuffer = NULL;
			entry->size = 0;
			entry->allocated = 0;
			entry->inUse = 0;
			entry->pid = 0;
		}
	}
	if (freeResult)
		result = freeResult;
	sceKernelUnlockFastMutex((SceKernelFastMutex *)&updateTableMutex);
	return result;
}
