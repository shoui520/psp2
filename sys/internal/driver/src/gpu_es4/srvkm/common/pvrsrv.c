/* IMG common/pvrsrv.c base; native SCE deltas are marked locally. */
#include <gpu/device_sce.h>
#include "../gpu-dump.h"
#include <gpu/power_sce.h>
#include <gpu/gpu_es4_trace.h>
#include <acmgr/acmgr.h>
#include <sysmem/sblaimgr.h>
#include <lowio/pervasive.h>
#include "sgx543_v1.164defs.h"
#include <string.h>
#include <stdarg.h>
#include <stddef.h>
#include <sysmem/sysmem.h>
#include <sysmem/sysroot.h>
#include <power/power.h>
#include <gpu/gpu-mmu.h>
#include <gpu/osfunc_sce.h>
#include <gpu/perproc_sce.h>
#include "../handle-types.h"
#include "../mapped_memory_sce.h"
#include <gpu/resman_sce.h>

static IMG_UINT32 g_ui32InitFlags = 0;
#define INIT_DATA_ENABLE_PDUMPINIT 0x1U

PVRSRV_ERROR PVRSRVFinaliseSystem_CompatCheck_AnyCb(PVRSRV_DEVICE_NODE *psDeviceNode);
int SignalWaitCounter(void);
void sceGpuWaitModObjPollAll(void);

/* SCE directory-bank selection, in the native trace slot order. */
static const IMG_UINT32 aui32TraceDirectoryRegisters[8] = {
	EUR_CR_BIF_DIR_LIST_BASE0, EUR_CR_BIF_DIR_LIST_BASE1,
	EUR_CR_BIF_DIR_LIST_BASE2, EUR_CR_BIF_DIR_LIST_BASE3,
	EUR_CR_BIF_DIR_LIST_BASE4, EUR_CR_BIF_DIR_LIST_BASE5,
	EUR_CR_BIF_DIR_LIST_BASE6, EUR_CR_BIF_DIR_LIST_BASE7,
};

/* Pristine IMG enumeration callback; device enum uses the SCE native ABI. */
IMG_VOID PVRSRVEnumerateDevicesKM_ForEachVaCb(PVRSRV_DEVICE_NODE *psDeviceNode,
		va_list va)
{
	IMG_UINT *pui32DevCount;
	PVRSRV_DEVICE_IDENTIFIER **ppsDevIdList;

	pui32DevCount = va_arg(va, IMG_UINT *);
	ppsDevIdList = va_arg(va, PVRSRV_DEVICE_IDENTIFIER **);
	if (psDeviceNode->sDevId.eDeviceType != PVRSRV_DEVICE_TYPE_EXT) {
		*(*ppsDevIdList) = psDeviceNode->sDevId;
		(*ppsDevIdList)++;
		(*pui32DevCount)++;
	}
}

/* Pristine IMG device selection callback. */
IMG_VOID *PVRSRVAcquireDeviceDataKM_Match_AnyVaCb(PVRSRV_DEVICE_NODE *psDeviceNode,
		va_list va)
{
	PVRSRV_DEVICE_TYPE eDeviceType;
	IMG_UINT32 ui32DevIndex;

	eDeviceType = va_arg(va, PVRSRV_DEVICE_TYPE);
	ui32DevIndex = va_arg(va, IMG_UINT32);
	if ((eDeviceType != PVRSRV_DEVICE_TYPE_UNKNOWN &&
			psDeviceNode->sDevId.eDeviceType == eDeviceType) ||
			(eDeviceType == PVRSRV_DEVICE_TYPE_UNKNOWN &&
			 psDeviceNode->sDevId.ui32DeviceIndex == ui32DevIndex))
		return psDeviceNode;
	return NULL;
}

/* Pristine IMG system interrupt callback. */
IMG_VOID PVRSRVSystemLISR_ForEachVaCb(PVRSRV_DEVICE_NODE *psDeviceNode, va_list va)
{
	IMG_BOOL *pbStatus;
	IMG_UINT32 *pui32InterruptSource;
	IMG_UINT32 *pui32ClearInterrupts;

	pbStatus = va_arg(va, IMG_BOOL *);
	pui32InterruptSource = va_arg(va, IMG_UINT32 *);
	pui32ClearInterrupts = va_arg(va, IMG_UINT32 *);
	if (psDeviceNode->pfnDeviceISR != NULL) {
		if (*pui32InterruptSource & psDeviceNode->ui32SOCInterruptBit) {
			if (psDeviceNode->pfnDeviceISR(psDeviceNode->pvISRData))
				*pbStatus = IMG_TRUE;
			*pui32ClearInterrupts |= psDeviceNode->ui32SOCInterruptBit;
		}
	}
}

