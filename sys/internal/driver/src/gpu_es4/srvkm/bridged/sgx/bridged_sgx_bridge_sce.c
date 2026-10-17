/* SCE bridge extensions; upstream IMG sources remain in contrib. */
#include "../../sce_ddk_port.h"
#include "../../perproc_sce.h"
#include "../../device_sce.h"
#include "../../transfer_sce.h"
#include "../../render_sce.h"
#include "../../memory_sce.h"
#include "../../kick_sce.h"
#include "../../misc_sce.h"
#include <sysmem/debug.h>
#include <gpucoredump_es4/gpucoredump_es4_imports.h>
#include <string.h>
#include <psp2kern/kernel/sysmem/data_transfers.h>
#include <threadmgr/threadmgr.h>

typedef struct SGX_BRIDGE_OUT_TRACE_COUNTERS {
	PVRSRV_ERROR eError;
	GpuTraceCounters counters;
} SGX_BRIDGE_OUT_TRACE_COUNTERS;

typedef struct SGX_BRIDGE_OUT_TRACE_INFO {
	PVRSRV_ERROR eError;
	GpuTraceInfo info;
} SGX_BRIDGE_OUT_TRACE_INFO;

PVRSRV_ERROR DestroyCommandContext(IMG_VOID *pvParam, IMG_UINT32 ui32Param);
PVRSRV_ERROR PVRSRVLookupHandle(struct _PVRSRV_HANDLE_BASE_ *psBase, IMG_VOID **ppvData,
			     IMG_SID hHandle, IMG_UINT32 eType);
PVRSRV_ERROR PVRSRVReleaseHandle(struct _PVRSRV_HANDLE_BASE_ *psBase, IMG_SID hHandle,
			      IMG_UINT32 eType);
PVRSRV_ERROR ResManFreeResByPtr(struct Resource *resource, IMG_BOOL forceCleanup);
struct Resource *ResManRegisterRes(struct ResourceContext *context,
		IMG_UINT32 resourceType, IMG_VOID *param, IMG_UINT32 value,
		PVRSRV_ERROR (*callback)(IMG_VOID *, IMG_UINT32));

typedef struct PVRSRV_BRIDGE_IN_RELEASECLIENTINFO {
	IMG_UINT32 ui32BridgeFlags;
	IMG_SID hDevCookie;
	SGX_CLIENT_INFO sClientInfo;
} PVRSRV_BRIDGE_IN_RELEASECLIENTINFO;

typedef struct SGX_BRIDGE_IN_TRANSFER_COMMANDS {
	IMG_UINT32 ui32BridgeFlags;
	IMG_SID hDevCookie;
	IMG_UINT32 commands[26];
	IMG_UINT32 numCommands;
	union {
		IMG_SID hContext;
		struct GpuTransferQueueContext *context;
	} transferQueue;
	IMG_SID syncHandle;
	IMG_BOOL sourceSyncEnable;
	IMG_BOOL destinationSyncEnable;
	GpuTransferStatus status;
	IMG_BOOL statusEnabled;
} SGX_BRIDGE_IN_TRANSFER_COMMANDS;

typedef struct SGX_BRIDGE_IN_DESTROY_TRANSFER_QUEUE {
	IMG_UINT32 ui32BridgeFlags;
	IMG_SID hContext;
	IMG_BOOL forceCleanup;
} SGX_BRIDGE_IN_DESTROY_TRANSFER_QUEUE;

typedef struct SGX_BRIDGE_IN_CREATE_TRANSFER_QUEUE {
	IMG_UINT32 ui32BridgeFlags;
	IMG_SID hDevCookie;
	IMG_UINT32 reservedParameter;
	GpuTransferMemoryContext memoryContext;
} SGX_BRIDGE_IN_CREATE_TRANSFER_QUEUE;

typedef struct SGX_BRIDGE_OUT_CREATE_TRANSFER_QUEUE {
	PVRSRV_ERROR eError;
	IMG_SID hContext;
} SGX_BRIDGE_OUT_CREATE_TRANSFER_QUEUE;

typedef struct SGX_BRIDGE_IN_REMOVE_RENDER_TARGET {
	IMG_UINT32 ui32BridgeFlags;
	IMG_SID hDevCookie;
	union {
		IMG_SID hRenderContext;
		struct SGX_RENDERCONTEXT *renderContext;
	} context;
	IMG_SID hRenderTarget;
} SGX_BRIDGE_IN_REMOVE_RENDER_TARGET;

typedef struct SGX_BRIDGE_IN_ADD_RENDER_TARGET {
	IMG_UINT32 ui32BridgeFlags;
	IMG_SID hDevCookie;
	SGX_ADDRENDTARG target;
} SGX_BRIDGE_IN_ADD_RENDER_TARGET;

typedef struct SGX_BRIDGE_OUT_ADD_RENDER_TARGET {
	PVRSRV_ERROR eError;
	IMG_SID hRenderTarget;
} SGX_BRIDGE_OUT_ADD_RENDER_TARGET;

typedef struct GpuRenderTargetResource {
	PVRSRV_DEVICE_NODE *psDeviceNode;
	struct SGX_RENDERCONTEXT *psRenderContext;
	SGX_RTDATASET *psRTDataSet;
} GpuRenderTargetResource;

typedef struct SGX_BRIDGE_IN_DESTROY_RENDER_CONTEXT {
	IMG_UINT32 ui32BridgeFlags;
	IMG_SID hRenderContext;
	IMG_BOOL forceCleanup;
} SGX_BRIDGE_IN_DESTROY_RENDER_CONTEXT;

typedef struct SGX_BRIDGE_IN_CREATE_RENDER_CONTEXT {
	IMG_UINT32 ui32BridgeFlags;
	IMG_SID hDevCookie;
	SGX_CREATERENDERCONTEXT context;
} SGX_BRIDGE_IN_CREATE_RENDER_CONTEXT;

typedef struct SGX_BRIDGE_OUT_CREATE_RENDER_CONTEXT {
	PVRSRV_ERROR eError;
	IMG_SID hRenderContext;
} SGX_BRIDGE_OUT_CREATE_RENDER_CONTEXT;

typedef struct SGX_BRIDGE_IN_SET_RENDER_CONTEXT_PRIORITY {
	IMG_UINT32 ui32BridgeFlags;
	IMG_SID hRenderContext;
	IMG_UINT32 ui32Priority;
} SGX_BRIDGE_IN_SET_RENDER_CONTEXT_PRIORITY;

typedef struct SGX_BRIDGE_OUT_SET_RENDER_CONTEXT_PRIORITY {
	PVRSRV_ERROR eError;
	IMG_UINT32 ui32Priority;
} SGX_BRIDGE_OUT_SET_RENDER_CONTEXT_PRIORITY;

int ksceSblACMgrIsGameProgram(int pid);
PVRSRV_ERROR SGXSetRenderContextPriorityKM(IMG_HANDLE hHWRenderContext,
		IMG_UINT32 *priority);

typedef struct PVRSRV_BRIDGE_IN_SUBMIT2D {
	IMG_UINT32 ui32BridgeFlags;
	IMG_SID hDevCookie;
	PVRSRV_2D_SGX_KICK sKick;
} PVRSRV_BRIDGE_IN_SUBMIT2D;

PVRSRV_ERROR SGXSubmit2DKM(PVRSRV_DEVICE_NODE *psDeviceNode,
		PVRSRV_2D_SGX_KICK *psKick);

typedef struct PVRSRV_BRIDGE_IN_SGX_FLUSH_HW_RENDER_TARGET {
	IMG_UINT32 ui32BridgeFlags;
	IMG_SID hDevCookie;
	IMG_DEV_VIRTADDR sHWRTDataSetDevVAddr;
} PVRSRV_BRIDGE_IN_SGX_FLUSH_HW_RENDER_TARGET;

PVRSRV_ERROR SGXFlushHWRenderTargetKM(PVRSRV_DEVICE_NODE *psDeviceNode,
		IMG_DEV_VIRTADDR sHWRTDataSetDevVAddr, IMG_BOOL forceCleanup);

