/* SCE kernel transfer queue, owned by the GPU driver. */
#ifndef TRANSFER_SCE_H
#define TRANSFER_SCE_H

#include <gpu/sce_ddk_port.h>

struct Resource;
struct PVRSRV_KERNEL_MEM_INFO;
struct PVRSRV_KERNEL_SYNC_INFO;
struct GpuCommandRing;
struct BM_CONTEXT;
struct GpuMemoryContext;

typedef struct GpuTransferMemoryContext {
	union {
		IMG_SID hMemoryContext;
		struct GpuMemoryContext *memoryContext;
	} memory;
	IMG_INT32 hMemBlockProcRef;
} GpuTransferMemoryContext;

/* Complete host-facing allocation. Firmware-private state is live opaque
 * data, initialized by the host and released after firmware cleanup.
 */
typedef struct SGXMKIF_HW2DCONTEXT {
	IMG_UINT32 ui32Flags;
	IMG_UINT32 ui32Priority;
	IMG_DEV_PHYADDR sPDDevPAddr;
	IMG_DEV_VIRTADDR sCCBDevVAddr;
	IMG_DEV_VIRTADDR sCCBControlDevVAddr;
	IMG_UINT32 firmwarePrivateWord;
	IMG_UINT32 ui32ProcessID;
	IMG_UINT32 firmwarePrivateState[2];
} SGXMKIF_HW2DCONTEXT;

/* IMG sgxapi_km.h status value; the SCE bridge retains handle translation. */
typedef struct _CTL_STATUS_ {
	IMG_DEV_VIRTADDR sStatusDevAddr;
	IMG_UINT32 ui32StatusValue;
} CTL_STATUS;

typedef struct _SGX_INTERNEL_STATUS_UPDATE_KM_ {
	union {
		CTL_STATUS sCtlStatus;
		struct {
			IMG_DEV_VIRTADDR statusAddress;
			IMG_UINT32 statusValue;
		};
	};
	union {
		IMG_SID memoryHandle;
		struct PVRSRV_KERNEL_MEM_INFO *memoryInfo;
	} memory;
} SGX_INTERNEL_STATUS_UPDATE_KM;

/* Existing SCE bridge spelling for the same complete owner. */
typedef SGX_INTERNEL_STATUS_UPDATE_KM GpuTransferStatus;

typedef union GpuSyncReference {
	IMG_SID hSyncInfo;
	struct PVRSRV_KERNEL_SYNC_INFO *syncInfo;
} GpuSyncReference;

/* SCE extends the upstream 2D kick with one status update. */
typedef struct PVRSRV_2D_SGX_KICK {
	union {
		IMG_SID hCCBMemInfo;
		struct PVRSRV_KERNEL_MEM_INFO *memoryInfo;
	} commandMemory;
	IMG_UINT32 ui32SharedCmdCCBOffset;
	IMG_DEV_VIRTADDR sHW2DContextDevVAddr;
	IMG_UINT32 ui32NumSrcSync;
	GpuSyncReference ahSrcSyncInfo[3];
	GpuSyncReference hDstSyncInfo;
	GpuSyncReference hTASyncInfo;
	GpuSyncReference h3DSyncInfo;
	IMG_UINT32 ui32NumStatusVals;
	GpuTransferStatus sStatusUpdate;
	IMG_UINT32 ui32PDumpFlags;
	struct GpuMemoryContext *hDevMemContext;
} PVRSRV_2D_SGX_KICK;

typedef struct GpuTransferQueueContext {
	int mutex;
	struct Resource *mutexResource;
	struct Resource *contextResource;
	IMG_UINT32 reserved;
	IMG_UINT32 waitGeneration;
	IMG_UINT32 reservedWait[2];
	struct PVRSRV_KERNEL_SYNC_INFO *sourceSync;
	struct PVRSRV_KERNEL_SYNC_INFO *destinationSync;
	IMG_UINT32 reservedSync;
	IMG_HANDLE hardwareRegistration;
	struct PVRSRV_KERNEL_MEM_INFO *contextMemInfo;
	IMG_DEV_VIRTADDR contextAddress;
	struct GpuCommandRing *commandRing;
	IMG_UINT32 reservedContext[12];
	struct GpuMemoryContext *memoryContext;
} GpuTransferQueueContext;

_Static_assert(sizeof(GpuTransferQueueContext) == 108, "SCE transfer queue");
_Static_assert(sizeof(PVRSRV_2D_SGX_KICK) == 64, "SCE 2D kick");
_Static_assert(sizeof(SGXMKIF_HW2DCONTEXT) == 36, "hardware 2D host interface");
_Static_assert(sizeof(GpuTransferMemoryContext) == 8, "transfer memory request");

#endif
