/* Sony env/playstation/osfunc.c: SCE native environment replacement.
 * Pristine IMG Linux implementation remains in the vendor directory.
 */
#include <sysmem/sysmem.h>
#include <psp2kern/kernel/threadmgr.h>
#include <psp2kern/kernel/intrmgr.h>
#include <sysmem/debug.h>
#include "../../system_sce.h"
#include "../../osfunc_sce.h"
#include <stddef.h>
#include <stdarg.h>
#include <stdio.h>
#include <sysmem/cpu.h>

IMG_BOOL PVRSRVSystemLISR(IMG_VOID *pvSysData);
IMG_VOID PVRSRVMISR(IMG_VOID *pvSysData);
/* Canonical processmgr_imports.h ABI; its unrelated interrupt declaration
 * conflicts with the SDK interrupt header used by this native port.
 */
IMG_VOID *AllocProcessGpuMemory(unsigned size);
int FreeProcessGpuMemory(IMG_VOID *address);

#if FW_DEBUG
static const SceKernelDebugInfo dbg_MISRTaskletThread_L817 = {
	0xd8108c50, 0xcbead34a, 0x60889eb4,
	"env/playstation/osfunc.c", 817, "MISRTaskletThread"
};
static const SceKernelDebugInfo dbg_OSInstallSystemLISR_L894 = {
	0xd8108c50, 0x19f66bbe, 0xb05e1be4,
	"env/playstation/osfunc.c", 894, "OSInstallSystemLISR"
};
static const SceKernelDebugInfo dbg_OSAddTimer_L1663 = {
	0xd8108c50, 0x405f95ea, 0x79894b36,
	"env/playstation/osfunc.c", 1663, "OSAddTimer"
};
#endif

int MISRTaskletThread(SceSize argSize, IMG_VOID *pvArgs)
{
	ENV_DATA *psEnvData = *(ENV_DATA **)pvArgs;
	SceUInt32 outBits;
	int res;

	while (psEnvData->misrRunning == IMG_TRUE) {
		res = sceKernelWaitEventFlag(psEnvData->misrEventFlag, 1, 5,
				&outBits, NULL);
		if (psEnvData->misrRunning == IMG_FALSE)
			break;
		if (res == 0) {
			psEnvData->pfnMISR(psEnvData->pvMISRData);
		} else {
#if FW_DEBUG
			SceDebugForDriver_D9703808(1, 0, &dbg_MISRTaskletThread_L817,
					"%s: Failed waiting %08x\n", "MISRTaskletThread", res);
#endif
		}
	}
	sceKernelExitDeleteThread(0);
	return 0;
}

/* SCE native callback returns the interrupt convention's -1; the timer
 * service discards it. Payload callback and active flag remain pristine.
 */
int OSTimerCallbackWrapper(SceSysTimerId timer, IMG_VOID *pvData)
{
	TIMER_CALLBACK_DATA *psTimerCBData = pvData;

	if (psTimerCBData->bActive)
		psTimerCBData->pfnTimerFunc(psTimerCBData->pvData);
	return -1;
}

/* SCE uses the native interrupt return value and signals its MISR thread. */
int SystemISRWrapper(int irq, IMG_VOID *pvSysData)
{
	SYS_DATA *psSysData = pvSysData;

	if (psSysData && PVRSRVSystemLISR(psSysData))
		sceKernelSetEventFlag(psSysData->pvEnvSpecificData->misrEventFlag, 1);
	return -1;
}

/* SCE omits upstream allocation diagnostics and the unused block handle. */
PVRSRV_ERROR OSAllocMem_Impl(IMG_UINT32 ui32Flags, IMG_SIZE_T ui32Size,
		IMG_VOID **ppvLinAddr, IMG_HANDLE *phBlockAlloc)
{
	*ppvLinAddr = AllocProcessGpuMemory(ui32Size);
	return *ppvLinAddr ? PVRSRV_OK : PVRSRV_ERROR_OUT_OF_MEMORY;
}

PVRSRV_ERROR OSFreeMem_Impl(IMG_UINT32 ui32Flags, IMG_SIZE_T ui32Size,
		IMG_VOID *pvLinAddr, IMG_HANDLE hBlockAlloc)
{
	FreeProcessGpuMemory(pvLinAddr);
	return PVRSRV_OK;
}

/* Pristine IMG formatting helper. */
IMG_INT32 OSSNPrintf(IMG_CHAR *pStr, IMG_UINT32 ui32Size,
		const IMG_CHAR *pszFormat, ...)
{
	va_list argList;
	IMG_INT32 iCount;

	va_start(argList, pszFormat);
	iCount = vsnprintf(pStr, (size_t)ui32Size, pszFormat, argList);
	va_end(argList);
	return iCount;
}

