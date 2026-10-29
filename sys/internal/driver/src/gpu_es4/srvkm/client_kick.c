/* Licensed IMG client middleware recovered from the approved TOOL originals.
 * The released kernel-only DDK does not contain this client component: SCE
 * moves the client's TA kick into the kernel, behind the DOKICK bridge.
 */
#include <gpu/sce_ddk_port.h>
#include <gpu/device_sce.h>
#include "kick_sce.h"
#include "client_pb.h"
#include "client_ccb.h"
#include "system_sce.h"
#include <stddef.h>
#include <string.h>
#include <threadmgr/threadmgr.h>
#include <gpu/memory_sce.h>
#include <gpu/osfunc_sce.h>

extern int gPVRSRVLock;
extern SceUID exitingProcess;
extern IMG_UINT32 gpuSuspended;
int WaitSignalCounterKernel(volatile IMG_UINT32 *counter, IMG_UINT32 timeout);
PVRSRV_ERROR SGXDoKickKM(PVRSRV_DEVICE_NODE *psDeviceNode,
		SGX_CCB_KICK_KM *psCCBKick);

/* What the submit step takes from a prepared TA kick (SCE). */
typedef struct SGX_TAKICK {
	SGXMKIF_CMDTA *psCommand;
	IMG_UINT32 reserved;
	SGX_RTDATA *psRTData;
	SGX_RTDATASET *psRTDataSet;
	SGX_RENDERDETAILS *psRenderDetails;
	SGX_DEVICE_SYNC_LIST *psDeviceSyncList;
	struct SGX_RENDERCONTEXT *psRenderContext;
	IMG_UINT32 ui32KickFlags;
	IMG_UINT32 ui32Frame;
	IMG_BOOL bFirstKickOrResume;
	IMG_UINT32 reservedStatus;
	IMG_UINT32 ui32NumTAStatusVals;
	IMG_UINT32 ui32Num3DStatusVals;
	GpuTransferStatus asTAStatusUpdate[32];
	GpuTransferStatus as3DStatusUpdate[4];
	IMG_UINT32 ui32NumSrcSyncs;
	GpuSyncReference apsSrcSyncInfo[8];
	struct PVRSRV_KERNEL_SYNC_INFO *psTA3DSyncInfo;
	IMG_BOOL bTADependency;
	struct PVRSRV_KERNEL_SYNC_INFO *psTATQSyncInfo;
	struct PVRSRV_KERNEL_SYNC_INFO *ps3DTQSyncInfo;
} SGX_TAKICK;

_Static_assert(sizeof(SGX_TAKICK) == 536, "SCE TA kick");

/* The TA command and the register patches that follow it in the ring. */
typedef struct SGX_TACOMMAND {
	SGXMKIF_CMDTA sCommand;
	SGX_REGISTER_PATCH asMoreRegisterPatches[12];
} SGX_TACOMMAND;

static inline SGX_REGISTER_PATCH *PatchRenderRegister(SGX_REGISTER_PATCH *psPatch,
		IMG_UINT32 ui32HWRenderDetails, SGX_RENDERREGISTERS *psRegisters,
		IMG_UINT32 *pui32Register, IMG_UINT32 ui32Value)
{
	if (ui32Value != *pui32Register) {
		psPatch->sAddress.uiAddr = ui32HWRenderDetails +
				((IMG_UINT8 *)pui32Register - (IMG_UINT8 *)psRegisters);
		psPatch->ui32Value = ui32Value;
		psPatch++;
		*pui32Register = ui32Value;
	}
	return psPatch;
}

