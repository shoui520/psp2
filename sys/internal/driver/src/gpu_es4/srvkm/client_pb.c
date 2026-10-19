/* Unreleased licensed IMG client component; see client_pb.h. */
#include "client_pb.h"
#include <gpu/device_sce.h>
#include <gpu/perproc_sce.h>
#include "render_sce.h"
#include <stddef.h>
#include <string.h>
#include <gpu/memory_sce.h>
#include <gpu/osfunc_sce.h>

PVRSRV_ERROR PVRSRVFreeSharedSysMemoryKM(PVRSRV_KERNEL_MEM_INFO *psMemInfo);
int FreeProcessGpuMemory(void *address);
void *AllocProcessGpuMemory(unsigned size);
PVRSRV_ERROR PVRSRVAllocSharedSysMemoryKM(PVRSRV_PER_PROCESS_DATA *psPerProc,
		IMG_SID hMemBlockProcRef, IMG_UINT32 ui32Flags, IMG_SIZE_T uSize,
		PVRSRV_KERNEL_MEM_INFO **ppsKernelMemInfo);
PVRSRV_ERROR SGXAddSharedPBDescKM(PVRSRV_PER_PROCESS_DATA *psPerProc,
		IMG_HANDLE hDevCookie, PVRSRV_KERNEL_MEM_INFO *psSharedPBDescKernelMemInfo,
		PVRSRV_KERNEL_MEM_INFO *psHWPBDescKernelMemInfo,
		PVRSRV_KERNEL_MEM_INFO *psBlockKernelMemInfo,
		PVRSRV_KERNEL_MEM_INFO *psHWBlockKernelMemInfo,
		IMG_UINT32 ui32TotalPBSize, IMG_HANDLE *phSharedPBDesc,
		PVRSRV_KERNEL_MEM_INFO **ppsSharedPBDescSubKernelMemInfos,
		IMG_UINT32 ui32SharedPBDescSubKernelMemInfosCount,
		IMG_DEV_VIRTADDR sHWPBDescDevVAddr);
PVRSRV_ERROR SGXUnrefSharedPBDescKM(IMG_HANDLE hSharedPBDesc);

SGX_PBDESC *UpdateParameterBufferDescriptor(SGX_PBDESC *psPBDesc)
{
	SGX_PBBLOCK *psBlock = psPBDesc->psPBBlockList;
	IMG_UINT32 ui32TotalPages = 0;
	IMG_UINT32 ui32Head = psBlock->ui16Head;
	IMG_UINT32 ui32Tail;
	IMG_UINT32 ui32UsablePages;
	IMG_UINT32 ui32TAThreshold;

	do {
		ui32TotalPages += psBlock->ui32PageCount;
		psBlock = psBlock->psNext;
	} while (psBlock);

	ui32Tail = psPBDesc->psPBBlockListTail->ui16Tail;
	psPBDesc->ui32UsableSize = (ui32TotalPages - 34) << 12;
	psPBDesc->ui32TotalPages = ui32TotalPages;
	psPBDesc->ui32InitialFreeListHT = ui32Head | (ui32Tail << 16);
	psPBDesc->ui32InitialPrev = (ui32Tail - 1) << 16;
	ui32UsablePages = psPBDesc->ui32UsableSize >> 12;
	ui32TAThreshold = ui32TotalPages + 16;
	psPBDesc->ui32TAThreshold = ui32TAThreshold;
	psPBDesc->ui32ZLSThreshold = ui32UsablePages;
	if (ui32UsablePages < ui32TAThreshold)
		ui32TAThreshold = ui32UsablePages;
	psPBDesc->ui32PDSThreshold = ui32TAThreshold;
	psPBDesc->ui32GlobalThreshold = ui32UsablePages;
	if (ui32TAThreshold > 256)
		psPBDesc->ui32PDSThreshold = ui32TAThreshold - 256;
	else
		psPBDesc->ui32PDSThreshold = 0;
	return psPBDesc;
}