/* Pristine resource ownership; SCE omits diagnostic messages. */
IMG_VOID OSBreakResourceLock(PVRSRV_RESOURCE *psResource, IMG_UINT32 ui32ID)
{
	volatile IMG_UINT32 *pui32Access = &psResource->ui32Lock;

	if (*pui32Access) {
		if (psResource->ui32ID == ui32ID) {
			psResource->ui32ID = 0;
			*pui32Access = 0;
		}
	}
}

PVRSRV_ERROR OSCreateResource(PVRSRV_RESOURCE *psResource)
{
	psResource->ui32ID = 0;
	psResource->ui32Lock = 0;
	return PVRSRV_OK;
}

PVRSRV_ERROR OSDestroyResource(PVRSRV_RESOURCE *psResource)
{
	OSBreakResourceLock(psResource, psResource->ui32ID);
	return PVRSRV_OK;
}

/* SCE inlines its native allocation port and retains the pristine lifecycle. */
PVRSRV_ERROR OSInitEnvData(IMG_VOID **ppvEnvSpecificData)
{
	ENV_DATA *psEnvData;

	psEnvData = AllocProcessGpuMemory(sizeof(*psEnvData));
	if (!psEnvData)
		return PVRSRV_ERROR_OUT_OF_MEMORY;
	psEnvData->pvBridgeData = AllocProcessGpuMemory(0x2000);
	if (!psEnvData->pvBridgeData) {
		FreeProcessGpuMemory(psEnvData);
		return PVRSRV_ERROR_OUT_OF_MEMORY;
	}
	psEnvData->bMISRInstalled = IMG_FALSE;
	psEnvData->bLISRInstalled = IMG_FALSE;
	*ppvEnvSpecificData = psEnvData;
	return PVRSRV_OK;
}

PVRSRV_ERROR OSDeInitEnvData(IMG_VOID *pvEnvSpecificData)
{
	ENV_DATA *psEnvData = pvEnvSpecificData;

	/* SCE omits upstream installed-handler assertions. */
	FreeProcessGpuMemory(psEnvData->pvBridgeData);
	psEnvData->pvBridgeData = NULL;
	FreeProcessGpuMemory(psEnvData);
	return PVRSRV_OK;
}

/* SCE clock is already in microseconds. */
IMG_UINT32 OSClockus(IMG_VOID)
{
	return SceThreadmgrForDriver_47F6DE49();
}

IMG_VOID OSWaitus(IMG_UINT32 ui32Timeus)
{
	IMG_UINT32 ui32Start = SceThreadmgrForDriver_47F6DE49();

	do {
	} while (SceThreadmgrForDriver_47F6DE49() - ui32Start < ui32Timeus);
}

IMG_VOID OSSleepms(IMG_UINT32 ui32Timems)
{
	sceKernelDelayThread(ui32Timems * 1000);
}

IMG_UINT32 OSGetCurrentProcessIDKM(IMG_VOID)
{
	return sceKernelGetProcessId();
}

/* SCE replaces request_irq/tasklets with a native interrupt and MISR thread. */
PVRSRV_ERROR OSInstallSystemLISR(IMG_VOID *pvSysData, IMG_UINT32 ui32Irq)
{
	SYS_DATA *psSysData = pvSysData;
	ENV_DATA *psEnvData = psSysData->pvEnvSpecificData;

	if (psEnvData->bLISRInstalled)
		return PVRSRV_ERROR_ISR_ALREADY_INSTALLED;
	psEnvData->misrThread = sceKernelCreateThread("SceGpuMisr",
			MISRTaskletThread, 20, 0x2000, 0, 15, NULL);
	if (psEnvData->misrThread < 0) {
#if FW_DEBUG
		SceDebugForDriver_D9703808(1, 0, &dbg_OSInstallSystemLISR_L894,
				"%s: Failed to create thread: %08x\n",
				"OSInstallSystemLISR", psEnvData->misrThread);
#endif
		return PVRSRV_ERROR_UNABLE_TO_INSTALL_ISR;
	}
	if (sceKernelRegisterIntrHandler(ui32Irq, "SceGpuLisr", 0,
			SystemISRWrapper, pvSysData, 80, 15, NULL) != 0)
		goto deleteThread;
	psEnvData->bLISRInstalled = IMG_TRUE;
	psEnvData->misrRunning = IMG_TRUE;
	psEnvData->ui32IRQ = ui32Irq;
	psEnvData->pvISRCookie = psSysData;
	psEnvData->misrEventFlag = sceKernelCreateEventFlag("SceGpuIsr", 0, 0, NULL);
	if (psEnvData->misrEventFlag < 0)
		goto deleteThread;
	sceKernelStartThread(psEnvData->misrThread, sizeof(psEnvData), &psEnvData);
	sceKernelEnableIntr(ui32Irq);
	return PVRSRV_OK;

deleteThread:
	sceKernelDeleteThread(psEnvData->misrThread);
	return PVRSRV_ERROR_UNABLE_TO_INSTALL_ISR;
}

