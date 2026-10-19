/* Licensed IMG client middleware recovered from the approved TOOL originals.
 * The released kernel-only DDK does not contain this client component.
 */
#include <gpu/sce_ddk_port.h>
#include <gpu/device_sce.h>
#include "render_sce.h"
#include "client_pb.h"
#include "sync_sce.h"
#include <gpu/perproc_sce.h>
#include "transfer_sce.h"
#include "client_ccb.h"
#include <gpu/gpu-mmu.h>
#include "sgxtransfer_sce.h"
#include <stddef.h>
#include <string.h>
#include <kernel/error.h>
#include <threadmgr/threadmgr.h>

PVRSRV_ERROR ResManFreeResByPtr(struct Resource *resource,
		IMG_BOOL forceCleanup);
PVRSRV_ERROR SGXUnregisterHWRenderContextKM(IMG_HANDLE hHWRenderContext,
		IMG_BOOL bForceCleanup);
int DestroyCommandRing(struct GpuCommandRing *ring);
PVRSRV_ERROR PVRSRVFreeSyncInfoKM(PVRSRV_KERNEL_SYNC_INFO *psSyncInfo);
PVRSRV_ERROR PVRSRVFreeDeviceMemKM(IMG_HANDLE hDevCookie,
		PVRSRV_KERNEL_MEM_INFO *psMemInfo);
IMG_VOID DestroyPrivateParameterBuffer(SGX_CLIENTPBDESC *psClientPBDesc);
PVRSRV_ERROR UnreferenceClientParameterBuffer(SGX_CLIENTPBDESC *psClientPBDesc);
int FreeProcessGpuMemory(void *address);
extern int gPVRSRVLock;
IMG_UINT32 GetRemoteAllocProcess(void);
unsigned LinuxLockMutexForPid(int *psPVRSRVMutex, int pid);
int LinuxUnLockMutex(int *psPVRSRVMutex);
PVRSRV_ERROR SGXFlushHWRenderTargetKM(IMG_HANDLE psDeviceNode,
		IMG_DEV_VIRTADDR sHWRTDataSetDevVAddr, IMG_BOOL bForceCleanup);
IMG_VOID *AllocProcessGpuMemory(IMG_SIZE_T size);
IMG_VOID *OSMemSet(IMG_VOID *address, IMG_INT value, IMG_SIZE_T size);
struct Resource *ResManRegisterRes(struct ResourceContext *context,
		IMG_UINT32 resourceType, IMG_VOID *param, IMG_UINT32 value,
		PVRSRV_ERROR (*callback)(IMG_VOID *, IMG_UINT32, IMG_BOOL));
PVRSRV_ERROR PVRSRVAllocHandle(struct _PVRSRV_HANDLE_BASE_ *base,
		IMG_SID *output, IMG_VOID *data, IMG_UINT32 type, IMG_UINT32 flags);
PVRSRV_ERROR AllocateRenderTargetResources(PVRSRV_DEVICE_NODE *psDeviceNode,
		PVRSRV_PER_PROCESS_DATA *psPerProc, IMG_SID hMemBlockProcRef,
		struct SGX_RENDERCONTEXT *psRenderContext, SGX_RTDATASET *psRTDataSet);
PVRSRV_ERROR SGXUnregisterHW2DContextKM(IMG_HANDLE hHW2DContext,
		IMG_BOOL bForceCleanup);
extern SceUID exitingProcess;
extern IMG_UINT32 gpuSuspended;
int WaitSignalCounterKernel(volatile IMG_UINT32 *counter, IMG_UINT32 timeout);
PVRSRV_ERROR PVRSRVGetDeviceMemHeapInfoKM(PVRSRV_DEVICE_NODE *psDeviceNode,
		GpuMemoryContext *context, IMG_UINT32 *pui32ClientHeapCount,
		PVRSRV_HEAP_INFO *psHeapInfo, IMG_BOOL *pbSharedDeviceMemHeap);
PVRSRV_ERROR _PVRSRVAllocDeviceMemKM(PVRSRV_DEVICE_NODE *psDeviceNode,
		PVRSRV_PER_PROCESS_DATA *psPerProc, GpuHeap *heap,
		IMG_UINT32 ui32Attribs, IMG_SIZE_T ui32Size, IMG_SIZE_T ui32Alignment,
		IMG_INT32 hMemBlockProcRef, PVRSRV_KERNEL_MEM_INFO **ppsMemInfo);
PVRSRV_ERROR PVRSRVAllocSyncInfoKM(PVRSRV_DEVICE_NODE *psDeviceNode,
		GpuMemoryContext *hDevMemContext, PVRSRV_KERNEL_SYNC_INFO **ppsSyncInfo);
int CreatePrivateParameterBuffer(PVRSRV_DEVICE_NODE *psDeviceNode,
		PVRSRV_PER_PROCESS_DATA *psPerProc, IMG_INT32 hMemBlockProcRef,
		struct SGX_RENDERCONTEXT *psRenderContext, IMG_UINT32 ui32Size,
		PVRSRV_HEAP_INFO *psParameterHeap, PVRSRV_HEAP_INFO *psKernelHeap);
int CreateSharedParameterBuffer(PVRSRV_DEVICE_NODE *psDeviceNode,
		PVRSRV_PER_PROCESS_DATA *psPerProc, IMG_INT32 hMemBlockProcRef,
		IMG_HANDLE *phSharedPBDesc, IMG_UINT32 ui32Size,
		IMG_HANDLE hDevMemContext, PVRSRV_HEAP_INFO *psParameterHeap,
		PVRSRV_HEAP_INFO *psKernelHeap);
int InitializeSharedClientParameterBuffer(IMG_HANDLE hSharedPBDesc,
		PVRSRV_KERNEL_MEM_INFO *psSharedMemInfo,
		PVRSRV_KERNEL_MEM_INFO *psHWMemInfo,
		PVRSRV_KERNEL_MEM_INFO *psBlockMemInfo,
		PVRSRV_KERNEL_MEM_INFO *psHWBlockMemInfo,
		PVRSRV_KERNEL_MEM_INFO **ppsSubMemInfo, IMG_UINT32 ui32SubMemInfoCount,
		SGX_CLIENTPBDESC *psClientPBDesc);
PVRSRV_ERROR SGXFindSharedPBDescKM(PVRSRV_PER_PROCESS_DATA *psPerProc,
		IMG_HANDLE hDevCookie, IMG_BOOL bLockOnFailure,
		IMG_UINT32 ui32TotalPBSize, IMG_HANDLE *phSharedPBDesc,
		PVRSRV_KERNEL_MEM_INFO **ppsSharedPBDescKernelMemInfo,
		PVRSRV_KERNEL_MEM_INFO **ppsHWPBDescKernelMemInfo,
		PVRSRV_KERNEL_MEM_INFO **ppsBlockKernelMemInfo,
		PVRSRV_KERNEL_MEM_INFO **ppsHWBlockKernelMemInfo,
		PVRSRV_KERNEL_MEM_INFO ***pppsSharedPBDescSubKernelMemInfos,
		IMG_UINT32 *ui32SharedPBDescSubKernelMemInfosCount);
PVRSRV_ERROR SGXUnrefSharedPBDescKM(IMG_HANDLE hSharedPBDesc);
PVRSRV_ERROR CreateCommandRing(PVRSRV_DEVICE_NODE *psDeviceNode,
		PVRSRV_PER_PROCESS_DATA *psPerProc, IMG_INT32 hMemBlockProcRef,
		IMG_UINT32 ui32BufferSize, IMG_UINT32 ui32CommandAlignment,
		IMG_UINT32 ui32OverrunSize, GpuHeap *heap,
		struct GpuCommandRing **ppsRing);
PVRSRV_ERROR SGXGetInternalDevInfoKM(IMG_HANDLE hDevCookie,
		SGX_INTERNAL_DEVINFO *psSGXInternalDevInfo);
IMG_HANDLE SGXRegisterHWRenderContextKM(IMG_HANDLE hDeviceNode,
		IMG_DEV_VIRTADDR *psHWRenderContextDevVAddr,
		PVRSRV_KERNEL_MEM_INFO *psHWRenderContextMemInfo,
		PVRSRV_PER_PROCESS_DATA *psPerProc);
IMG_UINT32 OSGetCurrentProcessIDKM(IMG_VOID);
PVRSRV_ERROR FreeSyncInfoCallback(IMG_VOID *pvParam, IMG_UINT32 ui32Param);
IMG_HANDLE SGXRegisterHW2DContextKM(IMG_HANDLE hDeviceNode,
		IMG_DEV_VIRTADDR *psHW2DContextDevVAddr,
		PVRSRV_KERNEL_MEM_INFO *psHW2DContextMemInfo,
		PVRSRV_PER_PROCESS_DATA *psPerProc);
IMG_VOID *AcquireCommandRingSpace(struct GpuCommandRing *psRing,
		IMG_UINT32 ui32Size, volatile IMG_UINT32 *pui32SignalCounter);
PVRSRV_ERROR SGXSubmit2DKM(PVRSRV_DEVICE_NODE *psDeviceNode,
		PVRSRV_2D_SGX_KICK *psKick);
IMG_VOID *OSMemCopy(IMG_VOID *destination, const IMG_VOID *source,
		IMG_SIZE_T size);

PVRSRV_ERROR DestroyRenderContextMutexCallback(IMG_VOID *pvParam,
		IMG_UINT32 ui32Param, IMG_BOOL bForceCleanup)
{
	sceKernelDeleteMutex(ui32Param);
	return PVRSRV_OK;
}

PVRSRV_ERROR DestroyRenderTargetMutexCallback(IMG_VOID *pvParam,
		IMG_UINT32 ui32Param, IMG_BOOL bForceCleanup)
{
	sceKernelDeleteMutex(ui32Param);
	return PVRSRV_OK;
}

PVRSRV_ERROR DestroyTransferQueueMutexCallback(IMG_VOID *pvParam,
		IMG_UINT32 ui32Param, IMG_BOOL bForceCleanup)
{
	sceKernelDeleteMutex(ui32Param);
	return PVRSRV_OK;
}

