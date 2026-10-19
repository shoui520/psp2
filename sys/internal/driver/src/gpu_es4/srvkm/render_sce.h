/* SCE kernel render-target state, recovered from the GPU owner's constructors. */
#ifndef RENDER_SCE_H
#define RENDER_SCE_H

#include <gpu/sce_ddk_port.h>
#include <gpu/memory_sce.h>
#include "transfer_sce.h"

struct PVRSRV_KERNEL_MEM_INFO;
struct SGXMKIF_HWRTDATASET;
struct SGX_RENDERDETAILS;
struct SGX_DEVICE_SYNC_LIST;
struct SGX_RENDERCONTEXT;
struct Resource;
struct SGX_CLIENTPBDESC;
struct GpuCommandRing;
struct SGXMKIF_HWRENDERCONTEXT;
struct PVRSRV_KERNEL_SYNC_INFO;
struct GpuMemoryContext;
struct SGXMKIF_HWRENDERDETAILS;
struct _SGXMKIF_HWDEVICE_SYNC_LIST_;

/* Complete host interface to the 108-byte firmware allocation. The opaque
 * words belong to firmware: host code initializes and publishes the whole
 * record, then unregisters it before freeing it. They are not padding and
 * do not describe the still-unimplemented firmware's internal state.
 */
struct SGXMKIF_HWRENDERCONTEXT {
	IMG_UINT32 ui32Flags;
	IMG_UINT32 ui32Priority;
	IMG_DEV_PHYADDR sPDDevPAddr;
	IMG_DEV_VIRTADDR sTACCBDevVAddr;
	IMG_DEV_VIRTADDR sTACCBControlDevVAddr;
	IMG_UINT32 firmwarePrivateWord;
	IMG_DEV_VIRTADDR sHWPBDescDevVAddr;
	IMG_UINT32 ui32MasterVDMSnapshotBufferAddress;
	IMG_UINT32 aui32VDMSnapshotBufferAddress[4];
	IMG_DEV_VIRTADDR asMTEStateFlushBufferDevVAddr[4];
	IMG_DEV_VIRTADDR sSABufferDevVAddr;
	IMG_UINT32 ui32SABufferSizeInDwords;
	IMG_UINT32 firmwarePrivateState[8];
	IMG_UINT32 ui32ProcessID;
};

_Static_assert(sizeof(struct SGXMKIF_HWRENDERCONTEXT) == 108,
		"hardware render context host interface");

/* Licensed IMG client owners: constructors and SGXKickTA in both TOOL builds.
 * The register block is copied to the hardware render details as 260 bytes.
 */
typedef struct SGX_RENDERREGISTERS {
	IMG_DEV_VIRTADDR asVisTestResultDevVAddr[4];
	IMG_UINT32 ui32ParameterBufferBase;
	IMG_UINT32 ui32ZLSBase;
	IMG_UINT32 ui32PixelBE;
	IMG_UINT32 ui32ISPIPFMisc;
	IMG_UINT32 ui323DAAMode;
	IMG_UINT32 ui32ISPDBias;
	IMG_BOOL bLargeRenderTarget;
	IMG_DEV_VIRTADDR asRgnHeaderDevVAddr[4];
	IMG_UINT32 ui32RgnHeaderAllocSize;
	IMG_UINT32 ui32MTileStride;
	IMG_UINT32 ui32TEMTile1;
	IMG_UINT32 ui32TEMTile2;
	IMG_UINT32 ui32DoublePixelPartitions;
	IMG_BOOL bColourAlphaValid;
	IMG_UINT32 ui32ISPPerpendicular;
	IMG_UINT32 ui32ISPCullValue;
	IMG_DEV_VIRTADDR asNextRgnHeaderDevVAddr[4];
	IMG_UINT32 ui32ZLSCtl;
	IMG_UINT32 ui32ISPZLoadBase;
	IMG_UINT32 ui32ISPZStoreBase;
	IMG_UINT32 ui32ISPStencilLoadBase;
	IMG_UINT32 ui32ISPStencilStoreBase;
	IMG_UINT32 ui32ISPBGObjDepth;
	IMG_UINT32 ui32ISPBGObj;
	IMG_UINT32 ui32BGObjTexPtr;
	IMG_UINT32 ui32ISPMultiSampleCtl2;
	IMG_UINT32 ui32ISPMultiSampleCtl;
	IMG_UINT32 ui32ISPRender;
	IMG_UINT32 ui32ISPRenderFlags;
	IMG_UINT32 ui32EDMPixelPDSAddr;
	IMG_UINT32 ui32EDMPixelPDSDataSize;
	IMG_UINT32 ui32EDMPixelPDSInfo;
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
	IMG_BOOL bDisablePixelSplit;
} SGX_RENDERREGISTERS;

