/* IMG sysconfig.c platform port; SCE native interrupt/environment ownership.
 * The pristine unified platform remains in the vendored kernel DDK.
 */
#include <sysmem/sysmem.h>
#include "gpu-dump.h"
#include <psp2kern/kernel/intrmgr.h>
#include <power/power.h>

PVRSRV_ERROR OSUninstallSystemLISR(IMG_VOID *pvSysData);
PVRSRV_ERROR OSUninstallMISR(IMG_VOID *pvSysData);
PVRSRV_ERROR PVRSRVDeinitialiseDevice(IMG_UINT32 ui32DevIndex);
PVRSRV_ERROR OSDeInitEnvData(IMG_VOID *pvEnvSpecificData);
IMG_VOID PVRSRVDeInit(SYS_DATA *psSysData);
PVRSRV_ERROR OSDestroyResource(PVRSRV_RESOURCE *psResource);
PVRSRV_ERROR OSInstallMISR(IMG_VOID *pvSysData);
PVRSRV_ERROR OSInstallSystemLISR(IMG_VOID *pvSysData, IMG_UINT32 ui32Irq);
IMG_VOID *OSMemSet(IMG_VOID *destination, IMG_INT value, IMG_SIZE_T size);
PVRSRV_ERROR OSInitEnvData(IMG_VOID **ppvEnvSpecificData);
PVRSRV_ERROR PVRSRVInit(SYS_DATA *psSysData);
PVRSRV_ERROR PVRSRVRegisterDevice(SYS_DATA *psSysData,
		PVRSRV_ERROR (*pfnRegisterDevice)(PVRSRV_DEVICE_NODE *),
		IMG_UINT32 ui32SOCInterruptBit, IMG_UINT32 *pui32DeviceIndex);
PVRSRV_ERROR SGXRegisterDevice(PVRSRV_DEVICE_NODE *psDeviceNode);
PVRSRV_ERROR PVRSRVInitialiseDevice(IMG_UINT32 ui32DevIndex);
int MapGpuRegisters(IMG_VOID);
IMG_VOID ScePervasiveForDriver_E8656ADF(IMG_VOID);
IMG_VOID ScePervasiveForDriver_84EAB5BC(IMG_VOID);

SYS_DATA gsSysData;

PVRSRV_ERROR SysDeinitialise(SYS_DATA *psSysData)
{
	PVRSRV_ERROR eError;
	SYS_DATA *psSystem;

	ksceKernelDisableIntr(gpuDumpRegion.state.platform.ui32IRQ);
	eError = OSUninstallSystemLISR(psSysData);
	if (eError != PVRSRV_OK)
		return eError;
	eError = OSUninstallMISR(psSysData);
	if (eError != PVRSRV_OK)
		return eError;
	eError = PVRSRVDeinitialiseDevice(gpuDumpRegion.state.deviceIndex);
	if (eError != PVRSRV_OK)
		return eError;
	eError = OSDeInitEnvData(gpuDumpRegion.state.system->pvEnvSpecificData);
	if (eError == PVRSRV_OK) {
		psSystem = gpuDumpRegion.state.system;
		PVRSRVDeInit(psSystem);
		OSDestroyResource(&psSystem->sPowerStateChangeResource);
		gpuDumpRegion.state.system = NULL;
	}
	return eError;
}

PVRSRV_ERROR SysFinalise(IMG_VOID)
{
	PVRSRV_ERROR eError;
	PVRSRV_ERROR eLISRError;

	eError = OSInstallMISR(gpuDumpRegion.state.system);
	if (eError != PVRSRV_OK) {
		SysDeinitialise(gpuDumpRegion.state.system);
		gpuDumpRegion.state.system = NULL;
		return eError;
	}
	eLISRError = OSInstallSystemLISR(gpuDumpRegion.state.system,
			gpuDumpRegion.state.platform.ui32IRQ);
	if (eLISRError != PVRSRV_OK) {
		OSUninstallMISR(gpuDumpRegion.state.system);
		SysDeinitialise(gpuDumpRegion.state.system);
		gpuDumpRegion.state.system = NULL;
	} else {
		ksceKernelEnableIntr(gpuDumpRegion.state.platform.ui32IRQ);
	}
	return eLISRError;
}