PVRSRV_ERROR DestroyRenderContext(struct SGX_RENDERCONTEXT *psRenderContext,
		IMG_UINT32 ui32Param, IMG_BOOL bForceCleanup)
{
	PVRSRV_ERROR eError;
	IMG_UINT32 i;

	if (!psRenderContext)
		return PVRSRV_ERROR_INVALID_PARAMS;
	if (psRenderContext->psRTDataSets)
		return (PVRSRV_ERROR)186;
	if (psRenderContext->mutexResource)
		ResManFreeResByPtr(psRenderContext->mutexResource, IMG_FALSE);
	else
		sceKernelDeleteMutex(psRenderContext->hMutex);
	eError = SGXUnregisterHWRenderContextKM(psRenderContext->hHWRenderContext,
			bForceCleanup);
	if (eError != PVRSRV_OK)
		return eError;
	DestroyCommandRing(psRenderContext->psTACCB);
	if (psRenderContext->psTA3DSyncObject->hResItem)
		ResManFreeResByPtr(psRenderContext->psTA3DSyncObject->hResItem,
				IMG_FALSE);
	else
		PVRSRVFreeSyncInfoKM(psRenderContext->psTA3DSyncObject);
	for (i = 0; i < 4; i++)
		PVRSRVFreeDeviceMemKM(NULL,
				psRenderContext->psVDMSnapShotBufferMemInfo[i]);
	PVRSRVFreeDeviceMemKM(NULL, psRenderContext->psMasterVDMSnapShotBufferMemInfo);
	for (i = 0; i < 4; i++)
		PVRSRVFreeDeviceMemKM(NULL, psRenderContext->psMTEStateFlushMemInfo[i]);
	PVRSRVFreeDeviceMemKM(NULL, psRenderContext->psSABufferMemInfo);
	PVRSRVFreeDeviceMemKM(NULL, psRenderContext->psHWRenderContextMemInfo);
	if (psRenderContext->psVisTestResultMemInfo)
		PVRSRVFreeDeviceMemKM(NULL, psRenderContext->psVisTestResultMemInfo);
	if (psRenderContext->bPerContextPB)
		DestroyPrivateParameterBuffer(psRenderContext->psClientPBDesc);
	if (!psRenderContext->bPerContextPB)
		UnreferenceClientParameterBuffer(psRenderContext->psClientPBDesc);
	FreeProcessGpuMemory(psRenderContext->psClientPBDesc);
	FreeProcessGpuMemory(psRenderContext);
	return PVRSRV_OK;
}

