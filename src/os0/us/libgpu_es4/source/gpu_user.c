/*
 * SceGpuEs4User module start/stop and its private heap.
 */

#include <psp2/kernel/clib.h>
#include <psp2/kernel/modulemgr.h>
#include <psp2/kernel/sysmem.h>
#include "gpu_es4_user.h"

#define GPU_USER_HEAP_SIZE	0x8000

SceClibMspace g_gpuUserHeap;
SceUID g_gpuUserHeapMemBlock;

/* IMG services.h PVRSRVAllocUserModeMem, from the process heap */
void *PVRSRVAllocUserModeMem(SceSize size)
{
	return sceClibMspaceMalloc(g_gpuUserHeap, size);
}

/* IMG services.h PVRSRVFreeUserModeMem */
void PVRSRVFreeUserModeMem(void *ptr)
{
	sceClibMspaceFree(g_gpuUserHeap, ptr);
}

int _sceGpuUserStart(SceSize args, void *argp)
{
	void *base;
	int ret;

	g_gpuUserHeapMemBlock = sceKernelAllocMemBlock("SceGpuUserHeap", SCE_KERNEL_MEMBLOCK_TYPE_USER_RW,
						       GPU_USER_HEAP_SIZE, NULL);
	if (g_gpuUserHeapMemBlock < 0) {
		sceClibPrintf("%s: Failed to allocate heap storage with error %08x\n", __func__,
			      g_gpuUserHeapMemBlock);
		return SCE_KERNEL_START_FAILED;
	}

	ret = sceKernelGetMemBlockBase(g_gpuUserHeapMemBlock, &base);
	if (ret < 0) {
		sceClibPrintf("%s: Failed to get address of heap storage with error %08x\n", __func__,
			      ret);
		sceKernelFreeMemBlock(g_gpuUserHeapMemBlock);
		return SCE_KERNEL_START_FAILED;
	}

	g_gpuUserHeap = sceClibMspaceCreate(base, GPU_USER_HEAP_SIZE);
	if (g_gpuUserHeap == NULL) {
		sceClibPrintf("%s: Failed to create internal heap\n", __func__);
		sceKernelFreeMemBlock(g_gpuUserHeapMemBlock);
		return SCE_KERNEL_START_FAILED;
	}
	return SCE_KERNEL_START_SUCCESS;
}

/* name unknown */
int module_exit(SceSize args, void *argp)
{
	if (g_gpuUserHeap != NULL)
		sceClibMspaceDestroy(g_gpuUserHeap);
	if (g_gpuUserHeapMemBlock > 0)
		sceKernelFreeMemBlock(g_gpuUserHeapMemBlock);
	PVRSRVRelease();
	return 0;
}
