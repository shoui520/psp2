/* IMG devices/sgx/sgxinit.c, GPL-2.0. Pristine candidate and license are in
 * contrib/img-sgx-ddk-1.7.862890; SCE port changes are marked below.
 */
#include "sgxinit_sce.h"
#include "memory_sce.h"
#include "sgx543_v1.164defs.h"
#include <stddef.h>
#include <string.h>
#include <sysmem/debug.h>
#include <gpu/sgxmkif_sce.h>
#include <psp2kern/kernel/threadmgr.h>
#include <sysmem/sblaimgr.h>
#include <acmgr/acmgr.h>
#include <lowio/pervasive.h>
#include <power/power.h>
#include <processmgr/processmgr.h>
#include <sysmem/sysroot.h>
#include <display/display.h>

#define SGXMKIF_CC_INVAL_BIF_SL 0x4U
#define DEVICE_MEMORY_HEAP_KERNEL 1
#define DEVICE_MEMORY_HEAP_SHARED 2
#define DEVICE_MEMORY_HEAP_SHARED_EXPORTED 3
/* SCE build identity, distinct from the pristine candidate's version. */
#define SGX_BUILD_OPTIONS 0x30322518U
#define PVRVERSION_BUILD 5029
#define PVRSRV_USSE_MISCINFO_READY 1U
#define PVRSRV_USSE_MISCINFO_GET_STRUCT_SIZES 2U
#define SGXMKIF_CMD_GETMISCINFO 6
#define KERNEL_ID 0xffffffffU

PVRSRV_ERROR SysGetDeviceMemoryMap(PVRSRV_DEVICE_TYPE eDeviceType,
		SGX_DEVICE_MAP **ppsDeviceMap);
IMG_VOID PVRSRVDevicePostClockSpeedChange(IMG_UINT32 ui32DeviceIndex,
		IMG_BOOL bIdleDevice, IMG_VOID *pvInfo);
PVRSRV_ERROR PVRSRVDevicePreClockSpeedChange(IMG_UINT32 ui32DeviceIndex,
		IMG_BOOL bIdleDevice, IMG_VOID *pvInfo);
PVRSRV_ERROR SGXScheduleProcessQueuesKM(PVRSRV_DEVICE_NODE *psDeviceNode);
PVRSRV_ERROR PVRSRVPowerLock(IMG_UINT32 ui32CallerID, IMG_BOOL bSystemPowerEvent);
IMG_VOID PVRSRVPowerUnlock(IMG_UINT32 ui32CallerID);
IMG_BOOL SGXIsDevicePowered(PVRSRV_DEVICE_NODE *psDeviceNode);
IMG_UINT32 OSReadHWReg(IMG_VOID *pvLinRegBaseAddr, IMG_UINT32 ui32Offset);
IMG_VOID OSWriteHWReg(IMG_VOID *pvLinRegBaseAddr, IMG_UINT32 ui32Offset,
		IMG_UINT32 ui32Value);
PVRSRV_ERROR OSAllocMem_Impl(IMG_UINT32 ui32Flags, IMG_SIZE_T ui32Size,
		IMG_VOID **ppvLinAddr, IMG_HANDLE *phBlockAlloc);
IMG_VOID OSMemSet(IMG_VOID *pvDest, IMG_UINT8 ui8Value, IMG_SIZE_T ui32Size);
IMG_HANDLE BM_CreateContext(PVRSRV_DEVICE_NODE *psDeviceNode,
		IMG_DEV_PHYADDR *psPDDevPAddr, PVRSRV_PER_PROCESS_DATA *psPerProc,
		IMG_BOOL *pbCreated);
IMG_HANDLE BM_CreateHeap(IMG_HANDLE hBMContext,
		DEVICE_MEMORY_HEAP_INFO *psDevMemHeapInfo);
PVRSRV_ERROR MMU_BIFResetPDAlloc(PVRSRV_SGXDEV_INFO *psDevInfo);
IMG_VOID MMU_BIFResetPDFree(PVRSRV_SGXDEV_INFO *psDevInfo);
PVRSRV_ERROR OSRemoveTimer(IMG_HANDLE hTimer);
IMG_VOID OSFreeMem_Impl(IMG_UINT32 ui32Flags, IMG_SIZE_T ui32Size,
		IMG_VOID *pvLinAddr, IMG_HANDLE hBlockAlloc);
PVRSRV_ERROR BM_DestroyContext(IMG_HANDLE hBMContext, IMG_BOOL *pbDestroyed);
IMG_VOID BM_DestroyHeap(IMG_HANDLE hDevMemHeap);
PVRSRV_ERROR PVRSRVRemovePowerDevice(IMG_UINT32 ui32DeviceIndex);
IMG_UINT32 OSClockus(IMG_VOID);
IMG_VOID SGXReset(PVRSRV_SGXDEV_INFO *psDevInfo, IMG_BOOL bHardwareRecovery,
		IMG_UINT32 ui32PDUMPFlags);
PVRSRV_ERROR PollForValueKM(volatile IMG_UINT32 *pui32LinMemAddr,
		IMG_UINT32 ui32Value, IMG_UINT32 ui32Mask, IMG_UINT32 ui32Timeoutus,
		IMG_UINT32 ui32PollPeriodus, IMG_BOOL bAllowPreemption);
IMG_UINT32 OSGetCurrentProcessIDKM(IMG_VOID);
IMG_VOID *OSMemCopy(IMG_VOID *pvDest, const IMG_VOID *pvSrc, IMG_SIZE_T ui32Size);
IMG_VOID VBAppend(const IMG_CHAR *pszFormat, ...);
IMG_VOID SGX_MISRHandler(IMG_VOID *pvData);
SceUID GetLockupEventUid(void);
IMG_VOID SGXTestActivePowerEvent(PVRSRV_DEVICE_NODE *psDeviceNode,
		IMG_UINT32 ui32CallerID);
PVRSRV_ERROR SGXPrePowerState(IMG_HANDLE hDevHandle,
		PVRSRV_DEV_POWER_STATE eNewPowerState, PVRSRV_DEV_POWER_STATE eCurrentPowerState);
PVRSRV_ERROR SGXPostPowerState(IMG_HANDLE hDevHandle,
		PVRSRV_DEV_POWER_STATE eNewPowerState, PVRSRV_DEV_POWER_STATE eCurrentPowerState);
PVRSRV_ERROR SGXPreClockSpeedChange(IMG_HANDLE hDevHandle, IMG_BOOL bIdleDevice,
		PVRSRV_DEV_POWER_STATE eCurrentPowerState);
PVRSRV_ERROR SGXPostClockSpeedChange(IMG_HANDLE hDevHandle, IMG_BOOL bIdleDevice,
		PVRSRV_DEV_POWER_STATE eCurrentPowerState);
PVRSRV_ERROR PVRSRVRegisterPowerDevice(IMG_UINT32 ui32DeviceIndex,
		PFN_PRE_POWER pfnPrePower, PFN_POST_POWER pfnPostPower,
		PFN_PRE_CLOCKSPEED_CHANGE pfnPreClockSpeedChange,
		PFN_POST_CLOCKSPEED_CHANGE pfnPostClockSpeedChange, IMG_HANDLE hDevCookie,
		PVRSRV_DEV_POWER_STATE eCurrentPowerState, PVRSRV_DEV_POWER_STATE eDefaultPowerState);
PVRSRV_ERROR SGXScheduleCCBCommandKM(PVRSRV_DEVICE_NODE *psDeviceNode,
		IMG_UINT32 eCmdType, SGXMKIF_COMMAND *psCommand,
		IMG_UINT32 ui32CallerID, IMG_UINT32 ui32PDumpFlags,
		IMG_HANDLE hDevMemContext, IMG_BOOL bLastInScene);