PVRSRV_ERROR OSUninstallSystemLISR(IMG_VOID *pvSysData)
{
	SYS_DATA *psSysData = pvSysData;
	ENV_DATA *psEnvData = psSysData->pvEnvSpecificData;

	if (!psEnvData->bLISRInstalled)
		return PVRSRV_ERROR_ISR_NOT_INSTALLED;
	sceKernelReleaseIntrHandler(psEnvData->ui32IRQ);
	psEnvData->misrRunning = IMG_FALSE;
	sceKernelSetEventFlag(psEnvData->misrEventFlag, 1);
	sceKernelWaitThreadEnd(psEnvData->misrThread, NULL, NULL);
	sceKernelDeleteEventFlag(psEnvData->misrEventFlag);
	psEnvData->bLISRInstalled = IMG_FALSE;
	return PVRSRV_OK;
}

PVRSRV_ERROR OSInstallMISR(IMG_VOID *pvSysData)
{
	SYS_DATA *psSysData = pvSysData;
	ENV_DATA *psEnvData = psSysData->pvEnvSpecificData;

	if (psEnvData->bMISRInstalled)
		return PVRSRV_ERROR_ISR_ALREADY_INSTALLED;
	/* SCE thread dispatch replaces the pristine tasklet. */
	psEnvData->pfnMISR = PVRSRVMISR;
	psEnvData->pvMISRData = pvSysData;
	psEnvData->bMISRInstalled = IMG_TRUE;
	return PVRSRV_OK;
}

PVRSRV_ERROR OSUninstallMISR(IMG_VOID *pvSysData)
{
	SYS_DATA *psSysData = pvSysData;
	ENV_DATA *psEnvData = psSysData->pvEnvSpecificData;

	if (!psEnvData->bMISRInstalled)
		return PVRSRV_ERROR_ISR_NOT_INSTALLED;
	psEnvData->pfnMISR = NULL;
	psEnvData->pvMISRData = NULL;
	psEnvData->bMISRInstalled = IMG_FALSE;
	return PVRSRV_OK;
}

PVRSRV_ERROR OSScheduleMISR(IMG_VOID *pvSysData)
{
	SYS_DATA *psSysData = pvSysData;
	ENV_DATA *psEnvData = psSysData->pvEnvSpecificData;

	if (psEnvData->bMISRInstalled)
		sceKernelSetEventFlag(psEnvData->misrEventFlag, 1);
	return PVRSRV_OK;
}

PVRSRV_ERROR OSLockResource(PVRSRV_RESOURCE *psResource, IMG_UINT32 ui32ID)
{
	/* SCE native compare-and-set replaces pristine OS_TAS. */
	if (SceCpuForDriver_CDA96E81((volatile SceInt32 *)&psResource->ui32Lock,
			0, 1) != 0)
		return PVRSRV_ERROR_UNABLE_TO_LOCK_RESOURCE;
	psResource->ui32ID = ui32ID;
	return PVRSRV_OK;
}

PVRSRV_ERROR OSUnlockResource(PVRSRV_RESOURCE *psResource, IMG_UINT32 ui32ID)
{
	/* SCE omits the pristine diagnostics and Linux smp_mb. */
	if (!psResource->ui32Lock)
		return PVRSRV_ERROR_RESOURCE_NOT_LOCKED;
	if (psResource->ui32ID != ui32ID)
		return PVRSRV_ERROR_INVALID_LOCK_ID;
	psResource->ui32ID = 0;
	psResource->ui32Lock = 0;
	return PVRSRV_OK;
}

IMG_BOOL OSIsResourceLocked(PVRSRV_RESOURCE *psResource, IMG_UINT32 ui32ID)
{
	return psResource->ui32Lock == 1 && psResource->ui32ID == ui32ID
			? IMG_TRUE : IMG_FALSE;
}

PVRSRV_ERROR OSPowerLockWrap(IMG_BOOL bTryLock)
{
	return PVRSRV_OK;
}

IMG_VOID OSPowerLockUnwrap(IMG_VOID)
{
}

IMG_UINT32 OSReadHWReg(IMG_VOID *pvLinRegBaseAddr, IMG_UINT32 ui32Offset)
{
	return *(volatile IMG_UINT32 *)((IMG_UINT8 *)pvLinRegBaseAddr + ui32Offset);
}

IMG_VOID OSWriteHWReg(IMG_VOID *pvLinRegBaseAddr, IMG_UINT32 ui32Offset,
		IMG_UINT32 ui32Value)
{
	*(volatile IMG_UINT32 *)((IMG_UINT8 *)pvLinRegBaseAddr + ui32Offset) = ui32Value;
}

