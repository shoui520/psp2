/*
 * PowerVR ES4 kernel driver.
 *
 * PLACEHOLDER: initialization, bridge services, MMU and device management
 * are being recovered from the original; the remaining STUB bodies are T2.
 */

#include <skeleton.h>
#include <gpu/sce_ddk_port.h>
#include <intrmgr/syscall.h>
#include <sysmem/procevent.h>
#include <sysmem/suspend.h>
#include <string.h>
#include <threadmgr/threadmgr.h>
#include <sysmem/sysclib.h>
#include <power/power.h>
#include <processmgr/processmgr.h>
#include <gpu/osfunc_sce.h>
#include "bridge_sce.h"
#include "system_sce.h"
#include "sgxkick_sce.h"
#include <gpu/power_sce.h>

/* SCE stores the services mutex UID in the native module's zeroed state. */
SceUID gPVRSRVLock;

/* SCE native user entry: start services and select the default GPU clock. */
int PVRSRVOpen(void)
{
	SceUInt32 context = sceSyscallEnter();
	int result;

	LinuxLockMutex(&gPVRSRVLock);
	if (PVRSRVProcessConnect(OSGetCurrentProcessIDKM(), 0) != PVRSRV_OK) {
		result = -1;
	} else {
		result = ScePowerForDriver_642E0AF2(0);
		if (result != 0)
			result = 0;
		else
			ScePowerForDriver_B104EFE2(0, 111);
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
int CreateSignalWaitObjects(void);
int CreateLockupWaitObjects(void);
int CheckRegisterDumpRegisterLists(void);
int InitWaitModObjList(void);

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
	handler.create = (ProcEventCreateHandler)
			sceGpuProcessCreateEvent;
	handler.exit = (ProcEventHandler)
			sceGpuProcessExitEvent;
	handler.kill = (ProcEventHandler)
			sceGpuProcessKillEvent;
	handler.suspend = (ProcEventPhaseHandler)
			sceGpuProcessStopEvent;
	handler.resume = (ProcEventPhaseHandler)
			sceGpuProcessStartEvent;
	processHandler = sceKernelRegisterProcEventHandler(
			"SceGpuProcessEventHandler", &handler, NULL);
	if (processHandler < 0) {
		result = processHandler;
		goto failure;
	}
	result = sceKernelRegisterSysEventHandler("SceGpu",
			(SceKernelSysEventHandler)sceGpuSysEventHandler, NULL);
	if (result < 0) {
		sceKernelUnregisterProcEventHandler(processHandler);
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
