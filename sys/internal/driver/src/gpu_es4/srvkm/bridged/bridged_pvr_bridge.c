/* IMG bridge core with SCE kernel-port changes, marked at each body.
 * Pristine upstream remains in contrib/img-sgx-ddk-1.7.862890.
 */
#include <gpu/sce_ddk_port.h>
#include <gpu/device_sce.h>
#include <gpu/perproc_sce.h>
#include "../sync_sce.h"
#include "../mapped_memory_sce.h"
#include <gpu/gpu-mmu.h>
#include "../gpu-dump.h"
#include <gpu/osfunc_sce.h>
#include "../bridge_sce.h"
#include <sysmem/debug.h>
#include <string.h>
#include <threadmgr/threadmgr.h>
#include <gpu/memory_sce.h>
#include <gpu/power_sce.h>
#include "../handle-types.h"
#include <gpu/resman_sce.h>

/* IMG shared-heap classification table, zero-initialized in both TOOL originals. */
static IMG_BOOL abSharedDeviceMemHeap[32];

/* SCE strips the generic bridge size and diagnostic counters from each entry. */
typedef struct PVRSRV_BRIDGE_DISPATCH_TABLE_ENTRY {
	BridgeWrapperFunction pfFunction;
} PVRSRV_BRIDGE_DISPATCH_TABLE_ENTRY;

PVRSRV_BRIDGE_DISPATCH_TABLE_ENTRY g_BridgeDispatchTable[148];
/* SCE shares this state with bridge-table initialization. */
IMG_UINT32 ui32PrevIndex = ~0U;

typedef struct PVRSRV_BRIDGE_OUT_CONNECT_SERVICES {
	PVRSRV_ERROR eError;
	IMG_HANDLE hKernelServices;
} PVRSRV_BRIDGE_OUT_CONNECT_SERVICES;

/* Pristine IMG bridge with PDUMP and command assertion disabled by SCE. */
IMG_INT PVRSRVConnectBW(IMG_UINT32 ui32BridgeID, IMG_VOID *psConnectServicesIN,
		PVRSRV_BRIDGE_OUT_CONNECT_SERVICES *psConnectServicesOUT,
		PVRSRV_PER_PROCESS_DATA *psPerProc)
{
	psConnectServicesOUT->hKernelServices = psPerProc->hPerProcData;
	psConnectServicesOUT->eError = PVRSRV_OK;
	return 0;
}

/* Pristine IMG disconnect bridge with the SCE command assertion disabled. */
IMG_INT PVRSRVDisconnectBW(IMG_UINT32 ui32BridgeID, IMG_VOID *psBridgeIn,
		PVRSRV_BRIDGE_RETURN *psRetOUT, PVRSRV_PER_PROCESS_DATA *psPerProc)
{
	psRetOUT->eError = PVRSRV_OK;
	return 0;
}

/* SCE removes upstream diagnostics; the unsupported ioctl result remains. */
int DummyBW(IMG_UINT32 ui32BridgeID, IMG_VOID *psBridgeIn,
		IMG_VOID *psBridgeOut, PVRSRV_PER_PROCESS_DATA *psPerProc)
{
	return -25;
}

/* IMG DoQuerySyncOpsSatisfied, with PDUMP disabled in the SCE port. */
PVRSRV_ERROR DoQuerySyncOpsSatisfied(PVRSRV_KERNEL_SYNC_INFO *psKernelSyncInfo,
		IMG_UINT32 ui32ReadOpsPendingSnapShot,
		IMG_UINT32 ui32WriteOpsPendingSnapShot)
{
	IMG_UINT32 ui32WriteOpsPending;
	IMG_UINT32 ui32ReadOpsPending;

	if (!psKernelSyncInfo)
		return PVRSRV_ERROR_INVALID_PARAMS;
	ui32WriteOpsPending = psKernelSyncInfo->psSyncData->ui32WriteOpsPending;
	ui32ReadOpsPending = psKernelSyncInfo->psSyncData->ui32ReadOpsPending;
	if (ui32WriteOpsPending - ui32WriteOpsPendingSnapShot >=
			ui32WriteOpsPending - psKernelSyncInfo->psSyncData->ui32WriteOpsComplete &&
			ui32ReadOpsPending - ui32ReadOpsPendingSnapShot >=
			ui32ReadOpsPending - psKernelSyncInfo->psSyncData->ui32ReadOpsComplete)
		return PVRSRV_OK;
	return PVRSRV_ERROR_RETRY;
}

/* SCE extends IMG DoModifyCompleteSyncOps with an object index. */
PVRSRV_ERROR DoModifyCompleteSyncOps(MODIFY_SYNC_OP_INFO *psModSyncOpInfo,
		IMG_UINT32 ui32SyncIndex)
{
	PVRSRV_KERNEL_SYNC_INFO *psKernelSyncInfo;

	psKernelSyncInfo = psModSyncOpInfo->apsKernelSyncInfo[ui32SyncIndex];
	if (!psKernelSyncInfo)
		return PVRSRV_ERROR_INVALID_PARAMS;
	if (psModSyncOpInfo->aui32WriteOpsPendingSnapShot[ui32SyncIndex] !=
			psKernelSyncInfo->psSyncData->ui32WriteOpsComplete ||
			psModSyncOpInfo->aui32ReadOpsPendingSnapShot[ui32SyncIndex] !=
			psKernelSyncInfo->psSyncData->ui32ReadOpsComplete)
		return PVRSRV_ERROR_BAD_SYNC_STATE;
	if (psModSyncOpInfo->ui32ModifyFlags & 1)
		psKernelSyncInfo->psSyncData->ui32WriteOpsComplete++;
	if (psModSyncOpInfo->ui32ModifyFlags & 2)
		psKernelSyncInfo->psSyncData->ui32ReadOpsComplete++;
	return PVRSRV_OK;
}

/* IMG resource callback; SCE retains the two-argument resman ABI. */
PVRSRV_ERROR FreeSyncInfoCallback(IMG_VOID *pvParam, IMG_UINT32 ui32Param,
		IMG_BOOL bForceCleanup)
{
	return PVRSRVFreeSyncInfoKM(pvParam);
}

typedef struct PVRSRV_BRIDGE_IN_GET_MAPPED_MEMORY_INFO {
	IMG_UINT32 ui32BridgeFlags;
	IMG_VOID *cpuAddress;
} PVRSRV_BRIDGE_IN_GET_MAPPED_MEMORY_INFO;

typedef struct PVRSRV_BRIDGE_OUT_GET_MAPPED_MEMORY_INFO {
	PVRSRV_ERROR eError;
	IMG_VOID *cpuAddress;
	IMG_SIZE_T size;
	IMG_UINT32 mmuFlags;
} PVRSRV_BRIDGE_OUT_GET_MAPPED_MEMORY_INFO;

/* SCE query returns the original mapped range and its GPU MMU flags. */
IMG_INT PVRSRVGetMappedMemoryInfoBW(IMG_UINT32 ui32BridgeID,
		PVRSRV_BRIDGE_IN_GET_MAPPED_MEMORY_INFO *psBridgeIn,
		PVRSRV_BRIDGE_OUT_GET_MAPPED_MEMORY_INFO *psBridgeOut,
		PVRSRV_PER_PROCESS_DATA *psPerProc)
{
	GpuMappedMemory info;

	psBridgeOut->eError = PVRSRV_ERROR_INVALID_PARAMS;
	if (psPerProc->memoryContext &&
			QueryMappedMemory(psPerProc->memoryContext,
				psBridgeIn->cpuAddress, &info) == PVRSRV_OK) {
		psBridgeOut->eError = PVRSRV_OK;
		psBridgeOut->cpuAddress = info.cpuAddress;
		psBridgeOut->size = info.size;
		psBridgeOut->mmuFlags = info.mmuFlags;
	}
	return 0;
}

typedef struct PVRSRV_BRIDGE_IN_CHECK_GPU_MAPPING {
	IMG_UINT32 ui32BridgeFlags;
	IMG_SID hDevMemContext;
	IMG_DEV_VIRTADDR deviceAddress;
	IMG_SIZE_T size;
	IMG_UINT32 access;
} PVRSRV_BRIDGE_IN_CHECK_GPU_MAPPING;

IMG_INT CheckGpuMmuMapping(GpuMmuContext *context, IMG_UINT32 address,
		IMG_SIZE_T size, IMG_UINT32 flags);

/* SCE translates bridge access modes into GPU page-table permission bits. */
IMG_INT PVRSRVCheckGpuMappingBW(IMG_UINT32 ui32BridgeID,
		PVRSRV_BRIDGE_IN_CHECK_GPU_MAPPING *psBridgeIn,
		PVRSRV_BRIDGE_RETURN *psBridgeOut,
		PVRSRV_PER_PROCESS_DATA *psPerProc)
{
	GpuMemoryContext *context;
	IMG_UINT32 flags;
	IMG_INT result;

	if (psBridgeIn->access > 3) {
		psBridgeOut->eError = PVRSRV_ERROR_INVALID_PARAMS;
		return 0;
	}
	if (psBridgeIn->access == 1)
		flags = 4;
	else if (psBridgeIn->access == 2)
		flags = 2;
	else
		flags = 0;
	psBridgeOut->eError = PVRSRVLookupHandle(psPerProc->psHandleBase,
			(IMG_VOID **)&context, psBridgeIn->hDevMemContext, 3);
	if (psBridgeOut->eError != PVRSRV_OK)
		return 0;
	result = CheckGpuMmuMapping(&context->mmu,
			psBridgeIn->deviceAddress.uiAddr, psBridgeIn->size, flags);
	if (result >= (IMG_INT)0x804c0002 && result <= (IMG_INT)0x804c0003)
		psBridgeOut->eError = PVRSRV_ERROR_MAPPING_NOT_FOUND;
	else if (!result)
		psBridgeOut->eError = PVRSRV_OK;
	else
		psBridgeOut->eError = PVRSRV_ERROR_INVALID_PARAMS;
	return 0;
}

typedef struct PVRSRV_BRIDGE_IN_UNREGISTER_MEMBLOCK {
	IMG_UINT32 ui32BridgeFlags;
	IMG_INT32 memBlockUid;
} PVRSRV_BRIDGE_IN_UNREGISTER_MEMBLOCK;

/* SCE memory-block bridge. */
IMG_INT PVRSRVUnregisterMemBlockBW(IMG_UINT32 ui32BridgeID,
		PVRSRV_BRIDGE_IN_UNREGISTER_MEMBLOCK *psBridgeIn,
		PVRSRV_BRIDGE_RETURN *psBridgeOut,
		PVRSRV_PER_PROCESS_DATA *psPerProc)
{
	psBridgeOut->eError = PVRSRVUnregisterMemBlockKM(psPerProc,
			psBridgeIn->memBlockUid);
	return 0;
}