/* Pristine IMG deferred-interrupt callback. */
IMG_VOID PVRSRVMISR_ForEachCb(PVRSRV_DEVICE_NODE *psDeviceNode)
{
	if (psDeviceNode->pfnDeviceMISR != NULL)
		psDeviceNode->pfnDeviceMISR(psDeviceNode->pvISRData);
}

/* Pristine IMG command-completion callback. */
IMG_VOID PVRSRVCommandCompleteCallbacks_ForEachCb(PVRSRV_DEVICE_NODE *psDeviceNode)
{
	if (psDeviceNode->pfnDeviceCommandComplete != NULL)
		psDeviceNode->pfnDeviceCommandComplete(psDeviceNode);
}

/* IMG finalise callback; SCE omits the failure diagnostic. */
PVRSRV_ERROR PVRSRVFinaliseSystem_SetPowerState_AnyCb(PVRSRV_DEVICE_NODE *psDeviceNode)
{
	PVRSRV_ERROR eError;

	eError = PVRSRVSetDevicePowerStateKM(psDeviceNode->sDevId.ui32DeviceIndex,
			PVRSRV_DEV_POWER_STATE_DEFAULT, 0xffffffffU, IMG_FALSE);
	return eError;
}

/* IMG ID allocation; SCE omits failure diagnostics and assertions. */
PVRSRV_ERROR AllocateDeviceID(SYS_DATA *psSysData, IMG_UINT32 *pui32DevID)
{
	SYS_DEVICE_ID *psDeviceWalker;
	SYS_DEVICE_ID *psDeviceEnd;

	psDeviceWalker = &psSysData->sDeviceID[0];
	psDeviceEnd = psDeviceWalker + psSysData->ui32NumDevices;
	while (psDeviceWalker < psDeviceEnd) {
		if (!psDeviceWalker->bInUse) {
			psDeviceWalker->bInUse = IMG_TRUE;
			*pui32DevID = psDeviceWalker->uiID;
			return PVRSRV_OK;
		}
		psDeviceWalker++;
	}
	return PVRSRV_ERROR_NO_FREE_DEVICEIDS_AVALIABLE;
}

/* IMG ID release; SCE omits failure diagnostics and assertions. */
PVRSRV_ERROR FreeDeviceID(SYS_DATA *psSysData, IMG_UINT32 ui32DevID)
{
	SYS_DEVICE_ID *psDeviceWalker;
	SYS_DEVICE_ID *psDeviceEnd;

	psDeviceWalker = &psSysData->sDeviceID[0];
	psDeviceEnd = psDeviceWalker + psSysData->ui32NumDevices;
	while (psDeviceWalker < psDeviceEnd) {
		if (psDeviceWalker->uiID == ui32DevID && psDeviceWalker->bInUse) {
			psDeviceWalker->bInUse = IMG_FALSE;
			return PVRSRV_OK;
		}
		psDeviceWalker++;
	}
	return PVRSRV_ERROR_INVALID_DEVICEID;
}

/* IMG enumeration; SCE has one device slot and omits diagnostics. */
PVRSRV_ERROR PVRSRVEnumerateDevicesKM(IMG_UINT32 *pui32NumDevices,
		PVRSRV_DEVICE_IDENTIFIER *psDevIdList)
{
	SYS_DATA *psSysData;

	if (!pui32NumDevices)
		goto invalid;
	if (!psDevIdList)
		goto invalid;
	psSysData = gpsSysData;
	psDevIdList[0].eDeviceType = PVRSRV_DEVICE_TYPE_UNKNOWN;
	*pui32NumDevices = 0;
	List_PVRSRV_DEVICE_NODE_ForEach_va(psSysData->psDeviceNodeList,
			PVRSRVEnumerateDevicesKM_ForEachVaCb, pui32NumDevices, &psDevIdList);
	return PVRSRV_OK;
invalid:
	return PVRSRV_ERROR_INVALID_PARAMS;
}

/* IMG teardown; SCE omits shared-workaround, event, PDUMP and diagnostics. */
IMG_VOID PVRSRVDeInit(SYS_DATA *psSysData)
{
	if (psSysData == NULL)
		return;
	PVRSRVHandleDeInit();
	PVRSRVPerProcessDataDeInit();
	ResManDeInit();
}

