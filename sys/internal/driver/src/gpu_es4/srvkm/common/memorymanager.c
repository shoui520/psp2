/* Sony's common/memorymanager.c replaces IMG's generic device-memory paths. */
#include "../memory_sce.h"
#include "../sync_sce.h"
#include "../perproc_sce.h"
#include "../device_sce.h"
#include "../gpu-mmu.h"
#include "../gpu-blockheap.h"
#include "../mapped_memory_sce.h"
#include "../handle-types.h"
#include <sysmem/debug.h>
#include <sysmem/sysmem.h>
#include <psp2kern/kernel/processmgr.h>
#include <psp2kern/kernel/threadmgr/misc.h>
#include <sysmem/sysclib.h>
#include <acmgr/acmgr.h>

int RemoveMemoryContextFromList(GpuMemoryContext *context, unsigned pid);
int InsertMappedMemoryLegacy(GpuMappedMemoryList *list, IMG_VOID *address,
		IMG_SIZE_T size, IMG_UINT32 flags, GpuHeap *heap,
		GpuMappedMemory **output);
int InsertMappedMemory(GpuMappedMemoryList *list, IMG_VOID *address,
		IMG_SIZE_T size, IMG_UINT32 flags, GpuHeap *heap,
		GpuMappedMemory **output);
int RemoveMappedMemory(GpuMappedMemoryList *list, IMG_VOID *address,
		GpuMappedMemory *output, GpuHeap *heap);
int CheckMappedMemory(GpuMappedMemoryList *list, const IMG_VOID *address,
		IMG_SIZE_T size, GpuHeap *heap);
int CheckMappedMemoryOverlap(GpuMappedMemoryList *list, IMG_VOID *address,
		IMG_SIZE_T size);
extern SceUID gPVRSRVLock;
extern IMG_UINT32 gpuSuspended;
extern SceUID exitingProcess;
/* SCE keeps an independent generation snapshot for device-memory unmap waits. */
IMG_UINT32 unmapWaitGeneration;
int LinuxLockMutex(SceUID *mutex);
int LinuxUnLockMutex(SceUID *mutex);
int WaitSignalCounterKernel(IMG_UINT32 *counter, IMG_UINT32 timeout);
PVRSRV_ERROR WaitTransferQueueIdle(struct GpuTransferQueueContext *context);
PVRSRV_ERROR PVRSRVLookupHandle(PVRSRV_HANDLE_BASE *psBase, IMG_VOID **ppvData,
		IMG_SID hHandle, IMG_UINT32 eType);

/* SCE native context teardown, preserving the IMG error boundary. */
PVRSRV_ERROR DestroyDeviceMemContextCallback(GpuMemoryContext *psContext,
		IMG_UINT32 ui32ProcessID)
{
	int ret;

	ret = RemoveMemoryContextFromList(psContext, ui32ProcessID);
	if (ret < 0) {
		SCE_DBG_PRINTF(0, 382,
				"%s: Failed to destroy memory context with error %08x.\n",
				__func__, ret);
		return PVRSRV_ERROR_INVALID_PARAMS;
	}
	return PVRSRV_OK;
}

/* The GPU map resource uses this address/process callback. Its two sysmem
 * imports identify the operation; no Sony name survives in debug records.
 */
PVRSRV_ERROR ReleaseDeviceMemBlockCallback(const IMG_VOID *address,
		SceUID processID)
{
	SceUID blockUID;

	blockUID = SceSysmemForDriver_857F1D5A(processID, address, 0);
	if (blockUID < 0)
		return PVRSRV_ERROR_INVALID_PARAMS;
	SceSysmemForDriver_F50BDC0C(blockUID);
	return PVRSRV_OK;
}

IMG_VOID *AllocProcessGpuMemory(IMG_SIZE_T size);
int FreeProcessGpuMemory(IMG_VOID *memory);
int sceGpuHeapAlloc(GpuHeap *heap, IMG_SIZE_T size, IMG_SIZE_T alignment,
		IMG_UINT32 allocationType, IMG_UINT32 flags, IMG_SID blockGroupId,
		GpuHeapAllocation *output, IMG_SIZE_T boundary);
int sceGpuHeapFreeByGpuVA(GpuHeap *heap, IMG_UINT32 address,
		GpuHeapAllocation *output);
int ksceGpuMmuMapMemory(GpuMmuContext *context, IMG_UINT32 gpuAddress,
		IMG_UINT32 backingAddress, IMG_UINT32 size, IMG_UINT32 permissions);
int ShareKernelMappingWithProcesses(IMG_UINT32 address, IMG_SIZE_T size);
GpuMemBlockHeap *GetProcessMemBlockHeap(void);

/* SCE uses native heap spans and MMU permissions instead of IMG BM buffers. */
PVRSRV_ERROR AllocDeviceMem(GpuHeap *psHeap, IMG_SIZE_T ui32Size,
		IMG_SIZE_T ui32Alignment, IMG_UINT32 ui32AllocationType,
		IMG_UINT32 ui32Flags, IMG_SID hMemBlockProcRef,
		PVRSRV_KERNEL_MEM_INFO **ppsMemInfo)
{
	PVRSRV_KERNEL_MEM_INFO *psMemInfo;
	GpuHeapAllocation allocation;
	IMG_UINT32 permissions;
	int ret;

	psMemInfo = AllocProcessGpuMemory(sizeof(*psMemInfo));
	if (!psMemInfo)
		return PVRSRV_ERROR_OUT_OF_MEMORY;
	ret = sceGpuHeapAlloc(psHeap, ui32Size, ui32Alignment, ui32AllocationType,
			ui32Flags, hMemBlockProcRef, &allocation, 0);
	if (ret < 0) {
		SCE_DBG_PRINTF(0, 59,
				"%s: Heap alloc failed with error %08x.\n", __func__, ret);
		goto fail;
	}
	if (allocation.releasedSpanSize) {
		permissions = ui32Flags & 4 ? 8 : 0;
		if (ui32Flags & 0x40)
			permissions |= 0x10;
		if ((ui32Flags & 3) != 3) {
			if (ui32Flags & 1)
				permissions |= 4;
			if (ui32Flags & 2)
				permissions |= 2;
		}
		if (ui32AllocationType == 1 || ui32AllocationType == 4)
			permissions |= 0x7e0;
		ret = ksceGpuMmuMapMemory(&psHeap->memoryContext->mmu,
				allocation.releasedSpanBase,
				allocation.releasedBackingAddress,
				allocation.releasedSpanSize, permissions);
		if (ret < 0) {
			SCE_DBG_PRINTF(0, 88,
					"%s: Mapping failed with error %08x.\n", __func__, ret);
			sceGpuHeapFreeByGpuVA(psHeap, allocation.gpuAddress, &allocation);
			goto fail;
		}
		if (psHeap->shared == 1)
			ShareKernelMappingWithProcesses(allocation.releasedSpanBase,
					allocation.releasedSpanSize);
	}
	memset(psMemInfo, 0, sizeof(*psMemInfo));
	psMemInfo->ui32Flags = ui32Flags | 0x200;
	psMemInfo->uAllocSize = ui32Size;
	psMemInfo->pvLinAddrKM = (IMG_VOID *)allocation.alternateAddress;
	psMemInfo->hMemBlockProcRef = hMemBlockProcRef;
	psMemInfo->pvLinAddr = (IMG_VOID *)allocation.primaryAddress;
	psMemInfo->sDevVAddr.uiAddr = allocation.gpuAddress;
	psMemInfo->ui32RefCount++;
	psMemInfo->heap = psHeap;
	psMemInfo->memBlockHeap = GetProcessMemBlockHeap();
	*ppsMemInfo = psMemInfo;
	return PVRSRV_OK;
fail:
	FreeProcessGpuMemory(psMemInfo);
	SCE_DBG_PRINTF(0, 121, "%s: Failed with error %08x.\n",
			__func__, PVRSRV_ERROR_OUT_OF_MEMORY);
	return PVRSRV_ERROR_OUT_OF_MEMORY;
}

