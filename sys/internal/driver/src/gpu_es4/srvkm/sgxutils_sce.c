/* IMG devices/sgx/sgxutils.c, GPL-2.0. Pristine candidate and license
 * remain in contrib/img-sgx-ddk-1.7.862890; SCE deltas marked below.
 */
#include "sgxutils_sce.h"
#include "gpu-dump.h"
#include "perproc_sce.h"
#include "sync_sce.h"
#include "render_sce.h"
#include <gpu/sgxmkif_sce.h>
#include "sgx543_v1.164defs.h"
#include <stddef.h>
#include <string.h>
#include <psp2kern/kernel/threadmgr/mutex.h>

/* Pristine sgx_mkif_km.h register-bank selection for SGX MP. */
#define SGX_REG_BANK_SHIFT 14
#define SGX_REG_BANK_SIZE (1U << SGX_REG_BANK_SHIFT)
#define SGX_REG_BANK_BASE_INDEX 2
#define SGX_MP_CORE_SELECT(x, i) ((x) + (((i) + SGX_REG_BANK_BASE_INDEX) * SGX_REG_BANK_SIZE))

/* SCE serializes asynchronous context cleanup using this kernel mutex. */
SceUID gSGXCleanupMutex;

IMG_UINT32 OSClockus(IMG_VOID);
IMG_VOID OSSleepms(IMG_UINT32 ui32Timems);
IMG_VOID OSWaitus(IMG_UINT32 ui32Timeus);
IMG_VOID SceCpuForDriver_E813EBB2(IMG_VOID);
IMG_VOID OSWriteHWReg(IMG_VOID *pvLinRegBaseAddr, IMG_UINT32 ui32Offset,
		IMG_UINT32 ui32Value);
PVRSRV_ERROR PVRSRVSetDevicePowerStateKM(IMG_UINT32 ui32DeviceIndex,
		PVRSRV_DEV_POWER_STATE eNewPowerState, IMG_UINT32 ui32CallerID,
		IMG_BOOL bRetainMutex);
IMG_VOID PVRSRVPowerUnlock(IMG_UINT32 ui32CallerID);
PVRSRV_ERROR OSScheduleMISR(SYS_DATA *psSysData);
IMG_BOOL PVRSRVIsDevicePowered(IMG_UINT32 ui32DeviceIndex);
PVRSRV_ERROR SGXCleanupRequest(PVRSRV_DEVICE_NODE *psDeviceNode,
		IMG_DEV_VIRTADDR *psHWDataDevVAddr, IMG_UINT32 ui32CleanupType,
		IMG_BOOL bForceCleanup);
PVRSRV_ERROR OSFreeMem_Impl(IMG_UINT32 ui32Flags, IMG_SIZE_T ui32Size,
		IMG_VOID *pvLinAddr, IMG_HANDLE hBlockAlloc);
PVRSRV_ERROR OSAllocMem_Impl(IMG_UINT32 ui32Flags, IMG_SIZE_T ui32Size,
		IMG_VOID **ppvLinAddr, IMG_HANDLE *phBlockAlloc);
RESMAN_ITEM *ResManRegisterRes(RESMAN_CONTEXT *psResManContext,
		IMG_UINT32 ui32ResType, IMG_VOID *pvParam, IMG_UINT32 ui32Param,
		RESMAN_FREE_FN pfnFreeResource);
PVRSRV_ERROR ResManFreeResByPtr(RESMAN_ITEM *psResItem, IMG_BOOL bForceCleanup);
RESMAN_ITEM *ResManFindResourceByCriteria(RESMAN_CONTEXT *psResManContext,
		IMG_UINT32 ui32SearchCriteria, IMG_UINT32 ui32ResType,
		IMG_VOID *pvParam, IMG_UINT32 ui32Param);