typedef struct SGX_RENDERDETAILS {
	struct SGXMKIF_HWRENDERDETAILS *psHWRenderDetails;
	struct PVRSRV_KERNEL_MEM_INFO *psHWRenderDetailsMemInfo;
	struct PVRSRV_KERNEL_MEM_INFO *psLockMemInfo;
	volatile IMG_UINT32 *pui32Lock;
	SGX_RENDERREGISTERS sRegisters;
	struct SGX_RENDERDETAILS *psNext;
} SGX_RENDERDETAILS;

/* Complete firmware allocations as observed by every host consumer. The
 * firmware-state arrays are live opaque data, never padding or code images.
 */
struct SGXMKIF_HWRENDERDETAILS {
	SGX_RENDERREGISTERS sRegisters;
	IMG_DEV_VIRTADDR sLockDevVAddr;
	IMG_UINT32 firmwarePrivateWord;
	IMG_DEV_VIRTADDR sHWRTDataSetDevVAddr;
	IMG_DEV_VIRTADDR sHWRTDataDevVAddr;
	IMG_UINT32 firmwarePrivateState[22];
	IMG_UINT32 ui32PixelPartitions;
	IMG_UINT32 firmwarePrivateTail[44];
};

typedef struct SGXMKIF_HWRTDATA {
	IMG_DEV_VIRTADDR sHWRenderContextDevVAddr;
	IMG_UINT32 firmwarePrivateWord;
	IMG_UINT32 ui32LastActiveTile;
	IMG_UINT8 ui8NumActiveTiles;
	IMG_UINT8 firmwarePrivateTileState[3];
	IMG_UINT32 ui32TilesPerMacroTile;
	IMG_UINT32 ui32RgnHeaderMacroTileSize;
	IMG_DEV_VIRTADDR sTileRgnLUTDevVAddr;
	IMG_DEV_VIRTADDR asRgnHeaderDevVAddr[4];
	IMG_DEV_VIRTADDR asLastRgnHeaderDevVAddr[4];
	IMG_DEV_VIRTADDR sSpecialObjDevVAddr;
	IMG_DEV_VIRTADDR sContextStateDevVAddr;
	IMG_DEV_VIRTADDR sPIMTableDevVAddr;
	IMG_DEV_VIRTADDR asContextControlDevVAddr[4];
	IMG_DEV_VIRTADDR asContextOTPMDevVAddr[4];
	IMG_DEV_VIRTADDR asTailPtrsDevVAddr[4];
	IMG_UINT32 ui32TailPtrsSize;
	IMG_DEV_VIRTADDR sRTStatusDevVAddr;
	volatile IMG_UINT32 ui32RTStatus;
} SGXMKIF_HWRTDATA;

struct SGXMKIF_HWRTDATASET {
	IMG_DEV_VIRTADDR sPendingCountDevVAddr;
	IMG_UINT32 firmwarePrivateWord;
	IMG_UINT32 ui32TileWidth;
	IMG_UINT32 firmwarePrivateState[2];
	IMG_UINT32 ui32NumRTData;
	SGXMKIF_HWRTDATA asRTData[];
};

typedef struct SGX_BGOBJ_VERTEX {
	IMG_UINT32 ui32PackedPosition;
	float fDepth;
} SGX_BGOBJ_VERTEX;

