/*
 * The PowerVR services bridge (the DDK's pvr_bridge.h): the package a user
 * library hands SCE's GPU driver through the SceGpuEs4ForUser syscall.
 */

#pragma once

#include <scetypes.h>

/* a services call, as the SceGpuEs4ForUser bridge syscall takes it */
typedef struct PVRSRV_BRIDGE_PACKAGE_TAG {
	SceUInt32 ui32BridgeID;
	SceUInt32 ui32Size;
	void *pvParamIn;
	SceUInt32 ui32InBufferSize;
	void *pvParamOut;
	SceUInt32 ui32OutBufferSize;
	SceUInt32 hKernelServices;	/* IMG_SID */
} PVRSRV_BRIDGE_PACKAGE;

_Static_assert(sizeof(PVRSRV_BRIDGE_PACKAGE) == 28, "PVRSRV_BRIDGE_PACKAGE size");
