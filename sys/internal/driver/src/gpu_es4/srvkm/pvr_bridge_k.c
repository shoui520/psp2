/* IMG pvr_bridge_k.c entry with the SCE native syscall/suspend port. */
#include "bridge_sce.h"
#include <gpu/perproc_sce.h>
#include <intrmgr/syscall.h>
#include <stddef.h>

extern int gPVRSRVLock;
extern IMG_UINT32 gpuSuspended;
extern SceUID exitingProcess;
extern struct _PVRSRV_HANDLE_BASE_ *gpsKernelHandleBase;
unsigned LinuxLockMutex(int *psMutex);
int LinuxUnLockMutex(int *psMutex);
IMG_UINT32 OSGetCurrentProcessIDKM(IMG_VOID);
PVRSRV_PER_PROCESS_DATA *PVRSRVPerProcessData(IMG_UINT32 ui32PID);
PVRSRV_ERROR PVRSRVLookupHandle(struct _PVRSRV_HANDLE_BASE_ *psBase,
		IMG_VOID **ppvData, IMG_SID hHandle, IMG_UINT32 eType);
PVRSRV_ERROR CopyFromUserWrapper(PVRSRV_PER_PROCESS_DATA *psPerProc,
		IMG_VOID *destination, const IMG_VOID *source, IMG_SIZE_T size);
IMG_INT BridgedDispatchKM(PVRSRV_PER_PROCESS_DATA *psPerProc,
		PVRSRV_BRIDGE_PACKAGE *psBridgePackageKM);
int sceGpuWaitSuspend(void);

/* SCE replaces Linux file/ioctl arguments with a native bridge-package pointer.
 * The unused first argument is retained in the exported ABI.
 */
int PVRSRV_BridgeDispatchKM(int command, const PVRSRV_BRIDGE_PACKAGE *package)
{
	SceUInt32 context = sceSyscallEnter();
	IMG_UINT32 ui32PID = OSGetCurrentProcessIDKM();
	PVRSRV_BRIDGE_PACKAGE sBridgePackageKM;
	PVRSRV_PER_PROCESS_DATA *psPerProc;
	int err;

	(void)command;
	if (CopyFromUserWrapper(NULL, &sBridgePackageKM, package,
			sizeof(sBridgePackageKM)) != PVRSRV_OK) {
		sceSyscallLeave(context);
		return -14;
	}
	while (gpuSuspended) {
		if (sceGpuWaitSuspend() < 0)
			goto wait_failed;
	}
	/* SCE permits sync completion/reset bridges during process suspension. */
	while (exitingProcess == ui32PID &&
			sBridgePackageKM.ui32BridgeID != PVRSRV_BRIDGE_CHECK_SYNC_INFO_MOD_OBJ &&
			sBridgePackageKM.ui32BridgeID != PVRSRV_BRIDGE_MODIFY_PENDING_SYNC_OPS &&
			sBridgePackageKM.ui32BridgeID != PVRSRV_BRIDGE_MODIFY_COMPLETE_SYNC_OPS &&
			sBridgePackageKM.ui32BridgeID != PVRSRV_BRIDGE_SGX_RESET_SYNC_WAITS) {
		if (sceGpuWaitSuspend() < 0)
			goto wait_failed;
	}
	LinuxLockMutex(&gPVRSRVLock);
	if (sBridgePackageKM.ui32BridgeID == PVRSRV_BRIDGE_CONNECT_SERVICES) {
		psPerProc = PVRSRVPerProcessData(ui32PID);
		if (!psPerProc)
			goto invalid_handle;
	} else {
		if (PVRSRVLookupHandle(gpsKernelHandleBase, (void **)&psPerProc,
				(IMG_SID)sBridgePackageKM.hKernelServices, 1) != PVRSRV_OK ||
				psPerProc->ui32PID != ui32PID)
			goto invalid_handle;
	}
	sBridgePackageKM.ui32BridgeID -= 0x1000;
	err = BridgedDispatchKM(psPerProc, &sBridgePackageKM);
	goto unlock;
invalid_handle:
	err = -14;
unlock:
	LinuxUnLockMutex(&gPVRSRVLock);
	sceSyscallLeave(context);
	return err;
wait_failed:
	sceSyscallLeave(context);
	return 24;
}
