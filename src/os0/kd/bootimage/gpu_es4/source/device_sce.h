/*
 * IMG DDK device definitions adapted for the SCE ARM port.
 * Copyright (C) Imagination Technologies Ltd. GPL-2.0; see the pristine
 * img-sgx-ddk-1.7.862890 vendor tree and its COPYING file.
 * SCE removes the generic MMU callbacks and simplifies device-memory state.
 */
#ifndef DEVICE_SCE_H
#define DEVICE_SCE_H

#include "sce_ddk_port.h"
#include <sce/pvrsrv_misc.h>
#include "../../../../../vendor/img-sgx-ddk-1.7.862890/include4/sgxscript.h"

struct ResourceContext;
struct RA_ARENA;
struct GpuMemoryContext;
struct _SYS_DATA_TAG_;
struct PVRSRV_KERNEL_MEM_INFO;
struct PVRSRV_STUB_PBDESC;
struct PVRSRV_SGX_KERNEL_CCB;
struct _SGXMKIF_COMMAND_;
/* Complete pristine command-ring owner; PDUMP is disabled in the SCE port. */
typedef struct PVRSRV_SGX_CCB_INFO {
	struct PVRSRV_KERNEL_MEM_INFO *psCCBMemInfo;
	struct PVRSRV_KERNEL_MEM_INFO *psCCBCtlMemInfo;
	struct _SGXMKIF_COMMAND_ *psCommands;
	IMG_UINT32 *pui32WriteOffset;
	volatile IMG_UINT32 *pui32ReadOffset;
} PVRSRV_SGX_CCB_INFO;
struct PVRSRV_SGX_CCB_CTL;
struct SGXMKIF_HOST_CTL;

typedef enum PVRSRV_DEVICE_TYPE {
	PVRSRV_DEVICE_TYPE_UNKNOWN = 0,
	PVRSRV_DEVICE_TYPE_MBX1 = 1,
	PVRSRV_DEVICE_TYPE_MBX1_LITE = 2,
	PVRSRV_DEVICE_TYPE_M24VA = 3,
	PVRSRV_DEVICE_TYPE_MVDA2 = 4,
	PVRSRV_DEVICE_TYPE_MVED1 = 5,
	PVRSRV_DEVICE_TYPE_MSVDX = 6,
	PVRSRV_DEVICE_TYPE_SGX = 7,
	PVRSRV_DEVICE_TYPE_VGX = 8,
	/* SCE native enumeration excludes EXT = 9; TOPAZ is not in this port. */
	PVRSRV_DEVICE_TYPE_EXT = 9,
	PVRSRV_DEVICE_TYPE_LAST = 9,
	PVRSRV_DEVICE_TYPE_FORCE_I32 = 0x7fffffff
} PVRSRV_DEVICE_TYPE;

typedef struct PVRSRV_DEVICE_IDENTIFIER {
	PVRSRV_DEVICE_TYPE eDeviceType;
	PVRSRV_DEVICE_CLASS eDeviceClass;
	IMG_UINT32 ui32DeviceIndex;
	IMG_CHAR *pszPDumpDevName;
	IMG_CHAR *pszPDumpRegName;
} PVRSRV_DEVICE_IDENTIFIER;

typedef struct DEVICE_MEMORY_HEAP_INFO {
	IMG_UINT32 ui32HeapID;
	IMG_CHAR *pszName;
	IMG_CHAR *pszBSName;
	IMG_DEV_VIRTADDR sDevVAddrBase;
	IMG_UINT32 ui32HeapSize;
	IMG_UINT32 ui32Attribs;
	IMG_UINT32 DevMemHeapType;
	IMG_HANDLE hDevMemHeap;
	/* SCE drops the IMG arena pointer, retaining page and tiling controls. */
	IMG_UINT32 ui32DataPageSize;
	IMG_UINT32 ui32XTileStride;
} DEVICE_MEMORY_HEAP_INFO;

typedef struct DEVICE_MEMORY_INFO {
	IMG_UINT32 ui32AddressSpaceSizeLog2;
	IMG_UINT32 ui32HeapCount;
	DEVICE_MEMORY_HEAP_INFO *psDeviceMemoryHeap;
	struct GpuMemoryContext *pBMKernelContext;
} DEVICE_MEMORY_INFO;

