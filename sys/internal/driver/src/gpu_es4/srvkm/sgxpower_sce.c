/* IMG devices/sgx/sgxpower.c, GPL-2.0; pristine candidate and license
 * remain in contrib/img-sgx-ddk-1.7.862890.
 */
#include <gpu/sgxinit_sce.h>
#include <gpu/sgxmkif_sce.h>
#include <threadmgr/threadmgr.h>
#include <string.h>
#include "sgxkick_sce.h"
#include "system_sce.h"
#include <gpu/osfunc_sce.h>

PVRSRV_ERROR SGXPrePowerState(IMG_HANDLE hDevHandle,
		PVRSRV_DEV_POWER_STATE eNewPowerState,
		PVRSRV_DEV_POWER_STATE eCurrentPowerState);
PVRSRV_ERROR SGXPostPowerState(IMG_HANDLE hDevHandle,
		PVRSRV_DEV_POWER_STATE eNewPowerState,
		PVRSRV_DEV_POWER_STATE eCurrentPowerState);
PVRSRV_ERROR SGXUpdateTimingInfo(PVRSRV_DEVICE_NODE *psDeviceNode);

/* SCE: the source-local PollForValueKM specialization uses a fixed
 * 500000us timeout and 50us non-preemptible delay. The suffix distinguishes
 * this original function from common/pvrsrv.c's general implementation.
 */
PVRSRV_ERROR PollForValueKM_SGXPower(volatile IMG_UINT32 *pui32LinMemAddr,
		IMG_UINT32 ui32Value, IMG_UINT32 ui32Mask)
{
	IMG_UINT32 uiOffset, uiStart, uiCurrent;
	IMG_INT32 iNotLastLoop;

	for (uiOffset = 0, uiStart = OSClockus(), uiCurrent = uiStart + 1,
			iNotLastLoop = 1;
			((uiCurrent - uiStart + uiOffset) < 500000) || iNotLastLoop--;
			uiCurrent = OSClockus(),
			uiOffset = uiCurrent < uiStart ? ~0U - uiStart : uiOffset,
			uiStart = uiCurrent < uiStart ? 0 : uiStart) {
		IMG_UINT32 ui32Start;

		if ((*pui32LinMemAddr & ui32Mask) == ui32Value)
			return PVRSRV_OK;
		/* SCE OSWaitus inline; the platform clock is in microseconds. */
		ui32Start = SceThreadmgrForDriver_47F6DE49();
		while ((IMG_UINT32)(SceThreadmgrForDriver_47F6DE49() - ui32Start) < 50)
			;
	}
	return PVRSRV_ERROR_TIMEOUT;
}

/* SCE passes the register mapping directly and omits IMG diagnostics/PDUMP. */
IMG_VOID SGXPollForClockGating(IMG_VOID *pvRegsBaseKM,
		IMG_UINT32 ui32Register, IMG_UINT32 ui32RegisterValue)
{
	PollForValueKM_SGXPower((IMG_UINT32 *)pvRegsBaseKM + (ui32Register >> 2),
			0, ui32RegisterValue);
}

