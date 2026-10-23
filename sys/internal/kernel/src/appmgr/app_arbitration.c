/* Application launch and arbitration helpers. */

#include <fw.h>
#include <sysmem/debug.h>
#include <sysmem/barrier.h>
#include <threadmgr/threadmgr.h>
#include <threadmgr/threadmgr_event.h>
#include <iofilemgr/iofilemgr.h>
#include <sysmem/sysmem.h>
#include <sysmem/sysclib.h>
#include <sysmem/pmmgr.h>
#include <appmgr.h>
#include "app_state_internal.h"
#include <processmgr/processmgr.h>
#include <appmgr/appmgr.h>
#include "appmgr_app_event.h"
#include "appmgr_event.h"
#include "appmgr_device.h"
#include "appmgr_internal_util.h"
#include "appmgr_shell_event.h"
#include <sysmem/sblaimgr.h>
#include <acmgr/acmgr.h>

extern SceUID g_appmgrMainMutexOwner;

struct AppState;
extern int _processUpdateThreadEntry(SceSize args, void *argp);
extern int _gcListenerThreadEntry(SceSize args, void *argp);
extern SceUID g_appmgrMainMutex;
extern SceUID g_appmgrCore2Mutex;
extern SceUID g_appmgrCore2MutexOwner;
extern SceUID g_appmgrCoreKmMutex;
extern SceUID g_appmgrCoreKmMutexOwner;
extern int g_appmgrSystemSuspended;
extern int g_appmgrPowerSaveMode;
extern int g_appmgrPowerSaveSuspendBackgroundApps;
extern int g_appmgrAvailableFD;
extern int g_appmgrAvailableDIR;
extern int g_appmgrAvailablePrimaryResourceQuota;
extern int g_appmgrAvailableSecondaryResourceQuota;

SceUID g_appmgrUpdateNotifySemaphore = -1;
SceUID g_appmgrProcessStateUpdaterThread = -1;
SceUID g_appmgrGcListenerThread = -1;

#if FW_DEBUG
SceUID g_appmgrGrw0MountEvent;
SceUID g_appmgrGro0MountEvent;
SceUID g_appmgrUx0MountEvent;
#else
SceUID g_appmgrUx0MountEvent;
SceUID g_appmgrGrw0MountEvent;
SceUID g_appmgrGro0MountEvent;
#endif

SceKernelWaitEvent g_appmgrGcWaitEvents[3];

#if !FW_DEBUG
static const SceKernelDebugInfo gcListenerStartPanicInfo = {
	.fileHash = 0xb8f4da66,
	.lineHash = 0x85948057,
	.funcHash = 0xa0a0ebb7,
};

static const SceKernelDebugInfo processStateUpdaterStartPanicInfo = {
	.fileHash = 0xb8f4da66,
	.lineHash = 0x0a3acb7c,
	.funcHash = 0xa0a0ebb7,
};

static const SceKernelDebugInfo arbitrationPanicInfo = {
	.fileHash = 0xc9656e8e,
	.lineHash = 0xc879dadc,
	.funcHash = 0x11a05ee1,
};

static const SceKernelDebugInfo gcListenerMutexPanicInfo = {
	.fileHash = 0xc9656e8e,
	.lineHash = 0x79d3a1f0,
	.funcHash = 0x9d536bce,
};

static const SceKernelDebugInfo deleteBudgetPanicInfo = {
	.fileHash = 0xb8f4da66, .lineHash = 0x422f9b9f, .funcHash = 0xaad107e4,
};
static const SceKernelDebugInfo launchStatusCopyPanicInfo = {
	.fileHash = 0xb8f4da66, .lineHash = 0xbc110f71, .funcHash = 0x7e21b0b3,
};
static const SceKernelDebugInfo launchParamResultCopyPanicInfo = {
	.fileHash = 0xb8f4da66, .lineHash = 0x6cf3815c, .funcHash = 0x7e21b0b3,
};
static const SceKernelDebugInfo gcWaitProcessEventsPanicInfo = {
	.fileHash = 0xb8f4da66, .lineHash = 0x658a5287, .funcHash = 0x1bbf336b,
};
static const SceKernelDebugInfo gcCheckProcessEventsPanicInfo = {
	.fileHash = 0xb8f4da66, .lineHash = 0xffb33277, .funcHash = 0x1bbf336b,
};
static const SceKernelDebugInfo gcGameWaitProcessEventsPanicInfo = {
	.fileHash = 0xb8f4da66, .lineHash = 0x7031627a, .funcHash = 0x1bbf336b,
};
static const SceKernelDebugInfo gcGameCheckProcessEventsPanicInfo = {
	.fileHash = 0xb8f4da66, .lineHash = 0x8299cbeb, .funcHash = 0x1bbf336b,
};
static const SceKernelDebugInfo removableWaitProcessEventsPanicInfo = {
	.fileHash = 0xb8f4da66, .lineHash = 0x0331b8ce, .funcHash = 0x64fee94b,
};
static const SceKernelDebugInfo removableCheckProcessEventsPanicInfo = {
	.fileHash = 0xb8f4da66, .lineHash = 0x3b7d0a8c, .funcHash = 0x64fee94b,
};
static const SceKernelDebugInfo removableGameWaitProcessEventsPanicInfo = {
	.fileHash = 0xb8f4da66, .lineHash = 0x026fd4e8, .funcHash = 0x64fee94b,
};
static const SceKernelDebugInfo removableGameCheckProcessEventsPanicInfo = {
	.fileHash = 0xb8f4da66, .lineHash = 0x1b174385, .funcHash = 0x64fee94b,
};

__attribute__((noinline)) void appmgrArbitrationPanic(void)
{
	SceDebugForDriver_391B5B74(&arbitrationPanicInfo, __builtin_return_address(0));
}

__attribute__((noinline)) _Noreturn void appmgrGcListenerMutexPanic(void)
{
	SceDebugForDriver_391B5B74(&gcListenerMutexPanicInfo, __builtin_return_address(0));
}
#endif

int _onlaunchCallback(int notifyId, int notifyCount, int notifyArg, void *userData)
{
	SceUID *semaphore = userData;
	int result;

	(void)notifyId;
	(void)notifyCount;
	(void)notifyArg;
	result = sceKernelSignalSema(*semaphore, 1);
	if (result != 0) {
#if FW_DEBUG
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_PRINTF_FUNC(0, 2119, "_onlaunchCallback",
			"[SceAppMgr] The actual value is %d(0x%08x), but expected value is %d(0x%08x)\n",
			result, result, 0, 0);
#else
		appmgrArbitrationPanic();
#endif
	}
	return 0;
}

#if FW_DEBUG
int copyArgv(char *destination, const char *const *userArgv)
{
	int total = 0;
	char *cursor = destination;
	const char *argument;
	int result;
	int length;

	*cursor = '\0';
	if (userArgv == NULL)
		return 0;
	for (;;) {
		result = sceKernelCopyFromUser(&argument, userArgv, sizeof(argument));
		if (result != 0)
			return result;
		if (argument == NULL) {
			if (total > 1024) {
				sceAppMgrDump();
				dumpBgmState();
				SCE_DBG_ASSERT_CHECK_FUNC(0, 925, "copyArgv");
			}
			return total;
		}
		result = sceKernelStrncpyFromUser(cursor, argument, 1024 - total);
		if (result == 1024 - total)
			return (int)0x8001005bU;
		if (result < 0)
			return result;
		length = result + 1;
		if ((SceUInt32)(1024 - total) < (SceUInt32)length) {
			sceAppMgrDump();
			dumpBgmState();
			SCE_DBG_ASSERT_CHECK_FUNC(0, 942, "copyArgv");
		}
		total += length;
		cursor += length;
		++userArgv;
	}
}
#endif