int ksceGpuMmuUnmapMemory(GpuMmuContext *context, IMG_UINT32 gpuAddress,
		IMG_SIZE_T size);

/* SCE frees native heap spans. The later TOOL source adds descriptor references;
 * the 3.650 original releases the descriptor on every invocation.
 */
PVRSRV_ERROR FreeDeviceMem(PVRSRV_KERNEL_MEM_INFO *psMemInfo)
{
	GpuHeapAllocation allocation;
	int ret;

#if FW_VERSION >= 0x04000000
	SCE_DBG_ASSERT(psMemInfo->ui32RefCount != 0, 135);
	if (--psMemInfo->ui32RefCount != 0)
		return PVRSRV_OK;
#endif
	allocation.releasedSpanSize = 0;
	ret = sceGpuHeapFreeByGpuVA(psMemInfo->heap, psMemInfo->sDevVAddr.uiAddr,
			&allocation);
	if (ret < 0)
		SCE_DBG_PRINTF(0, 143, "%s: Heap free error code %08x\n", __func__, ret);
	if (allocation.releasedSpanSize) {
		ret = ksceGpuMmuUnmapMemory(&psMemInfo->heap->memoryContext->mmu,
				allocation.releasedSpanBase, allocation.releasedSpanSize);
		if (ret < 0)
			SCE_DBG_PRINTF(0, 148, "%s: Unmap error code %08x\n", __func__, ret);
		if (psMemInfo->heap->shared == 1)
			ShareKernelMappingWithProcesses(allocation.releasedSpanBase,
					allocation.releasedSpanSize);
	}
#if FW_VERSION >= 0x04000000
	FreeProcessGpuMemory(psMemInfo);
	return PVRSRV_OK;
#else
	return (PVRSRV_ERROR)FreeProcessGpuMemory(psMemInfo);
#endif
}

/* SCE keeps IMG's resource callback boundary but moves the reference decrement
 * into FreeDeviceMem in the later TOOL source.
 */
PVRSRV_ERROR FreeDeviceMemCallBack(IMG_VOID *pvParam, IMG_UINT32 ui32Param)
{
	PVRSRV_KERNEL_MEM_INFO *psMemInfo = pvParam;

	(void)ui32Param;
	if (psMemInfo) {
#if FW_VERSION < 0x04000000
		if (--psMemInfo->ui32RefCount == 0)
#endif
			FreeDeviceMem(psMemInfo);
	}
	return PVRSRV_OK;
}

PVRSRV_DEVICE_NODE *GetGpuDeviceNode(void);
IMG_VOID *OSMemSet(IMG_VOID *destination, IMG_INT32 value, IMG_SIZE_T size);

/* Pristine devicemem.c heap enumeration with SCE's single-device lookup/assert. */
PVRSRV_ERROR PVRSRVGetDeviceMemHeapsKM(IMG_HANDLE hDevCookie,
		PVRSRV_HEAP_INFO *psHeapInfo)
{
	PVRSRV_DEVICE_NODE *psDeviceNode;
	DEVICE_MEMORY_HEAP_INFO *psDeviceMemoryHeap;
	IMG_UINT32 ui32HeapCount, i;

	if (!hDevCookie)
		return PVRSRV_ERROR_INVALID_PARAMS;
	psDeviceNode = GetGpuDeviceNode();
	SCE_DBG_ASSERT(psDeviceNode == hDevCookie, 184);
	ui32HeapCount = psDeviceNode->sDevMemoryInfo.ui32HeapCount;
	psDeviceMemoryHeap = psDeviceNode->sDevMemoryInfo.psDeviceMemoryHeap;
	for (i = 0; i != ui32HeapCount; i++) {
		psHeapInfo[i].ui32HeapID = psDeviceMemoryHeap[i].ui32HeapID;
		psHeapInfo[i].hDevMemHeap = psDeviceMemoryHeap[i].hDevMemHeap;
		psHeapInfo[i].sDevVAddrBase = psDeviceMemoryHeap[i].sDevVAddrBase;
		psHeapInfo[i].ui32HeapByteSize = psDeviceMemoryHeap[i].ui32HeapSize;
		psHeapInfo[i].ui32Attribs = psDeviceMemoryHeap[i].ui32Attribs;
	}
	for (; i < 32; i++) {
		OSMemSet(psHeapInfo + i, 0, sizeof(*psHeapInfo));
		psHeapInfo[i].ui32HeapID = (IMG_UINT32)-1;
	}
	return PVRSRV_OK;
}

int GetKernelMemoryContext(GpuMemoryContext **output);
int FindGpuHeapInMemoryContext(GpuMemoryContext *context, unsigned lookupKey,
		GpuHeap **output);
IMG_VOID *OSAllocMem(IMG_SIZE_T size);
int OSFreeMem(IMG_VOID *address);