typedef struct PVRSRV_BRIDGE_IN_REGISTER_MEMBLOCK {
	IMG_UINT32 ui32BridgeFlags;
	IMG_INT32 memBlockUid;
	IMG_UINT32 blockGroupId;
	IMG_BOOL restoreMapping;
} PVRSRV_BRIDGE_IN_REGISTER_MEMBLOCK;

typedef struct PVRSRV_BRIDGE_OUT_REGISTER_MEMBLOCK {
	PVRSRV_ERROR eError;
	IMG_UINT32 blockGroupId;
} PVRSRV_BRIDGE_OUT_REGISTER_MEMBLOCK;

/* SCE reserves block group 1. The requested group is also an output. */
IMG_INT PVRSRVRegisterMemBlockBW(IMG_UINT32 ui32BridgeID,
		PVRSRV_BRIDGE_IN_REGISTER_MEMBLOCK *psBridgeIn,
		PVRSRV_BRIDGE_OUT_REGISTER_MEMBLOCK *psBridgeOut,
		PVRSRV_PER_PROCESS_DATA *psPerProc)
{
	if (psBridgeIn->blockGroupId == 1) {
		psBridgeOut->eError = PVRSRV_ERROR_INVALID_PARAMS;
	} else {
		psBridgeOut->blockGroupId = psBridgeIn->blockGroupId;
		psBridgeOut->eError = PVRSRVRegisterMemBlockKM(psPerProc,
				psBridgeIn->memBlockUid, &psBridgeOut->blockGroupId,
				psBridgeIn->restoreMapping);
	}
	return 0;
}

/* SCE cleanup releases the shared object and its sync-info reference. */
PVRSRV_ERROR DeleteSharedSyncInfoCallback(IMG_VOID *pvParam,
		IMG_UINT32 ui32SharedKey,
		IMG_BOOL bForceCleanup)
{
	PVRSRV_KERNEL_SYNC_INFO *psSyncInfo = pvParam;

	sceGpuSharedSyncObjectDelete(ui32SharedKey);
	if (psSyncInfo)
		psSyncInfo->ui32RefCount--;
	return PVRSRV_OK;
}

typedef struct PVRSRV_BRIDGE_IN_DELETE_SHARED_SYNCINFO {
	IMG_UINT32 ui32BridgeFlags;
	IMG_UINT32 ui32SharedKey;
} PVRSRV_BRIDGE_IN_DELETE_SHARED_SYNCINFO;

extern struct _PVRSRV_HANDLE_BASE_ *gpsKernelHandleBase;

#if FW_DEBUG
static const SceKernelDebugInfo dbg_PVRSRVDeleteSharedSyncInfoBW_L5693 = {
	0x1399193b, 0x88551b47, 0xf22100aa,
	"bridged/bridged_pvr_bridge.c", 5693, "PVRSRVDeleteSharedSyncInfoBW"
};
static const SceKernelDebugInfo dbg_PVRSRVDeleteSharedSyncInfoBW_L5700 = {
	0x1399193b, 0x73794c5c, 0xf22100aa,
	"bridged/bridged_pvr_bridge.c", 5700, "PVRSRVDeleteSharedSyncInfoBW"
};
static const SceKernelDebugInfo dbg_PVRSRVDeleteSharedSyncInfoBW_L5707 = {
	0x1399193b, 0x5bf5687d, 0xf22100aa,
	"bridged/bridged_pvr_bridge.c", 5707, "PVRSRVDeleteSharedSyncInfoBW"
};
static const SceKernelDebugInfo dbg_PVRSRVDeleteSharedSyncInfoBW_L5713 = {
	0x1399193b, 0x0c88d983, 0xf22100aa,
	"bridged/bridged_pvr_bridge.c", 5713, "PVRSRVDeleteSharedSyncInfoBW"
};
static const SceKernelDebugInfo dbg_PVRSRVDeleteSharedSyncInfoBW_L5720 = {
	0x1399193b, 0x2e5ecc8c, 0xf22100aa,
	"bridged/bridged_pvr_bridge.c", 5720, "PVRSRVDeleteSharedSyncInfoBW"
};
#endif

/* SCE shared-sync deletion preserves separate object and handle lifetimes. */
IMG_INT PVRSRVDeleteSharedSyncInfoBW(IMG_UINT32 ui32BridgeID,
		PVRSRV_BRIDGE_IN_DELETE_SHARED_SYNCINFO *psBridgeIn,
		PVRSRV_BRIDGE_RETURN *psBridgeOut, PVRSRV_PER_PROCESS_DATA *psPerProc)
{
	PVRSRV_KERNEL_SYNC_INFO *psSyncInfo;
	IMG_SID hHandle;
	IMG_INT result;

	psBridgeOut->eError = PVRSRV_OK;
	result = sceGpuSharedSyncObjectGetHandle(psBridgeIn->ui32SharedKey, &hHandle);
	if (result < 0) {
#if FW_DEBUG
		SceDebugForDriver_D9703808(1, 0,
			&dbg_PVRSRVDeleteSharedSyncInfoBW_L5693,
			"%s: Error %08x trying to get sync object handle for key %08x!\n",
			__func__, result, psBridgeIn->ui32SharedKey);
#endif
		psBridgeOut->eError = PVRSRV_ERROR_INVALID_PARAMS;
		goto out;
	}
	psBridgeOut->eError = PVRSRVLookupHandle(gpsKernelHandleBase,
			(IMG_VOID **)&psSyncInfo, hHandle, 6);
	if (psBridgeOut->eError != PVRSRV_OK) {
#if FW_DEBUG
		SceDebugForDriver_D9703808(1, 0,
			&dbg_PVRSRVDeleteSharedSyncInfoBW_L5700,
			"%s: Error %08x looking up original sync object for key %08x!\n",
			__func__, psBridgeOut->eError, psBridgeIn->ui32SharedKey);
#endif
		goto out;
	}
	if (psSyncInfo->hSharedResItem) {
		psBridgeOut->eError = ResManFreeResByPtr(psSyncInfo->hResItem, IMG_FALSE);
		if (psBridgeOut->eError != PVRSRV_OK) {
#if FW_DEBUG
			SceDebugForDriver_D9703808(1, 0,
				&dbg_PVRSRVDeleteSharedSyncInfoBW_L5707,
				"%s: Error %08x deleting original sync object for key %08x!\n",
				__func__, psBridgeOut->eError, psBridgeIn->ui32SharedKey);
#endif
			goto out;
		}
	} else {
		result = sceGpuSharedSyncObjectDelete(psBridgeIn->ui32SharedKey);
		if (result < 0) {
#if FW_DEBUG
			SceDebugForDriver_D9703808(1, 0,
				&dbg_PVRSRVDeleteSharedSyncInfoBW_L5713,
				"%s: Error %08x trying to delete shared sync object with key %08x!\n",
				__func__, result, psBridgeIn->ui32SharedKey);
#endif
			psBridgeOut->eError = PVRSRV_ERROR_INVALID_PARAMS;
			goto out;
		}
	}
	if (psSyncInfo->ui32RefCount)
		psSyncInfo->ui32RefCount--;
	else {
#if FW_DEBUG
		SceDebugForDriver_D9703808(1, 0,
			&dbg_PVRSRVDeleteSharedSyncInfoBW_L5720,
			"%s: Sync object with key %08x has a zero refcount!\n",
			__func__, psBridgeIn->ui32SharedKey);
#endif
		psBridgeOut->eError = PVRSRV_ERROR_INVALID_PARAMS;
	}
	PVRSRVReleaseHandle(gpsKernelHandleBase, hHandle, 6);
out:
	return 0;
}

typedef struct PVRSRV_BRIDGE_IN_FREE_SYNC_INFO {
	IMG_UINT32 ui32BridgeFlags;
	IMG_SID hKernelSyncInfo;
} PVRSRV_BRIDGE_IN_FREE_SYNC_INFO;

/* SCE releases the handle before resman and decrements the process quota. */
IMG_INT PVRSRVFreeSyncInfoBW(IMG_UINT32 ui32BridgeID,
		PVRSRV_BRIDGE_IN_FREE_SYNC_INFO *psBridgeIn,
		PVRSRV_BRIDGE_RETURN *psBridgeOut, PVRSRV_PER_PROCESS_DATA *psPerProc)
{
	PVRSRV_KERNEL_SYNC_INFO *psSyncInfo;
	PVRSRV_ERROR eError;

	psBridgeOut->eError = PVRSRV_OK;
	eError = PVRSRVLookupHandle(psPerProc->psHandleBase,
			(IMG_VOID **)&psSyncInfo, psBridgeIn->hKernelSyncInfo, 6);
	if (eError != PVRSRV_OK) {
		psBridgeOut->eError = eError;
		return 0;
	}
	eError = PVRSRVReleaseHandle(psPerProc->psHandleBase,
			psBridgeIn->hKernelSyncInfo, 6);
	if (eError != PVRSRV_OK) {
		psBridgeOut->eError = eError;
		return 0;
	}
	eError = ResManFreeResByPtr(psSyncInfo->hResItem, IMG_FALSE);
	if (eError != PVRSRV_OK)
		psBridgeOut->eError = eError;
	else
		psPerProc->numSyncObjects--;
	return 0;
}

typedef struct PVRSRV_BRIDGE_IN_CLOSE_SHARED_SYNCINFO {
	IMG_UINT32 ui32BridgeFlags;
	IMG_UINT32 ui32SharedKey;
	IMG_SID hKernelSyncInfo;
} PVRSRV_BRIDGE_IN_CLOSE_SHARED_SYNCINFO;

#if FW_DEBUG
static const SceKernelDebugInfo dbg_PVRSRVCloseSharedSyncInfoBW_L5632 = {
	0x1399193b, 0x5d0525ef, 0xa2318864,
	"bridged/bridged_pvr_bridge.c", 5632, "PVRSRVCloseSharedSyncInfoBW"
};
static const SceKernelDebugInfo dbg_PVRSRVCloseSharedSyncInfoBW_L5639 = {
	0x1399193b, 0x527909df, 0xa2318864,
	"bridged/bridged_pvr_bridge.c", 5639, "PVRSRVCloseSharedSyncInfoBW"
};
static const SceKernelDebugInfo dbg_PVRSRVCloseSharedSyncInfoBW_L5645 = {
	0x1399193b, 0x1b701faf, 0xa2318864,
	"bridged/bridged_pvr_bridge.c", 5645, "PVRSRVCloseSharedSyncInfoBW"
};
static const SceKernelDebugInfo dbg_PVRSRVCloseSharedSyncInfoBW_L5650 = {
	0x1399193b, 0x77703551, 0xa2318864,
	"bridged/bridged_pvr_bridge.c", 5650, "PVRSRVCloseSharedSyncInfoBW"
};
static const SceKernelDebugInfo dbg_PVRSRVCloseSharedSyncInfoBW_L5654 = {
	0x1399193b, 0x3829ca11, 0xa2318864,
	"bridged/bridged_pvr_bridge.c", 5654, "PVRSRVCloseSharedSyncInfoBW"
};
static const SceKernelDebugInfo dbg_PVRSRVCloseSharedSyncInfoBW_L5661 = {
	0x1399193b, 0xb2019b2d, 0xa2318864,
	"bridged/bridged_pvr_bridge.c", 5661, "PVRSRVCloseSharedSyncInfoBW"
};
#endif