#if FW_DEBUG
#define DELETE_APP_FAILURE(_value, _expected, _line, _func) do { \
	sceAppMgrDump(); \
	dumpBgmState(); \
	SCE_DBG_ASSERT_PRINTF_FUNC(0, _line, _func, \
		"[SceAppMgr] The actual value is %d(0x%08x), but expected value is %d(0x%08x)\n", \
		_value, _value, _expected, _expected); \
} while (0)
#define DELETE_APP_ASSERT(_cond, _line, _func) do { \
	if (!(_cond)) { \
		sceAppMgrDump(); \
		dumpBgmState(); \
		SCE_DBG_ASSERT_CHECK_FUNC(0, _line, _func); \
	} \
} while (0)
#define DELETE_APP_CHECK(_value, _expected, _line, _func) do { \
	if ((_value) != (_expected)) \
		DELETE_APP_FAILURE(_value, _expected, _line, _func); \
} while (0)
#define DELETE_APP_MAIN_LOCK(_line, _func) do { \
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
#define DELETE_APP_MAIN_UNLOCK(_line, _func) do { \
	int unlockResult; \
	g_appmgrMainMutexOwner = -1; \
	unlockResult = sceKernelUnlockMutex(g_appmgrMainMutex, 1); \
	if (unlockResult != 0) \
		DELETE_APP_FAILURE(unlockResult, 0, _line, _func); \
} while (0)
#define DELETE_APP_PANIC_CHECK(_result, _line, _func) \
	DELETE_APP_CHECK(_result, 0, _line, _func)
#else
#define DELETE_APP_ASSERT(_cond, _line, _func) do { \
	if (!(_cond)) \
		appmgrArbitrationPanic(); \
} while (0)
#define DELETE_APP_CHECK(_value, _expected, _line, _func) \
	DELETE_APP_ASSERT((_value) == (_expected), _line, _func)
#define DELETE_APP_MAIN_LOCK(_line, _func) do { \
	if (sceKernelLockMutex(g_appmgrMainMutex, 1, NULL) != 0) \
		appmgrGcListenerMutexPanic(); \
	g_appmgrMainMutexOwner = sceKernelGetThreadId(); \
} while (0)
#define DELETE_APP_MAIN_UNLOCK(_line, _func) do { \
	g_appmgrMainMutexOwner = -1; \
	if (sceKernelUnlockMutex(g_appmgrMainMutex, 1) != 0) \
		appmgrGcListenerMutexPanic(); \
} while (0)
#define DELETE_APP_PANIC_CHECK(_result, _line, _func) do { \
	if ((_result) != 0) \
		SceDebugForDriver_00CCE39C(&deleteBudgetPanicInfo, __builtin_return_address(0), \
			"0x%08x,0x%08x\n", _result, 0); \
} while (0)
#endif

/* what is left plus what the applications hold must be the whole of each resource */
static inline __attribute__((always_inline)) void checkFds(void)
{
	struct AppState *app;
	int fd = g_appmgrAvailableFD;
	int dir = g_appmgrAvailableDIR;
	int primary = g_appmgrAvailablePrimaryResourceQuota;
	int secondary = g_appmgrAvailableSecondaryResourceQuota;
	int maxFD, directoryFds;

	DELETE_APP_ASSERT(g_appmgrMainMutexOwner == sceKernelGetThreadId(), 516, "checkFds");
	for (app = g_appmgrAppStates; app != &g_appmgrAppStates[20]; ++app) {
		if (app->launchInfo.budgetType == 0 && app->process.budgetId > 0) {
			maxFD = app->process.maxFD;
			directoryFds = maxFD > 2 ? maxFD - 2 : maxFD;
			fd += maxFD;
			dir += directoryFds * (int)app->process.maxDIR;
			primary += app->process.primaryResourceQuota;
			secondary += app->process.secondaryResourceQuota;
		}
	}
	DELETE_APP_CHECK(fd, 92, 532, "checkFds");
	DELETE_APP_CHECK(dir, 552, 533, "checkFds");
	DELETE_APP_CHECK(primary, 16, 534, "checkFds");
	DELETE_APP_CHECK(secondary, 12, 535, "checkFds");
}

/* the application is unloaded: gives its resources back and frees its slot */
int _sceAppMgrDeleteAppInternal(struct AppState *app)
{
	struct AppmgrWaitObject *unloadWaiter;
	SceUInt32 budgetType, requestedProcessState;
	SceUID budgetId, owner;
	int maxFD, permission, result;

	DELETE_APP_ASSERT(app != NULL && app->mutexOwner == sceKernelGetThreadId(), 2345,
		"_sceAppMgrDeleteAppInternal");
	owner = g_appmgrMainMutexOwner;
	DELETE_APP_ASSERT(owner != sceKernelGetThreadId(), 2346, "_sceAppMgrDeleteAppInternal");
	DELETE_APP_MAIN_LOCK(2348, "_sceAppMgrDeleteAppInternal");
	if (appmgrGetAppRunStatus(app) != 0x800)
		return (int)0x80802013;
	budgetType = app->launchInfo.budgetType;
	budgetId = -1;
	if (budgetType == 0) {
		budgetId = app->process.budgetId;
		app->process.budgetId = -1;
		if (budgetId > 0) {
			maxFD = app->process.maxFD;
			g_appmgrAvailableFD += maxFD;
			if (maxFD > 2)
				maxFD -= 2;
			g_appmgrAvailableDIR += (int)app->process.maxDIR * maxFD;
			g_appmgrAvailablePrimaryResourceQuota += app->process.primaryResourceQuota;
			g_appmgrAvailableSecondaryResourceQuota += app->process.secondaryResourceQuota;
			app->process.maxFD = 0;
			app->process.maxDIR = 0;
			app->process.primaryResourceQuota = 0;
			app->process.secondaryResourceQuota = 0;
			checkFds();
		}
	}
	DELETE_APP_MAIN_UNLOCK(2374, "_sceAppMgrDeleteAppInternal");
	if (budgetId > 0) {
		permission = sceKernelSetThreadAccessLevel(0x80);
		result = SceProcessmgrForKernel_459A223B(budgetId);
		DELETE_APP_PANIC_CHECK(result, 2382, "_sceAppMgrDeleteAppInternal");
		sceKernelSetThreadAccessLevel(permission);
	}
	requestedProcessState = app->process.requestedProcessState;
	DELETE_APP_MAIN_LOCK(2388, "_sceAppMgrDeleteAppInternal");
	_deviceDeleteApp(app->appId);
	unloadWaiter = app->process.unloadWaiter;
	app->process.unloadWaiter = NULL;
	appmgrClearPendingAppEvents(app);
	_appListClearAppState(app);
	DELETE_APP_MAIN_UNLOCK(2397, "_sceAppMgrDeleteAppInternal");
	if (requestedProcessState == 2)
		_shellEventAppRunningStatusChangedQueuePush();
	if (budgetType == 0)
		appmgrUtilWaitObjectNotify(appmgrUtilGetWaitObject(1));
	else if (budgetType == 1)
		appmgrUtilWaitObjectNotify(appmgrUtilGetWaitObject(2));
	appmgrUtilWaitObjectNotify(appmgrUtilGetWaitObject(3));
	if (unloadWaiter != NULL)
		appmgrUtilWaitObjectNotify(unloadWaiter);
	return 0;
}

/* whether the application runs now: not while the system sleeps, nor in power save mode */
int _applicationShouldMakeRun(struct AppState *app)
{
	SceUInt32 attributes;
	int run;

	DELETE_APP_MAIN_LOCK(2853, "_applicationShouldMakeRun");
	if (g_appmgrSystemSuspended) {
		run = 0;
	} else {
		attributes = app->launchInfo.processAttr;
		if (attributes & 1)
			run = 1;
		else if (attributes & 2)
			run = SceSblAIMgrForDriver_71608CA3() && g_appmgrPowerSaveMode ?
				!g_appmgrPowerSaveSuspendBackgroundApps : 1;
		else if (!g_appmgrPowerSaveMode)
			run = 1;
		else
			run = g_appmgrPowerSaveSuspendBackgroundApps ? 0 :
				app->process.runWhenUiOverlay;
	}
	DELETE_APP_MAIN_UNLOCK(2883, "_applicationShouldMakeRun");
	return run;
}