PVRSRV_ERROR CreateRenderContext(PVRSRV_DEVICE_NODE *psDeviceNode,
		PVRSRV_PER_PROCESS_DATA *psPerProc, SGX_CREATERENDERCONTEXT *psCreate,
		IMG_SID *phRenderContext, PVRSRV_KERNEL_MEM_INFO **ppsVisTestMemInfo)
{
	IMG_HANDLE hSharedPBDesc = NULL, hNewSharedPBDesc;
	PVRSRV_KERNEL_MEM_INFO *psSharedMemInfo = NULL, *psHWMemInfo = NULL;
	PVRSRV_KERNEL_MEM_INFO *psBlockMemInfo = NULL, *psHWBlockMemInfo = NULL;
	IMG_UINT32 ui32SubMemInfoCount = 0;
	PVRSRV_KERNEL_MEM_INFO **ppsSubMemInfo = NULL;
	SGX_INTERNAL_DEVINFO sInternalDevInfo;
	IMG_BOOL abSharedHeap[32];
	PVRSRV_HEAP_INFO asHeapInfo[32];
	IMG_UINT32 ui32HeapCount, ui32Size, ui32SABufferSize, i;
	PVRSRV_HEAP_INFO *psHeap, *psParameterHeap = NULL;
	PVRSRV_HEAP_INFO *psGeneralHeap = NULL, *psKernelHeap = NULL;
	PVRSRV_HEAP_INFO *psSyncHeap = NULL;
	struct SGX_RENDERCONTEXT *psRenderContext;
	struct SGXMKIF_HWRENDERCONTEXT *psHWRenderContext;
	SGX_CLIENTPBDESC *psClientPBDesc = NULL;
	SGX_PBDESC *psPBDesc = NULL;
	PVRSRV_ERROR eError;

	memset(&sInternalDevInfo, 0, sizeof(sInternalDevInfo));
	eError = PVRSRVGetDeviceMemHeapInfoKM(psDeviceNode,
			psCreate->memory.memoryContext, &ui32HeapCount,
			asHeapInfo, abSharedHeap);
	if (eError != PVRSRV_OK)
		return eError;
	for (psHeap = asHeapInfo; psHeap != asHeapInfo + ui32HeapCount; psHeap++) {
		switch (psHeap->ui32HeapID & 0xffffff) {
		case 0:
			psGeneralHeap = psHeap;
			break;
		case 3:
			psKernelHeap = psHeap;
			break;
		case 8:
			psSyncHeap = psHeap;
			break;
		case 9:
			if (!psCreate->bPerContextPB && !(psCreate->ui32Flags & 2))
				psParameterHeap = psHeap;
			break;
		case 10:
			if (psCreate->bPerContextPB && !(psCreate->ui32Flags & 2))
				psParameterHeap = psHeap;
			break;
		case 16:
			if (psCreate->bPerContextPB && (psCreate->ui32Flags & 2))
				psParameterHeap = psHeap;
			break;
		case 17:
			if (!psCreate->bPerContextPB && (psCreate->ui32Flags & 2))
				psParameterHeap = psHeap;
			break;
		}
	}
	psRenderContext = AllocProcessGpuMemory(sizeof(*psRenderContext));
	if (!psRenderContext) {
		*phRenderContext = 0;
		return PVRSRV_ERROR_OUT_OF_MEMORY;
	}
	OSMemSet(psRenderContext, 0, sizeof(*psRenderContext));
	psRenderContext->hDevMemContext = psCreate->memory.memoryContext;
	psRenderContext->bCDRAMPB = (psCreate->ui32Flags & 2) != 0;
	if (!psParameterHeap)
		goto outOfMemory;
	psRenderContext->bPerContextPB = psCreate->bPerContextPB;
	ui32Size = psCreate->ui32PBSize;
	if (!psKernelHeap)
		goto invalidHeap;
	eError = _PVRSRVAllocDeviceMemKM(psDeviceNode, psPerProc,
			psKernelHeap->hDevMemHeap, 0x40004f,
			sizeof(*psHWRenderContext), 32, psCreate->hMemBlockProcRef,
			&psRenderContext->psHWRenderContextMemInfo);
	if (eError != PVRSRV_OK)
		goto outOfMemory;
	psHWRenderContext = psRenderContext->psHWRenderContextMemInfo->pvLinAddrKM;
	OSMemSet(psHWRenderContext, 0, sizeof(*psHWRenderContext));
	psRenderContext->psHWRenderContext = psHWRenderContext;
	psClientPBDesc = AllocProcessGpuMemory(sizeof(*psClientPBDesc));
	if (!psClientPBDesc)
		goto outOfMemory;
	if (ui32Size < 0x40000)
		ui32Size = 0x40000;
	OSMemSet(psClientPBDesc, 0, sizeof(*psClientPBDesc));
	ui32Size = (ui32Size + 4095) & ~4095U;
	psRenderContext->psClientPBDesc = psClientPBDesc;
	if (psRenderContext->bPerContextPB) {
		eError = CreatePrivateParameterBuffer(psDeviceNode, psPerProc,
				psCreate->hMemBlockProcRef, psRenderContext, ui32Size,
				psParameterHeap, psKernelHeap);
		if (eError != PVRSRV_OK)
			goto failure;
		psPBDesc = psClientPBDesc->psPBDesc;
	}
	if (!psRenderContext->bPerContextPB) {
		do {
			eError = SGXFindSharedPBDescKM(psPerProc, psDeviceNode, IMG_TRUE,
					ui32Size, &hSharedPBDesc, &psSharedMemInfo,
					&psHWMemInfo, &psBlockMemInfo, &psHWBlockMemInfo,
					&ppsSubMemInfo, &ui32SubMemInfoCount);
		} while (eError == PVRSRV_ERROR_PROCESSING_BLOCKED);
		if (eError != PVRSRV_OK)
			goto failure;
		if (!hSharedPBDesc) {
			if (!(psCreate->ui32Flags & 1))
				goto sharedBufferFailure;
			eError = CreateSharedParameterBuffer(psDeviceNode, psPerProc,
					psCreate->hMemBlockProcRef, &hNewSharedPBDesc,
					ui32Size, psCreate->memory.memoryContext,
					psParameterHeap, psKernelHeap);
			if (eError != PVRSRV_OK)
				goto failure;
			eError = SGXFindSharedPBDescKM(psPerProc, psDeviceNode, IMG_FALSE,
					ui32Size, &hSharedPBDesc, &psSharedMemInfo,
					&psHWMemInfo, &psBlockMemInfo, &psHWBlockMemInfo,
					&ppsSubMemInfo, &ui32SubMemInfoCount);
			SGXUnrefSharedPBDescKM(hNewSharedPBDesc);
			if (eError != PVRSRV_OK || !psSharedMemInfo)
				goto sharedBufferFailure;
		}
		eError = InitializeSharedClientParameterBuffer(hSharedPBDesc,
				psSharedMemInfo, psHWMemInfo, psBlockMemInfo,
				psHWBlockMemInfo, ppsSubMemInfo, ui32SubMemInfoCount,
				psClientPBDesc);
		FreeProcessGpuMemory(ppsSubMemInfo);
		if (eError != PVRSRV_OK)
			goto failure;
		psPBDesc = psClientPBDesc->psPBDesc;
		psPBDesc->ui32Control |= 1;
	}
	eError = CreateCommandRing(psDeviceNode, psPerProc,
			psCreate->hMemBlockProcRef, 0x10000, 64, 0x400,
			psKernelHeap->hDevMemHeap, &psRenderContext->psTACCB);
	if (eError != PVRSRV_OK)
		goto failure;
	if (!psSyncHeap)
		goto invalidHeap;
	eError = PVRSRVAllocSyncInfoKM(psDeviceNode, NULL,
			&psRenderContext->psTA3DSyncObject);
	if (eError != PVRSRV_OK)
		goto outOfMemory;
	psRenderContext->psTA3DSyncObject->hResItem = ResManRegisterRes(
			psPerProc->hResManContext, 20, psRenderContext->psTA3DSyncObject,
			0, (PVRSRV_ERROR (*)(IMG_VOID *, IMG_UINT32, IMG_BOOL))
			FreeSyncInfoCallback);
	eError = _PVRSRVAllocDeviceMemKM(psDeviceNode, psPerProc,
			psGeneralHeap->hDevMemHeap, 0x40000b, 496, 16,
			psCreate->hMemBlockProcRef,
			&psRenderContext->psMasterVDMSnapShotBufferMemInfo);
	if (eError != PVRSRV_OK)
		goto outOfMemory;
	OSMemSet(psRenderContext->psMasterVDMSnapShotBufferMemInfo->pvLinAddrKM, 0, 496);
	for (i = 0; i < 4; i++) {
		eError = _PVRSRVAllocDeviceMemKM(psDeviceNode, psPerProc,
				psGeneralHeap->hDevMemHeap, 0x40000b, 496, 16,
				psCreate->hMemBlockProcRef,
				&psRenderContext->psVDMSnapShotBufferMemInfo[i]);
		if (eError != PVRSRV_OK)
			goto outOfMemory;
		OSMemSet(psRenderContext->psVDMSnapShotBufferMemInfo[i]->pvLinAddrKM, 0, 496);
	}
	for (i = 0; i < 4; i++) {
		eError = _PVRSRVAllocDeviceMemKM(psDeviceNode, psPerProc,
				psGeneralHeap->hDevMemHeap, 0x40000b, 100, 16,
				psCreate->hMemBlockProcRef,
				&psRenderContext->psMTEStateFlushMemInfo[i]);
		if (eError != PVRSRV_OK)
			return eError;
		OSMemSet(psRenderContext->psMTEStateFlushMemInfo[i]->pvLinAddrKM, 0, 100);
	}
	ui32SABufferSize = 4 * ((psCreate->ui32MaxSACount + 16) & ~15U);
	eError = _PVRSRVAllocDeviceMemKM(psDeviceNode, psPerProc,
			psGeneralHeap->hDevMemHeap, 0x40000b, ui32SABufferSize, 1,
			psCreate->hMemBlockProcRef, &psRenderContext->psSABufferMemInfo);
	if (eError != PVRSRV_OK)
		return eError;
	OSMemSet(psRenderContext->psSABufferMemInfo->pvLinAddrKM, 0, ui32SABufferSize);
	if (psCreate->ui32VisTestResultBufferSize) {
		eError = _PVRSRVAllocDeviceMemKM(psDeviceNode, psPerProc,
				psSyncHeap->hDevMemHeap, 0x40004b,
				4 * psCreate->ui32VisTestResultBufferSize, 32,
				psCreate->hMemBlockProcRef, &psRenderContext->psVisTestResultMemInfo);
		if (eError != PVRSRV_OK)
			goto outOfMemory;
		OSMemSet(psRenderContext->psVisTestResultMemInfo->pvLinAddrKM, 0,
				4 * psCreate->ui32VisTestResultBufferSize);
		*ppsVisTestMemInfo = psRenderContext->psVisTestResultMemInfo;
	} else {
		*ppsVisTestMemInfo = NULL;
	}
	psHWRenderContext->ui32Flags = psCreate->ui32Flags & 4 ? 65 : 1;
	psHWRenderContext->sTACCBDevVAddr = psRenderContext->psTACCB->psBufferMemInfo->sDevVAddr;
	psHWRenderContext->sTACCBControlDevVAddr = psRenderContext->psTACCB->psControlMemInfo->sDevVAddr;
	psHWRenderContext->firmwarePrivateWord = 0;
	psHWRenderContext->sPDDevPAddr.uiAddr = psCreate->memory.memoryContext->mmu.pageDirectoryPhysicalAddress;
	if (!psHWRenderContext->sPDDevPAddr.uiAddr || !psPBDesc)
		goto failure;
	psHWRenderContext->sHWPBDescDevVAddr = psPBDesc->sHWPBDescDevVAddr;
	psHWRenderContext->ui32Priority = 1;
	psHWRenderContext->ui32MasterVDMSnapshotBufferAddress =
			psRenderContext->psMasterVDMSnapShotBufferMemInfo->sDevVAddr.uiAddr >> 4;
	for (i = 0; i < 4; i++)
		psHWRenderContext->aui32VDMSnapshotBufferAddress[i] =
				psRenderContext->psVDMSnapShotBufferMemInfo[i]->sDevVAddr.uiAddr >> 4;
	for (i = 0; i < 4; i++)
		psHWRenderContext->asMTEStateFlushBufferDevVAddr[i].uiAddr =
				psRenderContext->psMTEStateFlushMemInfo[i]->sDevVAddr.uiAddr & ~15U;
	psHWRenderContext->sSABufferDevVAddr = psRenderContext->psSABufferMemInfo->sDevVAddr;
	psHWRenderContext->ui32SABufferSizeInDwords = ui32SABufferSize >> 2;
	if (SGXGetInternalDevInfoKM(psDeviceNode, &sInternalDevInfo) != PVRSRV_OK)
		goto failure;
	psRenderContext->bForcePTOff = sInternalDevInfo.bForcePTOff;
	psHWRenderContext->ui32ProcessID = OSGetCurrentProcessIDKM();
	if (psRenderContext->bPerContextPB)
		psHWRenderContext->ui32Flags |= 0x20;
	psPerProc->bContextSuspendEnabled = (psHWRenderContext->ui32Flags & 0x40) != 0;
	psRenderContext->hHWRenderContext = SGXRegisterHWRenderContextKM(psDeviceNode,
			&psRenderContext->psHWRenderContextMemInfo->sDevVAddr,
			psRenderContext->psHWRenderContextMemInfo, psPerProc);
	if (!psRenderContext->hHWRenderContext)
		goto failure;
	eError = PVRSRVAllocHandle(psPerProc->psHandleBase, phRenderContext,
			psRenderContext, 24, 0);
	if (eError != PVRSRV_OK)
		goto failure;
	psRenderContext->hMutex = sceKernelCreateMutex("SceGpuRcMutex", 0x2000, 0, NULL);
	if (psRenderContext->hMutex < 0)
		goto outOfMemory;
	psRenderContext->mutexResource = ResManRegisterRes(psPerProc->hResManContext,
			22, NULL, psRenderContext->hMutex, DestroyRenderContextMutexCallback);
	psRenderContext->ui32NumPixelPartitions = 0;
	psRenderContext->ui32NumVertexPartitions = 0;
	return eError;
sharedBufferFailure:
	eError = PVRSRV_ERROR_CREATE_RENDER_CONTEXT_FAILED;
	goto failure;
invalidHeap:
	eError = PVRSRV_ERROR_INVALID_HEAPINFO;
	goto failure;
outOfMemory:
	eError = PVRSRV_ERROR_OUT_OF_MEMORY;
failure:
	if (psRenderContext->psVisTestResultMemInfo)
		PVRSRVFreeDeviceMemKM(NULL, psRenderContext->psVisTestResultMemInfo);
	for (i = 0; i < 4; i++)
		if (psRenderContext->psVDMSnapShotBufferMemInfo[i])
			PVRSRVFreeDeviceMemKM(NULL, psRenderContext->psVDMSnapShotBufferMemInfo[i]);
	if (psRenderContext->psMasterVDMSnapShotBufferMemInfo)
		PVRSRVFreeDeviceMemKM(NULL, psRenderContext->psMasterVDMSnapShotBufferMemInfo);
	for (i = 0; i < 4; i++)
		if (psRenderContext->psMTEStateFlushMemInfo[i])
			PVRSRVFreeDeviceMemKM(NULL, psRenderContext->psMTEStateFlushMemInfo[i]);
	if (psRenderContext->psSABufferMemInfo)
		PVRSRVFreeDeviceMemKM(NULL, psRenderContext->psSABufferMemInfo);
	if (psRenderContext->psHWRenderContextMemInfo)
		PVRSRVFreeDeviceMemKM(NULL, psRenderContext->psHWRenderContextMemInfo);
	if (psRenderContext->psTA3DSyncObject) {
		if (psRenderContext->psTA3DSyncObject->hResItem)
			ResManFreeResByPtr(psRenderContext->psTA3DSyncObject->hResItem, IMG_FALSE);
		else
			PVRSRVFreeSyncInfoKM(psRenderContext->psTA3DSyncObject);
	}
	if (psRenderContext->psTACCB)
		DestroyCommandRing(psRenderContext->psTACCB);
	if (psRenderContext->bPerContextPB)
		DestroyPrivateParameterBuffer(psClientPBDesc);
	if (psRenderContext->bPerContextPB) {
		if (psPBDesc)
			UnreferenceClientParameterBuffer(psClientPBDesc);
		else if (hSharedPBDesc)
			SGXUnrefSharedPBDescKM(hSharedPBDesc);
	}
	FreeProcessGpuMemory(psClientPBDesc);
	if (psRenderContext->hMutex) {
		if (psRenderContext->mutexResource)
			ResManFreeResByPtr(psRenderContext->mutexResource, IMG_FALSE);
		else
			sceKernelDeleteMutex(psRenderContext->hMutex);
	}
	FreeProcessGpuMemory(psRenderContext);
	*phRenderContext = 0;
	return eError;
}

IMG_UINT32 RoundUpRenderTargetDimension(IMG_UINT32 ui32Dimension)
{
	IMG_UINT32 ui32Bits = 12;
	IMG_UINT32 ui32Rounded = 4096;

	do {
		if (ui32Rounded & ui32Dimension)
			break;
		ui32Bits--;
		ui32Rounded >>= 1;
	} while (ui32Bits);
	if ((ui32Rounded - 1) & ui32Dimension)
		ui32Rounded *= 2;
	return ui32Rounded;
}