typedef struct PVRSRV_BRIDGE_IN_SGXGETMISCINFO {
	IMG_UINT32 ui32BridgeFlags;
	IMG_SID hDevCookie;
	SGX_MISC_INFO *psMiscInfo;
} PVRSRV_BRIDGE_IN_SGXGETMISCINFO;

PVRSRV_ERROR CopyFromUserWrapper(PVRSRV_PER_PROCESS_DATA *psPerProc,
		IMG_VOID *destination, const IMG_VOID *source, IMG_SIZE_T size);
PVRSRV_ERROR CopyToUserWrapper(PVRSRV_PER_PROCESS_DATA *psPerProc,
		IMG_VOID *destination, const IMG_VOID *source, IMG_SIZE_T size);
PVRSRV_ERROR SGXGetMiscInfoKM(PVRSRV_SGXDEV_INFO *psDevInfo,
		SGX_MISC_INFO *psMiscInfo, PVRSRV_DEVICE_NODE *psDeviceNode,
		struct BM_CONTEXT *memoryContext);

typedef struct PVRSRV_BRIDGE_IN_2DQUERYBLTSCOMPLETE {
	IMG_UINT32 ui32BridgeFlags;
	IMG_SID hDevCookie;
	IMG_SID hKernSyncInfo;
	IMG_BOOL bWaitForComplete;
} PVRSRV_BRIDGE_IN_2DQUERYBLTSCOMPLETE;

PVRSRV_ERROR SGX2DQueryBlitsCompleteKM(PVRSRV_SGXDEV_INFO *psDevInfo,
		struct PVRSRV_KERNEL_SYNC_INFO *psSyncInfo, IMG_BOOL bWaitForComplete);

typedef struct PVRSRV_BRIDGE_IN_GETINTERNALDEVINFO {
	IMG_UINT32 ui32BridgeFlags;
	IMG_SID hDevCookie;
} PVRSRV_BRIDGE_IN_GETINTERNALDEVINFO;

typedef struct PVRSRV_BRIDGE_OUT_GETINTERNALDEVINFO {
	SGX_INTERNAL_DEVINFO sSGXInternalDevInfo;
	PVRSRV_ERROR eError;
} PVRSRV_BRIDGE_OUT_GETINTERNALDEVINFO;

PVRSRV_ERROR SGXGetInternalDevInfoKM(PVRSRV_DEVICE_NODE *psDeviceNode,
		SGX_INTERNAL_DEVINFO *psInternalDevInfo);
IMG_UINT32 PVRSRVAllocHandle(struct _PVRSRV_HANDLE_BASE_ *base, IMG_SID *output,
		IMG_VOID *data, IMG_UINT32 type, IMG_UINT32 flags);

typedef struct PVRSRV_BRIDGE_IN_GETCLIENTINFO {
	IMG_UINT32 ui32BridgeFlags;
	IMG_SID hDevCookie;
} PVRSRV_BRIDGE_IN_GETCLIENTINFO;

typedef struct PVRSRV_BRIDGE_OUT_GETCLIENTINFO {
	SGX_CLIENT_INFO sClientInfo;
	PVRSRV_ERROR eError;
} PVRSRV_BRIDGE_OUT_GETCLIENTINFO;

PVRSRV_ERROR SGXGetClientInfoKM(PVRSRV_DEVICE_NODE *psDeviceNode,
		SGX_CLIENT_INFO *psClientInfo);

typedef struct SGX_BRIDGE_IN_GET_RENDER_TARGET_INFO {
	IMG_UINT32 ui32BridgeFlags;
	union {
		IMG_SID hRenderTarget;
		SGX_RTDATASET *renderTarget;
	} target;
} SGX_BRIDGE_IN_GET_RENDER_TARGET_INFO;

typedef struct SGX_BRIDGE_OUT_GET_RENDER_TARGET_INFO {
	PVRSRV_ERROR eError;
	IMG_UINT32 ui32NumPixelsX;
	IMG_UINT32 ui32NumPixelsY;
	IMG_UINT16 ui16MSAASamplesInX;
	IMG_UINT16 ui16MSAASamplesInY;
	IMG_BOOL bDPMZLS;
} SGX_BRIDGE_OUT_GET_RENDER_TARGET_INFO;

typedef struct SGX_BRIDGE_OUT_GET_RENDER_TARGET_DETAILS {
	PVRSRV_ERROR eError;
	IMG_UINT32 ui32NumPixelsX;
	IMG_UINT32 ui32NumPixelsY;
	IMG_UINT16 ui16MSAASamplesInX;
	IMG_UINT16 ui16MSAASamplesInY;
	IMG_UINT32 ui32BGObjUCoord;
	IMG_DEV_VIRTADDR sSpecialObjDevVAddr;
	IMG_UINT32 ui32SpecialObjSize;
	IMG_UINT32 ui32BGObjPtr1;
	IMG_UINT32 ui32NumRTData;
	IMG_UINT32 ui32MTEMultiSampleCtl;
	SGX_RTDATASET *psRTDataSet;
	IMG_UINT32 ui32MTileNumber;
	IMG_UINT32 ui32MTileX1;
	IMG_UINT32 ui32MTileX2;
	IMG_UINT32 ui32MTileX3;
	IMG_UINT32 ui32MTileY1;
	IMG_UINT32 ui32MTileY2;
	IMG_UINT32 ui32MTileY3;
	IMG_UINT32 ui32MTileStride;
	IMG_UINT32 ui32Flags;
} SGX_BRIDGE_OUT_GET_RENDER_TARGET_DETAILS;

typedef struct SGX_BRIDGE_OUT_GET_RENDER_TARGET_MEMBLOCK {
	PVRSRV_ERROR eError;
	int i32DataMemblockUID;
	IMG_BOOL bUseExternalUID;
} SGX_BRIDGE_OUT_GET_RENDER_TARGET_MEMBLOCK;

typedef struct SGX_BRIDGE_IN_WAIT_TRANSFER_QUEUE {
	IMG_UINT32 ui32BridgeFlags;
	union {
		IMG_SID hContext;
		GpuTransferQueueContext *transferQueue;
	} context;
} SGX_BRIDGE_IN_WAIT_TRANSFER_QUEUE;

PVRSRV_ERROR WaitTransferQueueIdle(GpuTransferQueueContext *context);

typedef struct PVRSRV_BRIDGE_IN_DOKICK {
	IMG_UINT32 ui32BridgeFlags;
	IMG_SID hDevCookie;
	SGX_KICKTA sKickTA;
} PVRSRV_BRIDGE_IN_DOKICK;

typedef struct PVRSRV_BRIDGE_OUT_DOKICK {
	PVRSRV_ERROR eError;
	SGX_KICKTA_OUTPUT output;
} PVRSRV_BRIDGE_OUT_DOKICK;

PVRSRV_ERROR SGXKickTA(PVRSRV_DEVICE_NODE *device, SGX_KICKTA *kick,
		SGX_KICKTA_RESULT *result, IMG_UINT32 flags,
		IMG_UINT32 unused);

PVRSRV_ERROR DestroyRenderContext(struct SGX_RENDERCONTEXT *psRenderContext,
		IMG_UINT32 ui32Param, IMG_BOOL forceCleanup);
PVRSRV_ERROR CreateRenderContext(PVRSRV_DEVICE_NODE *psDeviceNode,
		PVRSRV_PER_PROCESS_DATA *psPerProc, SGX_CREATERENDERCONTEXT *context,
		IMG_SID *hRenderContext, struct PVRSRV_KERNEL_MEM_INFO **visTestMemInfo);

PVRSRV_ERROR AddRenderTarget(PVRSRV_DEVICE_NODE *psDeviceNode,
		PVRSRV_PER_PROCESS_DATA *psPerProc, SGX_ADDRENDTARG *target,
		IMG_SID *hRenderTarget);
PVRSRV_ERROR RemoveRenderTargetCallback(IMG_VOID *pvParam, IMG_UINT32 ui32Param);
IMG_VOID *AllocProcessGpuMemory(IMG_SIZE_T size);
int FreeProcessGpuMemory(IMG_VOID *memory);