int FreeParameterBufferBlock(SGX_PBDESC *psPBDesc,
		PVRSRV_KERNEL_MEM_INFO **ppsSharedBlockMemInfo,
		SGX_PBBLOCK *psBlock)
{
	if (psBlock) {
		if (psBlock->psParameterMemInfo)
			PVRSRVFreeDeviceMemKM(NULL, psBlock->psParameterMemInfo);
		if (psBlock->psHWBlockMemInfo)
			PVRSRVFreeDeviceMemKM(NULL, psBlock->psHWBlockMemInfo);
		if (psBlock->psPrev)
			psBlock->psPrev->psNext = psBlock->psNext;
		else if (psPBDesc->psPBBlockList == psBlock)
			psPBDesc->psPBBlockList = psBlock->psNext;
		if (psBlock->psNext)
			psBlock->psNext->psPrev = psBlock->psPrev;
		else if (psPBDesc->psPBBlockListTail == psBlock)
			psPBDesc->psPBBlockListTail = psBlock->psPrev;
		if (psPBDesc->bPerContextPB)
			FreeProcessGpuMemory(psBlock);
		if (!psPBDesc->bPerContextPB && *ppsSharedBlockMemInfo)
			PVRSRVFreeSharedSysMemoryKM(*ppsSharedBlockMemInfo);
	}
	return 0;
}

SGX_PBDESC *InitializeHardwareParameterBufferDescriptor(SGX_PBDESC *psPBDesc,
		IMG_DEV_VIRTADDR *psPageTableDevVAddr)
{
	SGX_HWPBDESC *psHWDesc = psPBDesc->psHWPBDesc;
	IMG_UINT32 ui32FreeListHT;
	IMG_UINT32 ui32Prev;

	psHWDesc->ui32Control = 0;
	psHWDesc->ui32TotalPages = psPBDesc->ui32TotalPages;
	ui32FreeListHT = psPBDesc->ui32InitialFreeListHT;
	psHWDesc->ui32InitialFreeListHT = ui32FreeListHT;
	ui32Prev = psPBDesc->ui32InitialPrev;
	psHWDesc->ui32InitialPrev = ui32Prev;
	psHWDesc->ui32FreeListHT = ui32FreeListHT;
	psHWDesc->ui32Prev = ui32Prev;
	psHWDesc->sPageTableDevVAddr = *psPageTableDevVAddr;
	psHWDesc->ui32TAThreshold = psPBDesc->ui32TAThreshold;
	psHWDesc->ui32ZLSThreshold = psPBDesc->ui32ZLSThreshold;
	psHWDesc->ui32GlobalThreshold = psPBDesc->ui32GlobalThreshold;
	psHWDesc->ui32PDSThreshold = psPBDesc->ui32PDSThreshold;
	return psPBDesc;
}

IMG_UINT32 *InitializeParameterBufferPageTable(IMG_UINT32 *pui32PageTable,
		SGX_PBBLOCK *psBlock)
{
	IMG_UINT32 ui32Page = psBlock->ui16Head;
	IMG_UINT32 ui32Previous = 0xffff;
	IMG_UINT32 ui32Index = 0;

	while (ui32Index < psBlock->ui32PageCount) {
		IMG_UINT32 ui32Next = (IMG_UINT16)(ui32Page + 1);

		++ui32Index;
		pui32PageTable[ui32Page] = ui32Next | (ui32Previous << 16);
		ui32Previous = ui32Page;
		ui32Page = ui32Next;
	}
	pui32PageTable[ui32Previous] |= 0xffff;
	return pui32PageTable;
}

