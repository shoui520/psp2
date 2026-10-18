/* Sony venezia/venezia.c: process-local suspend state. */
#include <psp2kern/kernel/processmgr.h>
#include <psp2kern/kernel/sysclib.h>
#include <sysmem/debug.h>
#include "../venezia_internal.h"
#include <lowio/pervasive.h>
#include <power/power.h>

VeneziaState g_veneziaState = { .processStorageKey = -1 };

extern int SceCodecEngineWrapperForDriver_0D3BB164(int command, ...);

#if FW_VERSION >= 0x04000000
extern int initVeneziaRpc(void); /* name unknown */
extern int waitVeneziaRpcSemas(void); /* name unknown */
extern int allocVeneziaImage(void); /* name unknown */
extern int bootVeneziaPhase2(void) __asm__("_sceVeneziaBootPhase2");
extern int veneziaClockSelector(int frequency); /* name unknown */
extern int initCodecEnginePerformanceMonitor(void); /* name unknown */
extern int signalVeneziaRpcSemas(void); /* name unknown */
extern int veneziaRpcClockDown(void) __asm__("_sceVeneziaRpcClockDown");
extern int startPerformanceMonitorSuspend(void); /* name unknown */
extern int isPerformanceMonitorSuspended(void); /* name unknown */
extern int veneziaSuspend(void) __asm__("_sceVeneziaSuspend");
extern int veneziaResume(void) __asm__("_sceVeneziaResume");
extern int resumePerformanceMonitor(void); /* name unknown */
extern void printVeneziaRpcBlockingCalls(SceUID pid) __asm__("_sceVeneziaRpcPrintBlockingCalls");
#else
extern int initVeneziaRpc(void);
extern int waitVeneziaRpcSemas(void);
extern int allocVeneziaImage(void);
extern int bootVeneziaPhase2(void) __asm__("_sceVeneziaBootPhase2");
extern int veneziaClockSelector(int frequency);
extern int initCodecEnginePerformanceMonitor(void);
extern int signalVeneziaRpcSemas(void);
extern int veneziaRpcClockDown(void) __asm__("_sceVeneziaRpcClockDown");
extern int startPerformanceMonitorSuspend(void);
extern int isPerformanceMonitorSuspended(void);
extern int veneziaSuspend(void) __asm__("_sceVeneziaSuspend");
extern int veneziaResume(void) __asm__("_sceVeneziaResume");
extern int resumePerformanceMonitor(void);
extern void printVeneziaRpcBlockingCalls(SceUID pid) __asm__("_sceVeneziaRpcPrintBlockingCalls");
#endif

int _sceVeneziaProcessCreateEntry(void)
{
	VeneziaProcessStorage *storage;
	int ret;

	ret = ksceKernelCreateProcessLocalStorage("SceVenezia", sizeof(*storage));
	if (ret < 0)
		goto out;
	storage = NULL;
	g_veneziaState.processStorageKey = ret;
	ret = ksceKernelGetProcessLocalStorageAddrForPid(0, ret, (void **)&storage, 1);
	if (ret < 0)
		goto out;
#if FW_DEBUG
	SceDebugForDriver_35A35322(storage != NULL,
			 SCE_DBGINFO_FUNC(191, "_sceVeneziaProcessCreateEntry"),
			 __builtin_return_address(0));
#endif
	memset(storage, 0, sizeof(*storage));
	ret = sceKernelInitializeFastMutex(&storage->suspendMutex, "SceVeneziaSuspend",
					  0x8000, NULL);
	if (ret < 0)
		goto out;
	storage->suspendLock = 0;
	storage->suspendLockCount = 0;
	storage->clockLockCount = 0;
	storage->suspendInProgress = 0;
	ret = initVeneziaRpc();
	if (ret < 0)
		goto out;
	ret = waitVeneziaRpcSemas();
	if (ret < 0)
		goto out;
	ret = allocVeneziaImage();
out:
	return ret;
}

int _sceVeneziaStartEntryPhase2(void)
{
	int ret;

	kscePowerSetVeneziaClockFrequency(166);
	ret = bootVeneziaPhase2();
	if (ret < 0) {
		SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 118, "_sceVeneziaStartEntryPhase2",
					  "Warning: _sceVeneziaBootPhase2 failed (0x%08x)\n", ret);
		ret = 0;
	} else {
		ScePervasiveForDriver_725B4F59(veneziaClockSelector(41));
		ret = initCodecEnginePerformanceMonitor();
		if (ret >= 0)
			signalVeneziaRpcSemas();
	}
	return ret;
}