extern SceUID gPVRSRVLock;
IMG_UINT32 GetRemoteAllocProcess(IMG_VOID);
unsigned LinuxLockMutexForPid(SceUID *psPVRSRVMutex, SceUID pid);
int LinuxUnLockMutex(SceUID *psPVRSRVMutex);
PVRSRV_ERROR PollForValueKM(volatile IMG_UINT32 *pui32LinMemAddr,
		IMG_UINT32 ui32Value, IMG_UINT32 ui32Mask, IMG_UINT32 ui32Timeoutus,
		IMG_UINT32 ui32PollPeriodus, IMG_BOOL bAllowPreemption);

/* Pristine inline kernel-ring acquisition, using the common timeout loop. */
static inline SGXMKIF_COMMAND *SGXAcquireKernelCCBSlot(PVRSRV_SGX_CCB_INFO *psCCB)
{
	IMG_UINT32 uiOffset, uiStart, uiCurrent;
	IMG_INT32 iNotLastLoop;

	for (uiOffset = 0, uiStart = OSClockus(), uiCurrent = uiStart + 1,
			iNotLastLoop = 1;
			((uiCurrent - uiStart + uiOffset) < 500000) || iNotLastLoop--;
			uiCurrent = OSClockus(),
			uiOffset = uiCurrent < uiStart ? ~0U - uiStart : uiOffset,
			uiStart = uiCurrent < uiStart ? 0 : uiStart) {
		if (((*psCCB->pui32WriteOffset + 1) & 255) != *psCCB->pui32ReadOffset)
			return &psCCB->psCommands[*psCCB->pui32WriteOffset];
		OSSleepms(1);
	}
	return NULL;
}

PVRSRV_ERROR SGXScheduleCCBCommand(PVRSRV_DEVICE_NODE *psDeviceNode,
		SGXMKIF_CMD_TYPE eCmdType, SGXMKIF_COMMAND *psCommandData,
		IMG_UINT32 ui32CallerID, IMG_UINT32 ui32PDumpFlags,
		IMG_HANDLE hDevMemContext, IMG_BOOL bLastInScene)
{
	PVRSRV_SGX_CCB_INFO *psKernelCCB;
	PVRSRV_SGXDEV_INFO *psDevInfo = psDeviceNode->pvDevice;
	SGXMKIF_COMMAND *psSGXCommand;

	/* SCE: no PDUMP, MMU-invalidation insertion or pending system cache op. */
	(void)ui32CallerID;
	(void)ui32PDumpFlags;
	(void)hDevMemContext;
	(void)bLastInScene;
	psKernelCCB = psDevInfo->psKernelCCBInfo;
	psSGXCommand = SGXAcquireKernelCCBSlot(psKernelCCB);
	if (!psSGXCommand)
		return PVRSRV_ERROR_TIMEOUT;
	psCommandData->ui32CacheControl = psDevInfo->ui32CacheControl;
	psDevInfo->ui32CacheControl = 0;
	*psSGXCommand = *psCommandData;
	if (eCmdType >= SGXMKIF_CMD_MAX)
		return PVRSRV_ERROR_INVALID_CCB_COMMAND;
	/* SCE cache visibility calls surround each ring/control publication. */
	SceCpuForDriver_E813EBB2();
	psSGXCommand->ui32ServiceAddress = psDevInfo->aui32HostKickAddr[eCmdType];
	SceCpuForDriver_E813EBB2();
	*psKernelCCB->pui32WriteOffset = (*psKernelCCB->pui32WriteOffset + 1) & 255;
	SceCpuForDriver_E813EBB2();
	*psDevInfo->pui32KernelCCBEventKicker = (*psDevInfo->pui32KernelCCBEventKicker + 1) & 255;
	SceCpuForDriver_E813EBB2();
	/* Genuine ARM device-memory boundaries from the SCE port. */
	__asm__ volatile("dmb sy" ::: "memory");
	OSWriteHWReg(psDevInfo->pvRegsBaseKM, SGX_MP_CORE_SELECT(EUR_CR_EVENT_KICK, 0),
			EUR_CR_EVENT_KICK_NOW_MASK);
	__asm__ volatile("dsb sy" ::: "memory");
	return PVRSRV_OK;
}