typedef struct SGX_BGOBJ {
	IMG_UINT32 firmwareHeader[3];
	IMG_UINT32 aui32TextureCoordinates[3][2];
	IMG_UINT32 ui32PrimitiveControl;
	IMG_UINT32 firmwareControl;
	SGX_BGOBJ_VERTEX asVertices[3];
	IMG_UINT32 firmwareStorage[111];
} SGX_BGOBJ;

_Static_assert(sizeof(SGX_RENDERREGISTERS) == 260, "render register payload");
_Static_assert(sizeof(struct SGXMKIF_HWRENDERDETAILS) == 544, "hardware render details");
_Static_assert(sizeof(SGXMKIF_HWRTDATA) == 132, "hardware render target");
_Static_assert(sizeof(struct SGXMKIF_HWRTDATASET) == 24, "hardware render target header");
_Static_assert(sizeof(SGX_BGOBJ) == 512, "background object allocation");

typedef struct SGX_DEVICE_SYNC_LIST {
	struct _SGXMKIF_HWDEVICE_SYNC_LIST_ *psHWDeviceSyncList;
	struct PVRSRV_KERNEL_MEM_INFO *psDeviceSyncListMemInfo;
	struct PVRSRV_KERNEL_MEM_INFO *psHWDeviceSyncListMemInfo;
	struct PVRSRV_KERNEL_MEM_INFO *psLockMemInfo;
	volatile IMG_UINT32 *pui32Lock;
	struct SGX_DEVICE_SYNC_LIST *psNext;
	IMG_UINT32 ui32NumSyncObjects;
	struct PVRSRV_KERNEL_SYNC_INFO *apsSyncHandles[2];
} SGX_DEVICE_SYNC_LIST;

typedef enum SGX_SCALING {
	SGX_SCALING_NONE = 0,
	SGX_DOWNSCALING,
	SGX_UPSCALING,
	SGX_SCALING_FORCE_I32 = 0x7fffffff
} SGX_SCALING;

typedef struct SGX_ADDRENDTARG {
	IMG_UINT32 ui32Flags;
	IMG_UINT32 ui32NumRTData;
	IMG_UINT32 ui32MaxQueuedRenders;
	union {
		IMG_SID hRenderContext;
		struct SGX_RENDERCONTEXT *renderContext;
	} context;
	IMG_SID hDevCookie;
	IMG_UINT32 ui32NumPixelsX;
	IMG_UINT32 ui32NumPixelsY;
	IMG_UINT16 ui16MSAASamplesInX;
	IMG_UINT16 ui16MSAASamplesInY;
	SGX_SCALING eForceScalingInX;
	SGX_SCALING eForceScalingInY;
	IMG_UINT32 ui32BGObjUCoord;
	PVRSRV_ROTATION eRotation;
	IMG_UINT16 ui16NumRTsInArray;
	IMG_UINT8 ui8MacrotileCountX;
	IMG_UINT8 ui8MacrotileCountY;
	IMG_INT32 i32DataMemblockUID;
	IMG_BOOL bUseExternalUID;
	IMG_SID hMemBlockProcRef;
	IMG_UINT32 ui32MultisampleLocations;
} SGX_ADDRENDTARG;

typedef struct SGX_CREATERENDERCONTEXT {
	IMG_UINT32 ui32Flags;
	IMG_SID hDevCookie;
	union {
		IMG_SID hDevMemContext;
		struct GpuMemoryContext *memoryContext;
	} memory;
	IMG_UINT32 ui32PBSize;
	IMG_UINT32 ui32PBSizeLimit;
	IMG_UINT32 ui32VisTestResultBufferSize;
	IMG_UINT32 ui32MaxSACount;
	IMG_SID hMemBlockProcRef;
	IMG_BOOL bPerContextPB;
} SGX_CREATERENDERCONTEXT;

typedef struct SGX_RTDATA {
	struct PVRSRV_KERNEL_MEM_INFO *apsRgnHeaderMemInfo[4];
	struct PVRSRV_KERNEL_MEM_INFO *psContextStateMemInfo;
	struct PVRSRV_KERNEL_MEM_INFO *psTileRgnLUTMemInfo;
	IMG_UINT32 ui32BGObjU;
	IMG_UINT32 ui32BGObjV;
	IMG_UINT32 ui32BGObjPtr1;
	IMG_UINT32 ui32BGObjPtr2;
	IMG_UINT32 ui32BGObjTexPtr;
	IMG_UINT32 ui32BGObjObjPtr;
	IMG_UINT32 ui32BGObjBlendPtr;
	volatile IMG_UINT32 *pui32RTStatus;
} SGX_RTDATA;

