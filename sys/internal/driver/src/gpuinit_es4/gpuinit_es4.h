/* SceGpuEs4 services that SceGpuEs4Init (the SGX init server) imports. */
#pragma once

#include <psp2kern/types.h>
#include <gpu/sgxmkif_sce.h>
#include "../gpu_es4/srvkm/sgxutils_sce.h"
#include "../gpu_es4/srvkm/perproc_sce.h"
#include "../gpu_es4/srvkm/power_sce.h"
#include "../gpu_es4/srvkm/gpu-mmu.h"
#include <lowio/pervasive.h>
#include <modulemgr/modulemgr.h>

IMG_BOOL OSProcHasPrivSrvInit(IMG_VOID);
IMG_UINT32 OSGetCurrentProcessIDKM(IMG_VOID);
IMG_VOID *OSAllocMem(IMG_SIZE_T size);
IMG_INT OSFreeMem(IMG_VOID *address);
IMG_VOID *OSMemSet(IMG_VOID *dest, IMG_INT value, IMG_SIZE_T size);
IMG_VOID *OSMemCopy(IMG_VOID *dest, const IMG_VOID *src, IMG_SIZE_T size);

IMG_BOOL PVRSRVGetInitServerState(PVRSRV_INIT_SERVER_STATE eInitServerState);
PVRSRV_ERROR PVRSRVSetInitServerState(PVRSRV_INIT_SERVER_STATE eInitServerState,
				      IMG_BOOL bState);
PVRSRV_ERROR PVRSRVPerProcessDataConnect(IMG_UINT32 ui32PID, IMG_UINT32 ui32Flags);
IMG_VOID PVRSRVPerProcessDataDisconnect(IMG_UINT32 ui32PID);
PVRSRV_PER_PROCESS_DATA *PVRSRVPerProcessData(IMG_UINT32 ui32PID);
PVRSRV_ERROR PVRSRVEnumerateDevicesKM(IMG_UINT32 *pui32NumDevices,
				      PVRSRV_DEVICE_IDENTIFIER *psDevIdList);
PVRSRV_ERROR PVRSRVFinaliseSystem(IMG_BOOL bInitSuccessful);
PVRSRV_ERROR PVRSRVAcquireDeviceDataKM(IMG_UINT32 ui32DevIndex,
				       PVRSRV_DEVICE_TYPE eDeviceType,
				       IMG_HANDLE *phDevCookie);
PVRSRV_ERROR SGXGetInfoForSrvinitKM(IMG_HANDLE hDevHandle,
				    SGX_BRIDGE_INFO_FOR_SRVINIT *psInitInfo);
PVRSRV_ERROR PVRSRVAllocDeviceMemKM(IMG_HANDLE hDevCookie,
				    PVRSRV_PER_PROCESS_DATA *psPerProc,
				    IMG_HANDLE hDevMemHeap, IMG_UINT32 ui32Attribs,
				    IMG_SIZE_T ui32Size, IMG_SIZE_T ui32Alignment,
				    IMG_INT32 hMemBlockProcRef,
				    PVRSRV_KERNEL_MEM_INFO **ppsMemInfo);
PVRSRV_ERROR DevInitSGXPart2KM(PVRSRV_PER_PROCESS_DATA *psPerProc,
			       IMG_HANDLE hDevHandle,
			       SGX_BRIDGE_INIT_INFO *psInitInfo);

int ksceGpuMmuMapMemory(GpuMmuContext *context, SceUInt32 vaddr, void *base,
			SceUInt32 size, SceUInt32 flags);
int ksceGpuMmuUnmapMemory(GpuMmuContext *context, SceUInt32 vaddr, SceUInt32 size);