IMG_HANDLE OSAddTimer(PFN_TIMER_FUNC pfnTimerFunc, IMG_VOID *pvData,
		IMG_UINT32 ui32MsTimeout)
{
	TIMER_CALLBACK_DATA *psTimerCBData;

	if (!pfnTimerFunc)
		return NULL;
	psTimerCBData = AllocProcessGpuMemory(sizeof(*psTimerCBData));
	if (!psTimerCBData)
		return NULL;
	psTimerCBData->pfnTimerFunc = pfnTimerFunc;
	psTimerCBData->pvData = pvData;
	psTimerCBData->bActive = IMG_FALSE;
	psTimerCBData->sTimer = SceSystimerForDriver_E2B9D8E9(SCE_SYSTIMER_TYPE_WORD);
	if (psTimerCBData->sTimer < 0) {
#if FW_DEBUG
		SceDebugForDriver_D9703808(1, 0, &dbg_OSAddTimer_L1663,
				"Creating timer failed: %08x\n", psTimerCBData->sTimer);
#endif
		FreeProcessGpuMemory(psTimerCBData);
		return NULL;
	}
	SceSystimerForDriver_0450933B(psTimerCBData->sTimer, 3, 0);
	/* SCE computes a word timer interval at 48 MHz before widening. */
	SceSystimerForDriver_A73351C1(psTimerCBData->sTimer,
			(IMG_UINT64)(ui32MsTimeout * 48000));
	SceSystimerForDriver_CCCD1EE3(psTimerCBData->sTimer,
			(SceSysTimerCallback)OSTimerCallbackWrapper, 15, psTimerCBData);
	SceSystimerForDriver_7DEBA6A5(psTimerCBData->sTimer);
	return psTimerCBData;
}

PVRSRV_ERROR OSRemoveTimer(IMG_HANDLE hTimer)
{
	TIMER_CALLBACK_DATA *psTimerCBData = hTimer;

	SceSystimerForDriver_8358611F(psTimerCBData->sTimer);
	FreeProcessGpuMemory(psTimerCBData);
	return PVRSRV_OK;
}

PVRSRV_ERROR OSEnableTimer(IMG_HANDLE hTimer)
{
	TIMER_CALLBACK_DATA *psTimerCBData = hTimer;

	psTimerCBData->bActive = IMG_TRUE;
	SceSystimerForDriver_7DEBA6A5(psTimerCBData->sTimer);
	SceSystimerForDriver_59EBD22D(psTimerCBData->sTimer);
	return PVRSRV_OK;
}

PVRSRV_ERROR OSDisableTimer(IMG_HANDLE hTimer)
{
	TIMER_CALLBACK_DATA *psTimerCBData = hTimer;

	psTimerCBData->bActive = IMG_FALSE;
	SceSystimerForDriver_BF8D42B0(psTimerCBData->sTimer);
	return PVRSRV_OK;
}

/* SCE native user-copy services replace Linux copy_to_user/from_user. */
PVRSRV_ERROR CopyToUserWrapper(PVRSRV_PER_PROCESS_DATA *psPerProc,
		IMG_VOID *pvDest, const IMG_VOID *pvSrc, IMG_UINT32 ui32Bytes)
{
	return SceSysmemForDriver_6D88EF8A(pvDest, pvSrc, ui32Bytes) == 0
			? PVRSRV_OK : PVRSRV_ERROR_FAILED_TO_COPY_VIRT_MEMORY;
}

PVRSRV_ERROR CopyFromUserWrapper(PVRSRV_PER_PROCESS_DATA *psPerProc,
		IMG_VOID *pvDest, const IMG_VOID *pvSrc, IMG_UINT32 ui32Bytes)
{
	return SceSysmemForDriver_BC996A7A(pvDest, pvSrc, ui32Bytes) == 0
			? PVRSRV_OK : PVRSRV_ERROR_FAILED_TO_COPY_VIRT_MEMORY;
}

/* SCE delegates address validation to the native user-copy service. */
IMG_BOOL OSAccessOK(IMG_VERIFY_TEST eVerification, IMG_VOID *pvUserPtr,
		IMG_SIZE_T ui32Bytes)
{
	return IMG_TRUE;
}

int sceGpuMemoryInit(void);
int sceGpuMmuInit(void);

/* SCE OS startup ignores native memory/MMU initialization failures. */
int InitGpuMemorySupport(void)
{
	sceGpuMemoryInit();
	sceGpuMmuInit();
	return 0;
}

/* Native OS-port hooks are empty in both TOOL builds. */
void GpuShutdownHook(void)
{
}

void GpuStartupHook(void)
{
}