int AllocateParameterBufferBlock(PVRSRV_DEVICE_NODE *psDeviceNode,
		PVRSRV_PER_PROCESS_DATA *psPerProc, IMG_INT32 hMemBlockProcRef,
		SGX_PBDESC *psPBDesc, IMG_UINT32 ui32Size,
		PVRSRV_KERNEL_MEM_INFO **ppsSharedBlockMemInfo,
		struct GpuHeap **ppsParameterHeap, struct GpuHeap **ppsKernelHeap)
{
	SGX_PBBLOCK *psBlock = NULL;
	IMG_UINT32 ui32Flags;

	if (psPBDesc->bPerContextPB) {
		psBlock = AllocProcessGpuMemory(sizeof(*psBlock));
		if (!psBlock)
			goto alloc_failed;
		memset(psBlock, 0, sizeof(*psBlock));
	}
	if (!psPBDesc->bPerContextPB) {
		if (PVRSRVAllocSharedSysMemoryKM(psPerProc, hMemBlockProcRef,
				0, 64, ppsSharedBlockMemInfo) != PVRSRV_OK)
			goto alloc_failed;
		psBlock = (*ppsSharedBlockMemInfo)->pvLinAddrKM;
		OSMemSet(psBlock, 0, sizeof(*psBlock));
	}
	if (!psBlock)
		goto alloc_failed;
	psBlock->psPBDesc = psPBDesc;
	psBlock->psNext = NULL;
	ui32Flags = (psPBDesc->bPerContextPB ? 11 : 1035) | 0x400000;
	if (_PVRSRVAllocDeviceMemKM(psDeviceNode, psPerProc, *ppsKernelHeap,
			ui32Flags | 4, sizeof(*psBlock->psHWBlock), 4,
			hMemBlockProcRef, &psBlock->psHWBlockMemInfo) != PVRSRV_OK)
		goto cleanup;
	psBlock->psHWBlock = psBlock->psHWBlockMemInfo->pvLinAddrKM;
	psBlock->sHWBlockDevVAddr = psBlock->psHWBlockMemInfo->sDevVAddr;
	if (_PVRSRVAllocDeviceMemKM(psDeviceNode, psPerProc, *ppsParameterHeap,
			ui32Flags, ui32Size, 4096, hMemBlockProcRef,
			&psBlock->psParameterMemInfo) != PVRSRV_OK)
		goto cleanup;
	psBlock->ui32PageCount = psBlock->psParameterMemInfo->uAllocSize >> 12;
	psBlock->ui16Head = (psBlock->psParameterMemInfo->sDevVAddr.uiAddr -
			psPBDesc->sPBBaseDevVAddr.uiAddr) >> 12;
	psBlock->ui16Tail = psBlock->ui16Head + psBlock->ui32PageCount - 1;
	psBlock->psHWBlock->ui32PageCount = psBlock->ui32PageCount;
	psBlock->psHWBlock->ui16Head = psBlock->ui16Head;
	psBlock->psHWBlock->ui16Tail = psBlock->ui16Tail;
	psBlock->psHWBlock->sHWPBDescDevVAddr = psPBDesc->sHWPBDescDevVAddr;
	psBlock->psHWBlock->sNextDevVAddr.uiAddr = 0;
	if (!psPBDesc->bDetachedBlockAllocation) {
		psBlock->psPrev = psPBDesc->psPBBlockListTail;
		if (psPBDesc->psPBBlockListTail)
			psPBDesc->psPBBlockListTail->psNext = psBlock;
		else
			psPBDesc->psPBBlockList = psBlock;
		psPBDesc->psPBBlockListTail = psBlock;
	}
	return 0;

alloc_failed:
	psBlock = NULL;
cleanup:
	FreeParameterBufferBlock(psPBDesc, ppsSharedBlockMemInfo, psBlock);
	if (ppsSharedBlockMemInfo)
		*ppsSharedBlockMemInfo = NULL;
	return 1;
}