int FreeRenderTargetResources(SGX_RTDATASET *psRTDataSet)
{
	IMG_UINT32 i, j;
	SGX_RENDERDETAILS *psDetails, *psDetailsScan, *psDetailsPrevious;
	SGX_DEVICE_SYNC_LIST *psSyncList, *psSyncScan, *psSyncPrevious;

	for (i = 0; i < psRTDataSet->ui32NumRTData; i++) {
		for (j = 0; j < 4; j++) {
			if (psRTDataSet->psRTData[i].apsRgnHeaderMemInfo[j])
				PVRSRVFreeDeviceMemKM(NULL,
						psRTDataSet->psRTData[i].apsRgnHeaderMemInfo[j]);
		}
		if (psRTDataSet->psRTData[i].psContextStateMemInfo)
			PVRSRVFreeDeviceMemKM(NULL,
					psRTDataSet->psRTData[i].psContextStateMemInfo);
		if (psRTDataSet->psRTData[i].psTileRgnLUTMemInfo)
			PVRSRVFreeDeviceMemKM(NULL,
					psRTDataSet->psRTData[i].psTileRgnLUTMemInfo);
	}
	if (psRTDataSet->psSpecialObjMemInfo)
		PVRSRVFreeDeviceMemKM(NULL, psRTDataSet->psSpecialObjMemInfo);
	if (psRTDataSet->psPIMTableMemInfo)
		PVRSRVFreeDeviceMemKM(NULL, psRTDataSet->psPIMTableMemInfo);
	for (i = 0; i < 4; i++) {
		if (psRTDataSet->apsTailPtrsMemInfo[i])
			PVRSRVFreeDeviceMemKM(NULL, psRTDataSet->apsTailPtrsMemInfo[i]);
		if (psRTDataSet->psContextControlMemInfo[i])
			PVRSRVFreeDeviceMemKM(NULL, psRTDataSet->psContextControlMemInfo[i]);
		if (psRTDataSet->psContextOTPMMemInfo[i])
			PVRSRVFreeDeviceMemKM(NULL, psRTDataSet->psContextOTPMMemInfo[i]);
	}
	if (psRTDataSet->psHWRTDataSetMemInfo)
		PVRSRVFreeDeviceMemKM(NULL, psRTDataSet->psHWRTDataSetMemInfo);
	if (psRTDataSet->psPendingCountMemInfo)
		PVRSRVFreeDeviceMemKM(NULL, psRTDataSet->psPendingCountMemInfo);
	while ((psDetails = psRTDataSet->psRenderDetailsList)) {
		PVRSRVFreeDeviceMemKM(NULL, psDetails->psLockMemInfo);
		PVRSRVFreeDeviceMemKM(NULL, psDetails->psHWRenderDetailsMemInfo);
		psDetailsPrevious = NULL;
		psDetailsScan = psRTDataSet->psRenderDetailsList;
		while (psDetailsScan != psDetails) {
			psDetailsPrevious = psDetailsScan;
			psDetailsScan = psDetailsScan->psNext;
		}
		if (psDetailsPrevious)
			psDetailsPrevious->psNext = psDetails->psNext;
		else
			psRTDataSet->psRenderDetailsList = psDetails->psNext;
		FreeProcessGpuMemory(psDetails);
	}
	while ((psSyncList = psRTDataSet->psDeviceSyncList)) {
		PVRSRVFreeDeviceMemKM(NULL, psSyncList->psLockMemInfo);
		PVRSRVFreeDeviceMemKM(NULL, psSyncList->psHWDeviceSyncListMemInfo);
		psSyncPrevious = NULL;
		psSyncScan = psRTDataSet->psDeviceSyncList;
		while (psSyncScan != psSyncList) {
			psSyncPrevious = psSyncScan;
			psSyncScan = psSyncScan->psNext;
		}
		if (psSyncPrevious)
			psSyncPrevious->psNext = psSyncList->psNext;
		else
			psRTDataSet->psDeviceSyncList = psSyncList->psNext;
		FreeProcessGpuMemory(psSyncList);
	}
	if (psRTDataSet->hMutex) {
		if (psRTDataSet->mutexResource)
			ResManFreeResByPtr(psRTDataSet->mutexResource, IMG_FALSE);
		else
			sceKernelDeleteMutex(psRTDataSet->hMutex);
	}
	FreeProcessGpuMemory(psRTDataSet->psRTData);
	return FreeProcessGpuMemory(psRTDataSet);
}

PVRSRV_ERROR AllocateRenderTargetResources(PVRSRV_DEVICE_NODE *psDeviceNode,
		PVRSRV_PER_PROCESS_DATA *psPerProc, IMG_SID hMemBlockProcRef,
		struct SGX_RENDERCONTEXT *psRenderContext, SGX_RTDATASET *psRTDataSet)
{
	IMG_BOOL abSharedHeap[32];
	PVRSRV_HEAP_INFO asHeapInfo[32];
	PVRSRV_HEAP_INFO *psHeap, *psSyncHeap = NULL, *psKernelHeap = NULL;
	PVRSRV_HEAP_INFO *psGeneralHeap = NULL, *psParameterHeap = NULL;
	struct SGXMKIF_HWRTDATASET *psHWDataSet;
	SGXMKIF_HWRTDATA *psHWData;
	SGX_RENDERDETAILS *psDetails;
	SGX_DEVICE_SYNC_LIST *psSyncList;
	SGX_RTDATA *psRTData;
	SGX_BGOBJ *psBGObj;
	IMG_UINT32 ui32HeapCount, i, j, x, y;
	IMG_UINT32 ui32MaxQueuedRenders = psRTDataSet->ui32MaxQueuedRenders;
	IMG_UINT32 ui32MTileX, ui32MTileY, ui32MTileCount;
	IMG_UINT32 ui32TileWidth, ui32TileHeight, ui32TailDimension;
	IMG_UINT32 ui32TailSize, ui32RgnSize, ui32ScreenXMax, ui32ScreenYMax;
	IMG_UINT32 ui32TileX, ui32TileY, ui32XEnd, ui32YEnd, ui32XRem, ui32YRem;
	IMG_UINT32 ui32RgnOffset = 0, ui32XOffset, ui32XMacroTile;
	IMG_UINT32 *pui32LUT;
	IMG_UINT32 ui32BGObjPtr, ui32X, ui32Y, ui32Origin;
	PVRSRV_ERROR eError;