#if FW_SINCE(0x04000000)
/* spawns the application again in place of the process that just ended, pid */
int relaunchApp(struct AppState *app, SceUID pid)
{
	AppmgrSpawnAppParam param;
	SceKernelProcessOpt opt;
	struct {
		SceUInt32 reserved[4];
		char reservedBuffer[128];
	} unused;
	SceUID owner;
	int error, result;

	DELETE_APP_ASSERT(app != NULL && app->mutexOwner == sceKernelGetThreadId(), 1779,
		"relaunchApp");
	owner = g_appmgrMainMutexOwner;
	DELETE_APP_ASSERT(owner != sceKernelGetThreadId(), 1780, "relaunchApp");
	DELETE_APP_ASSERT(app->relaunchPending != 0, 1782, "relaunchApp");
	app->relaunchPending = 0;
	app->relaunchInProgress = 1;
	memset(unused.reservedBuffer, 0, sizeof(unused.reservedBuffer));
	unused.reserved[0] = 0;
	unused.reserved[1] = 0;
	unused.reserved[2] = 0;
	unused.reserved[3] = 0;
	param.started = appmgrGetAppRunState(app) == 2;
	memset(&opt, 0, sizeof(opt));
	opt.flags = 0x20000;
	opt.kind = 1;
	opt.prevPid = pid;
	param.app = app;
	param.opt = &opt;
	param.options = &app->launchOptions;
	param.createParam = NULL;
	param.error = &error;
	result = (int)SceThreadmgrForDriver_E54FD746(0x4000, _spawnApp, &param);
	if (result != 0)
		SceProcessmgrForKernel_344DDE0B(pid, result);
	else
		_updateDeviceArbitration2();
	return result;
}
#endif

/* runs or suspends the application as the power state now asks */
int _updateAppOnPowerTickState(struct AppState *app)
{
	SceUInt32 requestedProcessState;
	int result;

	if (app != NULL && (app->state & 0xff0) == 0x700)
		return 0;
	if (_applicationShouldMakeRun(app)) {
		result = sceKernelLockMutex(g_appmgrMainMutex, 1, NULL);
#if FW_DEBUG
		if (result != 0) {
			SCE_DBG_PRINTF_FUNC(0, 4306, "_updateAppOnPowerTickState",
				"Failed to lock ret=0x%08x\n", result);
			sceAppMgrDump();
			dumpBgmState();
			SCE_DBG_ASSERT_CHECK_FUNC(0, 4306, "_updateAppOnPowerTickState");
		} else {
			sceKernelGetThreadId();
		}
#else
		if (result != 0)
			appmgrGcListenerMutexPanic();
		sceKernelGetThreadId();
#endif
		g_appmgrMainMutexOwner = -1;
		requestedProcessState = app->process.requestedProcessState;
		result = sceKernelUnlockMutex(g_appmgrMainMutex, 1);
#if FW_DEBUG
		if (result != 0)
			DELETE_APP_FAILURE(result, 0, 4308, "_updateAppOnPowerTickState");
#else
		if (result != 0)
			appmgrGcListenerMutexPanic();
#endif
		if (requestedProcessState == 3)
			return _appmgrProcessStartProcess(app);
		result = sceKernelSignalSema(g_appmgrUpdateNotifySemaphore, 1);
		DELETE_APP_CHECK(result, 0, 4317, "_updateAppOnPowerTickState");
		return result;
	}
	if (SceSblACMgrForDriver_E87D1777() && SceProcessmgrForDriver_C715591F())
		return 0;
	_appListSetRunStatus(app, 0x20);
	result = _appmgrProcessSuspendProcess(app, 0, NULL);
	DELETE_APP_CHECK(result, 0, 4332, "_updateAppOnPowerTickState");
	return result;
}

/* tells a launcher whose launch failed to resume */
#if FW_DEBUG
void sendResumeToPid(SceUID pid)
#else
static inline __attribute__((always_inline)) void sendResumeToPid(SceUID pid)
#endif
{
	struct AppState *app = NULL;
	struct AppState *lockedApp;
	SceAppMgrSystemEvent event;
	int result;

	if (_appListFindAndLockAppByProcessId(pid, &app) != 0)
		return;
	lockedApp = app;
	if (lockedApp == NULL) {
		DELETE_APP_ASSERT(0, 134, "sendResumeToPid");
		lockedApp = app;
	}
	memset(&event, 0, sizeof(event));
	event.systemEvent = SCE_APPMGR_SYSTEMEVENT_ON_RESUME;
	result = _appListPushEvent(lockedApp, &event);
	DELETE_APP_ASSERT(result == 0, 139, "sendResumeToPid");
	result = _appListUnlockApp(app);
	DELETE_APP_ASSERT(result == 0, 142, "sendResumeToPid");
}

#if !FW_DEBUG
inline __attribute__((always_inline, gnu_inline))
#endif
int getMiniAppValue(struct AppState *app)
{
	SceUID owner = g_appmgrMainMutexOwner;
	SceUID pid;
	int value, runningValue;

#if FW_DEBUG
	if (owner != sceKernelGetThreadId()) {
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_CHECK_FUNC(0, 148, "getMiniAppValue");
	}
	if (app == NULL) {
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_CHECK_FUNC(0, 149, "getMiniAppValue");
	}
#else
	if (owner != sceKernelGetThreadId() || app == NULL)
		appmgrArbitrationPanic();
#endif
	value = 100000;
	runningValue = 110000;
	if (app->protectionMode == 0) {
		value = 0;
		runningValue = 10000;
	}
	if (appmgrGetAppRunState(app) == 2)
		value = runningValue;
	if (app->launchInfo.processAttr & 1)
		value += 1000;
	pid = app->process.processId;
	if (pid == sceKernelGetProcessId())
		value += 100;
	return app->baseMiniEvictionValue + value;
}


#if FW_DEBUG
#define GC_FAILURE(_value, _line, _func) do { \
	sceAppMgrDump(); \
	dumpBgmState(); \
	SCE_DBG_ASSERT_PRINTF_FUNC(0, _line, _func, \
		"[SceAppMgr] The actual value is %d(0x%08x), but expected value is %d(0x%08x)\n", \
		_value, _value, 0, 0); \
} while (0)

#define GC_CHECK_APP_OWNERS(_line, _func) do { \
	int ownerIndex; \
	for (ownerIndex = 0; ownerIndex != 20; ++ownerIndex) { \
		__builtin_prefetch(&g_appmgrAppStates[ownerIndex + 5].mutexOwner); \
		SCE_DBG_ASSERT_CHECK_FUNC(g_appmgrAppStates[ownerIndex].mutexOwner != \
			sceKernelGetThreadId(), _line, _func); \
	} \
} while (0)

#define GC_LOCK(_mutex, _owner, _line, _func) do { \
	int lockResult = sceKernelLockMutex(_mutex, 1, NULL); \
	if (lockResult != 0) { \
		if ((unsigned int)(lockResult + 0x7ffd7fd4) > 1) \
			GC_FAILURE(lockResult, _line, _func); \
		GC_FAILURE(lockResult, (_line) + 1, _func); \
	} else { \
		_owner = sceKernelGetThreadId(); \
	} \
} while (0)

#define GC_MAIN_LOCK(_line, _func) do { \
	int lockResult = sceKernelLockMutex(g_appmgrMainMutex, 1, NULL); \
	if (lockResult != 0) { \
		SCE_DBG_PRINTF_FUNC(0, _line, _func, \
			"Failed to lock ret=0x%08x\n", lockResult); \
		sceAppMgrDump(); \
		dumpBgmState(); \
		SCE_DBG_ASSERT_CHECK_FUNC(0, _line, _func); \
	} else { \
		g_appmgrMainMutexOwner = sceKernelGetThreadId(); \
	} \
} while (0)

#define GC_UNLOCK_CHECK(_result, _line, _func) do { \
	if (_result != 0) \
		GC_FAILURE(_result, _line, _func); \
} while (0)
#define GC_PROCESS_CHECK(_result, _line, _func, _info) \
	GC_UNLOCK_CHECK(_result, _line, _func)
#define GC_SUSPEND_CHECK(_result, _line, _func) \
	GC_UNLOCK_CHECK(_result, _line, _func)