/* IMG sync initialization with the SCE kernel-only heap/allocation policy. */
PVRSRV_ERROR PVRSRVAllocSyncInfoKM(PVRSRV_DEVICE_NODE *psDeviceNode,
		GpuMemoryContext *hDevMemContext, PVRSRV_KERNEL_SYNC_INFO **ppsSyncInfo)
{
	PVRSRV_KERNEL_SYNC_INFO *psSyncInfo;
	PVRSRV_SYNC_DATA *psSyncData;
	PVRSRV_DEVICE_NODE *psCurrentDeviceNode;
	GpuMemoryContext *psContext;
	GpuHeap *psHeap = 0;
	PVRSRV_ERROR eError;
	int ret;

	psCurrentDeviceNode = GetGpuDeviceNode();
	SCE_DBG_ASSERT(psDeviceNode == psCurrentDeviceNode, 223);
	ret = GetKernelMemoryContext(&psContext);
	if (ret < 0) {
		SCE_DBG_PRINTF(0, 227,
				"%s: Failed to get kernel memory context with error %08x.\n",
				__func__, ret);
		return PVRSRV_ERROR_INVALID_PARAMS;
	}
	ret = FindGpuHeapInMemoryContext(psContext, 0x7000008, &psHeap);
	if (ret < 0 || !psHeap) {
		SCE_DBG_PRINTF(0, 234, "%s: Context Error %08x Heap %p\n",
				__func__, ret, psHeap);
		return PVRSRV_ERROR_INVALID_PARAMS;
	}
	SCE_DBG_ASSERT(psContext == hDevMemContext || hDevMemContext == 0, 238);
	psSyncInfo = OSAllocMem(sizeof(*psSyncInfo));
	if (!psSyncInfo)
		return PVRSRV_ERROR_OUT_OF_MEMORY;
	eError = AllocDeviceMem(psHeap, sizeof(*psSyncInfo->psSyncData), 4, 2,
			0x608004, 1, &psSyncInfo->psSyncDataMemInfoKM);
	if (eError != PVRSRV_OK) {
		SCE_DBG_PRINTF(0, 248,
				"%s: Failed to allocate device memory with error %08x.\n",
				__func__, eError);
		OSFreeMem(psSyncInfo);
		return eError;
	}
	psSyncData = psSyncInfo->psSyncDataMemInfoKM->pvLinAddrKM;
	psSyncInfo->ui32RefCount = 0;
	psSyncInfo->psSyncData = psSyncData;
	psSyncInfo->psSyncData->ui32ReadOpsPending = 0;
	psSyncInfo->psSyncData->ui32WriteOpsComplete = 0;
	psSyncInfo->psSyncData->ui32WriteOpsPending = 0;
	psSyncInfo->psSyncData->ui32ReadOpsComplete = 0;
	psSyncInfo->psSyncData->ui32LastOpDumpVal = 0;
	psSyncInfo->psSyncData->ui32LastReadOpDumpVal = 0;
	psSyncInfo->sWriteOpsCompleteDevVAddr.uiAddr =
			psSyncInfo->psSyncDataMemInfoKM->sDevVAddr.uiAddr +
			offsetof(PVRSRV_SYNC_DATA, ui32WriteOpsComplete);
	psSyncInfo->ui32SharedKey = 0;
	psSyncInfo->sReadOpsCompleteDevVAddr.uiAddr =
			psSyncInfo->psSyncDataMemInfoKM->sDevVAddr.uiAddr +
			offsetof(PVRSRV_SYNC_DATA, ui32ReadOpsComplete);
	*ppsSyncInfo = psSyncInfo;
	return eError;
}

/* SCE sync destruction uses its native descriptor lifetime and debug boundary. */
PVRSRV_ERROR PVRSRVFreeSyncInfoKM(PVRSRV_KERNEL_SYNC_INFO *psSyncInfo)
{
#if FW_VERSION >= 0x04000000
	int ret;

	SCE_DBG_ASSERT(psSyncInfo->ui32RefCount == 0, 283);
	ret = FreeDeviceMem(psSyncInfo->psSyncDataMemInfoKM);
	if (ret < 0) {
		SCE_DBG_PRINTF(0, 287,
				"%s: Failed to free sync object with error %08x!\n",
				__func__, ret);
		return PVRSRV_ERROR_INVALID_PARAMS;
	}
#else
	if (--psSyncInfo->psSyncDataMemInfoKM->ui32RefCount == 0)
		FreeDeviceMem(psSyncInfo->psSyncDataMemInfoKM);
#endif
	OSFreeMem(psSyncInfo);
	return PVRSRV_OK;
}

struct Resource *ResManRegisterRes(struct ResourceContext *context,
		IMG_UINT32 resourceType, IMG_VOID *param, IMG_UINT32 value,
		PVRSRV_ERROR (*callback)(IMG_VOID *, IMG_UINT32));

/* SCE replaces IMG's generic cache/mapping policy with native heap attributes.
 * Resource registration remains at the original IMG allocation boundary.
 */
PVRSRV_ERROR PVRSRVAllocDeviceMemKM(PVRSRV_DEVICE_NODE *psDeviceNode,
		PVRSRV_PER_PROCESS_DATA *psPerProc, GpuHeap *heap,
		IMG_UINT32 ui32Attribs, IMG_SIZE_T ui32Size, IMG_SIZE_T ui32Alignment,
		IMG_INT32 hMemBlockProcRef, PVRSRV_KERNEL_MEM_INFO **ppsMemInfo)
{
	PVRSRV_DEVICE_NODE *psCurrentDeviceNode;
	PVRSRV_KERNEL_MEM_INFO *psMemInfo;
	IMG_UINT32 ui32AllocationType;
	PVRSRV_ERROR eError;

	psCurrentDeviceNode = GetGpuDeviceNode();
#if FW_DEBUG
	SceDebugForDriver_35A35322(psDeviceNode == psCurrentDeviceNode,
			SCE_DBGINFO_FUNC(300, "_PVRSRVAllocDeviceMemKM"),
			__builtin_return_address(0));
#endif
	if (!heap || !ui32Size || !ppsMemInfo || !(ui32Attribs & 0x400000))
		return PVRSRV_ERROR_INVALID_PARAMS;
	if (!(ui32Attribs & 0x7000))
		ui32Attribs |= heap->attributes & 0x7000;
	ui32Attribs |= heap->attributes & 0x100000;
	switch (ui32Attribs & 0x7000) {
	case 0x2000:
	case 0x4000:
		ui32AllocationType = ui32Attribs & 0x8000 ? 2 : 5;
		break;
	case 0x1000:
		ui32AllocationType = ui32Attribs & 0x8000 ? 1 : 4;
		break;
	default:
		return PVRSRV_ERROR_INVALID_PARAMS;
	}
	if (ui32Attribs & 0x100000)
		ui32AllocationType = ui32Attribs & 0x8000 ? 3 : 6;
	eError = AllocDeviceMem(heap, ui32Size, ui32Alignment, ui32AllocationType,
			ui32Attribs, hMemBlockProcRef, &psMemInfo);
	if (eError == PVRSRV_OK) {
		psMemInfo->hResItem = ResManRegisterRes(psPerProc->hResManContext,
				16, psMemInfo, 0,
				FreeDeviceMemCallBack);
		*ppsMemInfo = psMemInfo;
	}
	return eError;
}

PVRSRV_ERROR ResManFreeResByPtr(struct Resource *resource, IMG_BOOL forceCleanup);

/* IMG public release boundary; SCE ignores the resource cleanup result. */
PVRSRV_ERROR PVRSRVFreeDeviceMemKM(PVRSRV_DEVICE_NODE *psDeviceNode,
		PVRSRV_KERNEL_MEM_INFO *psMemInfo)
{
	PVRSRV_DEVICE_NODE *psCurrentDeviceNode;

	psCurrentDeviceNode = GetGpuDeviceNode();
	SCE_DBG_ASSERT(psDeviceNode == psCurrentDeviceNode || psDeviceNode == 0, 362);
	if (!psMemInfo)
		return PVRSRV_ERROR_INVALID_PARAMS;
	if (psMemInfo->hResItem)
		ResManFreeResByPtr(psMemInfo->hResItem, IMG_FALSE);
	else
		FreeDeviceMemCallBack(psMemInfo, 0);
	return PVRSRV_OK;
}

