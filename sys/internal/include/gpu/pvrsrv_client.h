/*
 * The PowerVR services records SCE's GPU driver hands to its user libraries
 * (libgpu_es4, libgxm_es4), with the DDK's names.
 */

#pragma once

#include <psp2/types.h>
#include <gpu/pvrsrv_misc.h>

/*
 * A device memory allocation (PVRSRVAllocDeviceMem). SCE's DDK drops the CPU
 * physical address, sync and mapping fields of the IMG record.
 */
typedef struct PVRSRV_CLIENT_MEM_INFO {
	void *pvLinAddrKM;
	void *pvLinAddr;		/* the CPU's view; NULL without a CPU mapping */
	SceUInt32 sDevVAddr;		/* IMG_DEV_VIRTADDR: the GPU's view */
	SceUInt32 ui32Flags;
	SceUInt32 ui32ClientFlags;
	SceSize uAllocSize;
	SceUInt32 hKernelMemInfo;	/* IMG_SID */
} PVRSRV_CLIENT_MEM_INFO;

_Static_assert(sizeof(PVRSRV_CLIENT_MEM_INFO) == 0x1c, "PVRSRV_CLIENT_MEM_INFO size");

/* a device memory heap of a memory context (PVRSRVCreateDeviceMemContext) */
typedef struct PVRSRV_HEAP_INFO {
	SceUInt32 ui32HeapID;
	SceUInt32 hDevMemHeap;		/* IMG_SID */
	SceUInt32 sDevVAddrBase;	/* IMG_DEV_VIRTADDR */
	SceUInt32 ui32HeapByteSize;
	SceUInt32 ui32Attribs;
	SceUInt32 ui32XTileStride;
} PVRSRV_HEAP_INFO;

_Static_assert(sizeof(PVRSRV_HEAP_INFO) == 0x18, "PVRSRV_HEAP_INFO size");

/* SCE's render context request (SGXCreateRenderContext), as the driver takes it */
typedef struct SGX_CREATERENDERCONTEXT {
	SceUInt32 ui32Flags;
	SceUInt32 hDevCookie;		/* IMG_SID */
	SceUInt32 hDevMemContext;	/* IMG_SID */
	SceUInt32 ui32PBSize;
	SceUInt32 ui32PBSizeLimit;
	SceUInt32 ui32VisTestResultBufferSize;
	SceUInt32 ui32MaxSACount;
	SceUInt32 hMemBlockProcRef;	/* IMG_SID */
	SceUInt32 bPerContextPB;	/* IMG_BOOL */
} SGX_CREATERENDERCONTEXT;

_Static_assert(sizeof(SGX_CREATERENDERCONTEXT) == 36, "SGX_CREATERENDERCONTEXT size");

/* a device the services enumerate (PVRSRVEnumerateDevices) */
typedef struct PVRSRV_DEVICE_IDENTIFIER {
	SceUInt32 eDeviceType;		/* PVRSRV_DEVICE_TYPE */
	SceUInt32 eDeviceClass;		/* PVRSRV_DEVICE_CLASS */
	SceUInt32 ui32DeviceIndex;
	char *pszPDumpDevName;
	char *pszPDumpRegName;
} PVRSRV_DEVICE_IDENTIFIER;

/* SGXGetMiscInfo (sgxapi_km.h), with SCE's timer request */
typedef enum SGX_MISC_INFO_REQUEST {
	SGX_MISC_INFO_REQUEST_CLOCKSPEED,
	SGX_MISC_INFO_REQUEST_SGXREV,
	SGX_MISC_INFO_REQUEST_DRIVER_SGXREV,
	SGX_MISC_INFO_REQUEST_SET_HWPERF_STATUS,
	SGX_MISC_INFO_REQUEST_SET_BREAKPOINT,
	SGX_MISC_INFO_REQUEST_POLL_BREAKPOINT,
	SGX_MISC_INFO_REQUEST_RESUME_BREAKPOINT,
	SGX_MISC_INFO_DUMP_DEBUG_INFO,
	SGX_MISC_INFO_PANIC,
	SGX_MISC_INFO_REQUEST_SPM,		/* answered by the client itself */
	SGX_MISC_INFO_REQUEST_ACTIVEPOWER,
	SGX_MISC_INFO_REQUEST_LOCKUPS,
	SGX_MISC_INFO_REQUEST_TIMERS,		/* SCE */
	SGX_MISC_INFO_REQUEST_FORCE_I16 = 0x7fff
} SGX_MISC_INFO_REQUEST;