/* Pristine services.h heap-query output, with the SCE native pointer handle. */
typedef struct PVRSRV_HEAP_INFO {
	IMG_UINT32 ui32HeapID;
	IMG_HANDLE hDevMemHeap;
	IMG_DEV_VIRTADDR sDevVAddrBase;
	IMG_UINT32 ui32HeapByteSize;
	IMG_UINT32 ui32Attribs;
	IMG_UINT32 ui32XTileStride;
} PVRSRV_HEAP_INFO;

typedef struct PVRSRV_DEVICE_NODE {
	PVRSRV_DEVICE_IDENTIFIER sDevId;
	IMG_UINT32 ui32RefCount;
	PVRSRV_ERROR (*pfnInitDevice)(IMG_VOID *);
	PVRSRV_ERROR (*pfnDeInitDevice)(IMG_VOID *);
	PVRSRV_ERROR (*pfnInitDeviceCompatCheck)(struct PVRSRV_DEVICE_NODE *);
	IMG_UINT32 reserved;
	IMG_BOOL (*pfnDeviceISR)(IMG_VOID *);
	IMG_VOID *pvISRData;
	IMG_UINT32 ui32SOCInterruptBit;
	IMG_VOID (*pfnDeviceMISR)(IMG_VOID *);
	IMG_VOID (*pfnDeviceCommandComplete)(struct PVRSRV_DEVICE_NODE *);
	IMG_BOOL bReProcessDeviceCommandComplete;
	IMG_VOID (*pfnCacheInvalidate)(struct PVRSRV_DEVICE_NODE *);
	DEVICE_MEMORY_INFO sDevMemoryInfo;
	IMG_VOID *pvDevice;
	IMG_UINT32 ui32pvDeviceSize;
	struct ResourceContext *hResManContext;
	struct _SYS_DATA_TAG_ *psSysData;
	struct RA_ARENA *psLocalDevMemArena;
	struct PVRSRV_DEVICE_NODE *psNext;
	struct PVRSRV_DEVICE_NODE **ppsThis;
} PVRSRV_DEVICE_NODE;

/* SCE retains the two 2D-command size entries and the MP clock controls. */
typedef struct SGX_MISCINFO_STRUCT_SIZES {
	IMG_UINT32 ui32Sizeof_2DCMD;
	IMG_UINT32 ui32Sizeof_2DCMD_SHARED;
	IMG_UINT32 ui32Sizeof_CMDTA;
	IMG_UINT32 ui32Sizeof_CMDTA_SHARED;
	IMG_UINT32 ui32Sizeof_TRANSFERCMD;
	IMG_UINT32 ui32Sizeof_TRANSFERCMD_SHARED;
	IMG_UINT32 ui32Sizeof_3DREGISTERS;
	IMG_UINT32 ui32Sizeof_HWPBDESC;
	IMG_UINT32 ui32Sizeof_HWRENDERCONTEXT;
	IMG_UINT32 ui32Sizeof_HWRENDERDETAILS;
	IMG_UINT32 ui32Sizeof_HWRTDATA;
	IMG_UINT32 ui32Sizeof_HWRTDATASET;
	IMG_UINT32 ui32Sizeof_HWTRANSFERCONTEXT;
	IMG_UINT32 ui32Sizeof_HOST_CTL;
	IMG_UINT32 ui32Sizeof_COMMAND;
} SGX_MISCINFO_STRUCT_SIZES;

typedef struct SGX_INTERNAL_DEVINFO {
	IMG_UINT32 ui32Flags;
	union {
		IMG_SID hHostCtlKernelMemInfoHandle;
		struct PVRSRV_KERNEL_MEM_INFO *memoryInfo;
	} hostControl;
	IMG_BOOL bForcePTOff;
} SGX_INTERNAL_DEVINFO;