#define GC_APP_LOCK_CHECK(_line, _func) do { \
	SCE_DBG_ASSERT_CHECK_FUNC(g_appmgrMainMutexOwner != \
		sceKernelGetThreadId(), _line, _func); \
	if (g_appmgrCore2MutexOwner != sceKernelGetThreadId()) \
		GC_CHECK_APP_OWNERS(_line, _func); \
} while (0)
#define GC_APP_UNLOCK_CHECK(_line, _func) \
	SCE_DBG_ASSERT_CHECK_FUNC(g_appmgrMainMutexOwner != \
		sceKernelGetThreadId(), _line, _func)
#define GC_PREFETCH_APP(_app) __builtin_prefetch(&(_app)[1].mutex)
#else
#define GC_CHECK_APP_OWNERS(_line, _func) do { } while (0)
#define GC_LOCK(_mutex, _owner, _line, _func) do { \
	int lockResult = sceKernelLockMutex(_mutex, 1, NULL); \
	if (lockResult != 0) { \
		if ((unsigned int)(lockResult + 0x7ffd7fd4) > 1) \
			appmgrGcListenerMutexPanic(); \
		appmgrArbitrationPanic(); \
	} \
	_owner = sceKernelGetThreadId(); \
} while (0)
#define GC_MAIN_LOCK(_line, _func) do { \
	if (sceKernelLockMutex(g_appmgrMainMutex, 1, NULL) != 0) \
		appmgrGcListenerMutexPanic(); \
	g_appmgrMainMutexOwner = sceKernelGetThreadId(); \
} while (0)
#define GC_UNLOCK_CHECK(_result, _line, _func) do { \
	if (_result != 0) \
		appmgrGcListenerMutexPanic(); \
} while (0)
#define GC_PROCESS_CHECK(_result, _line, _func, _info) do { \
	if (_result != 0) \
		SceDebugForDriver_00CCE39C(&_info, __builtin_return_address(0), \
			"0x%08x,0x%08x\n", _result, 0); \
} while (0)
#define GC_SUSPEND_CHECK(_result, _line, _func) do { \
	if (_result != 0) \
		appmgrArbitrationPanic(); \
} while (0)
#define GC_APP_LOCK_CHECK(_line, _func) \
	((void)sceKernelGetThreadId())
#define GC_APP_UNLOCK_CHECK(_line, _func) do { } while (0)
#define GC_PREFETCH_APP(_app) __builtin_prefetch(&(_app)[2].mutex)
#endif

#if FW_DEBUG
#define MINI_APP_PREFETCH	4
#else
#define MINI_APP_PREFETCH	3
#endif

/*
 * Locks the suspended mini application worth least (at most maxValue), or the
 * one already being killed; 0x80803002 when an application that may not be
 * killed is being killed.
 */
int findAndLockLeastValuableMiniApp2(struct AppState **out, int maxValue)
{
	struct AppState *current, *app;
	SceUID owner, pid;
	int leastValue = -1, index = -1, killing = 0;
	int i, status, value, result;

	owner = g_appmgrMainMutexOwner;
	DELETE_APP_ASSERT(owner != sceKernelGetThreadId(), 261,
		"findAndLockLeastValuableMiniApp2");
retry:
	DELETE_APP_MAIN_LOCK(267, "findAndLockLeastValuableMiniApp2");
	current = appmgrFindAppByName(g_appmgrAppStates, NULL);
	for (i = appmgrGetShellApp() != NULL; i <= 19; i++) {
		app = &g_appmgrAppStates[i];
		if (app == current)
			continue;
		__builtin_prefetch(&g_appmgrAppStates[i + MINI_APP_PREFETCH].process.processId);
		if (app->process.processId < 0 || !(app->process.processId & 1) ||
		    app->launchInfo.budgetSize == 0)
			continue;
		status = appmgrGetAppRunStatus(app);
		if ((status == 32 || status == 16 || status == 48) && !killing) {
			value = getMiniAppValue(app);
			if (value <= maxValue && (index == -1 || value < leastValue)) {
				leastValue = value;
				index = i;
			}
			continue;
		}
		if (status == 128) {
			if (!(app->launchInfo.processAttr & 2)) {
				DELETE_APP_MAIN_UNLOCK(306, "findAndLockLeastValuableMiniApp2");
				return 0x80803002;
			}
			index = i;
			killing = 1;
		}
	}
	pid = index == -1 ? -1 : g_appmgrAppStates[index].process.processId;
	DELETE_APP_MAIN_UNLOCK(316, "findAndLockLeastValuableMiniApp2");
	if (index == -1)
		return 0x80802013;
	app = &g_appmgrAppStates[index];
#if FW_DEBUG
	SCE_DBG_ASSERT_CHECK_FUNC(({ owner = g_appmgrMainMutexOwner;
		owner != sceKernelGetThreadId(); }), 325, "findAndLockLeastValuableMiniApp2");
	owner = g_appmgrCore2MutexOwner;
	if (owner != sceKernelGetThreadId()) {
		for (i = 0; i != 20; i++) {
			__builtin_prefetch(&g_appmgrAppStates[i + 5].mutexOwner);
			SCE_DBG_ASSERT_CHECK_FUNC(({ owner = g_appmgrAppStates[i].mutexOwner;
				owner != sceKernelGetThreadId(); }), 325,
				"findAndLockLeastValuableMiniApp2");
		}
	}
#else
	sceKernelGetThreadId();
#endif
	result = sceKernelLockMutex(app->mutex, 1, NULL);
	if (result != 0) {
#if FW_DEBUG
		if ((unsigned int)(result + 0x7ffd7fd4) > 1)
			GC_FAILURE(result, 325, "findAndLockLeastValuableMiniApp2");
		if (result < 0)
			return result;
#else
		if ((unsigned int)(result + 0x7ffd7fd4) > 1)
			appmgrGcListenerMutexPanic();
		return result;
#endif
	} else {
		app->mutexOwner = sceKernelGetThreadId();
	}
	if (app->process.processId == pid) {
		*out = app;
		return 0;
	}
	result = _appListUnlockApp(&g_appmgrAppStates[index]);
	DELETE_APP_CHECK(result, 0, 331, "findAndLockLeastValuableMiniApp2");
	goto retry;
}

#if FW_DEBUG
#define KILL_CHECK_APP_OWNERS(_line, _func) do { \
	int ownerIndex; \
	SceUID appOwner; \
	for (ownerIndex = 0; ownerIndex != 20; ++ownerIndex) \
		SCE_DBG_ASSERT_CHECK_FUNC(({ \
			__builtin_prefetch(&g_appmgrAppStates[ownerIndex + 5].mutexOwner); \
			appOwner = g_appmgrAppStates[ownerIndex].mutexOwner; \
			appOwner != sceKernelGetThreadId(); }), _line, _func); \
} while (0)
#define KILL_KM_LOCK(_result, _line, _func) do { \
	_result = sceKernelLockMutex(g_appmgrCoreKmMutex, 1, NULL); \
	if (_result != 0) { \
		if ((unsigned int)(_result + 0x7ffd7fd4) > 1) \
			GC_FAILURE(_result, _line, _func); \
		return _result; \
	} \
	g_appmgrCoreKmMutexOwner = sceKernelGetThreadId(); \
} while (0)
#else
#define KILL_CHECK_APP_OWNERS(_line, _func) do { } while (0)
#define KILL_KM_LOCK(_result, _line, _func) do { \
	_result = sceKernelLockMutex(g_appmgrCoreKmMutex, 1, NULL); \
	if (_result != 0) { \
		if ((unsigned int)(_result + 0x7ffd7fd4) > 1) \
			appmgrGcListenerMutexPanic(); \
		return _result; \
	} \
	g_appmgrCoreKmMutexOwner = sceKernelGetThreadId(); \
} while (0)
#endif
#define KILL_KM_UNLOCK(_line, _func) do { \
	int unlockResult; \
	g_appmgrCoreKmMutexOwner = -1; \
	unlockResult = sceKernelUnlockMutex(g_appmgrCoreKmMutex, 1); \
	DELETE_APP_KM_UNLOCK_CHECK(unlockResult, _line, _func); \
} while (0)
#if FW_DEBUG
#define DELETE_APP_KM_UNLOCK_CHECK(_result, _line, _func) \
	DELETE_APP_CHECK(_result, 0, _line, _func)
#else
#define DELETE_APP_KM_UNLOCK_CHECK(_result, _line, _func) do { \
	if ((_result) != 0) \
		appmgrGcListenerMutexPanic(); \
} while (0)
#endif

