/*
 * PowerVR ES4 kernel driver.
 *
 * PLACEHOLDER: initialization, bridge services, MMU and device management
 * are being recovered from the original; the remaining STUB bodies are T2.
 */

#include <skeleton.h>
#include "sce_ddk_port.h"
#include <intrmgr/syscall.h>
#include <sysmem/procevent.h>
#include <sysmem/suspend.h>
#include <string.h>
#include <threadmgr/threadmgr.h>
#include <sysmem/sysclib.h>
#include <power/power.h>
#include <processmgr/processmgr.h>

/* SCE stores the services mutex UID in the native module's zeroed state. */
SceUID gPVRSRVLock;
IMG_VOID LinuxLockMutex(int *psMutex);
IMG_VOID LinuxUnLockMutex(int *psMutex);
IMG_UINT32 OSGetCurrentProcessIDKM(IMG_VOID);
PVRSRV_ERROR PVRSRVProcessConnect(IMG_UINT32 ui32PID, IMG_UINT32 ui32Flags);
IMG_VOID PVRSRVProcessDisconnect(IMG_UINT32 ui32PID);

/* SCE native user entry: start services and select the default GPU clock. */
int PVRSRVOpen(void)
{
	SceUInt32 context = sceSyscallEnter();
	int result;

	LinuxLockMutex(&gPVRSRVLock);
	if (PVRSRVProcessConnect(OSGetCurrentProcessIDKM(), 0) != PVRSRV_OK) {
		result = -1;
	} else {
		result = kscePowerGetGpuClockFrequencyProc(0);
		if (result != 0)
			result = 0;
		else
			kscePowerSetGpuClockFrequencyProc(0, 111);
	}
	LinuxUnLockMutex(&gPVRSRVLock);
	sceSyscallLeave(context);
	return result;
}

int PVRSRVRelease(void)
{
	SceUInt32 context = sceSyscallEnter();

	LinuxLockMutex(&gPVRSRVLock);
	PVRSRVProcessDisconnect(OSGetCurrentProcessIDKM());
	LinuxUnLockMutex(&gPVRSRVLock);
	sceSyscallLeave(context);
	return 0;
}

unsigned SetRemoteAllocProcess(unsigned pid);
IMG_VOID PVRDPFInit(IMG_VOID);
void GpuStartupHook(void);
void GpuShutdownHook(void);
void LinuxInitMutex(SceUID *psMutex);
int CreateSignalWaitObjects(void);
int CreateLockupWaitObjects(void);
SceUID SGXCleanupMutexInit(void);
int InitGpuMemorySupport(void);
PVRSRV_ERROR CommonBridgeInit(IMG_VOID);
PVRSRV_ERROR SysInitialise(IMG_VOID);
IMG_VOID sceGpuBridgeDeInit(IMG_VOID);
int sceGpuProcessCreateEvent(SceUID pid);
int sceGpuProcessExitEvent(SceUID pid);
int sceGpuProcessKillEvent(SceUID pid);
int sceGpuProcessStopEvent(SceUID pid, int eventType);
int sceGpuProcessStartEvent(SceUID pid, int eventType);
int sceGpuSysEventHandler(int eventId, int eventType);
int CheckRegisterDumpRegisterLists(void);
int InitWaitModObjList(void);
int CreateSuspendWaitEvent(void);

/* SCE startup registers native callbacks after initializing the DDK services. */
int module_start(SceSize arglen, const void *argp)
{
	SceKernelProcEventHandler handler;
	SceUID processHandler;
	int result;

	SetRemoteAllocProcess(0x10005);
	PVRDPFInit();
	GpuStartupHook();
	LinuxInitMutex(&gPVRSRVLock);
	if (CreateSignalWaitObjects() < 0 || CreateLockupWaitObjects() < 0 ||
	    SGXCleanupMutexInit() < 0 || InitGpuMemorySupport() != 0) {
		result = 2;
		goto failure;
	}
	CommonBridgeInit();
	if (SysInitialise() != PVRSRV_OK) {
		result = 2;
		goto failure;
	}

	memset(&handler, 0, sizeof(handler));
	handler.size = sizeof(handler);
	/* These callbacks consume only the leading arguments of the native ABI. */
	handler.create = (int (*)(SceUID, ProcEventCreateParam *, int))
			sceGpuProcessCreateEvent;
	handler.exit = (int (*)(SceUID, ProcEventParam *, int))
			sceGpuProcessExitEvent;
	handler.kill = (int (*)(SceUID, ProcEventParam *, int))
			sceGpuProcessKillEvent;
	handler.suspend = (int (*)(SceUID, int, ProcEventParam *, int))
			sceGpuProcessStopEvent;
	handler.resume = (int (*)(SceUID, int, ProcEventParam *, int))
			sceGpuProcessStartEvent;
	processHandler = sceKernelRegisterProcEventHandler(
			"SceGpuProcessEventHandler", &handler, 0);
	if (processHandler < 0) {
		result = processHandler;
		goto failure;
	}
	result = sceKernelRegisterSysEventHandler("SceGpu",
			(SceKernelSysEventHandler)sceGpuSysEventHandler, NULL);
	if (result < 0) {
		SceProcEventForDriver_3DED57CC(processHandler);
		goto failure;
	}
	if (CheckRegisterDumpRegisterLists() < 0 || InitWaitModObjList() < 0 ||
	    CreateSuspendWaitEvent() < 0) {
		result = 2;
		goto failure;
	}
	return 0;

failure:
	sceGpuBridgeDeInit();
	GpuShutdownHook();
	return result;
}

/* These imports are private declarations while shared ABI inputs are frozen.
 * Native callers pass process/size/options and process/address respectively.
 */

void *OSAllocMem(unsigned size)
{
	return sceKernelAllocRemoteProcessHeap(0x10005, size, 0);
}

int OSFreeMem(void *address)
{
	return sceKernelFreeRemoteProcessHeap(0x10005, address);
}

void *OSMemCopy(void *destination, const void *source, unsigned size)
{
	return memcpy(destination, source, size);
}

void *OSMemSet(void *destination, int value, unsigned size)
{
	return memset(destination, value, size);
}

int OSProcHasPrivSrvInit(void)
{
	return 1;
}