	if (PVRSRVGetDeviceMemHeapInfoKM(psDeviceNode,
			psRenderContext->hDevMemContext, &ui32HeapCount,
			asHeapInfo, abSharedHeap) != PVRSRV_OK)
		return PVRSRV_ERROR_FAILED_TO_RETRIEVE_HEAPINFO;
	for (psHeap = asHeapInfo; psHeap != asHeapInfo + ui32HeapCount; psHeap++) {
		switch (psHeap->ui32HeapID & 0xffffff) {
		case 1:
			psGeneralHeap = psHeap;
			break;
		case 3:
			psKernelHeap = psHeap;
			break;
		case 8:
			psSyncHeap = psHeap;
			break;
		case 9:
			if (!psRenderContext->bPerContextPB)
				psParameterHeap = psHeap;
			break;
		case 10:
			if (psRenderContext->bPerContextPB)
				psParameterHeap = psHeap;
			break;
		}
	}
	if (!psSyncHeap || !psKernelHeap || !psGeneralHeap)
		return PVRSRV_ERROR_INVALID_HEAPINFO;
	for (i = 0; i != ui32MaxQueuedRenders; i++) {
		psDetails = AllocProcessGpuMemory(sizeof(*psDetails));
		if (!psDetails)
			return PVRSRV_ERROR_OUT_OF_MEMORY;
		OSMemSet(psDetails, 0, sizeof(*psDetails));
		psDetails->psNext = NULL;
		psDetails->psHWRenderDetails = NULL;
		eError = _PVRSRVAllocDeviceMemKM(psDeviceNode, psPerProc,
				psKernelHeap->hDevMemHeap, 0x40000f,
				sizeof(*psDetails->psHWRenderDetails), 32,
				hMemBlockProcRef, &psDetails->psHWRenderDetailsMemInfo);
		if (eError != PVRSRV_OK)
			goto detailsFailure;
		psDetails->psHWRenderDetails = psDetails->psHWRenderDetailsMemInfo->pvLinAddrKM;
		OSMemSet(psDetails->psHWRenderDetails, 0, sizeof(*psDetails->psHWRenderDetails));
		psDetails->psHWRenderDetails->firmwarePrivateState[16] = 0;
		psDetails->psHWRenderDetails->firmwarePrivateState[17] = 0;
		eError = _PVRSRVAllocDeviceMemKM(psDeviceNode, psPerProc,
				psSyncHeap->hDevMemHeap, 0x40000f, 8, 4,
				hMemBlockProcRef, &psDetails->psLockMemInfo);
		if (eError != PVRSRV_OK)
			goto detailsFailure;
		psDetails->psHWRenderDetails->sLockDevVAddr = psDetails->psLockMemInfo->sDevVAddr;
		psDetails->pui32Lock = psDetails->psLockMemInfo->pvLinAddrKM;
		*psDetails->pui32Lock = 0;
		psDetails->psNext = psRTDataSet->psRenderDetailsList;
		psRTDataSet->psRenderDetailsList = psDetails;
		psSyncList = AllocProcessGpuMemory(sizeof(*psSyncList));
		if (!psSyncList)
			return PVRSRV_ERROR_OUT_OF_MEMORY;
		OSMemSet(psSyncList, 0, sizeof(*psSyncList));
		eError = _PVRSRVAllocDeviceMemKM(psDeviceNode, psPerProc,
				psKernelHeap->hDevMemHeap, 0x40000f,
				sizeof(SGXMKIF_HWDEVICE_SYNC_LIST) + sizeof(PVRSRV_DEVICE_SYNC_OBJECT),
				32, hMemBlockProcRef, &psSyncList->psHWDeviceSyncListMemInfo);
		if (eError != PVRSRV_OK)
			goto syncFailure;
		psSyncList->psHWDeviceSyncList = psSyncList->psHWDeviceSyncListMemInfo->pvLinAddrKM;
		psSyncList->psDeviceSyncListMemInfo = psSyncList->psHWDeviceSyncListMemInfo;
		OSMemSet(psSyncList->psHWDeviceSyncList, 0,
				sizeof(SGXMKIF_HWDEVICE_SYNC_LIST) + sizeof(PVRSRV_DEVICE_SYNC_OBJECT));
		eError = _PVRSRVAllocDeviceMemKM(psDeviceNode, psPerProc,
				psSyncHeap->hDevMemHeap, 0x40000f, 8, 4,
				hMemBlockProcRef, &psSyncList->psLockMemInfo);
		if (eError != PVRSRV_OK)
			goto syncFailure;
		psSyncList->psHWDeviceSyncList->sAccessDevAddr = psSyncList->psLockMemInfo->sDevVAddr;
		psSyncList->pui32Lock = psSyncList->psLockMemInfo->pvLinAddrKM;
		*psSyncList->pui32Lock = 0;
		psSyncList->psNext = psRTDataSet->psDeviceSyncList;
		psRTDataSet->psDeviceSyncList = psSyncList;
	}
	eError = _PVRSRVAllocDeviceMemKM(psDeviceNode, psPerProc,
			psKernelHeap->hDevMemHeap, 0x40004f, 4, 32,
			hMemBlockProcRef, &psRTDataSet->psPendingCountMemInfo);
	if (eError != PVRSRV_OK)
		return eError;
	eError = _PVRSRVAllocDeviceMemKM(psDeviceNode, psPerProc,
			psKernelHeap->hDevMemHeap, 0x40004f,
			sizeof(*psHWDataSet) + sizeof(SGXMKIF_HWRTDATA) * psRTDataSet->ui32NumRTData,
			32, hMemBlockProcRef, &psRTDataSet->psHWRTDataSetMemInfo);
	if (eError != PVRSRV_OK)
		return eError;
	OSMemSet(psRTDataSet->psHWRTDataSetMemInfo->pvLinAddrKM, 0,
			psRTDataSet->psHWRTDataSetMemInfo->uAllocSize);
	psHWDataSet = psRTDataSet->psHWRTDataSetMemInfo->pvLinAddrKM;
	psRTDataSet->psHWRTDataSet = psHWDataSet;
	psRTDataSet->pui32PendingCount = psRTDataSet->psPendingCountMemInfo->pvLinAddrKM;
	psHWDataSet->ui32NumRTData = psRTDataSet->ui32NumRTData;
	ui32MTileX = psRTDataSet->ui32MTileX1;
	ui32MTileY = psRTDataSet->ui32MTileY1;
	if (psRTDataSet->ui32MTileNumber) {
		ui32MTileCount = 4;
	} else {
		ui32MTileX = psRTDataSet->ui32MTileX2;
		ui32MTileY = psRTDataSet->ui32MTileY2;
		ui32MTileCount = 2;
	}
	ui32MTileX *= psRTDataSet->ui16MSAASamplesInX;
	ui32MTileY *= psRTDataSet->ui16MSAASamplesInY;
	*psRTDataSet->pui32PendingCount = 0;
	ui32TileWidth = ui32MTileX * ui32MTileCount;
	ui32TileHeight = ui32MTileY * ui32MTileCount;
	psHWDataSet->sPendingCountDevVAddr = psRTDataSet->psPendingCountMemInfo->sDevVAddr;
	psHWDataSet->firmwarePrivateWord = 0;
	psHWDataSet->ui32TileWidth = ui32TileWidth;
	ui32TailDimension = RoundUpRenderTargetDimension(ui32TileWidth);
	i = RoundUpRenderTargetDimension(ui32TileHeight);
	if (i > ui32TailDimension)
		ui32TailDimension = i;
	ui32TailSize = 8 * ui32TailDimension * ui32TailDimension;
	for (i = 0; i != 4; i++) {
		eError = _PVRSRVAllocDeviceMemKM(psDeviceNode, psPerProc,
				psGeneralHeap->hDevMemHeap, 0x40000b, ui32TailSize, 64,
				hMemBlockProcRef, &psRTDataSet->apsTailPtrsMemInfo[i]);
		if (eError != PVRSRV_OK)
			return eError;
		OSMemSet(psRTDataSet->apsTailPtrsMemInfo[i]->pvLinAddrKM, 0, ui32TailSize);
		eError = _PVRSRVAllocDeviceMemKM(psDeviceNode, psPerProc,
				psGeneralHeap->hDevMemHeap, 0x40000b, 128, 16,
				hMemBlockProcRef, &psRTDataSet->psContextControlMemInfo[i]);
		if (eError != PVRSRV_OK)
			return eError;
		OSMemSet(psRTDataSet->psContextControlMemInfo[i]->pvLinAddrKM, 0, 128);
		eError = _PVRSRVAllocDeviceMemKM(psDeviceNode, psPerProc,
				psGeneralHeap->hDevMemHeap, 0x40000b, 144, 16,
				hMemBlockProcRef, &psRTDataSet->psContextOTPMMemInfo[i]);
		if (eError != PVRSRV_OK)
			return eError;
		OSMemSet(psRTDataSet->psContextOTPMMemInfo[i]->pvLinAddrKM, 0, 144);
	}
	eError = _PVRSRVAllocDeviceMemKM(psDeviceNode, psPerProc,
			psParameterHeap->hDevMemHeap, 0x40004b, sizeof(SGX_BGOBJ), 4096,
			hMemBlockProcRef, &psRTDataSet->psSpecialObjMemInfo);
	if (eError != PVRSRV_OK)
		return eError;
	eError = _PVRSRVAllocDeviceMemKM(psDeviceNode, psPerProc,
			psParameterHeap->hDevMemHeap, 0x40000b, 256, 16,
			hMemBlockProcRef, &psRTDataSet->psPIMTableMemInfo);
	if (eError != PVRSRV_OK)
		return eError;
	OSMemSet(psRTDataSet->psPIMTableMemInfo->pvLinAddrKM, 0, 256);
	ui32RgnSize = 12 * ui32TileWidth * ui32TileHeight;
	for (i = 0; i < psRTDataSet->ui32NumRTData; i++) {
		psHWData = &psHWDataSet->asRTData[i];
		psRTData = &psRTDataSet->psRTData[i];
		psHWData->sHWRenderContextDevVAddr = psRenderContext->psHWRenderContextMemInfo->sDevVAddr;
		for (j = 0; j != 4; j++) {
			psHWData->asTailPtrsDevVAddr[j] = psRTDataSet->apsTailPtrsMemInfo[j]->sDevVAddr;
			psHWData->asContextControlDevVAddr[j] = psRTDataSet->psContextControlMemInfo[j]->sDevVAddr;
			psHWData->asContextOTPMDevVAddr[j] = psRTDataSet->psContextOTPMMemInfo[j]->sDevVAddr;
		}
		psHWData->ui32TailPtrsSize = ui32TailSize;
		psHWData->sSpecialObjDevVAddr = psRTDataSet->psSpecialObjMemInfo->sDevVAddr;
		psHWData->ui32TilesPerMacroTile = (ui32MTileX * ui32MTileY) >> 4;
		psHWData->sPIMTableDevVAddr = psRTDataSet->psPIMTableMemInfo->sDevVAddr;
		psHWData->ui32RgnHeaderMacroTileSize = 12 * ui32MTileX * ui32MTileY;
		for (j = 0; j != 4; j++) {
			eError = _PVRSRVAllocDeviceMemKM(psDeviceNode, psPerProc,
					psGeneralHeap->hDevMemHeap, 0x40000b, ui32RgnSize, 64,
					hMemBlockProcRef, &psRTData->apsRgnHeaderMemInfo[j]);
			if (eError != PVRSRV_OK)
				return eError;
		}
		eError = _PVRSRVAllocDeviceMemKM(psDeviceNode, psPerProc,
				psGeneralHeap->hDevMemHeap, 0x40000b, 1768, 16,
				hMemBlockProcRef, &psRTData->psContextStateMemInfo);
		if (eError != PVRSRV_OK)
			return eError;
		psHWData->sContextStateDevVAddr = psRTData->psContextStateMemInfo->sDevVAddr;
		OSMemSet(psRTData->psContextStateMemInfo->pvLinAddrKM, 0, 1768);
		eError = _PVRSRVAllocDeviceMemKM(psDeviceNode, psPerProc,
				psKernelHeap->hDevMemHeap, 0x40004f,
				16 * ui32MTileCount * ui32MTileCount, 32,
				hMemBlockProcRef, &psRTData->psTileRgnLUTMemInfo);
		if (eError != PVRSRV_OK)
			return eError;
		psHWData->sTileRgnLUTDevVAddr = psRTData->psTileRgnLUTMemInfo->sDevVAddr;
		ui32ScreenXMax = (psRTDataSet->ui32ScreenXMax + 1) * psRTDataSet->ui16MSAASamplesInX - 1;
		ui32ScreenYMax = (psRTDataSet->ui32ScreenYMax + 1) * psRTDataSet->ui16MSAASamplesInY - 1;
		pui32LUT = psRTData->psTileRgnLUTMemInfo->pvLinAddrKM;
		ui32TileX = 0;
		for (x = 0; x < ui32MTileCount; x++) {
			ui32XEnd = ui32MTileX - 1 + ui32TileX;
			if (ui32XEnd >= ui32ScreenXMax)
				ui32XEnd = ui32ScreenXMax;
			ui32XRem = ui32XEnd % ui32MTileX;
			ui32XOffset = (ui32XRem & 3) + 4 * (ui32XRem & ~3U);
			ui32XMacroTile = ui32MTileCount * (ui32XEnd / ui32MTileX);
			ui32TileY = 0;
			for (y = 0; y < ui32MTileCount; y++) {
				ui32YEnd = ui32MTileY - 1 + ui32TileY;
				if (ui32YEnd >= ui32ScreenYMax)
					ui32YEnd = ui32ScreenYMax;
				ui32YRem = ui32YEnd % ui32MTileY;
				ui32RgnOffset = 12 * (ui32XOffset + 4 * (ui32YRem & 3) +
						ui32MTileX * ((ui32YRem & ~3U) + ui32MTileY *
						(ui32YEnd / ui32MTileY + ui32XMacroTile)));
				if (ui32ScreenXMax >= ui32TileX && ui32ScreenYMax >= ui32TileY)
					ui32RgnOffset |= 1;
				*pui32LUT++ = ui32RgnOffset;
				ui32TileY += ui32MTileY;
			}
			ui32TileX += ui32MTileX;
		}
		for (j = 0; j != 4; j++) {
			psHWData->asRgnHeaderDevVAddr[j] = psRTData->apsRgnHeaderMemInfo[j]->sDevVAddr;
			psHWData->asLastRgnHeaderDevVAddr[j].uiAddr = psRTData->apsRgnHeaderMemInfo[j]->sDevVAddr.uiAddr + ui32RgnOffset;
		}
		psHWData->ui8NumActiveTiles = 0;
		pui32LUT = psRTData->psTileRgnLUTMemInfo->pvLinAddrKM;
		for (j = 0; j < ui32MTileCount * ui32MTileCount; j++) {
			if (pui32LUT[j] & 1) {
				psHWData->ui32LastActiveTile = j;
				psHWData->ui8NumActiveTiles++;
			}
		}
		psHWData->sRTStatusDevVAddr.uiAddr = psRTDataSet->psHWRTDataSetMemInfo->sDevVAddr.uiAddr +
				offsetof(struct SGXMKIF_HWRTDATASET, asRTData) +
				i * sizeof(*psHWData) + offsetof(SGXMKIF_HWRTDATA, ui32RTStatus);
		psRTData->pui32RTStatus = &psHWData->ui32RTStatus;
		psHWData->ui32RTStatus = 16;
	}
	psBGObj = psRTDataSet->psSpecialObjMemInfo->pvLinAddrKM;
	ui32BGObjPtr = ((psRTDataSet->psSpecialObjMemInfo->sDevVAddr.uiAddr -
			psRenderContext->psClientPBDesc->psPBDesc->sPBBaseDevVAddr.uiAddr) >> 4) & 0xffffff;
	ui32BGObjPtr |= 0x2000000;
	psBGObj->firmwareHeader[0] = 0;
	psBGObj->firmwareHeader[1] = 0;
	psBGObj->firmwareHeader[2] = 0;
	switch (psRTDataSet->eRotation) {
	case PVRSRV_ROTATE_180:
		psBGObj->aui32TextureCoordinates[0][0] = 0x3f800000;
		psBGObj->aui32TextureCoordinates[0][1] = 0x3f800000;
		psBGObj->aui32TextureCoordinates[1][0] = 0;
		psBGObj->aui32TextureCoordinates[1][1] = 0x3f800000;
		psBGObj->aui32TextureCoordinates[2][0] = 0x3f800000;
		psBGObj->aui32TextureCoordinates[2][1] = 0;
		break;
	case PVRSRV_ROTATE_270:
		psBGObj->aui32TextureCoordinates[0][0] = 0;
		psBGObj->aui32TextureCoordinates[0][1] = 0x3f800000;
		psBGObj->aui32TextureCoordinates[1][0] = 0;
		psBGObj->aui32TextureCoordinates[1][1] = 0;
		psBGObj->aui32TextureCoordinates[2][0] = 0x3f800000;
		psBGObj->aui32TextureCoordinates[2][1] = 0x3f800000;
		break;
	case PVRSRV_ROTATE_90:
		psBGObj->aui32TextureCoordinates[0][0] = 0x3f800000;
		psBGObj->aui32TextureCoordinates[0][1] = 0;
		psBGObj->aui32TextureCoordinates[1][0] = 0x3f800000;
		psBGObj->aui32TextureCoordinates[1][1] = 0x3f800000;
		psBGObj->aui32TextureCoordinates[2][0] = 0;
		psBGObj->aui32TextureCoordinates[2][1] = 0;
		break;
	default:
		psBGObj->aui32TextureCoordinates[0][0] = 0;
		psBGObj->aui32TextureCoordinates[0][1] = 0;
		psBGObj->aui32TextureCoordinates[1][0] = psRTDataSet->ui32BGObjUCoord;
		psBGObj->aui32TextureCoordinates[1][1] = 0;
		psBGObj->aui32TextureCoordinates[2][0] = 0;
		psBGObj->aui32TextureCoordinates[2][1] = 0x3f800000;
	}
	psBGObj->ui32PrimitiveControl = 2;
	psBGObj->firmwareControl = 0;
	ui32X = 16 * (psRTDataSet->ui32NumPixelsX + 1024);
	ui32Y = 16 * (psRTDataSet->ui32NumPixelsY + 1024);
	if (psRTDataSet->ui32Flags & 1) {
		ui32Origin = 0x4000;
	} else {
		ui32Origin = 0x3ff8;
		ui32X -= 8;
		ui32Y -= 8;
	}
	psBGObj->asVertices[0].ui32PackedPosition = (ui32Origin << 16) | ui32Origin;
	psBGObj->asVertices[1].ui32PackedPosition = (ui32X << 16) | ui32Origin;
	psBGObj->asVertices[1].fDepth = 1.0f;
	psBGObj->asVertices[0].fDepth = 1.0f;
	psBGObj->asVertices[2].ui32PackedPosition = (ui32Origin << 16) | ui32Y;
	psBGObj->asVertices[2].fDepth = 1.0f;
	for (i = 0; i < psRTDataSet->ui32NumRTData; i++)
		psRTDataSet->psRTData[i].ui32BGObjPtr1 = ui32BGObjPtr;
	return PVRSRV_OK;

detailsFailure:
	if (psDetails->psHWRenderDetails)
		PVRSRVFreeDeviceMemKM(NULL, psDetails->psHWRenderDetailsMemInfo);
	if (psDetails->psLockMemInfo)
		PVRSRVFreeDeviceMemKM(NULL, psDetails->psLockMemInfo);
	FreeProcessGpuMemory(psDetails);
	return PVRSRV_ERROR_OUT_OF_MEMORY;

syncFailure:
	if (psSyncList->psHWDeviceSyncList)
		PVRSRVFreeDeviceMemKM(NULL, psSyncList->psHWDeviceSyncListMemInfo);
	FreeProcessGpuMemory(psSyncList);
	return PVRSRV_ERROR_OUT_OF_MEMORY;
}

