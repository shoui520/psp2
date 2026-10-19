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

struct PVRSRV_DEVICE_NODE;

/* common/memorymanager.c */
PVRSRV_ERROR _PVRSRVAllocDeviceMemKM(struct PVRSRV_DEVICE_NODE *psDeviceNode,
		PVRSRV_PER_PROCESS_DATA *psPerProc, struct GpuHeap *heap,
		IMG_UINT32 ui32Attribs, IMG_SIZE_T ui32Size, IMG_SIZE_T ui32Alignment,
		IMG_INT32 hMemBlockProcRef, PVRSRV_KERNEL_MEM_INFO **ppsMemInfo);
PVRSRV_ERROR PVRSRVFreeDeviceMemKM(struct PVRSRV_DEVICE_NODE *psDeviceNode,
		PVRSRV_KERNEL_MEM_INFO *psMemInfo);

/* mem.c */
PVRSRV_ERROR PVRSRVAllocSharedSysMemoryKM(PVRSRV_PER_PROCESS_DATA *psPerProc,
		IMG_SID hMemBlockProcRef, IMG_UINT32 ui32Flags, IMG_SIZE_T uSize,
		PVRSRV_KERNEL_MEM_INFO **ppsKernelMemInfo);
PVRSRV_ERROR PVRSRVFreeSharedSysMemoryKM(PVRSRV_KERNEL_MEM_INFO *psKernelMemInfo);
PVRSRV_ERROR PVRSRVDissociateMemFromResmanKM(PVRSRV_KERNEL_MEM_INFO *psKernelMemInfo);

#endif
