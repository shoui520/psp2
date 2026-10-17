/* Shell power-save settings and watchdog. */

#include <sysmem/debug.h>
#include <threadmgr/threadmgr.h>
#include <psp2kern/kernel/threadmgr.h>

extern SceUID g_appmgrMainMutex;
extern SceUID g_appmgrMainMutexOwner;
extern int g_appmgrPowerSaveMode;
extern int g_appmgrPowerSaveSuspendBackgroundApps;
extern int sceAppMgrDump(void);
extern int dumpBgmState(void);
extern int _updatePowerTickStateForNonSuspendableApp(void);
extern void _deviceRefreshAudioPort(SceUID pid);
extern int _updateDeviceArbitration2(void);

SceInt64 g_appmgrShellWatchDogLastActivity;
int g_appmgrShellWatchDogStopRequested;

#if !FW_DEBUG
static const SceKernelDebugInfo shellMutexPanicInfo = {
	.fileHash = 0xc9656e8e,
	.lineHash = 0x79d3a1f0,
	.funcHash = 0x9d536bce,
};

void appmgrShellMutexPanic(void)
{
	SceDebugForDriver_391B5B74(&shellMutexPanicInfo, __builtin_return_address(0));
}
#endif

int _sceAppMgrSetPowerSaveModeInternal(int mode, int suspendBackgroundApps)
{
	int result;

	result = ksceKernelLockMutex(g_appmgrMainMutex, 1, NULL);
	if (result) {
#if FW_DEBUG
		SCE_DBG_PRINTF_FUNC(0, 296, "_sceAppMgrSetPowerSaveModeInternal",
			"Failed to lock ret=0x%08x\n", result);
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_CHECK_FUNC(0, 296, "_sceAppMgrSetPowerSaveModeInternal");
#else
		appmgrShellMutexPanic();
#endif
	} else {
		ksceKernelGetThreadId();
	}
	g_appmgrPowerSaveMode = mode;
	g_appmgrPowerSaveSuspendBackgroundApps = suspendBackgroundApps;
	g_appmgrMainMutexOwner = -1;
	result = ksceKernelUnlockMutex(g_appmgrMainMutex, 1);
	if (result) {
#if FW_DEBUG
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_PRINTF_FUNC(0, 301, "_sceAppMgrSetPowerSaveModeInternal",
			"[SceAppMgr] The actual value is %d(0x%08x), but expected value is %d(0x%08x)\n",
			result, result, 0, 0);
#else
		appmgrShellMutexPanic();
#endif
	}
	_updatePowerTickStateForNonSuspendableApp();
	_deviceRefreshAudioPort(-1);
	_updateDeviceArbitration2();
	return 0;
}

int appmgrStopShellWatchDog(void)
{
	g_appmgrShellWatchDogStopRequested = 1;
	return 0;
}

SceInt64 appmgrUpdateShellWatchDogActivity(void)
{
	SceInt64 activity = sceKernelGetSystemTimeWide();

	g_appmgrShellWatchDogLastActivity = activity;
	return activity;
}
