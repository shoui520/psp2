/* Descriptive filename; internal memory helpers have no debug source records. */
#include <sce/sysmem.h>
#include <sce/fast_mutex.h>
#include "hid_internal.h"

SceUID hidHeapId;
struct ThreadmgrFastMutex hidMutex;

int ksceKernelInitializeFastMutex(struct ThreadmgrFastMutex *mutex,
				 const char *name, int attr, void *opt);
int ksceKernelLockFastMutex(struct ThreadmgrFastMutex *mutex);
int ksceKernelUnlockFastMutex(struct ThreadmgrFastMutex *mutex);

int hidInitializeMemory(void)
{
	SceKernelHeapCreateOpt options = {
		.size = sizeof(SceKernelHeapCreateOpt),
		.flags = HEAP_HAS_MAX_SIZE,
		.maxSize = 0x10000,
	};
	int result;

	result = ksceKernelCreateHeap("SceHid", 0x7000, &options);
	if (result < 0)
		return result;
	hidHeapId = result;
	result = ksceKernelInitializeFastMutex(&hidMutex, "SceHid", 0x2000, NULL);
	return result & (result >> 31);
}

void *hidAllocateMemory(SceSize size)
{
	if (hidHeapId <= 0)
		return NULL;
	return ksceKernelAllocHeapMemory(hidHeapId, size);
}

int hidFreeMemory(void *memory)
{
	if (memory == NULL)
		return 0;
	return ksceKernelFreeHeapMemory(hidHeapId, memory);
}

/* Same wrappers as krm's driver.c, with this module's canonical mutex. */
int hidLock(void)
{
	return ksceKernelLockFastMutex(&hidMutex);
}

int hidUnlock(void)
{
	return ksceKernelUnlockFastMutex(&hidMutex);
}