PVRSRV_ERROR SGXScheduleCCBCommandKM(PVRSRV_DEVICE_NODE *psDeviceNode,
		SGXMKIF_CMD_TYPE eCmdType, SGXMKIF_COMMAND *psCommandData,
		IMG_UINT32 ui32CallerID, IMG_UINT32 ui32PDumpFlags,
		IMG_HANDLE hDevMemContext, IMG_BOOL bLastInScene)
{
	PVRSRV_ERROR eError;

	eError = PVRSRVSetDevicePowerStateKM(psDeviceNode->sDevId.ui32DeviceIndex,
			PVRSRV_DEV_POWER_STATE_ON, ui32CallerID, IMG_TRUE);
	if (eError == PVRSRV_OK) {
		psDeviceNode->bReProcessDeviceCommandComplete = IMG_FALSE;
	} else {
		if (eError != PVRSRV_ERROR_RETRY)
			return eError;
		if (ui32CallerID == (IMG_UINT32)-3) {
			psDeviceNode->bReProcessDeviceCommandComplete = IMG_TRUE;
			eError = PVRSRV_OK;
			/* SCE SysAcquireData inline is a direct system-owner load. */
			OSScheduleMISR(gpsSysData);
		}
		return eError;
	}
	eError = SGXScheduleCCBCommand(psDeviceNode, eCmdType, psCommandData,
			ui32CallerID, ui32PDumpFlags, hDevMemContext, bLastInScene);
	PVRSRVPowerUnlock(ui32CallerID);
	return eError;
}

/* Pristine queue command; SCE omits diagnostics and returns its status. */
PVRSRV_ERROR SGXScheduleProcessQueuesKM(PVRSRV_DEVICE_NODE *psDeviceNode)
{
	PVRSRV_SGXDEV_INFO *psDevInfo = psDeviceNode->pvDevice;
	struct SGXMKIF_HOST_CTL *psHostCtl =
		psDevInfo->psKernelSGXHostCtlMemInfo->pvLinAddrKM;
	SGXMKIF_COMMAND sCommand;

	memset(&sCommand, 0, sizeof(sCommand));
	if (psHostCtl->ui32PowerStatus & PVRSRV_USSE_EDM_POWMAN_NO_WORK)
		return PVRSRV_OK;
	return SGXScheduleCCBCommandKM(psDeviceNode, SGXMKIF_CMD_PROCESS_QUEUES,
			&sCommand, (IMG_UINT32)-3, 0, NULL, IMG_FALSE);
}

/* Pristine active-power transition, with the post-event helper inlined and
 * diagnostics/PDUMP omitted by the SCE build.
 */
IMG_VOID SGXTestActivePowerEvent(PVRSRV_DEVICE_NODE *psDeviceNode,
		IMG_UINT32 ui32CallerID)
{
	PVRSRV_SGXDEV_INFO *psDevInfo = psDeviceNode->pvDevice;
	struct SGXMKIF_HOST_CTL *psSGXHostCtl = psDevInfo->psSGXHostCtl;
	PVRSRV_ERROR eError;

	if (!(psSGXHostCtl->ui32InterruptFlags & 2U) ||
	    (psSGXHostCtl->ui32InterruptClearFlags & 2U))
		return;
	psSGXHostCtl->ui32InterruptClearFlags |= 2U;
	eError = PVRSRVSetDevicePowerStateKM(psDeviceNode->sDevId.ui32DeviceIndex,
			PVRSRV_DEV_POWER_STATE_OFF, ui32CallerID, IMG_FALSE);
	if (eError == PVRSRV_OK) {
		psDevInfo = psDeviceNode->pvDevice;
		psDevInfo->psSGXHostCtl->ui32NumActivePowerEvents++;
		if (psDevInfo->psSGXHostCtl->ui32PowerStatus & 0x10U) {
			if (ui32CallerID == (IMG_UINT32)-3)
				psDeviceNode->bReProcessDeviceCommandComplete = IMG_TRUE;
			else
				SGXScheduleProcessQueuesKM(psDeviceNode);
		}
	} else if (eError == PVRSRV_ERROR_RETRY) {
		psSGXHostCtl->ui32InterruptClearFlags &= ~2U;
	}
}