#if FW_DEBUG
static const SceKernelDebugInfo dbg_DevDeInitSGX_L1265 = {
	0xdeee73fd, 0x27af3dc3, 0x0ed9f42a,
	"devices/sgx/sgxinit.c", 1265, "DevDeInitSGX"
};
#endif

/* Pristine SGX_FEATURE_MP path for Sony's SGX543MP4 configuration. */
IMG_VOID SGXCacheInvalidate(PVRSRV_DEVICE_NODE *psDeviceNode)
{
	PVRSRV_SGXDEV_INFO *psDevInfo = psDeviceNode->pvDevice;

	psDevInfo->ui32CacheControl |= SGXMKIF_CC_INVAL_BIF_SL;
}

/* SCE Power clock-change notification; frequencies arrive in MHz. */
IMG_VOID sceGpuPostClockSpeedChange(IMG_UINT32 ui32CoreClockSpeed,
		IMG_UINT32 ui32SecondClockSpeed, IMG_INT bIdleDevice,
		IMG_VOID *pvDevice)
{
	SGX_DEVICE_MAP *psDeviceMap = NULL;
	PVRSRV_DEVICE_NODE *psDeviceNode = pvDevice;

	(void)ui32SecondClockSpeed;
	SysGetDeviceMemoryMap(PVRSRV_DEVICE_TYPE_SGX, &psDeviceMap);
	if (psDeviceMap != NULL && ui32CoreClockSpeed != 0)
		psDeviceMap->sTimingInfo.ui32CoreClockSpeed = ui32CoreClockSpeed * 1000000;
	PVRSRVDevicePostClockSpeedChange(psDeviceNode->sDevId.ui32DeviceIndex,
			bIdleDevice ? IMG_TRUE : IMG_FALSE, NULL);
}

/* SCE Power invokes both notifications with the same complete device node. */
IMG_VOID sceGpuPreClockSpeedChange(IMG_UINT32 ui32CoreClockSpeed,
		IMG_UINT32 ui32SecondClockSpeed, IMG_INT bIdleDevice,
		IMG_VOID *pvDevice)
{
	SGX_DEVICE_MAP *psDeviceMap = NULL;
	PVRSRV_DEVICE_NODE *psDeviceNode = pvDevice;

	(void)ui32SecondClockSpeed;
	SysGetDeviceMemoryMap(PVRSRV_DEVICE_TYPE_SGX, &psDeviceMap);
	if (psDeviceMap != NULL && ui32CoreClockSpeed != 0)
		psDeviceMap->sTimingInfo.ui32CoreClockSpeed = ui32CoreClockSpeed * 1000000;
	PVRSRVDevicePreClockSpeedChange(psDeviceNode->sDevId.ui32DeviceIndex,
			bIdleDevice ? IMG_TRUE : IMG_FALSE, NULL);
}

/* Pristine path with OS_SUPPORTS_IN_LISR disabled in the SCE port. */
IMG_VOID SGXCommandComplete(PVRSRV_DEVICE_NODE *psDeviceNode)
{
	SGXScheduleProcessQueuesKM(psDeviceNode);
}

/* Pristine SGX_FEATURE_DATA_BREAKPOINTS path; SCE omits diagnostics. */
IMG_BOOL SGX_ISRHandler(IMG_VOID *pvData)
{
	IMG_BOOL bInterruptProcessed = IMG_FALSE;
	IMG_UINT32 ui32EventStatus, ui32EventEnable;
	IMG_UINT32 ui32EventClear = 0;
	IMG_UINT32 ui32EventStatus2, ui32EventEnable2;
	IMG_UINT32 ui32EventClear2 = 0;
	PVRSRV_DEVICE_NODE *psDeviceNode;
	PVRSRV_SGXDEV_INFO *psDevInfo;

	if (pvData == NULL)
		return bInterruptProcessed;
	psDeviceNode = pvData;
	psDevInfo = psDeviceNode->pvDevice;
	ui32EventStatus = OSReadHWReg(psDevInfo->pvRegsBaseKM, EUR_CR_EVENT_STATUS);
	ui32EventEnable = OSReadHWReg(psDevInfo->pvRegsBaseKM, EUR_CR_EVENT_HOST_ENABLE);
	ui32EventStatus &= ui32EventEnable;
	ui32EventStatus2 = OSReadHWReg(psDevInfo->pvRegsBaseKM, EUR_CR_EVENT_STATUS2);
	ui32EventEnable2 = OSReadHWReg(psDevInfo->pvRegsBaseKM, EUR_CR_EVENT_HOST_ENABLE2);
	ui32EventStatus2 &= ui32EventEnable2;
	/* SCE retains the matching status bits directly as the clear mask. */
	ui32EventClear = ui32EventStatus & EUR_CR_EVENT_STATUS_SW_EVENT_MASK;
	ui32EventClear2 = ui32EventStatus2 &
			EUR_CR_EVENT_STATUS2_DATA_BREAKPOINT_UNTRAPPED_MASK;
	if (ui32EventStatus2 & EUR_CR_EVENT_STATUS2_DATA_BREAKPOINT_TRAPPED_MASK)
		ui32EventClear2 |= EUR_CR_EVENT_HOST_CLEAR2_DATA_BREAKPOINT_TRAPPED_MASK;
	if (!ui32EventClear) {
		if (!ui32EventClear2)
			return IMG_FALSE;
	}
	ui32EventClear |= EUR_CR_EVENT_HOST_CLEAR_MASTER_INTERRUPT_MASK;
	OSWriteHWReg(psDevInfo->pvRegsBaseKM, EUR_CR_EVENT_HOST_CLEAR,
			ui32EventClear);
	OSWriteHWReg(psDevInfo->pvRegsBaseKM, EUR_CR_EVENT_HOST_CLEAR2,
			ui32EventClear2);
	return IMG_TRUE;
}

/* SCE defers hardware recovery to the lockup thread via its event flag. */
IMG_VOID SGX_MISRHandler(IMG_VOID *pvData)
{
	PVRSRV_DEVICE_NODE *psDeviceNode = pvData;
	PVRSRV_SGXDEV_INFO *psDevInfo = psDeviceNode->pvDevice;
	struct SGXMKIF_HOST_CTL *psSGXHostCtl = psDevInfo->psSGXHostCtl;

	if ((psSGXHostCtl->ui32InterruptFlags & 1U) &&
	    !(psSGXHostCtl->ui32InterruptClearFlags & 1U))
		sceKernelSetEventFlag(GetLockupEventUid(), 1);
	SGXTestActivePowerEvent(psDeviceNode, (IMG_UINT32)-3);
}