void DestroyPrivateParameterBuffer(SGX_CLIENTPBDESC *psClientPBDesc)
{
	SGX_PBDESC *psPBDesc = psClientPBDesc->psPBDesc;
	SGX_PBBLOCK *psBlock;

	if (psClientPBDesc->psPageTableMemInfo) {
		PVRSRVFreeDeviceMemKM(NULL, psClientPBDesc->psPageTableMemInfo);
		psClientPBDesc->psPageTableMemInfo = NULL;
	}
	if (psClientPBDesc->psHWPBDescMemInfo) {
		PVRSRVFreeDeviceMemKM(NULL, psClientPBDesc->psHWPBDescMemInfo);
		psClientPBDesc->psHWPBDescMemInfo = NULL;
	}
	if (psPBDesc) {
		psBlock = psClientPBDesc->psPBDesc->psPBBlockList;
		while (psBlock) {
			SGX_PBBLOCK *psNext = psBlock->psNext;

			FreeParameterBufferBlock(psPBDesc, NULL, psBlock);
			psBlock = psNext;
		}
		FreeProcessGpuMemory(psPBDesc);
		psClientPBDesc->psPBDesc = NULL;
	}
}

int CreatePrivateParameterBuffer(PVRSRV_DEVICE_NODE *psDeviceNode,
		PVRSRV_PER_PROCESS_DATA *psPerProc, IMG_INT32 hMemBlockProcRef,
		struct SGX_RENDERCONTEXT *psRenderContext, IMG_UINT32 ui32Size,
		PVRSRV_HEAP_INFO *psParameterHeap, PVRSRV_HEAP_INFO *psKernelHeap)
{
	SGX_CLIENTPBDESC *psClientPBDesc = psRenderContext->psClientPBDesc;
	SGX_PBDESC *psPBDesc = AllocProcessGpuMemory(sizeof(*psPBDesc));
	SGX_HWPBDESC *psHWDesc;

	if (!psPBDesc)
		goto failed;
	psClientPBDesc->psPBDesc = psPBDesc;
	psPBDesc->bPerContextPB = IMG_TRUE;
	psPBDesc->psPBBlockList = NULL;
	psPBDesc->psPBBlockListTail = NULL;
	psPBDesc->bDetachedBlockAllocation = IMG_FALSE;
	psPBDesc->ui32Control = 0;
	psPBDesc->sPBBaseDevVAddr.uiAddr = 0x20000000;
	if (_PVRSRVAllocDeviceMemKM(psDeviceNode, psPerProc,
			psKernelHeap->hDevMemHeap, 0x40000f,
			sizeof(*psHWDesc), 32, hMemBlockProcRef,
			&psClientPBDesc->psHWPBDescMemInfo) != PVRSRV_OK)
		goto failed;
	psHWDesc = psClientPBDesc->psHWPBDescMemInfo->pvLinAddrKM;
	psPBDesc->psHWPBDesc = psHWDesc;
	psPBDesc->sHWPBDescDevVAddr = psClientPBDesc->psHWPBDescMemInfo->sDevVAddr;
	OSMemSet(psHWDesc, 0, sizeof(*psHWDesc));
	if (AllocateParameterBufferBlock(psDeviceNode, psPerProc,
			hMemBlockProcRef, psPBDesc, ui32Size + 0x22000, NULL,
			(struct GpuHeap **)&psParameterHeap->hDevMemHeap,
			(struct GpuHeap **)&psKernelHeap->hDevMemHeap))
		goto failed;
	psHWDesc->sPBBlockListDevVAddr = psPBDesc->psPBBlockList->sHWBlockDevVAddr;
	if (_PVRSRVAllocDeviceMemKM(psDeviceNode, psPerProc,
			psParameterHeap->hDevMemHeap, 0x40000b, 0x40000, 16,
			hMemBlockProcRef, &psClientPBDesc->psPageTableMemInfo) != PVRSRV_OK)
		goto failed;
	InitializeParameterBufferPageTable(psClientPBDesc->psPageTableMemInfo->pvLinAddrKM,
			psPBDesc->psPBBlockList);
	UpdateParameterBufferDescriptor(psPBDesc);
	InitializeHardwareParameterBufferDescriptor(psPBDesc,
			&psClientPBDesc->psPageTableMemInfo->sDevVAddr);
	return 0;

failed:
	DestroyPrivateParameterBuffer(psRenderContext->psClientPBDesc);
	return 1;
}