#if FW_DEBUG
static const SceKernelDebugInfo dbg_SGXRemoveRenderTargetBW_L291 = {
	0x8486887a, 0x9e414cbd, 0x270c958c,
	"bridged/sgx/bridged_sgx_bridge_sce.c", 291, "SGXRemoveRenderTargetBW"
};
static const SceKernelDebugInfo dbg_SGXRemoveRenderTargetBW_L293 = {
	0x8486887a, 0x983da090, 0x270c958c,
	"bridged/sgx/bridged_sgx_bridge_sce.c", 293, "SGXRemoveRenderTargetBW"
};
static const SceKernelDebugInfo dbg_SGXRemoveRenderTargetBW_L295 = {
	0x8486887a, 0xaf4b14c6, 0x270c958c,
	"bridged/sgx/bridged_sgx_bridge_sce.c", 295, "SGXRemoveRenderTargetBW"
};
#endif

PVRSRV_ERROR RemoveRenderTarget(PVRSRV_DEVICE_NODE *psDeviceNode,
		struct SGX_RENDERCONTEXT *psRenderContext, SGX_RTDATASET *psRTDataSet);

PVRSRV_ERROR CreateTransferQueue(PVRSRV_DEVICE_NODE *psDeviceNode,
		PVRSRV_PER_PROCESS_DATA *psPerProc, IMG_UINT32 reservedParameter,
		GpuTransferMemoryContext *memoryContext, IMG_SID *hContext);

PVRSRV_ERROR SubmitTransferCommands(IMG_UINT32 *commands, IMG_UINT32 numCommands,
		PVRSRV_DEVICE_NODE *psDeviceNode, PVRSRV_PER_PROCESS_DATA *psPerProc,
		struct GpuTransferQueueContext *context, IMG_VOID *syncInfo,
		IMG_BOOL sourceSyncEnable, IMG_BOOL destinationSyncEnable,
		GpuTransferStatus *status);

/* The command is registered but unsupported by the kernel driver. */
int SGXUnsupportedBW(IMG_UINT32 ui32BridgeID, IMG_VOID *psBridgeIn,
		     PVRSRV_BRIDGE_RETURN *psBridgeOut,
		     PVRSRV_PER_PROCESS_DATA *psPerProc)
{
	psBridgeOut->eError = PVRSRV_ERROR_NOT_SUPPORTED;
	return 0;
}

int SGXResetSyncWaitsBW(IMG_UINT32 ui32BridgeID, IMG_VOID *psBridgeIn,
		      PVRSRV_BRIDGE_RETURN *psBridgeOut,
		      PVRSRV_PER_PROCESS_DATA *psPerProc)
{
	IMG_UINT32 previousSubmit = psPerProc->lastSubmitUnlimited;
	int previousWasLimited;
	int reset;
	int currentUnlimited = SceThreadmgrForDriver_1096042E(0);

	previousWasLimited = previousSubmit == 0;
	if (currentUnlimited != 0)
		reset = !previousWasLimited;
	else
		reset = previousWasLimited;
	if (reset) {
		psPerProc->numSourceSyncs = 0;
		psPerProc->numDestinationSyncs = 0;
	}
	psBridgeOut->eError = PVRSRV_OK;
	return 0;
}

int SGXGetTraceCountersBW(IMG_UINT32 ui32BridgeID, IMG_VOID *psBridgeIn,
			SGX_BRIDGE_OUT_TRACE_COUNTERS *psBridgeOut,
			PVRSRV_PER_PROCESS_DATA *psPerProc)
{
	psBridgeOut->eError = SceGpuEs4ForDriver_C0C73C1B(&psBridgeOut->counters);
	return 0;
}

int SGXGetTraceInfoBW(IMG_UINT32 ui32BridgeID, IMG_VOID *psBridgeIn,
		     SGX_BRIDGE_OUT_TRACE_INFO *psBridgeOut,
		     PVRSRV_PER_PROCESS_DATA *psPerProc)
{
	psBridgeOut->eError = SceGpuEs4ForDriver_990338F6(&psBridgeOut->info);
	return 0;
}

/*
 * IMG SGXReleaseClientInfoBW, from the pristine vendored bridge source.
 * Copyright (C) Imagination Technologies Ltd.; GPL-2.0, vendor COPYING.
 */
int SGXReleaseClientInfoBW(IMG_UINT32 ui32BridgeID,
			 PVRSRV_BRIDGE_IN_RELEASECLIENTINFO *psReleaseClientInfoIN,
			 PVRSRV_BRIDGE_RETURN *psRetOUT,
			 PVRSRV_PER_PROCESS_DATA *psPerProc)
{
	PVRSRV_SGXDEV_INFO *psDevInfo;
	IMG_HANDLE hDevCookieInt;

	psRetOUT->eError = PVRSRVLookupHandle(psPerProc->psHandleBase,
					   &hDevCookieInt,
					   psReleaseClientInfoIN->hDevCookie, 2);
	if (psRetOUT->eError != PVRSRV_OK)
		return 0;
	psDevInfo = ((PVRSRV_DEVICE_NODE *)hDevCookieInt)->pvDevice;
	if (psDevInfo->ui32ClientRefCount > 0)
		psDevInfo->ui32ClientRefCount--;
	psRetOUT->eError = PVRSRV_OK;
	return 0;
}

int SGXSubmitTransferCommandsBW(IMG_UINT32 ui32BridgeID,
		SGX_BRIDGE_IN_TRANSFER_COMMANDS *psBridgeIn,
		PVRSRV_BRIDGE_RETURN *psBridgeOut, PVRSRV_PER_PROCESS_DATA *psPerProc)
{
	PVRSRV_DEVICE_NODE *psDeviceNode;
	IMG_VOID *syncInfo = 0;
	SGX_BRIDGE_IN_TRANSFER_COMMANDS input;
	int result = 0;

	if (psPerProc->processFlags != 0x1000000) {
		psBridgeOut->eError = PVRSRV_ERROR_NOT_SUPPORTED;
		goto out;
	}
	if (psPerProc->unmapInProgress) {
		psBridgeOut->eError = PVRSRV_ERROR_RETRY;
		result = PVRSRV_ERROR_RETRY;
		goto out;
	}
	psBridgeOut->eError = PVRSRVLookupHandle(psPerProc->psHandleBase,
			(IMG_VOID **)&psDeviceNode, psBridgeIn->hDevCookie, 2);
	if (psBridgeOut->eError != PVRSRV_OK)
		goto done;
	psBridgeOut->eError = PVRSRVLookupHandle(psPerProc->psHandleBase,
			(IMG_VOID **)&psBridgeIn->transferQueue.context,
			psBridgeIn->transferQueue.hContext, 26);
	if (psBridgeOut->eError != PVRSRV_OK)
		goto done;
	if (psBridgeIn->syncHandle) {
		psBridgeOut->eError = PVRSRVLookupHandle(psPerProc->psHandleBase,
				&syncInfo, psBridgeIn->syncHandle, 6);
		if (psBridgeOut->eError != PVRSRV_OK)
			goto done;
	}
	if (psBridgeIn->statusEnabled) {
		psBridgeOut->eError = PVRSRVLookupHandle(psPerProc->psHandleBase,
				(IMG_VOID **)&psBridgeIn->status.memory.memoryInfo,
				psBridgeIn->status.memory.memoryHandle, 5);
		if (psBridgeOut->eError != PVRSRV_OK)
			goto done;
	}
	memcpy(&input, psBridgeIn, sizeof(input));
	psBridgeOut->eError = SubmitTransferCommands(input.commands,
			input.numCommands, psDeviceNode, psPerProc,
			input.transferQueue.context, syncInfo, input.sourceSyncEnable,
			input.destinationSyncEnable, input.statusEnabled ? &input.status : 0);
done:
	result = 0;
out:
	return result;
}

PVRSRV_ERROR DestroyCommandContextCallback(IMG_VOID *pvParam, IMG_UINT32 ui32Param)
{
	DestroyCommandContext(pvParam, 0);
	return PVRSRV_OK;
}