typedef struct SGX_RTDATASET {
	int hMutex;
	struct Resource *mutexResource;
	int i32DataMemblockUID;
	IMG_BOOL bUseExternalUID;
	IMG_UINT32 ui32Flags;
	IMG_UINT32 ui32NumPixelsX;
	IMG_UINT32 ui32NumPixelsY;
	IMG_UINT32 ui323DAAMode;
	IMG_UINT32 ui32MTEMultiSampleCtl;
	IMG_UINT32 ui32ISPMultiSampleCtl;
	IMG_UINT32 ui32ISPMultiSampleCtl2;
	IMG_UINT16 ui16MSAASamplesInX;
	IMG_UINT16 ui16MSAASamplesInY;
	SGX_SCALING eScalingInX;
	SGX_SCALING eScalingInY;
	PVRSRV_ROTATION eRotation;
	IMG_UINT32 ui32BGObjUCoord;
	IMG_UINT32 ui32MTileNumber;
	IMG_UINT32 ui32MTileX1;
	IMG_UINT32 ui32MTileX2;
	IMG_UINT32 ui32MTileX3;
	IMG_UINT32 ui32MTileY1;
	IMG_UINT32 ui32MTileY2;
	IMG_UINT32 ui32MTileY3;
	IMG_UINT32 ui32ScreenXMax;
	IMG_UINT32 ui32ScreenYMax;
	IMG_UINT32 ui32MTileStride;
	IMG_UINT32 ui32RTDataSel;
	IMG_UINT32 ui32RefCount;
	IMG_UINT16 ui16NumRTsInArray;
	struct SGXMKIF_HWRTDATASET *psHWRTDataSet;
	IMG_UINT32 ui32NumRTData;
	SGX_RTDATA *psRTData;
	IMG_UINT32 ui32MaxQueuedRenders;
	struct PVRSRV_KERNEL_MEM_INFO *psHWRTDataSetMemInfo;
	struct PVRSRV_KERNEL_MEM_INFO *psPendingCountMemInfo;
	IMG_UINT32 *pui32PendingCount;
	struct PVRSRV_KERNEL_MEM_INFO *apsTailPtrsMemInfo[4];
	struct PVRSRV_KERNEL_MEM_INFO *psContextControlMemInfo[4];
	struct PVRSRV_KERNEL_MEM_INFO *psContextOTPMMemInfo[4];
	struct PVRSRV_KERNEL_MEM_INFO *psSpecialObjMemInfo;
	struct PVRSRV_KERNEL_MEM_INFO *psPIMTableMemInfo;
	struct SGX_RTDATASET *psNext;
	volatile IMG_UINT32 *pui32UserModeFlagsPtr;
	struct SGX_RENDERDETAILS *psRenderDetailsList;
	struct SGX_RENDERDETAILS *psCurrentRenderDetails;
	struct SGX_DEVICE_SYNC_LIST *psDeviceSyncList;
	struct SGX_DEVICE_SYNC_LIST *psCurrentDeviceSyncList;
	struct SGX_RENDERCONTEXT *psRenderContext;
	struct Resource *hResItem;
} SGX_RTDATASET;