int CreateSharedParameterBuffer(PVRSRV_DEVICE_NODE *psDeviceNode,
		PVRSRV_PER_PROCESS_DATA *psPerProc, IMG_INT32 hMemBlockProcRef,
		IMG_HANDLE *phSharedPBDesc, IMG_UINT32 ui32Size,
		IMG_HANDLE hDevMemContext, PVRSRV_HEAP_INFO *psParameterHeap,
		PVRSRV_HEAP_INFO *psKernelHeap)
{
	PVRSRV_KERNEL_MEM_INFO *psSharedMemInfo = NULL;
	PVRSRV_KERNEL_MEM_INFO *psHWMemInfo = NULL;
	PVRSRV_KERNEL_MEM_INFO *psPageTableMemInfo = NULL;
	PVRSRV_KERNEL_MEM_INFO *psBlockMemInfo = NULL;
	PVRSRV_KERNEL_MEM_INFO *psParameterMemInfo = NULL;
	PVRSRV_KERNEL_MEM_INFO *apsSubMemInfo[2] = { NULL, NULL };
	SGX_PBDESC *psPBDesc;
	SGX_HWPBDESC *psHWDesc;
	IMG_DEV_VIRTADDR sHWPBDescDevVAddr;
	int result = 1;

	(void)hDevMemContext;
	if (PVRSRVAllocSharedSysMemoryKM(psPerProc, hMemBlockProcRef, 0,
			sizeof(*psPBDesc), &psSharedMemInfo) != PVRSRV_OK)
		goto cleanup;
	psPBDesc = psSharedMemInfo->pvLinAddrKM;
	psPBDesc->sPBBaseDevVAddr.uiAddr = 0x20000000;
	psPBDesc->bPerContextPB = IMG_FALSE;
	psPBDesc->bDetachedBlockAllocation = IMG_FALSE;
	psPBDesc->ui32Control = 0;
	psPBDesc->psPBBlockList = NULL;
	psPBDesc->psPBBlockListTail = NULL;
	if (_PVRSRVAllocDeviceMemKM(psDeviceNode, psPerProc,
			psKernelHeap->hDevMemHeap, 0x40040f, sizeof(*psHWDesc),
			32, hMemBlockProcRef, &psHWMemInfo) != PVRSRV_OK)
		goto cleanup;
	psHWDesc = psHWMemInfo->pvLinAddrKM;
	psPBDesc->psHWPBDesc = psHWDesc;
	psPBDesc->sHWPBDescDevVAddr = psHWMemInfo->sDevVAddr;
	sHWPBDescDevVAddr = psHWMemInfo->sDevVAddr;
	OSMemSet(psHWDesc, 0, sizeof(*psHWDesc));
	if (AllocateParameterBufferBlock(psDeviceNode, psPerProc,
			hMemBlockProcRef, psPBDesc, ui32Size + 0x22000,
			&psBlockMemInfo,
			(struct GpuHeap **)&psParameterHeap->hDevMemHeap,
			(struct GpuHeap **)&psKernelHeap->hDevMemHeap))
		goto cleanup;
	psHWDesc->sPBBlockListDevVAddr = psPBDesc->psPBBlockList->sHWBlockDevVAddr;
	psParameterMemInfo = ((SGX_PBBLOCK *)psBlockMemInfo->pvLinAddrKM)->psParameterMemInfo;
	if (_PVRSRVAllocDeviceMemKM(psDeviceNode, psPerProc,
			psParameterHeap->hDevMemHeap, 0x40040b, 0x40000, 16,
			hMemBlockProcRef, &psPageTableMemInfo) != PVRSRV_OK)
		goto cleanup;
	result = 0;
	InitializeParameterBufferPageTable(psPageTableMemInfo->pvLinAddrKM,
			psPBDesc->psPBBlockList);
	UpdateParameterBufferDescriptor(psPBDesc);
	InitializeHardwareParameterBufferDescriptor(psPBDesc, &psPageTableMemInfo->sDevVAddr);
	apsSubMemInfo[0] = psParameterMemInfo;
	apsSubMemInfo[1] = psPageTableMemInfo;
	{
		PVRSRV_KERNEL_MEM_INFO *psShared = psSharedMemInfo;
		PVRSRV_KERNEL_MEM_INFO *psHardware = psHWMemInfo;
		PVRSRV_KERNEL_MEM_INFO *psBlock = psBlockMemInfo;
		PVRSRV_KERNEL_MEM_INFO *psHWBlock = psPBDesc->psPBBlockList->psHWBlockMemInfo;

		psPageTableMemInfo = NULL;
		psBlockMemInfo = NULL;
		psSharedMemInfo = NULL;
		psHWMemInfo = NULL;
		if (SGXAddSharedPBDescKM(psPerProc, psDeviceNode, psShared, psHardware,
				psBlock, psHWBlock, ui32Size, phSharedPBDesc, apsSubMemInfo,
				2, sHWPBDescDevVAddr) == PVRSRV_OK)
			return 0;
		/* Native registration failure retains the last allocation result. */
		psParameterMemInfo = NULL;
	}
cleanup:
	if (psHWMemInfo)
		PVRSRVFreeDeviceMemKM(NULL, psHWMemInfo);
	if (psPageTableMemInfo)
		PVRSRVFreeDeviceMemKM(NULL, psPageTableMemInfo);
	if (psParameterMemInfo)
		PVRSRVFreeDeviceMemKM(NULL, psParameterMemInfo);
	if (psSharedMemInfo)
		PVRSRVFreeSharedSysMemoryKM(psSharedMemInfo);
	if (psBlockMemInfo)
		PVRSRVFreeSharedSysMemoryKM(psBlockMemInfo);
	return result;
}

