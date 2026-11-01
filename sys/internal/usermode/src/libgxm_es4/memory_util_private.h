/*
 * SceGxm: device memory the library allocates for itself, mapped for the GPU
 * and reported to the capture tools.
 */

#ifndef GXM_MEMORY_UTIL_PRIVATE_H
#define GXM_MEMORY_UTIL_PRIVATE_H

#include "gxm_internal.h"

/* the size and mapping of a memory block the caller gave by its UID */
static inline __attribute__((always_inline)) int gxmGetMemBlockInfo(SceUID memBlock,
								    SceKernelMemBlockInfo *info)
{
	void *base;

	if (sceKernelGetMemBlockBase(memBlock, &base) != 0) {
		GXM_DBG_ERROR_AT("memory_util_private.h", 29,
				 "Failed to find base address for memblock id 0x%08x", memBlock);
		return SCE_GXM_ERROR_INVALID_VALUE;
	}
	info->size = sizeof(*info);
	if (sceKernelGetMemBlockInfoByAddr(base, info) != 0) {
		GXM_DBG_ERROR_AT("memory_util_private.h", 38,
				 "Failed to find memblock info for memblock id 0x%08x", memBlock);
		return SCE_GXM_ERROR_INVALID_VALUE;
	}
	return 0;
}

static inline __attribute__((always_inline)) int gxmAllocGpuBlock(SceUInt32 heap, SceUInt32 attr, SceSize size, SceSize alignment,
			    PVRSRV_CLIENT_MEM_INFO **block)
{
	int err;

	err = SceGpuEs4User_071E62F0(&gxmGlobals.top, heap, attr, size, alignment,
				     gxmGlobals.top.hMemBlockProcRef, block);
	if (err != 0) {
		GXM_DBG_ERROR_AT("memory_util_private.h", 62,
				 "Failed due to error in driver layer (%08X).", err);
		return SCE_GXM_ERROR_DRIVER;
	}
	if (gxmInitCallbacksMagic == GXM_CALLBACKS_MAGIC)
		gxmInitCallbacks.memoryMapped(0, (*block)->pvLinAddr, (*block)->sDevVAddr, (*block)->uAllocSize);
	return 0;
}

static inline __attribute__((always_inline)) int gxmFreeGpuBlock(PVRSRV_CLIENT_MEM_INFO *block)
{
	int err;

	if (gxmInitCallbacksMagic == GXM_CALLBACKS_MAGIC)
		gxmInitCallbacks.memoryUnmapped(0, block->pvLinAddr);
	err = SceGpuEs4User_B6CFD213(&gxmGlobals.top, block);
	if (err != 0) {
		GXM_DBG_ERROR_AT("memory_util_private.h", 91,
				 "Failed due to error in driver layer (%08X).", err);
		return SCE_GXM_ERROR_DRIVER;
	}
	return 0;
}

#endif