PVRSRV_ERROR RemoveRenderTarget(PVRSRV_DEVICE_NODE *psDeviceNode,
		struct SGX_RENDERCONTEXT *psRenderContext, SGX_RTDATASET *psRTDataSet)
{
	IMG_UINT32 pid = GetRemoteAllocProcess();
	IMG_BOOL bLastReference;
	PVRSRV_ERROR eError;
	SGX_RTDATASET *psScan;

	LinuxUnLockMutex(&gPVRSRVLock);
	sceKernelLockMutex(psRenderContext->hMutex, 1, NULL);
	bLastReference = --psRTDataSet->ui32RefCount == 0;
	if (bLastReference) {
		LinuxLockMutexForPid(&gPVRSRVLock, pid);
		eError = SGXFlushHWRenderTargetKM(psDeviceNode,
				psRTDataSet->psHWRTDataSetMemInfo->sDevVAddr, IMG_FALSE);
		LinuxUnLockMutex(&gPVRSRVLock);
		psScan = psRenderContext->psRTDataSets;
		if (psScan == psRTDataSet) {
			psRenderContext->psRTDataSets = psRTDataSet->psNext;
		} else {
			while (psScan) {
				if (psScan->psNext == psRTDataSet) {
					psScan->psNext = psRTDataSet->psNext;
					break;
				}
				psScan = psScan->psNext;
			}
		}
	} else {
		eError = PVRSRV_OK;
	}
	sceKernelUnlockMutex(psRenderContext->hMutex, 1);
	LinuxLockMutexForPid(&gPVRSRVLock, pid);
	if (bLastReference)
		FreeRenderTargetResources(psRTDataSet);
	return eError;
}

