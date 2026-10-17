/* IMG perproc.c base; SCE native mapping/backing teardown deltas below. */
#include "../perproc_sce.h"
#include "../handle-types.h"
#include "../gpu-blockheap.h"
#include "../mapped_memory_sce.h"
#include <sysmem/debug.h>
#include <psp2kern/kernel/threadmgr.h>

HASH_TABLE *psHashTab;
extern PVRSRV_HANDLE_BASE *gpsKernelHandleBase;
IMG_UINTPTR_T HASH_Remove(HASH_TABLE *pHash, IMG_UINTPTR_T k);
IMG_UINTPTR_T HASH_Retrieve(HASH_TABLE *pHash, IMG_UINTPTR_T k);
HASH_TABLE *HASH_Create(IMG_UINT32 uInitialLen);
IMG_VOID HASH_Delete(HASH_TABLE *pHash);
PVRSRV_ERROR PVRSRVFreeHandleBase(PVRSRV_HANDLE_BASE *psBase);
PVRSRV_ERROR PVRSRVReleaseHandle(PVRSRV_HANDLE_BASE *psBase, IMG_SID hHandle,
		IMG_UINT32 eType);
PVRSRV_ERROR OSFreeMem_Impl(IMG_UINT32 flags, IMG_SIZE_T size,
		IMG_VOID *memory, IMG_HANDLE block);
PVRSRV_ERROR OSPerProcessPrivateDataDeInit(GpuMappedMemoryList *list);
PVRSRV_ERROR PVRSRVPurgeHandles(PVRSRV_HANDLE_BASE *psBase);
PVRSRV_ERROR PVRSRVResManDisconnect(struct ResourceContext *hResManContext,
		IMG_BOOL bKernelContext);
IMG_INT sceGpuWaitModObjRemoveAll(IMG_INT waitEventFlag);
int kscePUIDClose(int pid, int puid);

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
				kscePUIDClose(psPerProc->ui32PID, psPerProc->processHandle);
			if (psPerProc->waitEventFlag > 0)
				ksceKernelDeleteEventFlag(psPerProc->waitEventFlag);
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

	psPerProc = PVRSRVPerProcessData(ksceKernelGetProcessId());
	return psPerProc ? psPerProc->memBlockHeap : NULL;
}