/* SCE: diagnostics and PDUMP omitted; native BM owners replace IMG contexts. */
PVRSRV_ERROR DevInitSGXPart1(IMG_VOID *pvDeviceNode)
{
	IMG_HANDLE hDevMemHeap = NULL;
	PVRSRV_SGXDEV_INFO *psDevInfo;
	IMG_HANDLE hKernelDevMemContext;
	IMG_DEV_PHYADDR sPDDevPAddr;
	IMG_UINT32 i;
	PVRSRV_DEVICE_NODE *psDeviceNode = pvDeviceNode;
	DEVICE_MEMORY_HEAP_INFO *psDeviceMemoryHeap =
			psDeviceNode->sDevMemoryInfo.psDeviceMemoryHeap;
	PVRSRV_ERROR eError;

	if (OSAllocMem_Impl(2, sizeof(PVRSRV_SGXDEV_INFO),
			(IMG_VOID **)&psDevInfo, NULL) != PVRSRV_OK)
		return PVRSRV_ERROR_OUT_OF_MEMORY;
	OSMemSet(psDevInfo, 0, sizeof(PVRSRV_SGXDEV_INFO));
	psDevInfo->eDeviceType = PVRSRV_DEVICE_TYPE_SGX;
	psDevInfo->eDeviceClass = PVRSRV_DEVICE_CLASS_3D;
	psDeviceNode->pvDevice = psDevInfo;
	psDevInfo->pvDeviceMemoryHeap = psDeviceMemoryHeap;
	hKernelDevMemContext = BM_CreateContext(psDeviceNode, &sPDDevPAddr,
			NULL, NULL);
	if (hKernelDevMemContext == NULL)
		return PVRSRV_ERROR_OUT_OF_MEMORY;
	psDevInfo->sKernelPDDevPAddr = sPDDevPAddr;
	for (i = 0; i < psDeviceNode->sDevMemoryInfo.ui32HeapCount; i++) {
		switch (psDeviceMemoryHeap[i].DevMemHeapType) {
		case DEVICE_MEMORY_HEAP_KERNEL:
		case DEVICE_MEMORY_HEAP_SHARED:
		case DEVICE_MEMORY_HEAP_SHARED_EXPORTED:
			if (psDeviceMemoryHeap[i].ui32HeapSize > 0) {
				hDevMemHeap = BM_CreateHeap(hKernelDevMemContext,
						&psDeviceMemoryHeap[i]);
				psDeviceMemoryHeap[i].hDevMemHeap = hDevMemHeap;
			}
			break;
		}
	}
	eError = MMU_BIFResetPDAlloc(psDevInfo);
	if (eError != PVRSRV_OK)
		return eError;
	return PVRSRV_OK;
}

/* SCE: the original takes the register-base member's address directly;
 * PDUMP and diagnostic operations are absent from this script interpreter.
 * The SCE loop tests exhaustion directly instead of the upstream bound test.
 */
PVRSRV_ERROR SGXRunScript(IMG_VOID **ppvRegsBaseKM,
		SGX_INIT_COMMAND *psScript, IMG_UINT32 ui32NumInitCommands)
{
	IMG_UINT32 ui32PC;
	SGX_INIT_COMMAND *psComm;

	for (ui32PC = 0, psComm = psScript; ; ui32PC++, psComm++) {
		if (ui32PC == ui32NumInitCommands)
			return PVRSRV_ERROR_UNKNOWN_SCRIPT_OPERATION;
		switch (psComm->eOp) {
		case SGX_INIT_OP_WRITE_HW_REG:
			OSWriteHWReg(*ppvRegsBaseKM, psComm->sWriteHWReg.ui32Offset,
					psComm->sWriteHWReg.ui32Value);
			break;
		case SGX_INIT_OP_HALT:
			return PVRSRV_OK;
		default:
			return PVRSRV_ERROR_UNKNOWN_SCRIPT_OPERATION;
		}
	}
}

/* SCE: DeinitDevInfo is inlined; register unmapping is an assertion. */
PVRSRV_ERROR DevDeInitSGX(IMG_VOID *pvDeviceNode)
{
	PVRSRV_DEVICE_NODE *psDeviceNode = pvDeviceNode;
	PVRSRV_SGXDEV_INFO *psDevInfo = psDeviceNode->pvDevice;
	PVRSRV_ERROR eError;
	IMG_UINT32 ui32Heap;
	DEVICE_MEMORY_HEAP_INFO *psDeviceMemoryHeap;
	SGX_DEVICE_MAP *psSGXDeviceMap;

	if (psDevInfo == NULL)
		return PVRSRV_OK;
	if (psDevInfo->hTimer != NULL) {
		eError = OSRemoveTimer(psDevInfo->hTimer);
		if (eError != PVRSRV_OK)
			return eError;
		psDevInfo->hTimer = NULL;
	}
	MMU_BIFResetPDFree(psDevInfo);
	if (psDevInfo->psKernelCCBInfo != NULL)
		OSFreeMem_Impl(1, sizeof(PVRSRV_SGX_CCB_INFO),
				psDevInfo->psKernelCCBInfo, NULL);
	psDeviceMemoryHeap = psDevInfo->pvDeviceMemoryHeap;
	for (ui32Heap = 0; ui32Heap < psDeviceNode->sDevMemoryInfo.ui32HeapCount;
			ui32Heap++) {
		switch (psDeviceMemoryHeap[ui32Heap].DevMemHeapType) {
		case DEVICE_MEMORY_HEAP_KERNEL:
		case DEVICE_MEMORY_HEAP_SHARED:
		case DEVICE_MEMORY_HEAP_SHARED_EXPORTED:
			if (psDeviceMemoryHeap[ui32Heap].hDevMemHeap != NULL)
				BM_DestroyHeap(psDeviceMemoryHeap[ui32Heap].hDevMemHeap);
			break;
		}
	}
	eError = BM_DestroyContext(psDeviceNode->sDevMemoryInfo.pBMKernelContext,
			NULL);
	if (eError != PVRSRV_OK)
		return eError;
	eError = PVRSRVRemovePowerDevice(psDeviceNode->sDevId.ui32DeviceIndex);
	if (eError != PVRSRV_OK)
		return eError;
	eError = SysGetDeviceMemoryMap(PVRSRV_DEVICE_TYPE_SGX, &psSGXDeviceMap);
	if (eError != PVRSRV_OK)
		return eError;
#if FW_DEBUG
	if (psSGXDeviceMap->pvRegsCpuVBase == NULL && psDevInfo->pvRegsBaseKM != NULL)
		SceDebugForDriver_35A35322(0, &dbg_DevDeInitSGX_L1265,
				__builtin_return_address(0));
#endif
	OSFreeMem_Impl(2, sizeof(PVRSRV_SGXDEV_INFO), psDevInfo, NULL);
	psDeviceNode->pvDevice = NULL;
	if (psDeviceMemoryHeap != NULL)
		OSFreeMem_Impl(1, sizeof(DEVICE_MEMORY_HEAP_INFO) * 22,
				psDeviceMemoryHeap, NULL);
	return PVRSRV_OK;
}

/* SCE: the original receives psKernelSGXMiscMemInfo directly. The polling
 * expression is pristine LOOP_UNTIL_TIMEOUT with diagnostics disabled.
 */