typedef struct PVRSRV_SGX_MISCINFO_FEATURES {
	SceUInt32 ui32CoreRev;
	SceUInt32 ui32CoreID;
	SceUInt32 ui32DDKVersion;
	SceUInt32 ui32DDKBuild;
	SceUInt32 ui32CoreIdSW;
	SceUInt32 ui32CoreRevSW;
	SceUInt32 ui32BuildOptions;
	SceUInt32 socRevision;		/* SCE */
} PVRSRV_SGX_MISCINFO_FEATURES;

typedef struct PVRSRV_SGX_MISCINFO_ACTIVEPOWER {
	SceUInt32 ui32NumActivePowerEvents;
} PVRSRV_SGX_MISCINFO_ACTIVEPOWER;

typedef struct PVRSRV_SGX_MISCINFO_LOCKUPS {
	SceUInt32 ui32HostDetectedLockups;
	SceUInt32 ui32uKernelDetectedLockups;
} PVRSRV_SGX_MISCINFO_LOCKUPS;

typedef struct PVRSRV_SGX_MISCINFO_SPM {
	void *hRTDataSet;		/* the client's render target */
	SceUInt32 ui32NumOutOfMemSignals;
	SceUInt32 ui32NumSPMRenders;
} PVRSRV_SGX_MISCINFO_SPM;

typedef struct SGX_BREAKPOINT_INFO {
	SceUInt32 bBPEnable;
	SceUInt32 ui32BPIndex;
	SceUInt32 ui32DataMasterMask;
	SceUInt32 sBPDevVAddr;
	SceUInt32 sBPDevVAddrEnd;
	SceUInt32 bTrapped;
	SceUInt32 bRead;
	SceUInt32 bWrite;
	SceUInt32 bTrappedBP;
	SceUInt32 ui32CoreNum;
	SceUInt32 sTrappedBPDevVAddr;
	SceUInt32 ui32TrappedBPBurstLength;
	SceUInt32 bTrappedBPRead;
	SceUInt32 ui32TrappedBPDataMaster;
	SceUInt32 ui32TrappedBPTag;
} SGX_BREAKPOINT_INFO;

typedef struct PVRSRV_SGX_MISCINFO_SET_HWPERF_STATUS {
	SceUInt32 ui32NewHWPerfStatus;
	SceUInt32 aui32PerfGroup[8];
	SceUInt32 aui32PerfBit[8];
} PVRSRV_SGX_MISCINFO_SET_HWPERF_STATUS;

typedef struct PVRSRV_SGX_MISCINFO_TIMERS {
	SceUInt64 ui64CPUTimer;
	SceUInt32 ui32GPUTimer;
} __attribute__((packed, aligned(4))) PVRSRV_SGX_MISCINFO_TIMERS;

typedef struct SGX_MISC_INFO {
	SGX_MISC_INFO_REQUEST eRequest;
	SceUInt32 ui32Padding;
	union {
		SceUInt32 reserved;
		PVRSRV_SGX_MISCINFO_FEATURES sSGXFeatures;
		SceUInt32 ui32SGXClockSpeed;
		PVRSRV_SGX_MISCINFO_ACTIVEPOWER sActivePower;
		PVRSRV_SGX_MISCINFO_LOCKUPS sLockups;
		PVRSRV_SGX_MISCINFO_SPM sSPM;
		SGX_BREAKPOINT_INFO sSGXBreakpointInfo;
		PVRSRV_SGX_MISCINFO_SET_HWPERF_STATUS sSetHWPerfStatus;
		PVRSRV_SGX_MISCINFO_TIMERS sTimers;
	} uData;
} SGX_MISC_INFO;

_Static_assert(sizeof(SGX_MISC_INFO) == 76, "SGX_MISC_INFO size");