/* SCE closes the shared reference before releasing the process handle. */
IMG_INT PVRSRVCloseSharedSyncInfoBW(IMG_UINT32 ui32BridgeID,
		PVRSRV_BRIDGE_IN_CLOSE_SHARED_SYNCINFO *psBridgeIn,
		PVRSRV_BRIDGE_RETURN *psBridgeOut, PVRSRV_PER_PROCESS_DATA *psPerProc)
{
	PVRSRV_KERNEL_SYNC_INFO *psSyncInfo;
	IMG_SID hHandle;
	IMG_INT result;

	result = sceGpuSharedSyncObjectGetHandle(psBridgeIn->ui32SharedKey, &hHandle);
	if (result < 0) {
#if FW_DEBUG
		SceDebugForDriver_D9703808(1, 0,
			&dbg_PVRSRVCloseSharedSyncInfoBW_L5632,
			"%s: Error %08x with GetHandle for sync object with key %08x\n",
			__func__, result, psBridgeIn->ui32SharedKey);
#endif
		psBridgeOut->eError = PVRSRV_ERROR_INVALID_PARAMS;
		goto out;
	}
	psBridgeOut->eError = PVRSRVLookupHandle(gpsKernelHandleBase,
			(IMG_VOID **)&psSyncInfo, hHandle, 6);
	if (psBridgeOut->eError != PVRSRV_OK) {
#if FW_DEBUG
		SceDebugForDriver_D9703808(1, 0,
			&dbg_PVRSRVCloseSharedSyncInfoBW_L5639,
			"%s: Error %08x finding kernel handle for sync object with key %08x\n",
			__func__, psBridgeOut->eError, psBridgeIn->ui32SharedKey);
#endif
		goto out;
	}
	psBridgeOut->eError = PVRSRVFindHandle(psPerProc->psHandleBase,
			&hHandle, psSyncInfo, 6);
	if (psBridgeOut->eError != PVRSRV_OK) {
#if FW_DEBUG
		SceDebugForDriver_D9703808(1, 0,
			&dbg_PVRSRVCloseSharedSyncInfoBW_L5645,
			"%s: Error %08x finding process handle for sync object with key %08x\n",
			__func__, psBridgeOut->eError, psBridgeIn->ui32SharedKey);
#endif
		goto out;
	}
#if FW_DEBUG
	if (hHandle != psBridgeIn->hKernelSyncInfo)
		SceDebugForDriver_D9703808(1, 0,
			&dbg_PVRSRVCloseSharedSyncInfoBW_L5650,
			"%s: Tried to close different shared sync object with key %08x (%p vs %p)\n",
			__func__, psBridgeIn->ui32SharedKey,
			(IMG_VOID *)hHandle, (IMG_VOID *)psBridgeIn->hKernelSyncInfo);
#endif
	if (psSyncInfo->ui32SharedKey != psBridgeIn->ui32SharedKey) {
#if FW_DEBUG
		SceDebugForDriver_D9703808(1, 0,
			&dbg_PVRSRVCloseSharedSyncInfoBW_L5654,
			"%s: Tried to close shared sync object with the wrong key (%08x vs %08x)\n",
			__func__, psSyncInfo->ui32SharedKey, psBridgeIn->ui32SharedKey);
#endif
		psBridgeOut->eError = PVRSRV_ERROR_INVALID_PARAMS;
		goto out;
	}
	result = sceGpuSharedSyncObjectClose(psSyncInfo->ui32SharedKey);
	if (result < 0) {
#if FW_DEBUG
		SceDebugForDriver_D9703808(1, 0,
			&dbg_PVRSRVCloseSharedSyncInfoBW_L5661,
			"%s: Could not close sync object with key %08x\n",
			__func__, psBridgeIn->ui32SharedKey);
#endif
		goto out;
	}
	PVRSRVReleaseHandle(psPerProc->psHandleBase, hHandle, 6);
out:
	return 0;
}

typedef struct PVRSRV_BRIDGE_IN_OPEN_SHARED_SYNCINFO {
	IMG_UINT32 ui32BridgeFlags;
	IMG_UINT32 ui32SharedKey;
} PVRSRV_BRIDGE_IN_OPEN_SHARED_SYNCINFO;

typedef struct PVRSRV_BRIDGE_OUT_OPEN_SHARED_SYNCINFO {
	PVRSRV_ERROR eError;
	IMG_SID hKernelSyncInfo;
} PVRSRV_BRIDGE_OUT_OPEN_SHARED_SYNCINFO;

#if FW_DEBUG
static const SceKernelDebugInfo dbg_PVRSRVOpenSharedSyncInfoBW_L5585 = {
	0x1399193b, 0xb3c2b5a7, 0x85b0db37,
	"bridged/bridged_pvr_bridge.c", 5585, "PVRSRVOpenSharedSyncInfoBW"
};
static const SceKernelDebugInfo dbg_PVRSRVOpenSharedSyncInfoBW_L5597 = {
	0x1399193b, 0x8aa926a9, 0x85b0db37,
	"bridged/bridged_pvr_bridge.c", 5597, "PVRSRVOpenSharedSyncInfoBW"
};
#endif

/* SCE imports a shared kernel object as a process subhandle. */
IMG_INT PVRSRVOpenSharedSyncInfoBW(IMG_UINT32 ui32BridgeID,
		PVRSRV_BRIDGE_IN_OPEN_SHARED_SYNCINFO *psBridgeIn,
		PVRSRV_BRIDGE_OUT_OPEN_SHARED_SYNCINFO *psBridgeOut,
		PVRSRV_PER_PROCESS_DATA *psPerProc)
{
	PVRSRV_KERNEL_SYNC_INFO *psSyncInfo;
	IMG_SID hHandle;
	IMG_INT result;

	result = sceGpuSharedSyncObjectOpen(psBridgeIn->ui32SharedKey, &hHandle);
	if (result < 0) {
#if FW_DEBUG
		SceDebugForDriver_D9703808(1, 0,
			&dbg_PVRSRVOpenSharedSyncInfoBW_L5585,
			"%s: Failed to open shared sync object with key %08x: %08x",
			__func__, psBridgeIn->ui32SharedKey, result);
#endif
		psBridgeOut->eError = PVRSRV_ERROR_INVALID_PARAMS;
		goto out;
	}
	psBridgeOut->eError = PVRSRVLookupHandle(gpsKernelHandleBase,
			(IMG_VOID **)&psSyncInfo, hHandle, 6);
	if (psBridgeOut->eError != PVRSRV_OK)
		goto close;
	if (psSyncInfo->ui32SharedKey != psBridgeIn->ui32SharedKey) {
#if FW_DEBUG
		SceDebugForDriver_D9703808(1, 0,
			&dbg_PVRSRVOpenSharedSyncInfoBW_L5597,
			"%s: Invalid shared key (%08x vs %08x)\n",
			__func__, psSyncInfo->ui32SharedKey, psBridgeIn->ui32SharedKey);
#endif
		psBridgeOut->eError = PVRSRV_ERROR_INVALID_PARAMS;
		goto out;
	}
	psBridgeOut->eError = PVRSRVAllocSubHandle(psPerProc->psHandleBase,
			&psBridgeOut->hKernelSyncInfo, psSyncInfo, 6, 0, hHandle);
	if (psBridgeOut->eError != PVRSRV_OK) {
close:
		sceGpuSharedSyncObjectClose(psBridgeIn->ui32SharedKey);
	}
out:
	return 0;
}

/* IMG bridge batch helper; SCE compiles out upstream PVR_ASSERT. */
PVRSRV_ERROR NewHandleBatch(PVRSRV_PER_PROCESS_DATA *psPerProc,
		IMG_UINT32 ui32BatchSize)
{
	PVRSRV_ERROR eError;

	eError = PVRSRVNewHandleBatch(psPerProc->psHandleBase, ui32BatchSize);
	if (eError == PVRSRV_OK)
		psPerProc->bHandlesBatched = IMG_TRUE;
	return eError;
}

typedef struct PVRSRV_BRIDGE_IN_CHECK_SYNC_INFO_MOD_OBJ {
	IMG_UINT32 ui32BridgeFlags;
	IMG_SID hKernelSyncInfoModObj;
} PVRSRV_BRIDGE_IN_CHECK_SYNC_INFO_MOD_OBJ;

typedef struct PVRSRV_BRIDGE_OUT_CHECK_SYNC_INFO_MOD_OBJ {
	IMG_INT result;
	IMG_INT processHandle;
	IMG_UINT32 ui32WaitState;
} PVRSRV_BRIDGE_OUT_CHECK_SYNC_INFO_MOD_OBJ;

IMG_INT sceGpuWaitModObjCheck(MODIFY_SYNC_OP_INFO *psModSyncOpInfo);

/* SCE wait-object bridge returns native status and the process wait state. */
IMG_INT PVRSRVCheckSyncInfoModObjBW(IMG_UINT32 ui32BridgeID,
		PVRSRV_BRIDGE_IN_CHECK_SYNC_INFO_MOD_OBJ *psBridgeIn,
		PVRSRV_BRIDGE_OUT_CHECK_SYNC_INFO_MOD_OBJ *psBridgeOut,
		PVRSRV_PER_PROCESS_DATA *psPerProc)
{
	MODIFY_SYNC_OP_INFO *psModSyncOpInfo;

	if (PVRSRVLookupHandle(psPerProc->psHandleBase,
			(IMG_VOID **)&psModSyncOpInfo,
			psBridgeIn->hKernelSyncInfoModObj, 22) != PVRSRV_OK) {
		psBridgeOut->result = (IMG_INT)0x804c0000;
	} else {
		psBridgeOut->result = sceGpuWaitModObjCheck(psModSyncOpInfo);
		psBridgeOut->processHandle = psPerProc->processHandle;
		psBridgeOut->ui32WaitState = psModSyncOpInfo->ui32WaitState;
	}
	return 0;
}

typedef struct PVRSRV_BRIDGE_IN_MODIFY_COMPLETE_SYNC_OPS {
	IMG_UINT32 ui32BridgeFlags;
	IMG_SID hKernelSyncInfoModObj;
} PVRSRV_BRIDGE_IN_MODIFY_COMPLETE_SYNC_OPS;