int SGXDestroyTransferQueueBW(IMG_UINT32 ui32BridgeID,
		SGX_BRIDGE_IN_DESTROY_TRANSFER_QUEUE *psBridgeIn,
		PVRSRV_BRIDGE_RETURN *psBridgeOut, PVRSRV_PER_PROCESS_DATA *psPerProc)
{
	GpuTransferQueueContext *context = 0;
	IMG_SID hContext;

	psBridgeOut->eError = PVRSRVLookupHandle(psPerProc->psHandleBase,
			(IMG_VOID **)&context, psBridgeIn->hContext, 26);
	if (psBridgeOut->eError != PVRSRV_OK)
		return 0;
	hContext = psBridgeIn->hContext;
	if (context->contextResource)
		psBridgeOut->eError = ResManFreeResByPtr(context->contextResource,
						      psBridgeIn->forceCleanup);
	else
		psBridgeOut->eError = DestroyCommandContext((IMG_VOID *)hContext,
							  psBridgeIn->forceCleanup);
	if (psBridgeOut->eError == PVRSRV_OK)
		psBridgeOut->eError = PVRSRVReleaseHandle(psPerProc->psHandleBase,
						       hContext, 26);
	return 0;
}

int SGXCreateTransferQueueBW(IMG_UINT32 ui32BridgeID,
		SGX_BRIDGE_IN_CREATE_TRANSFER_QUEUE *psBridgeIn,
		SGX_BRIDGE_OUT_CREATE_TRANSFER_QUEUE *psBridgeOut,
		PVRSRV_PER_PROCESS_DATA *psPerProc)
{
	PVRSRV_DEVICE_NODE *psDeviceNode;
	GpuTransferQueueContext *context = 0;

	psBridgeOut->eError = PVRSRVLookupHandle(psPerProc->psHandleBase,
			(IMG_VOID **)&psDeviceNode, psBridgeIn->hDevCookie, 2);
	if (psBridgeOut->eError != PVRSRV_OK)
		return 0;
	psBridgeOut->eError = PVRSRVLookupHandle(psPerProc->psHandleBase,
			(IMG_VOID **)&psBridgeIn->memoryContext.memory.memoryContext,
			psBridgeIn->memoryContext.memory.hMemoryContext, 3);
	if (psBridgeOut->eError != PVRSRV_OK)
		return 0;
	psBridgeOut->eError = CreateTransferQueue(psDeviceNode, psPerProc,
			psBridgeIn->reservedParameter, &psBridgeIn->memoryContext,
			&psBridgeOut->hContext);
	psBridgeOut->eError = PVRSRVLookupHandle(psPerProc->psHandleBase,
			(IMG_VOID **)&context, psBridgeOut->hContext, 26);
	if (psBridgeOut->eError != PVRSRV_OK)
		DestroyCommandContext(context, 0);
	else
		context->contextResource = ResManRegisterRes(psPerProc->hResManContext,
				6, context, 0, DestroyCommandContextCallback);
	return 0;
}

int SGXRemoveRenderTargetBW(IMG_UINT32 ui32BridgeID,
		SGX_BRIDGE_IN_REMOVE_RENDER_TARGET *psBridgeIn,
		PVRSRV_BRIDGE_RETURN *psBridgeOut, PVRSRV_PER_PROCESS_DATA *psPerProc)
{
	PVRSRV_DEVICE_NODE *psDeviceNode;
	SGX_RTDATASET *psRTDataSet = 0;
	IMG_UINT32 flags;
	IMG_SID hRenderTarget;
#if FW_DEBUG
	const SceKernelDebugInfo *debugInfo;
	SceBool hasRenderTarget;
#endif

	psBridgeOut->eError = PVRSRVLookupHandle(psPerProc->psHandleBase,
			(IMG_VOID **)&psDeviceNode, psBridgeIn->hDevCookie, 2);
	if (psBridgeOut->eError != PVRSRV_OK)
		return 0;
	psBridgeOut->eError = PVRSRVLookupHandle(psPerProc->psHandleBase,
			(IMG_VOID **)&psBridgeIn->context.renderContext,
			psBridgeIn->context.hRenderContext, 24);
	if (psBridgeOut->eError != PVRSRV_OK)
		return 0;
	psBridgeOut->eError = PVRSRVLookupHandle(psPerProc->psHandleBase,
			(IMG_VOID **)&psRTDataSet, psBridgeIn->hRenderTarget, 25);
	if (psBridgeOut->eError != PVRSRV_OK)
		return 0;
	flags = psRTDataSet->ui32Flags;
#if FW_DEBUG
	if (flags & 4) {
		debugInfo = &dbg_SGXRemoveRenderTargetBW_L291;
		hasRenderTarget = psPerProc->numRenderTargets[1] != 0;
	} else if (flags & 8) {
		debugInfo = &dbg_SGXRemoveRenderTargetBW_L293;
		hasRenderTarget = psPerProc->numRenderTargets[2] != 0;
	} else {
		debugInfo = &dbg_SGXRemoveRenderTargetBW_L295;
		hasRenderTarget = psPerProc->numRenderTargets[0] != 0;
	}
	ksceKernelAssert(hasRenderTarget, debugInfo, __builtin_return_address(0));
#endif
	hRenderTarget = psBridgeIn->hRenderTarget;
	if (psRTDataSet->hResItem)
		psBridgeOut->eError = ResManFreeResByPtr(psRTDataSet->hResItem, IMG_FALSE);
	else
		psBridgeOut->eError = RemoveRenderTarget(psDeviceNode,
				psBridgeIn->context.renderContext, psRTDataSet);
	if (psBridgeOut->eError == PVRSRV_OK) {
		if (flags & 4)
			psPerProc->numRenderTargets[1]--;
		else if (flags & 8)
			psPerProc->numRenderTargets[2]--;
		else
			psPerProc->numRenderTargets[0]--;
		psBridgeOut->eError = PVRSRVReleaseHandle(psPerProc->psHandleBase,
						       hRenderTarget, 25);
	}
	return 0;
}

int SGXAddRenderTargetBW(IMG_UINT32 ui32BridgeID,
		SGX_BRIDGE_IN_ADD_RENDER_TARGET *psBridgeIn,
		SGX_BRIDGE_OUT_ADD_RENDER_TARGET *psBridgeOut,
		PVRSRV_PER_PROCESS_DATA *psPerProc)
{
	PVRSRV_DEVICE_NODE *psDeviceNode;
	SGX_RTDATASET *psRTDataSet = 0;
	SGX_ADDRENDTARG target;
	IMG_SID hRenderTarget;
	GpuRenderTargetResource *resource;
	IMG_UINT32 count;
	IMG_UINT32 limit;

	if ((psBridgeIn->target.ui32Flags & 0x180000) == 0x180000) {
		psBridgeOut->eError = PVRSRV_ERROR_INVALID_PARAMS;
		return 0;
	}
	if (psBridgeIn->target.ui32Flags & 0x80000) {
		count = psPerProc->numRenderTargets[1];
		limit = 3;
	} else if (psBridgeIn->target.ui32Flags & 0x100000) {
		count = psPerProc->numRenderTargets[2];
		limit = 1;
	} else {
		count = psPerProc->numRenderTargets[0];
		limit = 47;
	}
	if (count > limit) {
		psBridgeOut->eError = (PVRSRV_ERROR)184;
		return 0;
	}
	psBridgeOut->eError = PVRSRVLookupHandle(psPerProc->psHandleBase,
			(IMG_VOID **)&psDeviceNode, psBridgeIn->hDevCookie, 2);
	if (psBridgeOut->eError != PVRSRV_OK)
		return 0;
	psBridgeOut->eError = PVRSRVLookupHandle(psPerProc->psHandleBase,
			(IMG_VOID **)&psBridgeIn->target.context.renderContext,
			psBridgeIn->target.context.hRenderContext, 24);
	if (psBridgeOut->eError != PVRSRV_OK)
		return 0;
	memcpy(&target, &psBridgeIn->target, sizeof(target));
	psBridgeOut->eError = AddRenderTarget(psDeviceNode, psPerProc,
					    &target, &hRenderTarget);
	if (psBridgeOut->eError != PVRSRV_OK)
		return 0;
	psBridgeOut->hRenderTarget = hRenderTarget;
	if (target.ui32Flags & 0x80000)
		psPerProc->numRenderTargets[1]++;
	else if (target.ui32Flags & 0x100000)
		psPerProc->numRenderTargets[2]++;
	else
		psPerProc->numRenderTargets[0]++;
	psBridgeOut->eError = PVRSRVLookupHandle(psPerProc->psHandleBase,
			(IMG_VOID **)&psRTDataSet, hRenderTarget, 25);
	if (psBridgeOut->eError != PVRSRV_OK) {
		RemoveRenderTarget(psDeviceNode, target.context.renderContext,
				   (SGX_RTDATASET *)hRenderTarget);
	} else {
		resource = AllocProcessGpuMemory(sizeof(*resource));
		resource->psDeviceNode = psDeviceNode;
		resource->psRenderContext = target.context.renderContext;
		resource->psRTDataSet = psRTDataSet;
		psRTDataSet->hResItem = ResManRegisterRes(psPerProc->hResManContext,
				24, resource, 0, RemoveRenderTargetCallback);
		if (!psRTDataSet->hResItem)
			FreeProcessGpuMemory(resource);
	}
	return 0;
}

