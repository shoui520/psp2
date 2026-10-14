/* SCE render-submit packet, replacing the upstream CCB-kick bridge packet.
 * IMG register vocabulary is retained; Sony adds scene and split controls.
 */
#ifndef KICK_SCE_H
#define KICK_SCE_H

#include "render_sce.h"
#include "transfer_sce.h"
#include "sgxkick_sce.h"

struct PVRSRV_HWREG;

typedef struct SGX_KICKTA_COMMON {
	IMG_UINT32 ui32Frame;
	IMG_UINT32 ui32KickFlags;
	IMG_DEV_VIRTADDR sVisTestResultMem;
	IMG_UINT32 ui32VisTestCount;
	IMG_UINT32 ui32Scene;
	IMG_UINT32 ui32SceneFlags;
	IMG_UINT32 ui32TAScreenXMax;
	IMG_UINT32 ui32TAScreenYMax;
	IMG_UINT16 ui16MaxDrawCallsPerCore;
	IMG_UINT16 ui16PrimitiveSplitThreshold;
	IMG_UINT32 ui32ISPBGObjDepth;
	IMG_UINT32 ui32ISPBGObj;
	IMG_UINT32 ui32ISPIPFMisc;
	IMG_UINT32 ui32ISPPerpendicular;
	IMG_UINT32 ui32ISPCullValue;
	IMG_UINT32 ui32DoublePixelPartitions;
	IMG_UINT32 ui32TriangleSplitPixelThreshold;
	IMG_BOOL bPerQuadrantPixelSplit;
	IMG_DEV_VIRTADDR sISPZLoadBase;
	IMG_DEV_VIRTADDR sISPStencilLoadBase;
	IMG_DEV_VIRTADDR sISPZStoreBase;
	IMG_DEV_VIRTADDR sISPStencilStoreBase;
	IMG_BOOL bSeperateStencilLoad;
	IMG_BOOL bSeperateStencilStore;
	IMG_UINT32 ui32ZLSCtl;
	IMG_UINT32 ui32ISPValidId;
	IMG_UINT32 ui32ISPDBias;
	IMG_UINT32 ui32ISPSceneBGObj;
	IMG_UINT32 ui32EDMPixelPDSAddr;
	IMG_UINT32 ui32EDMPixelPDSDataSize;
	IMG_UINT32 ui32EDMPixelPDSInfo;
	IMG_UINT32 ui32PixelBE;
	IMG_UINT32 ui32CSC0Coeff01;
	IMG_UINT32 ui32CSC0Coeff23;
	IMG_UINT32 ui32CSC0Coeff45;
	IMG_UINT32 ui32CSC0Coeff67;
	IMG_UINT32 ui32CSC1Coeff01;
	IMG_UINT32 ui32CSC1Coeff23;
	IMG_UINT32 ui32CSC1Coeff45;
	IMG_UINT32 ui32CSC1Coeff67;
	IMG_UINT32 ui32CSC0Coeff89;
	IMG_UINT32 ui32CSC1Coeff89;
	IMG_UINT32 ui32CSC0Coeff1011;
	IMG_UINT32 ui32CSC1Coeff1011;
	IMG_UINT32 ui32Filter0Left;
	IMG_UINT32 ui32Filter0Right;
	IMG_UINT32 ui32Filter0Extra;
	IMG_UINT32 ui32Filter1Left;
	IMG_UINT32 ui32Filter1Right;
	IMG_UINT32 ui32Filter1Extra;
	IMG_UINT32 ui32Filter2Left;
	IMG_UINT32 ui32Filter2Right;
	IMG_UINT32 ui32Filter2Extra;
	IMG_UINT32 ui32FilterTable;
	IMG_UINT32 ui32USEStoreRange;
	IMG_UINT32 ui32USELoadRedirect;
	IMG_DEV_VIRTADDR sTABufferCtlStreamBase;
	IMG_DEV_VIRTADDR sTABufferCtlStreamTerminate;
	IMG_UINT32 ui32MTECtrl;
	IMG_UINT32 aui32SpecObject[3];
	IMG_UINT32 ui32BGObjWidth;
	IMG_UINT32 ui32BGObjHeight;
	IMG_UINT32 ui32NumTAStatusVals;
	IMG_UINT32 ui32Num3DStatusVals;
	IMG_UINT32 ui32NumSrcSyncs;
} SGX_KICKTA_COMMON;

