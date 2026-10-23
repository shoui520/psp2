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
#include "appmgr_shell_event.h"

extern SceUID g_appmgrMainMutex;
extern SceUID g_appmgrMainMutexOwner;
extern int g_appmgrPowerSaveMode;
extern int g_appmgrPowerSaveSuspendBackgroundApps;
extern SceUID g_appmgrCore2MutexOwner;
extern int _updatePowerTickStateForNonSuspendableApp(void);
extern int _sceAppMgrIsExistAbortedAppOrDebugSuspendedApp(void);
extern SceUID appmgrGetAbortThreadId(void);
extern int appmgrAbortProcess(SceUID processId);

extern SceUID g_appmgrShellPid;
extern SceUID g_appmgrLiveAreaOperationMutex;
extern SceUID g_appmgrLiveAreaResultMutex;
extern SceUID g_appmgrLiveAreaResultCondition;

extern SceUID g_appmgrUpdateNotifySemaphore;

/* what the shell answers to the last LiveArea operation */
int g_appmgrLiveAreaOperationResult;
/* set by _sceAppMgrSystemSuspend, cleared by _sceAppMgrSystemResume */
int g_appmgrSystemSuspended;

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

static const SceKernelDebugInfo suspendBgAppPanicInfo = {
	.fileHash = 0xaa1163e9,
	.lineHash = 0x0fe71d6d,
	.funcHash = 0xe581fdf4,
};

static const SceKernelDebugInfo resumeBgAppPanicInfo = {
	.fileHash = 0xaa1163e9,
	.lineHash = 0x550a5499,
	.funcHash = 0xe581fdf4,
};

static const SceKernelDebugInfo shellPanicInfo = {
	.fileHash = 0xc9656e8e,
	.lineHash = 0xc879dadc,
	.funcHash = 0x11a05ee1,
};

__attribute__((noinline)) void appmgrShellMutexPanic(void)
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

/* a failure the release build stops on with appmgrShellPanic */
#if FW_DEBUG
#define SHELL_PANIC_FAILURE(_value, _line, _func) SHELL_FAILURE(_value, _line, _func)
#else
#define SHELL_PANIC_FAILURE(_value, _line, _func) appmgrShellPanic()
#endif

/* main lock: the owner is recorded only when the lock is taken */
#if FW_DEBUG
#define SHELL_MAIN_LOCK(_line, _func) do { \
	int lockResult = sceKernelLockMutex(g_appmgrMainMutex, 1, NULL); \
	if (lockResult != 0) { \
		SCE_DBG_PRINTF_FUNC(0, _line, _func, "Failed to lock ret=0x%08x\n", lockResult); \
		sceAppMgrDump(); \
		dumpBgmState(); \
		SCE_DBG_ASSERT_CHECK_FUNC(0, _line, _func); \
	} else { \
		g_appmgrMainMutexOwner = sceKernelGetThreadId(); \
	} \
} while (0)
#else
#define SHELL_MAIN_LOCK(_line, _func) do { \
	if (sceKernelLockMutex(g_appmgrMainMutex, 1, NULL) != 0) \
		appmgrShellMutexPanic(); \
	else \
		g_appmgrMainMutexOwner = sceKernelGetThreadId(); \
} while (0)
#endif
#define SHELL_MAIN_UNLOCK(_line, _func) do { \
	int unlockResult; \
	g_appmgrMainMutexOwner = -1; \
	unlockResult = sceKernelUnlockMutex(g_appmgrMainMutex, 1); \
	if (unlockResult != 0) \
		SHELL_FAILURE(unlockResult, _line, _func); \
} while (0)

/* a lock the caller may already hold (0x8002802c, 0x8002802d) is not a failure */
#define SHELL_LOCK_FAILED(_result) ((unsigned int)((_result) + 0x7ffd7fd4) > 1)

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