/* Pristine timing update with the SCE inlined timer helper's 67000 numerator. */
PVRSRV_ERROR SGXUpdateTimingInfo(PVRSRV_DEVICE_NODE *psDeviceNode)
{
	PVRSRV_SGXDEV_INFO *psDevInfo = psDeviceNode->pvDevice;
	SGX_DEVICE_MAP *psSGXDeviceMap;
	SGX_TIMING_INFORMATION *psSGXTimingInfo;
	IMG_UINT32 ui32ActivePowManSampleRate;
	IMG_HANDLE hNewTimer;

	SysGetDeviceMemoryMap(PVRSRV_DEVICE_TYPE_SGX, &psSGXDeviceMap);
	psSGXTimingInfo = &psSGXDeviceMap->sTimingInfo;
	if (psDevInfo->hTimer != NULL) {
		if (psDevInfo->ui32CoreClockSpeed / psDevInfo->ui32uKernelTimerClock !=
		    psSGXTimingInfo->ui32uKernelFreq) {
			hNewTimer = OSAddTimer(SGXOSTimer, psDeviceNode,
					67000 / psSGXTimingInfo->ui32uKernelFreq);
			if (hNewTimer != NULL) {
				OSRemoveTimer(psDevInfo->hTimer);
				psDevInfo->hTimer = hNewTimer;
			}
		}
	} else {
		psDevInfo->hTimer = OSAddTimer(SGXOSTimer, psDeviceNode,
				67000 / psSGXTimingInfo->ui32uKernelFreq);
		if (psDevInfo->hTimer == NULL)
			return PVRSRV_ERROR_OUT_OF_MEMORY;
	}
	psDevInfo->psSGXHostCtl->ui32HWRecoverySampleRate =
		psSGXTimingInfo->ui32uKernelFreq / psSGXTimingInfo->ui32HWRecoveryFreq;
	psDevInfo->ui32CoreClockSpeed = psSGXTimingInfo->ui32CoreClockSpeed;
	psDevInfo->ui32uKernelTimerClock =
		psSGXTimingInfo->ui32CoreClockSpeed / psSGXTimingInfo->ui32uKernelFreq;
	psDevInfo->psSGXHostCtl->ui32uKernelTimerClock = psDevInfo->ui32uKernelTimerClock;
	if (psSGXTimingInfo->bEnableActivePM)
		ui32ActivePowManSampleRate = psSGXTimingInfo->ui32ActivePowManLatencyms *
			psSGXTimingInfo->ui32uKernelFreq / 1000 + 1;
	else
		ui32ActivePowManSampleRate = 0;
	psDevInfo->psSGXHostCtl->ui32ActivePowManSampleRate = ui32ActivePowManSampleRate;
	return PVRSRV_OK;
}

/* Pristine SGX543MP4 transition with SCE's direct-register poll interface. */
PVRSRV_ERROR SGXPrePowerState(IMG_HANDLE hDevHandle,
		PVRSRV_DEV_POWER_STATE eNewPowerState,
		PVRSRV_DEV_POWER_STATE eCurrentPowerState)
{
	PVRSRV_DEVICE_NODE *psDeviceNode;
	PVRSRV_SGXDEV_INFO *psDevInfo;
	PVRSRV_ERROR eError;
	IMG_UINT32 ui32PowerCmd, ui32CompleteStatus;
	IMG_UINT32 ui32Core, ui32CoresEnabled;
	SGXMKIF_COMMAND sCommand;

	if (eNewPowerState == eCurrentPowerState)
		return PVRSRV_OK;
	if (eNewPowerState == PVRSRV_DEV_POWER_STATE_ON)
		return (PVRSRV_ERROR)eNewPowerState;
	psDeviceNode = hDevHandle;
	psDevInfo = psDeviceNode->pvDevice;
	eError = OSDisableTimer(psDevInfo->hTimer);
	if (eError != PVRSRV_OK)
		return eError;
	if (eNewPowerState == PVRSRV_DEV_POWER_STATE_OFF) {
		ui32PowerCmd = 1;
		ui32CompleteStatus = 8;
	} else {
		ui32PowerCmd = 2;
		ui32CompleteStatus = 4;
	}
	memset(&sCommand, 0, sizeof(sCommand));
	sCommand.ui32Data[1] = ui32PowerCmd;
	eError = SGXScheduleCCBCommand(psDeviceNode, 3, &sCommand,
			0xffffffffU, 0, NULL, IMG_FALSE);
	if (eError != PVRSRV_OK)
		return eError;
	PollForValueKM_SGXPower(&psDevInfo->psSGXHostCtl->ui32PowerStatus,
			ui32CompleteStatus, ui32CompleteStatus);
	ui32CoresEnabled = (OSReadHWReg(psDevInfo->pvRegsBaseKM, 0x4000) & 3U) + 1;
	for (ui32Core = 0; ui32Core < ui32CoresEnabled; ui32Core++)
		SGXPollForClockGating(psDevInfo->pvRegsBaseKM,
				psDevInfo->ui32ClkGateStatusReg + ((ui32Core + 2) << 14),
				psDevInfo->ui32ClkGateStatusMask);
	SGXPollForClockGating(psDevInfo->pvRegsBaseKM,
			psDevInfo->ui32MasterClkGateStatusReg, psDevInfo->ui32MasterClkGateStatusMask);
	SGXPollForClockGating(psDevInfo->pvRegsBaseKM,
			psDevInfo->ui32MasterClkGateStatus2Reg, psDevInfo->ui32MasterClkGateStatus2Mask);
	if (eNewPowerState == PVRSRV_DEV_POWER_STATE_OFF) {
		eError = SGXDeinitialise(psDevInfo);
		if (eError != PVRSRV_OK)
			return eError;
	}
	return PVRSRV_OK;
}