/* SGXAddRenderTarget's request (SCE) */
typedef struct SGX_ADDRENDTARG {
	SceUInt32 ui32Flags;
	SceUInt32 ui32NumRTData;
	SceUInt32 ui32MaxQueuedRenders;
	SceUInt32 hRenderContext;	/* IMG_SID */
	SceUInt32 hDevCookie;		/* IMG_SID */
	SceUInt32 ui32NumPixelsX;
	SceUInt32 ui32NumPixelsY;
	SceUInt16 ui16MSAASamplesInX;
	SceUInt16 ui16MSAASamplesInY;
	SceUInt32 eForceScalingInX;	/* SGX_SCALING */
	SceUInt32 eForceScalingInY;
	SceUInt32 ui32BGObjUCoord;
	SceUInt32 eRotation;		/* PVRSRV_ROTATION */
	SceUInt16 ui16NumRTsInArray;
	SceUInt8 ui8MacrotileCountX;
	SceUInt8 ui8MacrotileCountY;
	SceInt32 i32DataMemblockUID;
	SceUInt32 bUseExternalUID;
	SceUInt32 hMemBlockProcRef;	/* IMG_SID */
	SceUInt32 ui32MultisampleLocations;
} SGX_ADDRENDTARG;

_Static_assert(sizeof(SGX_ADDRENDTARG) == 0x44, "SGX_ADDRENDTARG size");

/* SGXGetRenderTargetDetails' answer (SCE) */
typedef struct SGX_RENDER_TARGET_DETAILS {
	SceUInt32 ui32NumPixelsX;
	SceUInt32 ui32NumPixelsY;
	SceUInt16 ui16MSAASamplesInX;
	SceUInt16 ui16MSAASamplesInY;
	SceUInt32 ui32BGObjUCoord;
	SceUInt32 sSpecialObjDevVAddr;	/* IMG_DEV_VIRTADDR */
	SceUInt32 ui32SpecialObjSize;
	SceUInt32 ui32BGObjPtr1;
	SceUInt32 ui32NumRTData;
	SceUInt32 ui32MTEMultiSampleCtl;
	void *psRTDataSet;
	SceUInt32 ui32MTileNumber;
	SceUInt32 ui32MTileX1;
	SceUInt32 ui32MTileX2;
	SceUInt32 ui32MTileX3;
	SceUInt32 ui32MTileY1;
	SceUInt32 ui32MTileY2;
	SceUInt32 ui32MTileY3;
	SceUInt32 ui32MTileStride;
	SceUInt32 ui32Flags;
} SGX_RENDER_TARGET_DETAILS;

_Static_assert(sizeof(SGX_RENDER_TARGET_DETAILS) == 0x4c, "SGX_RENDER_TARGET_DETAILS size");

/* SGXGetRenderTargetInfo's answer (SCE) */
typedef struct SGX_RENDER_TARGET_INFO {
	SceUInt32 ui32NumPixelsX;
	SceUInt32 ui32NumPixelsY;
	SceUInt16 ui16MSAASamplesInX;
	SceUInt16 ui16MSAASamplesInY;
	SceUInt32 bDPMZLS;
} SGX_RENDER_TARGET_INFO;

/* a status word the GPU writes when a transfer or scene completes (SCE) */
typedef struct GpuTransferStatus {
	SceUInt32 statusAddress;	/* IMG_DEV_VIRTADDR */
	SceUInt32 statusValue;
	SceUInt32 memoryHandle;		/* IMG_SID */
} GpuTransferStatus;

/* the memory context of a transfer queue (SCE) */
typedef struct GpuTransferMemoryContext {
	SceUInt32 hMemoryContext;	/* IMG_SID */
	SceUInt32 memoryFlags;
} GpuTransferMemoryContext;

