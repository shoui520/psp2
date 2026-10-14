/* IMG power.c base; SCE deltas are marked beside their native paths. */
#include "device_sce.h"
#include "power_sce.h"
#include "gpu-dump.h"
#include <stdarg.h>
#include <stddef.h>

#undef __FILE_NAME__
#define __FILE_NAME__ "common/power.c"

/* Pristine IMG state words, ordered as in the SCE native data. */
static IMG_BOOL gbInitServerRunning = IMG_FALSE;
static IMG_BOOL gbInitSuccessful = IMG_FALSE;
static IMG_BOOL gbInitServerRan = IMG_FALSE;

PVRSRV_ERROR SysDevicePrePowerState(IMG_UINT32 ui32DeviceIndex,
		PVRSRV_DEV_POWER_STATE eNewPowerState,
		PVRSRV_DEV_POWER_STATE eCurrentPowerState);
PVRSRV_ERROR SysDevicePostPowerState(IMG_UINT32 ui32DeviceIndex,
		PVRSRV_DEV_POWER_STATE eNewPowerState,
		PVRSRV_DEV_POWER_STATE eCurrentPowerState);
PVRSRV_ERROR OSUnlockResource(PVRSRV_RESOURCE *psResource, IMG_UINT32 ui32ID);
IMG_VOID OSPowerLockUnwrap(IMG_VOID);
PVRSRV_ERROR OSPowerLockWrap(IMG_VOID);
PVRSRV_ERROR OSLockResource(PVRSRV_RESOURCE *psResource, IMG_UINT32 ui32ID);
IMG_BOOL OSIsResourceLocked(PVRSRV_RESOURCE *psResource, IMG_UINT32 ui32ID);
IMG_VOID OSWaitus(IMG_UINT32 ui32Timeus);
PVRSRV_ERROR List_PVRSRV_POWER_DEV_PVRSRV_ERROR_Any_va(PVRSRV_POWER_DEV *psHead,
		PVRSRV_ERROR (*pfnCallBack)(PVRSRV_POWER_DEV *psNode, va_list va), ...);

#define ISR_ID 0xfffffffdU
#define KERNEL_ID 0xffffffffU

PVRSRV_ERROR SysSystemPrePowerState(PVRSRV_SYS_POWER_STATE eNewSysPowerState);
PVRSRV_ERROR SysSystemPostPowerState(PVRSRV_SYS_POWER_STATE eNewSysPowerState);
IMG_VOID PVRSRVScheduleDeviceCallbacks(IMG_VOID);
IMG_VOID *List_PVRSRV_POWER_DEV_Any_va(PVRSRV_POWER_DEV *psHead,
		IMG_VOID *(*pfnCallBack)(PVRSRV_POWER_DEV *psNode, va_list va), ...);
IMG_VOID *MatchPowerDeviceIndex_AnyVaCb(PVRSRV_POWER_DEV *psPowerDevice, va_list va);
IMG_VOID List_PVRSRV_POWER_DEV_Remove(PVRSRV_POWER_DEV *psPowerDevice);
PVRSRV_ERROR OSFreeMem_Impl(IMG_UINT32 ui32Flags, IMG_SIZE_T ui32Size,
		IMG_VOID *pvLinAddr, IMG_HANDLE hBlockAlloc);
PVRSRV_ERROR OSAllocMem_Impl(IMG_UINT32 ui32Flags, IMG_SIZE_T ui32Size,
		IMG_VOID **ppvLinAddr, IMG_HANDLE *phBlockAlloc);
IMG_VOID List_PVRSRV_POWER_DEV_Insert(PVRSRV_POWER_DEV **ppsHead,
		PVRSRV_POWER_DEV *psPowerDevice);

static IMG_BOOL _IsSystemStatePowered(PVRSRV_SYS_POWER_STATE eSystemPowerState)
{
	return (IMG_BOOL)(eSystemPowerState < PVRSRV_SYS_POWER_STATE_D2);
}