int appmgrRestoreDisplaySettingForShell(void)
{
	SceUInt32 syscallState = sceSyscallEnter();

	if (!isShellProcess()) {
		sceSyscallLeave(syscallState);
		return (int)0x8080201c;
	}
	_deviceRefreshDisplay();
	SHELL_MAIN_LOCK(85, "sceAppMgrRestoreDisplaySettingForShell");
	_deviceSetAudioPortsDisabled(0);
	SHELL_MAIN_UNLOCK(87, "sceAppMgrRestoreDisplaySettingForShell");
	_updateDeviceArbitration2();
	sceSyscallLeave(syscallState);
	return 0;
}

int appmgrRestoreBgmSettingForShell(void)
{
	SceUInt32 syscallState = sceSyscallEnter();

	if (!isShellProcess()) {
		sceSyscallLeave(syscallState);
		return (int)0x8080201c;
	}
	_deviceSetBgmEnabled(1);
	sceSyscallLeave(syscallState);
	return 0;
}

/* asks the shell to perform a LiveArea operation and waits for its answer */
inline __attribute__((always_inline))
int sceAppMgrLiveAreaOperationByNameForLoadp(int operation, const char *appName)
{
	int result, waitResult;

	result = sceKernelLockMutex(g_appmgrLiveAreaOperationMutex, 1, NULL);
	if (result != 0) {
		if (SHELL_LOCK_FAILED(result))
			SHELL_PANIC_FAILURE(result, 119, "sceAppMgrLiveAreaOperationByNameForLoadp");
		return result;
	}
	result = sceKernelLockMutex(g_appmgrLiveAreaResultMutex, 1, NULL);
	if (result != 0) {
		if (SHELL_LOCK_FAILED(result))
			SHELL_PANIC_FAILURE(result, 129, "sceAppMgrLiveAreaOperationByNameForLoadp");
		return result;
	}
	_shellEventLiveAreaOperationQueuePush(operation, appName);
	waitResult = sceKernelWaitCond(g_appmgrLiveAreaResultCondition, NULL);
	result = sceKernelUnlockMutex(g_appmgrLiveAreaResultMutex, 1);
	if (result != 0)
		SHELL_PANIC_FAILURE(result, 139, "sceAppMgrLiveAreaOperationByNameForLoadp");
	if (waitResult < 0) {
		if (SHELL_LOCK_FAILED(waitResult))
			SHELL_PANIC_FAILURE(waitResult, 143, "sceAppMgrLiveAreaOperationByNameForLoadp");
		result = sceKernelUnlockMutex(g_appmgrLiveAreaOperationMutex, 1);
		if (result != 0)
			SHELL_PANIC_FAILURE(result, 146, "sceAppMgrLiveAreaOperationByNameForLoadp");
		return waitResult;
	}
	waitResult = g_appmgrLiveAreaOperationResult;
	result = sceKernelUnlockMutex(g_appmgrLiveAreaOperationMutex, 1);
	if (result != 0)
		SHELL_PANIC_FAILURE(result, 155, "sceAppMgrLiveAreaOperationByNameForLoadp");
	return waitResult;
}

int sceAppMgrLiveAreaOperationForLoadp(int operation)
{
	return sceAppMgrLiveAreaOperationByNameForLoadp(operation, "NPXS10998");
}

int appmgrReturnLiveAreaOperationResultForShell(int operationResult)
{
	SceUInt32 syscallState = sceSyscallEnter();
	int result;

	if (!isShellProcess()) {
		sceSyscallLeave(syscallState);
		return (int)0x8080201c;
	}
	g_appmgrLiveAreaOperationResult = operationResult;
	result = sceKernelLockMutex(g_appmgrLiveAreaResultMutex, 1, NULL);
	if (result != 0) {
		if (SHELL_LOCK_FAILED(result))
			SHELL_PANIC_FAILURE(result, 185, "sceAppMgrReturnLiveAreaOperationResultForShell");
		sceSyscallLeave(syscallState);
		return result;
	}
	result = sceKernelSignalCond(g_appmgrLiveAreaResultCondition);
	if (result != 0)
		SHELL_PANIC_FAILURE(result, 191, "sceAppMgrReturnLiveAreaOperationResultForShell");
	result = sceKernelUnlockMutex(g_appmgrLiveAreaResultMutex, 1);
	if (result != 0)
		SHELL_PANIC_FAILURE(result, 193, "sceAppMgrReturnLiveAreaOperationResultForShell");
	sceSyscallLeave(syscallState);
	return 0;
}