/* IMG init; SCE omits shared-workaround, global events, timers and tracing. */
PVRSRV_ERROR PVRSRVInit(SYS_DATA *psSysData)
{
	PVRSRV_ERROR eError;

	eError = ResManInit();
	if (eError != PVRSRV_OK)
		goto Error;
	eError = PVRSRVPerProcessDataInit();
	if (eError != PVRSRV_OK)
		goto Error;
	eError = PVRSRVHandleInit();
	if (eError != PVRSRV_OK)
		goto Error;
	eError = OSCreateResource(&psSysData->sPowerStateChangeResource);
	if (eError != PVRSRV_OK)
		goto Error;
	psSysData->eCurrentPowerState = PVRSRV_SYS_POWER_STATE_D0;
	psSysData->eFailedPowerState = PVRSRV_SYS_POWER_STATE_Unspecified;
	g_ui32InitFlags |= INIT_DATA_ENABLE_PDUMPINIT;
	return eError;
Error:
	PVRSRVDeInit(psSysData);
	return eError;
}

/* IMG device registration; SCE omits diagnostics and assertions. */
PVRSRV_ERROR PVRSRVRegisterDevice(SYS_DATA *psSysData,
		PVRSRV_ERROR (*pfnRegisterDevice)(PVRSRV_DEVICE_NODE *),
		IMG_UINT32 ui32SOCInterruptBit, IMG_UINT32 *pui32DeviceIndex)
{
	PVRSRV_ERROR eError;
	PVRSRV_DEVICE_NODE *psDeviceNode;

	if (OSAllocMem_Impl(2, sizeof(PVRSRV_DEVICE_NODE),
			(IMG_VOID **)&psDeviceNode, NULL) != PVRSRV_OK)
		return PVRSRV_ERROR_OUT_OF_MEMORY;
	OSMemSet(psDeviceNode, 0, sizeof(PVRSRV_DEVICE_NODE));
	eError = pfnRegisterDevice(psDeviceNode);
	if (eError != PVRSRV_OK) {
		OSFreeMem_Impl(2, sizeof(PVRSRV_DEVICE_NODE), psDeviceNode, NULL);
		return PVRSRV_ERROR_DEVICE_REGISTER_FAILED;
	}
	psDeviceNode->ui32RefCount = 1;
	psDeviceNode->psSysData = psSysData;
	psDeviceNode->ui32SOCInterruptBit = ui32SOCInterruptBit;
	AllocateDeviceID(psSysData, &psDeviceNode->sDevId.ui32DeviceIndex);
	List_PVRSRV_DEVICE_NODE_Insert(&psSysData->psDeviceNodeList, psDeviceNode);
	*pui32DeviceIndex = psDeviceNode->sDevId.ui32DeviceIndex;
	return PVRSRV_OK;
}

/* IMG device initialization; SCE omits diagnostics and the refcount assertion. */
PVRSRV_ERROR PVRSRVInitialiseDevice(IMG_UINT32 ui32DevIndex)
{
	PVRSRV_DEVICE_NODE *psDeviceNode;
	SYS_DATA *psSysData;
	PVRSRV_ERROR eError;

	psSysData = gpsSysData;
	psDeviceNode = (PVRSRV_DEVICE_NODE *)List_PVRSRV_DEVICE_NODE_Any_va(
			psSysData->psDeviceNodeList, MatchDeviceKM_AnyVaCb,
			ui32DevIndex, IMG_TRUE);
	if (!psDeviceNode)
		return PVRSRV_ERROR_INIT_FAILURE;
	eError = PVRSRVResManConnect(NULL, &psDeviceNode->hResManContext);
	if (eError != PVRSRV_OK)
		return eError;
	if (psDeviceNode->pfnInitDevice != NULL) {
		eError = psDeviceNode->pfnInitDevice(psDeviceNode);
		if (eError != PVRSRV_OK)
			return eError;
	}
	return PVRSRV_OK;
}