/* SGXDoKick's request: SCE's render submit, with the IMG register vocabulary */
typedef struct SGX_KICKTA_COMMON {
	SceUInt32 ui32Frame;
	SceUInt32 ui32KickFlags;
	SceUInt32 sVisTestResultMem;	/* IMG_DEV_VIRTADDR */
	SceUInt32 ui32VisTestCount;
	SceUInt32 ui32Scene;
	SceUInt32 ui32SceneFlags;
	SceUInt32 ui32TAScreenXMax;
	SceUInt32 ui32TAScreenYMax;
	SceUInt16 ui16MaxDrawCallsPerCore;
	SceUInt16 ui16PrimitiveSplitThreshold;
	SceUInt32 ui32ISPBGObjDepth;
	SceUInt32 ui32ISPBGObj;
	SceUInt32 ui32ISPIPFMisc;
	SceUInt32 ui32ISPPerpendicular;
	SceUInt32 ui32ISPCullValue;
	SceUInt32 ui32DoublePixelPartitions;
	SceUInt32 ui32TriangleSplitPixelThreshold;
	SceUInt32 bPerQuadrantPixelSplit;
	SceUInt32 sISPZLoadBase;
	SceUInt32 sISPStencilLoadBase;
	SceUInt32 sISPZStoreBase;
	SceUInt32 sISPStencilStoreBase;
	SceUInt32 bSeperateStencilLoad;
	SceUInt32 bSeperateStencilStore;
	SceUInt32 ui32ZLSCtl;
	SceUInt32 ui32ISPValidId;
	SceUInt32 ui32ISPDBias;
	SceUInt32 ui32ISPSceneBGObj;
	SceUInt32 ui32EDMPixelPDSAddr;
	SceUInt32 ui32EDMPixelPDSDataSize;
	SceUInt32 ui32EDMPixelPDSInfo;
	SceUInt32 ui32PixelBE;
	SceUInt32 ui32CSC0Coeff01;
	SceUInt32 ui32CSC0Coeff23;
	SceUInt32 ui32CSC0Coeff45;
	SceUInt32 ui32CSC0Coeff67;
	SceUInt32 ui32CSC1Coeff01;
	SceUInt32 ui32CSC1Coeff23;
	SceUInt32 ui32CSC1Coeff45;
	SceUInt32 ui32CSC1Coeff67;
	SceUInt32 ui32CSC0Coeff89;
	SceUInt32 ui32CSC1Coeff89;
	SceUInt32 ui32CSC0Coeff1011;
	SceUInt32 ui32CSC1Coeff1011;
	SceUInt32 ui32Filter0Left;
	SceUInt32 ui32Filter0Right;
	SceUInt32 ui32Filter0Extra;
	SceUInt32 ui32Filter1Left;
	SceUInt32 ui32Filter1Right;
	SceUInt32 ui32Filter1Extra;
	SceUInt32 ui32Filter2Left;
	SceUInt32 ui32Filter2Right;
	SceUInt32 ui32Filter2Extra;
	SceUInt32 ui32FilterTable;
	SceUInt32 ui32USEStoreRange;
	SceUInt32 ui32USELoadRedirect;
	SceUInt32 sTABufferCtlStreamBase;
	SceUInt32 sTABufferCtlStreamTerminate;
	SceUInt32 ui32MTECtrl;
	SceUInt32 aui32SpecObject[3];
	SceUInt32 ui32BGObjWidth;
	SceUInt32 ui32BGObjHeight;
	SceUInt32 ui32NumTAStatusVals;
	SceUInt32 ui32Num3DStatusVals;
	SceUInt32 ui32NumSrcSyncs;
} SGX_KICKTA_COMMON;

typedef struct SGX_KICKTA {
	SGX_KICKTA_COMMON sKickTACommon;
	SceUInt32 *pahRenderSurfSyncInfo;	/* IMG_SIDs */
	SceUInt32 sPixEventUpdateDevAddr;	/* IMG_DEV_VIRTADDR */
	SceUInt32 *psPixEventUpdateList;
	SceUInt32 *psHWBgObjPrimPDSUpdateList;
	SceUInt32 ahSrcSurfSyncInfo[8];		/* IMG_SIDs */
	GpuTransferStatus asTAStatusUpdate[32];
	GpuTransferStatus as3DStatusUpdate[4];
	SceUInt32 hTQContext;			/* IMG_SID */
	SceUInt32 hRenderContext;		/* IMG_SID */
	SceUInt32 hRTDataSet;			/* IMG_SID */
} SGX_KICKTA;

_Static_assert(sizeof(SGX_KICKTA) == 0x2f4, "SGX_KICKTA size");

typedef struct SGX_KICKTA_OUTPUT {
	SceUInt32 bSPMOutOfMemEvent;
	SceUInt32 ui32NumPBPagesFree;
	SceUInt32 ui32MaxZTileX;
	SceUInt32 ui32MaxZTileY;
	void *psZLSReg;
	SceUInt32 *pui32VDMBaseRegVal;
} SGX_KICKTA_OUTPUT;