/* deactivates the active application (unless it is the shell) and activates appId */
int appmgrActivateApp(int appId)
{
	SceUInt32 syscallState = sceSyscallEnter();
	struct AppState *app, *active;
	int result;

	if (!isShellProcess()) {
		sceSyscallLeave(syscallState);
		return (int)0x8080201f;
	}
	for (;;) {
		if (_appListFindAndLockActiveApp(&active) == 0) {
			if (sceKernelGetProcessId() == active->process.processId) {
				result = _appListUnlockApp(active);
				if (result != 0)
					SHELL_PANIC_FAILURE(result, 222, "sceAppMgrActivateApp");
				sceSyscallLeave(syscallState);
				return (int)0x80802013;
			}
			result = _sceAppMgrDeactivateAppInternal(active, 1, 0, 0);
			if (result != 0)
				SHELL_PANIC_FAILURE(result, 229, "sceAppMgrActivateApp");
			result = _appListUnlockApp(active);
			if (result != 0)
				SHELL_PANIC_FAILURE(result, 232, "sceAppMgrActivateApp");
		}
		result = _appListFindAndLockAppByAppId(appId, &app);
		if (result != 0) {
			sceSyscallLeave(syscallState);
			return result;
		}
		if (g_appmgrDebugNotifier != NULL)
			g_appmgrDebugNotifier(2, app->appId, app->process.processId, 0);
		result = _sceAppMgrActivateAppInternal(app);
		{
			int unlockResult = _appListUnlockApp(app);

			if (unlockResult != 0)
				SHELL_PANIC_FAILURE(unlockResult, 246, "sceAppMgrActivateApp");
		}
		if (result != (int)0x80803001) {
			sceSyscallLeave(syscallState);
			return result;
		}
	}
}

