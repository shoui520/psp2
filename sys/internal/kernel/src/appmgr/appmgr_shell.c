/* Shell power-save settings and watchdog. */

#include <stddef.h>
#include <sysmem/debug.h>
#include <threadmgr/threadmgr.h>
#include <appmgr/appmgr.h>
#include <acmgr/acmgr.h>
#include <regmgr/regmgr.h>
#include <intrmgr/syscall.h>
#include <sysmem/sysclib.h>
#include "app_state_internal.h"
#include "appmgr_device.h"

extern SceUID g_appmgrMainMutex;
extern SceUID g_appmgrMainMutexOwner;
extern int g_appmgrPowerSaveMode;
extern int g_appmgrPowerSaveSuspendBackgroundApps;
extern SceUID g_appmgrCore2MutexOwner;
extern int _updatePowerTickStateForNonSuspendableApp(void);
extern int _sceAppMgrIsExistAbortedAppOrDebugSuspendedApp(void);
extern SceUID appmgrGetAbortThreadId(void);
extern int appmgrAbortProcess(SceUID processId);
extern void _destroyAllAppForAbortHandler(void);

extern SceUID g_appmgrShellPid;

SceInt64 g_appmgrShellWatchDogLastActivity;
SceUID g_appmgrShellWatchDogThreadId;
int g_appmgrShellWatchDogStopRequested;

#if !FW_DEBUG
static const SceKernelDebugInfo shellMutexPanicInfo = {
	.fileHash = 0xc9656e8e,
	.lineHash = 0x79d3a1f0,
	.funcHash = 0x9d536bce,
};

static const SceKernelDebugInfo watchDogExitPanicInfo = {
	.fileHash = 0xaa1163e9,
	.lineHash = 0xcdba1b9e,
	.funcHash = 0x3e3f1f42,
};

static const SceKernelDebugInfo watchDogStartPanicInfo = {
	.fileHash = 0xaa1163e9,
	.lineHash = 0x7ad863b6,
	.funcHash = 0xcde4b170,
};

static const SceKernelDebugInfo shellPanicInfo = {
	.fileHash = 0xc9656e8e,
	.lineHash = 0xc879dadc,
	.funcHash = 0x11a05ee1,
};

void appmgrShellMutexPanic(void)
{
	SceDebugForDriver_391B5B74(&shellMutexPanicInfo, __builtin_return_address(0));
}

__attribute__((noinline)) _Noreturn void appmgrShellPanic(void)
{
	SceDebugForDriver_391B5B74(&shellPanicInfo, __builtin_return_address(0));
}
#endif

#if FW_DEBUG
#define SHELL_FAILURE(_value, _line, _func) do { \
	sceAppMgrDump(); \
	dumpBgmState(); \
	SCE_DBG_ASSERT_PRINTF_FUNC(0, _line, _func, \
		"[SceAppMgr] The actual value is %d(0x%08x), but expected value is %d(0x%08x)\n", \
		_value, _value, 0, 0); \
} while (0)
/* the caller holds neither the main lock nor another application's lock */
#define SHELL_ASSERT_MAIN_UNOWNED(_line, _func) do { \
	if (SceDebugForDriver_374B7868() > 0) { \
		SceUID owner = g_appmgrMainMutexOwner; \
		SceUID tid = sceKernelGetThreadId(); \
		SCE_DBG_ASSERT_LEVEL_FUNC(1, owner != tid, _line, _func); \
	} \
} while (0)
#define SHELL_APP_LOCK_CHECK(_line, _func) do { \
	SceUID owner; \
	SHELL_ASSERT_MAIN_UNOWNED(_line, _func); \
	owner = g_appmgrCore2MutexOwner; \
	if (owner != sceKernelGetThreadId()) { \
		int slot; \
		for (slot = 0; slot != 20; ++slot) { \
			int level = SceDebugForDriver_374B7868(); \
			__builtin_prefetch(&g_appmgrAppStates[slot + 5].mutexOwner); \
			if (level > 0) { \
				SceUID slotOwner = g_appmgrAppStates[slot].mutexOwner; \
				SceUID tid = sceKernelGetThreadId(); \
				SCE_DBG_ASSERT_LEVEL_FUNC(1, slotOwner != tid, _line, _func); \
			} \
		} \
	} \
} while (0)
#else
#define SHELL_FAILURE(_value, _line, _func) appmgrShellMutexPanic()
#define SHELL_ASSERT_MAIN_UNOWNED(_line, _func) do { } while (0)
#define SHELL_APP_LOCK_CHECK(_line, _func) ((void)sceKernelGetThreadId())
#endif

