/* IMG perproc.c base; SCE native mapping/backing teardown deltas below. */
#include <gpu/perproc_sce.h>
#include "../handle-types.h"
#include "../gpu-blockheap.h"
#include "../mapped_memory_sce.h"
#include <sysmem/debug.h>
#include <threadmgr/threadmgr.h>
#include <sysmem/sysmem.h>
#include <gpu/osfunc_sce.h>
#include <gpu/resman_sce.h>

HASH_TABLE *psHashTab;
extern PVRSRV_HANDLE_BASE *gpsKernelHandleBase;
IMG_INT sceGpuWaitModObjRemoveAll(IMG_INT waitEventFlag);

/* IMG FreePerProcessData; SCE omits generic diagnostics/assertions and adds
 * native backing-heap destruction before private mapping-list cleanup.
 */
PVRSRV_ERROR FreePerProcessData(PVRSRV_PER_PROCESS_DATA *psPerProc)
{
	PVRSRV_ERROR eError;

	if (!psPerProc)
		return PVRSRV_ERROR_INVALID_PARAMS;
	HASH_Remove(psHashTab, psPerProc->ui32PID);
	if (psPerProc->psHandleBase) {
		eError = PVRSRVFreeHandleBase(psPerProc->psHandleBase);
		if (eError != PVRSRV_OK)
			return eError;
	}
	if (psPerProc->hPerProcData) {
		eError = PVRSRVReleaseHandle(gpsKernelHandleBase,
				(IMG_SID)psPerProc->hPerProcData, 1);
		if (eError != PVRSRV_OK)
			return eError;
	}
	if (psPerProc->memBlockHeap) {
		DestroyMemBlockHeap(psPerProc->memBlockHeap);
		psPerProc->memBlockHeap = NULL;
	}
	eError = OSPerProcessPrivateDataDeInit(psPerProc->memoryContext);
	if (eError != PVRSRV_OK)
		return eError;
	return OSFreeMem_Impl(2, sizeof(*psPerProc), psPerProc, psPerProc->hBlockAlloc);
}

/* Pristine IMG lookup, with the native32-bit hash key. */
PVRSRV_PER_PROCESS_DATA *PVRSRVPerProcessData(IMG_UINT32 ui32PID)
{
	return (PVRSRV_PER_PROCESS_DATA *)HASH_Retrieve(psHashTab, ui32PID);
}

/* IMG disconnect; SCE removes wait registrations and native process/event
 * handles, omits diagnostics, and returns the global handle purge result.
 */
PVRSRV_ERROR PVRSRVPerProcessDataDisconnect(IMG_UINT32 ui32PID)
{
	PVRSRV_PER_PROCESS_DATA *psPerProc;

	psPerProc = (PVRSRV_PER_PROCESS_DATA *)HASH_Retrieve(psHashTab, ui32PID);
	if (psPerProc) {
		psPerProc->ui32RefCount--;
		if (psPerProc->ui32RefCount == 0) {
			sceGpuWaitModObjRemoveAll(psPerProc->waitEventFlag);
			if (psPerProc->processHandle > 0)
				scePUIDClose(psPerProc->ui32PID, psPerProc->processHandle);
			if (psPerProc->waitEventFlag > 0)
				sceKernelDeleteEventFlag(psPerProc->waitEventFlag);
			PVRSRVResManDisconnect(psPerProc->hResManContext, IMG_FALSE);
			FreePerProcessData(psPerProc);
		}
	}
	return PVRSRVPurgeHandles(gpsKernelHandleBase);
}

/* IMG initialization; SCE omits the generic assertions and diagnostics. */
PVRSRV_ERROR PVRSRVPerProcessDataInit(IMG_VOID)
{
	psHashTab = HASH_Create(32);
	if (psHashTab == NULL)
		return PVRSRV_ERROR_UNABLE_TO_CREATE_HASH_TABLE;
	return PVRSRV_OK;
}

/* Pristine IMG teardown. */
PVRSRV_ERROR PVRSRVPerProcessDataDeInit(IMG_VOID)
{
	if (psHashTab != NULL) {
		HASH_Delete(psHashTab);
		psHashTab = NULL;
	}
	return PVRSRV_OK;
}

/* SCE backing-memory heap accessor. */
GpuMemBlockHeap *GetProcessMemBlockHeap(IMG_VOID)
{
	PVRSRV_PER_PROCESS_DATA *psPerProc;

	psPerProc = PVRSRVPerProcessData(sceKernelGetProcessId());
	return psPerProc ? psPerProc->memBlockHeap : NULL;
}