typedef struct PVRSRV_SGXDEV_INFO {
	PVRSRV_DEVICE_TYPE eDeviceType;
	PVRSRV_DEVICE_CLASS eDeviceClass;
	IMG_UINT8 ui8VersionMajor;
	IMG_UINT8 ui8VersionMinor;
	IMG_UINT32 ui32CoreConfig;
	IMG_UINT32 ui32CoreFlags;
	IMG_VOID *pvRegsBaseKM;
	IMG_HANDLE hRegMapping;
	IMG_SYS_PHYADDR sRegsPhysBase;
	IMG_UINT32 ui32RegSize;
	IMG_UINT32 ui32CoreClockSpeed;
	IMG_UINT32 ui32uKernelTimerClock;
	struct PVRSRV_STUB_PBDESC *psStubPBDescListKM;
	IMG_DEV_PHYADDR sKernelPDDevPAddr;
	IMG_VOID *pvDeviceMemoryHeap;
	struct PVRSRV_KERNEL_MEM_INFO *psKernelCCBMemInfo;
	struct PVRSRV_SGX_KERNEL_CCB *psKernelCCB;
	struct PVRSRV_SGX_CCB_INFO *psKernelCCBInfo;
	struct PVRSRV_KERNEL_MEM_INFO *psKernelCCBCtlMemInfo;
	struct PVRSRV_SGX_CCB_CTL *psKernelCCBCtl;
	struct PVRSRV_KERNEL_MEM_INFO *psKernelCCBEventKickerMemInfo;
	IMG_UINT32 *pui32KernelCCBEventKicker;
	struct PVRSRV_KERNEL_MEM_INFO *psKernelSGXMiscMemInfo;
	IMG_UINT32 aui32HostKickAddr[11];
	struct PVRSRV_KERNEL_MEM_INFO *psKernelHWPerfCBMemInfo;
	struct PVRSRV_KERNEL_MEM_INFO *psKernelTASigBufferMemInfo;
	struct PVRSRV_KERNEL_MEM_INFO *psKernel3DSigBufferMemInfo;
	IMG_UINT32 ui32ClientRefCount;
	IMG_UINT32 ui32CacheControl;
	IMG_UINT32 ui32ClientBuildOptions;
	SGX_MISCINFO_STRUCT_SIZES sSGXStructSizes;
	IMG_VOID *pvMMUContextList;
	IMG_BOOL bForcePTOff;
	IMG_UINT32 ui32EDMTaskReg0;
	IMG_UINT32 ui32EDMTaskReg1;
	IMG_UINT32 ui32ClkGateCtl;
	IMG_UINT32 ui32ClkGateCtl2;
	IMG_UINT32 ui32ClkGateStatusReg;
	IMG_UINT32 ui32ClkGateStatusMask;
	IMG_UINT32 ui32MasterClkGateStatusReg;
	IMG_UINT32 ui32MasterClkGateStatusMask;
	IMG_UINT32 ui32MasterClkGateStatus2Reg;
	IMG_UINT32 ui32MasterClkGateStatus2Mask;
	SGX_INIT_SCRIPTS sScripts;
	IMG_HANDLE hBIFResetPDOSMemHandle;
	IMG_DEV_PHYADDR sBIFResetPDDevPAddr;
	IMG_DEV_PHYADDR sBIFResetPTDevPAddr;
	IMG_DEV_PHYADDR sBIFResetPageDevPAddr;
	IMG_UINT32 *pui32BIFResetPD;
	IMG_UINT32 *pui32BIFResetPT;
	IMG_HANDLE hTimer;
	IMG_UINT32 ui32TimeStamp;
	IMG_UINT32 ui32NumResets;
	struct PVRSRV_KERNEL_MEM_INFO *psKernelSGXHostCtlMemInfo;
	struct SGXMKIF_HOST_CTL *psSGXHostCtl;
	struct PVRSRV_KERNEL_MEM_INFO *psKernelSGXTA3DCtlMemInfo;
	IMG_UINT32 ui32Flags;
	IMG_UINT32 ui32MemTilingUsage;
	IMG_UINT32 asSGXDevData[24];
} PVRSRV_SGXDEV_INFO;

_Static_assert(sizeof(DEVICE_MEMORY_HEAP_INFO) == 40, "SCE device heap");
_Static_assert(sizeof(PVRSRV_DEVICE_NODE) == 112, "SCE device node");
_Static_assert(sizeof(PVRSRV_SGXDEV_INFO) == 2144, "SCE SGX device info");
_Static_assert(sizeof(SGX_CLIENT_INFO) == 256, "SCE SGX client info");

#endif