/*
 * Kills suspended mini applications worth at most maxValue, least valuable first, until one
 * is gone (waiting up to 15 s for it each time).
 */
int killLeastValuableMiniApplication2(int maxValue)
{
	struct AppState *app;
	struct AppmgrWaitObject *waitObject;
	SceUInt32 timeout;
	SceUID owner;
	int result, quitResult;

	owner = g_appmgrMainMutexOwner;
	DELETE_APP_ASSERT(owner != sceKernelGetThreadId(), 391, "killLeastValuableMiniApplication2");
	KILL_CHECK_APP_OWNERS(392, "killLeastValuableMiniApplication2");
	KILL_KM_LOCK(result, 392, "killLeastValuableMiniApplication2");
	for (;;) {
		app = NULL;
		result = findAndLockLeastValuableMiniApp2(&app, maxValue);
		if (result != 0x80803002) {
			if (result != 0) {
				KILL_KM_UNLOCK(407, "killLeastValuableMiniApplication2");
				return result;
			}
			quitResult = _appmgrProcessQuitProcess(app, 3, 0, 0);
			result = _appListUnlockApp(app);
			DELETE_APP_CHECK(result, 0, 414, "killLeastValuableMiniApplication2");
			if (quitResult != 0)
				continue;
		}
		waitObject = appmgrUtilGetWaitObject(1);
		timeout = 15000000;
		result = appmgrUtilWaitObjectWait(waitObject, &timeout);
		if (result == 0 || result == (int)0x8002802d) {
			KILL_KM_UNLOCK(428, "killLeastValuableMiniApplication2");
			return result;
		}
	}
}

/* Kills every suspended mini application; 0x8002802d when one does not go within 15 s. */
int killAllMiniApplication(void)
{
	struct AppState *app;
	struct AppmgrWaitObject *waitObject;
	SceUInt32 timeout;
	SceUID owner;
	int result, quitResult;

	owner = g_appmgrMainMutexOwner;
	DELETE_APP_ASSERT(owner != sceKernelGetThreadId(), 436, "killAllMiniApplication");
	KILL_CHECK_APP_OWNERS(437, "killAllMiniApplication");
	KILL_KM_LOCK(result, 437, "killAllMiniApplication");
	for (;;) {
		app = NULL;
		result = findAndLockLeastValuableMiniApp2(&app, 999999);
		if (result != 0x80803002) {
			if (result != 0)
				break;
			quitResult = _appmgrProcessQuitProcess(app, 3, 0, 0);
			result = _appListUnlockApp(app);
			DELETE_APP_CHECK(result, 0, 464, "killAllMiniApplication");
			if (quitResult != 0)
				continue;
		}
		waitObject = appmgrUtilGetWaitObject(1);
		timeout = 15000000;
		if (appmgrUtilWaitObjectWait(waitObject, &timeout) == (int)0x8002802d) {
			KILL_KM_UNLOCK(479, "killAllMiniApplication");
			return 0x8002802d;
		}
	}
	KILL_KM_UNLOCK(453, "killAllMiniApplication");
	if (result == (int)0x80802013)
		return 0;
	return result;
}

/*
 * Quits the application of that name, if one runs, and waits for it to unload; 0 when there
 * is none or it is gone.
 */
static inline int killAndWaitByNameIfExist(const char *name)
{
	struct AppmgrWaitObject *waitObject;
	struct AppState *app;
	SceUID owner;
	int result;

	owner = g_appmgrCore2MutexOwner;
	DELETE_APP_ASSERT(owner == sceKernelGetThreadId(), 1249, "killAndWaitByNameIfExist");
	waitObject = appmgrUtilGetWaitObject(6);
	appmgrUtilWaitObjectClear(waitObject);
	if (_appListFindAndLockAppByName2(name, &app, -1) != 0)
		return 0;
	DELETE_APP_MAIN_LOCK(1261, "killAndWaitByNameIfExist");
	app->process.unloadWaiter = waitObject;
	DELETE_APP_MAIN_UNLOCK(1263, "killAndWaitByNameIfExist");
	result = _appmgrProcessQuitProcess(app, 1, 0, 0);
	if (result != 0) {
		DELETE_APP_MAIN_LOCK(1269, "killAndWaitByNameIfExist");
		app->process.unloadWaiter = NULL;
		DELETE_APP_MAIN_UNLOCK(1271, "killAndWaitByNameIfExist");
		_appListUnlockApp(app);
		return result;
	}
	_appListUnlockApp(app);
	return appmgrUtilWaitObjectWait(waitObject, NULL);
}

#if FW_DEBUG
#define LAUNCH_COPY_OUT(_user, _kernel, _size, _line, _info) do { \
	if ((_user) != NULL && sceKernelCopyToUser(_user, _kernel, _size) < 0) { \
		sceAppMgrDump(); \
		dumpBgmState(); \
		SCE_DBG_ASSERT_CHECK_FUNC(0, _line, "sceAppMgrLaunchAppByName2ndStage_internal"); \
	} \
} while (0)
#else
#define LAUNCH_COPY_OUT(_user, _kernel, _size, _line, _info) do { \
	if ((_user) != NULL && sceKernelCopyToUser(_user, _kernel, _size) < 0) \
		SceDebugForDriver_391B5B74(&_info, __builtin_return_address(0)); \
} while (0)
#endif

/*
 * Launches into the application slot appId: an application of the same name is quit first
 * when the slot asks for it, the slot takes the request, and _launchApp4 starts it.
 */