/* IMG system finalization; SCE omits PDUMP and diagnostics. */
PVRSRV_ERROR PVRSRVFinaliseSystem(IMG_BOOL bInitSuccessful)
{
	SYS_DATA *psSysData;
	PVRSRV_ERROR eError;

	psSysData = gpsSysData;
	if (bInitSuccessful) {
		eError = SysFinalise();
		if (eError != PVRSRV_OK)
			return eError;
		eError = List_PVRSRV_DEVICE_NODE_PVRSRV_ERROR_Any(
				psSysData->psDeviceNodeList, PVRSRVFinaliseSystem_SetPowerState_AnyCb);
		if (eError != PVRSRV_OK)
			return eError;
		eError = List_PVRSRV_DEVICE_NODE_PVRSRV_ERROR_Any(
				psSysData->psDeviceNodeList, PVRSRVFinaliseSystem_CompatCheck_AnyCb);
		if (eError != PVRSRV_OK)
			return eError;
	}
	return PVRSRV_OK;
}

/* Pristine IMG device compatibility hook. */
PVRSRV_ERROR PVRSRVDevInitCompatCheck(PVRSRV_DEVICE_NODE *psDeviceNode)
{
	if (psDeviceNode->pfnInitDeviceCompatCheck)
		return psDeviceNode->pfnInitDeviceCompatCheck(psDeviceNode);
	return PVRSRV_OK;
}

/* IMG compatibility iterator; SCE omits the failure diagnostic. */
PVRSRV_ERROR PVRSRVFinaliseSystem_CompatCheck_AnyCb(PVRSRV_DEVICE_NODE *psDeviceNode)
{
	PVRSRV_ERROR eError;

	eError = PVRSRVDevInitCompatCheck(psDeviceNode);
	return eError;
}

/* IMG device acquisition; SCE omits diagnostics and the refcount assertion. */
PVRSRV_ERROR PVRSRVAcquireDeviceDataKM(IMG_UINT32 ui32DevIndex,
		PVRSRV_DEVICE_TYPE eDeviceType, IMG_HANDLE *phDevCookie)
{
	PVRSRV_DEVICE_NODE *psDeviceNode;
	SYS_DATA *psSysData;

	psSysData = gpsSysData;
	psDeviceNode = List_PVRSRV_DEVICE_NODE_Any_va(psSysData->psDeviceNodeList,
			PVRSRVAcquireDeviceDataKM_Match_AnyVaCb, eDeviceType, ui32DevIndex);
	if (!psDeviceNode)
		return PVRSRV_ERROR_INIT_FAILURE;
	if (phDevCookie)
		*phDevCookie = (IMG_HANDLE)psDeviceNode;
	return PVRSRV_OK;
}

/* IMG device teardown; SCE omits diagnostics. */
PVRSRV_ERROR PVRSRVDeinitialiseDevice(IMG_UINT32 ui32DevIndex)
{
	PVRSRV_DEVICE_NODE *psDeviceNode;
	SYS_DATA *psSysData;
	PVRSRV_ERROR eError;

	psSysData = gpsSysData;
	psDeviceNode = (PVRSRV_DEVICE_NODE *)List_PVRSRV_DEVICE_NODE_Any_va(
			psSysData->psDeviceNodeList, MatchDeviceKM_AnyVaCb,
			ui32DevIndex, IMG_TRUE);
	if (!psDeviceNode)
		return PVRSRV_ERROR_DEVICEID_NOT_FOUND;
	eError = PVRSRVSetDevicePowerStateKM(ui32DevIndex,
			PVRSRV_DEV_POWER_STATE_OFF, 0xffffffffU, IMG_FALSE);
	if (eError != PVRSRV_OK)
		return eError;
	eError = ResManFreeResByCriteria(psDeviceNode->hResManContext,
			RESMAN_CRITERIA_RESTYPE, RESMAN_TYPE_DEVICEMEM_ALLOCATION, NULL, 0);
	if (eError != PVRSRV_OK)
		return eError;
	if (psDeviceNode->pfnDeInitDevice != NULL) {
		eError = psDeviceNode->pfnDeInitDevice(psDeviceNode);
		if (eError != PVRSRV_OK)
			return eError;
	}
	PVRSRVResManDisconnect(psDeviceNode->hResManContext, IMG_TRUE);
	psDeviceNode->hResManContext = NULL;
	List_PVRSRV_DEVICE_NODE_Remove(psDeviceNode);
	FreeDeviceID(psSysData, ui32DevIndex);
	OSFreeMem_Impl(2, sizeof(PVRSRV_DEVICE_NODE), psDeviceNode, NULL);
	return PVRSRV_OK;
}

