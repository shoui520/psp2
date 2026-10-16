/* Sony env/playstation/osperproc.c: SCE native process mapping port.
 * Pristine IMG Linux implementation remains in the vendor directory.
 */
#include "mapped_memory_sce.h"
#include <stddef.h>

struct _PVRSRV_HANDLE_BASE_;

int ClearMappedMemoryList(GpuMappedMemoryList *list);
int FreeProcessGpuMemory(IMG_VOID *address);

/* SCE creates its mapping list lazily; no Linux private state is required. */
PVRSRV_ERROR OSPerProcessPrivateDataInit(IMG_HANDLE *phOsPrivateData)
{
	return PVRSRV_OK;
}

PVRSRV_ERROR OSPerProcessPrivateDataDeInit(GpuMappedMemoryList *list)
{
	if (list) {
		ClearMappedMemoryList(list);
		FreeProcessGpuMemory(list);
	}
	return PVRSRV_OK;
}

/* SCE has no Linux mmap handle-range or purging options to apply. */
PVRSRV_ERROR OSPerProcessSetHandleOptions(struct _PVRSRV_HANDLE_BASE_ *psHandleBase)
{
	return PVRSRV_OK;
}
