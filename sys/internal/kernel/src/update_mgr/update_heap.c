#include <sysmem/sysmem.h>
#include "update_internal.h"

void *UpdateUtilHeapAllocate(SceUID heapId, SceSize count, SceSize size)
{
	if (heapId <= 0)
		return NULL;
	return SceSysmemForDriver_49D4DD9B(heapId, count * size, NULL);
}

int UpdateUtilHeapFree(SceUID heapId, void *buffer)
{
	if (heapId > 0)
		return sceKernelFreeHeapMemory(heapId, buffer);
	return heapId;
}
