/* SCE system/environment owners, adapted from IMG syscommon.h/env_data.h.
 * The pristine candidate remains in contrib/img-sgx-ddk-1.7.862890.
 */
#ifndef GPU_SYSTEM_SCE_H
#define GPU_SYSTEM_SCE_H

#include "device_sce.h"

typedef struct _SYS_DATA_TAG_ SYS_DATA;
typedef struct _PVRSRV_POWER_DEV_TAG_ PVRSRV_POWER_DEV;

typedef struct _SYS_DEVICE_ID_TAG {
	IMG_UINT32 uiID;
	IMG_BOOL bInUse;
} SYS_DEVICE_ID;

/* SCE replaces Linux workqueues with a kernel thread and event flag. */
typedef struct _ENV_DATA_TAG {
	IMG_CHAR *pvBridgeData;
	IMG_BOOL bLISRInstalled;
	IMG_BOOL bMISRInstalled;
	IMG_UINT32 ui32IRQ;
	SYS_DATA *pvISRCookie;
	IMG_INT misrThread;
	IMG_INT misrEventFlag;
	IMG_BOOL misrRunning;
	IMG_VOID (*pfnMISR)(IMG_VOID *pvData);
	IMG_VOID *pvMISRData;
} ENV_DATA;

/* SCE omits generic queue/shared-sync state and the unused platform machinery.
 * Both 80-byte owners are zeroed by SysInitialise. All observed readers/writers
 * end at pvEnvSpecificData; the trailing five words remain unused.
 */
struct _SYS_DATA_TAG_ {
	IMG_UINT32 ui32NumDevices;
	SYS_DEVICE_ID sDeviceID[3];
	PVRSRV_DEVICE_NODE *psDeviceNodeList;
	PVRSRV_POWER_DEV *psPowerDeviceList;
	PVRSRV_RESOURCE sPowerStateChangeResource;
	PVRSRV_SYS_POWER_STATE eCurrentPowerState;
	PVRSRV_SYS_POWER_STATE eFailedPowerState;
	IMG_UINT32 ui32CurrentOSPowerState;
	ENV_DATA *pvEnvSpecificData;
	IMG_UINT32 reserved[5];
};

#endif
