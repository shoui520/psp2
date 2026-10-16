/* IMG resman.c owners under the SCE ARM configuration (DEBUG disabled).
 * Copyright (C) Imagination Technologies Ltd. GPL-2.0; pristine source and
 * license are in contrib/img-sgx-ddk-1.7.862890.
 */
#ifndef RESMAN_SCE_H
#define RESMAN_SCE_H

#include "sce_ddk_port.h"

#define RESMAN_CRITERIA_ALL 0x00000000U
#define RESMAN_CRITERIA_RESTYPE 0x00000001U
#define RESMAN_CRITERIA_PVOID_PARAM 0x00000002U
#define RESMAN_CRITERIA_UI32_PARAM 0x00000004U

enum {
	RESMAN_TYPE_SHARED_PB_DESC = 1,
	RESMAN_TYPE_SHARED_PB_DESC_CREATE_LOCK,
	RESMAN_TYPE_HW_RENDER_CONTEXT,
	RESMAN_TYPE_HW_TRANSFER_CONTEXT,
	RESMAN_TYPE_HW_2D_CONTEXT,
	RESMAN_TYPE_TRANSFER_CONTEXT,
	RESMAN_TYPE_DMA_CLIENT_FIFO_DATA,
	RESMAN_TYPE_DISPLAYCLASS_SWAPCHAIN_REF,
	RESMAN_TYPE_DISPLAYCLASS_DEVICE,
	RESMAN_TYPE_BUFFERCLASS_DEVICE,
	RESMAN_TYPE_OS_USERMODE_MAPPING,
	RESMAN_TYPE_DEVICEMEM_CONTEXT,
	RESMAN_TYPE_DEVICECLASSMEM_MAPPING,
	RESMAN_TYPE_DEVICEMEM_MAPPING,
	RESMAN_TYPE_DEVICEMEM_WRAP,
	RESMAN_TYPE_DEVICEMEM_ALLOCATION,
	RESMAN_TYPE_EVENT_OBJECT,
	RESMAN_TYPE_SHARED_MEM_INFO,
	RESMAN_TYPE_MODIFY_SYNC_OPS,
	RESMAN_TYPE_SYNC_INFO,
	/* SCE extends the pristine enum before kernel allocations. */
	RESMAN_TYPE_SHARED_SYNC_INFO,
	RESMAN_TYPE_MUTEX,
	RESMAN_TYPE_RENDER_CONTEXT,
	RESMAN_TYPE_RENDER_TARGET,
	RESMAN_TYPE_KERNEL_DEVICEMEM_ALLOCATION,
};

typedef PVRSRV_ERROR (*RESMAN_FREE_FN)(IMG_VOID *pvParam,
		IMG_UINT32 ui32Param, IMG_BOOL bForceCleanup);

/* SCE retains the existing private tags used by earlier owner consumers. */
typedef struct Resource {
	struct Resource **ppsThis;
	struct Resource *psNext;
	IMG_UINT32 ui32Flags;
	IMG_UINT32 ui32ResType;
	IMG_VOID *pvParam;
	IMG_UINT32 ui32Param;
	RESMAN_FREE_FN pfnFreeResource;
} RESMAN_ITEM;

typedef struct ResourceContext {
	struct ResourceContext **ppsThis;
	struct ResourceContext *psNext;
	PVRSRV_PER_PROCESS_DATA *psPerProc;
	RESMAN_ITEM *psResItemList;
} RESMAN_CONTEXT;

typedef struct {
	RESMAN_CONTEXT *psContextList;
} RESMAN_LIST;

_Static_assert(sizeof(RESMAN_ITEM) == 28, "native resource item");
_Static_assert(sizeof(RESMAN_CONTEXT) == 16, "native resource context");

#endif