/* IMG bridge with SCE multiple-sync extension and diagnostics disabled. */
IMG_INT PVRSRVModifyCompleteSyncOpsBW(IMG_UINT32 ui32BridgeID,
		PVRSRV_BRIDGE_IN_MODIFY_COMPLETE_SYNC_OPS *psBridgeIn,
		PVRSRV_BRIDGE_RETURN *psBridgeOut, PVRSRV_PER_PROCESS_DATA *psPerProc)
{
	MODIFY_SYNC_OP_INFO *psModSyncOpInfo;
	IMG_INT i;

	psBridgeOut->eError = PVRSRVLookupHandle(psPerProc->psHandleBase,
			(IMG_VOID **)&psModSyncOpInfo,
			psBridgeIn->hKernelSyncInfoModObj, 22);
	if (psBridgeOut->eError != PVRSRV_OK)
		return 0;
	for (i = 0; i < (IMG_INT)psModSyncOpInfo->ui32NumSyncs; i++) {
		if (psModSyncOpInfo->apsKernelSyncInfo[i] == 0) {
			psBridgeOut->eError = PVRSRV_ERROR_INVALID_PARAMS;
			return 0;
		}
	}
	for (i = 0; i < (IMG_INT)psModSyncOpInfo->ui32NumSyncs; i++) {
		psBridgeOut->eError = DoModifyCompleteSyncOps(psModSyncOpInfo, i);
		if (psBridgeOut->eError != PVRSRV_OK)
			return 0;
		psModSyncOpInfo->apsKernelSyncInfo[i] = 0;
	}
	PVRSRVScheduleDeviceCallbacks();
	return 0;
}

typedef struct PVRSRV_BRIDGE_IN_MODIFY_PENDING_SYNC_OPS {
	IMG_UINT32 ui32BridgeFlags;
	IMG_SID hKernelSyncInfoModObj;
	IMG_SID ahKernelSyncInfo[4];
	IMG_UINT32 ui32ModifyFlags;
	IMG_UINT32 ui32NumSyncs;
} PVRSRV_BRIDGE_IN_MODIFY_PENDING_SYNC_OPS;

/* IMG operation with SCE four-sync validation, duplicate rejection and snapshots. */
IMG_INT PVRSRVModifyPendingSyncOpsBW(IMG_UINT32 ui32BridgeID,
		PVRSRV_BRIDGE_IN_MODIFY_PENDING_SYNC_OPS *psBridgeIn,
		PVRSRV_BRIDGE_RETURN *psBridgeOut, PVRSRV_PER_PROCESS_DATA *psPerProc)
{
	MODIFY_SYNC_OP_INFO *psModSyncOpInfo;
	PVRSRV_KERNEL_SYNC_INFO *apsKernelSyncInfo[4];
	IMG_INT i, j;

	psBridgeOut->eError = PVRSRVLookupHandle(psPerProc->psHandleBase,
			(IMG_VOID **)&psModSyncOpInfo,
			psBridgeIn->hKernelSyncInfoModObj, 22);
	if (psBridgeOut->eError != PVRSRV_OK)
		return 0;
	if ((IMG_INT)psBridgeIn->ui32NumSyncs > 4) {
		psBridgeOut->eError = PVRSRV_ERROR_INVALID_PARAMS;
		return 0;
	}
	for (i = 0; i < (IMG_INT)psBridgeIn->ui32NumSyncs; i++) {
		psBridgeOut->eError = PVRSRVLookupHandle(psPerProc->psHandleBase,
				(IMG_VOID **)&apsKernelSyncInfo[i],
				psBridgeIn->ahKernelSyncInfo[i], 6);
		if (psBridgeOut->eError != PVRSRV_OK)
			return 0;
		for (j = 0; j < i; j++) {
			if (apsKernelSyncInfo[j] == apsKernelSyncInfo[i]) {
				psBridgeOut->eError = PVRSRV_ERROR_RETRY;
				return 0;
			}
		}
		if (psModSyncOpInfo->apsKernelSyncInfo[i]) {
			psBridgeOut->eError = PVRSRV_ERROR_RETRY;
			return 0;
		}
	}
	psModSyncOpInfo->ui32ModifyFlags = psBridgeIn->ui32ModifyFlags;
	psModSyncOpInfo->ui32NumSyncs = psBridgeIn->ui32NumSyncs;
	for (i = 0; i < (IMG_INT)psBridgeIn->ui32NumSyncs; i++) {
		psModSyncOpInfo->apsKernelSyncInfo[i] = apsKernelSyncInfo[i];
		psModSyncOpInfo->aui32ReadOpsPendingSnapShot[i] =
			apsKernelSyncInfo[i]->psSyncData->ui32ReadOpsPending;
		psModSyncOpInfo->aui32WriteOpsPendingSnapShot[i] =
			apsKernelSyncInfo[i]->psSyncData->ui32WriteOpsPending;
		if (psBridgeIn->ui32ModifyFlags & 1)
			apsKernelSyncInfo[i]->psSyncData->ui32WriteOpsPending++;
		if (psBridgeIn->ui32ModifyFlags & 2)
			apsKernelSyncInfo[i]->psSyncData->ui32ReadOpsPending++;
	}
	psBridgeOut->eError = ResManDissociateRes(psModSyncOpInfo->hResItem,
			psPerProc->hResManContext);
	return 0;
}

typedef struct PVRSRV_BRIDGE_IN_DESTROY_SYNC_INFO_MOD_OBJ {
	IMG_UINT32 ui32BridgeFlags;
	IMG_SID hKernelSyncInfoModObj;
} PVRSRV_BRIDGE_IN_DESTROY_SYNC_INFO_MOD_OBJ;

IMG_INT sceGpuWaitModObjRemove(MODIFY_SYNC_OP_INFO *psModSyncOpInfo);

/* IMG destroy operation with SCE wait removal and per-process mask cleanup. */
IMG_INT PVRSRVDestroySyncInfoModObjBW(IMG_UINT32 ui32BridgeID,
		PVRSRV_BRIDGE_IN_DESTROY_SYNC_INFO_MOD_OBJ *psBridgeIn,
		PVRSRV_BRIDGE_RETURN *psBridgeOut, PVRSRV_PER_PROCESS_DATA *psPerProc)
{
	MODIFY_SYNC_OP_INFO *psModSyncOpInfo;
	IMG_UINT32 ui32WaitState;
	IMG_INT i;

	psBridgeOut->eError = PVRSRVLookupHandle(psPerProc->psHandleBase,
			(IMG_VOID **)&psModSyncOpInfo,
			psBridgeIn->hKernelSyncInfoModObj, 22);
	if (psBridgeOut->eError != PVRSRV_OK)
		return 0;
	if (sceGpuWaitModObjRemove(psModSyncOpInfo) < 0) {
		psBridgeOut->eError = PVRSRV_ERROR_INVALID_PARAMS;
		return 0;
	}
	ui32WaitState = psModSyncOpInfo->ui32WaitState;
	for (i = 0; i < (IMG_INT)psModSyncOpInfo->ui32NumSyncs; i++) {
		if (psModSyncOpInfo->apsKernelSyncInfo[i]) {
			psBridgeOut->eError = PVRSRV_ERROR_INVALID_PARAMS;
			return 0;
		}
	}
	psBridgeOut->eError = PVRSRVReleaseHandle(psPerProc->psHandleBase,
			psBridgeIn->hKernelSyncInfoModObj, 22);
	if (psBridgeOut->eError != PVRSRV_OK)
		return 0;
	psBridgeOut->eError = ResManFreeResByPtr(psModSyncOpInfo->hResItem, IMG_FALSE);
	if (psBridgeOut->eError == PVRSRV_OK)
		psPerProc->syncModObjectMask &= ~ui32WaitState;
	return 0;
}

/* IMG cleanup callback/LOOP_UNTIL_TIMEOUT with SCE multiple-sync extension.
 * SCE disables upstream diagnostics and uses its two-argument resman callback.
 */
PVRSRV_ERROR ModifyCompleteSyncOpsCallBack(IMG_VOID *pvParam, IMG_UINT32 ui32Param,
		IMG_BOOL bForceCleanup)
{
	MODIFY_SYNC_OP_INFO *psModSyncOpInfo = pvParam;
	IMG_INT i;

	if (!pvParam)
		return PVRSRV_ERROR_INVALID_PARAMS;
	for (i = 0; i < (IMG_INT)psModSyncOpInfo->ui32NumSyncs; i++) {
		IMG_UINT32 uiOffset, uiStart, uiCurrent;
		IMG_INT iNotLastLoop;

		if (!psModSyncOpInfo->apsKernelSyncInfo[i])
			continue;
		for (uiOffset = 0, uiStart = OSClockus(), uiCurrent = uiStart + 1,
				iNotLastLoop = 1;
				((uiCurrent - uiStart + uiOffset) < 500000) || iNotLastLoop--;
				uiCurrent = OSClockus(),
				uiOffset = uiCurrent < uiStart ? ~0U - uiStart : uiOffset,
				uiStart = uiCurrent < uiStart ? 0 : uiStart) {
			if (DoQuerySyncOpsSatisfied(psModSyncOpInfo->apsKernelSyncInfo[i],
					psModSyncOpInfo->aui32ReadOpsPendingSnapShot[i],
					psModSyncOpInfo->aui32WriteOpsPendingSnapShot[i]) == PVRSRV_OK)
				goto OpFlushedComplete;
			OSSleepms(1);
		}
		return PVRSRV_ERROR_TIMEOUT;
OpFlushedComplete:
		DoModifyCompleteSyncOps(psModSyncOpInfo, i);
		psModSyncOpInfo->apsKernelSyncInfo[i] = 0;
	}
	OSFreeMem_Impl(1, sizeof(*psModSyncOpInfo), psModSyncOpInfo, 0);
	PVRSRVScheduleDeviceCallbacks();
	return PVRSRV_OK;
}

typedef struct PVRSRV_BRIDGE_IN_GET_MISC_INFO {
	IMG_UINT32 ui32BridgeFlags;
	PVRSRV_MISC_INFO sMiscInfo;
} PVRSRV_BRIDGE_IN_GET_MISC_INFO;

typedef struct PVRSRV_BRIDGE_OUT_GET_MISC_INFO {
	PVRSRV_ERROR eError;
	PVRSRV_MISC_INFO sMiscInfo;
} PVRSRV_BRIDGE_OUT_GET_MISC_INFO;