PVRSRV_ERROR RemoveRenderTargetCallback(IMG_VOID *pvParam, IMG_UINT32 ui32Param)
{
	GpuRenderTargetResource *resource = pvParam;
	PVRSRV_ERROR eError;

	eError = RemoveRenderTarget(resource->psDeviceNode,
			resource->psRenderContext, resource->psRTDataSet);
	if (eError == PVRSRV_OK)
		FreeProcessGpuMemory(resource);
	return eError;
}

int SGXDestroyRenderContextBW(IMG_UINT32 ui32BridgeID,
		SGX_BRIDGE_IN_DESTROY_RENDER_CONTEXT *psBridgeIn,
		PVRSRV_BRIDGE_RETURN *psBridgeOut, PVRSRV_PER_PROCESS_DATA *psPerProc)
{
	struct SGX_RENDERCONTEXT *psRenderContext = 0;
	IMG_SID hRenderContext;

	psBridgeOut->eError = PVRSRVLookupHandle(psPerProc->psHandleBase,
			(IMG_VOID **)&psRenderContext, psBridgeIn->hRenderContext, 24);
	if (psBridgeOut->eError != PVRSRV_OK)
		return 0;
	hRenderContext = psBridgeIn->hRenderContext;
	if (psRenderContext->hResItem)
		psBridgeOut->eError = ResManFreeResByPtr(psRenderContext->hResItem, IMG_FALSE);
	else
		psBridgeOut->eError = DestroyRenderContext(psRenderContext, 0,
							 psBridgeIn->forceCleanup);
	if (psBridgeOut->eError == PVRSRV_OK)
		psBridgeOut->eError = PVRSRVReleaseHandle(psPerProc->psHandleBase,
						       hRenderContext, 24);
	return 0;
}

PVRSRV_ERROR DestroyRenderContextCallback(IMG_VOID *pvParam, IMG_UINT32 ui32Param)
{
	return DestroyRenderContext(pvParam, 0, IMG_FALSE);
}

int SGXCreateRenderContextBW(IMG_UINT32 ui32BridgeID,
		SGX_BRIDGE_IN_CREATE_RENDER_CONTEXT *psBridgeIn,
		SGX_BRIDGE_OUT_CREATE_RENDER_CONTEXT *psBridgeOut,
		PVRSRV_PER_PROCESS_DATA *psPerProc)
{
	PVRSRV_DEVICE_NODE *psDeviceNode;
	struct SGX_RENDERCONTEXT *psRenderContext = 0;
	SGX_CREATERENDERCONTEXT context;
	struct PVRSRV_KERNEL_MEM_INFO *visTestMemInfo;
	IMG_SID hRenderContext;

	psBridgeOut->eError = PVRSRVLookupHandle(psPerProc->psHandleBase,
			(IMG_VOID **)&psDeviceNode, psBridgeIn->hDevCookie, 2);
	if (psBridgeOut->eError != PVRSRV_OK)
		return 0;
	psBridgeOut->eError = PVRSRVLookupHandle(psPerProc->psHandleBase,
			(IMG_VOID **)&psBridgeIn->context.memory.memoryContext,
			psBridgeIn->context.memory.hDevMemContext, 3);
	if (psBridgeOut->eError != PVRSRV_OK)
		return 0;
	context = psBridgeIn->context;
	psBridgeOut->eError = CreateRenderContext(psDeviceNode, psPerProc,
					    &context, &hRenderContext, &visTestMemInfo);
	if (psBridgeOut->eError != PVRSRV_OK)
		return 0;
	psBridgeOut->eError = PVRSRVLookupHandle(psPerProc->psHandleBase,
			(IMG_VOID **)&psRenderContext, hRenderContext, 24);
	if (psBridgeOut->eError != PVRSRV_OK) {
		DestroyRenderContext(psRenderContext, 0, IMG_FALSE);
	} else {
		psRenderContext->hResItem = ResManRegisterRes(psPerProc->hResManContext,
				23, psRenderContext, 0, DestroyRenderContextCallback);
		psBridgeOut->hRenderContext = hRenderContext;
	}
	return 0;
}

/* SCE maps the public priority ordering and limits game-program requests. */
int SGXSetRenderContextPriorityBW(IMG_UINT32 ui32BridgeID,
		SGX_BRIDGE_IN_SET_RENDER_CONTEXT_PRIORITY *psBridgeIn,
		SGX_BRIDGE_OUT_SET_RENDER_CONTEXT_PRIORITY *psBridgeOut,
		PVRSRV_PER_PROCESS_DATA *psPerProc)
{
	struct SGX_RENDERCONTEXT *psRenderContext;
	IMG_UINT32 priority = psBridgeIn->ui32Priority;
	IMG_UINT32 hardwarePriority;

	if (!psBridgeIn->hRenderContext)
		goto invalid;
	if ((priority <= 1 || !ksceSblACMgrIsGameProgram(0)) && priority != 1) {
		if (priority == 0)
			hardwarePriority = 2;
		else if (priority == 2)
			hardwarePriority = 0;
		else
			goto invalid;
	} else {
		priority = 1;
		hardwarePriority = 1;
	}
	psBridgeOut->eError = PVRSRVLookupHandle(psPerProc->psHandleBase,
			(IMG_VOID **)&psRenderContext, psBridgeIn->hRenderContext, 24);
	if (psBridgeOut->eError != PVRSRV_OK)
		return 0;
	psBridgeOut->eError = SGXSetRenderContextPriorityKM(
			psRenderContext->hHWRenderContext, &hardwarePriority);
	if (psBridgeOut->eError != PVRSRV_OK)
		return 0;
	if (hardwarePriority == 1)
		priority = 1;
	else if (hardwarePriority == 0)
		priority = 2;
	else if (hardwarePriority == 2)
		priority = 0;
	psBridgeOut->ui32Priority = priority;
	return 0;
invalid:
	psBridgeOut->eError = PVRSRV_ERROR_INVALID_PARAMS;
	return 0;
}