int sceAppMgrLaunchAppByName2ndStage_internal(int appId, const char *name,
		SceAppMgrAppStatus *statusOut, const AppmgrLaunchRequest *request,
		int sendParamToRunning, int startByUpdater, const AppmgrCreateProcessParam *createParam,
		const struct SceKernelProcessOpt *opt, const AppmgrLaunchOptions *options,
		const AppmgrLaunchAppOption *option, int *error)
{
	struct AppState *app, *slot;
	AppmgrLaunchInfo *launchInfo;
	AppmgrLaunchApp4Param param;
	SceAppMgrAppStatus status;
	unsigned int paramResult[4];
	char appName[32];
	SceUID owner, creatorPid;
	int newAppId, killByName, launchResult, result;

	owner = g_appmgrCore2MutexOwner;
	DELETE_APP_ASSERT(owner == sceKernelGetThreadId(), 1295,
		"sceAppMgrLaunchAppByName2ndStage_internal");
	DELETE_APP_MAIN_LOCK(1301, "sceAppMgrLaunchAppByName2ndStage_internal");
	slot = appmgrFindAppById(appId);
	if (slot == NULL) {
		DELETE_APP_MAIN_UNLOCK(1324, "sceAppMgrLaunchAppByName2ndStage_internal");
	} else {
		if (name != NULL && name[0] != '\0')
			strncpy(slot->process.appName, name, sizeof(slot->process.appName));
		killByName = slot->launchMode & 0x400000;
		__builtin_memcpy(appName, slot->process.appName, sizeof(appName));
		DELETE_APP_MAIN_UNLOCK(1314, "sceAppMgrLaunchAppByName2ndStage_internal");
		if (killByName) {
			result = killAndWaitByNameIfExist(appName);
			if (result != 0)
				return result;
		}
	}

	result = _appListFindAndLockAppByAppId(appId, &app);
	if (result != 0)
		return result;
	DELETE_APP_MAIN_LOCK(1335, "sceAppMgrLaunchAppByName2ndStage_internal");
	if (appmgrGetAppRunStatus(app) != 0x300 && appmgrGetAppRunStatus(app) != 0x600) {
		DELETE_APP_MAIN_UNLOCK(1338, "sceAppMgrLaunchAppByName2ndStage_internal");
		result = _appListUnlockApp(app);
		DELETE_APP_CHECK(result, 0, 1340, "sceAppMgrLaunchAppByName2ndStage_internal");
		return 0x8080201a;
	}
	if (app->process.appName[0] == '\0') {
		DELETE_APP_MAIN_UNLOCK(1347, "sceAppMgrLaunchAppByName2ndStage_internal");
		result = _appListUnlockApp(app);
		DELETE_APP_CHECK(result, 0, 1349, "sceAppMgrLaunchAppByName2ndStage_internal");
		return 0x8080201a;
	}
	launchInfo = &app->launchInfo;
	if (request->appPath != NULL)
		strncpy(launchInfo->appPath, request->appPath, sizeof(launchInfo->appPath));
	else
		launchInfo->appPath[0] = '\0';
	__builtin_memcpy(launchInfo->proxyAppName, request->proxyAppName,
		sizeof(launchInfo->proxyAppName));
	launchInfo->budgetSize = request->budgetSize;
	launchInfo->pycontSize = request->pycontSize;
	launchInfo->processCreateFlags = request->processCreateFlags;
	launchInfo->budgetType = request->budgetType;
	launchInfo->processAttr = request->processAttr;
	launchInfo->defaultInfobar = request->defaultInfobar;
	if (request->launchArgs != NULL)
		memcpy(launchInfo->launchArgs, request->launchArgs, request->launchArgsSize);
	else
		memset(launchInfo->launchArgs, 0, sizeof(launchInfo->launchArgs));
	launchInfo->launchArgsSize = request->launchArgsSize;
	launchInfo->launchArgumentMarker = request->launchArgumentMarker;
	launchInfo->launchOrigin = request->launchOrigin;
	launchInfo->useCommonDialog = request->useCommonDialog;
	launchInfo->notifyBootComplete = request->notifyBootComplete;
	launchInfo->enlargeGameMemorySize = request->enlargeGameMemorySize;
	app->appInfobar = app->launchInfo.defaultInfobar;
	newAppId = app->appId | 0x1000;
	app->appId = newAppId;
	_deviceAddApp(newAppId);
	DELETE_APP_MAIN_UNLOCK(1360, "sceAppMgrLaunchAppByName2ndStage_internal");
	_appListUnlockApp(app);

	if (request->budgetType == 1 && request->enlargeGameMemorySize != 0) {
		killAllMiniApplication();
		SCE_DBG_PRINTF_FUNC(0, 1369, "sceAppMgrLaunchAppByName2ndStage_internal",
			"Failed to kill all mini-app.\n");
	}
	memset(&status, 0, sizeof(status));
	__builtin_memset(paramResult, 0, sizeof(paramResult));
	param.appId = newAppId;
	param.opt = opt;
	param.options = options;
	param.sendParamToRunning = sendParamToRunning;
	param.startByUpdater = startByUpdater;
	param.createParam = createParam;
	param.status = &status;
	param.paramResult = paramResult;
	param.error = error;
	launchResult = SceThreadmgrForDriver_E54FD746(0x4000, _launchApp4, &param);
	if (launchResult < 0) {
		creatorPid = -1;
		DELETE_APP_MAIN_LOCK(1404, "sceAppMgrLaunchAppByName2ndStage_internal");
		app = appmgrFindAppById(newAppId);
		if (app != NULL &&
		    (app->process.processId < 0 || !(app->process.processId & 1))) {
			_deviceDeleteApp(newAppId);
			appmgrClearFailedAppLaunch(app, launchResult, &creatorPid);
		}
		DELETE_APP_MAIN_UNLOCK(1412, "sceAppMgrLaunchAppByName2ndStage_internal");
		if (creatorPid >= 0)
			sendResumeToPid(creatorPid);
		return launchResult;
	}
	if (status.paramDeliveryHandled) {
		DELETE_APP_MAIN_LOCK(1417, "sceAppMgrLaunchAppByName2ndStage_internal");
		_deviceDeleteApp(newAppId);
		DELETE_APP_MAIN_UNLOCK(1419, "sceAppMgrLaunchAppByName2ndStage_internal");
		creatorPid = -1;
		slot = appmgrFindAppById(newAppId);
		if (slot != NULL)
			result = appmgrClearFailedAppLaunch(slot, launchResult, &creatorPid);
		else
			result = 0x80802012;
		DELETE_APP_CHECK(result, 0, 1424, "sceAppMgrLaunchAppByName2ndStage_internal");
		if (creatorPid >= 0)
			sendResumeToPid(creatorPid);
	}
	if (option != NULL) {
		LAUNCH_COPY_OUT(option->status, &status, sizeof(status), 1434, launchStatusCopyPanicInfo);
		LAUNCH_COPY_OUT(option->paramResult, paramResult, sizeof(paramResult), 1439,
			launchParamResultCopyPanicInfo);
	}
	if (statusOut != NULL)
		memcpy(statusOut, &status, sizeof(status));
	return launchResult;
}

/*
 * Locks the suspended mini application worth least, or the one already being killed;
 * 0x80803002 when an application that may not be killed is being killed.
 */
static inline int findAndLockLeastValuableMiniApp(struct AppState **out)
{
	struct AppState *current, *app;
	SceUID owner, pid;
	int leastValue = -1, index = -1, killing = 0;
	int i, status, value, result;

	owner = g_appmgrMainMutexOwner;
	DELETE_APP_ASSERT(owner != sceKernelGetThreadId(), 177, "findAndLockLeastValuableMiniApp");
retry:
	DELETE_APP_MAIN_LOCK(183, "findAndLockLeastValuableMiniApp");
	current = appmgrFindAppByName(g_appmgrAppStates, NULL);
	for (i = appmgrGetShellApp() != NULL; i <= 19; i++) {
		app = &g_appmgrAppStates[i];
		if (app == current)
			continue;
		__builtin_prefetch(&g_appmgrAppStates[i + MINI_APP_PREFETCH].process.processId);
		if (app->process.processId < 0 || !(app->process.processId & 1) ||
		    app->launchInfo.budgetSize == 0)
			continue;
		status = appmgrGetAppRunStatus(app);
		if ((status == 32 || status == 16 || status == 48) && !killing) {
			value = getMiniAppValue(app);
			if (index == -1 || value < leastValue) {
				leastValue = value;
				index = i;
			}
			continue;
		}
		if (status == 128) {
			if (!(app->launchInfo.processAttr & 2)) {
				DELETE_APP_MAIN_UNLOCK(219, "findAndLockLeastValuableMiniApp");
				return 0x80803002;
			}
			index = i;
			killing = 1;
		}
	}
	pid = index == -1 ? -1 : g_appmgrAppStates[index].process.processId;
	DELETE_APP_MAIN_UNLOCK(229, "findAndLockLeastValuableMiniApp");
	if (index == -1)
		return 0x80802013;
	app = &g_appmgrAppStates[index];
#if FW_DEBUG
	SCE_DBG_ASSERT_CHECK_FUNC(({ owner = g_appmgrMainMutexOwner;
		owner != sceKernelGetThreadId(); }), 237, "findAndLockLeastValuableMiniApp");
	owner = g_appmgrCore2MutexOwner;
	if (owner != sceKernelGetThreadId())
		KILL_CHECK_APP_OWNERS(237, "findAndLockLeastValuableMiniApp");
#else
	sceKernelGetThreadId();
#endif
	result = sceKernelLockMutex(app->mutex, 1, NULL);
	if (result != 0) {
#if FW_DEBUG
		if ((unsigned int)(result + 0x7ffd7fd4) > 1)
			GC_FAILURE(result, 237, "findAndLockLeastValuableMiniApp");
		if (result < 0)
			return result;
#else
		if ((unsigned int)(result + 0x7ffd7fd4) > 1)
			appmgrGcListenerMutexPanic();
		return result;
#endif
	} else {
		app->mutexOwner = sceKernelGetThreadId();
	}
	if (app->process.processId == pid) {
		*out = app;
		return 0;
	}
	result = _appListUnlockApp(&g_appmgrAppStates[index]);
	DELETE_APP_CHECK(result, 0, 243, "findAndLockLeastValuableMiniApp");
	goto retry;
}