/* Pristine resume and timer-start paths with SCE diagnostics omitted. */
PVRSRV_ERROR SGXPostPowerState(IMG_HANDLE hDevHandle,
		PVRSRV_DEV_POWER_STATE eNewPowerState,
		PVRSRV_DEV_POWER_STATE eCurrentPowerState)
{
	PVRSRV_DEVICE_NODE *psDeviceNode;
	PVRSRV_SGXDEV_INFO *psDevInfo;
	PVRSRV_ERROR eError;
	SGXMKIF_COMMAND sCommand;

	if (eNewPowerState == eCurrentPowerState)
		return PVRSRV_OK;
	if (eCurrentPowerState == PVRSRV_DEV_POWER_STATE_ON)
		return (PVRSRV_ERROR)eCurrentPowerState;
	psDeviceNode = hDevHandle;
	psDevInfo = psDeviceNode->pvDevice;
	psDevInfo->psSGXHostCtl->ui32PowerStatus = 0;
	if (eCurrentPowerState == PVRSRV_DEV_POWER_STATE_OFF) {
		eError = SGXUpdateTimingInfo(psDeviceNode);
		if (eError != PVRSRV_OK)
			return eError;
		eError = SGXInitialise(psDevInfo, IMG_FALSE);
	} else {
		memset(&sCommand, 0, sizeof(sCommand));
		sCommand.ui32Data[1] = 3;
		eError = SGXScheduleCCBCommand(psDeviceNode, 3, &sCommand,
				(IMG_UINT32)-3, 0, NULL, IMG_FALSE);
	}
	if (eError != PVRSRV_OK)
		return eError;
	OSEnableTimer(psDevInfo->hTimer);
	return PVRSRV_OK;
}

/* SCE: diagnostic and PDUMP calls are absent. */
PVRSRV_ERROR SGXPreClockSpeedChange(IMG_HANDLE hDevHandle,
		IMG_BOOL bIdleDevice, PVRSRV_DEV_POWER_STATE eCurrentPowerState)
{
	if (eCurrentPowerState != PVRSRV_DEV_POWER_STATE_ON)
		return PVRSRV_OK;
	if (bIdleDevice)
		return SGXPrePowerState(hDevHandle, PVRSRV_DEV_POWER_STATE_IDLE,
				PVRSRV_DEV_POWER_STATE_ON);
	return PVRSRV_OK;
}

PVRSRV_ERROR SGXPostClockSpeedChange(IMG_HANDLE hDevHandle,
		IMG_BOOL bIdleDevice, PVRSRV_DEV_POWER_STATE eCurrentPowerState)
{
	PVRSRV_DEVICE_NODE *psDeviceNode = hDevHandle;
	PVRSRV_SGXDEV_INFO *psDevInfo = psDeviceNode->pvDevice;

	if (eCurrentPowerState == PVRSRV_DEV_POWER_STATE_ON) {
		PVRSRV_ERROR eError;

		eError = SGXUpdateTimingInfo(psDeviceNode);
		if (eError != PVRSRV_OK)
			return eError;
		if (bIdleDevice) {
			eError = SGXPostPowerState(hDevHandle, PVRSRV_DEV_POWER_STATE_ON,
					PVRSRV_DEV_POWER_STATE_IDLE);
			if (eError != PVRSRV_OK)
				return eError;
		} else {
			/* SCE SGXStartTimer inline. */
			OSEnableTimer(psDevInfo->hTimer);
		}
	}
	return PVRSRV_OK;
}
