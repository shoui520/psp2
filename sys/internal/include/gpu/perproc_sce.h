/* SCE extensions to the upstream per-process services state. */
#ifndef PERPROC_SCE_H
#define PERPROC_SCE_H

#include <gpu/sce_ddk_port.h>

struct ResourceContext;
struct _PVRSRV_HANDLE_BASE_;
struct GpuMappedMemoryList;
struct GpuMemBlockHeap;
struct GpuTransferQueueContext;

typedef struct GpuSyncWait {
	IMG_DEV_VIRTADDR syncAddress;
	IMG_UINT32 pendingValue;
	IMG_HANDLE memoryHandle;
} GpuSyncWait;

struct _PVRSRV_PER_PROCESS_DATA_ {
	IMG_UINT32 ui32PID;
	IMG_HANDLE hBlockAlloc;
	struct ResourceContext *hResManContext;
	IMG_HANDLE hPerProcData;
	struct _PVRSRV_HANDLE_BASE_ *psHandleBase;
	IMG_BOOL bHandlesBatched;
	IMG_UINT32 ui32RefCount;
	IMG_BOOL bInitProcess;
	struct GpuMappedMemoryList *memoryContext;
	struct GpuMemBlockHeap *memBlockHeap;
	IMG_BOOL bContextSuspended;
	IMG_BOOL bContextSuspendEnabled;
	IMG_UINT32 numRenderTargets[3];
	IMG_UINT32 numSyncObjects;
	IMG_BOOL unmapInProgress;
	IMG_BOOL perContextSyncInfo;
	IMG_BOOL lastSubmitUnlimited;
	IMG_UINT32 numSourceSyncs;
	IMG_UINT32 numDestinationSyncs;
	GpuSyncWait sourceSyncs[32];
	GpuSyncWait destinationSyncs[4];
	struct GpuTransferQueueContext *transferQueue;
	IMG_UINT32 syncModObjectMask;
	int processHandle;
	int waitEventFlag;
	IMG_UINT32 processFlags;
	IMG_UINT32 sdkVersion;
};

_Static_assert(sizeof(PVRSRV_PER_PROCESS_DATA) == 0x21c, "SCE per-process state");

#endif