int appmgrDeactivateApp(int appId)
{
	SceUInt32 syscallState = sceSyscallEnter();
	struct AppState *app;
	int result, unlockResult;

	if (!isShellProcess()) {
		sceSyscallLeave(syscallState);
		return (int)0x8080201f;
	}
	result = _appListFindAndLockAppByAppId(appId, &app);
	if (result != 0) {
		sceSyscallLeave(syscallState);
		return result;
	}
	if (g_appmgrDebugNotifier != NULL)
		g_appmgrDebugNotifier(3, app->appId, app->process.processId, 0);
	result = _sceAppMgrDeactivateAppInternal(app, 1, 0, 0);
	unlockResult = _appListUnlockApp(app);
	if (unlockResult != 0)
		SHELL_PANIC_FAILURE(unlockResult, 287, "sceAppMgrDeactivateApp");
	sceSyscallLeave(syscallState);
	return result;
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

int appmgrSetPowerSaveMode(int mode, int suspendBackgroundApps)
{
	SceUInt32 syscallState = sceSyscallEnter();
	int result;

	if (!isShellProcess()) {
		sceSyscallLeave(syscallState);
		return (int)0x8080201c;
	}
	result = _sceAppMgrSetPowerSaveModeInternal(mode, suspendBackgroundApps);
	sceSyscallLeave(syscallState);
	return result;
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

/* asks the process state updater to suspend or resume a background application */
static inline __attribute__((always_inline))
int _sceAppMgrSuspendResumeBgAppByShell(int appId, int suspend)
{
	struct AppState *app;
	int result, unlockResult;

	if (!isShellProcess())
		return (int)0x8080201c;
	app = appmgrFindAppById(appId);
	if (app == NULL)
		return (int)0x80802016;
	if (!(app->launchInfo.processAttr & 1))
		return (int)0x8080201c;
	result = sceKernelLockMutex(g_appmgrMainMutex, 1, NULL);
	if (result != 0) {
#if FW_DEBUG
		SCE_DBG_PRINTF_FUNC(0, 504, "_sceAppMgrSuspendResumeBgAppByShell",
			"Failed to lock ret=0x%08x\n", result);
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_CHECK_FUNC(0, 504, "_sceAppMgrSuspendResumeBgAppByShell");
		SHELL_FAILURE(result, 505, "_sceAppMgrSuspendResumeBgAppByShell");
#else
		appmgrShellMutexPanic();
#endif
	} else {
		g_appmgrMainMutexOwner = sceKernelGetThreadId();
	}
	app->backgroundSuspendRequested = suspend;
	if (g_appmgrSystemSuspended == 0) {
		if (!suspend) {
			if (app->process.requestedProcessState == 3) {
				if (g_appmgrDebugNotifier != NULL)
					g_appmgrDebugNotifier(6, app->appId, app->process.processId, 0);
				result = sceKernelSignalSema(g_appmgrUpdateNotifySemaphore, 1);
				if (result != 0) {
#if FW_DEBUG
					SHELL_FAILURE(result, 515, "_sceAppMgrSuspendResumeBgAppByShell");
#else
					SceDebugForDriver_00CCE39C(&resumeBgAppPanicInfo,
						__builtin_return_address(0), "0x%08x,0x%08x\n", result, 0);
#endif
				}
			}
		} else if (app->process.requestedProcessState == 2) {
			if (g_appmgrDebugNotifier != NULL)
				g_appmgrDebugNotifier(5, app->appId, app->process.processId, 0);
			result = sceKernelSignalSema(g_appmgrUpdateNotifySemaphore, 1);
			if (result != 0) {
#if FW_DEBUG
				SHELL_FAILURE(result, 522, "_sceAppMgrSuspendResumeBgAppByShell");
#else
				SceDebugForDriver_00CCE39C(&suspendBgAppPanicInfo,
					__builtin_return_address(0), "0x%08x,0x%08x\n", result, 0);
#endif
			}
		}
	}
	g_appmgrMainMutexOwner = -1;
	unlockResult = sceKernelUnlockMutex(g_appmgrMainMutex, 1);
	if (unlockResult != 0)
		SHELL_FAILURE(unlockResult, 530, "_sceAppMgrSuspendResumeBgAppByShell");
	return result;
}

int appmgrSuspendBgAppByShell(int appId)
{
	return _sceAppMgrSuspendResumeBgAppByShell(appId, 1);
}

int appmgrResumeBgAppByShell(int appId)
{
	return _sceAppMgrSuspendResumeBgAppByShell(appId, 0);
}

int appmgrIsPidShellAndCrashed(SceUID pid)
{
	struct AppState *shell;
	int crashed, result;

	result = sceKernelLockMutex(g_appmgrMainMutex, 1, NULL);
	if (result != 0) {
#if FW_DEBUG
		SCE_DBG_PRINTF_FUNC(0, 554, "sceAppMgrIsPidShellAndCrashed",
			"Failed to lock ret=0x%08x\n", result);
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_CHECK_FUNC(0, 554, "sceAppMgrIsPidShellAndCrashed");
		SHELL_FAILURE(result, 555, "sceAppMgrIsPidShellAndCrashed");
#else
		appmgrShellMutexPanic();
#endif
	} else {
		g_appmgrMainMutexOwner = sceKernelGetThreadId();
	}
	shell = appmgrGetShellApp();
	crashed = shell != NULL && shell->process.processId == pid &&
		  appmgrGetAppRunStatus(shell) != 16;
	SHELL_MAIN_UNLOCK(566, "sceAppMgrIsPidShellAndCrashed");
	return crashed;
}
