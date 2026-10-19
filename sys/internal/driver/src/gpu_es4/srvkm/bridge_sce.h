/* IMG bridge package, shared by the native entry and common dispatcher. */
#ifndef BRIDGE_SCE_H
#define BRIDGE_SCE_H

#include <gpu/sce_ddk_port.h>

/* SCE native command numbers use a compact 0x1000-based bridge namespace. */
enum {
	PVRSRV_BRIDGE_CONNECT_SERVICES = 0x100c,
	PVRSRV_BRIDGE_MODIFY_PENDING_SYNC_OPS = 0x104f,
	PVRSRV_BRIDGE_MODIFY_COMPLETE_SYNC_OPS = 0x1050,
	PVRSRV_BRIDGE_CHECK_SYNC_INFO_MOD_OBJ = 0x1057,
	PVRSRV_BRIDGE_SGX_RESET_SYNC_WAITS = 0x1093
};

typedef struct PVRSRV_BRIDGE_PACKAGE_TAG {
	IMG_UINT32 ui32BridgeID;
	IMG_UINT32 ui32Size;
	IMG_VOID *pvParamIn;
	IMG_UINT32 ui32InBufferSize;
	IMG_VOID *pvParamOut;
	IMG_UINT32 ui32OutBufferSize;
	IMG_HANDLE hKernelServices;
} PVRSRV_BRIDGE_PACKAGE;

_Static_assert(sizeof(PVRSRV_BRIDGE_PACKAGE) == 28, "native bridge package");

#endif
