/* IMG sgxinfokm.h / sgx_mkif_km.h, GPL-2.0; SCE configuration.
 * Pristine candidate source and license remain under contrib/.
 */
#ifndef SGXKICK_SCE_H
#define SGXKICK_SCE_H

#include <gpu/sgxinit_sce.h>
#include <gpu/sgxutils_sce.h>
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

/* sgxutils_sce.c */
PVRSRV_ERROR SGXScheduleCCBCommand(PVRSRV_DEVICE_NODE *psDeviceNode,
		SGXMKIF_CMD_TYPE eCmdType, SGXMKIF_COMMAND *psCommandData,
		IMG_UINT32 ui32CallerID, IMG_UINT32 ui32PDumpFlags,
		IMG_HANDLE hDevMemContext, IMG_BOOL bLastInScene);
PVRSRV_ERROR SGXScheduleCCBCommandKM(PVRSRV_DEVICE_NODE *psDeviceNode,
		SGXMKIF_CMD_TYPE eCmdType, SGXMKIF_COMMAND *psCommandData,
		IMG_UINT32 ui32CallerID, IMG_UINT32 ui32PDumpFlags,
		IMG_HANDLE hDevMemContext, IMG_BOOL bLastInScene);
PVRSRV_ERROR SGXScheduleProcessQueuesKM(PVRSRV_DEVICE_NODE *psDeviceNode);
IMG_VOID SGXTestActivePowerEvent(PVRSRV_DEVICE_NODE *psDeviceNode, IMG_UINT32 ui32CallerID);
IMG_BOOL SGXIsDevicePowered(PVRSRV_DEVICE_NODE *psDeviceNode);
PVRSRV_ERROR SGXGetInternalDevInfoKM(IMG_HANDLE hDevCookie,
		SGX_INTERNAL_DEVINFO *psSGXInternalDevInfo);
PVRSRV_ERROR SGXCleanupRequest(PVRSRV_DEVICE_NODE *psDeviceNode,
		IMG_DEV_VIRTADDR *psHWDataDevVAddr, IMG_UINT32 ui32CleanupType,
		IMG_BOOL bForceCleanup);
IMG_HANDLE SGXRegisterHWRenderContextKM(IMG_HANDLE hDeviceNode,
		IMG_DEV_VIRTADDR *psHWRenderContextDevVAddr,
		struct PVRSRV_KERNEL_MEM_INFO *psHWRenderContextMemInfo,
		PVRSRV_PER_PROCESS_DATA *psPerProc);
PVRSRV_ERROR SGXUnregisterHWRenderContextKM(IMG_HANDLE hHWRenderContext, IMG_BOOL bForceCleanup);
IMG_HANDLE SGXRegisterHW2DContextKM(IMG_HANDLE hDeviceNode,
		IMG_DEV_VIRTADDR *psHW2DContextDevVAddr,
		struct PVRSRV_KERNEL_MEM_INFO *psHW2DContextMemInfo,
		PVRSRV_PER_PROCESS_DATA *psPerProc);
PVRSRV_ERROR SGXUnregisterHW2DContextKM(IMG_HANDLE hHW2DContext, IMG_BOOL bForceCleanup);
PVRSRV_ERROR SGX2DQueryBlitsCompleteKM(PVRSRV_SGXDEV_INFO *psDevInfo,
		PVRSRV_KERNEL_SYNC_INFO *psSyncInfo, IMG_BOOL bWaitForComplete);
PVRSRV_ERROR SGXFlushHWRenderTargetKM(IMG_HANDLE psDeviceNode,
		IMG_DEV_VIRTADDR sHWRTDataSetDevVAddr, IMG_BOOL bForceCleanup);
IMG_VOID SGXWaitClocks(PVRSRV_SGXDEV_INFO *psDevInfo, IMG_UINT32 ui32SGXClocks);
PVRSRV_ERROR SGXPerProcessContextSuspend(PVRSRV_PER_PROCESS_DATA *psPerProc, IMG_BOOL bResume);
PVRSRV_ERROR SGXSetRenderContextPriorityKM(IMG_HANDLE hHWRenderContext, IMG_UINT32 *pui32Priority);
SceUID SGXCleanupMutexInit(IMG_VOID);

#endif
