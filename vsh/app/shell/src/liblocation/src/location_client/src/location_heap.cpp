/*
 * The library's own heap: one user memory block made into an mspace by sceLocationInit, unless
 * the system's client supplied its allocator (SceLibLocation_822BBF06). The file's name is ours.
 * SceLibLocationFactory builds it too, for its debug client's heap.
 */
#include <stddef.h>
#include <scetypes.h>
#include <kernel/sysmem/memblock.h>
#include <libkernel/libkernel.h>
#include "location_client.h"

#ifdef LOCATION_HEAP_DEBUG_CLIENT
#define LOCATION_HEAP_BLOCK_NAME	"SceLocationDebugClient"
#else
#define LOCATION_HEAP_BLOCK_NAME	"SceLocationClient"
#endif

static SceUID s_heapBlock = 0;
static void *s_heapMspace = NULL;

int locationHeapCreate(SceSize size)
{
	int ret = SCE_LOCATION_ERROR_NO_MEMORY;

	s_heapBlock = sceKernelAllocMemBlock(LOCATION_HEAP_BLOCK_NAME, SCE_KERNEL_MEMBLOCK_TYPE_USER_RW,
					     size, NULL);
	if (s_heapBlock < 0)
		return ret;

	ret = sceKernelGetMemBlockBase(s_heapBlock, &s_heapMspace);
	if (ret < 0) {
		sceKernelFreeMemBlock(s_heapBlock);
		return ret;
	}

	s_heapMspace = sceClibMspaceCreate(s_heapMspace, size);
	if (s_heapMspace == NULL) {
		sceKernelFreeMemBlock(s_heapBlock);
		s_heapBlock = 0;
		return SCE_LOCATION_ERROR_NO_MEMORY;
	}
	return 0;
}

void locationHeapDestroy()
{
	if (s_heapMspace != NULL) {
		sceClibMspaceDestroy(s_heapMspace);
		s_heapMspace = NULL;
	}
	if (s_heapBlock >= 0) {
		sceKernelFreeMemBlock(s_heapBlock);
		s_heapBlock = 0;
	}
}

void *locationHeapAlloc(SceSize size)
{
	return sceClibMspaceMalloc(s_heapMspace, size);
}

void locationHeapFree(void *pointer)
{
	sceClibMspaceFree(s_heapMspace, pointer);
}