IMG_BOOL SGXIsDevicePowered(PVRSRV_DEVICE_NODE *psDeviceNode)
{
	return PVRSRVIsDevicePowered(psDeviceNode->sDevId.ui32DeviceIndex);
}

PVRSRV_ERROR SGXGetInternalDevInfoKM(IMG_HANDLE hDevCookie,
		SGX_INTERNAL_DEVINFO *psSGXInternalDevInfo)
{
	PVRSRV_SGXDEV_INFO *psDevInfo =
		((PVRSRV_DEVICE_NODE *)hDevCookie)->pvDevice;

	psSGXInternalDevInfo->ui32Flags = psDevInfo->ui32Flags;
	psSGXInternalDevInfo->bForcePTOff = psDevInfo->bForcePTOff;
	psSGXInternalDevInfo->hostControl.hHostCtlKernelMemInfoHandle =
		(IMG_SID)psDevInfo->psKernelSGXHostCtlMemInfo;
	return PVRSRV_OK;
}

/* SCE releases the bridge mutex while waiting on serialized cleanup. */
PVRSRV_ERROR SGXCleanupRequest(PVRSRV_DEVICE_NODE *psDeviceNode,
		IMG_DEV_VIRTADDR *psHWDataDevVAddr, IMG_UINT32 ui32CleanupType,
		IMG_BOOL bForceCleanup)
{
	PVRSRV_SGXDEV_INFO *psDevInfo = psDeviceNode->pvDevice;
	struct SGXMKIF_HOST_CTL *psHostCtl =
		psDevInfo->psKernelSGXHostCtlMemInfo->pvLinAddrKM;
	SGXMKIF_COMMAND sCommand;
	IMG_UINT32 pid;
	PVRSRV_ERROR eError;
	int lockResult;

	memset(&sCommand, 0, sizeof(sCommand));
	if (bForceCleanup == IMG_TRUE)
		goto complete;
	pid = GetRemoteAllocProcess();
	LinuxUnLockMutex(&gPVRSRVLock);
	lockResult = ksceKernelLockMutex(gSGXCleanupMutex, 1, NULL);
	LinuxLockMutexForPid(&gPVRSRVLock, pid);
	if (lockResult < 0)
		return PVRSRV_ERROR_UNABLE_TO_LOCK_RESOURCE;
	sCommand.ui32Data[0] = ui32CleanupType;
	sCommand.ui32Data[1] = psHWDataDevVAddr == NULL ? 0 : psHWDataDevVAddr->uiAddr;
	eError = SGXScheduleCCBCommandKM(psDeviceNode, SGXMKIF_CMD_CLEANUP,
			&sCommand, 0xffffffffU, 0, NULL, IMG_FALSE);
	if (eError != PVRSRV_OK) {
		ksceKernelUnlockMutex(gSGXCleanupMutex, 1);
		return eError;
	}
	LinuxUnLockMutex(&gPVRSRVLock);
	eError = PollForValueKM(&psHostCtl->ui32CleanupStatus, 1, 1,
			5000000, 1000, IMG_TRUE) == PVRSRV_OK ?
		PVRSRV_OK : PVRSRV_ERROR_TIMEOUT;
	LinuxLockMutexForPid(&gPVRSRVLock, pid);
	ksceKernelUnlockMutex(gSGXCleanupMutex, 1);
	if (eError != PVRSRV_OK)
		return eError;
complete:
	psHostCtl->ui32CleanupStatus &= ~1U;
	psDevInfo->ui32CacheControl |= 12U;
	return PVRSRV_OK;
}