PVRSRV_ERROR AddRenderTarget(PVRSRV_DEVICE_NODE *psDeviceNode,
		PVRSRV_PER_PROCESS_DATA *psPerProc, SGX_ADDRENDTARG *psAddRT,
		IMG_SID *phRenderTarget)
{
	IMG_UINT32 ui32PixelsX = psAddRT->ui32NumPixelsX;
	IMG_UINT32 ui32PixelsY = psAddRT->ui32NumPixelsY;
	struct SGX_RENDERCONTEXT *psRenderContext = psAddRT->context.renderContext;
	IMG_UINT32 ui32Samples;
	IMG_UINT32 ui32SamplesX, ui32SamplesY, ui32NumRTData, ui32MaxQueuedRenders;
	IMG_UINT32 ui32BGObjUCoord, ui32TilesX, ui32TilesY, ui32MTileX, ui32MTileY;
	IMG_UINT32 ui32CountX, ui32CountY, ui32MultiSample;
	SGX_RTDATASET *psRTDataSet;
	PVRSRV_ERROR eError;
	IMG_UINT32 pid;

	if (ui32PixelsX > 4096)
		return PVRSRV_ERROR_NOT_SUPPORTED;
	if (ui32PixelsY > 4096)
		return PVRSRV_ERROR_NOT_SUPPORTED;
	if (psAddRT->ui16NumRTsInArray != 1)
		return PVRSRV_ERROR_NOT_SUPPORTED;
	ui32Samples = psAddRT->ui16MSAASamplesInX |
			((IMG_UINT32)psAddRT->ui16MSAASamplesInY << 16);
	if (ui32Samples != 0x10001 && ui32Samples - 0x20001 > 1)
		return PVRSRV_ERROR_NOT_SUPPORTED;
	ui32SamplesX = psAddRT->ui16MSAASamplesInX;
	ui32SamplesY = psAddRT->ui16MSAASamplesInY;
	ui32BGObjUCoord = psAddRT->ui32BGObjUCoord;
	ui32NumRTData = psAddRT->ui32NumRTData;
	ui32MaxQueuedRenders = psAddRT->ui32MaxQueuedRenders;
	psRTDataSet = AllocProcessGpuMemory(sizeof(*psRTDataSet));
	if (!psRTDataSet)
		return PVRSRV_ERROR_OUT_OF_MEMORY;
	OSMemSet(psRTDataSet, 0, sizeof(*psRTDataSet));
	psRTDataSet->i32DataMemblockUID = psAddRT->i32DataMemblockUID;
	psRTDataSet->bUseExternalUID = psAddRT->bUseExternalUID;
	psRTDataSet->psRTData = AllocProcessGpuMemory(sizeof(SGX_RTDATA) * ui32NumRTData);
	if (!psRTDataSet->psRTData)
		goto outOfMemory;
	OSMemSet(psRTDataSet->psRTData, 0, sizeof(SGX_RTDATA) * ui32NumRTData);
	psRTDataSet->ui32NumRTData = ui32NumRTData;
	psRTDataSet->ui32NumPixelsY = ui32PixelsY;
	psRTDataSet->ui32MaxQueuedRenders = ui32MaxQueuedRenders;
	psRTDataSet->ui32Flags = 0;
	psRTDataSet->ui32NumPixelsX = ui32PixelsX;
	psRTDataSet->ui16MSAASamplesInX = ui32SamplesX;
	psRTDataSet->ui16MSAASamplesInY = ui32SamplesY;
	psRTDataSet->ui32BGObjUCoord = ui32BGObjUCoord;
	psRTDataSet->ui32RTDataSel = 0;
	psRTDataSet->ui32RefCount = 0;
	psRTDataSet->eRotation = psAddRT->eRotation;
	psRTDataSet->ui16NumRTsInArray = psAddRT->ui16NumRTsInArray;
	if (psAddRT->ui32Flags & 2)
		psRTDataSet->ui32Flags = 1;
	if (psAddRT->ui32Flags & 0x20000)
		psRTDataSet->ui32Flags |= 0x10;
	if (psAddRT->ui32Flags & 0x80000)
		psRTDataSet->ui32Flags |= 4;
	if (psAddRT->ui32Flags & 0x100000)
		psRTDataSet->ui32Flags |= 8;
	ui32TilesX = (ui32PixelsX + 31) >> 5;
	ui32TilesY = (ui32PixelsY + 31) >> 5;
	if (psAddRT->ui32Flags & 0x40000) {
		ui32CountX = psAddRT->ui8MacrotileCountX;
		ui32CountY = psAddRT->ui8MacrotileCountY;
		ui32MTileX = (ui32CountX - 1 + ui32TilesX) / ui32CountX;
		ui32MTileY = (ui32CountY - 1 + ui32TilesY) / ui32CountY;
		ui32MTileX = ui32SamplesX == 1 ?
				(ui32MTileX + 3) & ~3U : (ui32MTileX + 1) & ~1U;
		ui32MTileY = ui32SamplesY == 1 ?
				(ui32MTileY + 3) & ~3U : (ui32MTileY + 1) & ~1U;
		if (ui32CountX <= 2 && ui32CountY <= 2) {
			psRTDataSet->ui32MTileX2 = ui32MTileX;
			psRTDataSet->ui32MTileX3 = ui32MTileX;
			psRTDataSet->ui32MTileNumber = 0;
			psRTDataSet->ui32MTileX1 = ui32MTileX;
			psRTDataSet->ui32MTileY1 = ui32MTileY;
			psRTDataSet->ui32MTileY2 = ui32MTileY;
			psRTDataSet->ui32MTileY3 = ui32MTileY;
		} else {
			psRTDataSet->ui32MTileNumber = 1;
			psRTDataSet->ui32MTileX1 = ui32MTileX;
			psRTDataSet->ui32MTileX2 = 2 * ui32MTileX;
			psRTDataSet->ui32MTileX3 = 3 * ui32MTileX;
			psRTDataSet->ui32MTileY1 = ui32MTileY;
			psRTDataSet->ui32MTileY2 = 2 * ui32MTileY;
			psRTDataSet->ui32MTileY3 = 3 * ui32MTileY;
		}
	} else if (ui32PixelsX * ui32PixelsY < 0x4b000) {
		ui32MTileX = (ui32TilesX + 1) >> 1;
		ui32MTileY = (ui32TilesY + 1) >> 1;
		psRTDataSet->ui32MTileNumber = 0;
		psRTDataSet->ui32MTileX2 = ui32SamplesX == 1 ?
				(ui32MTileX + 3) & ~3U : (ui32MTileX + 1) & ~1U;
		psRTDataSet->ui32MTileY2 = ui32SamplesY == 1 ?
				(ui32MTileY + 3) & ~3U : (ui32MTileY + 1) & ~1U;
		psRTDataSet->ui32MTileX1 = psRTDataSet->ui32MTileX2;
		psRTDataSet->ui32MTileX3 = psRTDataSet->ui32MTileX2;
		psRTDataSet->ui32MTileY1 = psRTDataSet->ui32MTileY2;
		psRTDataSet->ui32MTileY3 = psRTDataSet->ui32MTileY2;
		ui32MTileX = psRTDataSet->ui32MTileX2;
		ui32MTileY = psRTDataSet->ui32MTileY2;
	} else {
		ui32MTileX = (ui32TilesX + 3) >> 2;
		ui32MTileY = (ui32TilesY + 3) >> 2;
		psRTDataSet->ui32MTileNumber = 1;
		psRTDataSet->ui32MTileX1 = ui32SamplesX == 1 ?
				(ui32MTileX + 3) & ~3U : (ui32MTileX + 1) & ~1U;
		ui32MTileX = psRTDataSet->ui32MTileX1;
		psRTDataSet->ui32MTileX2 = 2 * ui32MTileX;
		psRTDataSet->ui32MTileX3 = 3 * ui32MTileX;
		psRTDataSet->ui32MTileY1 = ui32SamplesY == 1 ?
				(ui32MTileY + 3) & ~3U : (ui32MTileY + 1) & ~1U;
		ui32MTileY = psRTDataSet->ui32MTileY1;
		psRTDataSet->ui32MTileY2 = 2 * ui32MTileY;
		psRTDataSet->ui32MTileY3 = 3 * ui32MTileY;
	}
	psRTDataSet->ui32ScreenXMax = ui32TilesX - 1;
	psRTDataSet->ui32ScreenYMax = ui32TilesY - 1;
	psRTDataSet->ui32MTileStride = ui32MTileX * ui32MTileY;
	ui32MultiSample = 0;
	if (psRTDataSet->ui16MSAASamplesInX == 1) {
		psRTDataSet->eScalingInX = SGX_SCALING_NONE;
		if (psRTDataSet->ui16MSAASamplesInY == 1) {
			psRTDataSet->eScalingInY = SGX_SCALING_NONE;
			psRTDataSet->ui323DAAMode = 0;
			ui32MultiSample = psRTDataSet->ui32Flags & 1 ? 0x88 : 0;
		} else if (psRTDataSet->ui16MSAASamplesInY == 2) {
			psRTDataSet->eScalingInY = SGX_DOWNSCALING;
			psRTDataSet->ui323DAAMode = 1;
			ui32MultiSample = psAddRT->eForceScalingInX == SGX_UPSCALING ||
					psAddRT->eForceScalingInY == SGX_UPSCALING ? 0xc848 : 0xcc44;
		}
	} else if (psRTDataSet->ui16MSAASamplesInX == 2) {
		psRTDataSet->eScalingInX = SGX_DOWNSCALING;
		if (psRTDataSet->ui16MSAASamplesInY == 2) {
			psRTDataSet->eScalingInY = SGX_DOWNSCALING;
			psRTDataSet->ui323DAAMode = 2;
			ui32MultiSample = psAddRT->eForceScalingInX == SGX_UPSCALING ||
					psAddRT->eForceScalingInY == SGX_UPSCALING ? 0xccc44c44 : 0xeaa26e26;
		}
	}
	psRTDataSet->ui32ISPMultiSampleCtl2 = psAddRT->ui32Flags & 8 ? 0 : 0x188;
	if (psAddRT->ui32Flags & 0x10000)
		ui32MultiSample = psAddRT->ui32MultisampleLocations;
	psRTDataSet->ui32MTEMultiSampleCtl = ui32MultiSample;
	psRTDataSet->ui32ISPMultiSampleCtl = ui32MultiSample;
	if ((IMG_UINT32)(psAddRT->eForceScalingInX - 1) <= 1)
		psRTDataSet->eScalingInX = psAddRT->eForceScalingInX;
	if ((IMG_UINT32)(psAddRT->eForceScalingInY - 1) <= 1)
		psRTDataSet->eScalingInY = psAddRT->eForceScalingInY;
	eError = AllocateRenderTargetResources(psDeviceNode, psPerProc,
			psAddRT->hMemBlockProcRef, psRenderContext, psRTDataSet);
	if (eError != PVRSRV_OK)
		goto failure;
	psRTDataSet->hMutex = sceKernelCreateMutex("SceGpuRtMutex", 0x2000, 0, NULL);
	if (psRTDataSet->hMutex < 0)
		goto outOfMemory;
	psRTDataSet->mutexResource = ResManRegisterRes(psPerProc->hResManContext,
			22, NULL, psRTDataSet->hMutex, DestroyRenderTargetMutexCallback);
	pid = GetRemoteAllocProcess();
	LinuxUnLockMutex(&gPVRSRVLock);
	sceKernelLockMutex(psRenderContext->hMutex, 1, NULL);
	psRTDataSet->psNext = psRenderContext->psRTDataSets;
	psRenderContext->psRTDataSets = psRTDataSet;
	psRTDataSet->psRenderContext = psRenderContext;
	psRTDataSet->ui32RefCount++;
	sceKernelUnlockMutex(psRenderContext->hMutex, 1);
	LinuxLockMutexForPid(&gPVRSRVLock, pid);
	eError = PVRSRVAllocHandle(psPerProc->psHandleBase, phRenderTarget,
			psRTDataSet, 25, 0);
	if (eError != PVRSRV_OK)
		goto failure;
	return eError;
outOfMemory:
	eError = PVRSRV_ERROR_OUT_OF_MEMORY;
failure:
	FreeRenderTargetResources(psRTDataSet);
	return eError;
}