/* IMG non-emulator poll and pristine timeout macro; SCE omits diagnostics. */
PVRSRV_ERROR PollForValueKM(volatile IMG_UINT32 *pui32LinMemAddr,
		IMG_UINT32 ui32Value, IMG_UINT32 ui32Mask, IMG_UINT32 ui32Timeoutus,
		IMG_UINT32 ui32PollPeriodus, IMG_BOOL bAllowPreemption)
{
	IMG_UINT32 ui32ActualValue = 0xffffffffU;
	IMG_UINT32 uiOffset, uiStart, uiCurrent;
	IMG_INT32 iNotLastLoop;

	for (uiOffset = 0, uiStart = OSClockus(), uiCurrent = uiStart + 1,
			iNotLastLoop = 1;
			((uiCurrent - uiStart + uiOffset) < ui32Timeoutus) || iNotLastLoop--;
			uiCurrent = OSClockus(),
			uiOffset = uiCurrent < uiStart ? ~0U - uiStart : uiOffset,
			uiStart = uiCurrent < uiStart ? 0 : uiStart) {
		ui32ActualValue = *pui32LinMemAddr & ui32Mask;
		if (ui32ActualValue == ui32Value)
			return PVRSRV_OK;
		if (bAllowPreemption)
			OSSleepms(ui32PollPeriodus / 1000);
		else
			OSWaitus(ui32PollPeriodus);
	}
	return PVRSRV_ERROR_TIMEOUT;
}

/* IMG version-string query. SCE accepts only its native request mask and
 * retains just this operation; the original reports 1.7 branch17 build5029,
 * distinct from the pristine candidate release used as the source base.
 */
PVRSRV_ERROR PVRSRVGetMiscInfoKM(PVRSRV_MISC_INFO *psMiscInfo)
{
	if (!psMiscInfo)
		return PVRSRV_ERROR_INVALID_PARAMS;
	psMiscInfo->ui32StatePresent = 0;
	if (psMiscInfo->ui32StateRequest & ~0x98U)
		return PVRSRV_ERROR_INVALID_PARAMS;
	if ((psMiscInfo->ui32StateRequest & 0x10U) && psMiscInfo->pszMemoryStr) {
		IMG_CHAR *pszStr;
		IMG_UINT32 ui32StrLen;
		IMG_UINT32 ui32LenStrPerNum = 12;
		IMG_INT32 i32Count;
		IMG_INT i;

		psMiscInfo->ui32StatePresent |= 0x10U;
		psMiscInfo->aui32DDKVersion[0] = 1;
		psMiscInfo->aui32DDKVersion[1] = 7;
		psMiscInfo->aui32DDKVersion[2] = 17;
		psMiscInfo->aui32DDKVersion[3] = 5029;
		pszStr = psMiscInfo->pszMemoryStr;
		ui32StrLen = psMiscInfo->ui32MemoryStrLen;
		for (i = 0; i < 4; i++) {
			if (ui32StrLen < ui32LenStrPerNum)
				return PVRSRV_ERROR_INVALID_PARAMS;
			i32Count = OSSNPrintf(pszStr, ui32LenStrPerNum, "%u",
					psMiscInfo->aui32DDKVersion[i]);
			if (i32Count == -1)
				return PVRSRV_ERROR_INVALID_PARAMS;
			pszStr += i32Count;
			ui32StrLen -= i32Count;
			if (i != 3) {
				i32Count = OSSNPrintf(pszStr, 2, ".");
				if (i32Count == -1)
					return PVRSRV_ERROR_INVALID_PARAMS;
				pszStr += i32Count;
				ui32StrLen -= i32Count;
			}
		}
	}
	return PVRSRV_OK;
}

/* SCE TOOL-only clock and wireless trace snapshot. */
int SceGpuEs4ForDriver_990338F6(GpuTraceInfo *info)
{
	SceUInt32 first, second;

	if (info == NULL)
		return PVRSRV_ERROR_INVALID_PARAMS;
	if (SceSblAIMgrForDriver_274663A0() && SceSblACMgrForDriver_E87D1777()) {
		info->socRevision = ScePervasiveForDriver_714EEFB7() & 0x1ffffU;
		scePowerGetGpuClockFrequencyInternal(&first, &second);
		info->gpuClockFrequency[0] = first * 1000000U;
		info->gpuClockFrequency[1] = second * 1000000U;
		info->gpuXbarClockFrequency = scePowerGetGpuXbarClockFrequency() * 1000000U;
		info->armClockFrequency = scePowerGetArmClockFrequency() * 1000000U;
		info->timebaseFrequency = SceSysrootForDriver_BFD8F2A2() * 1000000U;
		info->sysClockFrequency = ScePowerForDriver_C63DACD5() * 1000000U;
		info->busClockFrequency = scePowerGetBusClockFrequency() * 1000000U;
		if (info->gpuClockFrequency[0] == 166000000U)
			info->gpuClockFrequency[0] = 166500000U;
		if (info->gpuClockFrequency[1] == 166000000U)
			info->gpuClockFrequency[1] = 166500000U;
		if (info->gpuXbarClockFrequency == 166000000U)
			info->gpuXbarClockFrequency = 166500000U;
		info->wirelessState = ScePowerForDriver_06AF03DB(0);
	}
	return PVRSRV_OK;
}