/* Pristine IMG pre-power iterator callback. */
PVRSRV_ERROR PVRSRVDevicePrePowerStateKM_AnyVaCb(PVRSRV_POWER_DEV *psPowerDevice,
		va_list va)
{
	PVRSRV_DEV_POWER_STATE eNewDevicePowerState;
	PVRSRV_ERROR eError;
	IMG_BOOL bAllDevices;
	IMG_UINT32 ui32DeviceIndex;
	PVRSRV_DEV_POWER_STATE eNewPowerState;

	bAllDevices = va_arg(va, IMG_BOOL);
	ui32DeviceIndex = va_arg(va, IMG_UINT32);
	eNewPowerState = va_arg(va, PVRSRV_DEV_POWER_STATE);
	if (bAllDevices || ui32DeviceIndex == psPowerDevice->ui32DeviceIndex) {
		eNewDevicePowerState = eNewPowerState == PVRSRV_DEV_POWER_STATE_DEFAULT ?
				psPowerDevice->eDefaultPowerState : eNewPowerState;
		if (psPowerDevice->eCurrentPowerState != eNewDevicePowerState) {
			if (psPowerDevice->pfnPrePower != NULL) {
				eError = psPowerDevice->pfnPrePower(psPowerDevice->hDevCookie,
						eNewDevicePowerState,
						psPowerDevice->eCurrentPowerState);
				if (eError != PVRSRV_OK)
					return eError;
			}
			eError = SysDevicePrePowerState(psPowerDevice->ui32DeviceIndex,
					eNewDevicePowerState, psPowerDevice->eCurrentPowerState);
			if (eError != PVRSRV_OK)
				return eError;
		}
	}
	return PVRSRV_OK;
}

/* Pristine IMG post-power iterator callback. */
PVRSRV_ERROR PVRSRVDevicePostPowerStateKM_AnyVaCb(PVRSRV_POWER_DEV *psPowerDevice,
		va_list va)
{
	PVRSRV_DEV_POWER_STATE eNewDevicePowerState;
	PVRSRV_ERROR eError;
	IMG_BOOL bAllDevices;
	IMG_UINT32 ui32DeviceIndex;
	PVRSRV_DEV_POWER_STATE eNewPowerState;

	bAllDevices = va_arg(va, IMG_BOOL);
	ui32DeviceIndex = va_arg(va, IMG_UINT32);
	eNewPowerState = va_arg(va, PVRSRV_DEV_POWER_STATE);
	if (bAllDevices || ui32DeviceIndex == psPowerDevice->ui32DeviceIndex) {
		eNewDevicePowerState = eNewPowerState == PVRSRV_DEV_POWER_STATE_DEFAULT ?
				psPowerDevice->eDefaultPowerState : eNewPowerState;
		if (psPowerDevice->eCurrentPowerState != eNewDevicePowerState) {
			eError = SysDevicePostPowerState(psPowerDevice->ui32DeviceIndex,
					eNewDevicePowerState, psPowerDevice->eCurrentPowerState);
			if (eError != PVRSRV_OK)
				return eError;
			if (psPowerDevice->pfnPostPower != NULL) {
				eError = psPowerDevice->pfnPostPower(psPowerDevice->hDevCookie,
						eNewDevicePowerState,
						psPowerDevice->eCurrentPowerState);
				if (eError != PVRSRV_OK)
					return eError;
			}
			psPowerDevice->eCurrentPowerState = eNewDevicePowerState;
		}
	}
	return PVRSRV_OK;
}

/* IMG setter; SCE omits the unknown-selector diagnostic. */
PVRSRV_ERROR PVRSRVSetInitServerState(PVRSRV_INIT_SERVER_STATE eInitServerState,
		IMG_BOOL bState)
{
	switch (eInitServerState) {
	case PVRSRV_INIT_SERVER_RUNNING:
		gbInitServerRunning = bState;
		break;
	case PVRSRV_INIT_SERVER_RAN:
		gbInitServerRan = bState;
		break;
	case PVRSRV_INIT_SERVER_SUCCESSFUL:
		gbInitSuccessful = bState;
		break;
	default:
		return PVRSRV_ERROR_UNKNOWN_INIT_SERVER_STATE;
	}
	return PVRSRV_OK;
}

/* IMG getter; SCE omits the unknown-selector diagnostic. */
IMG_BOOL PVRSRVGetInitServerState(PVRSRV_INIT_SERVER_STATE eInitServerState)
{
	IMG_BOOL bReturnVal;

	switch (eInitServerState) {
	case PVRSRV_INIT_SERVER_RUNNING:
		bReturnVal = gbInitServerRunning;
		break;
	case PVRSRV_INIT_SERVER_RAN:
		bReturnVal = gbInitServerRan;
		break;
	case PVRSRV_INIT_SERVER_SUCCESSFUL:
		bReturnVal = gbInitSuccessful;
		break;
	default:
		bReturnVal = IMG_FALSE;
	}
	return bReturnVal;
}

/* Pristine IMG power unlock. */
IMG_VOID PVRSRVPowerUnlock(IMG_UINT32 ui32CallerID)
{
	OSUnlockResource(&gpsSysData->sPowerStateChangeResource, ui32CallerID);
	OSPowerLockUnwrap();
}

