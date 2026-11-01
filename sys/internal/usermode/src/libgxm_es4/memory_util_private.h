/*
 * SceGxm: device memory the library allocates for itself, mapped for the GPU
 * and reported to the capture tools.
 */

#ifndef GXM_MEMORY_UTIL_PRIVATE_H
#define GXM_MEMORY_UTIL_PRIVATE_H

#include "gxm_internal.h"

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

static inline __attribute__((always_inline)) void gxmFreeGpuBlock(PVRSRV_CLIENT_MEM_INFO *block)
{
	int err;

	if (gxmInitCallbacksMagic == GXM_CALLBACKS_MAGIC)
		gxmInitCallbacks.memoryUnmapped(0, block->pvLinAddr);
	err = SceGpuEs4User_B6CFD213(&gxmGlobals.top, block);
	if (err != 0)
		GXM_DBG_ERROR_AT("memory_util_private.h", 91,
				 "Failed due to error in driver layer (%08X).", err);
}

#endif