/* kills a shell that has not reported activity for 30 seconds, unless an application is aborted or suspended */
int _shellWatchDogThreadEntry(SceSize args, void *argp)
{
	struct AppState *shell;
	int enabled, result;

	for (;;) {
		sceKernelDelayThread(15000000);
		if (g_appmgrShellWatchDogStopRequested)
			break;
		result = sceKernelLockMutex(g_appmgrMainMutex, 1, NULL);
		if (result != 0) {
#if FW_DEBUG
			SCE_DBG_PRINTF_FUNC(0, 373, "_shellWatchDogThreadEntry",
				"Failed to lock ret=0x%08x\n", result);
			sceAppMgrDump();
			dumpBgmState();
			SCE_DBG_ASSERT_CHECK_FUNC(0, 373, "_shellWatchDogThreadEntry");
#else
			appmgrShellMutexPanic();
#endif
		} else {
			g_appmgrMainMutexOwner = sceKernelGetThreadId();
		}
		if ((SceUInt64)(sceKernelGetSystemTimeWide() - g_appmgrShellWatchDogLastActivity) >
		    30000000) {
			g_appmgrMainMutexOwner = -1;
			result = sceKernelUnlockMutex(g_appmgrMainMutex, 1);
			if (result != 0)
				SHELL_FAILURE(result, 380, "_shellWatchDogThreadEntry");
			if (_sceAppMgrIsExistAbortedAppOrDebugSuspendedApp() != 0) {
				appmgrUpdateShellWatchDogActivity();
				continue;
			}
			SCE_DBG_PRINTF_FUNC(0, 388, "_shellWatchDogThreadEntry",
				"[SceAppMgr] Shell watch dog timeout\n");
			if (!SceSblACMgrForDriver_E87D1777() ||
			    (sceRegMgrGetKeyInt("/DEVENV/TOOL", "enable_shell_watch_dog", &enabled) == 0 &&
			     enabled != 0)) {
				shell = appmgrGetShellApp();
				SCE_DBG_PRINTF_FUNC(0, 394, "_shellWatchDogThreadEntry", "[SceAppMgr] Shell hang up\n");
				if (shell != NULL) {
					sceKernelChangeThreadPriorityAndCpuAffinityMask(appmgrGetAbortThreadId(), 17,
						0x80000, NULL, NULL);
					if (appmgrAbortProcess(shell->process.processId)) {
						_destroyAllAppForAbortHandler();
						SCE_DBG_PRINTF_FUNC(0, 409, "_shellWatchDogThreadEntry", "Kill Shell!.\n");
						SHELL_APP_LOCK_CHECK(411, "_shellWatchDogThreadEntry");
						result = sceKernelLockMutex(shell->mutex, 1, NULL);
						if (result != 0) {
							if ((unsigned int)(result + 0x7ffd7fd4) > 1)
								SHELL_FAILURE(result, 411, "_shellWatchDogThreadEntry");
						} else {
							shell->mutexOwner = sceKernelGetThreadId();
							_appmgrProcessQuitProcess(shell, 1, 0, 0);
							SHELL_ASSERT_MAIN_UNOWNED(414, "_shellWatchDogThreadEntry");
							shell->mutexOwner = -1;
							result = sceKernelUnlockMutex(shell->mutex, 1);
							if (result != 0)
								SHELL_FAILURE(result, 414, "_shellWatchDogThreadEntry");
						}
					}
				}
			}
			break;
		}
		g_appmgrMainMutexOwner = -1;
		result = sceKernelUnlockMutex(g_appmgrMainMutex, 1);
		if (result != 0)
			SHELL_FAILURE(result, 424, "_shellWatchDogThreadEntry");
	}
	result = sceKernelExitDeleteThread(0);
	if (result != 0) {
#if FW_DEBUG
		SHELL_FAILURE(result, 428, "_shellWatchDogThreadEntry");
#else
		SceDebugForDriver_00CCE39C(&watchDogExitPanicInfo, __builtin_return_address(0),
			"0x%08x,0x%08x\n", result, 0);
#endif
	}
	return 0;
}