PVRSRV_ERROR SysInitialise(IMG_VOID)
{
	IMG_UINT32 ui32CoreClockMHz;
	PVRSRV_ERROR eError;
	PVRSRV_DEVICE_NODE *psDeviceNode;
	DEVICE_MEMORY_HEAP_INFO *psHeap;
	IMG_UINT32 i;
	SYS_DATA *psSysData;
	SGX_TIMING_INFORMATION *psTimingInfo;

	gpuDumpRegion.state.system = &gsSysData;
	OSMemSet(&gsSysData, 0, sizeof(gsSysData));
	eError = OSInitEnvData((IMG_VOID **)&gpuDumpRegion.state.system->pvEnvSpecificData);
	if (eError != PVRSRV_OK)
		goto failure;
	kscePowerGetGpuClockFrequencyInternal(&ui32CoreClockMHz, NULL);
	psTimingInfo = &gpuDumpRegion.state.platform.sTimingInfo;
	psTimingInfo->ui32uKernelFreq = 1000;
	psTimingInfo->ui32CoreClockSpeed = 1000000 * ui32CoreClockMHz;
	psTimingInfo->ui32HWRecoveryFreq = 10;
	psTimingInfo->bEnableActivePM = IMG_FALSE;
	psTimingInfo->ui32ActivePowManLatencyms = 1;
	gpuDumpRegion.state.system->ui32NumDevices = 3;
	psSysData = gpuDumpRegion.state.system;
	psSysData->sDeviceID[0].uiID = 0;
	psSysData->sDeviceID[0].bInUse = IMG_FALSE;
	psSysData->sDeviceID[1].uiID = 1;
	psSysData->sDeviceID[1].bInUse = IMG_FALSE;
	psSysData->sDeviceID[2].uiID = 2;
	psSysData->sDeviceID[2].bInUse = IMG_FALSE;
	psSysData->psDeviceNodeList = NULL;
	eError = PVRSRVInit(psSysData);
	if (eError != PVRSRV_OK)
		goto failure;
	eError = MapGpuRegisters();
	if (eError != PVRSRV_OK)
		goto failure;
	eError = PVRSRVRegisterDevice(gpuDumpRegion.state.system, SGXRegisterDevice,
			~0U, &gpuDumpRegion.state.deviceIndex);
	if (eError != PVRSRV_OK)
		goto failure;
	for (psDeviceNode = gpuDumpRegion.state.system->psDeviceNodeList;
			psDeviceNode; psDeviceNode = psDeviceNode->psNext) {
		if (psDeviceNode->sDevId.eDeviceType != PVRSRV_DEVICE_TYPE_SGX)
			return PVRSRV_ERROR_INIT_FAILURE;
		psHeap = psDeviceNode->sDevMemoryInfo.psDeviceMemoryHeap;
		for (i = 0; i < psDeviceNode->sDevMemoryInfo.ui32HeapCount;
				i++, psHeap++)
			psHeap->ui32Attribs |= 0x2000000;
	}
	eError = PVRSRVInitialiseDevice(gpuDumpRegion.state.deviceIndex);
	if (eError == PVRSRV_OK)
		return PVRSRV_OK;
failure:
	SysDeinitialise(gpuDumpRegion.state.system);
	gpuDumpRegion.state.system = NULL;
	return eError;
}

PVRSRV_ERROR SysGetDeviceMemoryMap(PVRSRV_DEVICE_TYPE eDeviceType,
		SGX_DEVICE_MAP **ppsDeviceMap)
{
	if (eDeviceType == PVRSRV_DEVICE_TYPE_SGX)
		*ppsDeviceMap = &gpuDumpRegion.state.platform;
	return PVRSRV_OK;
}

IMG_UINT32 SysGetInterruptSource(IMG_VOID)
{
	/* SCE dispatches the complete pending interrupt set. */
	return ~0U;
}

IMG_VOID SysClearInterrupts(SYS_DATA *psSysData, IMG_UINT32 ui32ClearBits)
{
}

PVRSRV_ERROR SysSystemPrePowerState(PVRSRV_SYS_POWER_STATE eNewSysPowerState)
{
	PVRSRV_SYS_POWER_STATE eCurrentPowerState =
			gpuDumpRegion.state.system->eCurrentPowerState;

	if (eNewSysPowerState != eCurrentPowerState &&
			eNewSysPowerState == PVRSRV_SYS_POWER_STATE_D3 &&
			eCurrentPowerState <= PVRSRV_SYS_POWER_STATE_D2) {
		ksceKernelDisableIntr(gpuDumpRegion.state.platform.ui32IRQ);
		ksceKernelFreeMemBlock(gpuDumpRegion.state.registerMemblockUid);
	}
	return PVRSRV_OK;
}

PVRSRV_ERROR SysSystemPostPowerState(PVRSRV_SYS_POWER_STATE eNewSysPowerState)
{
	PVRSRV_SYS_POWER_STATE eCurrentPowerState =
			gpuDumpRegion.state.system->eCurrentPowerState;
	PVRSRV_ERROR eError;

	if (eNewSysPowerState == eCurrentPowerState ||
			eCurrentPowerState != PVRSRV_SYS_POWER_STATE_D3 ||
			eNewSysPowerState > PVRSRV_SYS_POWER_STATE_D2)
		return PVRSRV_OK;
	eError = MapGpuRegisters();
	if (eError == PVRSRV_OK)
		ksceKernelEnableIntr(gpuDumpRegion.state.platform.ui32IRQ);
	return eError;
}

PVRSRV_ERROR SysDevicePrePowerState(IMG_UINT32 ui32DeviceIndex,
		PVRSRV_DEV_POWER_STATE eNewPowerState,
		PVRSRV_DEV_POWER_STATE eCurrentPowerState)
{
	if (ui32DeviceIndex == gpuDumpRegion.state.deviceIndex &&
			eNewPowerState != eCurrentPowerState &&
			eNewPowerState == PVRSRV_DEV_POWER_STATE_OFF)
		ScePervasiveForDriver_E8656ADF();
	return PVRSRV_OK;
}

PVRSRV_ERROR SysDevicePostPowerState(IMG_UINT32 ui32DeviceIndex,
		PVRSRV_DEV_POWER_STATE eNewPowerState,
		PVRSRV_DEV_POWER_STATE eCurrentPowerState)
{
	if (ui32DeviceIndex == gpuDumpRegion.state.deviceIndex &&
			eNewPowerState != eCurrentPowerState &&
			eCurrentPowerState == PVRSRV_DEV_POWER_STATE_OFF)
		ScePervasiveForDriver_84EAB5BC();
	return PVRSRV_OK;
}