PVRSRV_ERROR UnreferenceClientParameterBuffer(SGX_CLIENTPBDESC *psClientPBDesc)
{
	return SGXUnrefSharedPBDescKM(psClientPBDesc->hSharedPBDesc);
}

int InitializeSharedClientParameterBuffer(IMG_HANDLE hSharedPBDesc,
		PVRSRV_KERNEL_MEM_INFO *psSharedMemInfo,
		PVRSRV_KERNEL_MEM_INFO *psHWMemInfo,
		PVRSRV_KERNEL_MEM_INFO *psBlockMemInfo,
		PVRSRV_KERNEL_MEM_INFO *psHWBlockMemInfo,
		PVRSRV_KERNEL_MEM_INFO **ppsSubMemInfo, IMG_UINT32 ui32SubMemInfoCount,
		SGX_CLIENTPBDESC *psClientPBDesc)
{
	SGX_PBDESC *psPBDesc = psSharedMemInfo->pvLinAddrKM;
	PVRSRV_KERNEL_MEM_INFO *psPageTableMemInfo;

	(void)ui32SubMemInfoCount;
	psClientPBDesc->psSharedPBDescMemInfo = psSharedMemInfo;
	psClientPBDesc->psHWPBDescMemInfo = psHWMemInfo;
	psClientPBDesc->psPBBlockMemInfo = psBlockMemInfo;
	psClientPBDesc->psHWBlockMemInfo = psHWBlockMemInfo;
	psClientPBDesc->psPBDesc = psPBDesc;
	psPageTableMemInfo = ppsSubMemInfo[1];
	psClientPBDesc->hSharedPBDesc = hSharedPBDesc;
	psClientPBDesc->psPageTableMemInfo = psPageTableMemInfo;
	return 0;
}