/* Upstream handle translation, with SCE's bounded status-update extension. */
int SGXSubmit2DBW(IMG_UINT32 ui32BridgeID,
		PVRSRV_BRIDGE_IN_SUBMIT2D *psBridgeIn,
		PVRSRV_BRIDGE_RETURN *psBridgeOut, PVRSRV_PER_PROCESS_DATA *psPerProc)
{
	PVRSRV_DEVICE_NODE *psDeviceNode;
	PVRSRV_2D_SGX_KICK *psKick = &psBridgeIn->sKick;
	IMG_UINT32 i;

	psBridgeOut->eError = PVRSRVLookupHandle(psPerProc->psHandleBase,
			(IMG_VOID **)&psDeviceNode, psBridgeIn->hDevCookie, 2);
	if (psBridgeOut->eError != PVRSRV_OK)
		return 0;
	psBridgeOut->eError = PVRSRVLookupHandle(psPerProc->psHandleBase,
			(IMG_VOID **)&psKick->commandMemory.memoryInfo,
			psKick->commandMemory.hCCBMemInfo, 5);
	if (psBridgeOut->eError != PVRSRV_OK)
		return 0;
	if (psKick->hTASyncInfo.hSyncInfo) {
		psBridgeOut->eError = PVRSRVLookupHandle(psPerProc->psHandleBase,
				(IMG_VOID **)&psKick->hTASyncInfo.syncInfo,
				psKick->hTASyncInfo.hSyncInfo, 6);
		if (psBridgeOut->eError != PVRSRV_OK)
			return 0;
	}
	if (psKick->h3DSyncInfo.hSyncInfo) {
		psBridgeOut->eError = PVRSRVLookupHandle(psPerProc->psHandleBase,
				(IMG_VOID **)&psKick->h3DSyncInfo.syncInfo,
				psKick->h3DSyncInfo.hSyncInfo, 6);
		if (psBridgeOut->eError != PVRSRV_OK)
			return 0;
	}
	if (psKick->ui32NumSrcSync > 3)
		goto invalid;
	for (i = 0; i < psKick->ui32NumSrcSync; i++) {
		psBridgeOut->eError = PVRSRVLookupHandle(psPerProc->psHandleBase,
				(IMG_VOID **)&psKick->ahSrcSyncInfo[i].syncInfo,
				psKick->ahSrcSyncInfo[i].hSyncInfo, 6);
		if (psBridgeOut->eError != PVRSRV_OK)
			return 0;
	}
	if (psKick->hDstSyncInfo.hSyncInfo) {
		psBridgeOut->eError = PVRSRVLookupHandle(psPerProc->psHandleBase,
				(IMG_VOID **)&psKick->hDstSyncInfo.syncInfo,
				psKick->hDstSyncInfo.hSyncInfo, 6);
		if (psBridgeOut->eError != PVRSRV_OK)
			return 0;
	}
	if (psKick->ui32NumStatusVals > 1)
		goto invalid;
	for (i = 0; i < psKick->ui32NumStatusVals; i++) {
		GpuTransferStatus *status = &psKick->sStatusUpdate + i;

		psBridgeOut->eError = PVRSRVLookupHandle(psPerProc->psHandleBase,
				(IMG_VOID **)&status->memory.memoryInfo,
				status->memory.memoryHandle, 5);
		if (psBridgeOut->eError != PVRSRV_OK)
			return 0;
	}
	psBridgeOut->eError = SGXSubmit2DKM(psDeviceNode, psKick);
	return 0;
invalid:
	psBridgeOut->eError = PVRSRV_ERROR_INVALID_PARAMS;
	return 0;
}

/* Upstream IMG bridge body; SCE's debug command assertion is absent here. */
int SGXFlushHWRenderTargetBW(IMG_UINT32 ui32BridgeID,
		PVRSRV_BRIDGE_IN_SGX_FLUSH_HW_RENDER_TARGET *psBridgeIn,
		PVRSRV_BRIDGE_RETURN *psBridgeOut, PVRSRV_PER_PROCESS_DATA *psPerProc)
{
	PVRSRV_DEVICE_NODE *psDeviceNode;

	psBridgeOut->eError = PVRSRVLookupHandle(psPerProc->psHandleBase,
			(IMG_VOID **)&psDeviceNode, psBridgeIn->hDevCookie, 2);
	if (psBridgeOut->eError != PVRSRV_OK)
		return 0;
	psBridgeOut->eError = SGXFlushHWRenderTargetKM(psDeviceNode,
			psBridgeIn->sHWRTDataSetDevVAddr, IMG_FALSE);
	return 0;
}

/* Upstream IMG bridge body; SCE copy wrappers omit the unused bridge ID. */
int SGXGetMiscInfoBW(IMG_UINT32 ui32BridgeID,
		PVRSRV_BRIDGE_IN_SGXGETMISCINFO *psBridgeIn,
		PVRSRV_BRIDGE_RETURN *psBridgeOut, PVRSRV_PER_PROCESS_DATA *psPerProc)
{
	PVRSRV_DEVICE_NODE *psDeviceNode;
	PVRSRV_SGXDEV_INFO *psDevInfo;
	SGX_MISC_INFO miscInfo;

	psBridgeOut->eError = PVRSRVLookupHandle(psPerProc->psHandleBase,
			(IMG_VOID **)&psDeviceNode, psBridgeIn->hDevCookie, 2);
	if (psBridgeOut->eError != PVRSRV_OK)
		return 0;
	if (!psDeviceNode)
		return -14;
	psDevInfo = psDeviceNode->pvDevice;
	psBridgeOut->eError = CopyFromUserWrapper(psPerProc, &miscInfo,
			psBridgeIn->psMiscInfo, sizeof(miscInfo));
	if (psBridgeOut->eError != PVRSRV_OK)
		return -14;
	psBridgeOut->eError = SGXGetMiscInfoKM(psDevInfo, &miscInfo, psDeviceNode, 0);
	if (psBridgeOut->eError != PVRSRV_OK)
		return 0;
	psBridgeOut->eError = CopyToUserWrapper(psPerProc, psBridgeIn->psMiscInfo,
			&miscInfo, sizeof(miscInfo));
	if (psBridgeOut->eError != PVRSRV_OK)
		return -14;
	return 0;
}

/* Upstream IMG bridge body, with the command assertion compiled out. */
int SGX2DQueryBlitsCompleteBW(IMG_UINT32 ui32BridgeID,
		PVRSRV_BRIDGE_IN_2DQUERYBLTSCOMPLETE *psBridgeIn,
		PVRSRV_BRIDGE_RETURN *psBridgeOut, PVRSRV_PER_PROCESS_DATA *psPerProc)
{
	PVRSRV_DEVICE_NODE *psDeviceNode;
	struct PVRSRV_KERNEL_SYNC_INFO *psSyncInfo;

	psBridgeOut->eError = PVRSRVLookupHandle(psPerProc->psHandleBase,
			(IMG_VOID **)&psDeviceNode, psBridgeIn->hDevCookie, 2);
	if (psBridgeOut->eError != PVRSRV_OK)
		return 0;
	psBridgeOut->eError = PVRSRVLookupHandle(psPerProc->psHandleBase,
			(IMG_VOID **)&psSyncInfo, psBridgeIn->hKernSyncInfo, 6);
	if (psBridgeOut->eError != PVRSRV_OK)
		return 0;
	psBridgeOut->eError = SGX2DQueryBlitsCompleteKM(psDeviceNode->pvDevice,
			psSyncInfo, psBridgeIn->bWaitForComplete);
	return 0;
}

/* Upstream IMG bridge body; the kernel result is translated to a shared SID. */
int SGXGetInternalDevInfoBW(IMG_UINT32 ui32BridgeID,
		PVRSRV_BRIDGE_IN_GETINTERNALDEVINFO *psBridgeIn,
		PVRSRV_BRIDGE_OUT_GETINTERNALDEVINFO *psBridgeOut,
		PVRSRV_PER_PROCESS_DATA *psPerProc)
{
	PVRSRV_DEVICE_NODE *psDeviceNode;

	psBridgeOut->eError = PVRSRVLookupHandle(psPerProc->psHandleBase,
			(IMG_VOID **)&psDeviceNode, psBridgeIn->hDevCookie, 2);
	if (psBridgeOut->eError != PVRSRV_OK)
		return 0;
	psBridgeOut->eError = SGXGetInternalDevInfoKM(psDeviceNode,
			&psBridgeOut->sSGXInternalDevInfo);
	psBridgeOut->eError = PVRSRVAllocHandle(psPerProc->psHandleBase,
			&psBridgeOut->sSGXInternalDevInfo.hostControl.hHostCtlKernelMemInfoHandle,
			psBridgeOut->sSGXInternalDevInfo.hostControl.memoryInfo, 5, 1);
	return 0;
}

