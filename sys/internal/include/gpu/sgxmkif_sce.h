/*
 * SceGpuEs4's SGX microkernel interface, as SceGpuEs4Init sets it up: the
 * init info it hands to DevInitSGXPart2KM, and the control blocks it
 * allocates for the microkernel.
 *
 * SCE's layouts differ from the IMG DDK 1.7 headers (sgxinfo.h,
 * sgx_mkif_km.h); field names follow the IMG ones where SceGpuEs4's
 * DevInitSGXPart2KM copies a field into a named PVRSRV_SGXDEV_INFO field.
 * Words only the microkernel reads are left reserved.
 */

#pragma once

#include <stddef.h>
#include <gpu/device_sce.h>
#include <gpu/memory_sce.h>

/* ui32PowerStatus (sgx_mkif_km.h) */
#define PVRSRV_USSE_EDM_POWMAN_NO_WORK		(1U << 5)

/* PVRSRV_SGXDEV_INFO.psSGXHostCtl, 0x84 bytes */
struct SGXMKIF_HOST_CTL {
	volatile IMG_UINT32 ui32InitStatus;
	volatile IMG_UINT32 ui32PowerStatus;
	volatile IMG_UINT32 ui32CleanupStatus;
	IMG_UINT32 ui32uKernelDetectedLockups;
	IMG_UINT32 ui32HostDetectedLockups;
	IMG_UINT32 ui32HWRecoverySampleRate;
	IMG_UINT32 ui32uKernelTimerClock;
	IMG_UINT32 ui32ActivePowManSampleRate;
	IMG_UINT32 ui32InterruptFlags;
	IMG_UINT32 ui32InterruptClearFlags;
	IMG_UINT32 ui32BPSetClearSignal;
	IMG_UINT32 ui32NumActivePowerEvents;
	IMG_UINT32 ui32TimeWraps;
	IMG_UINT32 ui32HostClock;
	IMG_UINT32 ui32AssertFail;
	IMG_UINT32 aui32PerfGroup[8];
	IMG_UINT32 aui32PerfBit[8];
	IMG_UINT32 ui32OpenCLDelayCount;
	IMG_UINT32 partialRenders;
};

_Static_assert(sizeof(struct SGXMKIF_HOST_CTL) == 132,
	"SGXMKIF_HOST_CTL");
_Static_assert(offsetof(struct SGXMKIF_HOST_CTL, partialRenders) == 128,
	"SGXMKIF_HOST_CTL.partialRenders");

/* the TA/3D control block, PVRSRV_SGXDEV_INFO.psKernelSGXTA3DCtlMemInfo */
typedef struct SGXMKIF_TA3D_CTL {
	IMG_DEV_VIRTADDR sTA3DCtlDevAddr;	/* its own address */
	IMG_DEV_VIRTADDR sHostCtlDevAddr;
	IMG_DEV_VIRTADDR sKernelCCBCtlDevAddr;
	IMG_UINT32 reserved0[2];
	IMG_UINT32 ui32Mode;			/* 2 */
	IMG_UINT32 reserved1[0x27];
	IMG_DEV_VIRTADDR sKernelCCBDevAddr;
	IMG_DEV_PHYADDR sPDDevPAddr;
	IMG_UINT32 reserved2[0x13];
	IMG_UINT32 renderState[4];		/* vertex/fragment frame, vertex/fragment scene */
	IMG_UINT32 reserved3[5];
	IMG_DEV_VIRTADDR sTASigBufferDevAddr;
	IMG_DEV_VIRTADDR s3DSigBufferDevAddr;
	IMG_UINT32 reserved4[0x10];
} SGXMKIF_TA3D_CTL;

/*
 * What SceGpuEs4Init hands to DevInitSGXPart2KM. The memory infos are
 * PVRSRV_KERNEL_MEM_INFO pointers (IMG_HANDLE in the IMG header).
 */