/* SCE moves the client context into the kernel and adds resource ownership. */
struct SGX_RENDERCONTEXT {
	int hMutex;
	struct Resource *mutexResource;
	struct Resource *hResItem;
	struct SGX_CLIENTPBDESC *psClientPBDesc;
	SGX_RTDATASET *psRTDataSets;
	struct GpuCommandRing *psTACCB;
	struct SGXMKIF_HWRENDERCONTEXT *psHWRenderContext;
	struct PVRSRV_KERNEL_MEM_INFO *psHWRenderContextMemInfo;
	IMG_DEV_VIRTADDR sHWRenderContextDevVAddr;
	struct PVRSRV_KERNEL_SYNC_INFO *psTA3DSyncObject;
	struct PVRSRV_KERNEL_MEM_INFO *psMasterVDMSnapShotBufferMemInfo;
	struct PVRSRV_KERNEL_MEM_INFO *psVDMSnapShotBufferMemInfo[4];
	struct PVRSRV_KERNEL_MEM_INFO *psMTEStateFlushMemInfo[4];
	struct PVRSRV_KERNEL_MEM_INFO *psSABufferMemInfo;
	struct GpuMemoryContext *hDevMemContext;
	IMG_SID hTQContext;
	struct PVRSRV_KERNEL_MEM_INFO *psVisTestResultMemInfo;
	IMG_HANDLE hHWRenderContext;
	IMG_BOOL bForcePTOff;
	IMG_UINT32 ui32TAKickRegFlags;
	IMG_UINT32 waitGeneration;
	IMG_BOOL bTACommandQueued;
	IMG_UINT32 reserved;
	IMG_UINT32 ui32NumPixelPartitions;
	IMG_UINT32 ui32NumVertexPartitions;
	IMG_BOOL bPerContextPB;
	IMG_BOOL bCDRAMPB;
};

_Static_assert(sizeof(SGX_RTDATA) == 56, "SCE render data");
_Static_assert(sizeof(SGX_RENDERDETAILS) == 280, "IMG client render details");
_Static_assert(sizeof(SGX_DEVICE_SYNC_LIST) == 36, "IMG client device sync list");
_Static_assert(sizeof(SGX_ADDRENDTARG) == 68, "SCE render target request");
_Static_assert(sizeof(SGX_CREATERENDERCONTEXT) == 36, "SCE render context request");
_Static_assert(sizeof(SGX_RTDATASET) == 232, "SCE render target data set");
_Static_assert(sizeof(struct SGX_RENDERCONTEXT) == 132, "SCE render context");

/* client_render.c */
PVRSRV_ERROR DestroyRenderContext(struct SGX_RENDERCONTEXT *psRenderContext,
		IMG_UINT32 ui32Param, IMG_BOOL bForceCleanup);
PVRSRV_ERROR CreateRenderContext(PVRSRV_DEVICE_NODE *psDeviceNode,
		PVRSRV_PER_PROCESS_DATA *psPerProc, SGX_CREATERENDERCONTEXT *psCreate,
		IMG_SID *phRenderContext, PVRSRV_KERNEL_MEM_INFO **ppsVisTestMemInfo);
PVRSRV_ERROR RemoveRenderTarget(PVRSRV_DEVICE_NODE *psDeviceNode,
		struct SGX_RENDERCONTEXT *psRenderContext, SGX_RTDATASET *psRTDataSet);
PVRSRV_ERROR AddRenderTarget(PVRSRV_DEVICE_NODE *psDeviceNode,
		PVRSRV_PER_PROCESS_DATA *psPerProc, SGX_ADDRENDTARG *psAddRT,
		IMG_SID *phRenderTarget);
PVRSRV_ERROR DestroyCommandContext(GpuTransferQueueContext *context, IMG_BOOL bForceCleanup);
PVRSRV_ERROR CreateTransferQueue(PVRSRV_DEVICE_NODE *psDeviceNode,
		PVRSRV_PER_PROCESS_DATA *psPerProc, IMG_UINT32 reservedParameter,
		GpuTransferMemoryContext *psMemoryContext, IMG_SID *phContext);
PVRSRV_ERROR SubmitTransferCommands(IMG_UINT32 *commands, IMG_UINT32 numCommands,
		PVRSRV_DEVICE_NODE *psDeviceNode, PVRSRV_PER_PROCESS_DATA *psPerProc,
		GpuTransferQueueContext *context, IMG_VOID *syncInfo,
		IMG_BOOL sourceSyncEnable, IMG_BOOL destinationSyncEnable,
		GpuTransferStatus *status);
PVRSRV_ERROR WaitTransferQueueIdle(GpuTransferQueueContext *context);

#endif