int GetMemoryContext(GpuMemoryContext **output, unsigned pid);
int AddMemoryContextToList(GpuMemoryContext **output, unsigned pid);
int sceGpuMmuAllocatePageTablePool(GpuPageTablePool *pool, unsigned pid);
int AllocateGpuPageDirectory(GpuMmuContext *context);
int FreeGpuPageTablePool(GpuPageTablePool *pool);
int sceGpuMmuShareRange(GpuMmuContext *destination, GpuMmuContext *source,
		unsigned address, unsigned size);
int CreateGpuHeap(unsigned base, unsigned size, unsigned lookupKey,
		unsigned attributes, GpuHeap **output);
int AddGpuHeapToMemoryContext(GpuMemoryContext *context, GpuHeap *heap);

/* SCE native contexts replace IMG's BM context. The upstream heap-query and
 * shared/per-context policy remain, with Sony's process page-directory setup.
 */
PVRSRV_ERROR PVRSRVCreateDeviceMemContextKM(PVRSRV_DEVICE_NODE *psDeviceNode,
		PVRSRV_PER_PROCESS_DATA *psPerProc, GpuMemoryContext **context,
		IMG_UINT32 *pui32ClientHeapCount, PVRSRV_HEAP_INFO *psHeapInfo,
		IMG_BOOL *pbCreated, IMG_BOOL *pbSharedDeviceMemHeap)
{
	PVRSRV_DEVICE_NODE *psCurrentDeviceNode;
	DEVICE_MEMORY_HEAP_INFO *psDeviceMemoryHeap;
	GpuMemoryContext *psContext, *psKernelContext;
	GpuProcessMemory *processMemory;
	GpuHeap *heap;
	SceUID pid;
	IMG_UINT32 ui32HeapCount, ui32ClientHeapCount = 0, i;
	int ret;

	psCurrentDeviceNode = GetGpuDeviceNode();
	SCE_DBG_ASSERT(psCurrentDeviceNode == psDeviceNode, 394);
	SCE_DBG_ASSERT(psPerProc != 0, 396);
	if (pbCreated)
		*pbCreated = IMG_FALSE;
	if (!psDeviceNode || GetKernelMemoryContext(&psKernelContext) < 0)
		return PVRSRV_ERROR_INVALID_PARAMS;
	pid = ksceKernelGetProcessId();
	if (!sceSblACMgrIsGameProgram(pid) || GetMemoryContext(&psContext, pid) != 0) {
		if (AddMemoryContextToList(&psContext, pid) < 0)
			return PVRSRV_ERROR_OUT_OF_MEMORY;
		if (sceGpuMmuAllocatePageTablePool(&psContext->mmu.pageTablePool, pid) < 0)
			goto removeContext;
	}
	if (AllocateGpuPageDirectory(&psContext->mmu) < 0) {
		FreeGpuPageTablePool(&psContext->mmu.pageTablePool);
		goto removeContext;
	}
	processMemory = ksceKernelGetProcessLocalStorageAddr(psKernelContext->processLocalStorageUid);
	if (processMemory) {
		processMemory->pageDirectoryMemblockUid = psContext->mmu.pageDirectoryMemblockUid;
		processMemory->pageDirectory = psContext->mmu.pageDirectory;
		processMemory->pageDirectoryPhysicalAddress = psContext->mmu.pageDirectoryPhysicalAddress;
		processMemory->reserved = 0;
	} else {
		SCE_DBG_PRINTF(0, 444, "%s: Failed to get kernel process memory.\n", __func__);
	}
	if (pbCreated)
		*pbCreated = IMG_TRUE;
	ui32HeapCount = psCurrentDeviceNode->sDevMemoryInfo.ui32HeapCount;
	psDeviceMemoryHeap = psCurrentDeviceNode->sDevMemoryInfo.psDeviceMemoryHeap;
	for (i = 0; i != ui32HeapCount; i++, psDeviceMemoryHeap++) {
		switch (psDeviceMemoryHeap->DevMemHeapType) {
		case 3:
			ret = sceGpuMmuShareRange(&psContext->mmu, &psKernelContext->mmu,
					psDeviceMemoryHeap->sDevVAddrBase.uiAddr,
					psDeviceMemoryHeap->ui32HeapSize);
			if (ret < 0)
				SCE_DBG_PRINTF(0, 475, "%s: Share Heap Error %08x\n", __func__, ret);
			ret = FindGpuHeapInMemoryContext(psKernelContext,
					psDeviceMemoryHeap->ui32HeapID, &heap);
			if (ret < 0)
				SCE_DBG_PRINTF(0, 479, "%s: Find Heap Error %08x\n", __func__, ret);
			psHeapInfo[ui32ClientHeapCount].ui32HeapID = psDeviceMemoryHeap->ui32HeapID;
			psHeapInfo[ui32ClientHeapCount].hDevMemHeap = heap;
			psHeapInfo[ui32ClientHeapCount].sDevVAddrBase = psDeviceMemoryHeap->sDevVAddrBase;
			psHeapInfo[ui32ClientHeapCount].ui32HeapByteSize = psDeviceMemoryHeap->ui32HeapSize;
			psHeapInfo[ui32ClientHeapCount].ui32Attribs = psDeviceMemoryHeap->ui32Attribs;
			pbSharedDeviceMemHeap[ui32ClientHeapCount++] = IMG_TRUE;
			break;
		case 0:
			ret = CreateGpuHeap(psDeviceMemoryHeap->sDevVAddrBase.uiAddr,
					psDeviceMemoryHeap->ui32HeapSize,
					psDeviceMemoryHeap->ui32HeapID,
					psDeviceMemoryHeap->ui32Attribs, &heap);
			heap->shared = 0;
			AddGpuHeapToMemoryContext(psContext, heap);
			if (ret < 0)
				SCE_DBG_PRINTF(0, 503, "%s: Add Heap Error %08x\n", __func__, ret);
			psHeapInfo[ui32ClientHeapCount].ui32HeapID = psDeviceMemoryHeap->ui32HeapID;
			psHeapInfo[ui32ClientHeapCount].hDevMemHeap = heap;
			psHeapInfo[ui32ClientHeapCount].sDevVAddrBase = psDeviceMemoryHeap->sDevVAddrBase;
			psHeapInfo[ui32ClientHeapCount].ui32HeapByteSize = psDeviceMemoryHeap->ui32HeapSize;
			psHeapInfo[ui32ClientHeapCount].ui32Attribs = psDeviceMemoryHeap->ui32Attribs;
			pbSharedDeviceMemHeap[ui32ClientHeapCount++] = IMG_FALSE;
			break;
		}
	}
	*pui32ClientHeapCount = ui32ClientHeapCount;
	*context = psContext;
	psContext->hResItem = ResManRegisterRes(psPerProc->hResManContext, 12,
			psContext, ksceKernelGetProcessId(),
			(PVRSRV_ERROR (*)(IMG_VOID *, IMG_UINT32))DestroyDeviceMemContextCallback);
	return PVRSRV_OK;
removeContext:
	RemoveMemoryContextFromList(psContext, pid);
	return PVRSRV_ERROR_OUT_OF_MEMORY;
}

