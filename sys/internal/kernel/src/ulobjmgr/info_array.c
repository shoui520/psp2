/*
 * The info array: a page-aligned block mapped both into the process (libult
 * writes its object pointers there) and into the kernel.
 */
#include <string.h>
#include <sysmem/sysmem.h>
#include "ulobjmgr_internal.h"

#define ULOBJ_MEMBLOCK_ATTR_MIRROR_MAP	0x01000000	/* map the mirrored block's pages */

static inline __attribute__((always_inline)) void tidyUp(UlobjProcCB *procCB)
{
	int ret;

	SCE_DBG_ASSERT(procCB != NULL, 38);
	if (procCB->driverMemBlock != -1) {
		ret = sceKernelFreeMemBlock(procCB->driverMemBlock);
		SCE_DBG_ASSERT(ret == 0, 44);
		procCB->driverMemBlock = -1;
	}
	if (procCB->userMemBlock != -1) {
		ret = sceKernelFreeMemBlock(procCB->userMemBlock);
		SCE_DBG_ASSERT(ret == 0, 53);
		procCB->userMemBlock = -1;
	}
	(void)ret;
}

int allocateInfoArray(UlobjProcCB *procCB, SceUInt32 numEntries, SceUInt32 sdkVersion,
		      void **pUserBase)
{
	SceKernelAllocMemBlockKernelOpt opt;
	SceKernelAllocMemBlockKernelOpt mirrorOpt;
	SceUID userMemBlock;
	SceSize size;
	int ret;

	SCE_DBG_ASSERT(procCB != NULL, 62);
	SCE_DBG_ASSERT(pUserBase != NULL, 63);
	SCE_DBG_ASSERT(procCB->pid != -1, 64);
	SCE_DBG_ASSERT(procCB->driverMemBlock == -1, 65);
	SCE_DBG_ASSERT(procCB->userMemBlock == -1, 66);

	procCB->sdkVersion = sdkVersion;
	procCB->numEntries = numEntries;

	memset(&opt, 0, sizeof(opt));
	opt.size = sizeof(opt);
	opt.attr = SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_HAS_PID;
	opt.pid = procCB->pid;
	size = (numEntries * sizeof(SceUInt32) + sizeof(UlobjInfoArray) + 0xfff) & ~0xfff;
	userMemBlock = sceKernelAllocMemBlockForKernel("SceUlobjMgrUser",
						       SCE_KERNEL_MEMBLOCK_TYPE_USER_RW, size, &opt);
	procCB->userMemBlock = userMemBlock;
	if (userMemBlock < 0)
		return SCE_KERNEL_ERROR_NO_MEMORY;

	memset(&mirrorOpt, 0, sizeof(mirrorOpt));
	mirrorOpt.size = sizeof(mirrorOpt);
	mirrorOpt.attr = ULOBJ_MEMBLOCK_ATTR_MIRROR_MAP | SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_HAS_PID |
			 SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_HAS_MIRROR_BLOCKID;
	mirrorOpt.mirror_blockid = userMemBlock;
	mirrorOpt.pid = procCB->pid;
	procCB->driverMemBlock = sceKernelAllocMemBlockForKernel("SceUlobjMgrDriver",
							 SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_RW, size,
							 &mirrorOpt);
	if (procCB->driverMemBlock < 0) {
		tidyUp(procCB);
		return SCE_KERNEL_ERROR_NO_MEMORY;
	}

	ret = sceKernelGetMemBlockBase(procCB->driverMemBlock, (void **)&procCB->infoArray);
	if (ret < 0)
		goto error;
	ret = sceKernelGetMemBlockBase(procCB->userMemBlock, pUserBase);
	if (ret < 0)
		goto error;
	return 0;

error:
	tidyUp(procCB);
	return ret;
}

void freeInfoArray(UlobjProcCB *procCB)
{
	SCE_DBG_ASSERT(procCB != NULL, 129);
	SCE_DBG_ASSERT(procCB->pid != -1, 130);
	SCE_DBG_ASSERT(procCB->driverMemBlock != -1, 131);
	SCE_DBG_ASSERT(procCB->userMemBlock != -1, 132);
	tidyUp(procCB);
}

void initializeInfoArray(UlobjProcCB *procCB, SceUInt32 numEntries, SceUInt32 sdkVersion)
{
	UlobjInfoArray *infoArray;
	SceUInt32 i;

	SCE_DBG_ASSERT(procCB != NULL, 143);
	SCE_DBG_ASSERT(procCB->pid != -1, 144);
	SCE_DBG_ASSERT(procCB->driverMemBlock != -1, 145);
	SCE_DBG_ASSERT(procCB->userMemBlock != -1, 146);

	procCB->infoArray->userData[2] = 0;
	if (SceSysrootForDriver_1160FCA1(procCB->pid, &procCB->infoArray->ulobjmgrFlag) != 0)
		procCB->infoArray->ulobjmgrFlag = 0;
	infoArray = procCB->infoArray;
	for (i = 0; i < numEntries - 1; i++)
		infoArray->entry[i] = (i + 1) * 8;
	infoArray->entry[numEntries - 1] = ULOBJ_ENTRY_END;
}