/* IMG locking loop; SCE's empty wrap helper takes no try-lock argument. */
PVRSRV_ERROR PVRSRVPowerLock(IMG_UINT32 ui32CallerID, IMG_BOOL bSystemPowerEvent)
{
	PVRSRV_ERROR eError;
	SYS_DATA *psSysData;
	IMG_UINT32 ui32Timeout = 1000000;
	IMG_BOOL bTryLock = ui32CallerID == ISR_ID;

	psSysData = gpsSysData;
	eError = OSPowerLockWrap();
	if (eError != PVRSRV_OK)
		return eError;
	do {
		eError = OSLockResource(&psSysData->sPowerStateChangeResource, ui32CallerID);
		if (eError == PVRSRV_OK)
			break;
		else if (bTryLock) {
			eError = PVRSRV_ERROR_RETRY;
			break;
		}
		OSWaitus(1);
		ui32Timeout--;
	} while (ui32Timeout > 0);
	if (eError != PVRSRV_OK) {
		OSPowerLockUnwrap();
		return eError;
	}
	if (bSystemPowerEvent)
		return eError;
	if (!(psSysData->eCurrentPowerState < PVRSRV_SYS_POWER_STATE_D2)) {
		PVRSRVPowerUnlock(ui32CallerID);
		eError = PVRSRV_ERROR_RETRY;
	}
	return eError;
}

/* Pristine IMG iterator wrappers, inlined in Sony's transition callers. */
static PVRSRV_ERROR PVRSRVDevicePrePowerStateKM(IMG_BOOL bAllDevices,
		IMG_UINT32 ui32DeviceIndex, PVRSRV_DEV_POWER_STATE eNewPowerState)
{
	return List_PVRSRV_POWER_DEV_PVRSRV_ERROR_Any_va(gpsSysData->psPowerDeviceList,
			PVRSRVDevicePrePowerStateKM_AnyVaCb,
			bAllDevices, ui32DeviceIndex, eNewPowerState);
}

static PVRSRV_ERROR PVRSRVDevicePostPowerStateKM(IMG_BOOL bAllDevices,
		IMG_UINT32 ui32DeviceIndex, PVRSRV_DEV_POWER_STATE eNewPowerState)
{
	return List_PVRSRV_POWER_DEV_PVRSRV_ERROR_Any_va(gpsSysData->psPowerDeviceList,
			PVRSRVDevicePostPowerStateKM_AnyVaCb,
			bAllDevices, ui32DeviceIndex, eNewPowerState);
}

/* IMG transition with PDUMP disabled; SCE omits the error diagnostic. */
PVRSRV_ERROR PVRSRVSetDevicePowerStateKM(IMG_UINT32 ui32DeviceIndex,
		PVRSRV_DEV_POWER_STATE eNewPowerState,
		IMG_UINT32 ui32CallerID, IMG_BOOL bRetainMutex)
{
	PVRSRV_ERROR eError;

	eError = PVRSRVPowerLock(ui32CallerID, IMG_FALSE);
	if (eError != PVRSRV_OK)
		return eError;
	eError = PVRSRVDevicePrePowerStateKM(IMG_FALSE, ui32DeviceIndex, eNewPowerState);
	if (eError != PVRSRV_OK)
		goto Exit;
	eError = PVRSRVDevicePostPowerStateKM(IMG_FALSE, ui32DeviceIndex, eNewPowerState);
Exit:
	if (!bRetainMutex || eError != PVRSRV_OK)
		PVRSRVPowerUnlock(ui32CallerID);
	return eError;
}

/* IMG system transition; SCE omits the failure diagnostic. */
PVRSRV_ERROR PVRSRVSystemPrePowerStateKM(PVRSRV_SYS_POWER_STATE eNewSysPowerState)
{
	PVRSRV_ERROR eError;
	SYS_DATA *psSysData;
	PVRSRV_DEV_POWER_STATE eNewDevicePowerState;

	psSysData = gpsSysData;
	eError = PVRSRVPowerLock(KERNEL_ID, IMG_TRUE);
	if (eError != PVRSRV_OK)
		return eError;
	if (_IsSystemStatePowered(eNewSysPowerState) !=
			_IsSystemStatePowered(psSysData->eCurrentPowerState)) {
		if (_IsSystemStatePowered(eNewSysPowerState))
			eNewDevicePowerState = PVRSRV_DEV_POWER_STATE_DEFAULT;
		else
			eNewDevicePowerState = PVRSRV_DEV_POWER_STATE_OFF;
		eError = PVRSRVDevicePrePowerStateKM(IMG_TRUE, 0, eNewDevicePowerState);
		if (eError != PVRSRV_OK)
			goto ErrorExit;
	}
	if (eNewSysPowerState != psSysData->eCurrentPowerState) {
		eError = SysSystemPrePowerState(eNewSysPowerState);
		if (eError != PVRSRV_OK)
			goto ErrorExit;
	}
	return eError;
ErrorExit:
	psSysData->eFailedPowerState = eNewSysPowerState;
	PVRSRVPowerUnlock(KERNEL_ID);
	return eError;
}