/* SCE rejects live native spans before invoking the context resource cleanup. */
PVRSRV_ERROR PVRSRVDestroyDeviceMemContextKM(PVRSRV_DEVICE_NODE *psDeviceNode,
		GpuMemoryContext *context, IMG_BOOL *pbDestroyed)
{
	PVRSRV_DEVICE_NODE *psCurrentDeviceNode;
	GpuHeap *heap;
	PVRSRV_ERROR eError;

	psCurrentDeviceNode = GetGpuDeviceNode();
	SCE_DBG_ASSERT(psDeviceNode == psCurrentDeviceNode, 538);
	if (pbDestroyed)
		*pbDestroyed = IMG_FALSE;
	for (heap = context->heaps; heap; heap = heap->next) {
		if (heap->spans)
			return PVRSRV_ERROR_UNABLE_TO_DESTROY_BM_HEAP;
	}
	if (context->hResItem)
		eError = ResManFreeResByPtr(context->hResItem, IMG_FALSE);
	else
		eError = DestroyDeviceMemContextCallback(context, ksceKernelGetProcessId());
	if (pbDestroyed)
		*pbDestroyed = IMG_TRUE;
	return eError;
}

/* IMG heap-query layout; SCE obtains live native heaps from each context. */
PVRSRV_ERROR PVRSRVGetDeviceMemHeapInfoKM(PVRSRV_DEVICE_NODE *psDeviceNode,
		GpuMemoryContext *context, IMG_UINT32 *pui32ClientHeapCount,
		PVRSRV_HEAP_INFO *psHeapInfo, IMG_BOOL *pbSharedDeviceMemHeap)
{
	PVRSRV_DEVICE_NODE *psCurrentDeviceNode;
	DEVICE_MEMORY_HEAP_INFO *psDeviceMemoryHeap;
	GpuMemoryContext *psKernelContext;
	GpuHeap *heap;
	IMG_UINT32 ui32HeapCount, ui32ClientHeapCount = 0, i;
	int ret;

	if (!psDeviceNode || GetKernelMemoryContext(&psKernelContext) < 0)
		return PVRSRV_ERROR_INVALID_PARAMS;
	psCurrentDeviceNode = GetGpuDeviceNode();
	SCE_DBG_ASSERT(psCurrentDeviceNode == psDeviceNode, 592);
	ui32HeapCount = psCurrentDeviceNode->sDevMemoryInfo.ui32HeapCount;
	psDeviceMemoryHeap = psCurrentDeviceNode->sDevMemoryInfo.psDeviceMemoryHeap;
	for (i = 0; i != ui32HeapCount; i++, psDeviceMemoryHeap++) {
		switch (psDeviceMemoryHeap->DevMemHeapType) {
		case 3:
			ret = FindGpuHeapInMemoryContext(psKernelContext,
					psDeviceMemoryHeap->ui32HeapID, &heap);
			if (ret < 0) {
				SCE_DBG_PRINTF(0, 616,
						"%s: Heap %08x not found with error %08x.\n",
						__func__, psDeviceMemoryHeap->ui32HeapID, ret);
				break;
			}
			psHeapInfo[ui32ClientHeapCount].ui32HeapID = heap->lookupKey;
			psHeapInfo[ui32ClientHeapCount].hDevMemHeap = heap;
			psHeapInfo[ui32ClientHeapCount].sDevVAddrBase.uiAddr = heap->base;
			psHeapInfo[ui32ClientHeapCount].ui32HeapByteSize = heap->size;
			psHeapInfo[ui32ClientHeapCount].ui32Attribs = heap->attributes;
			pbSharedDeviceMemHeap[ui32ClientHeapCount++] = IMG_TRUE;
			break;
		case 0:
			ret = FindGpuHeapInMemoryContext(context,
					psDeviceMemoryHeap->ui32HeapID, &heap);
			if (ret < 0) {
				SCE_DBG_PRINTF(0, 636,
						"%s: Heap %08x not found with error %08x.\n",
						__func__, psDeviceMemoryHeap->ui32HeapID, ret);
				break;
			}
			psHeapInfo[ui32ClientHeapCount].ui32HeapID = heap->lookupKey;
			psHeapInfo[ui32ClientHeapCount].hDevMemHeap = heap;
			psHeapInfo[ui32ClientHeapCount].sDevVAddrBase.uiAddr = heap->base;
			psHeapInfo[ui32ClientHeapCount].ui32HeapByteSize = heap->size;
			psHeapInfo[ui32ClientHeapCount].ui32Attribs = heap->attributes;
			pbSharedDeviceMemHeap[ui32ClientHeapCount++] = IMG_FALSE;
			break;
		}
	}
	*pui32ClientHeapCount = ui32ClientHeapCount;
	return PVRSRV_OK;
}