/* IMG misc-info bridge; SCE retains only the DDK-version string copy path. */
IMG_INT PVRSRVGetMiscInfoBW(IMG_UINT32 ui32BridgeID,
		PVRSRV_BRIDGE_IN_GET_MISC_INFO *psBridgeIn,
		PVRSRV_BRIDGE_OUT_GET_MISC_INFO *psBridgeOut,
		PVRSRV_PER_PROCESS_DATA *psPerProc)
{
	PVRSRV_ERROR eError;

	OSMemCopy(&psBridgeOut->sMiscInfo, &psBridgeIn->sMiscInfo,
			sizeof(PVRSRV_MISC_INFO));
	if (psBridgeIn->sMiscInfo.ui32StateRequest & 0x10) {
		psBridgeOut->eError = OSAllocMem_Impl(1,
				psBridgeOut->sMiscInfo.ui32MemoryStrLen,
				(IMG_VOID **)&psBridgeOut->sMiscInfo.pszMemoryStr, 0);
		if (psBridgeOut->eError != PVRSRV_OK)
			return 0;
		psBridgeOut->eError = PVRSRVGetMiscInfoKM(&psBridgeOut->sMiscInfo);
		eError = CopyToUserWrapper(psPerProc, psBridgeIn->sMiscInfo.pszMemoryStr,
				psBridgeOut->sMiscInfo.pszMemoryStr,
				psBridgeOut->sMiscInfo.ui32MemoryStrLen);
		OSFreeMem_Impl(1, psBridgeOut->sMiscInfo.ui32MemoryStrLen,
				psBridgeOut->sMiscInfo.pszMemoryStr, 0);
		psBridgeOut->sMiscInfo.pszMemoryStr = psBridgeIn->sMiscInfo.pszMemoryStr;
		if (eError != PVRSRV_OK)
			return -14;
	} else {
		psBridgeOut->eError = PVRSRVGetMiscInfoKM(&psBridgeOut->sMiscInfo);
	}
	return 0;
}

typedef struct PVRSRV_BRIDGE_IN_UNMAP_MEMORY_FROM_GPU {
	IMG_UINT32 ui32BridgeFlags;
	IMG_VOID *cpuAddress;
	IMG_SID hDevMemHeap;
	IMG_BOOL bWaitForIdle;
} PVRSRV_BRIDGE_IN_UNMAP_MEMORY_FROM_GPU;

/* SCE memory-unmap bridge accepts an optional heap and propagates RETRY. */
IMG_INT PVRSRVUnmapMemoryFromGpuBW(IMG_UINT32 ui32BridgeID,
		PVRSRV_BRIDGE_IN_UNMAP_MEMORY_FROM_GPU *psBridgeIn,
		PVRSRV_BRIDGE_RETURN *psBridgeOut, PVRSRV_PER_PROCESS_DATA *psPerProc)
{
	GpuHeap *heap = 0;

	if (psBridgeIn->hDevMemHeap) {
		psBridgeOut->eError = PVRSRVLookupHandle(psPerProc->psHandleBase,
				(IMG_VOID **)&heap, psBridgeIn->hDevMemHeap, 4);
		if (psBridgeOut->eError != PVRSRV_OK)
			return 0;
	}
	psBridgeOut->eError = PVRSRVUnmapMemoryFromGpuKM(psPerProc,
			psBridgeIn->cpuAddress, heap, psBridgeIn->bWaitForIdle);
	return psBridgeOut->eError == PVRSRV_ERROR_RETRY ? PVRSRV_ERROR_RETRY : 0;
}

typedef struct PVRSRV_BRIDGE_IN_MAP_MEMORY_TO_GPU {
	IMG_UINT32 ui32BridgeFlags;
	IMG_SID hDevCookie;
	IMG_SID hDevMemContext;
	IMG_SID hDevMemHeap;
	IMG_VOID *cpuAddress;
	IMG_SIZE_T size;
	IMG_SIZE_T allocationSize;
	IMG_UINT32 flags;
} PVRSRV_BRIDGE_IN_MAP_MEMORY_TO_GPU;

typedef struct PVRSRV_BRIDGE_OUT_MAP_MEMORY_TO_GPU {
	PVRSRV_ERROR eError;
	IMG_DEV_VIRTADDR deviceAddress;
} PVRSRV_BRIDGE_OUT_MAP_MEMORY_TO_GPU;

/* SCE maps a CPU range through its process context and optional GPU heap. */
IMG_INT PVRSRVMapMemoryToGpuBW(IMG_UINT32 ui32BridgeID,
		PVRSRV_BRIDGE_IN_MAP_MEMORY_TO_GPU *psBridgeIn,
		PVRSRV_BRIDGE_OUT_MAP_MEMORY_TO_GPU *psBridgeOut,
		PVRSRV_PER_PROCESS_DATA *psPerProc)
{
	PVRSRV_DEVICE_NODE *psDeviceNode;
	GpuMemoryContext *context;
	GpuHeap *heap = 0;

	psBridgeOut->eError = PVRSRVLookupHandle(psPerProc->psHandleBase,
			(IMG_VOID **)&psDeviceNode, psBridgeIn->hDevCookie, 2);
	if (psBridgeOut->eError != PVRSRV_OK)
		return 0;
	psBridgeOut->eError = PVRSRVLookupHandle(psPerProc->psHandleBase,
			(IMG_VOID **)&context, psBridgeIn->hDevMemContext, 3);
	if (psBridgeOut->eError != PVRSRV_OK)
		return 0;
	if (psBridgeIn->hDevMemHeap) {
		psBridgeOut->eError = PVRSRVLookupHandle(psPerProc->psHandleBase,
				(IMG_VOID **)&heap, psBridgeIn->hDevMemHeap, 4);
		if (psBridgeOut->eError != PVRSRV_OK)
			return 0;
	}
	psBridgeOut->eError = PVRSRVMapMemoryToGpuKM(psPerProc, psDeviceNode,
			context, heap, psBridgeIn->cpuAddress, psBridgeIn->size,
			psBridgeIn->allocationSize, psBridgeIn->flags,
			&psBridgeOut->deviceAddress);
	return 0;
}

typedef struct PVRSRV_BRIDGE_IN_DESTROY_DEVMEMCONTEXT {
	IMG_UINT32 ui32BridgeFlags;
	IMG_SID hDevCookie;
	IMG_SID hDevMemContext;
} PVRSRV_BRIDGE_IN_DESTROY_DEVMEMCONTEXT;

/* IMG context destruction with SCE rejection while any CPU range is mapped. */
IMG_INT PVRSRVDestroyDeviceMemContextBW(IMG_UINT32 ui32BridgeID,
		PVRSRV_BRIDGE_IN_DESTROY_DEVMEMCONTEXT *psBridgeIn,
		PVRSRV_BRIDGE_RETURN *psBridgeOut, PVRSRV_PER_PROCESS_DATA *psPerProc)
{
	PVRSRV_DEVICE_NODE *psDeviceNode;
	GpuMemoryContext *context;
	IMG_BOOL bDestroyed;

	psBridgeOut->eError = PVRSRVLookupHandle(psPerProc->psHandleBase,
			(IMG_VOID **)&psDeviceNode, psBridgeIn->hDevCookie, 2);
	if (psBridgeOut->eError != PVRSRV_OK)
		return 0;
	psBridgeOut->eError = PVRSRVLookupHandle(psPerProc->psHandleBase,
			(IMG_VOID **)&context, psBridgeIn->hDevMemContext, 3);
	if (psBridgeOut->eError != PVRSRV_OK)
		return 0;
	if (psPerProc->memoryContext && psPerProc->memoryContext->head) {
		psBridgeOut->eError = (PVRSRV_ERROR)161;
		return 0;
	}
	psBridgeOut->eError = PVRSRVDestroyDeviceMemContextKM(psDeviceNode,
			context, &bDestroyed);
	if (psBridgeOut->eError != PVRSRV_OK)
		return 0;
	if (bDestroyed)
		psBridgeOut->eError = PVRSRVReleaseHandle(psPerProc->psHandleBase,
				psBridgeIn->hDevMemContext, 3);
	return 0;
}

typedef struct PVRSRV_BRIDGE_IN_ACQUIRE_DEVICEINFO {
	IMG_UINT32 ui32BridgeFlags;
	IMG_UINT32 uiDevIndex;
	PVRSRV_DEVICE_TYPE eDeviceType;
} PVRSRV_BRIDGE_IN_ACQUIRE_DEVICEINFO;

typedef struct PVRSRV_BRIDGE_OUT_ACQUIRE_DEVICEINFO {
	PVRSRV_ERROR eError;
	IMG_SID hDevCookie;
} PVRSRV_BRIDGE_OUT_ACQUIRE_DEVICEINFO;

/* IMG acquisition bridge; SCE compiles out the command assertion. */
IMG_INT PVRSRVAcquireDeviceDataBW(IMG_UINT32 ui32BridgeID,
		PVRSRV_BRIDGE_IN_ACQUIRE_DEVICEINFO *psBridgeIn,
		PVRSRV_BRIDGE_OUT_ACQUIRE_DEVICEINFO *psBridgeOut,
		PVRSRV_PER_PROCESS_DATA *psPerProc)
{
	IMG_HANDLE hDevCookieInt;

	psBridgeOut->eError = PVRSRVAcquireDeviceDataKM(psBridgeIn->uiDevIndex,
			psBridgeIn->eDeviceType, &hDevCookieInt);
	if (psBridgeOut->eError != PVRSRV_OK)
		return 0;
	psBridgeOut->eError = PVRSRVAllocHandle(psPerProc->psHandleBase,
			&psBridgeOut->hDevCookie, hDevCookieInt, 2, 1);
	return 0;
}

typedef struct PVRSRV_BRIDGE_OUT_ENUMDEVICE {
	PVRSRV_ERROR eError;
	IMG_UINT32 ui32NumDevices;
	PVRSRV_DEVICE_IDENTIFIER asDeviceIdentifier[16];
} PVRSRV_BRIDGE_OUT_ENUMDEVICE;

/* IMG enumeration bridge; SCE compiles out the command assertion. */
IMG_INT PVRSRVEnumerateDevicesBW(IMG_UINT32 ui32BridgeID, IMG_VOID *psBridgeIn,
		PVRSRV_BRIDGE_OUT_ENUMDEVICE *psBridgeOut,
		PVRSRV_PER_PROCESS_DATA *psPerProc)
{
	psBridgeOut->eError = PVRSRVEnumerateDevicesKM(&psBridgeOut->ui32NumDevices,
			psBridgeOut->asDeviceIdentifier);
	return 0;
}

typedef struct PVRSRV_BRIDGE_IN_FREEDEVICEMEM {
	IMG_UINT32 ui32BridgeFlags;
	IMG_SID hDevCookie;
	IMG_SID hKernelMemInfo;
} PVRSRV_BRIDGE_IN_FREEDEVICEMEM;