PVRSRV_ERROR SGXGetMiscInfoUkernel(PVRSRV_KERNEL_MEM_INFO *psMemInfo,
		PVRSRV_DEVICE_NODE *psDeviceNode, IMG_HANDLE hDevMemContext)
{
	PVRSRV_ERROR eError;
	SGXMKIF_COMMAND sCommandData;
	PVRSRV_SGX_MISCINFO_INFO *psSGXMiscInfoInt;
	PVRSRV_SGX_MISCINFO_FEATURES *psSGXFeatures;
	SGX_MISCINFO_STRUCT_SIZES *psSGXStructSizes;
	IMG_BOOL bExit = IMG_FALSE;
	IMG_UINT32 uiOffset, uiStart, uiCurrent;
	IMG_INT iNotLastLoop;

	if (psMemInfo->pvLinAddrKM == NULL)
		return PVRSRV_ERROR_INVALID_PARAMS;
	psSGXMiscInfoInt = psMemInfo->pvLinAddrKM;
	psSGXFeatures = &psSGXMiscInfoInt->sSGXFeatures;
	psSGXStructSizes = &psSGXMiscInfoInt->sSGXStructSizes;
	psSGXMiscInfoInt->ui32MiscInfoFlags &= ~PVRSRV_USSE_MISCINFO_READY;
	OSMemSet(psSGXFeatures, 0, sizeof(*psSGXFeatures));
	OSMemSet(psSGXStructSizes, 0, sizeof(*psSGXStructSizes));
	sCommandData.ui32Data[1] = psMemInfo->sDevVAddr.uiAddr;
	eError = SGXScheduleCCBCommandKM(psDeviceNode, SGXMKIF_CMD_GETMISCINFO,
			&sCommandData, KERNEL_ID, 0, hDevMemContext, IMG_FALSE);
	if (eError != PVRSRV_OK)
		return eError;
	for (uiOffset = 0, uiStart = OSClockus(), uiCurrent = uiStart + 1,
			iNotLastLoop = 1;
			((uiCurrent - uiStart + uiOffset) < 500000) || iNotLastLoop--;
			uiCurrent = OSClockus(),
			uiOffset = uiCurrent < uiStart ? ~0U - uiStart : uiOffset,
			uiStart = uiCurrent < uiStart ? 0 : uiStart) {
		if (psSGXMiscInfoInt->ui32MiscInfoFlags & PVRSRV_USSE_MISCINFO_READY) {
			bExit = IMG_TRUE;
			break;
		}
	}
	if (!bExit)
		return PVRSRV_ERROR_TIMEOUT;
	return PVRSRV_OK;
}

#if FW_DEBUG
#define PVR_LOG_SCE(...) VBAppend(__VA_ARGS__)
#else
#define PVR_LOG_SCE(...) do { } while (0)
#endif
#define CHECK_SIZE(NAME) do { \
	if (psSGXStructSizes->ui32Sizeof_##NAME != \
			psDevInfo->sSGXStructSizes.ui32Sizeof_##NAME) \
		bStructSizesFailed = IMG_TRUE; \
} while (0)

/* Pristine compatibility checks with SCE VBAppend diagnostics and identity. */
PVRSRV_ERROR SGXDevInitCompatCheck(PVRSRV_DEVICE_NODE *psDeviceNode)
{
	PVRSRV_SGXDEV_INFO *psDevInfo;
	PVRSRV_KERNEL_MEM_INFO *psMemInfo;
	PVRSRV_SGX_MISCINFO_INFO *psSGXMiscInfoInt;
	PVRSRV_SGX_MISCINFO_FEATURES *psSGXFeatures;
	SGX_MISCINFO_STRUCT_SIZES *psSGXStructSizes;
	PVRSRV_ERROR eError;
	IMG_UINT32 ui32BuildOptions, ui32BuildOptionsMismatch;
	IMG_BOOL bStructSizesFailed;
	IMG_BOOL bCheckCoreRev;
	const IMG_UINT32 aui32CoreRevExceptions[] = { 0x10100, 0x10101 };
	const IMG_UINT32 ui32NumCoreExceptions =
			sizeof(aui32CoreRevExceptions) / (2 * sizeof(IMG_UINT32));
	IMG_UINT32 i;

	if (psDeviceNode->sDevId.eDeviceType != PVRSRV_DEVICE_TYPE_SGX) {
		PVR_LOG_SCE("(FAIL) SGXInit: Device not of type SGX");
		return PVRSRV_ERROR_INVALID_PARAMS;
	}
	psDevInfo = psDeviceNode->pvDevice;
	ui32BuildOptions = SGX_BUILD_OPTIONS;
	if (ui32BuildOptions != psDevInfo->ui32ClientBuildOptions) {
		ui32BuildOptionsMismatch = ui32BuildOptions ^ psDevInfo->ui32ClientBuildOptions;
		if (psDevInfo->ui32ClientBuildOptions & ui32BuildOptionsMismatch)
			PVR_LOG_SCE("(FAIL) SGXInit: Mismatch in client-side and KM driver build options; "
					"extra options present in client-side driver: (0x%x). Please check sgx_options.h",
					psDevInfo->ui32ClientBuildOptions & ui32BuildOptionsMismatch);
		if (ui32BuildOptions & ui32BuildOptionsMismatch)
			PVR_LOG_SCE("(FAIL) SGXInit: Mismatch in client-side and KM driver build options; "
					"extra options present in KM: (0x%x). Please check sgx_options.h",
					ui32BuildOptions & ui32BuildOptionsMismatch);
		return PVRSRV_ERROR_BUILD_MISMATCH;
	}
	psMemInfo = psDevInfo->psKernelSGXMiscMemInfo;
	psSGXMiscInfoInt = psMemInfo->pvLinAddrKM;
	psSGXMiscInfoInt->ui32MiscInfoFlags = PVRSRV_USSE_MISCINFO_GET_STRUCT_SIZES;
	eError = SGXGetMiscInfoUkernel(psMemInfo, psDeviceNode, NULL);
	if (eError != PVRSRV_OK) {
		PVR_LOG_SCE("(FAIL) SGXInit: Unable to validate device DDK version");
		return eError;
	}
	psSGXFeatures = &((PVRSRV_SGX_MISCINFO_INFO *)psMemInfo->pvLinAddrKM)->sSGXFeatures;
	if (psSGXFeatures->ui32DDKVersion != 0x10711 ||
			psSGXFeatures->ui32DDKBuild != PVRVERSION_BUILD) {
		PVR_LOG_SCE("(FAIL) SGXInit: Incompatible driver DDK revision (%d)/device DDK revision (%d).",
				PVRVERSION_BUILD, psSGXFeatures->ui32DDKBuild);
		return PVRSRV_ERROR_DDK_VERSION_MISMATCH;
	}
	if (psSGXFeatures->ui32CoreRevSW == 0) {
		PVR_LOG_SCE("SGXInit: HW core rev (%x) check skipped.", psSGXFeatures->ui32CoreRev);
	} else {
		bCheckCoreRev = IMG_TRUE;
		for (i = 0; i < ui32NumCoreExceptions; i += 2) {
			if (psSGXFeatures->ui32CoreRev == aui32CoreRevExceptions[i] &&
					psSGXFeatures->ui32CoreRevSW == aui32CoreRevExceptions[i + 1]) {
				PVR_LOG_SCE("SGXInit: HW core rev (%x), SW core rev (%x) check skipped.",
						psSGXFeatures->ui32CoreRev, psSGXFeatures->ui32CoreRevSW);
				bCheckCoreRev = IMG_FALSE;
			}
		}
		if (bCheckCoreRev && psSGXFeatures->ui32CoreRev != psSGXFeatures->ui32CoreRevSW) {
			PVR_LOG_SCE("(FAIL) SGXInit: Incompatible HW core rev (%x) and SW core rev (%x).",
					psSGXFeatures->ui32CoreRev, psSGXFeatures->ui32CoreRevSW);
			return PVRSRV_ERROR_BUILD_MISMATCH;
		}
	}
	psSGXStructSizes = &((PVRSRV_SGX_MISCINFO_INFO *)psMemInfo->pvLinAddrKM)->sSGXStructSizes;
	bStructSizesFailed = IMG_FALSE;
	CHECK_SIZE(HOST_CTL);
	CHECK_SIZE(COMMAND);
	CHECK_SIZE(2DCMD);
	CHECK_SIZE(2DCMD_SHARED);
	CHECK_SIZE(CMDTA);
	CHECK_SIZE(CMDTA_SHARED);
	CHECK_SIZE(TRANSFERCMD);
	CHECK_SIZE(TRANSFERCMD_SHARED);
	CHECK_SIZE(3DREGISTERS);
	CHECK_SIZE(HWPBDESC);
	CHECK_SIZE(HWRENDERCONTEXT);
	CHECK_SIZE(HWRENDERDETAILS);
	CHECK_SIZE(HWRTDATA);
	CHECK_SIZE(HWRTDATASET);
	CHECK_SIZE(HWTRANSFERCONTEXT);
	if (bStructSizesFailed == IMG_TRUE) {
		PVR_LOG_SCE("(FAIL) SGXInit: Mismatch in SGXMKIF structure sizes.");
		return PVRSRV_ERROR_BUILD_MISMATCH;
	}
	ui32BuildOptions = psSGXFeatures->ui32BuildOptions;
	if (ui32BuildOptions != SGX_BUILD_OPTIONS) {
		ui32BuildOptionsMismatch = ui32BuildOptions ^ SGX_BUILD_OPTIONS;
		if (SGX_BUILD_OPTIONS & ui32BuildOptionsMismatch)
			PVR_LOG_SCE("(FAIL) SGXInit: Mismatch in driver and microkernel build options; "
					"extra options present in driver: (0x%x). Please check sgx_options.h",
					SGX_BUILD_OPTIONS & ui32BuildOptionsMismatch);
		if (ui32BuildOptions & ui32BuildOptionsMismatch)
			PVR_LOG_SCE("(FAIL) SGXInit: Mismatch in driver and microkernel build options; "
					"extra options present in microkernel: (0x%x). Please check sgx_options.h",
					ui32BuildOptions & ui32BuildOptionsMismatch);
		return PVRSRV_ERROR_BUILD_MISMATCH;
	}
	return PVRSRV_OK;
}

