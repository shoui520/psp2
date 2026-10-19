/* SCE replacement for the IMG servicesint.h kernel allocation descriptor.
 * The upstream allocation metadata is replaced by Sony's heap/block owners.
 */
#ifndef MEMORY_SCE_H
#define MEMORY_SCE_H

#include <gpu/sce_ddk_port.h>

struct GpuHeap;
struct GpuMemBlockHeap;
struct Resource;

typedef struct PVRSRV_KERNEL_MEM_INFO {
	IMG_VOID *pvLinAddrKM;
	IMG_VOID *pvLinAddr;
	IMG_DEV_VIRTADDR sDevVAddr;
	IMG_UINT32 ui32Flags;
	IMG_SIZE_T uAllocSize;
	IMG_UINT32 ui32RefCount;
	struct GpuHeap *heap;
	IMG_SID hMemBlockProcRef;
	struct GpuMemBlockHeap *memBlockHeap;
	struct Resource *hResItem;
} PVRSRV_KERNEL_MEM_INFO;

_Static_assert(sizeof(PVRSRV_KERNEL_MEM_INFO) == 40, "SCE kernel memory info");

#endif