/* Kills the least valuable suspended mini application (waiting up to 15 s for it to go). */
int killLeastValuableMiniApplication(void)
{
	struct AppState *app;
	struct AppmgrWaitObject *waitObject;
	SceUInt32 timeout;
	SceUID owner;
	int result, quitResult;

	owner = g_appmgrMainMutexOwner;
	DELETE_APP_ASSERT(owner != sceKernelGetThreadId(), 344, "killLeastValuableMiniApplication");
	KILL_CHECK_APP_OWNERS(346, "killLeastValuableMiniApplication");
	KILL_KM_LOCK(result, 346, "killLeastValuableMiniApplication");
	for (;;) {
		result = findAndLockLeastValuableMiniApp(&app);
		if (result != 0x80803002) {
			if (result != 0) {
				KILL_KM_UNLOCK(361, "killLeastValuableMiniApplication");
				return result;
			}
			quitResult = _appmgrProcessQuitProcess(app, 3, 0, 0);
			result = _appListUnlockApp(app);
			DELETE_APP_CHECK(result, 0, 368, "killLeastValuableMiniApplication");
			if (quitResult != 0)
				continue;
		}
		waitObject = appmgrUtilGetWaitObject(1);
		timeout = 15000000;
		result = appmgrUtilWaitObjectWait(waitObject, &timeout);
		if (result == 0 || result == (int)0x8002802d) {
			KILL_KM_UNLOCK(382, "killLeastValuableMiniApplication");
			return result;
		}
	}
}

/*
 * Reserves an application slot for a launch (killing the least valuable mini application
 * while none is free) and fills in who launches it and how; the slot is left unlocked.
 */
int _sceAppMgrAppStateReserve(struct AppState **out, const char *name, const char *param,
		SceUID paramCallbackId, SceUInt32 launchMode, SceUID launchCallbackId,
		int creatorAppId, SceUInt32 evictionThresholdClass,
		SceUInt32 resumeCreatorOnLaunchFailure)
{
	struct AppState *creator, *app;
	const char *creatorName;
	SceUID owner, creatorPid;
	int creatorId, result;

	owner = g_appmgrMainMutexOwner;
	DELETE_APP_ASSERT(owner != sceKernelGetThreadId(), 825, "_sceAppMgrAppStateReserve");
	owner = g_appmgrCoreKmMutexOwner;
	DELETE_APP_ASSERT(owner != sceKernelGetThreadId(), 826, "_sceAppMgrAppStateReserve");
	DELETE_APP_MAIN_LOCK(828, "_sceAppMgrAppStateReserve");
	creator = appmgrFindAppById(creatorAppId);
	if (creator == NULL)
		creator = appmgrFindAppByProcessId(g_appmgrAppStates, sceKernelGetProcessId());
	if (creator != NULL) {
		creatorPid = creator->process.processId;
		creatorId = creator->appId;
	} else {
		creatorPid = -1;
		creatorId = -1;
	}
	DELETE_APP_MAIN_UNLOCK(838, "_sceAppMgrAppStateReserve");
	while ((result = _appListReserveAndLockApp(&app)) != 0) {
		if (result != (int)0x80802012)
			return result;
		result = killLeastValuableMiniApplication();
		if (result < 0)
			return result;
	}
	DELETE_APP_MAIN_LOCK(849, "_sceAppMgrAppStateReserve");
	DELETE_APP_CHECK(app->state, 0x101, 851, "_sceAppMgrAppStateReserve");
	if (name != NULL)
		strncpy(app->process.appName, name, sizeof(app->process.appName));
	else
		app->process.appName[0] = '\0';
	creatorName = creator != NULL ? creator->process.appName : NULL;
	result = appmgrParamQueuePush(&app->params, param, paramCallbackId,
		sceKernelGetProcessId(), creatorName);
	DELETE_APP_CHECK(result, 0, 865, "_sceAppMgrAppStateReserve");
	app->launchSourceProcessId = creatorPid;
	app->launchSourceAppId = creatorId;
	_appListSetRunStatus(app, 0x300);
	_appListSetActiveness(app, 1);
	app->creatorPid = sceKernelGetProcessId();
	app->launchCallbackId = launchCallbackId;
	app->process.processId = app->appId;
	app->process.notifiedProcessState = 0;
	app->launchMode = launchMode;
	app->process.requestedProcessState = 0;
	app->unusedLaunchMarker = 255;
	app->protectionMode = 0;
	app->evictionThresholdClass = evictionThresholdClass;
	app->resumeCreatorOnLaunchFailure = resumeCreatorOnLaunchFailure;
	DELETE_APP_MAIN_UNLOCK(881, "_sceAppMgrAppStateReserve");
	_appListUnlockApp(app);
	*out = app;
	return 0;
}

/*
 * Reserves a slot for an application launched by name and keeps the caller's extra
 * parameters (at most 6 KiB) for it; the launch itself is left to the shell, which
 * the queued event tells. Returns the new application's ID.
 */
int sceAppMgrLaunchAppByNameInternal(SceUInt32 launchMode, const SceChar8 *name,
		const char *param, SceUID launchCallbackId, SceUID paramCallbackId,
		int creatorAppId, SceUInt32 evictionThresholdClass, const void *extraParam,
		SceSize extraParamSize, SceUInt32 resumeCreatorOnLaunchFailure)
{
	struct AppState *app;
	SceUID owner, creatorPid;
	int hasExtraParam, result;

	owner = g_appmgrMainMutexOwner;
	DELETE_APP_ASSERT(owner != sceKernelGetThreadId(), 1847, "sceAppMgrLaunchAppByNameInternal");
	if (!(launchMode & 0x100000)) {
		if (name == NULL)
			return 0x80802016;
		if (name[0] == '\0')
			return 0x8080201a;
	}
	if (evictionThresholdClass > 4)
		return 0x8080201a;
	hasExtraParam = extraParam != NULL;
	if (extraParamSize > sizeof(g_appmgrExtraAppParam.arguments) && hasExtraParam)
		return 0x8080201a;
	app = NULL;
	result = _sceAppMgrAppStateReserve(&app, (const char *)name, param, paramCallbackId, launchMode,
		launchCallbackId, creatorAppId, evictionThresholdClass,
		resumeCreatorOnLaunchFailure);
	if (result != 0)
		return result;
	DELETE_APP_MAIN_LOCK(1884, "sceAppMgrLaunchAppByNameInternal");
	if (extraParamSize != 0 && hasExtraParam) {
		app->extraAppParamAllowed = 1;
		memset(&g_appmgrExtraAppParam, 0, sizeof(g_appmgrExtraAppParam));
		result = sceKernelCopyFromUser(&g_appmgrExtraAppParam, extraParam, extraParamSize);
		if (result != 0) {
			creatorPid = -1;
			appmgrClearFailedAppLaunch(app, result, &creatorPid);
			DELETE_APP_MAIN_UNLOCK(1895, "sceAppMgrLaunchAppByNameInternal");
			if (creatorPid >= 0)
				sendResumeToPid(creatorPid);
			return result;
		}
		g_appmgrExtraAppParam.argumentSize = extraParamSize;
		if (appmgrFindAppByProcessId(g_appmgrAppStates, sceKernelGetProcessId()) != NULL)
			__builtin_memcpy(g_appmgrExtraAppParam.appName, app->process.appName,
				sizeof(g_appmgrExtraAppParam.appName));
	}
	_shellEventOnAppLaunchByNameQueuePush(app->appId, app->appId);
	result = app->appId;
	DELETE_APP_MAIN_UNLOCK(1910, "sceAppMgrLaunchAppByNameInternal");
	return result;
}