/* IMG system completion; SCE omits the success diagnostic. */
PVRSRV_ERROR PVRSRVSystemPostPowerStateKM(PVRSRV_SYS_POWER_STATE eNewSysPowerState)
{
	PVRSRV_ERROR eError = PVRSRV_OK;
	SYS_DATA *psSysData;
	PVRSRV_DEV_POWER_STATE eNewDevicePowerState;

	psSysData = gpsSysData;
	if (eNewSysPowerState != psSysData->eCurrentPowerState) {
		eError = SysSystemPostPowerState(eNewSysPowerState);
		if (eError != PVRSRV_OK)
			goto Exit;
	}
	if (_IsSystemStatePowered(eNewSysPowerState) !=
			_IsSystemStatePowered(psSysData->eCurrentPowerState)) {
		if (_IsSystemStatePowered(eNewSysPowerState))
			eNewDevicePowerState = PVRSRV_DEV_POWER_STATE_DEFAULT;
		else
			eNewDevicePowerState = PVRSRV_DEV_POWER_STATE_OFF;
		eError = PVRSRVDevicePostPowerStateKM(IMG_TRUE, 0, eNewDevicePowerState);
		if (eError != PVRSRV_OK)
			goto Exit;
	}
	psSysData->eCurrentPowerState = eNewSysPowerState;
Exit:
	PVRSRVPowerUnlock(KERNEL_ID);
	if (_IsSystemStatePowered(eNewSysPowerState) &&
			PVRSRVGetInitServerState(PVRSRV_INIT_SERVER_SUCCESSFUL))
		PVRSRVScheduleDeviceCallbacks();
	return eError;
}

/* IMG system-state setter; SCE omits the failure diagnostic. */
PVRSRV_ERROR PVRSRVSetPowerStateKM(PVRSRV_SYS_POWER_STATE eNewSysPowerState)
{
	PVRSRV_ERROR eError;
	SYS_DATA *psSysData;

	psSysData = gpsSysData;
	eError = PVRSRVSystemPrePowerStateKM(eNewSysPowerState);
	if (eError != PVRSRV_OK)
		goto ErrorExit;
	eError = PVRSRVSystemPostPowerStateKM(eNewSysPowerState);
	if (eError != PVRSRV_OK)
		goto ErrorExit;
	psSysData->eFailedPowerState = PVRSRV_SYS_POWER_STATE_Unspecified;
	return PVRSRV_OK;
ErrorExit:
	psSysData->eFailedPowerState = eNewSysPowerState;
	return eError;
}

/* Pristine IMG power-device removal. */
PVRSRV_ERROR PVRSRVRemovePowerDevice(IMG_UINT32 ui32DeviceIndex)
{
	SYS_DATA *psSysData;
	PVRSRV_POWER_DEV *psPowerDev;

	psSysData = gpsSysData;
	psPowerDev = (PVRSRV_POWER_DEV *)List_PVRSRV_POWER_DEV_Any_va(
			psSysData->psPowerDeviceList,
			MatchPowerDeviceIndex_AnyVaCb, ui32DeviceIndex);
	if (psPowerDev) {
		List_PVRSRV_POWER_DEV_Remove(psPowerDev);
		OSFreeMem_Impl(1, sizeof(PVRSRV_POWER_DEV), psPowerDev, NULL);
	}
	return PVRSRV_OK;
}

