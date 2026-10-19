/* SCE process mapping records owned by common/memorymanager.c. */
#ifndef MAPPED_MEMORY_SCE_H
#define MAPPED_MEMORY_SCE_H

#include <gpu/sce_ddk_port.h>
#include <gpu/device_sce.h>
#include <gpu/gpu-mmu.h>
#include "gpu-blockheap.h"
#include "sync_sce.h"

struct GpuHeap;
struct Resource;

typedef struct GpuMappedMemory {
	IMG_VOID *cpuAddress;
	IMG_DEV_VIRTADDR deviceAddress;
	IMG_SIZE_T size;
	IMG_UINT32 flags;
	struct GpuHeap *heap;
	struct Resource *resource;
	IMG_UINT32 mmuFlags;
	struct GpuMappedMemory *next;
} GpuMappedMemory;

typedef struct GpuMappedMemoryList {
	GpuMappedMemory *head;
} GpuMappedMemoryList;

/* common/memorymanager.c */
PVRSRV_ERROR PVRSRVGetDeviceMemHeapsKM(IMG_HANDLE hDevCookie, PVRSRV_HEAP_INFO *psHeapInfo);
PVRSRV_ERROR PVRSRVAllocSyncInfoKM(PVRSRV_DEVICE_NODE *psDeviceNode,
		GpuMemoryContext *hDevMemContext, PVRSRV_KERNEL_SYNC_INFO **ppsSyncInfo);
PVRSRV_ERROR PVRSRVFreeSyncInfoKM(PVRSRV_KERNEL_SYNC_INFO *psSyncInfo);
PVRSRV_ERROR PVRSRVCreateDeviceMemContextKM(PVRSRV_DEVICE_NODE *psDeviceNode,
		PVRSRV_PER_PROCESS_DATA *psPerProc, GpuMemoryContext **context,
		IMG_UINT32 *pui32ClientHeapCount, PVRSRV_HEAP_INFO *psHeapInfo,
		IMG_BOOL *pbCreated, IMG_BOOL *pbSharedDeviceMemHeap);
PVRSRV_ERROR PVRSRVDestroyDeviceMemContextKM(PVRSRV_DEVICE_NODE *psDeviceNode,
		GpuMemoryContext *context, IMG_BOOL *pbDestroyed);
PVRSRV_ERROR PVRSRVGetDeviceMemHeapInfoKM(PVRSRV_DEVICE_NODE *psDeviceNode,
		GpuMemoryContext *context, IMG_UINT32 *pui32ClientHeapCount,
		PVRSRV_HEAP_INFO *psHeapInfo, IMG_BOOL *pbSharedDeviceMemHeap);
PVRSRV_ERROR PVRSRVMapMemoryToGpuKM(PVRSRV_PER_PROCESS_DATA *psPerProc,
		PVRSRV_DEVICE_NODE *psDeviceNode, GpuMemoryContext *context,
		GpuHeap *heap, IMG_VOID *cpuAddress, IMG_SIZE_T size,
		IMG_SIZE_T allocationSize, IMG_UINT32 flags,
		IMG_DEV_VIRTADDR *deviceAddress);
PVRSRV_ERROR PVRSRVUnmapMemoryFromGpuKM(PVRSRV_PER_PROCESS_DATA *psPerProc,
		const IMG_VOID *cpuAddress, GpuHeap *heap, int bWaitForIdle);
PVRSRV_ERROR PVRSRVRegisterMemBlockKM(PVRSRV_PER_PROCESS_DATA *psPerProc,
		IMG_INT32 memBlockUid, IMG_UINT32 *blockGroupId, IMG_BOOL restoreMapping);
PVRSRV_ERROR PVRSRVUnregisterMemBlockKM(PVRSRV_PER_PROCESS_DATA *psPerProc, IMG_INT32 memBlockUid);
void SetGpuCacheControl(IMG_UINT32 flags);
PVRSRV_ERROR QueryMappedMemory(GpuMappedMemoryList *list, IMG_VOID *cpuAddress,
		GpuMappedMemory *info);
int ClearMappedMemoryList(GpuMappedMemoryList *list);

/* common/perproc.c */
PVRSRV_ERROR PVRSRVPerProcessDataInit(IMG_VOID);
PVRSRV_ERROR PVRSRVPerProcessDataDeInit(IMG_VOID);
GpuMemBlockHeap *GetProcessMemBlockHeap(IMG_VOID);

/* osperproc.c */
PVRSRV_ERROR OSPerProcessPrivateDataDeInit(GpuMappedMemoryList *list);

#endif