/* The two Sony media helpers are inlined twice into the listener. */
#define GC_SUSPEND_MEDIA(_func, _lockLine, _mainLine, _unlockLine, \
		_suspendLine, _appUnlockLine, _waitLine, _checkLine, \
		_includeUx, _game, _markRemoval, _waitInfo, _checkInfo) do { \
	int index = appmgrGetShellApp() != NULL; \
	struct AppState *app = &g_appmgrAppStates[index]; \
	int result; \
	do { \
		SceUID pid; \
		SceUInt32 attributes; \
		int fromMedia; \
		GC_APP_LOCK_CHECK(_lockLine, _func); \
		GC_PREFETCH_APP(app); \
		GC_LOCK(app->mutex, app->mutexOwner, _lockLine, _func); \
		GC_MAIN_LOCK(_mainLine, _func); \
		__builtin_prefetch(&app[1].process.processId); \
		pid = app->process.processId; \
		attributes = app->launchInfo.processAttr; \
		fromMedia = strncmp(app->process.content.devicePrefix, "gro0:", 5) == 0 || \
			strncmp(app->process.content.devicePrefix, "grw0:", 5) == 0 || \
			((_includeUx) && strncmp(app->process.content.devicePrefix, "ux0:", 4) == 0); \
		g_appmgrMainMutexOwner = -1; \
		result = sceKernelUnlockMutex(g_appmgrMainMutex, 1); \
		GC_UNLOCK_CHECK(result, _unlockLine, _func); \
		if (fromMedia && pid >= 0 && (pid & 1) != 0 && \
			(attributes & 1) == (_game)) { \
			if (_markRemoval) { \
				__builtin_prefetch(&app[1].gameCardRemovalQuitPending); \
				app->gameCardRemovalQuitPending = 1; \
			} \
			result = _appmgrProcessSuspendProcess(app, 0, NULL); \
			GC_SUSPEND_CHECK(result, _suspendLine, _func); \
		} \
		GC_APP_UNLOCK_CHECK(_appUnlockLine, _func); \
		app->mutexOwner = -1; \
		result = sceKernelUnlockMutex(app->mutex, 1); \
		GC_UNLOCK_CHECK(result, _appUnlockLine, _func); \
		++index; \
		++app; \
	} while (index <= 19); \
	result = SceProcessmgrForKernel_8321B827(NULL); \
	GC_PROCESS_CHECK(result, _waitLine, _func, _waitInfo); \
	result = SceProcessmgrForKernel_D5DF895D(); \
	GC_PROCESS_CHECK(result, _checkLine, _func, _checkInfo); \
} while (0)

int _gcListenerThreadEntry(SceSize args, void *argp)
{
	SceKernelResultEvent results[3];
	SceProductMode productMode;
	int count;
	int gameCardChanged;
	int removableChanged;
	int result;

	(void)args;
	(void)argp;
	for (;;) {
		sceKernelClearEvent(g_appmgrGro0MountEvent, 0);
		sceKernelClearEvent(g_appmgrGrw0MountEvent, 0);
		g_appmgrGcWaitEvents[0].eventId = g_appmgrGro0MountEvent;
		g_appmgrGcWaitEvents[1].eventId = g_appmgrGrw0MountEvent;
		g_appmgrGcWaitEvents[2].eventId = g_appmgrUx0MountEvent;
		g_appmgrGcWaitEvents[0].eventPattern = -1;
		g_appmgrGcWaitEvents[1].eventPattern = -1;
		g_appmgrGcWaitEvents[2].eventPattern = -1;
		memset(results, 0, sizeof(results));
		count = SceThreadmgrForDriver_36F8E58A(g_appmgrGcWaitEvents, 3,
			1, results, NULL);
		GC_CHECK_APP_OWNERS(2769, "_gcListenerThreadEntry");
		GC_LOCK(g_appmgrCore2Mutex, g_appmgrCore2MutexOwner, 2769,
			"_gcListenerThreadEntry");
		if (count > 0) {
			gameCardChanged = results[0].eventId == g_appmgrGro0MountEvent ||
				results[0].eventId == g_appmgrGrw0MountEvent;
			removableChanged = results[0].eventId == g_appmgrUx0MountEvent;
			if (count != 1) {
				if (results[1].eventId == g_appmgrGro0MountEvent ||
					results[1].eventId == g_appmgrGrw0MountEvent)
					gameCardChanged = 1;
				if (results[1].eventId == g_appmgrUx0MountEvent)
					removableChanged = 1;
				if (count != 2) {
					if (results[2].eventId == g_appmgrGro0MountEvent ||
						results[2].eventId == g_appmgrGrw0MountEvent)
						gameCardChanged = 1;
					if (results[2].eventId == g_appmgrUx0MountEvent)
						removableChanged = 1;
				}
			}
			if (gameCardChanged) {
				GC_SUSPEND_MEDIA("suspendAppBootedfromGC", 2530, 2533,
					2537, 2543, 2545, 2552, 2554, 0, 0, 1,
					gcWaitProcessEventsPanicInfo, gcCheckProcessEventsPanicInfo);
				GC_SUSPEND_MEDIA("suspendAppBootedfromGC", 2559, 2562,
					2566, 2569, 2571, 2576, 2578, 0, 1, 0,
					gcGameWaitProcessEventsPanicInfo, gcGameCheckProcessEventsPanicInfo);
			}
			if (removableChanged) {
				sceSblPmMgrGetProductMode(&productMode);
				if (productMode != 1) {
					GC_SUSPEND_MEDIA("suspendAppBootedfromGCorRM", 2589, 2592,
						2596, 2599, 2601, 2607, 2609, 1, 0, 0,
						removableWaitProcessEventsPanicInfo, removableCheckProcessEventsPanicInfo);
					GC_SUSPEND_MEDIA("suspendAppBootedfromGCorRM", 2613, 2616,
						2620, 2623, 2625, 2630, 2632, 1, 1, 0,
						removableGameWaitProcessEventsPanicInfo, removableGameCheckProcessEventsPanicInfo);
				}
			}
		}
		g_appmgrCore2MutexOwner = -1;
		result = sceKernelUnlockMutex(g_appmgrCore2Mutex, 1);
		GC_UNLOCK_CHECK(result, 2791, "_gcListenerThreadEntry");
#if FW_DEBUG
		if (result != 0)
			GC_FAILURE(result, 2792, "_gcListenerThreadEntry");
#endif
	}
}

void initProcessStateUpdaterThread(void)
{
	int result;

	g_appmgrUpdateNotifySemaphore = sceKernelCreateSema("SceAppMgrUpdtNtfy",
		0, 0, 2000, NULL);
	if (g_appmgrUpdateNotifySemaphore <= 0) {
#if FW_DEBUG
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_CHECK_FUNC(0, 2811, "initProcessStateUpdaterThread");
#else
		appmgrArbitrationPanic();
#endif
	}
	dmb();
	g_appmgrProcessStateUpdaterThread = sceKernelCreateThread(
		"SceAppMgrProcessStateUpdater", _processUpdateThreadEntry,
		96, 8192, 0, 0x80000, NULL);
	if (g_appmgrProcessStateUpdaterThread <= 0) {
#if FW_DEBUG
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_CHECK_FUNC(0, 2822, "initProcessStateUpdaterThread");
#else
		appmgrArbitrationPanic();
#endif
	}
	result = sceKernelStartThread(g_appmgrProcessStateUpdaterThread, 0, NULL);
	if (result != 0) {
#if FW_DEBUG
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_CHECK_FUNC(0, 2825, "initProcessStateUpdaterThread");
#else
		SceDebugForDriver_391B5B74(&processStateUpdaterStartPanicInfo, __builtin_return_address(0));
#endif
	}
	g_appmgrGro0MountEvent = sceIoCreateMountEvent(0x900, 0x202);
	g_appmgrGrw0MountEvent = sceIoCreateMountEvent(0xa00, 0x202);
	g_appmgrUx0MountEvent = sceIoCreateMountEvent(0x800, 2);
	g_appmgrGcListenerThread = sceKernelCreateThread("SceAppMgrGcListener",
		_gcListenerThreadEntry, 96, 4096, 0, 0x80000, NULL);
	if (g_appmgrGcListenerThread <= 0) {
#if FW_DEBUG
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_CHECK_FUNC(0, 2843, "initProcessStateUpdaterThread");
#else
		appmgrArbitrationPanic();
#endif
	}
	result = sceKernelStartThread(g_appmgrGcListenerThread, 0, NULL);
	if (result != 0) {
#if FW_DEBUG
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_CHECK_FUNC(0, 2846, "initProcessStateUpdaterThread");
#else
		SceDebugForDriver_391B5B74(&gcListenerStartPanicInfo, __builtin_return_address(0));
#endif
	}
}

void appmgrProcessIdleCallback(SceUID pid, unsigned int elapsed, void *argument)
{
	struct AppState *app;

	(void)argument;
	app = appmgrFindSpecialApp(g_appmgrAppStates, pid);
	if (app == NULL || app->process.processId != pid)
		return;
	app->process.runWhenUiOverlay = elapsed == 0;
	dmb();
	_sceAppMgrNotifyAppMgrHandler();
}