#undef CHECK_SIZE
#undef PVR_LOG_SCE

/* Pristine initialisation with SCE's register-member script ABI and init
 * status store ordered before the host-clock publication.
 */
PVRSRV_ERROR SGXInitialise(PVRSRV_SGXDEV_INFO *psDevInfo,
		IMG_BOOL bHardwareRecovery)
{
	static IMG_BOOL bFirstTime = IMG_TRUE;
	struct SGXMKIF_HOST_CTL *psSGXHostCtl =
		psDevInfo->psKernelSGXHostCtlMemInfo->pvLinAddrKM;
	PVRSRV_ERROR eError;
	IMG_UINT32 ui32HostClock;

	eError = SGXRunScript(&psDevInfo->pvRegsBaseKM,
			psDevInfo->sScripts.asInitCommandsPart1, SGX_MAX_INIT_COMMANDS);
	if (eError != PVRSRV_OK)
		return eError;
	psDevInfo->ui32NumResets++;
	if (bFirstTime)
		bHardwareRecovery = IMG_TRUE;
	else if (bHardwareRecovery)
		bHardwareRecovery = IMG_TRUE;
	SGXReset(psDevInfo, bHardwareRecovery, 0x40000000U);
	OSWriteHWReg(psDevInfo->pvRegsBaseKM, EUR_CR_POWER, 0);
	*psDevInfo->pui32KernelCCBEventKicker = 0;
	eError = SGXRunScript(&psDevInfo->pvRegsBaseKM,
			psDevInfo->sScripts.asInitCommandsPart2, SGX_MAX_INIT_COMMANDS);
	if (eError != PVRSRV_OK)
		return eError;
	ui32HostClock = OSClockus();
	psSGXHostCtl->ui32InitStatus = 0;
	psSGXHostCtl->ui32HostClock = ui32HostClock;
	__asm__ volatile("dmb sy" ::: "memory");
	OSWriteHWReg(psDevInfo->pvRegsBaseKM, 0x8ac0, 1);
	__asm__ volatile("dsb sy" ::: "memory");
	eError = PollForValueKM(&psSGXHostCtl->ui32InitStatus, 1, 1,
			500000, 50, IMG_FALSE);
	if (eError != PVRSRV_OK)
		return PVRSRV_ERROR_RETRY;
	bFirstTime = IMG_FALSE;
	return eError;
}

/* Pristine deinitialisation with SCE's direct register-member script ABI. */
PVRSRV_ERROR SGXDeinitialise(IMG_HANDLE hDevCookie)
{
	PVRSRV_SGXDEV_INFO *psDevInfo = hDevCookie;
	PVRSRV_ERROR eError;

	if (psDevInfo->pvRegsBaseKM == NULL)
		return PVRSRV_OK;
	eError = SGXRunScript(&psDevInfo->pvRegsBaseKM,
			psDevInfo->sScripts.asDeinitCommands, SGX_MAX_DEINIT_COMMANDS);
	if (eError != PVRSRV_OK)
		return eError;
	return PVRSRV_OK;
}