PVRSRV_ERROR DestroyCommandContext(GpuTransferQueueContext *context,
		IMG_BOOL bForceCleanup)
{
	if (context) {
		if (context->contextMemInfo) {
			SGXUnregisterHW2DContextKM(context->hardwareRegistration,
					bForceCleanup);
			PVRSRVFreeDeviceMemKM(NULL, context->contextMemInfo);
		}
		if (context->commandRing)
			DestroyCommandRing(context->commandRing);
		if (context->sourceSync) {
			if (context->sourceSync->hResItem)
				ResManFreeResByPtr(context->sourceSync->hResItem, IMG_FALSE);
			else
				PVRSRVFreeSyncInfoKM(context->sourceSync);
		}
		if (context->destinationSync) {
			if (context->destinationSync->hResItem)
				ResManFreeResByPtr(context->destinationSync->hResItem, IMG_FALSE);
			else
				PVRSRVFreeSyncInfoKM(context->destinationSync);
		}
		if (context->mutex) {
			if (context->mutexResource)
				ResManFreeResByPtr(context->mutexResource, IMG_FALSE);
			else
				sceKernelDeleteMutex(context->mutex);
		}
		FreeProcessGpuMemory(context);
	}
	return PVRSRV_OK;
}

PVRSRV_ERROR CreateTransferQueue(PVRSRV_DEVICE_NODE *psDeviceNode,
		PVRSRV_PER_PROCESS_DATA *psPerProc, IMG_UINT32 reservedParameter,
		GpuTransferMemoryContext *psMemoryContext, IMG_SID *phContext)
{
	GpuTransferQueueContext *context;
	SGXMKIF_HW2DCONTEXT *psHWContext;
	IMG_UINT32 ui32HeapCount, i;
	IMG_BOOL abSharedHeap[32];
	PVRSRV_HEAP_INFO asHeapInfo[32];
	GpuHeap *heap = NULL;

	(void)reservedParameter;
	if (psPerProc->transferQueue)
		return PVRSRV_ERROR_INIT_FAILURE;
	context = AllocProcessGpuMemory(sizeof(*context));
	if (!context)
		return PVRSRV_ERROR_OUT_OF_MEMORY;
	OSMemSet(context, 0, sizeof(*context));
	if (PVRSRVGetDeviceMemHeapInfoKM(psDeviceNode,
			psMemoryContext->memory.memoryContext, &ui32HeapCount,
			asHeapInfo, abSharedHeap) != PVRSRV_OK)
		return PVRSRV_ERROR_FAILED_TO_RETRIEVE_HEAPINFO;
	context->memoryContext = psMemoryContext->memory.memoryContext;
	for (i = 0; i < ui32HeapCount; i++)
		if ((asHeapInfo[i].ui32HeapID & 0xffffff) == 3)
			heap = asHeapInfo[i].hDevMemHeap;
	if (PVRSRVAllocSyncInfoKM(psDeviceNode, NULL, &context->sourceSync) != PVRSRV_OK)
		goto failure;
	if (PVRSRVAllocSyncInfoKM(psDeviceNode, NULL, &context->destinationSync) != PVRSRV_OK)
		goto failure;
	context->sourceSync->hResItem = ResManRegisterRes(psPerProc->hResManContext,
			20, context->sourceSync, 0,
			(PVRSRV_ERROR (*)(IMG_VOID *, IMG_UINT32, IMG_BOOL))FreeSyncInfoCallback);
	context->destinationSync->hResItem = ResManRegisterRes(psPerProc->hResManContext,
			20, context->destinationSync, 0,
			(PVRSRV_ERROR (*)(IMG_VOID *, IMG_UINT32, IMG_BOOL))FreeSyncInfoCallback);
	if (CreateCommandRing(psDeviceNode, psPerProc, psMemoryContext->hMemBlockProcRef,
			0x10000, 64, 0x400, heap, &context->commandRing) != PVRSRV_OK)
		goto failure;
	if (_PVRSRVAllocDeviceMemKM(psDeviceNode, psPerProc, heap, 0x40004b,
			sizeof(*psHWContext), 16, psMemoryContext->hMemBlockProcRef,
			&context->contextMemInfo) != PVRSRV_OK)
		goto failure;
	context->hardwareRegistration = SGXRegisterHW2DContextKM(psDeviceNode,
			&context->contextMemInfo->sDevVAddr, context->contextMemInfo, psPerProc);
	if (!context->hardwareRegistration)
		goto failure;
	context->contextAddress = context->contextMemInfo->sDevVAddr;
	psHWContext = context->contextMemInfo->pvLinAddrKM;
	psHWContext->ui32Flags = 1;
	psHWContext->sCCBDevVAddr = context->commandRing->psBufferMemInfo->sDevVAddr;
	psHWContext->firmwarePrivateState[0] = 0;
	psHWContext->firmwarePrivateState[1] = 0;
	psHWContext->sCCBControlDevVAddr = context->commandRing->psControlMemInfo->sDevVAddr;
	psHWContext->firmwarePrivateWord = 0;
	psHWContext->sPDDevPAddr.uiAddr = psMemoryContext->memory.memoryContext->mmu.pageDirectoryPhysicalAddress;
	if (!psHWContext->sPDDevPAddr.uiAddr)
		goto failure;
	psHWContext->ui32Priority = 1;
	psHWContext->ui32ProcessID = OSGetCurrentProcessIDKM();
	context->mutex = sceKernelCreateMutex("SceGpuTqMutex", 0x2000, 0, NULL);
	if (context->mutex < 0)
		goto failure;
	context->mutexResource = ResManRegisterRes(psPerProc->hResManContext,
			22, NULL, context->mutex, DestroyTransferQueueMutexCallback);
	if (PVRSRVAllocHandle(psPerProc->psHandleBase, phContext, context, 26, 0) != PVRSRV_OK)
		goto failure;
	psPerProc->transferQueue = context;
	return PVRSRV_OK;
failure:
	DestroyCommandContext(context, IMG_FALSE);
	return PVRSRV_ERROR_OUT_OF_MEMORY;
}

PVRSRV_ERROR SubmitTransferCommands(IMG_UINT32 *commands, IMG_UINT32 numCommands,
		PVRSRV_DEVICE_NODE *psDeviceNode, PVRSRV_PER_PROCESS_DATA *psPerProc,
		GpuTransferQueueContext *context, IMG_VOID *syncInfo,
		IMG_BOOL sourceSyncEnable, IMG_BOOL destinationSyncEnable,
		GpuTransferStatus *status)
{
	struct GpuCommandRing *ring;
	SGXMKIF_2DCMD *psCommand;
	PVRSRV_2D_SGX_KICK sKick;
	IMG_UINT32 pid, ui32CommandCount;
	PVRSRV_ERROR eError;

	if (!commands || !context || numCommands > 25)
		return PVRSRV_ERROR_INVALID_PARAMS;
	ring = context->commandRing;
	pid = GetRemoteAllocProcess();
	LinuxUnLockMutex(&gPVRSRVLock);
	sceKernelLockMutex(context->mutex, 1, NULL);
	psCommand = AcquireCommandRingSpace(ring, 256, &context->waitGeneration);
	if (psCommand) {
		OSMemSet(&sKick, 0, sizeof(sKick));
		sKick.commandMemory.memoryInfo = ring->psBufferMemInfo;
		sKick.ui32SharedCmdCCBOffset = *ring->pui32WriteOffset +
				offsetof(SGXMKIF_2DCMD, sShared);
		*ring->pui32WriteOffset = (*ring->pui32WriteOffset + 256) &
				(ring->ui32BufferSize - 1);
		OSMemSet(psCommand, 0, sizeof(*psCommand));
		psCommand->ui32CommandSize = 256;
		psCommand->ui32NumCommands = numCommands;
		OSMemCopy(psCommand->aui32Commands, commands, numCommands * sizeof(*commands));
		ui32CommandCount = psCommand->ui32NumCommands;
		psCommand->sShared.ui32NumSrcSync = 0;
		psCommand->ui32NumCommands = ui32CommandCount + 1;
		psCommand->aui32Commands[ui32CommandCount] = 0x80000000;
		sKick.ui32NumSrcSync = 0;
		sKick.hTASyncInfo.syncInfo = sourceSyncEnable ? context->sourceSync : NULL;
		sKick.h3DSyncInfo.syncInfo = destinationSyncEnable ? context->destinationSync : NULL;
		sKick.hDstSyncInfo.syncInfo = syncInfo;
		sKick.sHW2DContextDevVAddr = context->contextAddress;
		sKick.hDevMemContext = context->memoryContext;
		if (status) {
			sKick.ui32NumStatusVals = 1;
			sKick.sStatusUpdate = *status;
		} else {
			sKick.ui32NumStatusVals = 0;
		}
		eError = SGXSubmit2DKM(psDeviceNode, &sKick);
	} else {
		eError = PVRSRV_ERROR_OUT_OF_MEMORY;
	}
	sceKernelUnlockMutex(context->mutex, 1);
	LinuxLockMutexForPid(&gPVRSRVLock, pid);
	return eError;
}

PVRSRV_ERROR WaitTransferQueueIdle(GpuTransferQueueContext *context)
{
	struct GpuCommandRing *ring;
	SceUID pid;
	int result;

	if (!context)
		return PVRSRV_ERROR_INVALID_PARAMS;
	GetRemoteAllocProcess();
	LinuxUnLockMutex(&gPVRSRVLock);
	sceKernelLockMutex(context->mutex, 1, NULL);
	ring = context->commandRing;
	pid = sceKernelGetProcessId();
	for (;;) {
		if (*ring->pui32ReadOffset == *ring->pui32WriteOffset) {
			result = PVRSRV_OK;
			break;
		}
		if (exitingProcess == pid || gpuSuspended) {
			result = PVRSRV_ERROR_RETRY;
			break;
		}
		result = WaitSignalCounterKernel(&context->waitGeneration, 100000);
		if (result != 0 && result != SCE_KERNEL_ERROR_WAIT_TIMEOUT)
			break;
	}
	sceKernelUnlockMutex(context->mutex, 1);
	LinuxLockMutexForPid(&gPVRSRVLock, pid);
	return result;
}