/* IMG free-device-memory bridge; SCE disables upstream diagnostics. */
IMG_INT PVRSRVFreeDeviceMemBW(IMG_UINT32 ui32BridgeID,
		PVRSRV_BRIDGE_IN_FREEDEVICEMEM *psBridgeIn,
		PVRSRV_BRIDGE_RETURN *psBridgeOut, PVRSRV_PER_PROCESS_DATA *psPerProc)
{
	PVRSRV_DEVICE_NODE *psDeviceNode;
	PVRSRV_KERNEL_MEM_INFO *psMemInfo;

	psBridgeOut->eError = PVRSRVLookupHandle(psPerProc->psHandleBase,
			(IMG_VOID **)&psDeviceNode, psBridgeIn->hDevCookie, 2);
	if (psBridgeOut->eError != PVRSRV_OK)
		return 0;
	psBridgeOut->eError = PVRSRVLookupHandle(psPerProc->psHandleBase,
			(IMG_VOID **)&psMemInfo, psBridgeIn->hKernelMemInfo, 5);
	if (psBridgeOut->eError != PVRSRV_OK)
		return 0;
	psBridgeOut->eError = PVRSRVFreeDeviceMemKM(psDeviceNode, psMemInfo);
	if (psBridgeOut->eError != PVRSRV_OK)
		return 0;
	psBridgeOut->eError = PVRSRVReleaseHandle(psPerProc->psHandleBase,
			psBridgeIn->hKernelMemInfo, 5);
	return 0;
}

/* IMG services.h full heap descriptor (native handle form). */
typedef struct PVRSRV_BRIDGE_IN_GET_DEVMEM_HEAPINFO {
	IMG_UINT32 ui32BridgeFlags;
	IMG_SID hDevCookie;
	IMG_SID hDevMemContext;
} PVRSRV_BRIDGE_IN_GET_DEVMEM_HEAPINFO;

typedef struct PVRSRV_BRIDGE_OUT_GET_DEVMEM_HEAPINFO {
	PVRSRV_ERROR eError;
	IMG_UINT32 ui32ClientHeapCount;
	PVRSRV_HEAP_INFO sHeapInfo[32];
} PVRSRV_BRIDGE_OUT_GET_DEVMEM_HEAPINFO;

/* IMG heap-info bridge with SCE diagnostics and inline assertions disabled. */
IMG_INT PVRSRVGetDeviceMemHeapInfoBW(IMG_UINT32 ui32BridgeID,
		PVRSRV_BRIDGE_IN_GET_DEVMEM_HEAPINFO *psBridgeIn,
		PVRSRV_BRIDGE_OUT_GET_DEVMEM_HEAPINFO *psBridgeOut,
		PVRSRV_PER_PROCESS_DATA *psPerProc)
{
	PVRSRV_DEVICE_NODE *psDeviceNode;
	GpuMemoryContext *context;
	IMG_UINT32 i;

	psBridgeOut->eError = NewHandleBatch(psPerProc, 32);
	if (psBridgeOut->eError != PVRSRV_OK)
		return 0;
	psBridgeOut->eError = PVRSRVLookupHandle(psPerProc->psHandleBase,
			(IMG_VOID **)&psDeviceNode, psBridgeIn->hDevCookie, 2);
	if (psBridgeOut->eError != PVRSRV_OK)
		return 0;
	psBridgeOut->eError = PVRSRVLookupHandle(psPerProc->psHandleBase,
			(IMG_VOID **)&context, psBridgeIn->hDevMemContext, 3);
	if (psBridgeOut->eError != PVRSRV_OK)
		return 0;
	psBridgeOut->eError = PVRSRVGetDeviceMemHeapInfoKM(psDeviceNode, context,
			&psBridgeOut->ui32ClientHeapCount, psBridgeOut->sHeapInfo,
			abSharedDeviceMemHeap);
	if (psBridgeOut->eError != PVRSRV_OK)
		return 0;
	for (i = 0; i < psBridgeOut->ui32ClientHeapCount; i++) {
		IMG_SID hDevMemHeapExt;

		if (abSharedDeviceMemHeap[i]) {
			PVRSRVAllocHandle(psPerProc->psHandleBase, &hDevMemHeapExt,
					psBridgeOut->sHeapInfo[i].hDevMemHeap, 4, 1);
		} else {
			psBridgeOut->eError = PVRSRVFindHandle(psPerProc->psHandleBase,
					&hDevMemHeapExt,
					psBridgeOut->sHeapInfo[i].hDevMemHeap, 4);
			if (psBridgeOut->eError != PVRSRV_OK)
				return 0;
		}
		psBridgeOut->sHeapInfo[i].hDevMemHeap = (IMG_HANDLE)hDevMemHeapExt;
	}
	psPerProc->bHandlesBatched = IMG_FALSE;
	psBridgeOut->eError = PVRSRVCommitHandleBatch(psPerProc->psHandleBase);
	return 0;
}

typedef struct PVRSRV_BRIDGE_OUT_CREATE_SYNC_INFO_MOD_OBJ {
	PVRSRV_ERROR eError;
	IMG_SID hKernelSyncInfoModObj;
} PVRSRV_BRIDGE_OUT_CREATE_SYNC_INFO_MOD_OBJ;

IMG_INT sceGpuWaitModObjAdd(MODIFY_SYNC_OP_INFO *psModSyncOpInfo,
		IMG_INT waitEventFlag);

/* IMG constructor with SCE multi-sync owner, mask quota and wait registration. */
IMG_INT PVRSRVCreateSyncInfoModObjBW(IMG_UINT32 ui32BridgeID, IMG_VOID *psBridgeIn,
		PVRSRV_BRIDGE_OUT_CREATE_SYNC_INFO_MOD_OBJ *psBridgeOut,
		PVRSRV_PER_PROCESS_DATA *psPerProc)
{
	MODIFY_SYNC_OP_INFO *psModSyncOpInfo;
	IMG_UINT32 i, mask;

	psBridgeOut->eError = NewHandleBatch(psPerProc, 1);
	if (psBridgeOut->eError != PVRSRV_OK)
		return 0;
	for (i = 0; i < 32; i++) {
		mask = 1U << i;
		if (!(mask & psPerProc->syncModObjectMask))
			break;
	}
	if (i == 32) {
		psBridgeOut->eError = PVRSRV_ERROR_OUT_OF_MEMORY;
		return 0;
	}
	psBridgeOut->eError = OSAllocMem_Impl(1, sizeof(*psModSyncOpInfo),
			(IMG_VOID **)&psModSyncOpInfo, 0);
	if (psBridgeOut->eError != PVRSRV_OK)
		return 0;
	memset(psModSyncOpInfo, 0, sizeof(*psModSyncOpInfo));
	psModSyncOpInfo->ui32WaitState |= mask;
	psBridgeOut->eError = PVRSRVAllocHandle(psPerProc->psHandleBase,
			&psBridgeOut->hKernelSyncInfoModObj, psModSyncOpInfo, 22, 4);
	if (psBridgeOut->eError != PVRSRV_OK)
		return 0;
	if (sceGpuWaitModObjAdd(psModSyncOpInfo, psPerProc->waitEventFlag) < 0) {
		psBridgeOut->eError = PVRSRV_ERROR_OUT_OF_MEMORY;
		return 0;
	}
	psPerProc->syncModObjectMask |= mask;
	psModSyncOpInfo->hResItem = ResManRegisterRes(psPerProc->hResManContext,
			19, psModSyncOpInfo, 0, ModifyCompleteSyncOpsCallBack);
	psPerProc->bHandlesBatched = IMG_FALSE;
	psBridgeOut->eError = PVRSRVCommitHandleBatch(psPerProc->psHandleBase);
	return 0;
}

typedef struct PVRSRV_BRIDGE_IN_ALLOC_SYNC_INFO {
	IMG_UINT32 ui32BridgeFlags;
	IMG_SID hDevCookie;
} PVRSRV_BRIDGE_IN_ALLOC_SYNC_INFO;

typedef struct PVRSRV_BRIDGE_OUT_ALLOC_SYNC_INFO {
	PVRSRV_ERROR eError;
	IMG_SID hKernelSyncInfo;
} PVRSRV_BRIDGE_OUT_ALLOC_SYNC_INFO;

/* IMG allocation bridge; SCE adds the per-process 48-sync quota. */
IMG_INT PVRSRVAllocSyncInfoBW(IMG_UINT32 ui32BridgeID,
		PVRSRV_BRIDGE_IN_ALLOC_SYNC_INFO *psAllocSyncInfoIN,
		PVRSRV_BRIDGE_OUT_ALLOC_SYNC_INFO *psAllocSyncInfoOUT,
		PVRSRV_PER_PROCESS_DATA *psPerProc)
{
	PVRSRV_KERNEL_SYNC_INFO *psSyncInfo;
	PVRSRV_DEVICE_NODE *psDeviceNode;
	PVRSRV_ERROR eError;

	psAllocSyncInfoOUT->eError = PVRSRV_OK;
	if (psPerProc->numSyncObjects > 47) {
		psAllocSyncInfoOUT->eError = (PVRSRV_ERROR)185;
		return 0;
	}
	psAllocSyncInfoOUT->eError = NewHandleBatch(psPerProc, 1);
	if (psAllocSyncInfoOUT->eError != PVRSRV_OK)
		return 0;
	eError = PVRSRVLookupHandle(psPerProc->psHandleBase,
			(IMG_VOID **)&psDeviceNode, psAllocSyncInfoIN->hDevCookie, 2);
	if (eError != PVRSRV_OK)
		goto allocsyncinfo_errorexit;
	eError = PVRSRVAllocSyncInfoKM(psDeviceNode,
			psDeviceNode->sDevMemoryInfo.pBMKernelContext, &psSyncInfo);
	if (eError != PVRSRV_OK)
		goto allocsyncinfo_errorexit;
	eError = PVRSRVAllocHandle(psPerProc->psHandleBase,
			&psAllocSyncInfoOUT->hKernelSyncInfo, psSyncInfo, 6, 4);
	if (eError != PVRSRV_OK) {
		PVRSRVFreeSyncInfoKM(psSyncInfo);
		goto allocsyncinfo_errorexit;
	}
	psSyncInfo->hResItem = ResManRegisterRes(psPerProc->hResManContext,
			20, psSyncInfo, 0, FreeSyncInfoCallback);
	psPerProc->numSyncObjects++;
allocsyncinfo_errorexit:
	psAllocSyncInfoOUT->eError = eError;
	psPerProc->bHandlesBatched = IMG_FALSE;
	PVRSRVCommitHandleBatch(psPerProc->psHandleBase);
	return 0;
}

typedef struct PVRSRV_BRIDGE_IN_CREATE_SHARED_SYNC_INFO {
	IMG_UINT32 ui32BridgeFlags;
	IMG_UINT32 ui32SharedKey;
	IMG_SID hKernelSyncInfo;
} PVRSRV_BRIDGE_IN_CREATE_SHARED_SYNC_INFO;