/* IMG registration; SCE omits the allocation diagnostic. */
PVRSRV_ERROR PVRSRVRegisterPowerDevice(IMG_UINT32 ui32DeviceIndex,
		PFN_PRE_POWER pfnPrePower, PFN_POST_POWER pfnPostPower,
		PFN_PRE_CLOCKSPEED_CHANGE pfnPreClockSpeedChange,
		PFN_POST_CLOCKSPEED_CHANGE pfnPostClockSpeedChange,
		IMG_HANDLE hDevCookie, PVRSRV_DEV_POWER_STATE eCurrentPowerState,
		PVRSRV_DEV_POWER_STATE eDefaultPowerState)
{
	PVRSRV_ERROR eError;
	SYS_DATA *psSysData;
	PVRSRV_POWER_DEV *psPowerDevice;

	if (pfnPrePower == NULL && pfnPostPower == NULL)
		return PVRSRVRemovePowerDevice(ui32DeviceIndex);
	psSysData = gpsSysData;
	eError = OSAllocMem_Impl(2, sizeof(PVRSRV_POWER_DEV),
			(IMG_VOID **)&psPowerDevice, NULL);
	if (eError != PVRSRV_OK)
		return eError;
	psPowerDevice->pfnPrePower = pfnPrePower;
	psPowerDevice->pfnPostPower = pfnPostPower;
	psPowerDevice->pfnPreClockSpeedChange = pfnPreClockSpeedChange;
	psPowerDevice->pfnPostClockSpeedChange = pfnPostClockSpeedChange;
	psPowerDevice->eDefaultPowerState = eDefaultPowerState;
	psPowerDevice->eCurrentPowerState = eCurrentPowerState;
	psPowerDevice->hDevCookie = hDevCookie;
	psPowerDevice->ui32DeviceIndex = ui32DeviceIndex;
	List_PVRSRV_POWER_DEV_Insert(&psSysData->psPowerDeviceList, psPowerDevice);
	return PVRSRV_OK;
}

/* Pristine IMG device-power query. */
IMG_BOOL PVRSRVIsDevicePowered(IMG_UINT32 ui32DeviceIndex)
{
	SYS_DATA *psSysData;
	PVRSRV_POWER_DEV *psPowerDevice;

	psSysData = gpsSysData;
	if (OSIsResourceLocked(&psSysData->sPowerStateChangeResource, KERNEL_ID) ||
			OSIsResourceLocked(&psSysData->sPowerStateChangeResource, ISR_ID))
		return IMG_FALSE;
	psPowerDevice = (PVRSRV_POWER_DEV *)List_PVRSRV_POWER_DEV_Any_va(
			psSysData->psPowerDeviceList,
			MatchPowerDeviceIndex_AnyVaCb, ui32DeviceIndex);
	if (psPowerDevice && psPowerDevice->eCurrentPowerState == PVRSRV_DEV_POWER_STATE_ON)
		return IMG_TRUE;
	return IMG_FALSE;
}

/* IMG clock callbacks; SCE omits diagnostics. */
PVRSRV_ERROR PVRSRVDevicePreClockSpeedChange(IMG_UINT32 ui32DeviceIndex,
		IMG_BOOL bIdleDevice, IMG_VOID *pvInfo)
{
	PVRSRV_ERROR eError = PVRSRV_OK;
	SYS_DATA *psSysData;
	PVRSRV_POWER_DEV *psPowerDevice;

	(void)pvInfo;
	psSysData = gpsSysData;
	if (bIdleDevice) {
		eError = PVRSRVPowerLock(KERNEL_ID, IMG_FALSE);
		if (eError != PVRSRV_OK)
			return eError;
	}
	psPowerDevice = (PVRSRV_POWER_DEV *)List_PVRSRV_POWER_DEV_Any_va(
			psSysData->psPowerDeviceList,
			MatchPowerDeviceIndex_AnyVaCb, ui32DeviceIndex);
	if (psPowerDevice && psPowerDevice->pfnPostClockSpeedChange)
		eError = psPowerDevice->pfnPreClockSpeedChange(psPowerDevice->hDevCookie,
				bIdleDevice, psPowerDevice->eCurrentPowerState);
	if (!bIdleDevice)
		return eError;
	if (eError != PVRSRV_OK)
		PVRSRVPowerUnlock(KERNEL_ID);
	return eError;
}

IMG_VOID PVRSRVDevicePostClockSpeedChange(IMG_UINT32 ui32DeviceIndex,
		IMG_BOOL bIdleDevice, IMG_VOID *pvInfo)
{
	SYS_DATA *psSysData;
	PVRSRV_POWER_DEV *psPowerDevice;

	(void)pvInfo;
	psSysData = gpsSysData;
	psPowerDevice = (PVRSRV_POWER_DEV *)List_PVRSRV_POWER_DEV_Any_va(
			psSysData->psPowerDeviceList,
			MatchPowerDeviceIndex_AnyVaCb, ui32DeviceIndex);
	if (psPowerDevice && psPowerDevice->pfnPostClockSpeedChange)
		psPowerDevice->pfnPostClockSpeedChange(psPowerDevice->hDevCookie,
				bIdleDevice, psPowerDevice->eCurrentPowerState);
	if (bIdleDevice)
		PVRSRVPowerUnlock(KERNEL_ID);
}