int _sceVeneziaProcessCreateEntryInternal2(SceUID pid, SceUInt32 type)
{
	VeneziaProcessStorage *storage = NULL;
	int ret;

	ret = ksceKernelGetProcessLocalStorageAddrForPid(pid, g_veneziaState.processStorageKey,
						      (void **)&storage, 1);
	if (ret < 0)
		return ret;
#if FW_DEBUG
	SceDebugForDriver_35A35322(storage != NULL,
			 SCE_DBGINFO_FUNC(191, "_sceVeneziaProcessCreateEntry"),
			 __builtin_return_address(0));
#endif
	memset(storage, 0, sizeof(*storage));
	ret = sceKernelInitializeFastMutex(&storage->suspendMutex, "SceVeneziaSuspend",
					  0x8000, NULL);
	if (ret < 0)
		return ret;
	storage->suspendLock = 0;
	storage->suspendLockCount = 0;
	storage->clockLockCount = 0;
	storage->suspendInProgress = 0;
	return ret;
}

int _sceVeneziaProcessExitEntry(SceUID pid, SceUInt32 type)
{
	VeneziaProcessStorage *storage = NULL;
	int ret, err;

	ret = ksceKernelGetProcessLocalStorageAddrForPid(pid, g_veneziaState.processStorageKey,
						      (void **)&storage, 0);
	if (ret >= 0)
		ret = 0;
#if FW_DEBUG
	SceDebugForDriver_35A35322(storage != NULL,
			 SCE_DBGINFO_FUNC(225, "_sceVeneziaProcessExitEntry"),
			 __builtin_return_address(0));
	SceDebugForDriver_35A35322(storage->suspendLockCount == 0,
			 SCE_DBGINFO_FUNC(226, "_sceVeneziaProcessExitEntry"),
			 __builtin_return_address(0));
	SceDebugForDriver_35A35322(storage->clockLockCount == 0,
			 SCE_DBGINFO_FUNC(227, "_sceVeneziaProcessExitEntry"),
			 __builtin_return_address(0));
#endif
	if (storage->suspendInProgress != 0) {
		err = sceKernelUnlockFastMutex(&storage->suspendMutex);
		if (err < 0)
			ret = err;
	}
	err = sceKernelFinalizeFastMutex(&storage->suspendMutex);
	if (err < 0)
		ret = err;
	return ret;
}

int SceCodecEngineWrapperForDriver_CB8703D8(void)
{
	VeneziaProcessStorage *storage;
	int ret, intr;

	storage = ksceKernelGetProcessLocalStorageAddr(g_veneziaState.processStorageKey);
#if FW_DEBUG
	SceDebugForDriver_35A35322(storage != NULL, SCE_DBGINFO_FUNC(269, "_lockProcessSuspendCore"),
			 __builtin_return_address(0));
#endif
	ret = lockFastMutex(&storage->suspendMutex);
	if (ret >= 0) {
		intr = SceCpuForDriver_D32ACE9E((SceKernelSpinlock *)&storage->suspendLock);
		storage->suspendLockCount++;
		sceKernelSpinlockLowUnlockCpuResumeIntr((SceKernelSpinlock *)&storage->suspendLock, intr);
		sceKernelUnlockFastMutex(&storage->suspendMutex);
	}
	return ret;
}

int _lockProcessSuspendCore(void)
{
	VeneziaProcessStorage *storage;
	int ret, intr;

	storage = ksceKernelGetProcessLocalStorageAddr(g_veneziaState.processStorageKey);
#if FW_DEBUG
	SceDebugForDriver_35A35322(storage != NULL, SCE_DBGINFO_FUNC(269, "_lockProcessSuspendCore"),
			 __builtin_return_address(0));
#endif
	ret = lockFastMutex(&storage->suspendMutex);
	if (ret >= 0) {
		intr = SceCpuForDriver_D32ACE9E((SceKernelSpinlock *)&storage->suspendLock);
		storage->suspendLockCount++;
		sceKernelSpinlockLowUnlockCpuResumeIntr((SceKernelSpinlock *)&storage->suspendLock, intr);
		sceKernelUnlockFastMutex(&storage->suspendMutex);
	}
	return ret;
}

int _unlockProcessSuspendCore(void)
{
	VeneziaProcessStorage *storage;
	int intr;

	storage = ksceKernelGetProcessLocalStorageAddr(g_veneziaState.processStorageKey);
#if FW_DEBUG
	SceDebugForDriver_35A35322(storage != NULL, SCE_DBGINFO_FUNC(318, "_unlockProcessSuspendCore"),
			 __builtin_return_address(0));
#endif
	intr = SceCpuForDriver_D32ACE9E((SceKernelSpinlock *)&storage->suspendLock);
	storage->suspendLockCount--;
	sceKernelSpinlockLowUnlockCpuResumeIntr((SceKernelSpinlock *)&storage->suspendLock, intr);
	return 0;
}