#if FW_DEBUG
static const SceKernelDebugInfo dbg_PVRSRVCreateSharedSyncInfoBW_L5541 = {
	0x1399193b, 0xb03b9a9d, 0x5c517d76,
	"bridged/bridged_pvr_bridge.c", 5541, "PVRSRVCreateSharedSyncInfoBW"
};
static const SceKernelDebugInfo dbg_PVRSRVCreateSharedSyncInfoBW_L5549 = {
	0x1399193b, 0xb19f1816, 0x5c517d76,
	"bridged/bridged_pvr_bridge.c", 5549, "PVRSRVCreateSharedSyncInfoBW"
};
#endif

/* SCE shared-sync bridge, absent from the pristine IMG bridge source. */
IMG_INT PVRSRVCreateSharedSyncInfoBW(IMG_UINT32 ui32BridgeID,
		PVRSRV_BRIDGE_IN_CREATE_SHARED_SYNC_INFO *psBridgeIn,
		PVRSRV_BRIDGE_RETURN *psBridgeOut, PVRSRV_PER_PROCESS_DATA *psPerProc)
{
	PVRSRV_KERNEL_SYNC_INFO *psSyncInfo;
	IMG_SID hHandle;
	IMG_INT result;

	psBridgeOut->eError = NewHandleBatch(psPerProc, 1);
	if (psBridgeOut->eError != PVRSRV_OK)
		return 0;
	psBridgeOut->eError = PVRSRVLookupHandle(psPerProc->psHandleBase,
			(IMG_VOID **)&psSyncInfo, psBridgeIn->hKernelSyncInfo, 6);
	if (psBridgeOut->eError != PVRSRV_OK)
		return 0;
	psBridgeOut->eError = PVRSRVFindHandle(gpsKernelHandleBase, &hHandle,
			psSyncInfo, 6);
	if (psBridgeOut->eError == PVRSRV_OK) {
		psBridgeOut->eError = PVRSRV_ERROR_INVALID_PARAMS;
		return 0;
	}
	psBridgeOut->eError = PVRSRVAllocHandle(gpsKernelHandleBase, &hHandle,
			psSyncInfo, 6, 3);
	if (psBridgeOut->eError != PVRSRV_OK) {
#if FW_DEBUG
		SceDebugForDriver_D9703808(1, 0,
				&dbg_PVRSRVCreateSharedSyncInfoBW_L5541,
				"Failed to create handle: %08x\n", psBridgeOut->eError);
#endif
		return 0;
	}
	result = sceGpuSharedSyncObjectCreate(psBridgeIn->ui32SharedKey, hHandle);
	if (result < 0) {
#if FW_DEBUG
		SceDebugForDriver_D9703808(1, 0,
				&dbg_PVRSRVCreateSharedSyncInfoBW_L5549,
				"Failed to create shared object: %08x\n", result);
#endif
		psBridgeOut->eError = PVRSRV_ERROR_INVALID_PARAMS;
		return 0;
	}
	psSyncInfo->ui32SharedKey = psBridgeIn->ui32SharedKey;
	psSyncInfo->ui32RefCount++;
	psSyncInfo->hSharedResItem = ResManRegisterRes(psPerProc->hResManContext,
			21, psSyncInfo, psBridgeIn->ui32SharedKey,
			DeleteSharedSyncInfoCallback);
	psPerProc->bHandlesBatched = IMG_FALSE;
	psBridgeOut->eError = PVRSRVCommitHandleBatch(psPerProc->psHandleBase);
	return 0;
}

/* SCE removes CPU-physical, sync, mapping and resource metadata from the ABI. */
typedef struct PVRSRV_CLIENT_MEM_INFO {
	IMG_VOID *pvLinAddrKM;
	IMG_VOID *pvLinAddr;
	IMG_DEV_VIRTADDR sDevVAddr;
	IMG_UINT32 ui32Flags;
	IMG_UINT32 ui32ClientFlags;
	IMG_SIZE_T uAllocSize;
	IMG_SID hKernelMemInfo;
} PVRSRV_CLIENT_MEM_INFO;

typedef struct PVRSRV_BRIDGE_IN_ALLOCDEVICEMEM {
	IMG_UINT32 ui32BridgeFlags;
	IMG_SID hDevCookie;
	IMG_SID hDevMemHeap;
	IMG_UINT32 ui32Attribs;
	IMG_SIZE_T ui32Size;
	IMG_SIZE_T ui32Alignment;
	IMG_INT32 hMemBlockProcRef;
} PVRSRV_BRIDGE_IN_ALLOCDEVICEMEM;

typedef struct PVRSRV_BRIDGE_OUT_ALLOCDEVICEMEM {
	PVRSRV_ERROR eError;
	IMG_SID hKernelMemInfo;
	PVRSRV_CLIENT_MEM_INFO sClientMemInfo;
} PVRSRV_BRIDGE_OUT_ALLOCDEVICEMEM;

static const SceKernelDebugInfo dbg_PVRSRVAllocDeviceMemBW_L816 = {
	0x1399193b, 0xc915e7fa, 0x532a2c10,
	"bridged/bridged_pvr_bridge.c", 816, "PVRSRVAllocDeviceMemBW"
};

/* IMG allocation bridge with the SCE memblock-backed allocator and compact ABI. */
IMG_INT PVRSRVAllocDeviceMemBW(IMG_UINT32 ui32BridgeID,
		PVRSRV_BRIDGE_IN_ALLOCDEVICEMEM *psAllocDeviceMemIN,
		PVRSRV_BRIDGE_OUT_ALLOCDEVICEMEM *psAllocDeviceMemOUT,
		PVRSRV_PER_PROCESS_DATA *psPerProc)
{
	PVRSRV_DEVICE_NODE *psDeviceNode;
	struct GpuHeap *heap;
	PVRSRV_KERNEL_MEM_INFO *psMemInfo;

	if (psAllocDeviceMemIN->hMemBlockProcRef <= 1 ||
			!(psAllocDeviceMemIN->ui32Attribs & 0x400000)) {
		psAllocDeviceMemOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;
		return 0;
	}
	psAllocDeviceMemOUT->eError = NewHandleBatch(psPerProc, 2);
	if (psAllocDeviceMemOUT->eError != PVRSRV_OK)
		return 0;
	psAllocDeviceMemOUT->eError = PVRSRVLookupHandle(psPerProc->psHandleBase,
			(IMG_VOID **)&psDeviceNode, psAllocDeviceMemIN->hDevCookie, 2);
	if (psAllocDeviceMemOUT->eError != PVRSRV_OK)
		return 0;
	psAllocDeviceMemOUT->eError = PVRSRVLookupHandle(psPerProc->psHandleBase,
			(IMG_VOID **)&heap, psAllocDeviceMemIN->hDevMemHeap, 4);
	if (psAllocDeviceMemOUT->eError != PVRSRV_OK)
		return 0;
	psAllocDeviceMemIN->ui32Attribs &= ~0x200000U;
	psAllocDeviceMemOUT->eError = _PVRSRVAllocDeviceMemKM(psDeviceNode,
			psPerProc, heap, psAllocDeviceMemIN->ui32Attribs,
			psAllocDeviceMemIN->ui32Size, psAllocDeviceMemIN->ui32Alignment,
			psAllocDeviceMemIN->hMemBlockProcRef, &psMemInfo);
	if (psAllocDeviceMemOUT->eError != PVRSRV_OK)
		return 0;
	OSMemSet(&psAllocDeviceMemOUT->sClientMemInfo, 0,
			sizeof(psAllocDeviceMemOUT->sClientMemInfo));
	psAllocDeviceMemOUT->sClientMemInfo.pvLinAddrKM = psMemInfo->pvLinAddrKM;
	psAllocDeviceMemOUT->sClientMemInfo.pvLinAddr = psMemInfo->pvLinAddr;
	psAllocDeviceMemOUT->sClientMemInfo.sDevVAddr = psMemInfo->sDevVAddr;
	psAllocDeviceMemOUT->sClientMemInfo.ui32Flags = psMemInfo->ui32Flags;
	psAllocDeviceMemOUT->sClientMemInfo.uAllocSize = psMemInfo->uAllocSize;
	PVRSRVAllocHandle(psPerProc->psHandleBase,
			&psAllocDeviceMemOUT->sClientMemInfo.hKernelMemInfo, psMemInfo, 5, 0);
	if (!(psAllocDeviceMemIN->ui32Attribs & 8))
		SceDebugForDriver_391B5B74(&dbg_PVRSRVAllocDeviceMemBW_L816,
				__builtin_return_address(0));
	psPerProc->bHandlesBatched = IMG_FALSE;
	psAllocDeviceMemOUT->eError = PVRSRVCommitHandleBatch(psPerProc->psHandleBase);
	return 0;
}

typedef struct PVRSRV_BRIDGE_IN_CREATE_DEVMEMCONTEXT {
	IMG_UINT32 ui32BridgeFlags;
	IMG_SID hDevCookie;
} PVRSRV_BRIDGE_IN_CREATE_DEVMEMCONTEXT;

typedef struct PVRSRV_BRIDGE_OUT_CREATE_DEVMEMCONTEXT {
	PVRSRV_ERROR eError;
	IMG_SID hDevMemContext;
	IMG_UINT32 ui32ClientHeapCount;
	PVRSRV_HEAP_INFO sHeapInfo[32];
} PVRSRV_BRIDGE_OUT_CREATE_DEVMEMCONTEXT;

