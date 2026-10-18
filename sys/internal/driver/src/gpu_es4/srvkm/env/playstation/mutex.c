/* Sony env/playstation/mutex.c: SCE native mutex/remote-allocation port.
 * Pristine IMG Linux mutex implementation remains in the vendor directory.
 */
#include <threadmgr/threadmgr.h>
#include <sysmem/debug.h>
#include <stddef.h>
#include "../../sce_ddk_port.h"

unsigned SetRemoteAllocProcess(unsigned pid);

void LinuxInitMutex(SceUID *psPVRSRVMutex)
{
	SceKernelMutexOptParam opt;
	SceUID uid;

	opt.ceilingPriority = 0;
	opt.size = sizeof(opt);
	uid = sceKernelCreateMutex("SceGpuSrvKmMutex", 0x2000, 0, &opt);
	*psPVRSRVMutex = uid < 0 ? 0 : uid;
}

#if FW_DEBUG
static const SceKernelDebugInfo dbg_LinuxLockMutexForPid_L53 = {
	0xc40adf61, 0x30e81352, 0x0307cf00,
	"env/playstation/mutex.c", 53, "LinuxLockMutexForPid"
};
#endif

unsigned LinuxLockMutexForPid(SceUID *psPVRSRVMutex, SceUID pid)
{
	int res;

	res = sceKernelLockMutex(*psPVRSRVMutex, 1, NULL);
	if (res != 0) {
#if FW_DEBUG
		SceDebugForDriver_D9703808(1, 0, &dbg_LinuxLockMutexForPid_L53,
				"%s failed with error: %08x\n", "LinuxLockMutexForPid", res);
#endif
		pid = -1;
	}
	return SetRemoteAllocProcess(pid);
}

unsigned LinuxLockMutex(SceUID *psPVRSRVMutex)
{
	return LinuxLockMutexForPid(psPVRSRVMutex, sceKernelGetProcessId());
}

/* SCE maps the native failure to IMG's mutex-interrupted result. */
int LinuxTryLockMutex(SceUID *psPVRSRVMutex)
{
	return sceKernelTryLockMutex(*psPVRSRVMutex, 1) != 0 ?
		PVRSRV_ERROR_MUTEX_INTERRUPTIBLE_ERROR : PVRSRV_OK;
}

int LinuxUnLockMutex(SceUID *psPVRSRVMutex)
{
	SetRemoteAllocProcess((unsigned)-1);
	return sceKernelUnlockMutex(*psPVRSRVMutex, 1);
}