/* returns the thread's creation error, or 0 once it runs */
static inline __attribute__((always_inline)) int _shellWatchDogInitialize(void)
{
	SceUID thread;

	g_appmgrShellWatchDogStopRequested = 0;
	g_appmgrShellWatchDogLastActivity = sceKernelGetSystemTimeWide();
	__sync_synchronize();
	thread = sceKernelCreateThread("SceAppMgrShellWatchDog", _shellWatchDogThreadEntry, 17, 4096,
		0, 0x80000, NULL);
	if (thread < 0)
		return thread;
	g_appmgrShellWatchDogThreadId = thread;
	if (sceKernelStartThread(thread, 0, NULL) != 0) {
#if FW_DEBUG
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_CHECK_FUNC(0, 461, "_shellWatchDogInitialize");
#else
		SceDebugForDriver_391B5B74(&watchDogStartPanicInfo, __builtin_return_address(0));
#endif
	}
	return 0;
}

int sceAppMgrDeclareShellProcess2(const char *appName)
{
	SceUInt32 syscallState = sceSyscallEnter();
	SceUID processId;
	int appId, result;

	if (!SceSblACMgrForDriver_991FDC15(0)) {
		sceSyscallLeave(syscallState);
		return (int)0x8080201f;
	}
	result = sceKernelLockMutex(g_appmgrMainMutex, 1, NULL);
	if (result != 0) {
#if FW_DEBUG
		SCE_DBG_PRINTF_FUNC(0, 44, "sceAppMgrDeclareShellProcess2",
			"Failed to lock ret=0x%08x\n", result);
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_CHECK_FUNC(0, 44, "sceAppMgrDeclareShellProcess2");
#else
		appmgrShellMutexPanic();
#endif
	} else {
		g_appmgrMainMutexOwner = sceKernelGetThreadId();
	}
	if (g_appmgrShellPid != -1) {
		g_appmgrMainMutexOwner = -1;
		result = sceKernelUnlockMutex(g_appmgrMainMutex, 1);
		if (result != 0)
			SHELL_FAILURE(result, 48, "sceAppMgrDeclareShellProcess2");
		sceSyscallLeave(syscallState);
		return (int)0x8080201f;
	}
	_appListDeclareShellProcess(g_appmgrAppStates);
	if (appName != NULL)
		strncpy(g_appmgrAppStates[0].process.appName, appName, 32);
	g_appmgrMainMutexOwner = -1;
	appId = g_appmgrAppStates[0].appId;
	processId = g_appmgrAppStates[0].process.processId;
	result = sceKernelUnlockMutex(g_appmgrMainMutex, 1);
	if (result != 0)
		SHELL_FAILURE(result, 59, "sceAppMgrDeclareShellProcess2");
	_sceAppMgrDeviceDeclareShell(appName, appId, processId);
	result = _shellWatchDogInitialize();
	if (result != 0) {
#if FW_DEBUG
		SHELL_FAILURE(result, 64, "sceAppMgrDeclareShellProcess2");
#else
		appmgrShellPanic();
#endif
	}
	sceSyscallLeave(syscallState);
	return 0;
}

int _sceAppMgrSetPowerSaveModeInternal(int mode, int suspendBackgroundApps)
{
	int result;

	result = sceKernelLockMutex(g_appmgrMainMutex, 1, NULL);
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
		sceKernelGetThreadId();
	}
	g_appmgrPowerSaveMode = mode;
	g_appmgrPowerSaveSuspendBackgroundApps = suspendBackgroundApps;
	g_appmgrMainMutexOwner = -1;
	result = sceKernelUnlockMutex(g_appmgrMainMutex, 1);
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