/* IMG context bridge with SCE native heap descriptors and assertions disabled. */
IMG_INT PVRSRVCreateDeviceMemContextBW(IMG_UINT32 ui32BridgeID,
		PVRSRV_BRIDGE_IN_CREATE_DEVMEMCONTEXT *psCreateDevMemContextIN,
		PVRSRV_BRIDGE_OUT_CREATE_DEVMEMCONTEXT *psCreateDevMemContextOUT,
		PVRSRV_PER_PROCESS_DATA *psPerProc)
{
	PVRSRV_DEVICE_NODE *psDeviceNode;
	GpuMemoryContext *context;
	IMG_BOOL bCreated;
	IMG_UINT32 i;

	psCreateDevMemContextOUT->eError = NewHandleBatch(psPerProc, 33);
	if (psCreateDevMemContextOUT->eError != PVRSRV_OK)
		return 0;
	psCreateDevMemContextOUT->eError = PVRSRVLookupHandle(psPerProc->psHandleBase,
			(IMG_VOID **)&psDeviceNode, psCreateDevMemContextIN->hDevCookie, 2);
	if (psCreateDevMemContextOUT->eError != PVRSRV_OK)
		return 0;
	psCreateDevMemContextOUT->eError = PVRSRVCreateDeviceMemContextKM(psDeviceNode,
			psPerProc, &context, &psCreateDevMemContextOUT->ui32ClientHeapCount,
			psCreateDevMemContextOUT->sHeapInfo, &bCreated, abSharedDeviceMemHeap);
	if (psCreateDevMemContextOUT->eError != PVRSRV_OK)
		return 0;
	if (bCreated) {
		PVRSRVAllocHandle(psPerProc->psHandleBase,
				&psCreateDevMemContextOUT->hDevMemContext, context, 3, 0);
	} else {
		psCreateDevMemContextOUT->eError = PVRSRVFindHandle(psPerProc->psHandleBase,
				&psCreateDevMemContextOUT->hDevMemContext, context, 3);
		if (psCreateDevMemContextOUT->eError != PVRSRV_OK)
			return 0;
	}
	for (i = 0; i < psCreateDevMemContextOUT->ui32ClientHeapCount; i++) {
		IMG_SID hDevMemHeapExt;

		if (abSharedDeviceMemHeap[i]) {
			PVRSRVAllocHandle(psPerProc->psHandleBase, &hDevMemHeapExt,
					psCreateDevMemContextOUT->sHeapInfo[i].hDevMemHeap, 4, 1);
		} else if (bCreated) {
			PVRSRVAllocSubHandle(psPerProc->psHandleBase, &hDevMemHeapExt,
					psCreateDevMemContextOUT->sHeapInfo[i].hDevMemHeap, 4, 0,
					psCreateDevMemContextOUT->hDevMemContext);
		} else {
			psCreateDevMemContextOUT->eError = PVRSRVFindHandle(psPerProc->psHandleBase,
					&hDevMemHeapExt,
					psCreateDevMemContextOUT->sHeapInfo[i].hDevMemHeap, 4);
			if (psCreateDevMemContextOUT->eError != PVRSRV_OK)
				return 0;
		}
		psCreateDevMemContextOUT->sHeapInfo[i].hDevMemHeap = (IMG_HANDLE)hDevMemHeapExt;
	}
	psPerProc->bHandlesBatched = IMG_FALSE;
	psCreateDevMemContextOUT->eError = PVRSRVCommitHandleBatch(psPerProc->psHandleBase);
	return 0;
}

/* IMG table setter with SCE compact entries and bridge diagnostics disabled. */
IMG_VOID _SetDispatchTableEntry(IMG_UINT32 ui32Index, const IMG_CHAR *pszIOCName,
		BridgeWrapperFunction pfFunction, const IMG_CHAR *pszFunctionName)
{
	g_BridgeDispatchTable[ui32Index].pfFunction = pfFunction;
	ui32PrevIndex = ui32Index;
}

/* SCE wait-registry callback; no Sony debug name is available. */
IMG_INT sceGpuWaitModObjSignal(IMG_INT waitEventFlag,
		MODIFY_SYNC_OP_INFO *psModSyncOpInfo)
{
	if (waitEventFlag > 0 && psModSyncOpInfo)
		return sceKernelSetEventFlag(waitEventFlag, psModSyncOpInfo->ui32WaitState);
	return (IMG_INT)0x804c0000;
}

#if FW_DEBUG
static const SceKernelDebugInfo dbg_sceGpuModObjPoll_L5248 = {
	0x1399193b, 0xcf67515a, 0xcddd55c2,
	"bridged/bridged_pvr_bridge.c", 5248, "sceGpuModObjPoll"
};
#endif

/* SCE polls all populated slots of its multi-sync modification object. */
IMG_INT sceGpuModObjPoll(MODIFY_SYNC_OP_INFO *psModSyncOpInfo)
{
	IMG_INT i, completed = 0;
	IMG_INT result = (IMG_INT)0x804c0007;

	if (!psModSyncOpInfo)
		return (IMG_INT)0x804c0000;
	if ((IMG_INT)psModSyncOpInfo->ui32NumSyncs <= 0)
		return (IMG_INT)0x804c000c;
	for (i = 0; i < (IMG_INT)psModSyncOpInfo->ui32NumSyncs; i++) {
		if (!psModSyncOpInfo->apsKernelSyncInfo[i])
			continue;
		if (DoQuerySyncOpsSatisfied(psModSyncOpInfo->apsKernelSyncInfo[i],
				psModSyncOpInfo->aui32ReadOpsPendingSnapShot[i],
				psModSyncOpInfo->aui32WriteOpsPendingSnapShot[i]) != PVRSRV_OK) {
			result = (IMG_INT)0x804c0006;
			break;
		}
		completed++;
	}
	if (completed == psModSyncOpInfo->ui32NumSyncs)
		result = 0;
#if FW_DEBUG
	SceDebugForDriver_35A35322(result != (IMG_INT)0x804c0007,
			&dbg_sceGpuModObjPoll_L5248, __builtin_return_address(0));
#endif
	return result;
}

/* IMG initializer with SCE's reduced bridge set and compact dispatch entries. */
PVRSRV_ERROR CommonBridgeInit(IMG_VOID)
{
	IMG_UINT32 i;

#define PVR_SET_BRIDGE(id, wrapper) do { \
	g_BridgeDispatchTable[id].pfFunction = (BridgeWrapperFunction)wrapper; \
	ui32PrevIndex = id; \
} while (0)
	PVR_SET_BRIDGE(0, PVRSRVEnumerateDevicesBW);
	PVR_SET_BRIDGE(1, PVRSRVAcquireDeviceDataBW);
	PVR_SET_BRIDGE(2, DummyBW);
	PVR_SET_BRIDGE(3, PVRSRVCreateDeviceMemContextBW);
	PVR_SET_BRIDGE(4, PVRSRVDestroyDeviceMemContextBW);
	PVR_SET_BRIDGE(5, PVRSRVGetDeviceMemHeapInfoBW);
	PVR_SET_BRIDGE(6, PVRSRVAllocDeviceMemBW);
	PVR_SET_BRIDGE(7, PVRSRVFreeDeviceMemBW);
	PVR_SET_BRIDGE(12, PVRSRVConnectBW);
	PVR_SET_BRIDGE(13, PVRSRVDisconnectBW);
	PVR_SET_BRIDGE(29, PVRSRVMapMemoryToGpuBW);
	PVR_SET_BRIDGE(30, PVRSRVUnmapMemoryFromGpuBW);
	PVR_SET_BRIDGE(42, PVRSRVGetMiscInfoBW);
	PVR_SET_BRIDGE(43, DummyBW);
	PVR_SET_BRIDGE(77, PVRSRVCreateSyncInfoModObjBW);
	PVR_SET_BRIDGE(78, PVRSRVDestroySyncInfoModObjBW);
	PVR_SET_BRIDGE(79, PVRSRVModifyPendingSyncOpsBW);
	PVR_SET_BRIDGE(80, PVRSRVModifyCompleteSyncOpsBW);
	PVR_SET_BRIDGE(87, PVRSRVCheckSyncInfoModObjBW);
	PVR_SET_BRIDGE(85, PVRSRVAllocSyncInfoBW);
	PVR_SET_BRIDGE(86, PVRSRVFreeSyncInfoBW);
	PVR_SET_BRIDGE(31, PVRSRVCreateSharedSyncInfoBW);
	PVR_SET_BRIDGE(32, PVRSRVOpenSharedSyncInfoBW);
	PVR_SET_BRIDGE(33, PVRSRVCloseSharedSyncInfoBW);
	PVR_SET_BRIDGE(34, PVRSRVDeleteSharedSyncInfoBW);
	PVR_SET_BRIDGE(88, PVRSRVRegisterMemBlockBW);
	PVR_SET_BRIDGE(89, PVRSRVUnregisterMemBlockBW);
	PVR_SET_BRIDGE(91, PVRSRVCheckGpuMappingBW);
	PVR_SET_BRIDGE(92, PVRSRVGetMappedMemoryInfoBW);
#undef PVR_SET_BRIDGE
	SetSGXDispatchTableEntry();
	for (i = 0; i < 148; i++) {
		if (!g_BridgeDispatchTable[i].pfFunction)
			g_BridgeDispatchTable[i].pfFunction = DummyBW;
	}
	return PVRSRV_OK;
}

/* SCE bridge teardown is empty; Sony's function name is unavailable. */
IMG_VOID sceGpuBridgeDeInit(IMG_VOID)
{
}

/* IMG dispatcher, with SCE compact table and four-argument copy wrappers.
 * SCE ignores the copy-out result and rejects any nonzero bridge result.
 */
IMG_INT BridgedDispatchKM(PVRSRV_PER_PROCESS_DATA *psPerProc,
		PVRSRV_BRIDGE_PACKAGE *psBridgePackageKM)
{
	IMG_VOID *psBridgeIn;
	IMG_VOID *psBridgeOut;
	IMG_UINT32 ui32BridgeID = psBridgePackageKM->ui32BridgeID;
	IMG_INT err = -14;

	if (!psPerProc->bInitProcess) {
		if (PVRSRVGetInitServerState(1)) {
			if (!PVRSRVGetInitServerState(2))
				goto return_fault;
		} else {
			if (PVRSRVGetInitServerState(0))
				goto return_fault;
			switch (ui32BridgeID) {
			case 12:
			case 13:
			case 72:
			case 73:
				break;
			default:
				goto return_fault;
			}
		}
	}
	psBridgeIn = gpsSysData->pvEnvSpecificData->pvBridgeData;
	psBridgeOut = (IMG_CHAR *)psBridgeIn + 4096;
	if (psBridgePackageKM->ui32InBufferSize > 4096 ||
			psBridgePackageKM->ui32OutBufferSize > 4096)
		goto return_fault;
	if (psBridgePackageKM->ui32InBufferSize > 0) {
		OSAccessOK(1, psBridgePackageKM->pvParamIn,
				psBridgePackageKM->ui32InBufferSize);
		if (CopyFromUserWrapper(psPerProc, psBridgeIn,
				psBridgePackageKM->pvParamIn,
				psBridgePackageKM->ui32InBufferSize) != PVRSRV_OK)
			goto return_fault;
	}
	if (ui32BridgeID >= 148)
		goto return_fault;
	err = g_BridgeDispatchTable[ui32BridgeID].pfFunction(ui32BridgeID,
			psBridgeIn, psBridgeOut, psPerProc);
	if (err == 0)
		CopyToUserWrapper(psPerProc, psBridgePackageKM->pvParamOut,
				psBridgeOut, psBridgePackageKM->ui32OutBufferSize);
return_fault:
	if (psPerProc->bHandlesBatched) {
		IMG_HANDLE psHandleBase = psPerProc->psHandleBase;

		psPerProc->bHandlesBatched = IMG_FALSE;
		PVRSRVReleaseHandleBatch(psHandleBase);
	}
	return err;
}