typedef struct SGX_KICKTA {
	SGX_KICKTA_COMMON sKickTACommon;
	union {
		IMG_SID *pahRenderSurfSyncInfo;
		struct PVRSRV_KERNEL_SYNC_INFO **ppsRenderSurfSyncInfo;
	} renderSync;
	IMG_DEV_VIRTADDR sPixEventUpdateDevAddr;
	IMG_UINT32 *psPixEventUpdateList;
	IMG_UINT32 *psHWBgObjPrimPDSUpdateList;
	GpuSyncReference apsSrcSurfSyncInfo[8];
	GpuTransferStatus asTAStatusUpdate[32];
	GpuTransferStatus as3DStatusUpdate[4];
	union {
		IMG_SID hTQContext;
		GpuTransferQueueContext *transferQueue;
	} transfer;
	union {
		IMG_SID hRenderContext;
		struct SGX_RENDERCONTEXT *renderContext;
	} context;
	union {
		IMG_SID hRTDataSet;
		SGX_RTDATASET *renderTarget;
	} target;
} SGX_KICKTA;

/* Complete host command interface. Uninterpreted firmware words are live
 * command data; the host clears/copies them and never decodes their state.
 */
typedef struct SGXMKIF_TAREGISTERS {
	IMG_UINT32 ui32AAMode;
	IMG_UINT32 ui32TEMTile1;
	IMG_UINT32 ui32TEMTile2;
	IMG_UINT32 ui32TEScreen;
	IMG_UINT32 ui32MTileStride;
	IMG_UINT32 ui32TECtrl;
	IMG_DEV_VIRTADDR sTABufferCtlStreamBase;
	IMG_UINT32 ui32MTECtrl;
	IMG_DEV_VIRTADDR asRgnHeaderDevVAddr[4];
	IMG_DEV_VIRTADDR asTailPtrsDevVAddr[4];
	IMG_UINT32 ui32MultiSampleLocationsX;
	IMG_UINT32 ui32MultiSampleLocationsY;
	IMG_UINT32 ui32ScreenPixels;
	IMG_UINT32 ui32USELoadRedirect;
	IMG_UINT32 ui32USEStoreRange;
	IMG_UINT32 ui32ParameterBufferBase;
	IMG_BOOL bLargeRenderTarget;
	IMG_UINT32 ui323DAAMode;
	IMG_UINT32 ui32PrimitiveSplitThreshold;
} SGXMKIF_TAREGISTERS;

typedef struct SGX_REGISTER_PATCH {
	IMG_DEV_VIRTADDR sAddress;
	IMG_UINT32 ui32Value;
} SGX_REGISTER_PATCH;

typedef struct SGXMKIF_CMDTA {
	IMG_UINT32 ui32CommandSize;
	IMG_UINT32 ui32Flags;
	IMG_UINT32 aui32SpecObject[3];
	IMG_UINT32 ui32BackgroundSize;
	IMG_UINT32 ui32VertexPartitions;
	IMG_UINT32 ui32PrimitiveControl;
	IMG_UINT32 ui32TAControl;
	IMG_UINT32 ui32Frame;
	IMG_UINT32 ui32Scene;
	IMG_DEV_VIRTADDR sHWPBDescDevVAddr;
	IMG_UINT32 firmwarePrivateState[4];
	IMG_DEV_VIRTADDR sHWRenderDetailsDevVAddr;
	IMG_DEV_VIRTADDR sHWDeviceSyncListDevVAddr;
	IMG_DEV_VIRTADDR sHWRTDataSetDevVAddr;
	IMG_DEV_VIRTADDR sHWRTDataDevVAddr;
	IMG_UINT32 ui32VisTestCount;
	IMG_UINT32 ui32NumRegisterPatches;
	SGXMKIF_CMDTA_SHARED sShared;
	SGXMKIF_TAREGISTERS sRegisters;
	SGX_REGISTER_PATCH asRegisterPatches[1];
} SGXMKIF_CMDTA;

_Static_assert(sizeof(SGXMKIF_TAREGISTERS) == 100, "TA register interface");
_Static_assert(sizeof(SGXMKIF_CMDTA) == 676, "TA command host interface");

typedef struct SGX_KICKTA_OUTPUT {
	IMG_BOOL bSPMOutOfMemEvent;
	IMG_UINT32 ui32NumPBPagesFree;
	IMG_UINT32 ui32MaxZTileX;
	IMG_UINT32 ui32MaxZTileY;
	struct PVRSRV_HWREG *psZLSReg;
	IMG_UINT32 *pui32VDMBaseRegVal;
} SGX_KICKTA_OUTPUT;

/* SCE returns the pre-translation status records for later process waits. */
typedef struct SGX_KICKTA_RESULT {
	SGX_KICKTA_OUTPUT output;
	GpuTransferStatus destinationSyncs[4];
	GpuTransferStatus sourceSyncs[32];
} SGX_KICKTA_RESULT;

_Static_assert(sizeof(SGX_KICKTA_COMMON) == 264, "SCE common render packet");
_Static_assert(sizeof(SGX_KICKTA) == 756, "SCE render packet");
_Static_assert(sizeof(SGX_KICKTA_OUTPUT) == 24, "SCE render output");
_Static_assert(sizeof(SGX_KICKTA_RESULT) == 456, "SCE render result and waits");

#endif
