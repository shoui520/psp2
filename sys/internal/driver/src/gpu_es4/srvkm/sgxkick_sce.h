/* IMG sgxinfokm.h / sgx_mkif_km.h, GPL-2.0; SCE configuration.
 * Pristine candidate source and license remain under contrib/.
 */
#ifndef SGXKICK_SCE_H
#define SGXKICK_SCE_H

#include <gpu/sgxinit_sce.h>
#include "sync_sce.h"
#include "transfer_sce.h"

typedef struct _PVRSRV_DEVICE_SYNC_OBJECT_ {
	IMG_UINT32 ui32ReadOpsPendingVal;
	IMG_DEV_VIRTADDR sReadOpsCompleteDevVAddr;
	IMG_UINT32 ui32WriteOpsPendingVal;
	IMG_DEV_VIRTADDR sWriteOpsCompleteDevVAddr;
} PVRSRV_DEVICE_SYNC_OBJECT;

typedef struct _SGX_CCB_KICK_KM_ {
	SGXMKIF_COMMAND sCommand;
	PVRSRV_KERNEL_MEM_INFO *hCCBKernelMemInfo;
	IMG_UINT32 ui32NumDstSyncObjects;
	PVRSRV_KERNEL_MEM_INFO *hKernelHWSyncListMemInfo;
	PVRSRV_KERNEL_SYNC_INFO **pahDstSyncHandles;
	IMG_UINT32 ui32NumTAStatusVals;
	IMG_UINT32 ui32Num3DStatusVals;
	/* Existing complete SCE status owner includes the handle/pointer union. */
	SGX_INTERNEL_STATUS_UPDATE_KM asTAStatusUpdate[32];
	SGX_INTERNEL_STATUS_UPDATE_KM as3DStatusUpdate[4];
	IMG_BOOL bFirstKickOrResume;
	/* SCE: retained without PDUMP, passed to the scheduler. */
	IMG_BOOL bLastInScene;
	IMG_UINT32 ui32CCBOffset;
	IMG_UINT32 ui32NumSrcSyncs;
	PVRSRV_KERNEL_SYNC_INFO *ahSrcKernelSyncInfo[8];
	IMG_BOOL bTADependency;
	PVRSRV_KERNEL_SYNC_INFO *hTA3DSyncInfo;
	PVRSRV_KERNEL_SYNC_INFO *hTASyncInfo;
	PVRSRV_KERNEL_SYNC_INFO *h3DSyncInfo;
	/* SCE client producer stores this; the kernel scheduler port ignores it. */
	struct GpuMemoryContext *hDevMemContext;
} SGX_CCB_KICK_KM;

typedef struct _SGXMKIF_CMDTA_SHARED_ {
	IMG_UINT32 ui32CtrlFlags;
	IMG_UINT32 ui32NumTAStatusVals;
	IMG_UINT32 ui32Num3DStatusVals;
	IMG_UINT32 ui32TATQSyncWriteOpsPendingVal;
	IMG_DEV_VIRTADDR sTATQSyncWriteOpsCompleteDevVAddr;
	IMG_UINT32 ui32TATQSyncReadOpsPendingVal;
	IMG_DEV_VIRTADDR sTATQSyncReadOpsCompleteDevVAddr;
	IMG_UINT32 ui323DTQSyncWriteOpsPendingVal;
	IMG_DEV_VIRTADDR s3DTQSyncWriteOpsCompleteDevVAddr;
	IMG_UINT32 ui323DTQSyncReadOpsPendingVal;
	IMG_DEV_VIRTADDR s3DTQSyncReadOpsCompleteDevVAddr;
	IMG_UINT32 ui32NumSrcSyncs;
	PVRSRV_DEVICE_SYNC_OBJECT asSrcSyncs[8];
	PVRSRV_DEVICE_SYNC_OBJECT sTA3DDependency;
	CTL_STATUS sCtlTAStatusInfo[32];
	CTL_STATUS sCtl3DStatusInfo[4];
} SGXMKIF_CMDTA_SHARED;

typedef struct _SGXMKIF_HWDEVICE_SYNC_LIST_ {
	IMG_DEV_VIRTADDR sAccessDevAddr;
	IMG_UINT32 ui32NumSyncObjects;
	PVRSRV_DEVICE_SYNC_OBJECT asSyncData[1];
} SGXMKIF_HWDEVICE_SYNC_LIST;

_Static_assert(sizeof(SGX_CCB_KICK_KM) == 556, "SCE CCB kick input");
_Static_assert(sizeof(SGXMKIF_CMDTA_SHARED) == 480, "TA shared command");

#endif
