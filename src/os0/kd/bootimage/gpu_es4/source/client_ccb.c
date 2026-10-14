/* Unreleased licensed IMG client component; pristine kernel DDK is separate. */
#include "client_ccb.h"
#include "device_sce.h"
#include <stddef.h>

void *AllocProcessGpuMemory(unsigned size);
int FreeProcessGpuMemory(void *address);
IMG_VOID *OSMemSet(IMG_VOID *destination, IMG_INT32 value, IMG_SIZE_T size);
PVRSRV_ERROR PVRSRVAllocDeviceMemKM(PVRSRV_DEVICE_NODE *psDeviceNode,
		PVRSRV_PER_PROCESS_DATA *psPerProc, struct GpuHeap *heap,
		IMG_UINT32 ui32Attribs, IMG_SIZE_T ui32Size, IMG_SIZE_T ui32Alignment,
		IMG_INT32 hMemBlockProcRef, PVRSRV_KERNEL_MEM_INFO **ppsMemInfo);
PVRSRV_ERROR PVRSRVFreeDeviceMemKM(IMG_HANDLE hDevCookie,
		PVRSRV_KERNEL_MEM_INFO *psMemInfo);
IMG_UINT32 OSClockus(IMG_VOID);
IMG_VOID OSWaitus(IMG_UINT32 ui32Timeus);
int WaitSignalCounterKernel(volatile IMG_UINT32 *counter, IMG_UINT32 timeout);

PVRSRV_ERROR CreateCommandRing(PVRSRV_DEVICE_NODE *psDeviceNode,
		PVRSRV_PER_PROCESS_DATA *psPerProc, IMG_INT32 hMemBlockProcRef,
		IMG_UINT32 ui32BufferSize, IMG_UINT32 ui32CommandAlignment,
		IMG_UINT32 ui32OverrunSize, struct GpuHeap *heap,
		struct GpuCommandRing **ppsRing)
{
	struct GpuCommandRing *psRing;
	IMG_UINT32 ui32Alignment;
	IMG_UINT32 ui32Bits;
	IMG_UINT32 ui32Flags;

	psRing = AllocProcessGpuMemory(sizeof(*psRing));
	if (!psRing)
		return PVRSRV_ERROR_OUT_OF_MEMORY;
	psRing->psBufferMemInfo = NULL;
	psRing->psControlMemInfo = NULL;
	psRing->pvBuffer = NULL;
	psRing->pui32WriteOffset = NULL;
	psRing->pui32ReadOffset = NULL;
	ui32Alignment = 4096;
	if (ui32BufferSize < 4096) {
		ui32Bits = 12;
		do {
			if (ui32Alignment & ui32BufferSize)
				break;
			ui32Bits--;
			ui32Alignment >>= 1;
		} while (ui32Bits);
		if ((ui32Alignment - 1) & ui32BufferSize)
			ui32Alignment *= 2;
	}
	ui32Flags = hMemBlockProcRef ? 0x40004f : 0x4f;
	if (PVRSRVAllocDeviceMemKM(psDeviceNode, psPerProc, heap, ui32Flags,
			ui32BufferSize + ui32OverrunSize, ui32Alignment,
			hMemBlockProcRef, &psRing->psBufferMemInfo) != PVRSRV_OK)
		goto fail;
	psRing->pvBuffer = psRing->psBufferMemInfo->pvLinAddrKM;
	psRing->ui32BufferSize = ui32BufferSize;
	psRing->ui32BufferAlignment = ui32Alignment;
	psRing->sBufferDevVAddr = psRing->psBufferMemInfo->sDevVAddr;
	if (PVRSRVAllocDeviceMemKM(psDeviceNode, psPerProc, heap, ui32Flags,
			8, 32, hMemBlockProcRef,
			&psRing->psControlMemInfo) != PVRSRV_OK)
		goto fail;
	OSMemSet(psRing->psControlMemInfo->pvLinAddrKM, 0, 8);
	psRing->pui32WriteOffset = psRing->psControlMemInfo->pvLinAddrKM;
	psRing->pui32ReadOffset =
			(IMG_UINT32 *)psRing->psControlMemInfo->pvLinAddrKM + 1;
	*ppsRing = psRing;
	return PVRSRV_OK;
fail:
	if (psRing->psBufferMemInfo)
		PVRSRVFreeDeviceMemKM(NULL, psRing->psBufferMemInfo);
	if (psRing->psControlMemInfo)
		PVRSRVFreeDeviceMemKM(NULL, psRing->psControlMemInfo);
	FreeProcessGpuMemory(psRing);
	return PVRSRV_ERROR_OUT_OF_MEMORY;
}

int DestroyCommandRing(struct GpuCommandRing *ring)
{
	PVRSRVFreeDeviceMemKM(NULL, ring->psBufferMemInfo);
	PVRSRVFreeDeviceMemKM(NULL, ring->psControlMemInfo);
	return FreeProcessGpuMemory(ring);
}

IMG_VOID *AcquireCommandRingSpace(struct GpuCommandRing *psRing,
		IMG_UINT32 ui32Size, volatile IMG_UINT32 *pui32SignalCounter)
{
	IMG_UINT32 ui32WriteOffset;
	IMG_UINT32 ui32ReadOffset;
	IMG_UINT32 ui32StartTime;
	IMG_UINT32 ui32FreeSpace;

	ui32ReadOffset = *psRing->pui32ReadOffset;
	ui32WriteOffset = *psRing->pui32WriteOffset;
	ui32FreeSpace = (ui32ReadOffset + psRing->ui32BufferSize - 1 -
			ui32WriteOffset) & (psRing->ui32BufferSize - 1);
	if (ui32FreeSpace <= ui32Size) {
		ui32StartTime = OSClockus();
		for (;;) {
			if (pui32SignalCounter)
				WaitSignalCounterKernel(pui32SignalCounter, 100000);
			else
				OSWaitus(50);
			ui32ReadOffset = *psRing->pui32ReadOffset;
			ui32WriteOffset = *psRing->pui32WriteOffset;
			ui32FreeSpace = (ui32ReadOffset +
					psRing->ui32BufferSize - 1 - ui32WriteOffset) &
					(psRing->ui32BufferSize - 1);
			if (ui32FreeSpace > ui32Size)
				break;
			if (OSClockus() - ui32StartTime >= 500000)
				return NULL;
		}
	}
	return (IMG_UINT8 *)psRing->psBufferMemInfo->pvLinAddrKM + ui32WriteOffset;
}
