/* System suspend and resume: every application suspended before the system sleeps. */

#include <stddef.h>
#include <fw.h>
#include <sysmem/debug.h>
#include <sysmem/sysroot.h>
#include <threadmgr/threadmgr.h>
#include <processmgr/processmgr.h>
#include <kernel/error.h>
#include <appmgr/appmgr.h>
#include "app_state_internal.h"
#include "appmgr_device.h"
#include "appmgr_internal_util.h"

extern SceUID g_appmgrMainMutex;
extern SceUID g_appmgrMainMutexOwner;
extern SceUID g_appmgrCore2Mutex;
extern SceUID g_appmgrCore2MutexOwner;
extern SceUID g_appmgrUpdateNotifySemaphore;
extern int g_appmgrSystemSuspended;

#if !FW_DEBUG
static const SceKernelDebugInfo systemPanicInfo = {
	.fileHash = 0xc9656e8e, .lineHash = 0xc879dadc, .funcHash = 0x11a05ee1,
};
static const SceKernelDebugInfo systemMutexPanicInfo = {
	.fileHash = 0xc9656e8e, .lineHash = 0x79d3a1f0, .funcHash = 0x9d536bce,
};
static const SceKernelDebugInfo suspendNotifyBootPanicInfo = {
	.fileHash = 0xf9faea88, .lineHash = 0xf350f892, .funcHash = 0xd8d1e268,
};
static const SceKernelDebugInfo suspendWaitProcessEventsPanicInfo = {
	.fileHash = 0xf9faea88, .lineHash = 0xc7c218c5, .funcHash = 0xc39847fd,
};
static const SceKernelDebugInfo suspendCheckProcessEventsPanicInfo = {
	.fileHash = 0xf9faea88, .lineHash = 0x10e5f098, .funcHash = 0xc39847fd,
};
static const SceKernelDebugInfo suspendStatusPanicInfo = {
	.fileHash = 0xf9faea88, .lineHash = 0x3881097c, .funcHash = 0xc39847fd,
};
static const SceKernelDebugInfo suspendRunningPanicInfo = {
	.fileHash = 0xf9faea88, .lineHash = 0x62ef5ed7, .funcHash = 0xc39847fd,
};
static const SceKernelDebugInfo suspendAllWaitProcessEventsPanicInfo = {
	.fileHash = 0xf9faea88, .lineHash = 0xc3ab520e, .funcHash = 0xc39847fd,
};
static const SceKernelDebugInfo suspendAllStatusPanicInfo = {
	.fileHash = 0xf9faea88, .lineHash = 0x405ed9e1, .funcHash = 0xc39847fd,
};
static const SceKernelDebugInfo suspendAllRunningPanicInfo = {
	.fileHash = 0xf9faea88, .lineHash = 0x66ee1323, .funcHash = 0xc39847fd,
};
static const SceKernelDebugInfo resumeCheckProcessEventsPanicInfo = {
	.fileHash = 0xf9faea88, .lineHash = 0xd2b7fdd2, .funcHash = 0xe1b6ee9e,
};
static const SceKernelDebugInfo resumeNotifyPanicInfo = {
	.fileHash = 0xf9faea88, .lineHash = 0x94e1ad40, .funcHash = 0xe1b6ee9e,
};

__attribute__((noinline)) _Noreturn void appmgrSystemPanic(void)
{
	SceDebugForDriver_391B5B74(&systemPanicInfo, __builtin_return_address(0));
}

__attribute__((noinline)) _Noreturn void appmgrSystemMutexPanic(void)
{
	SceDebugForDriver_391B5B74(&systemMutexPanicInfo, __builtin_return_address(0));
}
#endif