/* Upstream IMG bridge body, with the command assertion compiled out. */
int SGXGetClientInfoBW(IMG_UINT32 ui32BridgeID,
		PVRSRV_BRIDGE_IN_GETCLIENTINFO *psBridgeIn,
		PVRSRV_BRIDGE_OUT_GETCLIENTINFO *psBridgeOut,
		PVRSRV_PER_PROCESS_DATA *psPerProc)
{
	PVRSRV_DEVICE_NODE *psDeviceNode;

	psBridgeOut->eError = PVRSRVLookupHandle(psPerProc->psHandleBase,
			(IMG_VOID **)&psDeviceNode, psBridgeIn->hDevCookie, 2);
	if (psBridgeOut->eError != PVRSRV_OK)
		return 0;
	psBridgeOut->eError = SGXGetClientInfoKM(psDeviceNode, &psBridgeOut->sClientInfo);
	return 0;
}

/* SCE query; descriptive bridge name, original debug name unavailable. */
int SGXGetRenderTargetInfoBW(IMG_UINT32 ui32BridgeID,
		SGX_BRIDGE_IN_GET_RENDER_TARGET_INFO *psBridgeIn,
		SGX_BRIDGE_OUT_GET_RENDER_TARGET_INFO *psBridgeOut,
		PVRSRV_PER_PROCESS_DATA *psPerProc)
{
	SGX_RTDATASET *psRTDataSet;

	psBridgeOut->eError = PVRSRVLookupHandle(psPerProc->psHandleBase,
			(IMG_VOID **)&psBridgeIn->target.renderTarget,
			psBridgeIn->target.hRenderTarget, 25);
	if (psBridgeOut->eError != PVRSRV_OK)
		return 0;
	psRTDataSet = psBridgeIn->target.renderTarget;
	psBridgeOut->ui32NumPixelsX = psRTDataSet->ui32NumPixelsX;
	psBridgeOut->ui32NumPixelsY = psRTDataSet->ui32NumPixelsY;
	psBridgeOut->ui16MSAASamplesInX = psRTDataSet->ui16MSAASamplesInX;
	psBridgeOut->ui16MSAASamplesInY = psRTDataSet->ui16MSAASamplesInY;
	psBridgeOut->bDPMZLS = (psRTDataSet->ui32Flags & 0x10) != 0;
	return 0;
}

/* SCE query; descriptive bridge name, original debug name unavailable. */
int SGXGetRenderTargetDetailsBW(IMG_UINT32 ui32BridgeID,
		SGX_BRIDGE_IN_GET_RENDER_TARGET_INFO *psBridgeIn,
		SGX_BRIDGE_OUT_GET_RENDER_TARGET_DETAILS *psBridgeOut,
		PVRSRV_PER_PROCESS_DATA *psPerProc)
{
	SGX_RTDATASET *psRTDataSet;

	psBridgeOut->eError = PVRSRVLookupHandle(psPerProc->psHandleBase,
			(IMG_VOID **)&psBridgeIn->target.renderTarget,
			psBridgeIn->target.hRenderTarget, 25);
	if (psBridgeOut->eError != PVRSRV_OK)
		return 0;
	psRTDataSet = psBridgeIn->target.renderTarget;
	psBridgeOut->ui32NumPixelsX = psRTDataSet->ui32NumPixelsX;
	psBridgeOut->ui32NumPixelsY = psRTDataSet->ui32NumPixelsY;
	psBridgeOut->ui16MSAASamplesInX = psRTDataSet->ui16MSAASamplesInX;
	psBridgeOut->ui16MSAASamplesInY = psRTDataSet->ui16MSAASamplesInY;
	psBridgeOut->ui32BGObjUCoord = psRTDataSet->ui32BGObjUCoord;
	psBridgeOut->sSpecialObjDevVAddr = psRTDataSet->psSpecialObjMemInfo->sDevVAddr;
	psBridgeOut->ui32SpecialObjSize = 512;
	psBridgeOut->ui32NumRTData = psRTDataSet->ui32NumRTData;
	psBridgeOut->ui32MTEMultiSampleCtl = psRTDataSet->ui32MTEMultiSampleCtl;
	psBridgeOut->psRTDataSet = psRTDataSet;
	psBridgeOut->ui32MTileNumber = psRTDataSet->ui32MTileNumber;
	psBridgeOut->ui32MTileX1 = psRTDataSet->ui32MTileX1;
	psBridgeOut->ui32MTileX2 = psRTDataSet->ui32MTileX2;
	psBridgeOut->ui32MTileX3 = psRTDataSet->ui32MTileX3;
	psBridgeOut->ui32MTileY1 = psRTDataSet->ui32MTileY1;
	psBridgeOut->ui32MTileY2 = psRTDataSet->ui32MTileY2;
	psBridgeOut->ui32MTileY3 = psRTDataSet->ui32MTileY3;
	psBridgeOut->ui32MTileStride = psRTDataSet->ui32MTileStride;
	psBridgeOut->ui32Flags = psRTDataSet->ui32Flags;
	psBridgeOut->ui32BGObjPtr1 =
		psRTDataSet->psRTData[psRTDataSet->ui32RTDataSel].ui32BGObjPtr1;
	return 0;
}

/* SCE query; API vocabulary from the prototype debug records. */
int SGXGetRenderTargetDriverMemBlockBW(IMG_UINT32 ui32BridgeID,
		SGX_BRIDGE_IN_GET_RENDER_TARGET_INFO *psBridgeIn,
		SGX_BRIDGE_OUT_GET_RENDER_TARGET_MEMBLOCK *psBridgeOut,
		PVRSRV_PER_PROCESS_DATA *psPerProc)
{
	SGX_RTDATASET *psRTDataSet;

	psBridgeOut->eError = PVRSRVLookupHandle(psPerProc->psHandleBase,
			(IMG_VOID **)&psBridgeIn->target.renderTarget,
			psBridgeIn->target.hRenderTarget, 25);
	if (psBridgeOut->eError != PVRSRV_OK)
		return 0;
	psRTDataSet = psBridgeIn->target.renderTarget;
	psBridgeOut->i32DataMemblockUID = psRTDataSet->i32DataMemblockUID;
	psBridgeOut->bUseExternalUID = psRTDataSet->bUseExternalUID;
	return 0;
}

/* SCE bridge; waits for the queue's read and write positions to agree. */
int SGXWaitTransferQueueIdleBW(IMG_UINT32 ui32BridgeID,
		SGX_BRIDGE_IN_WAIT_TRANSFER_QUEUE *psBridgeIn,
		PVRSRV_BRIDGE_RETURN *psBridgeOut, PVRSRV_PER_PROCESS_DATA *psPerProc)
{
	if (psPerProc->processFlags != 0x1000000) {
		psBridgeOut->eError = PVRSRV_ERROR_NOT_SUPPORTED;
		return 0;
	}
	psBridgeOut->eError = PVRSRVLookupHandle(psPerProc->psHandleBase,
			(IMG_VOID **)&psBridgeIn->context.transferQueue,
			psBridgeIn->context.hContext, 26);
	if (psBridgeOut->eError != PVRSRV_OK)
		return 0;
	psBridgeOut->eError = WaitTransferQueueIdle(psBridgeIn->context.transferQueue);
	if (psBridgeOut->eError == PVRSRV_ERROR_RETRY)
		return PVRSRV_ERROR_RETRY;
	return 0;
}

