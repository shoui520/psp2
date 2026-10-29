/* IMG perproc.c base; SCE native mapping/backing teardown deltas below. */
#include <gpu/perproc_sce.h>
#include "../handle-types.h"
#include "../gpu-blockheap.h"
#include "../mapped_memory_sce.h"
#include <string.h>
#include <sysmem/debug.h>
#include <threadmgr/threadmgr.h>
#include <sysmem/sysmem.h>
#include <sysmem/uid_guid.h>
#include <processmgr/processmgr.h>
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

/* IMG connect; SCE records the process type and SDK version, and gives every
 * user process a wait event flag, opened in the process as a PUID.
 */
PVRSRV_ERROR PVRSRVPerProcessDataConnect(IMG_UINT32 ui32PID, IMG_UINT32 ui32Flags)
{
	PVRSRV_PER_PROCESS_DATA *psPerProc;
	IMG_HANDLE hBlockAlloc;
	SceKernelProcessInfo info;
	SceUInt32 sdkVersion;
	PVRSRV_ERROR eError;
	int ret;

	if (psHashTab == NULL)
		return PVRSRV_ERROR_INIT_FAILURE;
	psPerProc = (PVRSRV_PER_PROCESS_DATA *)HASH_Retrieve(psHashTab, ui32PID);
	if (psPerProc == NULL) {
		memset(&info, 0, sizeof(info));
		info.size = sizeof(info);
		ret = sceKernelGetProcessInfo(ui32PID, &info);
		if (ret < 0) {
			SCE_DBG_PRINTF(0, 215,
					"%s: Failed to get process info for PID %08x with error %08x\n",
					__func__, ui32PID, ret);
			return PVRSRV_ERROR_INVALID_PARAMS;
		}
		ret = SceProcessmgrForDriver_D141C076(ui32PID, &sdkVersion);
		if (ret < 0) {
			SCE_DBG_PRINTF(0, 221,
					"%s: Failed to get SDK version for PID %08x with error %08x\n",
					__func__, ui32PID, ret);
			return PVRSRV_ERROR_INVALID_PARAMS;
		}
		eError = OSAllocMem_Impl(2, sizeof(*psPerProc), (IMG_VOID **)&psPerProc,
				&hBlockAlloc);
		if (eError != PVRSRV_OK)
			return eError;
		OSMemSet(psPerProc, 0, sizeof(*psPerProc));
		psPerProc->hBlockAlloc = hBlockAlloc;
		if (!HASH_Insert(psHashTab, ui32PID, (IMG_UINTPTR_T)psPerProc)) {
			eError = PVRSRV_ERROR_INSERT_HASH_TABLE_DATA_FAILED;
			goto failure;
		}
		psPerProc->processFlags = info.type;
		psPerProc->ui32PID = ui32PID;
		psPerProc->ui32RefCount = 0;
		psPerProc->sdkVersion = sdkVersion;
		eError = OSPerProcessPrivateDataInit((IMG_HANDLE *)&psPerProc->memoryContext);
		if (eError != PVRSRV_OK)
			goto failure;
		eError = PVRSRVAllocHandle(gpsKernelHandleBase, (IMG_SID *)&psPerProc->hPerProcData,
				psPerProc, 1, 0);
		if (eError != PVRSRV_OK)
			goto failure;
		eError = PVRSRVAllocHandleBase(&psPerProc->psHandleBase);
		if (eError != PVRSRV_OK)
			goto failure;
		eError = OSPerProcessSetHandleOptions(psPerProc->psHandleBase);
		if (eError != PVRSRV_OK)
			goto failure;
		eError = PVRSRVResManConnect(psPerProc, &psPerProc->hResManContext);
		if (eError != PVRSRV_OK)
			goto failure;
		if (psPerProc->ui32PID == SCE_KERNEL_PROCESS_ID) {
			psPerProc->processHandle = -1;
			psPerProc->waitEventFlag = -1;
		} else {
			psPerProc->waitEventFlag = sceKernelCreateEventFlag("SceGpuWaitEventFlag",
					0x1000, 0, NULL);
			if (psPerProc->waitEventFlag < 0) {
				eError = PVRSRV_ERROR_OUT_OF_MEMORY;
				goto failure;
			}
			psPerProc->processHandle = scePUIDOpenByGUID(psPerProc->ui32PID,
					psPerProc->waitEventFlag);
			if (psPerProc->processHandle < 0) {
				sceKernelDeleteEventFlag(psPerProc->waitEventFlag);
				eError = PVRSRV_ERROR_OUT_OF_MEMORY;
				goto failure;
			}
		}
	}
	psPerProc->ui32RefCount++;
	return PVRSRV_OK;

failure:
	FreePerProcessData(psPerProc);
	return eError;
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
