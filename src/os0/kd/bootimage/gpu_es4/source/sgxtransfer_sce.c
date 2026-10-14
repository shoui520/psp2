/* IMG devices/sgx/sgxtransfer.c, GPL-2.0. Pristine candidate and license
 * remain in src/vendor/img-sgx-ddk-1.7.862890; SCE changes marked below.
 */
#include "sgxtransfer_sce.h"
#include <stddef.h>
#include <string.h>

#define SGXMKIF_CMD_2D 2
#define KERNEL_ID 0xffffffffU

extern int gPVRSRVLock;
int LinuxLockMutex(int *mutex);
int LinuxUnLockMutex(int *mutex);
IMG_VOID OSMemSet(IMG_VOID *pvDest, IMG_UINT8 ui8Value, IMG_SIZE_T ui32Size);
PVRSRV_ERROR SGXScheduleCCBCommandKM(PVRSRV_DEVICE_NODE *psDeviceNode,
		IMG_UINT32 eCmdType, SGXMKIF_COMMAND *psCommand,
		IMG_UINT32 ui32CallerID, IMG_UINT32 ui32PDumpFlags,
		IMG_HANDLE hDevMemContext, IMG_BOOL bLastInScene);

PVRSRV_ERROR SGXSubmit2DKM(PVRSRV_DEVICE_NODE *psDeviceNode,
		PVRSRV_2D_SGX_KICK *psKick)
{
	PVRSRV_KERNEL_MEM_INFO *psCCBMemInfo = psKick->commandMemory.memoryInfo;
	SGXMKIF_COMMAND sCommand;
	SGXMKIF_2DCMD_SHARED *ps2DCmd;
	PVRSRV_KERNEL_SYNC_INFO *psSyncInfo;
	PVRSRV_ERROR eError;
	IMG_UINT32 i;

	/* SCE uses the kernel memset import for the command packet. */
	memset(&sCommand, 0, sizeof(sCommand));
	if (psCCBMemInfo->uAllocSize < sizeof(*ps2DCmd) ||
			psKick->ui32SharedCmdCCBOffset > psCCBMemInfo->uAllocSize - sizeof(*ps2DCmd))
		return PVRSRV_ERROR_INVALID_PARAMS;
	ps2DCmd = (SGXMKIF_2DCMD_SHARED *)((IMG_UINT8 *)psCCBMemInfo->pvLinAddrKM +
			psKick->ui32SharedCmdCCBOffset);
	OSMemSet(ps2DCmd, 0, sizeof(*ps2DCmd));
	if (psKick->hTASyncInfo.syncInfo) {
		psSyncInfo = psKick->hTASyncInfo.syncInfo;
		ps2DCmd->sTASyncData.ui32WriteOpsPendingVal = psSyncInfo->psSyncData->ui32WriteOpsPending++;
		ps2DCmd->sTASyncData.ui32ReadOpsPendingVal = psSyncInfo->psSyncData->ui32ReadOpsPending;
		ps2DCmd->sTASyncData.sWriteOpsCompleteDevVAddr = psSyncInfo->sWriteOpsCompleteDevVAddr;
		ps2DCmd->sTASyncData.sReadOpsCompleteDevVAddr = psSyncInfo->sReadOpsCompleteDevVAddr;
	}
	if (psKick->h3DSyncInfo.syncInfo) {
		psSyncInfo = psKick->h3DSyncInfo.syncInfo;
		ps2DCmd->s3DSyncData.ui32WriteOpsPendingVal = psSyncInfo->psSyncData->ui32WriteOpsPending++;
		ps2DCmd->s3DSyncData.ui32ReadOpsPendingVal = psSyncInfo->psSyncData->ui32ReadOpsPending;
		ps2DCmd->s3DSyncData.sWriteOpsCompleteDevVAddr = psSyncInfo->sWriteOpsCompleteDevVAddr;
		ps2DCmd->s3DSyncData.sReadOpsCompleteDevVAddr = psSyncInfo->sReadOpsCompleteDevVAddr;
	}
	/* SCE status extension, preceding the pristine sync snapshots. */
	ps2DCmd->ui32NumStatusVals = psKick->ui32NumStatusVals;
	if (psKick->ui32NumStatusVals) {
		ps2DCmd->sCtlStatusInfo.sStatusDevAddr = psKick->sStatusUpdate.statusAddress;
		ps2DCmd->sCtlStatusInfo.ui32StatusValue = psKick->sStatusUpdate.statusValue;
	}
	ps2DCmd->ui32NumSrcSync = psKick->ui32NumSrcSync;
	for (i = 0; i < psKick->ui32NumSrcSync; i++) {
		psSyncInfo = psKick->ahSrcSyncInfo[i].syncInfo;
		ps2DCmd->sSrcSyncData[i].ui32WriteOpsPendingVal = psSyncInfo->psSyncData->ui32WriteOpsPending;
		ps2DCmd->sSrcSyncData[i].ui32ReadOpsPendingVal = psSyncInfo->psSyncData->ui32ReadOpsPending;
		ps2DCmd->sSrcSyncData[i].sWriteOpsCompleteDevVAddr = psSyncInfo->sWriteOpsCompleteDevVAddr;
		ps2DCmd->sSrcSyncData[i].sReadOpsCompleteDevVAddr = psSyncInfo->sReadOpsCompleteDevVAddr;
	}
	if (psKick->hDstSyncInfo.syncInfo) {
		psSyncInfo = psKick->hDstSyncInfo.syncInfo;
		ps2DCmd->sDstSyncData.ui32WriteOpsPendingVal = psSyncInfo->psSyncData->ui32WriteOpsPending;
		ps2DCmd->sDstSyncData.ui32ReadOpsPendingVal = psSyncInfo->psSyncData->ui32ReadOpsPending;
		ps2DCmd->sDstSyncData.sWriteOpsCompleteDevVAddr = psSyncInfo->sWriteOpsCompleteDevVAddr;
		ps2DCmd->sDstSyncData.sReadOpsCompleteDevVAddr = psSyncInfo->sReadOpsCompleteDevVAddr;
	}
	for (i = 0; i < psKick->ui32NumSrcSync; i++) {
		psSyncInfo = psKick->ahSrcSyncInfo[i].syncInfo;
		psSyncInfo->psSyncData->ui32ReadOpsPending++;
	}
	if (psKick->hDstSyncInfo.syncInfo) {
		psSyncInfo = psKick->hDstSyncInfo.syncInfo;
		psSyncInfo->psSyncData->ui32WriteOpsPending++;
	}
	sCommand.ui32Data[1] = psKick->sHW2DContextDevVAddr.uiAddr;
	/* SCE serializes scheduling; PDUMP and NO_HARDWARE are absent. */
	LinuxLockMutex(&gPVRSRVLock);
	eError = SGXScheduleCCBCommandKM(psDeviceNode, SGXMKIF_CMD_2D, &sCommand,
			KERNEL_ID, psKick->ui32PDumpFlags, NULL, IMG_FALSE);
	LinuxUnLockMutex(&gPVRSRVLock);
	if (eError == PVRSRV_ERROR_RETRY) {
		for (i = 0; i < psKick->ui32NumSrcSync; i++) {
			psSyncInfo = psKick->ahSrcSyncInfo[i].syncInfo;
			psSyncInfo->psSyncData->ui32ReadOpsPending--;
		}
		if (psKick->hDstSyncInfo.syncInfo) {
			psSyncInfo = psKick->hDstSyncInfo.syncInfo;
			psSyncInfo->psSyncData->ui32WriteOpsPending--;
		}
		if (psKick->hTASyncInfo.syncInfo) {
			psSyncInfo = psKick->hTASyncInfo.syncInfo;
			psSyncInfo->psSyncData->ui32WriteOpsPending--;
		}
		if (psKick->h3DSyncInfo.syncInfo) {
			psSyncInfo = psKick->h3DSyncInfo.syncInfo;
			psSyncInfo->psSyncData->ui32WriteOpsPending--;
		}
	}
	return eError;
}