typedef struct SGX_BRIDGE_INIT_INFO {
	PVRSRV_KERNEL_MEM_INFO *hKernelCCBMemInfo;
	IMG_VOID *pvKernelCCB;
	PVRSRV_KERNEL_MEM_INFO *hKernelCCBEventKickerMemInfo;
	PVRSRV_KERNEL_MEM_INFO *hKernelCCBCtlMemInfo;
	IMG_VOID *pvKernelCCBCtl;
	PVRSRV_KERNEL_MEM_INFO *hKernelSGXTA3DCtlMemInfo;
	PVRSRV_KERNEL_MEM_INFO *hKernelSGXHostCtlMemInfo;
	PVRSRV_KERNEL_MEM_INFO *hKernelSGXMiscMemInfo;
	IMG_UINT32 ui32ClientBuildOptions;
	SGX_MISCINFO_STRUCT_SIZES sSGXStructSizes;
	/* FIX_HW_BRN_31542 */
	PVRSRV_KERNEL_MEM_INFO *hKernelClearClipWAVDMStreamMemInfo;
	PVRSRV_KERNEL_MEM_INFO *hKernelClearClipWAIndexStreamMemInfo;
	PVRSRV_KERNEL_MEM_INFO *hKernelClearClipWAPDSMemInfo;
	PVRSRV_KERNEL_MEM_INFO *hKernelClearClipWAUSEMemInfo;
	PVRSRV_KERNEL_MEM_INFO *hKernelClearClipWAParamMemInfo;
	PVRSRV_KERNEL_MEM_INFO *hKernelClearClipWAPMPTMemInfo;
	PVRSRV_KERNEL_MEM_INFO *hKernelClearClipWATPCMemInfo;
	PVRSRV_KERNEL_MEM_INFO *hKernelClearClipWAPSGRgnHdrMemInfo;
	PVRSRV_KERNEL_MEM_INFO *hKernelTASigBufferMemInfo;
	PVRSRV_KERNEL_MEM_INFO *hKernel3DSigBufferMemInfo;
	/* the microkernel programs, program0 to program7 (programs.h) */
	PVRSRV_KERNEL_MEM_INFO *hKernelProgramMemInfo[8];
	IMG_UINT32 aui32HostKickAddr[11];	/* by SGXMKIF_CMD_TYPE */
	/* copied to PVRSRV_SGXDEV_INFO.asSGXDevData[n] */
	IMG_UINT32 ui32DevData8;
	IMG_UINT32 ui32PixelShaderHeapBase;
	IMG_UINT32 ui32DevData6;
	IMG_UINT32 ui32DevData7;
	IMG_UINT32 ui32CacheControl;
	IMG_UINT32 reserved[6];
	IMG_UINT32 ui32DevData4;
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
} SGX_BRIDGE_INIT_INFO;

_Static_assert(sizeof(struct SGXMKIF_HOST_CTL) == 0x84, "SGXMKIF_HOST_CTL");
_Static_assert(sizeof(SGXMKIF_TA3D_CTL) == 0x174, "SGXMKIF_TA3D_CTL");
_Static_assert(offsetof(SGXMKIF_TA3D_CTL, sKernelCCBDevAddr) == 0xb4, "TA3D CCB");
_Static_assert(offsetof(SGXMKIF_TA3D_CTL, renderState) == 0x108, "TA3D render state");
_Static_assert(offsetof(SGXMKIF_TA3D_CTL, sTASigBufferDevAddr) == 0x12c, "TA3D TA signatures");
_Static_assert(offsetof(SGX_BRIDGE_INIT_INFO, hKernelClearClipWAVDMStreamMemInfo) == 0x60, "init clear clip");
_Static_assert(offsetof(SGX_BRIDGE_INIT_INFO, aui32HostKickAddr) == 0xa8, "init host kick");
_Static_assert(offsetof(SGX_BRIDGE_INIT_INFO, ui32CacheControl) == 0xe4, "init cache control");
_Static_assert(offsetof(SGX_BRIDGE_INIT_INFO, ui32DevData4) == 0x100, "init dev data 4");
_Static_assert(offsetof(SGX_BRIDGE_INIT_INFO, sScripts) == 0x12c, "init scripts");
_Static_assert(sizeof(SGX_BRIDGE_INIT_INFO) == 0x7ec, "SGX_BRIDGE_INIT_INFO");
