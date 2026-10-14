#include <psp2kern/kernel/sysmem.h>
#include "update_internal.h"

void *UpdateUtilHeapAllocate(SceUID heapId, SceSize count, SceSize size)
{
	if (heapId <= 0)
		return NULL;
	return ksceKernelAllocHeapMemoryWithOption(heapId, count * size, NULL);
}

int UpdateUtilHeapFree(SceUID heapId, void *buffer)
{
	if (heapId > 0)
		return ksceKernelFreeHeapMemory(heapId, buffer);
	return heapId;
}