/* SCE validates native memblocks and tracks per-process GPU mappings. */
PVRSRV_ERROR PVRSRVMapMemoryToGpuKM(PVRSRV_PER_PROCESS_DATA *psPerProc,
		PVRSRV_DEVICE_NODE *psDeviceNode, GpuMemoryContext *context,
		GpuHeap *heap, IMG_VOID *cpuAddress, IMG_SIZE_T size,
		IMG_SIZE_T allocationSize, IMG_UINT32 flags,
		IMG_DEV_VIRTADDR *deviceAddress)
{
	MemBlockPhyInfo info;
	GpuHeapAllocation allocation;
	GpuHeap *selectedHeap;
	GpuMappedMemoryList *list;
	GpuMappedMemory *record;
	IMG_UINT32 accessFlags, heapID, mappingFlags, permissions = 0;
	SceUID guid;
	int region, memtype, ret;

	if (!psPerProc || !psDeviceNode || !context || (size & 0xfff) ||
			((IMG_UINTPTR_T)cpuAddress & 0xfff) || !(flags & 3) ||
			(flags & ~0x103u) || (!(flags & 0x100) && !deviceAddress) ||
			(allocationSize && size > allocationSize))
		return PVRSRV_ERROR_INVALID_PARAMS;
	accessFlags = flags & 1 ? 0x10 : 0;
	if (flags & 2)
		accessFlags |= 0x20;
	if (SceSysmemForDriver_9C78064C(accessFlags, cpuAddress, size) < 0)
		return PVRSRV_ERROR_INVALID_PARAMS;
	guid = SceSysmemForDriver_8A1742F6(cpuAddress, 0);
	if (guid < 0)
		return PVRSRV_ERROR_INVALID_PARAMS;
	memset(&info, 0, sizeof(info));
	info.ex.size = sizeof(info);
	if (SceSysmemForDriver_24A99FFF(guid, &info) < 0 ||
			(SceSysmemForDriver_6A0792A3(info.ex.details.type) & 0x44))
		return PVRSRV_ERROR_INVALID_PARAMS;
	selectedHeap = heap;
	region = SceSysmemForDriver_CB0F3A33(info.ex.details.type);
	if (region == 0x200000) {
		memtype = SceSysmemForDriver_20C811FA(info.ex.details.type);
		if (memtype == 0x8000) {
			heapID = 0x07000014;
			mappingFlags = 0;
		} else if (memtype == 0xd000) {
			heapID = 0x0700000e;
			mappingFlags = 4;
			permissions = 0x7e0;
		} else {
			SCE_DBG_PRINTF(0, 758,
					"%s: memblock %08x for %p has invalid source memory %08x\n",
					__func__, guid, cpuAddress,
					SceSysmemForDriver_CB0F3A33(info.ex.details.type));
			return PVRSRV_ERROR_INVALID_PARAMS;
		}
	} else if (region == 0x400000) {
		heapID = 0x0700000f;
		mappingFlags = 8;
	} else {
		SCE_DBG_PRINTF(0, 769,
				"%s: memblock %08x for %p has invalid source memory %08x\n",
				__func__, guid, cpuAddress,
				SceSysmemForDriver_CB0F3A33(info.ex.details.type));
		return PVRSRV_ERROR_INVALID_PARAMS;
	}
	if ((flags & 0x100) &&
			FindGpuHeapInMemoryContext(context, heapID, &selectedHeap) < 0) {
		SCE_DBG_PRINTF(0, 778, "%s: GPU Heap %08x not found\n", __func__, heapID);
		return PVRSRV_ERROR_INVALID_PARAMS;
	}
	if ((IMG_UINTPTR_T)cpuAddress + size >
			(IMG_UINTPTR_T)info.ex.details.vbase + info.ex.details.size)
		return PVRSRV_ERROR_INVALID_PARAMS;
	if (!psPerProc->memoryContext) {
		psPerProc->memoryContext = AllocProcessGpuMemory(sizeof(*list));
		if (!psPerProc->memoryContext)
			return PVRSRV_ERROR_OUT_OF_MEMORY;
		memset(psPerProc->memoryContext, 0, sizeof(*list));
	}
	list = psPerProc->memoryContext;
	if (psPerProc->sdkVersion < 0x02100000)
		ret = InsertMappedMemoryLegacy(list, cpuAddress, size, flags,
				selectedHeap, &record);
	else
		ret = InsertMappedMemory(list, cpuAddress, size, flags, selectedHeap, &record);
	if (ret < 0)
		return ret == (int)0x804c0006 ? PVRSRV_ERROR_BAD_MAPPING :
				PVRSRV_ERROR_INVALID_PARAMS;
	SceSysmemForDriver_EAF3849B(guid);
	if ((flags & 3) == 3) {
		mappingFlags |= 3;
	} else {
		if (flags & 1) {
			permissions |= 4;
			mappingFlags |= 1;
		}
		if (flags & 2) {
			permissions |= 2;
			mappingFlags |= 2;
		}
	}
	record->mmuFlags = mappingFlags;
	if (heap) {
		ret = sceGpuHeapAlloc(selectedHeap, size, 4096, 7, flags, 0,
				&allocation, allocationSize);
		if (ret < 0) {
			SCE_DBG_PRINTF(0, 833,
					"%s: Failed to allocate VA with error %08x.\n", __func__, ret);
			RemoveMappedMemory(list, cpuAddress, NULL, selectedHeap);
			SceSysmemForDriver_F50BDC0C(guid);
			return PVRSRV_ERROR_OUT_OF_MEMORY;
		}
		record->deviceAddress.uiAddr = allocation.releasedSpanBase;
	} else {
		allocation.releasedSpanBase = (IMG_UINTPTR_T)cpuAddress;
		allocation.releasedSpanSize = size;
		record->deviceAddress.uiAddr = (IMG_UINTPTR_T)cpuAddress;
	}
	allocation.releasedBackingAddress = (IMG_UINTPTR_T)cpuAddress;
	ret = ksceGpuMmuMapMemory(&selectedHeap->memoryContext->mmu,
			allocation.releasedSpanBase, (IMG_UINTPTR_T)cpuAddress,
			allocation.releasedSpanSize, permissions);
	if (ret < 0) {
		SCE_DBG_PRINTF(0, 851,
				"%s: Failed to map memory with error %08x.\n", __func__, ret);
		RemoveMappedMemory(list, cpuAddress, NULL, selectedHeap);
		SceSysmemForDriver_F50BDC0C(guid);
		return PVRSRV_ERROR_BAD_MAPPING;
	}
	if (deviceAddress)
		deviceAddress->uiAddr = allocation.releasedSpanBase;
	record->resource = ResManRegisterRes(psPerProc->hResManContext, 15,
			cpuAddress, ksceKernelGetProcessId(),
			(PVRSRV_ERROR (*)(IMG_VOID *, IMG_UINT32))ReleaseDeviceMemBlockCallback);
	return PVRSRV_OK;
}