/* SCE inlines InitDevInfo and supplies an already-mapped register aperture. */
PVRSRV_ERROR DevInitSGXPart2KM(PVRSRV_PER_PROCESS_DATA *psPerProc,
		IMG_HANDLE hDevHandle, SGX_BRIDGE_INIT_INFO *psInitInfo)
{
	PVRSRV_DEVICE_NODE *psDeviceNode = hDevHandle;
	PVRSRV_SGXDEV_INFO *psDevInfo = psDeviceNode->pvDevice;
	PVRSRV_SGX_CCB_INFO *psKernelCCBInfo = NULL;
	SGX_DEVICE_MAP *psSGXDeviceMap;
	PVRSRV_ERROR eError;
#if FW_VERSION >= 0x04000000
	static const SceKernelDebugInfo dbg_DevInitSGXPart2KM_L1047 = {
		0xdeee73fd, 0x27af3dc3, 0x0ed9f42a,
		"devices/sgx/sgxinit.c", 1047, "DevInitSGXPart2KM"
	};
#endif

	(void)psPerProc;
	psDevInfo->sScripts = psInitInfo->sScripts;
	psDevInfo->psKernelCCBMemInfo = psInitInfo->hKernelCCBMemInfo;
	psDevInfo->psKernelCCB = psDevInfo->psKernelCCBMemInfo->pvLinAddrKM;
	psDevInfo->psKernelCCBCtlMemInfo = psInitInfo->hKernelCCBCtlMemInfo;
	psDevInfo->psKernelCCBCtl = psDevInfo->psKernelCCBCtlMemInfo->pvLinAddrKM;
	psDevInfo->psKernelCCBEventKickerMemInfo = psInitInfo->hKernelCCBEventKickerMemInfo;
	psDevInfo->pui32KernelCCBEventKicker = psDevInfo->psKernelCCBEventKickerMemInfo->pvLinAddrKM;
	psDevInfo->psKernelSGXHostCtlMemInfo = psInitInfo->hKernelSGXHostCtlMemInfo;
	psDevInfo->psSGXHostCtl = psDevInfo->psKernelSGXHostCtlMemInfo->pvLinAddrKM;
	psDevInfo->psKernelSGXTA3DCtlMemInfo = psInitInfo->hKernelSGXTA3DCtlMemInfo;
	psDevInfo->psKernelSGXMiscMemInfo = psInitInfo->hKernelSGXMiscMemInfo;
	psDevInfo->psKernelTASigBufferMemInfo = psInitInfo->hKernelTASigBufferMemInfo;
	psDevInfo->psKernel3DSigBufferMemInfo = psInitInfo->hKernel3DSigBufferMemInfo;
	psDevInfo->ui32ClientBuildOptions = psInitInfo->ui32ClientBuildOptions;
	psDevInfo->sSGXStructSizes = psInitInfo->sSGXStructSizes;
	eError = OSAllocMem_Impl(1, sizeof(*psKernelCCBInfo),
			(IMG_VOID **)&psKernelCCBInfo, NULL);
	if (eError != PVRSRV_OK) {
		if (psDevInfo->psKernelCCBInfo != NULL)
			OSFreeMem_Impl(1, sizeof(*psKernelCCBInfo), psDevInfo->psKernelCCBInfo, NULL);
		return eError;
	}
	OSMemSet(psKernelCCBInfo, 0, sizeof(*psKernelCCBInfo));
	psKernelCCBInfo->psCCBMemInfo = psDevInfo->psKernelCCBMemInfo;
	psKernelCCBInfo->psCCBCtlMemInfo = psDevInfo->psKernelCCBCtlMemInfo;
	psKernelCCBInfo->psCommands = psDevInfo->psKernelCCB->asCommands;
	psKernelCCBInfo->pui32WriteOffset = &psDevInfo->psKernelCCBCtl->ui32WriteOffset;
	psKernelCCBInfo->pui32ReadOffset = &psDevInfo->psKernelCCBCtl->ui32ReadOffset;
	psDevInfo->psKernelCCBInfo = psKernelCCBInfo;
	OSMemCopy(psDevInfo->aui32HostKickAddr, psInitInfo->aui32HostKickAddr,
			sizeof(psDevInfo->aui32HostKickAddr));
	psDevInfo->bForcePTOff = IMG_FALSE;
	psDevInfo->ui32CacheControl = psInitInfo->ui32CacheControl;
	psDevInfo->ui32EDMTaskReg0 = psInitInfo->ui32EDMTaskReg0;
	psDevInfo->ui32EDMTaskReg1 = psInitInfo->ui32EDMTaskReg1;
	psDevInfo->ui32ClkGateCtl = psInitInfo->ui32ClkGateCtl;
	psDevInfo->ui32ClkGateCtl2 = psInitInfo->ui32ClkGateCtl2;
	psDevInfo->ui32ClkGateStatusReg = psInitInfo->ui32ClkGateStatusReg;
	psDevInfo->ui32ClkGateStatusMask = psInitInfo->ui32ClkGateStatusMask;
	psDevInfo->ui32MasterClkGateStatusReg = psInitInfo->ui32MasterClkGateStatusReg;
	psDevInfo->ui32MasterClkGateStatusMask = psInitInfo->ui32MasterClkGateStatusMask;
	psDevInfo->ui32MasterClkGateStatus2Reg = psInitInfo->ui32MasterClkGateStatus2Reg;
	psDevInfo->ui32MasterClkGateStatus2Mask = psInitInfo->ui32MasterClkGateStatus2Mask;
	psDevInfo->asSGXDevData[4] = psInitInfo->ui32DevData4;
	psDevInfo->asSGXDevData[8] = psInitInfo->ui32DevData8;
	psDevInfo->asSGXDevData[9] = psInitInfo->ui32PixelShaderHeapBase;
	psDevInfo->asSGXDevData[7] = psInitInfo->ui32DevData7;
	psDevInfo->asSGXDevData[6] = psInitInfo->ui32DevData6;
	if (SysGetDeviceMemoryMap(PVRSRV_DEVICE_TYPE_SGX, &psSGXDeviceMap) != PVRSRV_OK)
		return PVRSRV_ERROR_INIT_FAILURE;
	if (psSGXDeviceMap->pvRegsCpuVBase != NULL)
		psDevInfo->pvRegsBaseKM = psSGXDeviceMap->pvRegsCpuVBase;
#if FW_VERSION >= 0x04000000
	else
		SceDebugForDriver_35A35322(0, &dbg_DevInitSGXPart2KM_L1047, __builtin_return_address(0));
#endif
	psDevInfo->ui32RegSize = psSGXDeviceMap->ui32RegsSize;
	psDevInfo->sRegsPhysBase = psSGXDeviceMap->sRegsSysPBase;
	psDeviceNode->pvISRData = psDeviceNode;
	psDevInfo->psSGXHostCtl->ui32PowerStatus |= PVRSRV_USSE_EDM_POWMAN_NO_WORK;
	eError = PVRSRVRegisterPowerDevice(psDeviceNode->sDevId.ui32DeviceIndex,
			SGXPrePowerState, SGXPostPowerState, SGXPreClockSpeedChange,
			SGXPostClockSpeedChange, psDeviceNode,
			PVRSRV_DEV_POWER_STATE_OFF, PVRSRV_DEV_POWER_STATE_OFF);
	if (eError != PVRSRV_OK)
		return eError;
	ScePowerForDriver_711C68DF(sceGpuPreClockSpeedChange, psDeviceNode,
			sceGpuPostClockSpeedChange, psDeviceNode);
	OSMemSet(psDevInfo->psKernelCCB, 0, sizeof(*psDevInfo->psKernelCCB));
	OSMemSet(psDevInfo->psKernelCCBCtl, 0, sizeof(*psDevInfo->psKernelCCBCtl));
	OSMemSet(psDevInfo->pui32KernelCCBEventKicker, 0, sizeof(IMG_UINT32));
	return eError;
}

/* SCE omits diagnostics and queue processing already handled by its caller. */
IMG_VOID HWRecoveryResetSGX(PVRSRV_DEVICE_NODE *psDeviceNode,
		IMG_UINT32 ui32Component, IMG_UINT32 ui32CallerID)
{
	PVRSRV_SGXDEV_INFO *psDevInfo = psDeviceNode->pvDevice;
	struct SGXMKIF_HOST_CTL *psSGXHostCtl = psDevInfo->psSGXHostCtl;
	PVRSRV_ERROR eError;

	(void)ui32Component;
	eError = PVRSRVPowerLock(ui32CallerID, IMG_FALSE);
	if (eError != PVRSRV_OK)
		return;
	psSGXHostCtl->ui32InterruptClearFlags |= 1U;
	SGXInitialise(psDevInfo, IMG_TRUE);
	PVRSRVPowerUnlock(ui32CallerID);
	SGXScheduleProcessQueuesKM(psDeviceNode);
}

/* Pristine recovery sampling; SCE signals the asynchronous lockup thread. */
IMG_VOID SGXOSTimer(IMG_VOID *pvData)
{
	PVRSRV_DEVICE_NODE *psDeviceNode = pvData;
	PVRSRV_SGXDEV_INFO *psDevInfo = psDeviceNode->pvDevice;
	static IMG_UINT32 ui32EDMTasks;
	static IMG_UINT32 ui32LockupCounter;
	static IMG_UINT32 ui32OpenCLDelayCounter;
	static IMG_UINT32 ui32NumResets;
	IMG_UINT32 ui32CurrentEDMTasks, ui32CurrentOpenCLDelayCounter;
	IMG_UINT32 ui32CurrentNumResets;
	struct SGXMKIF_HOST_CTL *psSGXHostCtl;

	psDevInfo->ui32TimeStamp++;
	if (!SGXIsDevicePowered(psDeviceNode)) {
		ui32LockupCounter = 0;
		return;
	}
	ui32CurrentEDMTasks = OSReadHWReg(psDevInfo->pvRegsBaseKM,
			psDevInfo->ui32EDMTaskReg0);
	if (psDevInfo->ui32EDMTaskReg1 != 0)
		ui32CurrentEDMTasks ^= OSReadHWReg(psDevInfo->pvRegsBaseKM,
				psDevInfo->ui32EDMTaskReg1);
	ui32CurrentNumResets = psDevInfo->ui32NumResets;
	if (ui32CurrentEDMTasks == ui32EDMTasks &&
	    ui32CurrentNumResets == ui32NumResets) {
		ui32LockupCounter++;
		if (ui32LockupCounter == 3) {
			ui32LockupCounter = 0;
			psSGXHostCtl = psDevInfo->psSGXHostCtl;
			ui32CurrentOpenCLDelayCounter = psSGXHostCtl->ui32OpenCLDelayCount;
			if (ui32CurrentOpenCLDelayCounter != 0) {
				if (ui32OpenCLDelayCounter != ui32CurrentOpenCLDelayCounter)
					ui32OpenCLDelayCounter = ui32CurrentOpenCLDelayCounter;
				else
					psSGXHostCtl->ui32OpenCLDelayCount = --ui32OpenCLDelayCounter;
			} else {
				psSGXHostCtl->ui32HostDetectedLockups++;
				sceKernelSetEventFlag(GetLockupEventUid(), 2);
			}
		}
	} else {
		ui32EDMTasks = ui32CurrentEDMTasks;
		ui32LockupCounter = 0;
		ui32NumResets = ui32CurrentNumResets;
	}
}