/* SCE TOOL-only directory-bank process identifiers and names. */
int SceGpuEs4ForDriver_C0C73C1B(GpuTraceCounters *names)
{
	PVRSRV_DEVICE_NODE *psDeviceNode = GetGpuDeviceNode();
	PVRSRV_SGXDEV_INFO *psDevInfo;
	IMG_UINT32 i;
	unsigned pid;
	const char *name;

	if (psDeviceNode == NULL || psDeviceNode->pvDevice == NULL || names == NULL)
		return PVRSRV_ERROR_INVALID_PARAMS;
	psDevInfo = psDeviceNode->pvDevice;
	if (SceSblAIMgrForDriver_274663A0() && SceSblACMgrForDriver_E87D1777()) {
		OSMemSet(names, 0, sizeof(*names));
		for (i = 0; i < 8; i++) {
			IMG_UINT32 physical = OSReadHWReg(psDevInfo->pvRegsBaseKM,
					aui32TraceDirectoryRegisters[i] + 0x8000U);

			pid = (unsigned)-1;
			if (SceGpuEs4ForDriver_7AA79658(physical, &pid) == 0 &&
					(SceUID)pid > 0) {
				names->id[i] = pid;
				name = SceSysmemForDriver_E655852F(pid);
				if (name != NULL) {
					strncpy(names->name[i], name, 31);
					names->name[i][31] = 0;
				}
			} else {
				names->id[i] = -1;
				names->name[i][0] = 0;
			}
		}
	}
	return PVRSRV_OK;
}

/* Pristine IMG LISR; SCE interrupt-source query takes no arguments. */
IMG_BOOL PVRSRVSystemLISR(IMG_VOID *pvSysData)
{
	SYS_DATA *psSysData = pvSysData;
	IMG_BOOL bStatus = IMG_FALSE;
	IMG_UINT32 ui32InterruptSource;
	IMG_UINT32 ui32ClearInterrupts = 0;

	if (psSysData != NULL) {
		ui32InterruptSource = SysGetInterruptSource();
		if (ui32InterruptSource) {
			List_PVRSRV_DEVICE_NODE_ForEach_va(psSysData->psDeviceNodeList,
					PVRSRVSystemLISR_ForEachVaCb, &bStatus,
					&ui32InterruptSource, &ui32ClearInterrupts);
			SysClearInterrupts(psSysData, ui32ClearInterrupts);
		}
	}
	return bStatus;
}

/* SCE replaces pristine queue/event processing with native sync wait wakes. */
IMG_VOID PVRSRVMISR(IMG_VOID *pvSysData)
{
	SYS_DATA *psSysData = pvSysData;

	if (psSysData == NULL)
		return;
	List_PVRSRV_DEVICE_NODE_ForEach(psSysData->psDeviceNodeList,
			PVRSRVMISR_ForEachCb);
	SignalWaitCounter();
	sceGpuWaitModObjPollAll();
}

/* Pristine IMG lifecycle wrappers. */
PVRSRV_ERROR PVRSRVProcessConnect(IMG_UINT32 ui32PID, IMG_UINT32 ui32Flags)
{
	return PVRSRVPerProcessDataConnect(ui32PID, ui32Flags);
}

IMG_VOID PVRSRVProcessDisconnect(IMG_UINT32 ui32PID)
{
	PVRSRVPerProcessDataDisconnect(ui32PID);
}

IMG_VOID PVRSRVScheduleDeviceCallbacks(IMG_VOID)
{
	SYS_DATA *psSysData;

	/* SCE retains the upstream inline load without its diagnostic check. */
	psSysData = gpsSysData;
	List_PVRSRV_DEVICE_NODE_ForEach(psSysData->psDeviceNodeList,
			PVRSRVCommandCompleteCallbacks_ForEachCb);
}