static inline PVRSRV_ERROR SubmitTAKick(PVRSRV_DEVICE_NODE *psDeviceNode,
		SGX_TAKICK *psTAKick)
{
	SGX_CCB_KICK_KM sCCBKick;
	struct SGX_RENDERCONTEXT *psRenderContext;
	struct GpuCommandRing *psRing;
	SGXMKIF_CMDTA *psCommand;
	IMG_UINT32 ui32Size, ui32AlignedSize, i;
	PVRSRV_ERROR eError;

	memset(&sCCBKick, 0, sizeof(sCCBKick));
	psRenderContext = psTAKick->psRenderContext;
	sceKernelLockMutex(psRenderContext->hMutex, 1, NULL);
	ui32Size = psTAKick->psCommand->ui32CommandSize;
	ui32AlignedSize = (ui32Size + 63) & ~63U;
	psCommand = AcquireCommandRingSpace(psRenderContext->psTACCB, ui32AlignedSize,
			&psRenderContext->waitGeneration);
	if (psCommand) {
		(*psTAKick->psRTDataSet->pui32PendingCount)++;
		psTAKick->psCommand->ui32CommandSize = ui32AlignedSize;
		OSMemCopy(psCommand, psTAKick->psCommand, ui32Size);
		psRing = psRenderContext->psTACCB;
		sCCBKick.ui32CCBOffset = *psRing->pui32WriteOffset +
				offsetof(SGXMKIF_CMDTA, sShared);
		*psRing->pui32WriteOffset = (*psRing->pui32WriteOffset + ui32AlignedSize) &
				(psRing->ui32BufferSize - 1);
		psRenderContext->bTACommandQueued = IMG_TRUE;
		sCCBKick.hCCBKernelMemInfo = psRenderContext->psTACCB->psBufferMemInfo;
		sCCBKick.sCommand.ui32Data[1] =
				psRenderContext->psHWRenderContextMemInfo->sDevVAddr.uiAddr;
		if (psTAKick->psTA3DSyncInfo) {
			sCCBKick.hTA3DSyncInfo = psTAKick->psTA3DSyncInfo;
			sCCBKick.bTADependency = psTAKick->bTADependency;
		}
		if (psTAKick->psTATQSyncInfo)
			sCCBKick.hTASyncInfo = psTAKick->psTATQSyncInfo;
		if (psTAKick->ps3DTQSyncInfo)
			sCCBKick.h3DSyncInfo = psTAKick->ps3DTQSyncInfo;
		for (i = 0; i < psTAKick->ui32NumTAStatusVals; i++)
			sCCBKick.asTAStatusUpdate[i] = psTAKick->asTAStatusUpdate[i];
		sCCBKick.ui32NumTAStatusVals = psTAKick->ui32NumTAStatusVals;
		for (i = 0; i < psTAKick->ui32Num3DStatusVals; i++)
			sCCBKick.as3DStatusUpdate[i] = psTAKick->as3DStatusUpdate[i];
		sCCBKick.ui32Num3DStatusVals = psTAKick->ui32Num3DStatusVals;
		for (i = 0; i != psTAKick->ui32NumSrcSyncs; i++)
			sCCBKick.ahSrcKernelSyncInfo[i] = psTAKick->apsSrcSyncInfo[i].syncInfo;
		sCCBKick.ui32NumSrcSyncs = psTAKick->ui32NumSrcSyncs;
		if (psTAKick->psDeviceSyncList) {
			sCCBKick.ui32NumDstSyncObjects =
					psTAKick->psDeviceSyncList->ui32NumSyncObjects;
			sCCBKick.pahDstSyncHandles = psTAKick->psDeviceSyncList->apsSyncHandles;
			sCCBKick.hKernelHWSyncListMemInfo =
					psTAKick->psDeviceSyncList->psDeviceSyncListMemInfo;
		}
		sCCBKick.bFirstKickOrResume = psTAKick->bFirstKickOrResume;
		sCCBKick.hDevMemContext = psRenderContext->hDevMemContext;
		sCCBKick.bLastInScene = (psCommand->ui32Flags & 2) != 0;
		eError = SGXDoKickKM(psDeviceNode, &sCCBKick);
	} else {
		eError = PVRSRV_ERROR_TIMEOUT;
	}
	sceKernelUnlockMutex(psRenderContext->hMutex, 1);
	return eError;
}