#if FW_DEBUG
#define SYSTEM_FAILURE(_value, _line, _func) do { \
	sceAppMgrDump(); \
	dumpBgmState(); \
	SCE_DBG_ASSERT_PRINTF_FUNC(0, _line, _func, \
		"[SceAppMgr] The actual value is %d(0x%08x), but expected value is %d(0x%08x)\n", \
		_value, _value, 0, 0); \
} while (0)
/* a check the release build stops on with appmgrSystemPanic */
#define SYSTEM_CHECK(_result, _line, _func) do { \
	if ((_result) != 0) \
		SYSTEM_FAILURE(_result, _line, _func); \
} while (0)
/* a check the release build reports with its own record */
#define SYSTEM_PANIC_CHECK(_result, _line, _func, _info) SYSTEM_CHECK(_result, _line, _func)
#define SYSTEM_ASSERT_FAILED(_line, _func, _info) do { \
	sceAppMgrDump(); \
	dumpBgmState(); \
	SCE_DBG_ASSERT_CHECK_FUNC(0, _line, _func); \
} while (0)
/* the caller holds no application's lock */
#define SYSTEM_CHECK_APP_OWNERS(_line, _func) do { \
	int slot; \
	for (slot = 0; slot != 20; ++slot) { \
		int level = SceDebugForDriver_374B7868(); \
		if (level > 0) { \
			SceUID slotOwner = g_appmgrAppStates[slot].mutexOwner; \
			SceUID tid = sceKernelGetThreadId(); \
			SCE_DBG_ASSERT_LEVEL_FUNC(1, slotOwner != tid, _line, _func); \
		} \
	} \
} while (0)
/* the caller holds neither the main lock nor, unless it holds core2, another application's */
#define SYSTEM_APP_LOCK_CHECK(_line, _func) do { \
	if (SceDebugForDriver_374B7868() > 0) { \
		SceUID owner = g_appmgrMainMutexOwner; \
		SceUID tid = sceKernelGetThreadId(); \
		SCE_DBG_ASSERT_LEVEL_FUNC(1, owner != tid, _line, _func); \
	} \
	if (g_appmgrCore2MutexOwner != sceKernelGetThreadId()) \
		SYSTEM_CHECK_APP_OWNERS(_line, _func); \
} while (0)
#define SYSTEM_APP_UNLOCK_CHECK(_line, _func) do { \
	if (SceDebugForDriver_374B7868() > 0) { \
		SceUID owner = g_appmgrMainMutexOwner; \
		SceUID tid = sceKernelGetThreadId(); \
		SCE_DBG_ASSERT_LEVEL_FUNC(1, owner != tid, _line, _func); \
	} \
} while (0)
/* a lock the caller may already hold (0x8002802c, 0x8002802d) fails the next line only */
#define SYSTEM_LOCK(_mutex, _owner, _line, _func) do { \
	int lockResult = sceKernelLockMutex(_mutex, 1, NULL); \
	if (lockResult != 0) { \
		if ((unsigned int)(lockResult + 0x7ffd7fd4) > 1) \
			SYSTEM_FAILURE(lockResult, _line, _func); \
		SYSTEM_FAILURE(lockResult, (_line) + 1, _func); \
	} else { \
		_owner = sceKernelGetThreadId(); \
	} \
} while (0)
#define SYSTEM_MAIN_LOCK_FAILED(_result, _line, _func) do { \
	SCE_DBG_PRINTF_FUNC(0, _line, _func, "Failed to lock ret=0x%08x\n", _result); \
	sceAppMgrDump(); \
	dumpBgmState(); \
	SCE_DBG_ASSERT_CHECK_FUNC(0, _line, _func); \
} while (0)
#define SYSTEM_MAIN_LOCK(_line, _func) do { \
	int lockResult = sceKernelLockMutex(g_appmgrMainMutex, 1, NULL); \
	if (lockResult != 0) \
		SYSTEM_MAIN_LOCK_FAILED(lockResult, _line, _func); \
	else \
		g_appmgrMainMutexOwner = sceKernelGetThreadId(); \
} while (0)
/* the main lock taken only to read: the owner is not recorded */
#define SYSTEM_MAIN_LOCK_READ(_line, _func) do { \
	int lockResult = sceKernelLockMutex(g_appmgrMainMutex, 1, NULL); \
	if (lockResult != 0) \
		SYSTEM_MAIN_LOCK_FAILED(lockResult, _line, _func); \
	else \
		sceKernelGetThreadId(); \
} while (0)
/* the same, its result checked again on the next line */
#define SYSTEM_MAIN_LOCK_READ_CHECKED(_line, _func) do { \
	int lockResult = sceKernelLockMutex(g_appmgrMainMutex, 1, NULL); \
	if (lockResult != 0) { \
		SYSTEM_MAIN_LOCK_FAILED(lockResult, _line, _func); \
		SYSTEM_FAILURE(lockResult, (_line) + 1, _func); \
	} else { \
		sceKernelGetThreadId(); \
	} \
} while (0)
#define SYSTEM_UNLOCK(_mutex, _owner, _line, _func) do { \
	int unlockResult; \
	_owner = -1; \
	unlockResult = sceKernelUnlockMutex(_mutex, 1); \
	if (unlockResult != 0) \
		SYSTEM_FAILURE(unlockResult, _line, _func); \
} while (0)
#define SYSTEM_UNLOCK_CHECKED(_mutex, _owner, _line, _func) do { \
	int unlockResult; \
	_owner = -1; \
	unlockResult = sceKernelUnlockMutex(_mutex, 1); \
	if (unlockResult != 0) { \
		SYSTEM_FAILURE(unlockResult, _line, _func); \
		SYSTEM_FAILURE(unlockResult, (_line) + 1, _func); \
	} \
} while (0)
#else
#define SYSTEM_CHECK(_result, _line, _func) do { \
	if ((_result) != 0) \
		appmgrSystemPanic(); \
} while (0)
#define SYSTEM_PANIC_CHECK(_result, _line, _func, _info) do { \
	if ((_result) != 0) \
		SceDebugForDriver_00CCE39C(&_info, __builtin_return_address(0), \
			"0x%08x,0x%08x\n", _result, 0); \
} while (0)
#define SYSTEM_ASSERT_FAILED(_line, _func, _info) \
	SceDebugForDriver_391B5B74(&_info, __builtin_return_address(0))
