/* Sony env/playstation/process.c: native process and system lifecycle. */
#include <gpu/sce_ddk_port.h>
#include <gpu/perproc_sce.h>
#include <gpu/memory_sce.h>
#include <stddef.h>
#include <gpu/gpu-mmu.h>
#include <acmgr/acmgr.h>
#include <sysmem/debug.h>
#include "../../system_sce.h"
#include "../../handle-types.h"
#include "../../sgxkick_sce.h"
#include <gpu/power_sce.h>

extern int gPVRSRVLock;
int AddMemoryContextToList(GpuMemoryContext **output, unsigned pid);
int RemoveMemoryContextFromList(GpuMemoryContext *context, unsigned pid);
int GetMemoryContext(GpuMemoryContext **output, unsigned pid);
int sceGpuMmuAllocatePageTablePool(GpuPageTablePool *pool, unsigned pid);
int sceGpuUnregisterSalvage(int eventUid);
int sceGpuIsHalted(void);

SceUID exitingProcess = -1;
IMG_UINT32 gpuSuspended;

/* SCE: create the native GPU address space for a game process. */
int sceGpuProcessCreateEvent(SceUID pid)
{
	GpuMemoryContext *context;
	int result;

	if (sceSblACMgrIsGameProgram(pid)) {
		LinuxLockMutexForPid(&gPVRSRVLock, pid);
		result = AddMemoryContextToList(&context, pid);
		if (result == 0) {
			result = sceGpuMmuAllocatePageTablePool(
				&context->mmu.pageTablePool, pid);
			if (result < 0)
				RemoveMemoryContextFromList(context, pid);
		}
		LinuxUnLockMutex(&gPVRSRVLock);
	} else {
		result = 0;
	}
	return result;
}

/* SCE: disconnect services before removing the native process address space. */
int sceGpuProcessExitEvent(SceUID pid)
{
	GpuMemoryContext *context;

	LinuxLockMutexForPid(&gPVRSRVLock, pid);
	PVRSRVProcessDisconnect(pid);
	if (GetMemoryContext(&context, pid) == 0)
		RemoveMemoryContextFromList(context, pid);
	LinuxUnLockMutex(&gPVRSRVLock);
	sceGpuUnregisterSalvage(pid);
	return 0;
}

/* SCE: resume a suspended context before forced process teardown. */
int sceGpuProcessKillEvent(SceUID pid)
{
	GpuMemoryContext *context;
	PVRSRV_PER_PROCESS_DATA *psPerProc;

	LinuxLockMutexForPid(&gPVRSRVLock, pid);
	psPerProc = PVRSRVPerProcessData(pid);
	if (psPerProc && psPerProc->bContextSuspended == IMG_TRUE)
		SGXPerProcessContextSuspend(psPerProc, IMG_TRUE);
	PVRSRVProcessDisconnect(pid);
	if (GetMemoryContext(&context, pid) == 0)
		RemoveMemoryContextFromList(context, pid);
	LinuxUnLockMutex(&gPVRSRVLock);
	sceGpuUnregisterSalvage(pid);
	return 0;
}

/* SCE: synchronize process suspension with pending GPU source syncs. */
int sceGpuProcessStopEvent(SceUID pid, int eventType)
{
	PVRSRV_PER_PROCESS_DATA *psPerProc;
	PVRSRV_KERNEL_MEM_INFO *psMemInfo;
	unsigned i;
	int pending;

	switch (eventType) {
	case 0x202:
		if (sceGpuIsHalted() != 0)
			break;
		psPerProc = PVRSRVPerProcessData(pid);
		if (!psPerProc || psPerProc->bContextSuspendEnabled != IMG_TRUE)
			break;
		pending = 0;
		for (i = 0; i < psPerProc->numSourceSyncs; i++) {
			GpuSyncWait *wait = &psPerProc->sourceSyncs[i];

			if (!wait->memoryHandle)
				continue;
			psMemInfo = NULL;
			if (PVRSRVLookupHandle(psPerProc->psHandleBase,
					(void **)&psMemInfo,
					(IMG_SID)wait->memoryHandle, 5) == PVRSRV_OK) {
				IMG_UINT32 *value = (IMG_UINT32 *)
					((IMG_UINTPTR_T)psMemInfo->pvLinAddrKM -
					 psMemInfo->sDevVAddr.uiAddr +
					 wait->syncAddress.uiAddr);

				if (*value == wait->pendingValue)
					break;
				pending = 1;
			}
		}
		if (pending == 1 && i == psPerProc->numSourceSyncs)
			return (int)0x804c0006;
		break;
	case 0x800:
		LinuxLockMutex(&gPVRSRVLock);
		psPerProc = PVRSRVPerProcessData(pid);
		if (psPerProc && psPerProc->bContextSuspended == IMG_FALSE &&
				psPerProc->bContextSuspendEnabled == IMG_TRUE) {
			if (sceGpuIsHalted() == 0)
				SGXPerProcessContextSuspend(psPerProc, IMG_FALSE);
			psPerProc->bContextSuspended = IMG_TRUE;
		}
		sceGpuSignalSuspendWait();
		exitingProcess = -1;
		LinuxUnLockMutex(&gPVRSRVLock);
		break;
	case 0x200:
		sceGpuCancelSuspendWait();
		exitingProcess = pid;
		break;
	}
	return 0;
}

/* SCE: restore a suspended hardware context when the process starts. */
int sceGpuProcessStartEvent(SceUID pid, int eventType)
{
	PVRSRV_PER_PROCESS_DATA *psPerProc;

	if (eventType == 0x10000) {
		LinuxLockMutex(&gPVRSRVLock);
		psPerProc = PVRSRVPerProcessData(pid);
		if (psPerProc && psPerProc->bContextSuspended == IMG_TRUE) {
			SGXPerProcessContextSuspend(psPerProc, IMG_TRUE);
			psPerProc->bContextSuspended = IMG_FALSE;
		}
		LinuxUnLockMutex(&gPVRSRVLock);
	}
	return 0;
}

/* Sony system-event callback: quiesce services before suspend and resume. */
int sceGpuSysEventHandler(int eventId, int eventType)
{
	if (eventId != 0) {
		if (eventId != 1)
			return 0;
		if (eventType == 0x10005) {
			PVRSRVSetPowerStateKM(0);
			sceGpuSignalSuspendWait();
			gpuSuspended = 0;
		}
		return 0;
	}
	switch (eventType) {
	case 0x402:
		if (LinuxTryLockMutex(&gPVRSRVLock) != 0)
			return (int)0x804c0006;
		LinuxUnLockMutex(&gPVRSRVLock);
		break;
	case 0x4005:
		SCE_DBG_ASSERT(PVRSRVSetPowerStateKM(3) == PVRSRV_OK, 230);
#if !FW_DEBUG
		PVRSRVSetPowerStateKM(3);
#endif
		break;
	case 0x400:
		sceGpuCancelSuspendWait();
		gpuSuspended = 1;
		break;
	}
	return 0;
}