/* SCE releases the global services lock while waiting for process GPU work. */
PVRSRV_ERROR PVRSRVUnmapMemoryFromGpuKM(PVRSRV_PER_PROCESS_DATA *psPerProc,
		const IMG_VOID *cpuAddress, GpuHeap *heap, int bWaitForIdle)
{
	GpuMappedMemoryList *list;
	GpuMappedMemory output;
	GpuHeapAllocation allocation;
	PVRSRV_KERNEL_MEM_INFO *memoryInfo;
	volatile IMG_UINT32 *completion;
	IMG_UINT32 pending, i;
	SceUID pid;
	PVRSRV_ERROR eError;
	int ret;

	if (!cpuAddress)
		return PVRSRV_ERROR_INVALID_PARAMS;
	list = psPerProc->memoryContext;
	if (!list) {
		SCE_DBG_PRINTF(0, 876, "%s: Failed to obtain range list\n", __func__);
		return PVRSRV_ERROR_INVALID_PARAMS;
	}
	if (CheckMappedMemory(list, cpuAddress, 0, heap) != 0) {
		SceDebugForDriver_02B04343(0, SCE_DBGINFO(884),
				"%s: Range check failed for VA %08x\n", __func__, cpuAddress);
		return PVRSRV_ERROR_INVALID_PARAMS;
	}
	pid = ksceKernelGetProcessId();
	if (bWaitForIdle) {
		psPerProc->unmapInProgress = IMG_TRUE;
		for (i = 0; i < psPerProc->numSourceSyncs; i++) {
			if (!psPerProc->sourceSyncs[i].memoryHandle)
				continue;
			memoryInfo = NULL;
			if (PVRSRVLookupHandle(psPerProc->psHandleBase,
					(IMG_VOID **)&memoryInfo,
					(IMG_SID)psPerProc->sourceSyncs[i].memoryHandle, 5) != PVRSRV_OK)
				continue;
			completion = (volatile IMG_UINT32 *)((IMG_UINTPTR_T)memoryInfo->pvLinAddrKM -
					memoryInfo->sDevVAddr.uiAddr +
					psPerProc->sourceSyncs[i].syncAddress.uiAddr);
			pending = psPerProc->sourceSyncs[i].pendingValue;
			if (*completion != pending) {
				LinuxUnLockMutex(&gPVRSRVLock);
				while (*completion != pending) {
					if (gpuSuspended || exitingProcess == pid)
						goto retry;
					WaitSignalCounterKernel(&unmapWaitGeneration, 100000);
				}
				LinuxLockMutex(&gPVRSRVLock);
			}
		}
		for (i = 0; i < psPerProc->numDestinationSyncs; i++) {
			if (!psPerProc->destinationSyncs[i].memoryHandle)
				continue;
			memoryInfo = NULL;
			if (PVRSRVLookupHandle(psPerProc->psHandleBase,
					(IMG_VOID **)&memoryInfo,
					(IMG_SID)psPerProc->destinationSyncs[i].memoryHandle, 5) != PVRSRV_OK)
				continue;
			completion = (volatile IMG_UINT32 *)((IMG_UINTPTR_T)memoryInfo->pvLinAddrKM -
					memoryInfo->sDevVAddr.uiAddr +
					psPerProc->destinationSyncs[i].syncAddress.uiAddr);
			pending = psPerProc->destinationSyncs[i].pendingValue;
			if (*completion != pending) {
				LinuxUnLockMutex(&gPVRSRVLock);
				while (*completion != pending) {
					if (gpuSuspended || exitingProcess == pid)
						goto retry;
					WaitSignalCounterKernel(&unmapWaitGeneration, 100000);
				}
				LinuxLockMutex(&gPVRSRVLock);
			}
		}
		if (psPerProc->transferQueue) {
			eError = WaitTransferQueueIdle(psPerProc->transferQueue);
			if (eError != PVRSRV_OK) {
				psPerProc->unmapInProgress = IMG_FALSE;
				return eError;
			}
		}
		psPerProc->unmapInProgress = IMG_FALSE;
	}
	if (RemoveMappedMemory(list, (IMG_VOID *)cpuAddress, &output, heap) < 0) {
		SCE_DBG_PRINTF(0, 978, "%s: Failed to delete range %p\n", __func__, cpuAddress);
		return PVRSRV_ERROR_INVALID_PARAMS;
	}
	heap = output.heap;
	if (!(output.flags & 0x100)) {
		ret = sceGpuHeapFreeByGpuVA(heap, output.deviceAddress.uiAddr, &allocation);
		if (ret < 0)
			SCE_DBG_PRINTF(0, 996,
					"%s: Failed to free VA %08x with error %08x.\n",
					__func__, output.deviceAddress.uiAddr, ret);
		SCE_DBG_ASSERT(allocation.releasedSpanBase == output.deviceAddress.uiAddr, 998);
		SCE_DBG_ASSERT(allocation.releasedSpanSize == output.size, 999);
	}
	ret = ksceGpuMmuUnmapMemory(&heap->memoryContext->mmu,
			output.deviceAddress.uiAddr, output.size);
	if (ret < 0)
		SCE_DBG_PRINTF(0, 1004,
				"%s: Failed to unmap memory %08x+%08x with error %08x.\n",
				__func__, output.deviceAddress.uiAddr, output.size, ret);
	return ResManFreeResByPtr(output.resource, IMG_FALSE);
retry:
	LinuxLockMutex(&gPVRSRVLock);
	psPerProc->unmapInProgress = IMG_FALSE;
	return PVRSRV_ERROR_RETRY;
}

/* SCE registers process memblocks as backing allocations after map validation. */
PVRSRV_ERROR PVRSRVRegisterMemBlockKM(PVRSRV_PER_PROCESS_DATA *psPerProc,
		IMG_INT32 memBlockUid, IMG_UINT32 *blockGroupId, IMG_BOOL restoreMapping)
{
	MemBlockPhyInfo info;
#if FW_DEBUG
	const SceKernelDebugInfo *typeDebugInfo;
#endif
	SceUID guid;
	int region, ret;

	if (!blockGroupId)
		return PVRSRV_ERROR_INVALID_PARAMS;
	guid = SceSysmemForDriver_45D22597(ksceKernelGetProcessId(), memBlockUid);
	if (guid < 0) {
		SCE_DBG_PRINTF(0, 1020, "%s: Failed to get GUID for MemBlock %08x\n",
				__func__, memBlockUid);
		return PVRSRV_ERROR_INVALID_PARAMS;
	}
	memset(&info, 0, sizeof(info));
	info.ex.size = sizeof(info);
	ret = SceSysmemForDriver_24A99FFF(guid, &info);
	if (ret < 0) {
		SCE_DBG_PRINTF(0, 1030,
				"%s: sceKernelGetMemBlockInfo failed for %08x:%08x with error %08x\n",
				__func__, memBlockUid, guid, ret);
		return PVRSRV_ERROR_INVALID_PARAMS;
	}
	if (SceSysmemForDriver_6A0792A3(info.ex.details.type) & 0x44)
		return PVRSRV_ERROR_INVALID_PARAMS;
	region = SceSysmemForDriver_CB0F3A33(info.ex.details.type);
	if (region == 0x200000 || region == 0x400000) {
		if (SceSysmemForDriver_20C811FA(info.ex.details.type) != 0x8000) {
#if FW_DEBUG
			typeDebugInfo = SCE_DBGINFO(1044);
#endif
			goto unsupportedType;
		}
	} else {
#if FW_DEBUG
		typeDebugInfo = SCE_DBGINFO(1049);
#endif
		goto unsupportedType;
	}
	if (CheckMappedMemoryOverlap(psPerProc->memoryContext,
			(IMG_VOID *)info.ex.details.vbase, info.ex.details.size) == 0)
		return PVRSRV_ERROR_BAD_MAPPING;
	if (!psPerProc->memBlockHeap) {
		psPerProc->memBlockHeap = AllocProcessGpuMemory(sizeof(*psPerProc->memBlockHeap));
		if (!psPerProc->memBlockHeap)
			return PVRSRV_ERROR_OUT_OF_MEMORY;
		memset(psPerProc->memBlockHeap, 0, sizeof(*psPerProc->memBlockHeap));
	}
	ret = sceGpuMemBlockAddBlock(psPerProc->memBlockHeap, memBlockUid,
			blockGroupId, restoreMapping);
	if (ret < 0) {
		SCE_DBG_PRINTF(0, 1070, "%s: Failed to add block %08x with error %08x\n",
				__func__, memBlockUid, ret);
		return PVRSRV_ERROR_INVALID_PARAMS;
	}
	return PVRSRV_OK;
unsupportedType:
#if FW_DEBUG
	SceDebugForDriver_D9703808(1, 0, typeDebugInfo,
			"%s: MemBlock ID %08x:%08x has an unsupported type %08x\n",
			__func__, memBlockUid, guid, info.ex.details.type);
#endif
	return PVRSRV_ERROR_INVALID_PARAMS;
}

PVRSRV_ERROR PVRSRVUnregisterMemBlockKM(PVRSRV_PER_PROCESS_DATA *psPerProc,
		IMG_INT32 memBlockUid)
{
	SceUID referencedUid;
	int ret;

	if (!psPerProc->memBlockHeap)
		return PVRSRV_ERROR_INVALID_PARAMS;
	ret = sceGpuMemBlockDelBlock(psPerProc->memBlockHeap, memBlockUid, &referencedUid);
	if (ret < 0) {
		SCE_DBG_PRINTF(0, 1089, "%s: Failed to del block %08x with error %08x\n",
				__func__, memBlockUid, ret);
		return PVRSRV_ERROR_INVALID_PARAMS;
	}
	return PVRSRV_OK;
}