PVRSRV_ERROR SGXKickTA(PVRSRV_DEVICE_NODE *device, SGX_KICKTA *kick,
		SGX_KICKTA_RESULT *result, IMG_UINT32 flags, IMG_UINT32 unused)
{
	SGX_RTDATASET *psRTDataSet = kick->target.renderTarget;
	struct SGX_RENDERCONTEXT *psRenderContext;
	SGX_CLIENTPBDESC *psClientPBDesc;
	SGX_PBDESC *psPBDesc, *psRenderPBDesc;
	SGX_RENDERDETAILS *psRenderDetails;
	SGX_RENDERREGISTERS *psRegisters;
	struct SGXMKIF_HWRENDERDETAILS *psHWRenderDetails;
	SGX_DEVICE_SYNC_LIST *psSyncList;
	SGX_RTDATA *psRTData;
	PVRSRV_KERNEL_MEM_INFO *psHWPBDescMemInfo;
	SGX_REGISTER_PATCH *psPatch;
	SGXMKIF_TAREGISTERS *psTARegisters;
	IMG_DEV_VIRTADDR sHWRTDataSetDevVAddr;
	SGX_TAKICK sTAKick;
	SGX_TACOMMAND sTACommand;
	SceUID pid;
	IMG_BOOL bNewSyncList;
	IMG_UINT32 ui32KickFlags, ui32SceneFlags, ui32Frame, ui32Flags, ui32TAControl;
	IMG_UINT32 ui32NumRegisterPatches, ui32RTDataSel, ui32NumRTData;
	IMG_UINT32 ui32ISPMultiSampleCtl, ui32ISPMultiSampleCtl2;
	IMG_UINT32 ui32ISPRender, ui32ZLSBase, ui32Value, ui32TECtrl, ui32AAMode;
	IMG_UINT32 ui32TAScreenXMax, ui32TAScreenYMax, ui32VertexPartitions;
	IMG_UINT32 ui32BGObjWidth, ui32BGObjHeight, ui32NumTAStatusVals, ui32Num3DStatusVals;
	IMG_UINT32 ui32HWRenderDetails, ui32VisTestAddr;
	IMG_BOOL bDisablePixelSplit;
	PVRSRV_ERROR eError;
	IMG_UINT32 i;

	if (!psRTDataSet || psRTDataSet->ui16NumRTsInArray > 1)
		return PVRSRV_ERROR_INVALID_PARAMS;
	LinuxUnLockMutex(&gPVRSRVLock);
	sceKernelLockMutex(psRTDataSet->hMutex, 1, NULL);
	psRenderContext = kick->context.renderContext;
	psClientPBDesc = psRenderContext->psClientPBDesc;
	psPBDesc = psClientPBDesc->psPBDesc;
	OSMemSet(&sTAKick, 0, sizeof(sTAKick));
	OSMemSet(&sTACommand, 0, sizeof(sTACommand.sCommand));
	OSMemSet(&result->output, 0, sizeof(result->output));
	result->output.bSPMOutOfMemEvent = IMG_FALSE;
	for (i = 0; i != psRTDataSet->ui32NumRTData; i++) {
		if (psRTDataSet->psHWRTDataSet->asRTData[i].ui32RTStatus & 0x80) {
			result->output.bSPMOutOfMemEvent = IMG_TRUE;
			result->output.ui32MaxZTileX = 2 * psRTDataSet->ui32MTileX2;
			result->output.ui32MaxZTileY = 2 * psRTDataSet->ui32MTileY2;
			break;
		}
	}

	/* A new scene takes a free device sync list, waiting for one. */
	if (kick->sKickTACommon.ui32KickFlags & 0x14) {
		psSyncList = NULL;
		if (kick->renderSync.pahRenderSurfSyncInfo) {
			pid = sceKernelGetProcessId();
			for (;;) {
				for (psSyncList = psRTDataSet->psDeviceSyncList; psSyncList;
						psSyncList = psSyncList->psNext)
					if (!*psSyncList->pui32Lock)
						break;
				if (psSyncList)
					break;
				if (exitingProcess == pid || gpuSuspended)
					goto retry;
				WaitSignalCounterKernel(&psRenderContext->waitGeneration, 100000);
			}
			*psSyncList->pui32Lock = 1;
		}
		bNewSyncList = IMG_TRUE;
	} else {
		psSyncList = psRTDataSet->psCurrentDeviceSyncList;
		bNewSyncList = IMG_FALSE;
	}

	ui32KickFlags = kick->sKickTACommon.ui32KickFlags;
	if (ui32KickFlags & 4) {
		/* A new render takes free render details, waiting for them. */
		pid = sceKernelGetProcessId();
		for (;;) {
			for (psRenderDetails = psRTDataSet->psRenderDetailsList; psRenderDetails;
					psRenderDetails = psRenderDetails->psNext)
				if (!*psRenderDetails->pui32Lock)
					break;
			if (psRenderDetails)
				break;
			if (exitingProcess == pid || gpuSuspended) {
				if (bNewSyncList && psSyncList)
					*psSyncList->pui32Lock = 0;
				goto retry;
			}
			WaitSignalCounterKernel(&psRenderContext->waitGeneration, 100000);
		}
		*psRenderDetails->pui32Lock = 1;
		psRTDataSet->psCurrentRenderDetails = psRenderDetails;
		psHWRenderDetails = psRenderDetails->psHWRenderDetails;
		psRTDataSet->ui32RTDataSel = (psRTDataSet->ui32RTDataSel + 1) %
				psRTDataSet->ui32NumRTData;
		sHWRTDataSetDevVAddr = psRTDataSet->psHWRTDataSetMemInfo->sDevVAddr;
		psHWRenderDetails->sHWRTDataSetDevVAddr = sHWRTDataSetDevVAddr;
		psHWRenderDetails->sHWRTDataDevVAddr.uiAddr = sHWRTDataSetDevVAddr.uiAddr +
				offsetof(struct SGXMKIF_HWRTDATASET, asRTData) +
				psRTDataSet->ui32RTDataSel * sizeof(SGXMKIF_HWRTDATA);
		psHWRenderDetails->firmwarePrivateState[11] = 0;
		psHWRenderDetails->firmwarePrivateState[12] = 0;
		psHWRenderDetails->firmwarePrivateState[13] = 0;
		psHWRenderDetails->firmwarePrivateState[14] = 0;
		psHWRenderDetails->firmwarePrivateState[15] = 0;
		psRTData = psRTDataSet->psRTData;
		ui32RTDataSel = psRTDataSet->ui32RTDataSel;
		ui32NumRTData = psRTDataSet->ui32NumRTData;
		psRenderPBDesc = psRenderContext->psClientPBDesc->psPBDesc;
		ui32ISPMultiSampleCtl = psRTDataSet->ui32ISPMultiSampleCtl;
		ui32ISPMultiSampleCtl2 = psRTDataSet->ui32ISPMultiSampleCtl2;
		psRegisters = &psRenderDetails->sRegisters;
		psRegisters->ui32PixelBE = kick->sKickTACommon.ui32PixelBE | 0x80;
		if (psRTDataSet->ui32NumPixelsX > 0x800 || psRTDataSet->ui32NumPixelsY > 0x800)
			psRegisters->bLargeRenderTarget = IMG_TRUE;
		else
			psRegisters->bLargeRenderTarget = IMG_FALSE;
		for (i = 0; i != 4; i++)
			psRegisters->asVisTestResultDevVAddr[i].uiAddr =
					kick->sKickTACommon.ui32KickFlags & 0x20 ?
					kick->sKickTACommon.sVisTestResultMem.uiAddr +
					i * 4 * kick->sKickTACommon.ui32VisTestCount : 0;
		for (i = 0; i != 4; i++)
			psRegisters->asRgnHeaderDevVAddr[i] =
					psRTData[ui32RTDataSel].apsRgnHeaderMemInfo[i]->sDevVAddr;
		psRegisters->ui32RgnHeaderAllocSize =
				psRTData[ui32RTDataSel].apsRgnHeaderMemInfo[0]->uAllocSize;
		psRegisters->ui32MTileStride = psRTDataSet->ui32MTileStride;
		psRegisters->ui32TEMTile1 = psRTDataSet->ui32MTileX1 << 22 |
				psRTDataSet->ui32MTileNumber << 31 | psRTDataSet->ui32MTileX3 |
				psRTDataSet->ui32MTileX2 << 12;
		psRegisters->ui32TEMTile2 = psRTDataSet->ui32MTileY2 << 12 |
				psRTDataSet->ui32MTileY1 << 22 | psRTDataSet->ui32MTileY3;
		psRegisters->ui32ISPIPFMisc = kick->sKickTACommon.ui32ISPIPFMisc |
				kick->sKickTACommon.ui32ISPValidId;
		psRegisters->ui32DoublePixelPartitions =
				kick->sKickTACommon.ui32DoublePixelPartitions;
		psRegisters->bColourAlphaValid = psRTDataSet->ui32Flags & 1;
		psRegisters->ui32ISPPerpendicular = kick->sKickTACommon.ui32ISPPerpendicular;
		psRegisters->ui32ISPCullValue = kick->sKickTACommon.ui32ISPCullValue;
		/* SCE: every next region header is the next render's first. */
		for (i = 0; i != 4; i++)
			psRegisters->asNextRgnHeaderDevVAddr[i] =
					psRTData[(ui32RTDataSel + 1) % ui32NumRTData].apsRgnHeaderMemInfo[0]->sDevVAddr;
		psRegisters->ui32ISPDBias = kick->sKickTACommon.ui32ISPDBias;
		psRegisters->ui323DAAMode = psRTDataSet->ui323DAAMode;
		psRegisters->ui32ZLSCtl = kick->sKickTACommon.ui32ZLSCtl;
		psRegisters->ui32ISPBGObjDepth = kick->sKickTACommon.ui32ISPBGObjDepth;
		psRegisters->ui32ISPBGObj = kick->sKickTACommon.ui32ISPBGObj |
				kick->sKickTACommon.ui32ISPSceneBGObj;
		psRegisters->ui32BGObjTexPtr = psRTData[ui32RTDataSel].ui32BGObjPtr1;
		psRegisters->ui32ISPMultiSampleCtl2 = ui32ISPMultiSampleCtl2;
		psRegisters->ui32ISPMultiSampleCtl = ui32ISPMultiSampleCtl;
		psRegisters->ui32ISPRender = psRenderContext->bForcePTOff ? 2 : 0;
		if (kick->sKickTACommon.ui32TriangleSplitPixelThreshold) {
			psRegisters->ui32ISPRender |=
					kick->sKickTACommon.ui32TriangleSplitPixelThreshold << 4;
			bDisablePixelSplit = IMG_FALSE;
		} else {
			psRegisters->ui32ISPRender |= 8;
			bDisablePixelSplit = !kick->sKickTACommon.bPerQuadrantPixelSplit;
		}
		psRegisters->bDisablePixelSplit = bDisablePixelSplit;
		psRegisters->ui32ISPRenderFlags = 16;
		psRegisters->ui32ParameterBufferBase =
				psRenderPBDesc->sPBBaseDevVAddr.uiAddr >> 20 << 20;
		if ((kick->sKickTACommon.ui32ZLSCtl & 0xe00000) < 0x800000 ||
				(kick->sKickTACommon.ui32ZLSCtl & 0x100000))
			ui32ZLSBase = 0;
		else
			ui32ZLSBase = psRenderPBDesc->sPBBaseDevVAddr.uiAddr >> 20 << 20;
		psRegisters->ui32ZLSBase = ui32ZLSBase;
		psRegisters->ui32ISPZLoadBase =
				(kick->sKickTACommon.sISPZLoadBase.uiAddr - ui32ZLSBase) & ~15U;
		psRegisters->ui32ISPZStoreBase =
				(kick->sKickTACommon.sISPZStoreBase.uiAddr - ui32ZLSBase) & ~15U;
		if (kick->sKickTACommon.bSeperateStencilLoad)
			psRegisters->ui32ISPStencilLoadBase =
					((kick->sKickTACommon.sISPStencilLoadBase.uiAddr - ui32ZLSBase) & ~15U) | 1;
		else
			psRegisters->ui32ISPStencilLoadBase =
					(kick->sKickTACommon.sISPStencilLoadBase.uiAddr - ui32ZLSBase) & ~15U;
		ui32Value = (kick->sKickTACommon.sISPStencilStoreBase.uiAddr - ui32ZLSBase) & ~15U;
		if (kick->sKickTACommon.bSeperateStencilStore)
			ui32Value |= 1;
		psRegisters->ui32ISPStencilStoreBase = ui32Value;
		psRegisters->ui32EDMPixelPDSAddr = kick->sKickTACommon.ui32EDMPixelPDSAddr;
		psRegisters->ui32EDMPixelPDSDataSize = kick->sKickTACommon.ui32EDMPixelPDSDataSize;
		psRegisters->ui32EDMPixelPDSInfo = kick->sKickTACommon.ui32EDMPixelPDSInfo;
		psRenderDetails->psHWRenderDetails->ui32PixelPartitions =
				psRenderContext->ui32NumPixelPartitions ?
				psRenderContext->ui32NumPixelPartitions << 22 : 0x2000000;
		psRegisters->ui32Filter0Left = kick->sKickTACommon.ui32Filter0Left;
		psRegisters->ui32Filter0Right = kick->sKickTACommon.ui32Filter0Right;
		psRegisters->ui32Filter0Extra = kick->sKickTACommon.ui32Filter0Extra;
		psRegisters->ui32Filter1Left = kick->sKickTACommon.ui32Filter1Left;
		psRegisters->ui32Filter1Right = kick->sKickTACommon.ui32Filter1Right;
		psRegisters->ui32Filter1Extra = kick->sKickTACommon.ui32Filter1Extra;
		psRegisters->ui32Filter2Left = kick->sKickTACommon.ui32Filter2Left;
		psRegisters->ui32Filter2Right = kick->sKickTACommon.ui32Filter2Right;
		psRegisters->ui32Filter2Extra = kick->sKickTACommon.ui32Filter2Extra;
		psRegisters->ui32FilterTable = kick->sKickTACommon.ui32FilterTable;
		psRegisters->ui32CSC0Coeff01 = kick->sKickTACommon.ui32CSC0Coeff01;
		psRegisters->ui32CSC0Coeff23 = kick->sKickTACommon.ui32CSC0Coeff23;
		psRegisters->ui32CSC0Coeff45 = kick->sKickTACommon.ui32CSC0Coeff45;
		psRegisters->ui32CSC0Coeff67 = kick->sKickTACommon.ui32CSC0Coeff67;
		psRegisters->ui32CSC1Coeff01 = kick->sKickTACommon.ui32CSC1Coeff01;
		psRegisters->ui32CSC1Coeff23 = kick->sKickTACommon.ui32CSC1Coeff23;
		psRegisters->ui32CSC1Coeff45 = kick->sKickTACommon.ui32CSC1Coeff45;
		psHWRenderDetails = psRenderDetails->psHWRenderDetails;
		psRegisters->ui32CSC1Coeff67 = kick->sKickTACommon.ui32CSC1Coeff67;
		psRegisters->ui32CSC0Coeff89 = kick->sKickTACommon.ui32CSC0Coeff89;
		psRegisters->ui32CSC0Coeff1011 = kick->sKickTACommon.ui32CSC0Coeff1011;
		psRegisters->ui32CSC1Coeff89 = kick->sKickTACommon.ui32CSC1Coeff89;
		psRegisters->ui32CSC1Coeff1011 = kick->sKickTACommon.ui32CSC1Coeff1011;
		OSMemCopy(psHWRenderDetails, psRegisters, sizeof(*psRegisters));
		ui32Flags = 1;
		ui32NumRegisterPatches = 0;
	} else {
		/* The same render: patch only the registers that changed. */
		psRenderDetails = psRTDataSet->psCurrentRenderDetails;
		psRenderPBDesc = psRenderContext->psClientPBDesc->psPBDesc;
		ui32HWRenderDetails = psRenderDetails->psHWRenderDetailsMemInfo->sDevVAddr.uiAddr;
		psRegisters = &psRenderDetails->sRegisters;
		psPatch = sTACommand.sCommand.asRegisterPatches;
		if (kick->sKickTACommon.sVisTestResultMem.uiAddr !=
				psRegisters->asVisTestResultDevVAddr[0].uiAddr &&
				(ui32KickFlags & 0x20)) {
			for (i = 0; i != 4; i++) {
				ui32VisTestAddr = kick->sKickTACommon.sVisTestResultMem.uiAddr +
						i * 4 * kick->sKickTACommon.ui32VisTestCount;
				psPatch[i].sAddress.uiAddr = ui32HWRenderDetails + 4 * i;
				psPatch[i].ui32Value = ui32VisTestAddr;
				psRegisters->asVisTestResultDevVAddr[i].uiAddr = ui32VisTestAddr;
			}
			psPatch += 4;
		}
		psPatch = PatchRenderRegister(psPatch, ui32HWRenderDetails, psRegisters,
				&psRegisters->ui32EDMPixelPDSAddr,
				kick->sKickTACommon.ui32EDMPixelPDSAddr);
		psPatch = PatchRenderRegister(psPatch, ui32HWRenderDetails, psRegisters,
				&psRegisters->ui32EDMPixelPDSDataSize,
				kick->sKickTACommon.ui32EDMPixelPDSDataSize);
		psPatch = PatchRenderRegister(psPatch, ui32HWRenderDetails, psRegisters,
				&psRegisters->ui32EDMPixelPDSInfo,
				kick->sKickTACommon.ui32EDMPixelPDSInfo);
		psPatch = PatchRenderRegister(psPatch, ui32HWRenderDetails, psRegisters,
				&psRegisters->ui32ISPIPFMisc,
				kick->sKickTACommon.ui32ISPIPFMisc | kick->sKickTACommon.ui32ISPValidId);
		psPatch = PatchRenderRegister(psPatch, ui32HWRenderDetails, psRegisters,
				&psRegisters->ui32ISPBGObj,
				kick->sKickTACommon.ui32ISPBGObj | kick->sKickTACommon.ui32ISPSceneBGObj);
		psPatch = PatchRenderRegister(psPatch, ui32HWRenderDetails, psRegisters,
				&psRegisters->ui32ISPDBias, kick->sKickTACommon.ui32ISPDBias);
		psPatch = PatchRenderRegister(psPatch, ui32HWRenderDetails, psRegisters,
				&psRegisters->ui32ZLSCtl, kick->sKickTACommon.ui32ZLSCtl);
		if ((kick->sKickTACommon.ui32ZLSCtl & 0xe00000) < 0x800000 ||
				(kick->sKickTACommon.ui32ZLSCtl & 0x100000))
			ui32ZLSBase = 0;
		else
			ui32ZLSBase = psRenderPBDesc->sPBBaseDevVAddr.uiAddr >> 20 << 20;
		psPatch = PatchRenderRegister(psPatch, ui32HWRenderDetails, psRegisters,
				&psRegisters->ui32ZLSBase, ui32ZLSBase);
		psPatch = PatchRenderRegister(psPatch, ui32HWRenderDetails, psRegisters,
				&psRegisters->ui32ISPZLoadBase,
				(kick->sKickTACommon.sISPZLoadBase.uiAddr - ui32ZLSBase) & ~15U);
		psPatch = PatchRenderRegister(psPatch, ui32HWRenderDetails, psRegisters,
				&psRegisters->ui32ISPZStoreBase,
				(kick->sKickTACommon.sISPZStoreBase.uiAddr - ui32ZLSBase) & ~15U);
		ui32Value = (kick->sKickTACommon.sISPStencilLoadBase.uiAddr - ui32ZLSBase) & ~15U;
		if (kick->sKickTACommon.bSeperateStencilLoad)
			ui32Value |= 1;
		psPatch = PatchRenderRegister(psPatch, ui32HWRenderDetails, psRegisters,
				&psRegisters->ui32ISPStencilLoadBase, ui32Value);
		ui32Value = (kick->sKickTACommon.sISPStencilStoreBase.uiAddr - ui32ZLSBase) & ~15U;
		if (kick->sKickTACommon.bSeperateStencilStore)
			ui32Value |= 1;
		psPatch = PatchRenderRegister(psPatch, ui32HWRenderDetails, psRegisters,
				&psRegisters->ui32ISPStencilStoreBase, ui32Value);
		psPatch = PatchRenderRegister(psPatch, ui32HWRenderDetails, psRegisters,
				&psRegisters->ui32DoublePixelPartitions,
				kick->sKickTACommon.ui32DoublePixelPartitions);
		ui32Value = kick->sKickTACommon.ui32TriangleSplitPixelThreshold;
		ui32ISPRender = psRenderContext->bForcePTOff ? 2 : 0;
		if (ui32Value) {
			ui32ISPRender |= ui32Value << 4;
			bDisablePixelSplit = IMG_FALSE;
		} else {
			ui32ISPRender |= 8;
			bDisablePixelSplit = !kick->sKickTACommon.bPerQuadrantPixelSplit;
		}
		psPatch = PatchRenderRegister(psPatch, ui32HWRenderDetails, psRegisters,
				&psRegisters->ui32ISPRender, ui32ISPRender);
		psPatch = PatchRenderRegister(psPatch, ui32HWRenderDetails, psRegisters,
				(IMG_UINT32 *)&psRegisters->bDisablePixelSplit, bDisablePixelSplit);
		ui32Flags = 0;
		ui32NumRegisterPatches = psPatch - sTACommand.sCommand.asRegisterPatches;
	}

	if (bNewSyncList)
		psRTDataSet->psCurrentDeviceSyncList = psSyncList;
	ui32KickFlags = kick->sKickTACommon.ui32KickFlags;
	if ((ui32KickFlags & 0x90) == 0x10)
		ui32Flags |= ui32KickFlags & 0x100 ? 4 : 0xc;
	ui32TAControl = ui32KickFlags & 0x80000 ? 0x80 : 0;
	if (ui32KickFlags & 0x100000)
		ui32TAControl |= 0x100;
	if (!(ui32KickFlags & 2))
		ui32TAControl |= 1;
	if (ui32KickFlags & 0x800)
		ui32TAControl |= 2;
	if (ui32KickFlags & 0x3000)
		ui32TAControl |= 8;
	if ((ui32KickFlags & 0x5008) != 8)
		ui32TAControl |= 0x10;
	if (ui32KickFlags & 0x400)
		ui32TAControl |= 0x1000;
	if (ui32KickFlags & 0x4000000)
		ui32TAControl |= 0x800;
	if (!(psRTDataSet->ui32Flags & 0x10))
		ui32TAControl |= 0x2000;
	if (ui32KickFlags & 0x10000)
		ui32Flags |= 0x40;
	if (ui32KickFlags & 0x20000)
		ui32Flags |= 0x20;
	if (ui32KickFlags & 0x40000)
		ui32Flags |= 0x10;
	if (ui32KickFlags & 0x40000000)
		ui32Flags |= 0x10000000;
	if (ui32KickFlags & 0x2000000)
		ui32Flags |= 0x80;
	if (ui32KickFlags & 0x20000000)
		ui32Flags |= 0x4000;
	psRenderPBDesc = psRenderContext->psClientPBDesc->psPBDesc;
	sTACommand.sCommand.ui32PrimitiveControl = 0x4000000;
	ui32VertexPartitions = psRenderContext->ui32NumVertexPartitions;
	if (!ui32VertexPartitions)
		ui32VertexPartitions = 3;
	sTACommand.sCommand.ui32VertexPartitions = ui32VertexPartitions << 26;
	if (ui32KickFlags & 0xa00)
		ui32TECtrl = ui32KickFlags & 1 ? 0x400000 : 0x800000;
	else
		ui32TECtrl = 0;
	ui32TAScreenYMax = kick->sKickTACommon.ui32TAScreenYMax;
	if (psRenderContext->ui32TAKickRegFlags & 2)
		ui32TECtrl |= 0x80000;
	if (kick->sKickTACommon.ui32ZLSCtl & 0x100000)
		ui32TECtrl |= 0x2000000;
	ui32AAMode = psRTDataSet->ui16MSAASamplesInX == 2 ? 0x80000000 : 0;
	if (psRTDataSet->ui16MSAASamplesInY == 2)
		ui32AAMode |= 0x40000000;
	psTARegisters = &sTACommand.sCommand.sRegisters;
	psTARegisters->ui32AAMode = ui32AAMode;
	psTARegisters->ui32TEMTile1 = psRTDataSet->ui32MTileX1 << 22 |
			psRTDataSet->ui32MTileNumber << 31 | psRTDataSet->ui32MTileX3 |
			psRTDataSet->ui32MTileX2 << 12;
	psTARegisters->ui32TEMTile2 = psRTDataSet->ui32MTileY2 << 12 |
			psRTDataSet->ui32MTileY1 << 22 | psRTDataSet->ui32MTileY3;
	ui32TAScreenXMax = kick->sKickTACommon.ui32TAScreenXMax;
	psTARegisters->ui32TEScreen = ui32TAScreenXMax >> 5 | ui32TAScreenYMax >> 5 << 12;
	psTARegisters->ui32MTileStride = psRTDataSet->ui32MTileStride;
	psTARegisters->ui32TECtrl = ui32TECtrl;
	for (i = 0; i != 4; i++) {
		psTARegisters->asRgnHeaderDevVAddr[i] = psRTDataSet->psRTData[
				psRTDataSet->ui32RTDataSel].apsRgnHeaderMemInfo[i]->sDevVAddr;
		psTARegisters->asTailPtrsDevVAddr[i] =
				psRTDataSet->apsTailPtrsMemInfo[i]->sDevVAddr;
	}
	psTARegisters->sTABufferCtlStreamBase = kick->sKickTACommon.sTABufferCtlStreamBase;
	psTARegisters->ui32MTECtrl = kick->sKickTACommon.ui32MTECtrl;
	psTARegisters->ui32MultiSampleLocationsX = 0x358635bd;
	psTARegisters->ui32MultiSampleLocationsY = 0x358635bd;
	psTARegisters->ui32ScreenPixels = ui32TAScreenXMax | ui32TAScreenYMax << 12;
	psTARegisters->ui323DAAMode = psRTDataSet->ui32MTEMultiSampleCtl;
	psTARegisters->ui32USELoadRedirect = kick->sKickTACommon.ui32USELoadRedirect;
	psTARegisters->ui32USEStoreRange = kick->sKickTACommon.ui32USEStoreRange;
	psTARegisters->ui32ParameterBufferBase = psRenderPBDesc->sPBBaseDevVAddr.uiAddr >> 20 << 20;
	psTARegisters->bLargeRenderTarget = psRTDataSet->ui32NumPixelsX > 0x800 ||
			psRTDataSet->ui32NumPixelsY > 0x800;
	if (kick->sKickTACommon.ui16MaxDrawCallsPerCore)
		psTARegisters->ui32PrimitiveSplitThreshold =
				kick->sKickTACommon.ui16PrimitiveSplitThreshold | 0x50000 |
				(kick->sKickTACommon.ui16MaxDrawCallsPerCore - 1) << 20;
	else
		psTARegisters->ui32PrimitiveSplitThreshold =
				kick->sKickTACommon.ui16PrimitiveSplitThreshold | 0x10000;
	sTAKick.ui32NumSrcSyncs = 0;
	if (ui32KickFlags & 0xa00) {
		ui32SceneFlags = kick->sKickTACommon.ui32SceneFlags;
		ui32TAControl |= 0x400;
		ui32Flags |= 2;
		if (ui32KickFlags & 0x8000)
			ui32TAControl |= 4;
		ui32TAControl |= (ui32SceneFlags & 4 ? 0x4000 : 0) |
				(ui32SceneFlags & 8 ? 0x8000 : 0);
		sTAKick.ui32NumSrcSyncs = kick->sKickTACommon.ui32NumSrcSyncs;
	}
	if (ui32KickFlags & 0x20) {
		sTACommand.sCommand.ui32VisTestCount = kick->sKickTACommon.ui32VisTestCount;
		if (!(ui32KickFlags & 0x40))
			ui32TAControl |= 0x40;
		ui32TAControl |= 0x20;
	}
	if (ui32KickFlags & 0x1800000) {
		sTAKick.psTA3DSyncInfo = psRenderContext->psTA3DSyncObject;
		if (ui32KickFlags & 0x800000)
			ui32Flags |= 0x400;
		sTAKick.bTADependency = IMG_FALSE;
		if (ui32KickFlags & 0x1000000) {
			ui32TAControl |= 0x200;
			sTAKick.bTADependency = IMG_TRUE;
		}
	} else {
		sTAKick.psTA3DSyncInfo = NULL;
		sTAKick.bTADependency = IMG_FALSE;
	}
	ui32SceneFlags = kick->sKickTACommon.ui32SceneFlags;
	sTACommand.sCommand.ui32TAControl = ui32TAControl;
	ui32Frame = kick->sKickTACommon.ui32Frame;
	ui32Flags |= (ui32SceneFlags & 1 ? 0x400000 : 0) |
			(ui32SceneFlags & 2 ? 0x800000 : 0) |
			(ui32SceneFlags & 4 ? 0x1000000 : 0) |
			(ui32SceneFlags & 8 ? 0x2000000 : 0) |
			(ui32SceneFlags & 0x10 ? 0x4000000 : 0);
	sTACommand.sCommand.ui32Frame = ui32Frame;
	sTACommand.sCommand.ui32Scene = kick->sKickTACommon.ui32Scene;
	sTACommand.sCommand.ui32Flags = ui32Flags;
	sTACommand.sCommand.ui32CommandSize = sizeof(SGXMKIF_CMDTA) +
			ui32NumRegisterPatches * sizeof(SGX_REGISTER_PATCH);
	psHWPBDescMemInfo = psClientPBDesc->psHWPBDescMemInfo;
	sTACommand.sCommand.sHWPBDescDevVAddr = psHWPBDescMemInfo->sDevVAddr;
	sTACommand.sCommand.sHWRenderDetailsDevVAddr =
			psRenderDetails->psHWRenderDetailsMemInfo->sDevVAddr;
	sTACommand.sCommand.sHWDeviceSyncListDevVAddr.uiAddr = psSyncList ?
			psSyncList->psHWDeviceSyncListMemInfo->sDevVAddr.uiAddr : 0;
	sTACommand.sCommand.sHWRTDataSetDevVAddr = psRTDataSet->psHWRTDataSetMemInfo->sDevVAddr;
	sTACommand.sCommand.sHWRTDataDevVAddr.uiAddr =
			sTACommand.sCommand.sHWRTDataSetDevVAddr.uiAddr +
			offsetof(struct SGXMKIF_HWRTDATASET, asRTData) +
			psRTDataSet->ui32RTDataSel * sizeof(SGXMKIF_HWRTDATA);
	sTAKick.psTATQSyncInfo = ui32KickFlags & 0x200000 && kick->transfer.transferQueue ?
			kick->transfer.transferQueue->sourceSync : NULL;
	sTAKick.ps3DTQSyncInfo = ui32KickFlags & 0x400000 && kick->transfer.transferQueue ?
			kick->transfer.transferQueue->destinationSync : NULL;
	sTACommand.sCommand.ui32NumRegisterPatches = ui32NumRegisterPatches;
	sTACommand.sCommand.aui32SpecObject[0] = kick->sKickTACommon.aui32SpecObject[0];
	sTACommand.sCommand.aui32SpecObject[1] = kick->sKickTACommon.aui32SpecObject[1];
	sTACommand.sCommand.aui32SpecObject[2] = kick->sKickTACommon.aui32SpecObject[2];
	ui32BGObjHeight = kick->sKickTACommon.ui32BGObjHeight;
	ui32BGObjWidth = kick->sKickTACommon.ui32BGObjWidth;
	if (!ui32BGObjWidth)
		ui32BGObjWidth = psRTDataSet->ui32NumPixelsX;
	if (!ui32BGObjHeight)
		ui32BGObjHeight = psRTDataSet->ui32NumPixelsY;
	sTAKick.psCommand = &sTACommand.sCommand;
	ui32Num3DStatusVals = kick->sKickTACommon.ui32Num3DStatusVals;
	sTACommand.sCommand.ui32BackgroundSize = (ui32BGObjWidth + 1024) << 20 |
			16 * (ui32BGObjHeight + 1024);
	ui32NumTAStatusVals = kick->sKickTACommon.ui32NumTAStatusVals;
	sTAKick.ui32Num3DStatusVals = ui32Num3DStatusVals;
	sTAKick.bFirstKickOrResume = (ui32Flags & 5) != 0;
	sTAKick.ui32NumTAStatusVals = ui32NumTAStatusVals;
	for (i = 0; i != ui32NumTAStatusVals; i++)
		sTAKick.asTAStatusUpdate[i] = kick->asTAStatusUpdate[i];
	for (i = 0; i != ui32Num3DStatusVals; i++)
		sTAKick.as3DStatusUpdate[i] = kick->as3DStatusUpdate[i];
	sTAKick.psRenderDetails = psRenderDetails;
	sTAKick.psDeviceSyncList = psSyncList;
	sTAKick.psRTData = &psRTDataSet->psRTData[psRTDataSet->ui32RTDataSel];
	sTAKick.psRTDataSet = psRTDataSet;
	sTAKick.psRenderContext = kick->context.renderContext;
	sTAKick.ui32KickFlags = ui32KickFlags;
	sTAKick.ui32Frame = ui32Frame;
	if (psSyncList) {
		ui32NumRTData = psRTDataSet->ui16NumRTsInArray;
		psSyncList->ui32NumSyncObjects = ui32NumRTData;
		for (i = 0; i < ui32NumRTData; i++)
			psSyncList->apsSyncHandles[i] = kick->renderSync.ppsRenderSurfSyncInfo[i];
	}
	for (i = 0; i != sTAKick.ui32NumSrcSyncs; i++)
		sTAKick.apsSrcSyncInfo[i] = kick->apsSrcSurfSyncInfo[i];
	result->output.ui32NumPBPagesFree = psPBDesc->ui32TotalPages - 32;
	result->output.ui32NumPBPagesFree -=
			((SGX_HWPBDESC *)psHWPBDescMemInfo->pvLinAddrKM)->ui32AllocatedPages;
	eError = SubmitTAKick(device, &sTAKick);
	goto unlock;

retry:
	eError = PVRSRV_ERROR_RETRY;
unlock:
	sceKernelUnlockMutex(psRTDataSet->hMutex, 1);
	LinuxLockMutex(&gPVRSRVLock);
	return eError;
}