#define SYSTEM_CHECK_APP_OWNERS(_line, _func) do { } while (0)
#define SYSTEM_APP_LOCK_CHECK(_line, _func) ((void)sceKernelGetThreadId())
#define SYSTEM_APP_UNLOCK_CHECK(_line, _func) do { } while (0)
#define SYSTEM_LOCK(_mutex, _owner, _line, _func) do { \
	int lockResult = sceKernelLockMutex(_mutex, 1, NULL); \
	if (lockResult != 0) { \
		if ((unsigned int)(lockResult + 0x7ffd7fd4) > 1) \
			appmgrSystemMutexPanic(); \
		appmgrSystemPanic(); \
	} \
	_owner = sceKernelGetThreadId(); \
} while (0)
#define SYSTEM_MAIN_LOCK(_line, _func) do { \
	if (sceKernelLockMutex(g_appmgrMainMutex, 1, NULL) != 0) \
		appmgrSystemMutexPanic(); \
	g_appmgrMainMutexOwner = sceKernelGetThreadId(); \
} while (0)
#define SYSTEM_MAIN_LOCK_READ(_line, _func) do { \
	if (sceKernelLockMutex(g_appmgrMainMutex, 1, NULL) != 0) \
		appmgrSystemMutexPanic(); \
	sceKernelGetThreadId(); \
} while (0)
#define SYSTEM_MAIN_LOCK_READ_CHECKED(_line, _func) SYSTEM_MAIN_LOCK_READ(_line, _func)
#define SYSTEM_UNLOCK(_mutex, _owner, _line, _func) do { \
	_owner = -1; \
	if (sceKernelUnlockMutex(_mutex, 1) != 0) \
		appmgrSystemMutexPanic(); \
} while (0)
#define SYSTEM_UNLOCK_CHECKED(_mutex, _owner, _line, _func) \
	SYSTEM_UNLOCK(_mutex, _owner, _line, _func)