int _sceVeneziaStartProcessSuspend(SceUID pid)
{
	VeneziaProcessStorage *storage = NULL;
	int ret;

	ret = ksceKernelGetProcessLocalStorageAddrForPid(pid, g_veneziaState.processStorageKey,
						      (void **)&storage, 0);
	if (ret < 0)
		return ret;
#if FW_DEBUG
	SceDebugForDriver_35A35322(storage != NULL,
			 SCE_DBGINFO_FUNC(345, "_sceVeneziaStartProcessSuspend"),
			 __builtin_return_address(0));
#endif
	storage->suspendPollCount = 0;
	ret = lockFastMutex(&storage->suspendMutex);
	if (ret >= 0)
		storage->suspendInProgress = 1;
	return ret;
}

int _sceVeneziaProcessSuspendPhase0(SceUID pid)
{
	VeneziaProcessStorage *storage = NULL;
	int ret, intr;

	ret = ksceKernelGetProcessLocalStorageAddrForPid(pid, g_veneziaState.processStorageKey,
						      (void **)&storage, 0);
	if (ret < 0)
		return ret;
#if FW_DEBUG
	SceDebugForDriver_35A35322(storage != NULL,
			 SCE_DBGINFO_FUNC(374, "_sceVeneziaProcessSuspendPhase0"),
			 __builtin_return_address(0));
#endif
	intr = SceCpuForDriver_D32ACE9E(&g_veneziaState.clockLock);
	g_veneziaState.clockLockCount -= storage->clockLockCount;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&g_veneziaState.clockLock, intr);
	return veneziaRpcClockDown();
}

int sceCodecEngineDsp0IsProcessSuspended(SceUID pid)
{
	VeneziaProcessStorage *storage = NULL;
	unsigned int count;
	int ret, intr;

	ret = ksceKernelGetProcessLocalStorageAddrForPid(pid, g_veneziaState.processStorageKey,
						      (void **)&storage, 0);
	if (ret < 0)
		return 0;
#if FW_DEBUG
	SceDebugForDriver_35A35322(storage != NULL,
			 SCE_DBGINFO_FUNC(412, "sceCodecEngineDsp0IsProcessSuspended"),
			 __builtin_return_address(0));
#endif
	intr = SceCpuForDriver_D32ACE9E((SceKernelSpinlock *)&storage->suspendLock);
	count = storage->suspendLockCount;
	sceKernelSpinlockLowUnlockCpuResumeIntr((SceKernelSpinlock *)&storage->suspendLock, intr);
	if (count != 0)
		storage->suspendPollCount++;
	return count == 0;
}

int _sceVeneziaProcessResume(SceUID pid)
{
	VeneziaProcessStorage *storage = NULL;
	int ret, intr;

	ret = ksceKernelGetProcessLocalStorageAddrForPid(pid, g_veneziaState.processStorageKey,
						      (void **)&storage, 0);
	if (ret < 0)
		return ret;
#if FW_DEBUG
	SceDebugForDriver_35A35322(storage != NULL,
			 SCE_DBGINFO_FUNC(444, "_sceVeneziaProcessResume"),
			 __builtin_return_address(0));
#endif
	intr = SceCpuForDriver_D32ACE9E(&g_veneziaState.clockLock);
	g_veneziaState.clockLockCount += storage->clockLockCount;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&g_veneziaState.clockLock, intr);
	ret = sceKernelUnlockFastMutex(&storage->suspendMutex);
	if (ret >= 0)
		storage->suspendInProgress = 0;
	return ret;
}

int _sceVeneziaStartSystemSuspend(void)
{
	VeneziaProcessStorage *storage = NULL;
	int ret;

	ret = ksceKernelGetProcessLocalStorageAddrForPid(0, g_veneziaState.processStorageKey,
						      (void **)&storage, 0);
	if (ret < 0)
		goto out;
#if FW_DEBUG
	SceDebugForDriver_35A35322(storage != NULL,
			 SCE_DBGINFO_FUNC(482, "_sceVeneziaStartSystemSuspend"),
			 __builtin_return_address(0));
#endif
	storage->suspendPollCount = 0;
	ret = lockFastMutex(&storage->suspendMutex);
	if (ret < 0)
		goto out;
	storage->suspendInProgress = 1;
	startPerformanceMonitorSuspend();
out:
	return ret;
}

