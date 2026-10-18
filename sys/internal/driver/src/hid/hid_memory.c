/* Descriptive filename; internal memory helpers have no debug source records. */
#include <sysmem/sysmem.h>
#include <threadmgr/fast_mutex.h>
#include "hid_internal.h"
#include <threadmgr/threadmgr.h>

SceUID hidHeapId;
struct ThreadmgrFastMutex hidMutex;

int hidInitializeMemory(void)
{
	SceKernelHeapCreateOpt options = {
		.size = sizeof(SceKernelHeapCreateOpt),
		.flags = HEAP_HAS_MAX_SIZE,
		.maxSize = 0x10000,
	};
	int result;

	result = sceKernelCreateHeap("SceHid", 0x7000, &options);
	if (result < 0)
		return result;
	hidHeapId = result;
	result = sceKernelInitializeFastMutex(&hidMutex, "SceHid", 0x2000, NULL);
	return result & (result >> 31);
}

void *hidAllocateMemory(SceSize size)
{
	if (hidHeapId <= 0)
		return NULL;
	return sceKernelAllocHeapMemory(hidHeapId, size);
}

int hidFreeMemory(void *memory)
{
	if (memory == NULL)
		return 0;
	return sceKernelFreeHeapMemory(hidHeapId, memory);
}

/* Same wrappers as krm's driver.c, with this module's canonical mutex. */
int hidLock(void)
{
	return lockFastMutex(&hidMutex);
}

int hidUnlock(void)
{
	return sceKernelUnlockFastMutex(&hidMutex);
}