/* SCE: native MMU registration, unnamed heaps, and twenty active regions in
 * a twenty-two-entry allocation replace the pristine platform heap recipe.
 */
PVRSRV_ERROR SGXRegisterDevice(PVRSRV_DEVICE_NODE *psDeviceNode)
{
	DEVICE_MEMORY_INFO *psDevMemoryInfo;
	DEVICE_MEMORY_HEAP_INFO *psDeviceMemoryHeap;

	psDeviceNode->sDevId.eDeviceType = PVRSRV_DEVICE_TYPE_SGX;
	psDeviceNode->sDevId.eDeviceClass = PVRSRV_DEVICE_CLASS_3D;
	psDeviceNode->pfnInitDevice = DevInitSGXPart1;
	psDeviceNode->pfnDeInitDevice = DevDeInitSGX;
	psDeviceNode->pfnInitDeviceCompatCheck = SGXDevInitCompatCheck;
	psDeviceNode->reserved = 0;
	psDeviceNode->pfnDeviceISR = SGX_ISRHandler;
	psDeviceNode->pfnDeviceMISR = SGX_MISRHandler;
	psDeviceNode->pfnDeviceCommandComplete = SGXCommandComplete;
	psDeviceNode->pfnCacheInvalidate = SGXCacheInvalidate;
	psDevMemoryInfo = &psDeviceNode->sDevMemoryInfo;
	psDevMemoryInfo->ui32AddressSpaceSizeLog2 = 0;
	if (OSAllocMem_Impl(1, sizeof(DEVICE_MEMORY_HEAP_INFO) * 22,
			(IMG_VOID **)&psDevMemoryInfo->psDeviceMemoryHeap, NULL) != PVRSRV_OK)
		return PVRSRV_ERROR_OUT_OF_MEMORY;
	OSMemSet(psDevMemoryInfo->psDeviceMemoryHeap, 0,
			sizeof(DEVICE_MEMORY_HEAP_INFO) * 22);
	psDeviceMemoryHeap = psDevMemoryInfo->psDeviceMemoryHeap;

#define SCE_HEAP(ID, BASE, SIZE, ATTRIBS, TYPE) do { \
	psDeviceMemoryHeap->ui32HeapID = 0x07000000U | (ID); \
	psDeviceMemoryHeap->sDevVAddrBase.uiAddr = (BASE); \
	psDeviceMemoryHeap->ui32HeapSize = (SIZE); \
	psDeviceMemoryHeap->ui32Attribs = (ATTRIBS); \
	psDeviceMemoryHeap->DevMemHeapType = (TYPE); \
	psDeviceMemoryHeap->ui32DataPageSize = 4096; \
	psDeviceMemoryHeap++; \
} while (0)
	SCE_HEAP(0, 0x40000000, 0x0ffff000, 0x14200, 0);
	SCE_HEAP(1, 0x50000000, 0x0cfff000, 0x24200, 0);
	SCE_HEAP(2, 0x14000000, 0x0007f000, 0x24200, 3);
	SCE_HEAP(3, 0x1c000000, 0x03fff000, 0x24200, 3);
	SCE_HEAP(4, 0x30000000, 0x02000000, 0x14200, 0);
	SCE_HEAP(5, 0x34000000, 0x02000000, 0x14200, 0);
	SCE_HEAP(6, 0x10000000, 0x01fff000, 0x14200, 0);
	SCE_HEAP(7, 0x18000000, 0x01bff000, 0x14200, 0);
	SCE_HEAP(8, 0x0c000000, 0x00fff000, 0x24200, 3);
	SCE_HEAP(9, 0x28000000, 0x04000000, 0x24200, 3);
	SCE_HEAP(10, 0x20000000, 0x04000000, 0x14200, 0);
	SCE_HEAP(12, 0x04000000, 0x03eff000, 0x14200, 3);
	SCE_HEAP(14, 0x80000000, 0x80000000, 0x14200, 0);
	SCE_HEAP(15, 0x60000000, 0x08000000, 0x14200, 0);
	SCE_HEAP(16, 0x24000000, 0x04000000, 0x114200, 0);
	SCE_HEAP(17, 0x2c000000, 0x04000000, 0x124200, 3);
	SCE_HEAP(18, 0x38000000, 0x00800000, 0x14200, 0);
	SCE_HEAP(19, 0x3c000000, 0x00800000, 0x14200, 0);
	SCE_HEAP(20, 0x68000000, 0x18000000, 0x14200, 0);
	SCE_HEAP(21, 0x08000000, 0x00002000, 0x24200, 3);
#undef SCE_HEAP
	psDevMemoryInfo->ui32HeapCount =
			psDeviceMemoryHeap - psDevMemoryInfo->psDeviceMemoryHeap;
	return PVRSRV_OK;
}

/* Pristine client registration with the SCE process identifier provider. */
PVRSRV_ERROR SGXGetClientInfoKM(IMG_HANDLE hDevCookie,
		SGX_CLIENT_INFO *psClientInfo)
{
	PVRSRV_SGXDEV_INFO *psDevInfo =
			((PVRSRV_DEVICE_NODE *)hDevCookie)->pvDevice;

	psDevInfo->ui32ClientRefCount++;
	psClientInfo->ui32ProcessID = OSGetCurrentProcessIDKM();
	OSMemCopy(&psClientInfo->asDevData, &psDevInfo->asSGXDevData,
			sizeof(psClientInfo->asDevData));
	return PVRSRV_OK;
}

PVRSRV_ERROR PVRSRVGetDeviceMemHeapsKM(IMG_HANDLE hDevCookie,
		PVRSRV_HEAP_INFO *psHeapInfo);

/* Pristine SGX initialization query; SCE compiles out the error diagnostics. */
/* SCE clock/SoC queries, direct hardware counter setup, timers and
 * development display control supplement the pristine miscellaneous API.
 */
