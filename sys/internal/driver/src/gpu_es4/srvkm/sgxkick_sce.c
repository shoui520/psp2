/* IMG devices/sgx/sgxkick.c, GPL-2.0. Pristine candidate and license remain
 * in contrib/img-sgx-ddk-1.7.862890; SCE port differences marked below.
 */
#include "sgxkick_sce.h"
#include <stddef.h>
#include <sysmem/cpu.h>
#include "system_sce.h"

#define SGXMKIF_CMDTA_CTRLFLAGS_READY 1U
#define SGXMKIF_CMD_TA 0
#define KERNEL_ID 0xffffffffU

extern int gPVRSRVLock;
/* NID is absent from the pinned vitasdk vocabulary. */

PVRSRV_ERROR SGXDoKickKM(PVRSRV_DEVICE_NODE *psDeviceNode,
		SGX_CCB_KICK_KM *psCCBKick)
{
	PVRSRV_ERROR eError;
	PVRSRV_KERNEL_SYNC_INFO *psSyncInfo;
	PVRSRV_KERNEL_MEM_INFO *psCCBMemInfo = psCCBKick->hCCBKernelMemInfo;
	SGXMKIF_CMDTA_SHARED *psTACmd;
	IMG_UINT32 i;

	/* SCE: diagnostics and PDUMP omitted in this port. */
	if (psCCBMemInfo->uAllocSize < sizeof(*psTACmd) ||
			psCCBKick->ui32CCBOffset > psCCBMemInfo->uAllocSize - sizeof(*psTACmd))
		return PVRSRV_ERROR_INVALID_PARAMS;
	psTACmd = (SGXMKIF_CMDTA_SHARED *)((IMG_UINT8 *)psCCBMemInfo->pvLinAddrKM +
			psCCBKick->ui32CCBOffset);
	if (psCCBKick->hTA3DSyncInfo) {
		psSyncInfo = psCCBKick->hTA3DSyncInfo;
		psTACmd->sTA3DDependency.sWriteOpsCompleteDevVAddr = psSyncInfo->sWriteOpsCompleteDevVAddr;
		psTACmd->sTA3DDependency.ui32WriteOpsPendingVal = psSyncInfo->psSyncData->ui32WriteOpsPending;
		if (psCCBKick->bTADependency)
			psSyncInfo->psSyncData->ui32WriteOpsPending++;
	}
	if (psCCBKick->hTASyncInfo) {
		psSyncInfo = psCCBKick->hTASyncInfo;
		psTACmd->sTATQSyncReadOpsCompleteDevVAddr = psSyncInfo->sReadOpsCompleteDevVAddr;
		psTACmd->sTATQSyncWriteOpsCompleteDevVAddr = psSyncInfo->sWriteOpsCompleteDevVAddr;
		psTACmd->ui32TATQSyncReadOpsPendingVal = psSyncInfo->psSyncData->ui32ReadOpsPending++;
		psTACmd->ui32TATQSyncWriteOpsPendingVal = psSyncInfo->psSyncData->ui32WriteOpsPending;
	}
	if (psCCBKick->h3DSyncInfo) {
		psSyncInfo = psCCBKick->h3DSyncInfo;
		psTACmd->s3DTQSyncReadOpsCompleteDevVAddr = psSyncInfo->sReadOpsCompleteDevVAddr;
		psTACmd->s3DTQSyncWriteOpsCompleteDevVAddr = psSyncInfo->sWriteOpsCompleteDevVAddr;
		psTACmd->ui323DTQSyncReadOpsPendingVal = psSyncInfo->psSyncData->ui32ReadOpsPending++;
		psTACmd->ui323DTQSyncWriteOpsPendingVal = psSyncInfo->psSyncData->ui32WriteOpsPending;
	}
	psTACmd->ui32NumTAStatusVals = psCCBKick->ui32NumTAStatusVals;
	if (psCCBKick->ui32NumTAStatusVals)
		for (i = 0; i < psCCBKick->ui32NumTAStatusVals; i++) {
			psTACmd->sCtlTAStatusInfo[i] = psCCBKick->asTAStatusUpdate[i].sCtlStatus;
		}
	psTACmd->ui32Num3DStatusVals = psCCBKick->ui32Num3DStatusVals;
	if (psCCBKick->ui32Num3DStatusVals)
		for (i = 0; i < psCCBKick->ui32Num3DStatusVals; i++) {
			psTACmd->sCtl3DStatusInfo[i] = psCCBKick->as3DStatusUpdate[i].sCtlStatus;
		}
	psTACmd->ui32NumSrcSyncs = psCCBKick->ui32NumSrcSyncs;
	for (i = 0; i < psCCBKick->ui32NumSrcSyncs; i++) {
		psSyncInfo = psCCBKick->ahSrcKernelSyncInfo[i];
		psTACmd->asSrcSyncs[i].sWriteOpsCompleteDevVAddr = psSyncInfo->sWriteOpsCompleteDevVAddr;
		psTACmd->asSrcSyncs[i].sReadOpsCompleteDevVAddr = psSyncInfo->sReadOpsCompleteDevVAddr;
		psTACmd->asSrcSyncs[i].ui32ReadOpsPendingVal = psSyncInfo->psSyncData->ui32ReadOpsPending++;
		psTACmd->asSrcSyncs[i].ui32WriteOpsPendingVal = psSyncInfo->psSyncData->ui32WriteOpsPending;
	}
	if (psCCBKick->bFirstKickOrResume && psCCBKick->ui32NumDstSyncObjects > 0) {
		SGXMKIF_HWDEVICE_SYNC_LIST *psHWDeviceSyncList = psCCBKick->hKernelHWSyncListMemInfo->pvLinAddrKM;
		IMG_UINT32 ui32NumDstSyncs = psCCBKick->ui32NumDstSyncObjects;

		psHWDeviceSyncList->ui32NumSyncObjects = ui32NumDstSyncs;
		for (i = 0; i < ui32NumDstSyncs; i++) {
			psSyncInfo = psCCBKick->pahDstSyncHandles[i];
			if (psSyncInfo) {
				psHWDeviceSyncList->asSyncData[i].sWriteOpsCompleteDevVAddr = psSyncInfo->sWriteOpsCompleteDevVAddr;
				psHWDeviceSyncList->asSyncData[i].sReadOpsCompleteDevVAddr = psSyncInfo->sReadOpsCompleteDevVAddr;
				psHWDeviceSyncList->asSyncData[i].ui32ReadOpsPendingVal = psSyncInfo->psSyncData->ui32ReadOpsPending;
				psHWDeviceSyncList->asSyncData[i].ui32WriteOpsPendingVal = psSyncInfo->psSyncData->ui32WriteOpsPending++;
			} else {
				psHWDeviceSyncList->asSyncData[i].sWriteOpsCompleteDevVAddr.uiAddr = 0;
				psHWDeviceSyncList->asSyncData[i].sReadOpsCompleteDevVAddr.uiAddr = 0;
				psHWDeviceSyncList->asSyncData[i].ui32ReadOpsPendingVal = 0;
				psHWDeviceSyncList->asSyncData[i].ui32WriteOpsPendingVal = 0;
			}
		}
	}
	/* SCE: cache visibility and scheduler serialization. */
	SceCpuForDriver_E813EBB2();
	psTACmd->ui32CtrlFlags |= SGXMKIF_CMDTA_CTRLFLAGS_READY;
	LinuxLockMutex(&gPVRSRVLock);
	eError = SGXScheduleCCBCommandKM(psDeviceNode, SGXMKIF_CMD_TA,
			&psCCBKick->sCommand, KERNEL_ID, 0, NULL, psCCBKick->bLastInScene);
	LinuxUnLockMutex(&gPVRSRVLock);
	if (eError == PVRSRV_ERROR_RETRY) {
		if (psCCBKick->bFirstKickOrResume && psCCBKick->ui32NumDstSyncObjects > 0)
			for (i = 0; i < psCCBKick->ui32NumDstSyncObjects; i++) {
				psSyncInfo = psCCBKick->pahDstSyncHandles[i];
				if (psSyncInfo)
					psSyncInfo->psSyncData->ui32WriteOpsPending--;
			}
		for (i = 0; i < psCCBKick->ui32NumSrcSyncs; i++) {
			psSyncInfo = psCCBKick->ahSrcKernelSyncInfo[i];
			psSyncInfo->psSyncData->ui32ReadOpsPending--;
		}
	}
	return eError;
}
