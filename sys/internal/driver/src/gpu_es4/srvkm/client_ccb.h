/* Licensed IMG client command-buffer owner, recovered from TOOL originals. */
#ifndef CLIENT_CCB_H
#define CLIENT_CCB_H

#include <gpu/memory_sce.h>

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

#endif
