/* IMG sgxinfo.h command vocabulary, shared by the SCE scheduler. */
#ifndef SGXUTILS_SCE_H
#define SGXUTILS_SCE_H

#include "sgxinit_sce.h"
#include "resman_sce.h"

/* SCE: native contexts are supplied by the caller. The DDK cleanup
 * owners retain the device address separately from the memory owner.
 */
typedef struct _SGX_HW_RENDER_CONTEXT_CLEANUP_ {
	PVRSRV_DEVICE_NODE *psDeviceNode;
	IMG_DEV_VIRTADDR sHWRenderContextDevVAddr;
	IMG_HANDLE hBlockAlloc;
	RESMAN_ITEM *psResItem;
	struct PVRSRV_KERNEL_MEM_INFO *psHWRenderContextMemInfo;
	IMG_BOOL bPrioritySet;
} SGX_HW_RENDER_CONTEXT_CLEANUP;

typedef struct _SGX_HW_2D_CONTEXT_CLEANUP_ {
	PVRSRV_DEVICE_NODE *psDeviceNode;
	IMG_DEV_VIRTADDR sHW2DContextDevVAddr;
	IMG_HANDLE hBlockAlloc;
	RESMAN_ITEM *psResItem;
	struct PVRSRV_KERNEL_MEM_INFO *psHW2DContextMemInfo;
} SGX_HW_2D_CONTEXT_CLEANUP;

_Static_assert(sizeof(SGX_HW_RENDER_CONTEXT_CLEANUP) == 24, "SCE render cleanup");
_Static_assert(sizeof(SGX_HW_2D_CONTEXT_CLEANUP) == 20, "SCE 2D cleanup");

typedef enum _SGXMKIF_CMD_TYPE_ {
	SGXMKIF_CMD_TA = 0,
	SGXMKIF_CMD_TRANSFER = 1,
	SGXMKIF_CMD_2D = 2,
	SGXMKIF_CMD_POWER = 3,
	SGXMKIF_CMD_CONTEXTSUSPEND = 4,
	SGXMKIF_CMD_CLEANUP = 5,
	SGXMKIF_CMD_GETMISCINFO = 6,
	SGXMKIF_CMD_PROCESS_QUEUES = 7,
	SGXMKIF_CMD_DATABREAKPOINT = 8,
	SGXMKIF_CMD_SETHWPERFSTATUS = 9,
	SGXMKIF_CMD_FLUSHPDCACHE = 10,
	SGXMKIF_CMD_MAX = 11,
	SGXMKIF_CMD_FORCE_I32 = -1
} SGXMKIF_CMD_TYPE;

#endif