#endif

#define SYSTEM_MAIN_UNLOCK(_line, _func) \
	SYSTEM_UNLOCK(g_appmgrMainMutex, g_appmgrMainMutexOwner, _line, _func)

/* resume reads the attributes under a main lock it records from 4.00 on no more */
#if FW_SINCE(0x04000000)
#define SYSTEM_RESUME_MAIN_LOCK(_line, _func) SYSTEM_MAIN_LOCK_READ_CHECKED(_line, _func)
#else
#define SYSTEM_RESUME_MAIN_LOCK(_line, _func) SYSTEM_MAIN_LOCK(_line, _func)
#endif

#define SYSTEM_APP_UNLOCK(_app, _line, _func) do { \
	int unlockResult = _appListUnlockApp(_app); \
	SYSTEM_CHECK(unlockResult, _line, _func); \
} while (0)

/*
 * Suspends one application and waits up to 100 ms for it, asking once; an application quitting
 * is made to quit now. skipBackground leaves the applications the shell suspends itself.
 */
#if FW_DEBUG
static void systemSuspendApp(struct AppState *app, int skipBackground)
#else
static inline __attribute__((always_inline)) void systemSuspendApp(struct AppState *app,
	int skipBackground)
#endif
{
	struct AppmgrWaitObject *waitObject;
	SceUInt32 timeout;
	SceUInt32 background, state;
	SceUID pid;
	int retried, result;

	waitObject = appmgrUtilGetWaitObject(0);
	appmgrUtilWaitObjectClear(waitObject);
	retried = 0;
	for (;;) {
		SYSTEM_APP_LOCK_CHECK(34, "systemSuspendApp");
		SYSTEM_LOCK(app->mutex, app->mutexOwner, 34, "systemSuspendApp");
		pid = app->process.processId;
		if (pid < 0 || !(pid & 1)) {
			SYSTEM_APP_UNLOCK(app, 42, "systemSuspendApp");
			return;
		}
		SYSTEM_MAIN_LOCK_READ(47, "systemSuspendApp");
		background = app->launchInfo.processAttr & 1;
		state = app->state & 0xff0;
		SYSTEM_MAIN_UNLOCK(50, "systemSuspendApp");
		if (skipBackground && background) {
			SYSTEM_APP_UNLOCK(app, 54, "systemSuspendApp");
			return;
		}
		if (state == 0x700) {
			result = _appmgrProcessQuitProcess(app, 1, 0, 0);
			SYSTEM_CHECK(result, 60, "systemSuspendApp");
			SYSTEM_APP_UNLOCK(app, 63, "systemSuspendApp");
			return;
		}
		if (app->process.requestedProcessState != 2 &&
		    app->process.notifiedProcessState != 2) {
			SYSTEM_APP_UNLOCK(app, 99, "systemSuspendApp");
			return;
		}
		if (!retried) {
			result = _appmgrProcessSuspendProcess(app, 0, waitObject);
			SYSTEM_CHECK(result, 75, "systemSuspendApp");
			SYSTEM_MAIN_LOCK(77, "systemSuspendApp");
			if (app->process.processId == pid)
				appmgrSetAppStateFlags(app, 0x2000);
			SYSTEM_MAIN_UNLOCK(81, "systemSuspendApp");
		}
		SYSTEM_APP_UNLOCK(app, 87, "systemSuspendApp");
		timeout = 100000;
		if (appmgrUtilWaitObjectWait(waitObject, &timeout) == 0)
			return;
		retried = 1;
	}
}