PVRSRV_ERROR SGXGetMiscInfoKM(PVRSRV_SGXDEV_INFO *psDevInfo,
		SGX_MISC_INFO *psMiscInfo, PVRSRV_DEVICE_NODE *psDeviceNode,
		IMG_HANDLE hDevMemContext)
{
	PVRSRV_KERNEL_MEM_INFO *psMemInfo = psDevInfo->psKernelSGXMiscMemInfo;
	PVRSRV_SGX_MISCINFO_INFO *psMiscInfoData = psMemInfo->pvLinAddrKM;
	PVRSRV_SGX_MISCINFO_FEATURES *psSGXFeatures;
	PVRSRV_SGX_MISCINFO_SET_HWPERF_STATUS *psSetHWPerfStatus;
	PVRSRV_ERROR eError;
	IMG_UINT32 ui32ClockSpeed, ui32RegOffset;
	IMG_UINT32 ui32PerfSelect0, ui32PerfSelect1, ui32PerfSelect2, ui32PerfSelect3;
	IMG_UINT32 ui32Flags;
	SceUInt32 *displayStatus;
	SceUID displayPid;

	psMiscInfoData->ui32MiscInfoFlags = 0;
	switch (psMiscInfo->eRequest) {
	case SGX_MISC_INFO_REQUEST_CLOCKSPEED:
		scePowerGetGpuClockFrequencyInternal(&ui32ClockSpeed, NULL);
		psMiscInfo->uData.ui32SGXClockSpeed = ui32ClockSpeed * 1000000;
		return PVRSRV_OK;
	case SGX_MISC_INFO_REQUEST_SGXREV:
		eError = SGXGetMiscInfoUkernel(psMemInfo, psDeviceNode, hDevMemContext);
		if (eError != PVRSRV_OK)
			return eError;
		psSGXFeatures = &((PVRSRV_SGX_MISCINFO_INFO *)psMemInfo->pvLinAddrKM)->sSGXFeatures;
		psSGXFeatures->socRevision = ScePervasiveForDriver_714EEFB7() & 0x1ffffU;
		psMiscInfo->uData.sSGXFeatures = *psSGXFeatures;
		return PVRSRV_OK;
	case SGX_MISC_INFO_REQUEST_DRIVER_SGXREV:
		psSGXFeatures = &((PVRSRV_SGX_MISCINFO_INFO *)psMemInfo->pvLinAddrKM)->sSGXFeatures;
		OSMemSet(psMemInfo->pvLinAddrKM, 0, sizeof(PVRSRV_SGX_MISCINFO_INFO));
		psSGXFeatures->ui32DDKVersion = 0x10711;
		psSGXFeatures->ui32DDKBuild = PVRVERSION_BUILD;
		psSGXFeatures->ui32BuildOptions = SGX_BUILD_OPTIONS;
		psMiscInfo->uData.sSGXFeatures = *psSGXFeatures;
		return PVRSRV_OK;
	case SGX_MISC_INFO_REQUEST_SET_HWPERF_STATUS:
		psSetHWPerfStatus = &psMiscInfo->uData.sSetHWPerfStatus;
		if (psSetHWPerfStatus->ui32NewHWPerfStatus & ~15U)
			return PVRSRV_ERROR_INVALID_PARAMS;
		if (psSetHWPerfStatus->ui32NewHWPerfStatus & 1U) {
			OSWriteHWReg(psDevInfo->pvRegsBaseKM, 0x40, 0xff);
			OSWriteHWReg(psDevInfo->pvRegsBaseKM, 0xe80, 1);
			OSWriteHWReg(psDevInfo->pvRegsBaseKM, 0xe80, 0);
			psSetHWPerfStatus->ui32NewHWPerfStatus &= ~1U;
		}
		if (psSetHWPerfStatus->ui32NewHWPerfStatus != 0) {
			ui32PerfSelect0 = (psSetHWPerfStatus->aui32PerfGroup[1] << 16) |
				(psSetHWPerfStatus->aui32PerfBit[0] << 8) |
				psSetHWPerfStatus->aui32PerfGroup[0] |
				(psSetHWPerfStatus->aui32PerfBit[1] << 24);
			ui32PerfSelect1 = (psSetHWPerfStatus->aui32PerfGroup[3] << 16) |
				(psSetHWPerfStatus->aui32PerfBit[2] << 8) |
				psSetHWPerfStatus->aui32PerfGroup[2] |
				(psSetHWPerfStatus->aui32PerfBit[3] << 24);
			ui32PerfSelect2 = (psSetHWPerfStatus->aui32PerfGroup[5] << 16) |
				(psSetHWPerfStatus->aui32PerfBit[4] << 8) |
				psSetHWPerfStatus->aui32PerfGroup[4] |
				(psSetHWPerfStatus->aui32PerfBit[5] << 24);
			ui32PerfSelect3 = (psSetHWPerfStatus->aui32PerfGroup[7] << 16) |
				(psSetHWPerfStatus->aui32PerfBit[6] << 8) |
				psSetHWPerfStatus->aui32PerfGroup[6] |
				(psSetHWPerfStatus->aui32PerfBit[7] << 24);
			OSWriteHWReg(psDevInfo->pvRegsBaseKM, 0x78, 1);
			for (ui32RegOffset = 0x8090; ui32RegOffset != 0x18090;
			     ui32RegOffset += 0x4000) {
				OSWriteHWReg(psDevInfo->pvRegsBaseKM, ui32RegOffset, ui32PerfSelect0);
				OSWriteHWReg(psDevInfo->pvRegsBaseKM, ui32RegOffset + 4, ui32PerfSelect1);
				OSWriteHWReg(psDevInfo->pvRegsBaseKM, ui32RegOffset + 8, ui32PerfSelect2);
				OSWriteHWReg(psDevInfo->pvRegsBaseKM, ui32RegOffset + 12, ui32PerfSelect3);
			}
			OSWriteHWReg(psDevInfo->pvRegsBaseKM, 0x40, 0);
		} else {
			OSWriteHWReg(psDevInfo->pvRegsBaseKM, 0x78, 0);
		}
		return PVRSRV_OK;
	case SGX_MISC_INFO_REQUEST_SPM:
		return PVRSRV_OK;
	case SGX_MISC_INFO_REQUEST_ACTIVEPOWER:
		psMiscInfo->uData.sActivePower.ui32NumActivePowerEvents =
			psDevInfo->psSGXHostCtl->ui32NumActivePowerEvents;
		return PVRSRV_OK;
	case SGX_MISC_INFO_REQUEST_LOCKUPS:
		psMiscInfo->uData.sLockups.ui32uKernelDetectedLockups =
			psDevInfo->psSGXHostCtl->ui32uKernelDetectedLockups;
		psMiscInfo->uData.sLockups.ui32HostDetectedLockups =
			psDevInfo->psSGXHostCtl->ui32HostDetectedLockups;
		return PVRSRV_OK;
	case SGX_MISC_INFO_REQUEST_TIMERS:
		psMiscInfo->uData.sTimers.ui32GPUTimer = OSReadHWReg(psDevInfo->pvRegsBaseKM, 0x144);
		psMiscInfo->uData.sTimers.ui64CPUTimer = SceProcessmgrForDriver_82D94BE9();
		return PVRSRV_OK;
	case SGX_MISC_INFO_REQUEST_DISPLAY_CONTROL:
		if (!SceSblAIMgrForDriver_274663A0() || !SceSblACMgrForDriver_E87D1777())
			return PVRSRV_OK;
		ui32Flags = psMiscInfo->uData.ui32DisplayControlFlags;
		if (ui32Flags & ~3U)
			return PVRSRV_ERROR_INVALID_PARAMS;
		if (ui32Flags & 2U) {
			displayStatus = (SceUInt32 *)SceSysrootForDriver_6655F48C(1, 3);
			displayPid = 0;
		} else {
			displayStatus = NULL;
			displayPid = -1;
		}
		SceDisplayForDriver_086DEFB6(displayStatus, displayPid);
		return PVRSRV_OK;
	default:
		return PVRSRV_ERROR_INVALID_PARAMS;
	}
}

PVRSRV_ERROR SGXGetInfoForSrvinitKM(IMG_HANDLE hDevHandle,
		SGX_BRIDGE_INFO_FOR_SRVINIT *psInitInfo)
{
	PVRSRV_DEVICE_NODE *psDeviceNode = hDevHandle;
	PVRSRV_SGXDEV_INFO *psDevInfo = psDeviceNode->pvDevice;

	psInitInfo->sPDDevPAddr = psDevInfo->sKernelPDDevPAddr;
	return PVRSRVGetDeviceMemHeapsKM(hDevHandle, &psInitInfo->asHeapInfo[0]);
}