/* SCE replaces the upstream CCB packet and retains user status records. */
int SGXDoKickInternal(PVRSRV_BRIDGE_IN_DOKICK *psBridgeIn,
		PVRSRV_BRIDGE_OUT_DOKICK *psBridgeOut, PVRSRV_PER_PROCESS_DATA *psPerProc)
{
	PVRSRV_DEVICE_NODE *psDeviceNode;
	struct PVRSRV_KERNEL_SYNC_INFO *psRenderSync;
	IMG_SID hRenderSync;
	SGX_KICKTA_RESULT result;
	SGX_KICKTA kick;
	PVRSRV_ERROR eError;
	IMG_UINT32 i;

	psBridgeOut->eError = PVRSRVLookupHandle(psPerProc->psHandleBase,
			(IMG_VOID **)&psDeviceNode, psBridgeIn->hDevCookie, 2);
	if (psBridgeOut->eError != PVRSRV_OK)
		return 0;
	psBridgeOut->eError = PVRSRVLookupHandle(psPerProc->psHandleBase,
			(IMG_VOID **)&psBridgeIn->sKickTA.context.renderContext,
			psBridgeIn->sKickTA.context.hRenderContext, 24);
	if (psBridgeOut->eError != PVRSRV_OK)
		return 0;
	psBridgeOut->eError = PVRSRVLookupHandle(psPerProc->psHandleBase,
			(IMG_VOID **)&psBridgeIn->sKickTA.target.renderTarget,
			psBridgeIn->sKickTA.target.hRTDataSet, 25);
	if (psBridgeOut->eError != PVRSRV_OK)
		return 0;
	for (i = 0; i < psBridgeIn->sKickTA.sKickTACommon.ui32NumTAStatusVals; i++) {
		result.sourceSyncs[i] = psBridgeIn->sKickTA.asTAStatusUpdate[i];
		psBridgeOut->eError = PVRSRVLookupHandle(psPerProc->psHandleBase,
				(IMG_VOID **)&psBridgeIn->sKickTA.asTAStatusUpdate[i].memory.memoryInfo,
				psBridgeIn->sKickTA.asTAStatusUpdate[i].memory.memoryHandle, 5);
		if (psBridgeOut->eError != PVRSRV_OK)
			return 0;
	}
	for (i = 0; i < psBridgeIn->sKickTA.sKickTACommon.ui32Num3DStatusVals; i++) {
		result.destinationSyncs[i] = psBridgeIn->sKickTA.as3DStatusUpdate[i];
		psBridgeOut->eError = PVRSRVLookupHandle(psPerProc->psHandleBase,
				(IMG_VOID **)&psBridgeIn->sKickTA.as3DStatusUpdate[i].memory.memoryInfo,
				psBridgeIn->sKickTA.as3DStatusUpdate[i].memory.memoryHandle, 5);
		if (psBridgeOut->eError != PVRSRV_OK)
			return 0;
	}
	if (psBridgeIn->sKickTA.renderSync.pahRenderSurfSyncInfo) {
		if (ksceKernelCopyFromUser(&hRenderSync,
				psBridgeIn->sKickTA.renderSync.pahRenderSurfSyncInfo,
				sizeof(hRenderSync)) < 0) {
			psBridgeOut->eError = PVRSRV_ERROR_INVALID_PARAMS;
			return 0;
		}
		psBridgeOut->eError = PVRSRVLookupHandle(psPerProc->psHandleBase,
				(IMG_VOID **)&psRenderSync, hRenderSync, 6);
		if (psBridgeOut->eError != PVRSRV_OK)
			return 0;
		psBridgeIn->sKickTA.renderSync.ppsRenderSurfSyncInfo = &psRenderSync;
	}
	if (psBridgeIn->sKickTA.transfer.hTQContext) {
		psBridgeOut->eError = PVRSRVLookupHandle(psPerProc->psHandleBase,
				(IMG_VOID **)&psBridgeIn->sKickTA.transfer.transferQueue,
				psBridgeIn->sKickTA.transfer.hTQContext, 26);
		if (psBridgeOut->eError != PVRSRV_OK)
			return 0;
	}
	memcpy(&kick, &psBridgeIn->sKickTA, sizeof(kick));
	eError = SGXKickTA(psDeviceNode, &kick, &result, 0, 0);
	psBridgeOut->eError = eError;
	psBridgeOut->output = result.output;
	if (eError != PVRSRV_OK) {
		if (eError == PVRSRV_ERROR_RETRY)
			return PVRSRV_ERROR_RETRY;
		return 0;
	}
	psPerProc->lastSubmitUnlimited = SceThreadmgrForDriver_1096042E(0);
	psPerProc->numSourceSyncs = kick.sKickTACommon.ui32NumTAStatusVals;
	for (i = 0; i != kick.sKickTACommon.ui32NumTAStatusVals; i++)
		psPerProc->sourceSyncs[i] = (GpuSyncWait) {
			result.sourceSyncs[i].statusAddress,
			result.sourceSyncs[i].statusValue,
			result.sourceSyncs[i].memory.memoryInfo
		};
	psPerProc->numDestinationSyncs = kick.sKickTACommon.ui32Num3DStatusVals;
	for (i = 0; i != kick.sKickTACommon.ui32Num3DStatusVals; i++)
		psPerProc->destinationSyncs[i] = (GpuSyncWait) {
			result.destinationSyncs[i].statusAddress,
			result.destinationSyncs[i].statusValue,
			result.destinationSyncs[i].memory.memoryInfo
		};
	return 0;
}

/* SCE validates the fixed status-array bounds before entering the bridge. */
int SGXDoKickBW(IMG_UINT32 ui32BridgeID, PVRSRV_BRIDGE_IN_DOKICK *psBridgeIn,
		PVRSRV_BRIDGE_OUT_DOKICK *psBridgeOut, PVRSRV_PER_PROCESS_DATA *psPerProc)
{
	if (psPerProc->unmapInProgress) {
		psBridgeOut->eError = PVRSRV_ERROR_RETRY;
		return PVRSRV_ERROR_RETRY;
	}
	if (psBridgeIn->sKickTA.sKickTACommon.ui32NumTAStatusVals > 32 ||
	    psBridgeIn->sKickTA.sKickTACommon.ui32Num3DStatusVals > 4) {
		psBridgeOut->eError = PVRSRV_ERROR_INVALID_PARAMS;
		return 0;
	}
	return SGXDoKickInternal(psBridgeIn, psBridgeOut, psPerProc);
}

typedef int (*BridgeWrapper)(IMG_UINT32 bridgeID, IMG_VOID *input,
		IMG_VOID *output, PVRSRV_PER_PROCESS_DATA *process);

int DummyBW(IMG_UINT32 ui32BridgeID, IMG_VOID *psBridgeIn,
		IMG_VOID *psBridgeOut, PVRSRV_PER_PROCESS_DATA *psPerProc);
void _SetDispatchTableEntry(IMG_UINT32 index, const IMG_CHAR *name,
		BridgeWrapper wrapper, const IMG_CHAR *wrapperName);

/* IMG dispatch initialization with SCE bridge IDs and extensions. */
void SetSGXDispatchTableEntry(void)
{
#define SGX_ENTRY(id, wrapper) \
	_SetDispatchTableEntry(id, "None", (BridgeWrapper)wrapper, "None")
	SGX_ENTRY(94, SGXGetClientInfoBW);
	SGX_ENTRY(95, SGXReleaseClientInfoBW);
	SGX_ENTRY(96, SGXGetInternalDevInfoBW);
	SGX_ENTRY(98, DummyBW);
	SGX_ENTRY(99, DummyBW);
	SGX_ENTRY(103, SGX2DQueryBlitsCompleteBW);
	SGX_ENTRY(104, DummyBW);
	SGX_ENTRY(108, SGXGetMiscInfoBW);
	SGX_ENTRY(115, SGXFlushHWRenderTargetBW);
	SGX_ENTRY(117, SGXSubmit2DBW);
	SGX_ENTRY(131, SGXSetRenderContextPriorityBW);
	SGX_ENTRY(132, SGXCreateRenderContextBW);
	SGX_ENTRY(133, SGXDestroyRenderContextBW);
	SGX_ENTRY(134, SGXAddRenderTargetBW);
	SGX_ENTRY(135, SGXRemoveRenderTargetBW);
	SGX_ENTRY(137, SGXGetRenderTargetDriverMemBlockBW);
	SGX_ENTRY(138, SGXUnsupportedBW);
	SGX_ENTRY(136, SGXGetRenderTargetDetailsBW);
	SGX_ENTRY(139, SGXGetRenderTargetInfoBW);
	SGX_ENTRY(140, SGXDoKickBW);
	SGX_ENTRY(141, SGXCreateTransferQueueBW);
	SGX_ENTRY(142, SGXDestroyTransferQueueBW);
	SGX_ENTRY(143, SGXSubmitTransferCommandsBW);
	SGX_ENTRY(144, SGXWaitTransferQueueIdleBW);
	SGX_ENTRY(145, SGXGetTraceInfoBW);
	SGX_ENTRY(146, SGXGetTraceCountersBW);
	SGX_ENTRY(147, SGXResetSyncWaitsBW);
#undef SGX_ENTRY
}
