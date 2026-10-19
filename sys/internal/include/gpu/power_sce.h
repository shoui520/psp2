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

/* common/power.c */
PVRSRV_ERROR PVRSRVSetInitServerState(PVRSRV_INIT_SERVER_STATE eInitServerState,
		IMG_BOOL bState);
IMG_BOOL PVRSRVGetInitServerState(PVRSRV_INIT_SERVER_STATE eInitServerState);

/* common/power.c */
IMG_VOID PVRSRVPowerUnlock(IMG_UINT32 ui32CallerID);
PVRSRV_ERROR PVRSRVPowerLock(IMG_UINT32 ui32CallerID, IMG_BOOL bSystemPowerEvent);
PVRSRV_ERROR PVRSRVSetDevicePowerStateKM(IMG_UINT32 ui32DeviceIndex,
		PVRSRV_DEV_POWER_STATE eNewPowerState,
		IMG_UINT32 ui32CallerID, IMG_BOOL bRetainMutex);
PVRSRV_ERROR PVRSRVSetPowerStateKM(PVRSRV_SYS_POWER_STATE eNewSysPowerState);
PVRSRV_ERROR PVRSRVRemovePowerDevice(IMG_UINT32 ui32DeviceIndex);
PVRSRV_ERROR PVRSRVRegisterPowerDevice(IMG_UINT32 ui32DeviceIndex,
		PFN_PRE_POWER pfnPrePower, PFN_POST_POWER pfnPostPower,
		PFN_PRE_CLOCKSPEED_CHANGE pfnPreClockSpeedChange,
		PFN_POST_CLOCKSPEED_CHANGE pfnPostClockSpeedChange,
		IMG_HANDLE hDevCookie, PVRSRV_DEV_POWER_STATE eCurrentPowerState,
		PVRSRV_DEV_POWER_STATE eDefaultPowerState);
IMG_BOOL PVRSRVIsDevicePowered(IMG_UINT32 ui32DeviceIndex);
PVRSRV_ERROR PVRSRVDevicePreClockSpeedChange(IMG_UINT32 ui32DeviceIndex,
		IMG_BOOL bIdleDevice, IMG_VOID *pvInfo);
IMG_VOID PVRSRVDevicePostClockSpeedChange(IMG_UINT32 ui32DeviceIndex,
		IMG_BOOL bIdleDevice, IMG_VOID *pvInfo);

/* sgxpower_sce.c */
PVRSRV_ERROR SGXPrePowerState(IMG_HANDLE hDevHandle,
		PVRSRV_DEV_POWER_STATE eNewPowerState,
		PVRSRV_DEV_POWER_STATE eCurrentPowerState);
PVRSRV_ERROR SGXPostPowerState(IMG_HANDLE hDevHandle,
		PVRSRV_DEV_POWER_STATE eNewPowerState,
		PVRSRV_DEV_POWER_STATE eCurrentPowerState);
PVRSRV_ERROR SGXPreClockSpeedChange(IMG_HANDLE hDevHandle,
		IMG_BOOL bIdleDevice, PVRSRV_DEV_POWER_STATE eCurrentPowerState);
PVRSRV_ERROR SGXPostClockSpeedChange(IMG_HANDLE hDevHandle,
		IMG_BOOL bIdleDevice, PVRSRV_DEV_POWER_STATE eCurrentPowerState);

/* suspend_sce.c */
int CreateSuspendWaitEvent(void);
int sceGpuCancelSuspendWait(void);
int sceGpuSignalSuspendWait(void);
int sceGpuWaitSuspend(void);

#endif