PVRSRV_ERROR SGXCleanupHWRenderContextCallback(IMG_VOID *pvParam,
		IMG_UINT32 ui32Param, IMG_BOOL bForceCleanup)
{
	SGX_HW_RENDER_CONTEXT_CLEANUP *psCleanup = pvParam;
	PVRSRV_ERROR eError;

	(void)ui32Param;
	/* SCE: the caller retains the device-memory allocation. */
	eError = SGXCleanupRequest(psCleanup->psDeviceNode,
			&psCleanup->sHWRenderContextDevVAddr, 2, bForceCleanup);
	OSFreeMem_Impl(1, sizeof(*psCleanup), psCleanup, psCleanup->hBlockAlloc);
	return eError;
}

PVRSRV_ERROR SGXCleanupHW2DContextCallback(IMG_VOID *pvParam,
		IMG_UINT32 ui32Param, IMG_BOOL bForceCleanup)
{
	SGX_HW_2D_CONTEXT_CLEANUP *psCleanup = pvParam;
	PVRSRV_ERROR eError;

	(void)ui32Param;
	/* SCE: the caller retains the device-memory allocation. */
	eError = SGXCleanupRequest(psCleanup->psDeviceNode,
			&psCleanup->sHW2DContextDevVAddr, 4, bForceCleanup);
	OSFreeMem_Impl(1, sizeof(*psCleanup), psCleanup, psCleanup->hBlockAlloc);
	return eError;
}

/* SCE native registration omits the upstream user copy and MMU mapping;
 * the caller supplies both the address and the complete memory owner.
 */
IMG_HANDLE SGXRegisterHWRenderContextKM(IMG_HANDLE hDeviceNode,
		IMG_DEV_VIRTADDR *psHWRenderContextDevVAddr,
		struct PVRSRV_KERNEL_MEM_INFO *psHWRenderContextMemInfo,
		PVRSRV_PER_PROCESS_DATA *psPerProc)
{
	SGX_HW_RENDER_CONTEXT_CLEANUP *psCleanup;
	IMG_HANDLE hBlockAlloc;
	RESMAN_ITEM *psResItem;

	if (OSAllocMem_Impl(1, sizeof(*psCleanup), (IMG_VOID **)&psCleanup,
			&hBlockAlloc) != PVRSRV_OK)
		return NULL;
	psCleanup->psDeviceNode = hDeviceNode;
	psCleanup->hBlockAlloc = hBlockAlloc;
	psCleanup->sHWRenderContextDevVAddr = *psHWRenderContextDevVAddr;
	psCleanup->psHWRenderContextMemInfo = psHWRenderContextMemInfo;
	psCleanup->bPrioritySet = IMG_FALSE;
	psResItem = ResManRegisterRes(psPerProc->hResManContext,
			RESMAN_TYPE_HW_RENDER_CONTEXT, psCleanup, 0,
			SGXCleanupHWRenderContextCallback);
	if (!psResItem) {
		OSFreeMem_Impl(1, sizeof(*psCleanup), psCleanup, psCleanup->hBlockAlloc);
		return NULL;
	}
	psCleanup->psResItem = psResItem;
	return psCleanup;
}

PVRSRV_ERROR SGXUnregisterHWRenderContextKM(IMG_HANDLE hHWRenderContext,
		IMG_BOOL bForceCleanup)
{
	SGX_HW_RENDER_CONTEXT_CLEANUP *psCleanup = hHWRenderContext;

	if (!psCleanup)
		return PVRSRV_ERROR_INVALID_PARAMS;
	return ResManFreeResByPtr(psCleanup->psResItem, bForceCleanup);
}

