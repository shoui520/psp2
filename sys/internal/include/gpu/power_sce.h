/* IMG 1.7 power-device owner, unchanged in the SCE ARM port.
 * Full 40-byte layout verified against the original registration writer.
 */
#ifndef GPU_POWER_SCE_H
#define GPU_POWER_SCE_H

#include <gpu/sce_ddk_port.h>

typedef enum _PVRSRV_INIT_SERVER_STATE_ {
	PVRSRV_INIT_SERVER_Unspecified = -1,
	PVRSRV_INIT_SERVER_RUNNING = 0,
	PVRSRV_INIT_SERVER_RAN = 1,
	PVRSRV_INIT_SERVER_SUCCESSFUL = 2,
	PVRSRV_INIT_SERVER_NUM = 3,
	PVRSRV_INIT_SERVER_FORCE_I32 = 0x7fffffff
} PVRSRV_INIT_SERVER_STATE, *PPVRSRV_INIT_SERVER_STATE;

typedef struct _PVRSRV_POWER_DEV_TAG_ {
	PFN_PRE_POWER pfnPrePower;
	PFN_POST_POWER pfnPostPower;
	PFN_PRE_CLOCKSPEED_CHANGE pfnPreClockSpeedChange;
	PFN_POST_CLOCKSPEED_CHANGE pfnPostClockSpeedChange;
	IMG_HANDLE hDevCookie;
	IMG_UINT32 ui32DeviceIndex;
	PVRSRV_DEV_POWER_STATE eDefaultPowerState;
	PVRSRV_DEV_POWER_STATE eCurrentPowerState;
	struct _PVRSRV_POWER_DEV_TAG_ *psNext;
	struct _PVRSRV_POWER_DEV_TAG_ **ppsThis;
} PVRSRV_POWER_DEV;

#endif