GpuMappedMemory *CreateMappedMemory(IMG_VOID *address, IMG_SIZE_T size,
		IMG_UINT32 flags, GpuHeap *heap)
{
	GpuMappedMemory *record;

	record = AllocProcessGpuMemory(sizeof(*record));
	if (record) {
		memset(record, 0, sizeof(*record));
		record->cpuAddress = address;
		record->size = size;
		record->flags = flags;
		record->heap = heap;
		record->resource = NULL;
	}
	return record;
}

/* SCE compatibility path preserves the pre-2.10 endpoint overlap test. */
int InsertMappedMemoryLegacy(GpuMappedMemoryList *list, IMG_VOID *address,
		IMG_SIZE_T size, IMG_UINT32 flags, GpuHeap *heap,
		GpuMappedMemory **output)
{
	GpuMappedMemory *record, *previous, *next, *insert;
	IMG_UINTPTR_T begin = (IMG_UINTPTR_T)address, end = begin + size;

	if (!list || !output)
		return (int)0x804c0000;
	record = list->head;
	if (!record) {
		insert = CreateMappedMemory(address, size, flags, heap);
		if (!insert)
			return (int)0x804c0001;
		insert->next = NULL;
		list->head = insert;
		*output = insert;
		return 0;
	}
	previous = NULL;
	for (;;) {
		if (record->heap == heap) {
			if (begin >= (IMG_UINTPTR_T)record->cpuAddress &&
					begin < (IMG_UINTPTR_T)record->cpuAddress + record->size)
				return (int)0x804c0006;
			if (end <= (IMG_UINTPTR_T)record->cpuAddress) {
				next = record;
				break;
			}
			if (end < (IMG_UINTPTR_T)record->cpuAddress + record->size)
				return (int)0x804c0006;
		}
		next = record->next;
		previous = record;
		if (!next)
			break;
		record = next;
	}
	insert = CreateMappedMemory(address, size, flags, heap);
	if (!insert)
		return (int)0x804c0001;
	insert->next = next;
	if (previous)
		previous->next = insert;
	else
		list->head = insert;
	*output = insert;
	return 0;
}

/* SCE: queue the native MMU cache request for the next SGX command. */
void SetGpuCacheControl(IMG_UINT32 flags)
{
	PVRSRV_DEVICE_NODE *psDeviceNode;
	PVRSRV_SGXDEV_INFO *psDevInfo;

	psDeviceNode = GetGpuDeviceNode();
	psDevInfo = psDeviceNode->pvDevice;
	psDevInfo->ui32CacheControl |= flags;
}

PVRSRV_ERROR QueryMappedMemory(GpuMappedMemoryList *list, IMG_VOID *cpuAddress,
		GpuMappedMemory *info)
{
	GpuMappedMemory *record;

	if (!list)
		goto invalid;
	if (!info)
		goto invalid;
	for (record = list->head; ; record = record->next) {
		if (!record)
			return (PVRSRV_ERROR)0x804c0005;
		if ((IMG_UINTPTR_T)cpuAddress < (IMG_UINTPTR_T)record->cpuAddress)
			continue;
		if ((IMG_UINTPTR_T)cpuAddress >= (IMG_UINTPTR_T)record->cpuAddress + record->size)
			continue;
		if (!(record->flags & 0x100))
			continue;
		*info = *record;
		return PVRSRV_OK;
	}
invalid:
	return (PVRSRV_ERROR)0x804c0000;
}

int CheckMappedMemoryOverlap(GpuMappedMemoryList *list, IMG_VOID *address,
		IMG_SIZE_T size)
{
	GpuMappedMemory *record;

	if (!list)
		return (int)0x804c0000;
	for (record = list->head; ; record = record->next) {
		if (!record)
			return (int)0x804c0005;
		if ((IMG_UINTPTR_T)address < (IMG_UINTPTR_T)record->cpuAddress + record->size &&
				(IMG_UINTPTR_T)address + size > (IMG_UINTPTR_T)record->cpuAddress)
			break;
	}
	return 0;
}

int ClearMappedMemoryList(GpuMappedMemoryList *list)
{
	GpuMappedMemory *record, *next;

	if (!list)
		return (int)0x804c0000;
	for (record = list->head; record; record = next) {
		next = record->next;
		FreeProcessGpuMemory(record);
	}
	list->head = NULL;
	return 0;
}

int CheckMappedMemory(GpuMappedMemoryList *list, const IMG_VOID *address,
		IMG_SIZE_T size, GpuHeap *heap)
{
	GpuMappedMemory *record;

	if (!list)
		return (int)0x804c0000;
	for (record = list->head; ; record = record->next) {
		if (!record)
			return (int)0x804c0005;
		if (record->cpuAddress == address) {
			if ((record->flags & 0x100) && !heap)
				return 0;
			if (record->heap == heap)
				break;
		}
	}
	return 0;
}

int RemoveMappedMemory(GpuMappedMemoryList *list, IMG_VOID *address,
		GpuMappedMemory *output, GpuHeap *heap)
{
	GpuMappedMemory *record, *previous = NULL;

	if (!list)
		return (int)0x804c0000;
	for (record = list->head; ; record = record->next) {
		if (!record)
			return (int)0x804c0005;
		if (record->cpuAddress == address &&
				((record->flags & 0x100) || record->heap == heap))
			break;
		previous = record;
	}
	if (previous)
		previous->next = record->next;
	else
		list->head = record->next;
	if (output)
		*output = *record;
	FreeProcessGpuMemory(record);
	return 0;
}
/* SCE uses the complete interval test for SDK2.10 and later. */
int InsertMappedMemory(GpuMappedMemoryList *list, IMG_VOID *address,
		IMG_SIZE_T size, IMG_UINT32 flags, GpuHeap *heap,
		GpuMappedMemory **output)
{
	GpuMappedMemory *record, *previous, *next, *insert;
	IMG_UINTPTR_T begin = (IMG_UINTPTR_T)address, end = begin + size;

	if (!list || !output)
		return (int)0x804c0000;
	record = list->head;
	if (!record) {
		insert = CreateMappedMemory(address, size, flags, heap);
		if (!insert)
			return (int)0x804c0001;
		insert->next = NULL;
		list->head = insert;
		*output = insert;
		return 0;
	}
	previous = NULL;
	for (;;) {
		if (record->heap == heap) {
			if (end <= (IMG_UINTPTR_T)record->cpuAddress) {
				next = record;
				break;
			}
			if (begin < (IMG_UINTPTR_T)record->cpuAddress + record->size)
				return (int)0x804c0006;
		}
		next = record->next;
		previous = record;
		if (!next)
			break;
		record = next;
	}
	insert = CreateMappedMemory(address, size, flags, heap);
	if (!insert)
		return (int)0x804c0001;
	insert->next = next;
	if (previous)
		previous->next = insert;
	else
		list->head = insert;
	*output = insert;
	return 0;
}