/* suspends the foreground applications, then the background ones, and stops each process state */
static inline __attribute__((always_inline)) void _appmgrSuspendAllAppAndLockProcessState(void)
{
	struct AppState *app;
	SceUInt32 status;
	SceUID pid;
	int index, result;

	index = appmgrGetShellApp() != NULL;
	app = &g_appmgrAppStates[index];
	do {
		systemSuspendApp(app, 1);
		++index;
		++app;
	} while (index <= 19);
	result = SceProcessmgrForKernel_8321B827(NULL);
	SYSTEM_PANIC_CHECK(result, 120, "_appmgrSuspendAllAppAndLockProcessState",
		suspendWaitProcessEventsPanicInfo);
	result = SceProcessmgrForKernel_D5DF895D();
	SYSTEM_PANIC_CHECK(result, 122, "_appmgrSuspendAllAppAndLockProcessState",
		suspendCheckProcessEventsPanicInfo);
	index = appmgrGetShellApp() != NULL;
	app = &g_appmgrAppStates[index];
	do {
		pid = app->process.processId;
		if (app->appId > 0 && pid >= 0 && (pid & 1) &&
		    !(app->launchInfo.processAttr & 1)) {
			result = sceKernelGetProcessStatus(pid, &status);
			if (result != SCE_KERNEL_ERROR_INVALID_PID) {
				SYSTEM_PANIC_CHECK(result, 137, "_appmgrSuspendAllAppAndLockProcessState",
					suspendStatusPanicInfo);
				if (status == 2)
					SYSTEM_ASSERT_FAILED(140, "_appmgrSuspendAllAppAndLockProcessState",
						suspendRunningPanicInfo);
			}
		}
		++index;
		++app;
	} while (index <= 19);

	index = appmgrGetShellApp() != NULL;
	app = &g_appmgrAppStates[index];
	do {
		systemSuspendApp(app, 0);
		++index;
		++app;
	} while (index <= 19);
	result = SceProcessmgrForKernel_8321B827(NULL);
	SYSTEM_PANIC_CHECK(result, 154, "_appmgrSuspendAllAppAndLockProcessState",
		suspendAllWaitProcessEventsPanicInfo);
	index = appmgrGetShellApp() != NULL;
	app = &g_appmgrAppStates[index];
	do {
		pid = app->process.processId;
		if (app->appId > 0 && pid >= 0 && (pid & 1)) {
			result = sceKernelGetProcessStatus(pid, &status);
			if (result != SCE_KERNEL_ERROR_INVALID_PID) {
				SYSTEM_PANIC_CHECK(result, 167, "_appmgrSuspendAllAppAndLockProcessState",
					suspendAllStatusPanicInfo);
				if (status == 2)
					SYSTEM_ASSERT_FAILED(170, "_appmgrSuspendAllAppAndLockProcessState",
						suspendAllRunningPanicInfo);
			}
		}
		++index;
		++app;
	} while (index <= 19);
}

int _sceAppMgrSystemSuspend(void)
{
	struct AppState *app;
	int index, result;

	SCE_DBG_PRINTF_FUNC(0, 184, "_sceAppMgrSystemSuspend", "[SceAppMgr] Start system suspend.\n");
	if (g_appmgrDebugNotifier != NULL)
		g_appmgrDebugNotifier(7, 0, 0, 0);
	result = _sceAppMgrSetPowerSaveModeInternal(1, 1);
	SYSTEM_CHECK(result, 189, "_sceAppMgrSystemSuspend");
	SYSTEM_CHECK_APP_OWNERS(191, "_sceAppMgrSystemSuspend");
	SYSTEM_LOCK(g_appmgrCore2Mutex, g_appmgrCore2MutexOwner, 191, "_sceAppMgrSystemSuspend");
	SYSTEM_MAIN_LOCK(194, "_sceAppMgrSystemSuspend");
	g_appmgrSystemSuspended = 1;
	index = appmgrGetShellApp() != NULL;
	app = &g_appmgrAppStates[index];
	do {
		if (app->launchInfo.notifyBootComplete) {
			result = SceSysrootForDriver_421EFC96(0x80802027);
			SYSTEM_PANIC_CHECK(result, 202, "_sceAppMgrSystemSuspend",
				suspendNotifyBootPanicInfo);
			app->launchInfo.notifyBootComplete = 0;
		}
		++index;
		++app;
	} while (index <= 19);
	_deviceSetAudioPortsDisabled(1);
	SYSTEM_MAIN_UNLOCK(212, "_sceAppMgrSystemSuspend");
	_updatePowerTickStateForNonSuspendableApp();
	_updateDeviceArbitration2();
	_appmgrSuspendAllAppAndLockProcessState();
	_deviceSetBgmEnabled(0);
	SCE_DBG_PRINTF_FUNC(0, 232, "_sceAppMgrSystemSuspend",
		"[SceAppMgr] System suspend completed.\n");
	return 0;
}

