/* IMG sgxinfokm.h platform map under the Sony SGX543MP4 configuration.
 * GPL-2.0; pristine source and license remain in the vendor directory.
 */
#ifndef SGXINIT_SCE_H
#define SGXINIT_SCE_H

#include <gpu/device_sce.h>
#include <gpu/misc_sce.h>

/* Pristine sgxinfo.h owner with the SCE port's complete heap record. */
typedef struct _SGX_BRIDGE_INFO_FOR_SRVINIT {
	IMG_DEV_PHYADDR sPDDevPAddr;
	PVRSRV_HEAP_INFO asHeapInfo[32];
} SGX_BRIDGE_INFO_FOR_SRVINIT;

_Static_assert(sizeof(SGX_BRIDGE_INFO_FOR_SRVINIT) == 772,
		"SGX initialization heap information");

/* Complete pristine command packet and misc payload; SCE feature/size
 * extensions are defined once in misc_sce.h and device_sce.h respectively.
 */
typedef struct _SGXMKIF_COMMAND_ {
	IMG_UINT32 ui32ServiceAddress;
	IMG_UINT32 ui32CacheControl;
	IMG_UINT32 ui32Data[6];
} SGXMKIF_COMMAND;

/* Complete pristine command-ring owners (sgx_mkif_km.h). */
typedef struct PVRSRV_SGX_KERNEL_CCB {
	SGXMKIF_COMMAND asCommands[256];
} PVRSRV_SGX_KERNEL_CCB;

typedef struct PVRSRV_SGX_CCB_CTL {
	IMG_UINT32 ui32WriteOffset;
	IMG_UINT32 ui32ReadOffset;
} PVRSRV_SGX_CCB_CTL;

_Static_assert(sizeof(PVRSRV_SGX_KERNEL_CCB) == 8192, "SGX kernel CCB");
_Static_assert(sizeof(PVRSRV_SGX_CCB_CTL) == 8, "SGX kernel CCB control");

typedef struct PVRSRV_SGX_MISCINFO_INFO {
	IMG_UINT32 ui32MiscInfoFlags;
	PVRSRV_SGX_MISCINFO_FEATURES sSGXFeatures;
	SGX_MISCINFO_STRUCT_SIZES sSGXStructSizes;
} PVRSRV_SGX_MISCINFO_INFO;

_Static_assert(sizeof(SGXMKIF_COMMAND) == 32, "SGX command packet");
_Static_assert(sizeof(PVRSRV_SGX_MISCINFO_INFO) == 96, "SGX misc payload");

typedef struct _SGX_TIMING_INFORMATION_ {
	IMG_UINT32 ui32CoreClockSpeed;
	IMG_UINT32 ui32HWRecoveryFreq;
	IMG_BOOL bEnableActivePM;
	IMG_UINT32 ui32ActivePowManLatencyms;
	IMG_UINT32 ui32uKernelFreq;
} SGX_TIMING_INFORMATION;

typedef struct _SGX_DEVICE_MAP_ {
	IMG_UINT32 ui32Flags;
	IMG_SYS_PHYADDR sRegsSysPBase;
	IMG_CPU_PHYADDR sRegsCpuPBase;
	IMG_CPU_VIRTADDR pvRegsCpuVBase;
	IMG_UINT32 ui32RegsSize;
	IMG_SYS_PHYADDR sLocalMemSysPBase;
	IMG_DEV_PHYADDR sLocalMemDevPBase;
	IMG_CPU_PHYADDR sLocalMemCpuPBase;
	IMG_UINT32 ui32LocalMemSize;
	IMG_UINT32 ui32IRQ;
	SGX_TIMING_INFORMATION sTimingInfo;
} SGX_DEVICE_MAP;

_Static_assert(sizeof(SGX_DEVICE_MAP) == 60, "SGX platform device map");

struct SGX_BRIDGE_INIT_INFO;

/* sgxinit_sce.c */
PVRSRV_ERROR DevInitSGXPart2KM(PVRSRV_PER_PROCESS_DATA *psPerProc,
		IMG_HANDLE hDevHandle, struct SGX_BRIDGE_INIT_INFO *psInitInfo);
PVRSRV_ERROR SGXGetInfoForSrvinitKM(IMG_HANDLE hDevHandle,
		SGX_BRIDGE_INFO_FOR_SRVINIT *psInitInfo);

/* sgxinit_sce.c */
PVRSRV_ERROR SGXInitialise(PVRSRV_SGXDEV_INFO *psDevInfo, IMG_BOOL bHardwareRecovery);
PVRSRV_ERROR SGXDeinitialise(IMG_HANDLE hDevCookie);
IMG_VOID SGXOSTimer(IMG_VOID *pvData);
PVRSRV_ERROR SGXRegisterDevice(PVRSRV_DEVICE_NODE *psDeviceNode);
PVRSRV_ERROR SGXGetClientInfoKM(IMG_HANDLE hDevCookie, SGX_CLIENT_INFO *psClientInfo);
PVRSRV_ERROR SGXGetMiscInfoKM(PVRSRV_SGXDEV_INFO *psDevInfo,
		SGX_MISC_INFO *psMiscInfo, PVRSRV_DEVICE_NODE *psDeviceNode,
		IMG_HANDLE hDevMemContext);

/* sgxreset_sce.c */
IMG_VOID SGXReset(PVRSRV_SGXDEV_INFO *psDevInfo, IMG_BOOL bHardwareRecovery,
		IMG_UINT32 ui32PDUMPFlags);

#endif