/* SCE native 2D registration retains caller-owned device memory. */
IMG_HANDLE SGXRegisterHW2DContextKM(IMG_HANDLE hDeviceNode,
		IMG_DEV_VIRTADDR *psHW2DContextDevVAddr,
		struct PVRSRV_KERNEL_MEM_INFO *psHW2DContextMemInfo,
		PVRSRV_PER_PROCESS_DATA *psPerProc)
{
	SGX_HW_2D_CONTEXT_CLEANUP *psCleanup;
	IMG_HANDLE hBlockAlloc;
	RESMAN_ITEM *psResItem;

	if (OSAllocMem_Impl(1, sizeof(*psCleanup), (IMG_VOID **)&psCleanup,
			&hBlockAlloc) != PVRSRV_OK)
		return NULL;
	psCleanup->psDeviceNode = hDeviceNode;
	psCleanup->hBlockAlloc = hBlockAlloc;
	psCleanup->sHW2DContextDevVAddr = *psHW2DContextDevVAddr;
	psCleanup->psHW2DContextMemInfo = psHW2DContextMemInfo;
	psResItem = ResManRegisterRes(psPerProc->hResManContext,
			RESMAN_TYPE_HW_2D_CONTEXT, psCleanup, 0,
			SGXCleanupHW2DContextCallback);
	if (!psResItem) {
		OSFreeMem_Impl(1, sizeof(*psCleanup), psCleanup, psCleanup->hBlockAlloc);
		return NULL;
	}
	psCleanup->psResItem = psResItem;
	return psCleanup;
}

PVRSRV_ERROR SGXUnregisterHW2DContextKM(IMG_HANDLE hHW2DContext,
		IMG_BOOL bForceCleanup)
{
	SGX_HW_2D_CONTEXT_CLEANUP *psCleanup = hHW2DContext;

	if (!psCleanup)
		return PVRSRV_ERROR_INVALID_PARAMS;
	return ResManFreeResByPtr(psCleanup->psResItem, bForceCleanup);
}

/* Pristine sync completion test, with Sony's diagnostics omitted. */
static inline IMG_BOOL SGX2DQuerySyncOpsComplete(PVRSRV_KERNEL_SYNC_INFO *psSyncInfo,
		IMG_UINT32 ui32ReadOpsPending, IMG_UINT32 ui32WriteOpsPending)
{
	PVRSRV_SYNC_DATA *psSyncData = psSyncInfo->psSyncData;

	return psSyncData->ui32ReadOpsComplete >= ui32ReadOpsPending &&
		psSyncData->ui32WriteOpsComplete >= ui32WriteOpsPending;
}

PVRSRV_ERROR SGX2DQueryBlitsCompleteKM(PVRSRV_SGXDEV_INFO *psDevInfo,
		PVRSRV_KERNEL_SYNC_INFO *psSyncInfo, IMG_BOOL bWaitForComplete)
{
	IMG_UINT32 ui32ReadOpsPending, ui32WriteOpsPending;
	IMG_UINT32 uiOffset, uiStart, uiCurrent;
	IMG_INT32 iNotLastLoop;

	(void)psDevInfo;
	ui32ReadOpsPending = psSyncInfo->psSyncData->ui32ReadOpsPending;
	ui32WriteOpsPending = psSyncInfo->psSyncData->ui32WriteOpsPending;
	if (SGX2DQuerySyncOpsComplete(psSyncInfo, ui32ReadOpsPending, ui32WriteOpsPending))
		return PVRSRV_OK;
	if (!bWaitForComplete)
		return PVRSRV_ERROR_CMD_NOT_PROCESSED;
	for (uiOffset = 0, uiStart = OSClockus(), uiCurrent = uiStart + 1,
			iNotLastLoop = 1;
			((uiCurrent - uiStart + uiOffset) < 500000) || iNotLastLoop--;
			uiCurrent = OSClockus(),
			uiOffset = uiCurrent < uiStart ? ~0U - uiStart : uiOffset,
			uiStart = uiCurrent < uiStart ? 0 : uiStart) {
		OSSleepms(1);
		if (SGX2DQuerySyncOpsComplete(psSyncInfo, ui32ReadOpsPending, ui32WriteOpsPending))
			return PVRSRV_OK;
		OSSleepms(1);
	}
	return PVRSRV_ERROR_TIMEOUT;
}