int _sceAppMgrSystemResume(void)
{
	struct AppState *app;
	SceUInt32 attributes;
	int index, resume, result;

	SCE_DBG_PRINTF_FUNC(0, 241, "_sceAppMgrSystemResume", "[SceAppMgr] Start system resume.\n");
	if (g_appmgrDebugNotifier != NULL)
		g_appmgrDebugNotifier(8, 0, 0, 0);
	result = SceProcessmgrForKernel_D5DF895D();
	SYSTEM_PANIC_CHECK(result, 246, "_sceAppMgrSystemResume", resumeCheckProcessEventsPanicInfo);
	SYSTEM_MAIN_LOCK_READ_CHECKED(257, "_sceAppMgrSystemResume");
	g_appmgrMainMutexOwner = -1;
	g_appmgrSystemSuspended = 0;
	result = sceKernelUnlockMutex(g_appmgrMainMutex, 1);
#if FW_DEBUG
	if (result != 0)
		SYSTEM_FAILURE(result, 260, "_sceAppMgrSystemResume");
#else
	if (result != 0)
		appmgrSystemMutexPanic();
#endif
	index = appmgrGetShellApp() != NULL;
	app = &g_appmgrAppStates[index];
	do {
		SYSTEM_APP_LOCK_CHECK(265, "_sceAppMgrSystemResume");
		SYSTEM_LOCK(app->mutex, app->mutexOwner, 265, "_sceAppMgrSystemResume");
		if (app->process.processId >= 0 && (app->process.processId & 1)) {
			SYSTEM_RESUME_MAIN_LOCK(273, "_sceAppMgrSystemResume");
			attributes = app->launchInfo.processAttr;
			if (attributes & 2)
				resume = 1;
			else if (attributes & 1)
				resume = !app->backgroundSuspendRequested;
			else
				resume = 0;
			SYSTEM_MAIN_UNLOCK(279, "_sceAppMgrSystemResume");
			if (resume) {
				appmgrMaskAppStateFlags(app, ~0x2000);
				if (_appmgrProcessStartProcess(app) != 0)
					_appmgrProcessQuitProcess(app, 2, 0, 0);
			}
		}
		SYSTEM_APP_UNLOCK_CHECK(292, "_sceAppMgrSystemResume");
		SYSTEM_UNLOCK_CHECKED(app->mutex, app->mutexOwner, 292, "_sceAppMgrSystemResume");
		++index;
		++app;
	} while (index <= 19);
	SYSTEM_UNLOCK_CHECKED(g_appmgrCore2Mutex, g_appmgrCore2MutexOwner, 299,
		"_sceAppMgrSystemResume");
	updateActiveAppState();
	result = sceKernelSignalSema(g_appmgrUpdateNotifySemaphore, 1);
	SYSTEM_PANIC_CHECK(result, 306, "_sceAppMgrSystemResume", resumeNotifyPanicInfo);
	SCE_DBG_PRINTF_FUNC(0, 308, "_sceAppMgrSystemResume", "[SceAppMgr] System resume completed.\n");
	return 0;
}