int _sceVeneziaIsSystemSuspended(void)
{
	VeneziaProcessStorage *storage = NULL;
	unsigned int count;
	int ret, intr, suspended;

	ret = ksceKernelGetProcessLocalStorageAddrForPid(0, g_veneziaState.processStorageKey,
						      (void **)&storage, 0);
	if (ret < 0)
		return 0;
#if FW_DEBUG
	SceDebugForDriver_35A35322(storage != NULL,
			 SCE_DBGINFO_FUNC(519, "_sceVeneziaIsSystemSuspended"),
			 __builtin_return_address(0));
#endif
	intr = SceCpuForDriver_D32ACE9E((SceKernelSpinlock *)&storage->suspendLock);
	count = storage->suspendLockCount;
	sceKernelSpinlockLowUnlockCpuResumeIntr((SceKernelSpinlock *)&storage->suspendLock, intr);
	suspended = isPerformanceMonitorSuspended();
	if (count != 0)
		storage->suspendPollCount++;
	return suspended != 0 && count == 0;
}

/* name unknown: system event phase 1. */
int suspendSystem(void)
{
	ScePervasiveForDriver_725B4F59(veneziaClockSelector(166));
	return veneziaSuspend();
}

int _sceVeneziaSystemResumePhase0(void)
{
	VeneziaProcessStorage *storage = NULL;
	int ret;

	ret = ksceKernelGetProcessLocalStorageAddrForPid(0, g_veneziaState.processStorageKey,
						      (void **)&storage, 0);
	if (ret < 0)
		goto out;
#if FW_DEBUG
	SceDebugForDriver_35A35322(storage != NULL,
			 SCE_DBGINFO_FUNC(576, "_sceVeneziaSystemResumePhase0"),
			 __builtin_return_address(0));
#endif
	ScePervasiveForDriver_725B4F59(veneziaClockSelector(kscePowerGetVeneziaClockFrequency()));
	ret = veneziaResume();
	if (ret < 0)
		goto out;
	ret = sceKernelUnlockFastMutex(&storage->suspendMutex);
	if (ret < 0)
		goto out;
	storage->suspendInProgress = 0;
	resumePerformanceMonitor();
out:
	return ret;
}

/* name unknown: system event phase 1 after resume. */
int resumeSystem(void)
{
	return SceCodecEngineWrapperForDriver_0D3BB164(509);
}

void _sceVeneziaPrintBlockingCalls(SceUID pid)
{
	VeneziaProcessStorage *storage = NULL;
	unsigned int count;
	int ret;

	ret = ksceKernelGetProcessLocalStorageAddrForPid(pid, g_veneziaState.processStorageKey,
						      (void **)&storage, 0);
	if (ret < 0)
		return;
#if FW_DEBUG
	SceDebugForDriver_35A35322(storage != NULL,
			 SCE_DBGINFO_FUNC(664, "_sceVeneziaPrintBlockingCalls"),
			 __builtin_return_address(0));
#endif
	count = storage->suspendPollCount;
	if (count != 0 && count % 200 == 0)
		printVeneziaRpcBlockingCalls(pid);
}

/* name unknown: outstanding background clock requests. */
unsigned int getVeneziaClockLockCount(void)
{
	return g_veneziaState.clockLockCount;
}

int _sceVeneziaRunInBackground(void)
{
	VeneziaProcessStorage *storage;
	unsigned int count;
	int intr;

	storage = ksceKernelGetProcessLocalStorageAddr(g_veneziaState.processStorageKey);
#if FW_DEBUG
	SceDebugForDriver_35A35322(storage != NULL,
			 SCE_DBGINFO_FUNC(687, "_sceVeneziaRunInBackground"),
			 __builtin_return_address(0));
#endif
	intr = SceCpuForDriver_D32ACE9E(&g_veneziaState.clockLock);
	count = g_veneziaState.clockLockCount + 1;
	storage->clockLockCount++;
	g_veneziaState.clockLockCount = count;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&g_veneziaState.clockLock, intr);
	return 0;
}

int _sceVeneziaStopInBackground(void)
{
	VeneziaProcessStorage *storage;
	unsigned int count;
	int intr;

	storage = ksceKernelGetProcessLocalStorageAddr(g_veneziaState.processStorageKey);
#if FW_DEBUG
	SceDebugForDriver_35A35322(storage != NULL,
			 SCE_DBGINFO_FUNC(712, "_sceVeneziaStopInBackground"),
			 __builtin_return_address(0));
#endif
	if (storage->clockLockCount != 0) {
		intr = SceCpuForDriver_D32ACE9E(&g_veneziaState.clockLock);
		count = g_veneziaState.clockLockCount - 1;
		storage->clockLockCount--;
		g_veneziaState.clockLockCount = count;
		sceKernelSpinlockLowUnlockCpuResumeIntr(&g_veneziaState.clockLock, intr);
	}
	return 0;
}