PVRSRV_ERROR SGXFlushHWRenderTargetKM(IMG_HANDLE psDeviceNode,
		IMG_DEV_VIRTADDR sHWRTDataSetDevVAddr, IMG_BOOL bForceCleanup)
{
	/* SCE omits the upstream nonzero-address assertion. */
	return SGXCleanupRequest(psDeviceNode, &sHWRTDataSetDevVAddr, 1,
			bForceCleanup);
}

IMG_VOID SGXWaitClocks(PVRSRV_SGXDEV_INFO *psDevInfo, IMG_UINT32 ui32SGXClocks)
{
	OSWaitus(1 + (ui32SGXClocks * 1000000 / psDevInfo->ui32CoreClockSpeed));
}

PVRSRV_ERROR SGXContextSuspend(PVRSRV_DEVICE_NODE *psDeviceNode,
		IMG_DEV_VIRTADDR *psHWContextDevVAddr, IMG_BOOL bResume)
{
	SGXMKIF_COMMAND sCommand;
	IMG_UINT32 ui32SuspendCommand = bResume ? 2 : 1;

	/* SCE uses an explicit memset and omits upstream diagnostics. */
	memset(&sCommand, 0, sizeof(sCommand));
	sCommand.ui32Data[0] = psHWContextDevVAddr->uiAddr;
	sCommand.ui32Data[1] = ui32SuspendCommand;
	return SGXScheduleCCBCommandKM(psDeviceNode, SGXMKIF_CMD_CONTEXTSUSPEND,
			&sCommand, (IMG_UINT32)-1, 0, NULL, IMG_FALSE);
}

/* SCE: select the process's first registered hardware render context. */
PVRSRV_ERROR SGXPerProcessContextSuspend(PVRSRV_PER_PROCESS_DATA *psPerProc,
		IMG_BOOL bResume)
{
	RESMAN_ITEM *psResItem;
	SGX_HW_RENDER_CONTEXT_CLEANUP *psCleanup;

	if (!psPerProc)
		return PVRSRV_ERROR_INVALID_PARAMS;
	psResItem = ResManFindResourceByCriteria(psPerProc->hResManContext,
			RESMAN_CRITERIA_RESTYPE, RESMAN_TYPE_HW_RENDER_CONTEXT, NULL, 0);
	if (!psResItem)
		return PVRSRV_ERROR_INVALID_CONTEXT;
	psCleanup = psResItem->pvParam;
	if (!psCleanup)
		return PVRSRV_ERROR_INVALID_CONTEXT;
	return SGXContextSuspend(psCleanup->psDeviceNode,
			&psCleanup->sHWRenderContextDevVAddr, bResume);
}

/* SCE supplies the complete native context instead of an arbitrary offset. */
PVRSRV_ERROR SGXSetRenderContextPriorityKM(IMG_HANDLE hHWRenderContext,
		IMG_UINT32 *pui32Priority)
{
	SGX_HW_RENDER_CONTEXT_CLEANUP *psCleanup = hHWRenderContext;
	struct SGXMKIF_HWRENDERCONTEXT *psHWRenderContext;

	if (!psCleanup)
		return PVRSRV_ERROR_INVALID_PARAMS;
	if (!pui32Priority)
		return PVRSRV_ERROR_INVALID_PARAMS;
	if (!psCleanup->psHWRenderContextMemInfo)
		return PVRSRV_ERROR_INVALID_PARAMS;
	psHWRenderContext = psCleanup->psHWRenderContextMemInfo->pvLinAddrKM;
	if (!psHWRenderContext)
		return PVRSRV_ERROR_INVALID_PARAMS;
	if (!psCleanup->bPrioritySet) {
		psCleanup->bPrioritySet = IMG_TRUE;
		psHWRenderContext->ui32Priority = *pui32Priority;
	}
	return PVRSRV_OK;
}

SceUID SGXCleanupMutexInit(IMG_VOID)
{
	gSGXCleanupMutex = ksceKernelCreateMutex("SceGpuCleanupMutex", 0, 0, NULL);
	return gSGXCleanupMutex;
}
