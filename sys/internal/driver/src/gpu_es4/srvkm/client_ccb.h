/* Licensed IMG client command-buffer owner, recovered from TOOL originals. */
#ifndef CLIENT_CCB_H
#define CLIENT_CCB_H

#include <gpu/memory_sce.h>
#include <gpu/device_sce.h>

struct GpuCommandRing {
	PVRSRV_KERNEL_MEM_INFO *psBufferMemInfo;
	PVRSRV_KERNEL_MEM_INFO *psControlMemInfo;
	IMG_VOID *pvBuffer;
	IMG_DEV_VIRTADDR sBufferDevVAddr;
	volatile IMG_UINT32 *pui32WriteOffset;
	volatile IMG_UINT32 *pui32ReadOffset;
	IMG_UINT32 ui32BufferSize;
	IMG_UINT32 ui32BufferAlignment;
};

_Static_assert(sizeof(struct GpuCommandRing) == 32, "client command ring");

/* client_ccb.c */
PVRSRV_ERROR CreateCommandRing(PVRSRV_DEVICE_NODE *psDeviceNode,
		PVRSRV_PER_PROCESS_DATA *psPerProc, IMG_INT32 hMemBlockProcRef,
		IMG_UINT32 ui32BufferSize, IMG_UINT32 ui32CommandAlignment,
		IMG_UINT32 ui32OverrunSize, struct GpuHeap *heap,
		struct GpuCommandRing **ppsRing);
int DestroyCommandRing(struct GpuCommandRing *ring);
IMG_VOID *AcquireCommandRingSpace(struct GpuCommandRing *psRing,
		IMG_UINT32 ui32Size, volatile IMG_UINT32 *pui32SignalCounter);

#endif
