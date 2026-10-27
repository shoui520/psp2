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
#include "appmgr_core_internal.h"
#include "appmgr_shell_event.h"
#include "mount.h"
#include <sysmem/sblaimgr.h>
#include <acmgr/acmgr.h>
#include <intrmgr/syscall.h>
#include <kernel/error.h>
#include <sysmem/uid_guid.h>
#include <sysmem/qafmgr.h>
#include <stdlib.h>

extern SceUID g_appmgrMainMutexOwner;

struct AppState;
extern int _processUpdateThreadEntry(SceSize args, void *argp);
extern int _gcListenerThreadEntry(SceSize args, void *argp);
extern SceUID g_appmgrMainMutex;
extern SceUID g_appmgrCore2Mutex;
extern SceUID g_appmgrBottomSemaphore;
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

int g_appmgrActiveAppId = -1;
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

static const SceKernelDebugInfo processUpdateWaitPanicInfo = {
	.fileHash = 0xb8f4da66,
	.lineHash = 0x4589b95f,
	.funcHash = 0x03a58fab,
};

static const SceKernelDebugInfo activateSignalPanicInfo = {
	.fileHash = 0xb8f4da66,
	.lineHash = 0xe97d9e12,
	.funcHash = 0x606d92dd,
};

static const SceKernelDebugInfo spawnUpdaterSignalPanicInfo = {
	.fileHash = 0xb8f4da66,
	.lineHash = 0x6557fb22,
	.funcHash = 0x3840945d,
};

static const SceKernelDebugInfo spawnGameSignalPanicInfo = {
	.fileHash = 0xb8f4da66,
	.lineHash = 0x6a9427b8,
	.funcHash = 0x3840945d,
};

static const SceKernelDebugInfo spawnBackgroundSignalPanicInfo = {
	.fileHash = 0xb8f4da66,
	.lineHash = 0xb835d657,
	.funcHash = 0x3840945d,
};

static const SceKernelDebugInfo loadExecQuitWaitPanicInfo = {
	.fileHash = 0xb8f4da66,
	.lineHash = 0xe145442a,
	.funcHash = 0x46a5d58e,
};

static const SceKernelDebugInfo loadExecRelaunchWaitPanicInfo = {
	.fileHash = 0xb8f4da66,
	.lineHash = 0x6f07793a,
	.funcHash = 0x46a5d58e,
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

/* Copies a NULL-terminated user argv into one buffer of 1024 bytes; returns its size. */
#if FW_DEBUG
int copyArgv(char *destination, const char *const *userArgv)
#else
static inline __attribute__((always_inline)) int copyArgv(char *destination,
		const char *const *userArgv)
#endif
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
			DELETE_APP_ASSERT(total <= 1024, 925, "copyArgv");
			return total;
		}
		result = sceKernelStrncpyFromUser(cursor, argument, 1024 - total);
		if (result == 1024 - total)
			return (int)0x8001005bU;
		if (result < 0)
			return result;
		length = result + 1;
		DELETE_APP_ASSERT((SceUInt32)(1024 - total) >= (SceUInt32)length, 942, "copyArgv");
		total += length;
		cursor += length;
		++userArgv;
	}
}

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
		result = sceKernelDeleteBudget(budgetId);
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

#if FW_DEBUG
#define LAUNCH_CORE2_LOCK_FAILURE(_result, _line, _func) \
	GC_FAILURE(_result, _line, _func)
#else
#define LAUNCH_CORE2_LOCK_FAILURE(_result, _line, _func) appmgrGcListenerMutexPanic()
#endif
#define LAUNCH_CORE2_UNLOCK(_line, _func) do { \
	int unlockResult; \
	g_appmgrCore2MutexOwner = -1; \
	unlockResult = sceKernelUnlockMutex(g_appmgrCore2Mutex, 1); \
	DELETE_APP_KM_UNLOCK_CHECK(unlockResult, _line, _func); \
} while (0)

/* What a debugger's launch arguments ask for, completed from the SELF's eboot parameters. */
typedef struct LaunchAppArg {
	SceUInt32 budgetType;
	SceSize memSize;
	SceSize phcontMemSize;
	const char *titleId;
	int liveareaType;
	int background;
	int nonSuspendable;
	SceUInt32 launchMode;		/* 0x10000: no LiveArea, 0x40000: exits to exitTo */
	const char *exitTo;
	const char *param;
	int useCommonDialog;
	int budgetSpecified;
} LaunchAppArg;

/* A memory size option in MiB, kept when it is 1 to 128. */
static inline __attribute__((always_inline)) void setLaunchMemSize(SceSize *size,
		const char *value)
{
	long megabytes = strtol(value, NULL, 10);

	if ((unsigned long)(megabytes - 1) <= 127)
		*size = megabytes << 20;
}

/* Splits args into its strings (at most 31) and reads the options among them. */
static inline __attribute__((always_inline)) void initLaunchAppArg(LaunchAppArg *arg,
		const char *args, SceSize argSize)
{
	const char *argv[32];
	const char *p, *type;
	int argc, i;

	arg->budgetType = 1;
	arg->memSize = 0;
	arg->phcontMemSize = 0;
	arg->titleId = NULL;
	arg->liveareaType = 0;
	arg->background = 0;
	arg->nonSuspendable = 0;
	arg->launchMode = 0;
	arg->exitTo = NULL;
	arg->param = NULL;
	arg->useCommonDialog = 0;
	arg->budgetSpecified = 0;
	if (SceQafMgrForDriver_7B14DC45() == 0 || args == NULL)
		return;
	memset(argv, 0, sizeof(argv));
	p = args;
	for (argc = 0; argc < 31; argc++) {
		if (p - args >= (int)argSize)
			break;
		argv[argc] = p;
		while (*p != '\0' && ++p - args < (int)argSize)
			;
		p++;
	}
	for (i = 0; i < argc; ) {
		if (strncmp(argv[i], "-budget", 8) == 0 && i + 1 < argc) {
			type = argv[i + 1];
			if (strncmp(type, "big", 4) == 0 || strncmp(type, "game", 5) == 0) {
				arg->budgetType = 1;
				arg->memSize = 0;
			} else if (strncmp(type, "sys", 4) == 0 || strncmp(type, "shell", 6) == 0) {
				arg->budgetType = 2;
				arg->memSize = 0;
			} else if (strncmp(type, "small", 6) == 0 || strncmp(type, "mini", 5) == 0) {
				arg->budgetType = 0;
				arg->memSize = 0x2000000;
			} else {
				SCE_DBG_PRINTF_FUNC(0, 647, "initLaunchAppArg",
					"[SceAppMgr] Warning: unknown budgettype %s (ignored)\n", type);
			}
			i += 2;
			arg->budgetSpecified = 1;
		} else if (strncmp(argv[i], "-memsize", 9) == 0 && i + 1 < argc) {
			setLaunchMemSize(&arg->memSize, argv[i + 1]);
			i += 2;
		} else if (strncmp(argv[i], "-la_type", 9) == 0 && i + 1 < argc) {
			type = argv[i + 1];
			if (strncmp(type, "B", 2) == 0)
				arg->liveareaType = 1;
			else if (strncmp(type, "C", 2) == 0)
				arg->liveareaType = 2;
			i += 2;
		} else if (strncmp(argv[i], "-bg", 4) == 0) {
			arg->background = 1;
			i++;
		} else if (strncmp(argv[i], "-nonsuspendable", 16) == 0) {
			arg->nonSuspendable = 1;
			i++;
		} else if (strncmp(argv[i], "-titleid", 9) == 0 && i + 1 < argc) {
			arg->titleId = argv[i + 1];
			i += 2;
		} else if (strncmp(argv[i], "-phcontmemsize", 15) == 0 && i + 1 < argc) {
			setLaunchMemSize(&arg->phcontMemSize, argv[i + 1]);
			i += 2;
		} else if (strncmp(argv[i], "-livearea_off", 14) == 0) {
			arg->launchMode |= 0x10000;
			i++;
		} else if (strncmp(argv[i], "-exit_to", 6) == 0 && i + 1 < argc) {
			arg->launchMode |= 0x40000;
			arg->exitTo = argv[i + 1];
			i += 2;
		} else if (strncmp(argv[i], "-param", 6) == 0 && i + 1 < argc) {
			arg->param = argv[i + 1];
			i += 2;
		} else {
			if (strncmp(argv[i], "-usecdlg", 8) == 0)
				arg->useCommonDialog = 1;
			i++;
		}
	}
}

/* Takes the budget, background and common dialog use the arguments left open from the SELF. */
static inline __attribute__((always_inline)) void setLaunchAppArgFromEbootparam(
		LaunchAppArg *arg, const char *path)
{
	AppmgrEbootParam *ebootParam;
	int result;

	ebootParam = appmgrAllocMountHeapMemory(sizeof(*ebootParam));
	if (ebootParam == NULL) {
		SCE_DBG_PRINTF_FUNC(0, 731, "setLaunchAppArgFromEbootparam",
			"Failed mountMalloc\n");
		return;
	}
	result = sceAppMgrGetEbootParam(path, ebootParam);
	if (result < 0) {
		SCE_DBG_PRINTF_FUNC(0, 737, "setLaunchAppArgFromEbootparam",
			"sceAppMgrGetEbootParam = 0x%08x\n", result);
	} else {
		if (!arg->budgetSpecified) {
			if (ebootParam->memSize != 0) {
				arg->budgetType = 0;
				arg->memSize = ebootParam->memSize << 10;
				arg->phcontMemSize = ebootParam->phcontMemSize << 10;
			} else {
				arg->budgetType = 1;
				arg->phcontMemSize = 0;
				arg->memSize = 0;
			}
		}
		if (!arg->background)
			arg->background = ebootParam->attribute & 1;
		if (!arg->useCommonDialog)
			arg->useCommonDialog = (ebootParam->attribute & 4) != 0;
	}
	appmgrFreeMountHeapMemory(ebootParam);
}

/* The ID of the application the launched one exits to: -1 without one. */
static inline __attribute__((always_inline)) int initLaunchContext(const LaunchAppArg *arg)
{
	struct AppState *app;
	int creatorAppId;

	if (arg->exitTo == NULL)
		return -1;
	DELETE_APP_MAIN_LOCK(801, "initLaunchContext");
	app = appmgrFindAppByName(g_appmgrAppStates, arg->exitTo);
	creatorAppId = app != NULL ? app->appId : -1;
	DELETE_APP_MAIN_UNLOCK(808, "initLaunchContext");
	return creatorAppId;
}

/* Quits the shell, for a launch in the system budget; nonzero: the error that stops it. */
static inline __attribute__((always_inline)) int killShellApplication(void)
{
	struct AppState *shell;
	int result, unlockResult;

	DELETE_APP_MAIN_LOCK(512, "killShellApplication");
	shell = appmgrGetShellApp();
	DELETE_APP_MAIN_UNLOCK(514, "killShellApplication");
	if (shell == NULL)
		return 0;
	GC_APP_LOCK_CHECK(519, "killShellApplication");
	result = sceKernelLockMutex(shell->mutex, 1, NULL);
	if (result != 0) {
		if (result != SCE_KERNEL_ERROR_THREAD_STOPPED &&
				result != SCE_KERNEL_ERROR_THREAD_SUSPENDED)
			LAUNCH_CORE2_LOCK_FAILURE(result, 519, "killShellApplication");
		if (result < 0)
			return result;
	} else {
		shell->mutexOwner = sceKernelGetThreadId();
	}
	DELETE_APP_MAIN_LOCK(524, "killShellApplication");
	if (shell != appmgrGetShellApp()) {
		DELETE_APP_MAIN_UNLOCK(526, "killShellApplication");
		unlockResult = _appListUnlockApp(shell);
		DELETE_APP_CHECK(unlockResult, 0, 529, "killShellApplication");
		return 0;
	}
	DELETE_APP_MAIN_UNLOCK(533, "killShellApplication");
	result = _appmgrProcessQuitProcess(shell, 3, 0, 0);
	unlockResult = _appListUnlockApp(shell);
	DELETE_APP_CHECK(unlockResult, 0, 538, "killShellApplication");
	if (result != 0)
		return result;
	appmgrUtilWaitObjectWait(appmgrUtilGetWaitObject(4), NULL);
	return 0;
}

SceUID sceAppMgrLaunchAppForDebugger(const char *path, const char *args, SceSize argSize,
				     SceUInt32 processCreateFlags,
				     const struct SceKernelProcessOpt *opt,
				     const AppmgrLaunchOptions *options)
{
	AppmgrCreateProcessParam createParam;
	AppmgrLaunchRequest request, launchRequest;
	SceAppMgrAppStatus status;
	LaunchAppArg arg;
	struct AppState *app;
	struct AppmgrWaitObject *waitObject;
	SceUInt32 processAttr;
	int creatorAppId, appId, hasParam, error, result, unlockResult;

	createParam.useTitleIdAsName = 1;
	createParam.enableActivationEvents = 1;
	if (opt->flags & 0x0fd00fff)
		return SCE_APPMGR_ERROR_PARAM;
	if (SceProcessmgrForDriver_C715591F() == 1)
		return (int)0x80802025;
	initLaunchAppArg(&arg, args, argSize);
	hasParam = arg.param != NULL;
	if (!arg.budgetSpecified || !arg.background || !arg.useCommonDialog)
		setLaunchAppArgFromEbootparam(&arg, path);
	processAttr = (arg.nonSuspendable ? 2 : 0) | (arg.background ? 1 : 0);
	creatorAppId = initLaunchContext(&arg);
	if (!(processAttr & 1) &&
			waitSemaForDebuggerShellEventCount() == SCE_KERNEL_ERROR_WAIT_TIMEOUT)
		return (int)0x80802021;
	if (arg.titleId != NULL)
		createParam.useTitleIdAsName = 0;
	else
		arg.titleId = "NPXS10999";
	if (arg.nonSuspendable == 1)
		createParam.enableActivationEvents = 0;
	GC_CHECK_APP_OWNERS(1003, "sceAppMgrLaunchAppForDebugger");
	GC_LOCK(g_appmgrCore2Mutex, g_appmgrCore2MutexOwner, 1003,
		"sceAppMgrLaunchAppForDebugger");
	if (arg.budgetType == 2) {
		DELETE_APP_MAIN_LOCK(1008, "sceAppMgrLaunchAppForDebugger");
		while (appmgrGetShellApp() != NULL) {
			DELETE_APP_MAIN_UNLOCK(1011, "sceAppMgrLaunchAppForDebugger");
			LAUNCH_CORE2_UNLOCK(1012, "sceAppMgrLaunchAppForDebugger");
			result = killShellApplication();
			if (result != 0)
				return result;
			GC_CHECK_APP_OWNERS(1020, "sceAppMgrLaunchAppForDebugger");
			GC_LOCK(g_appmgrCore2Mutex, g_appmgrCore2MutexOwner, 1020,
				"sceAppMgrLaunchAppForDebugger");
			DELETE_APP_MAIN_LOCK(1022, "sceAppMgrLaunchAppForDebugger");
		}
		DELETE_APP_MAIN_UNLOCK(1024, "sceAppMgrLaunchAppForDebugger");
	}
	if (g_appmgrSystemSuspended) {
		if (!(processAttr & 1))
			signalSemaForDebuggerShellEventCount();
		LAUNCH_CORE2_UNLOCK(1033, "sceAppMgrLaunchAppForDebugger");
		return SCE_APPMGR_ERROR_STATE;
	}
	if (!hasParam) {
		while (_appListFindAndLockAppByName(arg.titleId, &app) >= 0) {
			waitObject = appmgrUtilGetWaitObject(3);
			appmgrUtilWaitObjectClear(waitObject);
			result = _appmgrProcessQuitProcess(app, 4, 0, 0);
			DELETE_APP_ASSERT(result == SCE_KERNEL_ERROR_INVALID_PROCESS_STATUS ||
				result == 0, 1052, "sceAppMgrLaunchAppForDebugger");
			unlockResult = _appListUnlockApp(app);
			DELETE_APP_CHECK(unlockResult, 0, 1055, "sceAppMgrLaunchAppForDebugger");
			appmgrUtilWaitObjectWait(waitObject, NULL);
		}
	}
	app = NULL;
	result = _sceAppMgrAppStateReserve(&app, arg.titleId, arg.param, -1, arg.launchMode, -1,
		creatorAppId, 0, 0);
	if (result != 0) {
		if (!(processAttr & 1))
			signalSemaForDebuggerShellEventCount();
		LAUNCH_CORE2_UNLOCK(1078, "sceAppMgrLaunchAppForDebugger");
		return result;
	}
	DELETE_APP_MAIN_LOCK(1082, "sceAppMgrLaunchAppForDebugger");
	appId = app->appId;
	if (options != NULL)
		memcpy(&app->launchOptions, options, sizeof(app->launchOptions));
	DELETE_APP_MAIN_UNLOCK(1091, "sceAppMgrLaunchAppForDebugger");
	if (!(processAttr & 1))
		_shellEventStartLaunchAppFromDebuggerQueuePush(-1, arg.liveareaType, appId);
	memset(&status, 0, sizeof(status));
	memset(&request, 0, sizeof(request));
	request.appPath = path;
	request.budgetSize = arg.memSize;
	request.pycontSize = arg.phcontMemSize;
	request.processCreateFlags = processCreateFlags;
	request.budgetType = arg.budgetType;
	request.processAttr = processAttr;
	if (args != NULL && argSize != 0) {
		request.launchArgs = args;
		request.launchArgsSize = argSize < 0x400 ? argSize : 0x400;
	}
	request.launchOrigin = 2;
	request.useCommonDialog = arg.useCommonDialog;
	request.launchArgumentMarker = 1;
	request.notifyBootComplete = 1;
	launchRequest = request;
	error = 0;
	result = sceAppMgrLaunchAppByName2ndStage_internal(appId, arg.titleId, &status,
		&launchRequest, hasParam, 0, &createParam, opt, options, NULL, &error);
	if (result >= 0 && error == 0) {
		if (!(processAttr & 1))
			_shellEventResultLaunchAppFromDebuggerQueuePush(status.appId, appId,
				status.processId);
	} else if (!(processAttr & 1)) {
		_shellEventResultLaunchAppFromDebuggerQueuePush(-1, appId, result);
	}
	LAUNCH_CORE2_UNLOCK(1147, "sceAppMgrLaunchAppForDebugger");
	return error != 0 ? error : result;
}

/* The launch request of a launch by the shell, from its arguments and options. */
static inline __attribute__((always_inline)) void makeLaunchRequest(AppmgrLaunchRequest *request,
		const char *appPath, SceUInt32 budgetSize, SceUInt32 processAttr,
		const AppmgrLaunchAppOption *option)
{
	SceUInt32 pycontSize;
	int useCommonDialog;

	if (option != NULL) {
		pycontSize = option->pycontSize;
		useCommonDialog = option->useCommonDialog != 0;
		memset(request, 0, sizeof(*request));
		request->appPath = appPath;
		__builtin_memcpy(request->proxyAppName, option->proxyAppName,
			sizeof(request->proxyAppName));
		request->proxyAppName[sizeof(request->proxyAppName) - 1] = '\0';
		request->budgetSize = budgetSize;
		request->pycontSize = pycontSize;
		request->budgetType = budgetSize == 0;
		request->processAttr = processAttr;
		request->defaultInfobar = option->defaultInfobar;
		request->launchArgumentMarker = option->launchArgumentMarker;
		if (option->launchOrigin == 1)
			request->launchOrigin = 1;
		request->enlargeGameMemorySize = option->enlargeGameMemorySize;
	} else {
		memset(request, 0, sizeof(*request));
		useCommonDialog = 0;
		request->launchArgumentMarker = 1;
		request->appPath = appPath;
		request->budgetSize = budgetSize;
		request->budgetType = budgetSize == 0;
		request->processAttr = processAttr;
	}
	request->useCommonDialog = useCommonDialog;
}

/* The abort handler's hook after an aborted application is handled; empty in this build. */
void appmgrArbitrationAfterAbort(void)
{
}

/*
 * The shell launches an application by path: a slot is reserved under the name, and the
 * launch request is built from the arguments and the shell's options.
 */
int _sceAppMgrLaunchApp2(const char *appPath, const char *name, SceUInt32 budgetSize,
		const char *param, SceUInt32 processAttr, const AppmgrLaunchAppOption *option)
{
	SceUInt32 syscallState = sceSyscallEnter();
	AppmgrLaunchRequest request, launchRequest;
	struct AppState *app;
	int appId, launchResult, error, result;

	if (!isShellProcess()) {
		sceSyscallLeave(syscallState);
		return SCE_APPMGR_ERROR_NOEXEC;
	}
	if ((processAttr & ~7) != 0 ||
	    (option != NULL && option->enlargeGameMemorySize > 109)) {
		sceSyscallLeave(syscallState);
		return SCE_APPMGR_ERROR_PARAM;
	}
	app = NULL;
	KILL_CHECK_APP_OWNERS(1573, "_sceAppMgrLaunchApp2");
	result = sceKernelLockMutex(g_appmgrCore2Mutex, 1, NULL);
	if (result != 0) {
		if ((unsigned int)(result + 0x7ffd7fd4) > 1)
			LAUNCH_CORE2_LOCK_FAILURE(result, 1573, "_sceAppMgrLaunchApp2");
		sceSyscallLeave(syscallState);
		return result;
	}
	g_appmgrCore2MutexOwner = sceKernelGetThreadId();
	result = _sceAppMgrAppStateReserve(&app, name, param, -1, 0, -1, -1, 0, 0);
	if (result != 0) {
		LAUNCH_CORE2_UNLOCK(1593, "_sceAppMgrLaunchApp2");
		sceSyscallLeave(syscallState);
		return result;
	}
	DELETE_APP_ASSERT(app != NULL, 1597, "_sceAppMgrLaunchApp2");
	appId = app->appId;
	makeLaunchRequest(&request, appPath, budgetSize, processAttr, option);
	launchRequest = request;
	error = 0;
	launchResult = sceAppMgrLaunchAppByName2ndStage_internal(appId, name, NULL,
		&launchRequest, 1, 0, NULL, NULL, NULL, option, &error);
	LAUNCH_CORE2_UNLOCK(1626, "_sceAppMgrLaunchApp2");
	sceSyscallLeave(syscallState);
	return error != 0 ? error : launchResult;
}

/* The shell's second stage of a launch by name: the application is already reserved. */
int sceAppMgrLaunchAppByName2ndStage(int appId, const char *appPath, SceUInt32 budgetSize,
		SceUInt32 processAttr, const AppmgrLaunchAppOption *option)
{
	SceUInt32 syscallState = sceSyscallEnter();
	AppmgrLaunchRequest request, launchRequest;
	const char *name;
	int launchResult, error, result;

	if (!isShellProcess()) {
		sceSyscallLeave(syscallState);
		return SCE_APPMGR_ERROR_NOEXEC;
	}
	if ((processAttr & ~7) != 0 ||
	    (option != NULL && option->enlargeGameMemorySize > 109)) {
		sceSyscallLeave(syscallState);
		return SCE_APPMGR_ERROR_PARAM;
	}
	KILL_CHECK_APP_OWNERS(1481, "sceAppMgrLaunchAppByName2ndStage");
	result = sceKernelLockMutex(g_appmgrCore2Mutex, 1, NULL);
	if (result != 0) {
		if ((unsigned int)(result + 0x7ffd7fd4) > 1)
			LAUNCH_CORE2_LOCK_FAILURE(result, 1481, "sceAppMgrLaunchAppByName2ndStage");
		sceSyscallLeave(syscallState);
		return result;
	}
	g_appmgrCore2MutexOwner = sceKernelGetThreadId();
	if (option != NULL && (option->flags & 0x100000))
		name = option->appName;
	else
		name = NULL;
	makeLaunchRequest(&request, appPath, budgetSize, processAttr, option);
	launchRequest = request;
	error = 0;
	launchResult = sceAppMgrLaunchAppByName2ndStage_internal(appId, name, NULL,
		&launchRequest, 1, 0, NULL, NULL, NULL, option, &error);
	LAUNCH_CORE2_UNLOCK(1538, "sceAppMgrLaunchAppByName2ndStage");
	sceSyscallLeave(syscallState);
	return error != 0 ? error : launchResult;
}

/*
 * A test launch: the arguments come as a user argv, and an app0: path is taken from the
 * calling application's own directory.
 */
int sceAppMgrLaunchAppForTest2(const char *appPath, const char *name, SceUInt32 budgetSize,
		const char *param, SceUInt32 processAttr, int startByUpdater,
		const char *const *argv, const AppmgrLaunchAppOption *option)
{
	static char launchArgs[1024];
	static char resolvedAppPath[256];
	static char rawAppPath[256];
	AppmgrLaunchRequest request, launchRequest;
	struct AppState *app, *caller;
	const char *colon;
	SceUInt32 useCommonDialog;
	int appId, argsSize, resolved, launchResult, error, result;

	app = NULL;
	if (option != NULL)
		useCommonDialog = option->useCommonDialog;
	else
		useCommonDialog = 0;
	KILL_CHECK_APP_OWNERS(1662, "sceAppMgrLaunchAppForTest2");
	result = sceKernelLockMutex(g_appmgrCore2Mutex, 1, NULL);
	if (result != 0) {
		if ((unsigned int)(result + 0x7ffd7fd4) > 1)
			LAUNCH_CORE2_LOCK_FAILURE(result, 1662, "sceAppMgrLaunchAppForTest2");
		return result;
	}
	g_appmgrCore2MutexOwner = sceKernelGetThreadId();
	argsSize = copyArgv(launchArgs, argv);
	if (argsSize < 0) {
		LAUNCH_CORE2_UNLOCK(1671, "sceAppMgrLaunchAppForTest2");
		return argsSize;
	}
	if (strncmp(appPath, "app0:", 5) == 0) {
		caller = appmgrFindAppByProcessId(g_appmgrAppStates, -2);
		if (caller == NULL) {
			LAUNCH_CORE2_UNLOCK(1687, "sceAppMgrLaunchAppForTest2");
			return (int)0x8080201c;
		}
		if (caller->launchInfo.budgetType == 2) {
			LAUNCH_CORE2_UNLOCK(1692, "sceAppMgrLaunchAppForTest2");
			return (int)0x8080201e;
		}
		colon = strchr(appPath, ':');
		if (colon == NULL) {
			LAUNCH_CORE2_UNLOCK(1701, "sceAppMgrLaunchAppForTest2");
			return (int)0x8080201e;
		}
		resolved = 1;
		snprintf(rawAppPath, sizeof(rawAppPath), "%s/%s",
			caller->process.content.appDirectory, colon + 1);
		SCE_DBG_PRINTF_FUNC(0, 1708, "sceAppMgrLaunchAppForTest2", "rawAppPath=%s\n",
			rawAppPath);
		appmgrResolveLaunchPath(rawAppPath, resolvedAppPath, sizeof(resolvedAppPath));
	} else {
		resolved = 0;
	}
	result = _sceAppMgrAppStateReserve(&app, name, param, -1, 0, -1, -1, 0, 0);
	if (result != 0) {
		LAUNCH_CORE2_UNLOCK(1717, "sceAppMgrLaunchAppForTest2");
		return result;
	}
	DELETE_APP_ASSERT(app != NULL, 1722, "sceAppMgrLaunchAppForTest2");
	appId = app->appId;
	if (appmgrGetShellApp() == NULL)
		processAttr |= 1;
	if (resolved)
		appPath = resolvedAppPath;
	memset(&request, 0, sizeof(request));
	request.appPath = appPath;
	request.budgetSize = budgetSize;
	request.pycontSize = option != NULL ? option->pycontSize : 0;
	request.budgetType = budgetSize == 0;
	request.processAttr = processAttr;
	if (argsSize != 0) {
		request.launchArgs = launchArgs;
		request.launchArgsSize = (SceUInt32)argsSize < sizeof(launchArgs) ?
			(SceSize)argsSize : sizeof(launchArgs);
	}
	if (option != NULL) {
		__builtin_memcpy(request.proxyAppName, option->proxyAppName,
			sizeof(request.proxyAppName));
		request.proxyAppName[sizeof(request.proxyAppName) - 1] = '\0';
		request.defaultInfobar = option->defaultInfobar;
		request.enlargeGameMemorySize = option->enlargeGameMemorySize;
		request.launchArgumentMarker = option->launchArgumentMarker;
		if (option->launchOrigin == 1)
			request.launchOrigin = 1;
	} else {
		request.launchArgumentMarker = 1;
	}
	request.useCommonDialog = useCommonDialog != 0;
	launchRequest = request;
	error = 0;
	launchResult = sceAppMgrLaunchAppByName2ndStage_internal(appId, name, NULL,
		&launchRequest, 1, startByUpdater, NULL, NULL, NULL, option, &error);
	LAUNCH_CORE2_UNLOCK(1752, "sceAppMgrLaunchAppForTest2");
	return error != 0 ? error : launchResult;
}

/* A test harness ends the application of a process. */
int sceAppMgrDestroyAppForTest(SceUID pid)
{
	struct AppState *app;
	int result, unlockResult;

	result = _appListFindAndLockAppByProcessId(pid, &app);
	if (result != 0)
		return result;
	if (g_appmgrDebugNotifier != NULL)
		g_appmgrDebugNotifier(4, app->appId, app->process.processId, 0);
	result = _appmgrProcessQuitProcess(app, 2, 0, 0);
	unlockResult = _appListUnlockApp(app);
	DELETE_APP_CHECK(unlockResult, 0, 1772, "sceAppMgrDestroyAppForTest");
	return result;
}

/* The system calls that launch by name: the shell launches the application later. */
int appmgrLaunchAppByName2(const SceChar8 *name, const char *param,
		const AppmgrLaunchAppByNameOption *option)
{
	SceUInt32 syscallState = sceSyscallEnter();
	SceUInt32 launchMode, evictionThresholdClass, resumeCreatorOnLaunchFailure;
	SceUID launchCallbackId, paramCallbackId;
	int creatorAppId, i, result;

	if (!sceSblACMgrIsSystemProgram(0)) {
		sceSyscallLeave(syscallState);
		return SCE_APPMGR_ERROR_NOEXEC;
	}
	if (option != NULL) {
		for (i = 0; i < sizeof(option->reservedTail); i++) {
			if (option->reservedTail[i] != 0) {
				sceSyscallLeave(syscallState);
				return SCE_APPMGR_ERROR_PARAM;
			}
		}
		launchMode = option->launchMode;
		launchCallbackId = option->launchCallbackId;
		paramCallbackId = option->paramCallbackId;
		if (launchMode & 0x840000)
			creatorAppId = option->creatorAppId;
		else
			creatorAppId = -1;
		evictionThresholdClass = option->evictionThresholdClass;
		if (launchCallbackId > 0)
			launchCallbackId = scePUIDtoGUID(sceKernelGetProcessId(),
				launchCallbackId);
		else
			launchCallbackId = -1;
		if (paramCallbackId > 0)
			paramCallbackId = scePUIDtoGUID(sceKernelGetProcessId(),
				paramCallbackId);
		else
			paramCallbackId = -1;
		resumeCreatorOnLaunchFailure = option->resumeCreatorOnLaunchFailure;
	} else {
		creatorAppId = -1;
		launchMode = 0;
		evictionThresholdClass = 0;
		resumeCreatorOnLaunchFailure = 0;
		paramCallbackId = -1;
		launchCallbackId = -1;
	}
	result = sceAppMgrLaunchAppByNameInternal(launchMode, name, param,
		launchCallbackId, paramCallbackId, creatorAppId, evictionThresholdClass,
		NULL, 0, resumeCreatorOnLaunchFailure);
	sceSyscallLeave(syscallState);
	return result < 0 ? result : 0;
}

/* The video streaming application, with its extra parameters, for the web browser too */
int appmgrLaunchVideoStreamingApp(const void *extraParam, SceSize extraParamSize,
		const AppmgrLaunchAppByNameOption *option)
{
	SceUInt32 syscallState = sceSyscallEnter();
	SceUInt32 launchMode, evictionThresholdClass, resumeCreatorOnLaunchFailure;
	SceUID launchCallbackId, paramCallbackId;
	int creatorAppId, i, result;

	if (!SceSblACMgrForDriver_0139FC20(0) && !sceSblACMgrIsSystemProgram(0)) {
		sceSyscallLeave(syscallState);
		return SCE_APPMGR_ERROR_NOEXEC;
	}
	if (option != NULL) {
		for (i = 0; i < sizeof(option->reservedTail); i++) {
			if (option->reservedTail[i] != 0) {
				sceSyscallLeave(syscallState);
				return SCE_APPMGR_ERROR_PARAM;
			}
		}
		launchMode = option->launchMode;
		launchCallbackId = option->launchCallbackId;
		paramCallbackId = option->paramCallbackId;
		if (launchMode & 0x840000)
			creatorAppId = option->creatorAppId;
		else
			creatorAppId = -1;
		evictionThresholdClass = option->evictionThresholdClass;
		if (launchCallbackId > 0)
			launchCallbackId = scePUIDtoGUID(sceKernelGetProcessId(),
				launchCallbackId);
		else
			launchCallbackId = -1;
		if (paramCallbackId > 0)
			paramCallbackId = scePUIDtoGUID(sceKernelGetProcessId(),
				paramCallbackId);
		else
			paramCallbackId = -1;
		resumeCreatorOnLaunchFailure = option->resumeCreatorOnLaunchFailure;
	} else {
		creatorAppId = -1;
		launchMode = 0;
		evictionThresholdClass = 0;
		resumeCreatorOnLaunchFailure = 0;
		paramCallbackId = -1;
		launchCallbackId = -1;
	}
	result = sceAppMgrLaunchAppByNameInternal(launchMode, (const SceChar8 *)"NPXS10085", NULL,
		launchCallbackId, paramCallbackId, creatorAppId, evictionThresholdClass,
		extraParam, extraParamSize, resumeCreatorOnLaunchFailure);
	sceSyscallLeave(syscallState);
	return result < 0 ? result : 0;
}

int appmgrLaunchAppByName2ForShell(const SceChar8 *name, const char *param,
		const AppmgrLaunchAppByNameOption *option)
{
	SceUInt32 syscallState = sceSyscallEnter();
	SceUInt32 launchMode, evictionThresholdClass, resumeCreatorOnLaunchFailure;
	SceUID launchCallbackId, paramCallbackId;
	int creatorAppId, i, result;

	if (!isShellProcess()) {
		sceSyscallLeave(syscallState);
		return SCE_APPMGR_ERROR_NOEXEC;
	}
	if (option != NULL) {
		for (i = 0; i < sizeof(option->reservedTail); i++) {
			if (option->reservedTail[i] != 0) {
				sceSyscallLeave(syscallState);
				return SCE_APPMGR_ERROR_PARAM;
			}
		}
		launchMode = option->launchMode;
		launchCallbackId = option->launchCallbackId;
		paramCallbackId = option->paramCallbackId;
		if (launchMode & 0x840000)
			creatorAppId = option->creatorAppId;
		else
			creatorAppId = -1;
		evictionThresholdClass = option->evictionThresholdClass;
		if (launchCallbackId > 0)
			launchCallbackId = scePUIDtoGUID(sceKernelGetProcessId(),
				launchCallbackId);
		else
			launchCallbackId = -1;
		if (paramCallbackId > 0)
			paramCallbackId = scePUIDtoGUID(sceKernelGetProcessId(),
				paramCallbackId);
		else
			paramCallbackId = -1;
		resumeCreatorOnLaunchFailure = option->resumeCreatorOnLaunchFailure;
	} else {
		creatorAppId = -1;
		launchMode = 0;
		evictionThresholdClass = 0;
		resumeCreatorOnLaunchFailure = 0;
		paramCallbackId = -1;
		launchCallbackId = -1;
	}
	result = sceAppMgrLaunchAppByNameInternal(launchMode, name, param,
		launchCallbackId, paramCallbackId, creatorAppId, evictionThresholdClass,
		NULL, 0, resumeCreatorOnLaunchFailure);
	sceSyscallLeave(syscallState);
	return result;
}

int appmgrLaunchAppByName(SceUInt32 launchMode, const SceChar8 *name, const char *param)
{
	SceUInt32 syscallState = sceSyscallEnter();
	int result;

	if (!sceSblACMgrIsSystemProgram(0)) {
		sceSyscallLeave(syscallState);
		return SCE_APPMGR_ERROR_NOEXEC;
	}
	result = sceAppMgrLaunchAppByNameInternal(launchMode, name, param, -1, -1, -1, 0,
		NULL, 0, 0);
	sceSyscallLeave(syscallState);
	return result < 0 ? result : 0;
}

int appmgrLaunchAppByNameForShell(SceUInt32 launchMode, const SceChar8 *name,
		const char *param)
{
	SceUInt32 syscallState = sceSyscallEnter();
	int result;

	if (!isShellProcess()) {
		sceSyscallLeave(syscallState);
		return SCE_APPMGR_ERROR_NOEXEC;
	}
	result = sceAppMgrLaunchAppByNameInternal(launchMode, name, param, -1, -1, -1, 0,
		NULL, 0, 0);
	sceSyscallLeave(syscallState);
	return result;
}

/*
 * The debugger launches by name from the kernel process and waits until the shell
 * has launched the application (the launch callback signals the semaphore).
 */
int sceAppMgrLaunchAppByNameForDebugger(const char *name, const char *param)
{
	SceUInt32 syscallState = sceSyscallEnter();
	SceUID semaphore, callbackId;
	int appId, result;

	if (sceKernelGetProcessId() != SCE_KERNEL_PROCESS_ID) {
		sceSyscallLeave(syscallState);
		return SCE_APPMGR_ERROR_NOEXEC;
	}
	/* Sony leaves the syscall state set when either object cannot be created. */
	semaphore = sceKernelCreateSema("SceAppMgr:semafordbg", 0, 0, 1, NULL);
	if (semaphore < 0)
		return semaphore;
	callbackId = sceKernelCreateCallback("SceAppMgr:cbkfordbg", 0, _onlaunchCallback,
		&semaphore);
	if (callbackId < 0) {
		sceKernelDeleteSema(semaphore);
		return callbackId;
	}
	appId = sceAppMgrLaunchAppByNameInternal(0x20000, (const SceChar8 *)name, param,
		callbackId, -1, -1, 0, NULL, 0, 0);
	result = sceKernelWaitSemaCB(semaphore, 1, NULL);
	DELETE_APP_CHECK(result, 0, 2160, "sceAppMgrLaunchAppByNameForDebugger");
	result = sceKernelDeleteSema(semaphore);
	DELETE_APP_CHECK(result, 0, 2163, "sceAppMgrLaunchAppByNameForDebugger");
	result = sceKernelDeleteCallback(callbackId);
	DELETE_APP_CHECK(result, 0, 2166, "sceAppMgrLaunchAppByNameForDebugger");
	sceSyscallLeave(syscallState);
	return appId < 0 ? appId : 0;
}

/* The kernel process launches an application by name, without a parameter. */
int SceAppMgrForDriver_99AA37C1(const char *name)
{
	SceUInt32 syscallState = sceSyscallEnter();
	int result;

	if (sceKernelGetProcessId() != SCE_KERNEL_PROCESS_ID) {
		sceSyscallLeave(syscallState);
		return SCE_APPMGR_ERROR_NOEXEC;
	}
	result = sceAppMgrLaunchAppByNameInternal(0, (const SceChar8 *)name, NULL, -1, -1, -1,
		0, NULL, 0, 0);
	sceSyscallLeave(syscallState);
	return result < 0 ? result : 0;
}

/* The launches by URI: the shell finds the application from the parameter. */
int appmgrLaunchAppByUri(SceUInt32 launchMode, const char *uri)
{
	SceUInt32 syscallState = sceSyscallEnter();
	int result;

	result = sceAppMgrLaunchAppByNameInternal(launchMode | 0x100000, NULL, uri, -1, -1, -1,
		0, NULL, 0, 0);
	sceSyscallLeave(syscallState);
	return result < 0 ? result : 0;
}

int appmgrLaunchAppByUri2(const char *uri, const AppmgrLaunchAppByNameOption *option)
{
	SceUInt32 syscallState = sceSyscallEnter();
	SceUInt32 launchMode, evictionThresholdClass, resumeCreatorOnLaunchFailure;
	SceUID launchCallbackId, paramCallbackId;
	int creatorAppId, i, result;

	if (option != NULL) {
		for (i = 0; i < sizeof(option->reservedTail); i++) {
			if (option->reservedTail[i] != 0) {
				sceSyscallLeave(syscallState);
				return SCE_APPMGR_ERROR_PARAM;
			}
		}
		launchMode = option->launchMode;
		launchCallbackId = option->launchCallbackId;
		paramCallbackId = option->paramCallbackId;
		if (launchMode & 0x40000)
			creatorAppId = option->creatorAppId;
		else
			creatorAppId = -1;
		evictionThresholdClass = option->evictionThresholdClass;
		launchMode |= 0x100000;
		if (launchCallbackId > 0)
			launchCallbackId = scePUIDtoGUID(sceKernelGetProcessId(),
				launchCallbackId);
		else
			launchCallbackId = -1;
		if (paramCallbackId > 0)
			paramCallbackId = scePUIDtoGUID(sceKernelGetProcessId(),
				paramCallbackId);
		else
			paramCallbackId = -1;
		resumeCreatorOnLaunchFailure = option->resumeCreatorOnLaunchFailure;
	} else {
		creatorAppId = -1;
		evictionThresholdClass = 0;
		resumeCreatorOnLaunchFailure = 0;
		launchCallbackId = -1;
		paramCallbackId = -1;
		launchMode = 0x100000;
	}
	result = sceAppMgrLaunchAppByNameInternal(launchMode, NULL, uri, launchCallbackId,
		paramCallbackId, creatorAppId, evictionThresholdClass, NULL, 0,
		resumeCreatorOnLaunchFailure);
	sceSyscallLeave(syscallState);
	return result < 0 ? result : 0;
}

/* The shell cancels a launch that has not started yet. */
int sceAppMgrLaunchAppCancel(int appId)
{
	SceUInt32 syscallState = sceSyscallEnter();
	struct AppState *app;
	SceUID creatorPid;
	int result;

	if (!isShellProcess()) {
		sceSyscallLeave(syscallState);
		return SCE_APPMGR_ERROR_NOEXEC;
	}
	DELETE_APP_MAIN_LOCK(2310, "sceAppMgrLaunchAppCancel");
	app = appmgrFindAppById(appId);
	if (app == NULL) {
		DELETE_APP_MAIN_UNLOCK(2315, "sceAppMgrLaunchAppCancel");
		sceSyscallLeave(syscallState);
		return (int)0x80802012;
	}
	if (appmgrGetAppRunStatus(app) != 0x300) {
		DELETE_APP_MAIN_UNLOCK(2323, "sceAppMgrLaunchAppCancel");
		sceSyscallLeave(syscallState);
		return (int)0x80802013;
	}
	creatorPid = -1;
	result = appmgrClearFailedAppLaunch(app, -1, &creatorPid);
	DELETE_APP_CHECK(result, 0, 2331, "sceAppMgrLaunchAppCancel");
	DELETE_APP_MAIN_UNLOCK(2333, "sceAppMgrLaunchAppCancel");
	if (creatorPid >= 0)
		sendResumeToPid(creatorPid);
	sceSyscallLeave(syscallState);
	return 0;
}

/*
 * Called with the application locked once its process is gone: the shell's end takes the
 * shell down, a pending relaunch spawns the process again, anything else deletes the slot.
 */
void _appArbitrationOnProcessUnloaded(struct AppState *app)
{
	struct AppState *shell;
	SceUID owner, pid;
	int relaunchPending, result;

	DELETE_APP_ASSERT(app != NULL && app->mutexOwner == sceKernelGetThreadId(), 2423, "_appArbitrationOnProcessUnloaded");
	owner = g_appmgrMainMutexOwner;
	DELETE_APP_ASSERT(owner != sceKernelGetThreadId(), 2424, "_appArbitrationOnProcessUnloaded");
	DELETE_APP_MAIN_LOCK(2427, "_appArbitrationOnProcessUnloaded");
	shell = appmgrGetShellApp();
	relaunchPending = app->relaunchPending;
	if (app->launchInfo.budgetType == 1) {
		result = sceKernelResetGameBudget();
		SCE_DBG_PRINTF_FUNC(0, 2434, "_appArbitrationOnProcessUnloaded", "sceKernelResetGameBudget() => %#x\n", result);
	}
	DELETE_APP_MAIN_UNLOCK(2437, "_appArbitrationOnProcessUnloaded");
	if (app == shell) {
		result = appmgrStopShellWatchDog();
		DELETE_APP_CHECK(result, 0, 2443, "_appArbitrationOnProcessUnloaded");
		DELETE_APP_MAIN_LOCK(2445, "_appArbitrationOnProcessUnloaded");
		_appListUndeclareShellProcess(g_appmgrAppStates);
		DELETE_APP_MAIN_UNLOCK(2447, "_appArbitrationOnProcessUnloaded");
		_sceAppMgrDeviceUndeclareShell();
		appmgrUtilWaitObjectNotify(appmgrUtilGetWaitObject(4));
		appmgrUtilWaitObjectNotify(appmgrUtilGetWaitObject(3));
		return;
	}
	if (shell != NULL) {
		if (relaunchPending) {
			DELETE_APP_MAIN_LOCK(2490, "_appArbitrationOnProcessUnloaded");
			pid = app->process.processId;
			app->process.processId = -1;
			_appListSetRunStatus(app, 0x600);
			app->quitRequested = 0;
			DELETE_APP_MAIN_UNLOCK(2495, "_appArbitrationOnProcessUnloaded");
			result = relaunchApp(app, pid);
			if (result >= 0)
				return;
			SCE_DBG_PRINTF_FUNC(0, 2499, "_appArbitrationOnProcessUnloaded", "[AppMgr:Error] ExitSpawn failed\n");
			DELETE_APP_MAIN_LOCK(2501, "_appArbitrationOnProcessUnloaded");
			_appListSetRunStatus(app, 0x800);
			_shellEventFatalErrorQueuePush(app->appId, result);
			DELETE_APP_MAIN_UNLOCK(2504, "_appArbitrationOnProcessUnloaded");
			result = _sceAppMgrDeleteAppInternal(app);
			DELETE_APP_CHECK(result, 0, 2507, "_appArbitrationOnProcessUnloaded");
			return;
		}
		result = _sceAppMgrDeleteAppInternal(app);
		DELETE_APP_CHECK(result, 0, 2512, "_appArbitrationOnProcessUnloaded");
		return;
	}
	if (relaunchPending) {
		DELETE_APP_MAIN_LOCK(2463, "_appArbitrationOnProcessUnloaded");
		_appListSetRunStatus(app, 0x600);
		pid = app->process.processId;
		app->process.processId = -1;
		app->quitRequested = 0;
		DELETE_APP_MAIN_UNLOCK(2468, "_appArbitrationOnProcessUnloaded");
		result = relaunchApp(app, pid);
		if (result >= 0)
			return;
		SCE_DBG_PRINTF_FUNC(0, 2472, "_appArbitrationOnProcessUnloaded", "[AppMgr:Error] ExitSpawn failed\n");
		DELETE_APP_MAIN_LOCK(2473, "_appArbitrationOnProcessUnloaded");
		_appListSetRunStatus(app, 0x800);
		DELETE_APP_MAIN_UNLOCK(2475, "_appArbitrationOnProcessUnloaded");
		result = _sceAppMgrDeleteAppInternal(app);
		DELETE_APP_CHECK(result, 0, 2478, "_appArbitrationOnProcessUnloaded");
		return;
	}
	result = _sceAppMgrDeleteAppInternal(app);
	DELETE_APP_CHECK(result, 0, 2483, "_appArbitrationOnProcessUnloaded");
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
		GC_LOCK(app->mutex, app->mutexOwner, _lockLine, _func); \
		GC_MAIN_LOCK(_mainLine, _func); \
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

/*
 * Runs or suspends the active application's process as the power state asks; a process
 * that fails to start or suspend loses the activation.
 */
void updateActiveAppState(void)
{
	struct AppState *app;
	int result;

	if (_appListFindAndLockActiveApp(&app) != 0)
		return;
	if (app != NULL && (app->state & 0xff0) == 0x700) {
		_appListUnlockApp(app);
		return;
	}
	if (_applicationShouldMakeRun(app))
		result = _appmgrProcessStartProcess(app);
	else if (app->process.requestedProcessState == 3)
		goto unlock;
	else
		result = _appmgrProcessSuspendProcess(app, 0, NULL);
	if (result != 0) {
		DELETE_APP_MAIN_LOCK(2922, "updateActiveAppState");
		_appListSetActiveness(app, 1);
		g_appmgrActiveAppId = -1;
		DELETE_APP_MAIN_UNLOCK(2925, "updateActiveAppState");
		result = _appListUnlockApp(app);
		DELETE_APP_CHECK(result, 0, 2927, "updateActiveAppState");
		return;
	}
unlock:
	result = _appListUnlockApp(app);
	DELETE_APP_CHECK(result, 0, 2932, "updateActiveAppState");
	_updateDeviceArbitration2();
}

/* what the update thread does with a process */
#define PROCESS_UPDATE_NONE	0
#define PROCESS_UPDATE_START	1
#define PROCESS_UPDATE_SUSPEND	2

/*
 * The process state updater: woken through g_appmgrUpdateNotifySemaphore, it brings the
 * active application up to date, then starts or suspends every other process as its
 * attributes and the power state ask.
 */
int _processUpdateThreadEntry(SceSize args, void *argp)
{
	struct AppState *app;
	SceUInt32 attributes;
	int index, action, result;

	(void)args;
	(void)argp;
	for (;;) {
		result = sceKernelWaitSema(g_appmgrUpdateNotifySemaphore, 1, NULL);
		if (result != 0) {
#if FW_DEBUG
			sceAppMgrDump();
			dumpBgmState();
			SCE_DBG_ASSERT_CHECK_FUNC(0, 2654, "_processUpdateThreadEntry");
#else
			SceDebugForDriver_391B5B74(&processUpdateWaitPanicInfo,
				__builtin_return_address(0));
#endif
		}
		dmb();
		updateActiveAppState();
		index = appmgrGetShellApp() != NULL;
		app = &g_appmgrAppStates[index];
		do {
			GC_APP_LOCK_CHECK(2663, "_processUpdateThreadEntry");
			result = sceKernelLockMutex(app->mutex, 1, NULL);
			if (result != 0) {
				if ((unsigned int)(result + 0x7ffd7fd4) > 1) {
#if FW_DEBUG
					GC_FAILURE(result, 2663, "_processUpdateThreadEntry");
#else
					appmgrGcListenerMutexPanic();
#endif
				}
			} else {
				app->mutexOwner = sceKernelGetThreadId();
			}
			GC_MAIN_LOCK(2668, "_processUpdateThreadEntry");
			action = PROCESS_UPDATE_NONE;
			if (appmgrGetAppRunState(app) == 2)
				;
			else if (g_appmgrSystemSuspended)
				goto suspend;
			else if ((attributes = app->launchInfo.processAttr) & 1) {
				if (app->backgroundSuspendRequested == 1)
					goto suspend;
				if (app->backgroundSuspendRequested == 0)
					action = app->process.requestedProcessState != 2;
			} else if (attributes & 2) {
				if (!SceSblAIMgrForDriver_71608CA3() || !g_appmgrPowerSaveMode)
					action = app->process.requestedProcessState != 2;
				else if (g_appmgrPowerSaveSuspendBackgroundApps)
					goto suspend;
				else if (app->process.runWhenUiOverlay &&
						app->process.requestedProcessState != 2)
					action = PROCESS_UPDATE_START;
				else if (!app->process.runWhenUiOverlay) {
suspend:
					if (app->process.requestedProcessState == 2)
						action = PROCESS_UPDATE_SUSPEND;
				}
			}
			g_appmgrMainMutexOwner = -1;
			result = sceKernelUnlockMutex(g_appmgrMainMutex, 1);
			GC_UNLOCK_CHECK(result, 2713, "_processUpdateThreadEntry");
			if (app->quitRequested && app->gameCardRemovalQuitPending)
				result = _appmgrProcessQuitProcess(app, 1, 0, 1);
			else if (action == PROCESS_UPDATE_SUSPEND)
				result = _appmgrProcessSuspendProcess(app, 0, NULL);
			else if (action == PROCESS_UPDATE_START)
				result = _appmgrProcessStartProcess(app);
			else
				goto unlock;
			if (result != 0)
				_appmgrProcessQuitProcess(app, 2, 0, 0);
unlock:
			GC_APP_UNLOCK_CHECK(2728, "_processUpdateThreadEntry");
			app->mutexOwner = -1;
			result = sceKernelUnlockMutex(app->mutex, 1);
			GC_UNLOCK_CHECK(result, 2728, "_processUpdateThreadEntry");
			++index;
			++app;
		} while (index <= 19);
	}
}

/*
 * Takes the activation from app, suspending its process when suspend asks for it and the
 * process is a running one of the background kind (waiter is notified when it is not).
 */
int _sceAppMgrDeactivateAppInternal(struct AppState *app, int suspend, int flags,
		struct AppmgrWaitObject *waiter)
{
	SceAppMgrSystemEvent event;
	SceUInt32 attributes;
	SceUID owner;
	int runStatus, result;

	DELETE_APP_ASSERT(app != NULL && app->mutexOwner == sceKernelGetThreadId(), 3092,
		"_sceAppMgrDeactivateAppInternal");
	owner = g_appmgrMainMutexOwner;
	DELETE_APP_ASSERT(owner != sceKernelGetThreadId(), 3093, "_sceAppMgrDeactivateAppInternal");
	DELETE_APP_MAIN_LOCK(3095, "_sceAppMgrDeactivateAppInternal");
	if (appmgrGetAppRunState(app) != 2) {
		DELETE_APP_MAIN_UNLOCK(3098, "_sceAppMgrDeactivateAppInternal");
		return 0x80802013;
	}
	runStatus = appmgrGetAppRunStatus(app);
	attributes = app->launchInfo.processAttr;
	if (attributes & 1) {
		DELETE_APP_MAIN_UNLOCK(3104, "_sceAppMgrDeactivateAppInternal");
		return 0x8080201a;
	}
	if (suspend) {
		if (attributes & 3)
			suspend = attributes & 1;
		else if (runStatus == 0x10) {
			suspend = 1;
			_appListSetRunStatus(app, 0x20);
		} else
			suspend = 0;
	}
	DELETE_APP_MAIN_UNLOCK(3120, "_sceAppMgrDeactivateAppInternal");
	if (suspend) {
		result = _appmgrProcessSuspendProcess(app, flags, waiter);
		DELETE_APP_CHECK(result, 0, 3125, "_sceAppMgrDeactivateAppInternal");
	} else if (waiter != NULL) {
		appmgrUtilWaitObjectNotify(waiter);
	}
	DELETE_APP_MAIN_LOCK(3132, "_sceAppMgrDeactivateAppInternal");
	_appListSetActiveness(app, 1);
	g_appmgrActiveAppId = -1;
	_deviceDeactivate();
	if (app->launchInfo.processAttr & 2) {
		memset(&event, 0, sizeof(event));
		event.systemEvent = 0x10000002;
		result = _appListPushEvent(app, &event);
		DELETE_APP_CHECK(result, 0, 3146, "_sceAppMgrDeactivateAppInternal");
	}
	DELETE_APP_MAIN_UNLOCK(3148, "_sceAppMgrDeactivateAppInternal");
	_updateDeviceArbitration2();
	return 0;
}

/*
 * Gives app the activation. On the core thread the application active now is deactivated
 * first; elsewhere an active application fails it with 0x80803001.
 */
int _sceAppMgrActivateAppInternal(struct AppState *app)
{
	SceAppMgrSystemEvent event;
	struct AppState *active, *other;
	SceUID owner, pid;
	int index, result;

	DELETE_APP_ASSERT(app != NULL && app->mutexOwner == sceKernelGetThreadId(), 2943,
		"_sceAppMgrActivateAppInternal");
	owner = g_appmgrMainMutexOwner;
	DELETE_APP_ASSERT(owner != sceKernelGetThreadId(), 2944, "_sceAppMgrActivateAppInternal");
	if (app->launchInfo.processAttr & 1)
		return 0x8080201a;
	owner = g_appmgrCore2MutexOwner;
	if (owner == sceKernelGetThreadId()) {
		for (;;) {
			if (_appListFindAndLockActiveApp(&active) == 0) {
				if (sceKernelGetProcessId() == active->process.processId) {
					result = _appListUnlockApp(active);
					DELETE_APP_CHECK(result, 0, 2967, "_sceAppMgrActivateAppInternal");
					return 0x80802013;
				}
				result = _sceAppMgrDeactivateAppInternal(active, 1, 0, NULL);
				DELETE_APP_CHECK(result, 0, 2973, "_sceAppMgrActivateAppInternal");
				result = _appListUnlockApp(active);
				DELETE_APP_CHECK(result, 0, 2976, "_sceAppMgrActivateAppInternal");
			}
			DELETE_APP_MAIN_LOCK(2980, "_sceAppMgrActivateAppInternal");
			if (appmgrFindActiveApp(g_appmgrAppStates) == NULL)
				break;
			DELETE_APP_MAIN_UNLOCK(2984, "_sceAppMgrActivateAppInternal");
		}
	} else {
		DELETE_APP_MAIN_LOCK(2992, "_sceAppMgrActivateAppInternal");
		if (appmgrFindActiveApp(g_appmgrAppStates) != NULL) {
			DELETE_APP_MAIN_UNLOCK(2996, "_sceAppMgrActivateAppInternal");
			return 0x80803001;
		}
	}
	owner = g_appmgrMainMutexOwner;
	DELETE_APP_ASSERT(owner == sceKernelGetThreadId(), 3001, "_sceAppMgrActivateAppInternal");
	_appListSetActiveness(app, 2);
	_deviceSetActiveAppId(app->appId);
	g_appmgrActiveAppId = app->appId;
	SCE_DBG_PRINTF_FUNC(0, 3010, "_sceAppMgrActivateAppInternal",
		"[SceAppMgr] Activate app : %s, appid=0x%08x, pid=0x%08x.\n",
		app->process.appName, g_appmgrActiveAppId, app->process.processId);
	app->baseMiniEvictionValue = 100;
	for (index = appmgrGetShellApp() != NULL; index <= 19; index++) {
		other = &g_appmgrAppStates[index];
		if (other == app)
			continue;
		pid = other->process.processId;
		if (pid >= 0 && (pid & 1) != 0 && other->launchInfo.budgetSize != 0 &&
				other->baseMiniEvictionValue > 0)
			other->baseMiniEvictionValue--;
	}
	_deviceSetRunStatus(app->appId, 1);
	DELETE_APP_MAIN_UNLOCK(3028, "_sceAppMgrActivateAppInternal");
	result = sceKernelSignalSema(g_appmgrUpdateNotifySemaphore, 1);
	if (result != 0 && result != SCE_KERNEL_ERROR_SEMA_OVF) {
#if FW_DEBUG
		DELETE_APP_FAILURE(result, SCE_KERNEL_ERROR_SEMA_OVF, 3064, "_sceAppMgrActivateAppInternal");
#else
		SceDebugForDriver_00CCE39C(&activateSignalPanicInfo, __builtin_return_address(0),
			"0x%08x,0x%08x\n", result, SCE_KERNEL_ERROR_SEMA_OVF);
#endif
	}
	_updateDeviceArbitration2();
	if (app->launchInfo.processAttr & 2) {
		DELETE_APP_MAIN_LOCK(3071, "_sceAppMgrActivateAppInternal");
		memset(&event, 0, sizeof(event));
		event.systemEvent = 0x10000001;
		result = _appListPushEvent(app, &event);
		DELETE_APP_CHECK(result, 0, 3077, "_sceAppMgrActivateAppInternal");
		DELETE_APP_MAIN_UNLOCK(3078, "_sceAppMgrActivateAppInternal");
	}
	return 0;
}

#if FW_DEBUG
#define SPAWN_SIGNAL_CHECK(_result, _line, _info) \
	DELETE_APP_CHECK(_result, 0, _line, "_spawnApp")
#else
#define SPAWN_SIGNAL_CHECK(_result, _line, _info) do { \
	if ((_result) != 0) \
		SceDebugForDriver_00CCE39C(&_info, __builtin_return_address(0), \
			"0x%08x,0x%08x\n", _result, 0); \
} while (0)
#endif

/* the mini application value a launch of each eviction threshold class may kill below */
static const int launchEvictionThresholds[4] = { 99999, 9999, 999, 0 };

/*
 * Creates app's process; with no shell the process starts and takes the activation at
 * once, otherwise the update thread or the launcher starts it.
 */
static inline __attribute__((always_inline)) int spawnApp(const AppmgrSpawnAppParam *param)
{
	struct AppState *app = param->app;
	const struct SceKernelProcessOpt *opt = param->opt;
	const AppmgrLaunchOptions *options = param->options;
	int started = param->started;
	const AppmgrCreateProcessParam *createParam = param->createParam;
	int *error = param->error;
	char name[32];
	char pathStart;
	SceUInt32 state;
	SceUID owner;
	int result;

	DELETE_APP_ASSERT(app != NULL && app->mutexOwner == sceKernelGetThreadId(), 3171,
		"_spawnApp");
	owner = g_appmgrMainMutexOwner;
	DELETE_APP_ASSERT(owner != sceKernelGetThreadId(), 3172, "_spawnApp");
	DELETE_APP_MAIN_LOCK(3176, "_spawnApp");
	pathStart = app->launchInfo.appPath[0];
	state = app->state;
	if (pathStart == '\0') {
		DELETE_APP_MAIN_UNLOCK(3181, "_spawnApp");
		return 0x80802016;
	}
	if (app->launchInfo.budgetType == 1 && _appListGetBigApp(g_appmgrAppStates) != NULL) {
		DELETE_APP_MAIN_UNLOCK(3188, "_spawnApp");
		return 0x80802019;
	}
	if (app->launchInfo.budgetType == 2 && appmgrGetShellApp() != NULL) {
		DELETE_APP_MAIN_UNLOCK(3195, "_spawnApp");
		return 0x80802022;
	}
	DELETE_APP_MAIN_UNLOCK(3200, "_spawnApp");
	strncpy(name, app->process.appName, sizeof(name));
	name[sizeof(name) - 1] = '\0';
	result = _appmgrProcessCreateProcess(app, name, options, opt, createParam, error);
	if (result < 0)
		return result;
	if (appmgrGetShellApp() == NULL) {
		result = _appmgrProcessStartProcess(app);
		if (result != 0) {
			int quitResult = _appmgrProcessQuitProcess(app, 2, 0, 0);
			DELETE_APP_CHECK(quitResult, 0, 3220, "_spawnApp");
			app->state = state;
			return result;
		}
		if (!(app->launchInfo.processAttr & 1)) {
			result = _sceAppMgrActivateAppInternal(app);
			DELETE_APP_CHECK(result, 0, 3226, "_spawnApp");
		}
		return 0;
	}
	if (started) {
		result = sceKernelSignalSema(g_appmgrUpdateNotifySemaphore, 1);
		SPAWN_SIGNAL_CHECK(result, 3231, spawnUpdaterSignalPanicInfo);
		return 0;
	}
	switch (app->launchInfo.budgetType) {
	case 0:
		if (app->launchInfo.processAttr & 1) {
			result = sceKernelSignalSema(g_appmgrUpdateNotifySemaphore, 1);
			SPAWN_SIGNAL_CHECK(result, 3253, spawnBackgroundSignalPanicInfo);
		}
		break;
	case 1:
		if (app->launchInfo.processAttr & 1) {
			result = sceKernelSignalSema(g_appmgrUpdateNotifySemaphore, 1);
			SPAWN_SIGNAL_CHECK(result, 3261, spawnGameSignalPanicInfo);
		}
		break;
	case 2:
		result = _appmgrProcessStartProcess(app);
		if (result != 0) {
			int quitResult = _appmgrProcessQuitProcess(app, 2, 0, 0);
			DELETE_APP_CHECK(quitResult, 0, 3242, "_spawnApp");
			app->state = state;
			return result;
		}
		break;
	default:
		DELETE_APP_ASSERT(0, 3267, "_spawnApp");
		break;
	}
	return 0;
}

/* run on a stack of its own (SceThreadmgrForDriver_E54FD746) */
int _spawnApp(void *argument)
{
	return spawnApp(argument);
}

/* quits the running game and waits for it to end */
static inline __attribute__((always_inline)) int killGameApplication(void)
{
	struct AppState *game;
	SceUID owner;
	int result, unlockResult;

	owner = g_appmgrMainMutexOwner;
	DELETE_APP_ASSERT(owner != sceKernelGetThreadId(), 486, "killGameApplication");
	if (_appListFindAndLockAppByAppId(-3, &game) != 0)
		return 0;
	result = _appmgrProcessQuitProcess(game, 3, 0, 0);
	unlockResult = _appListUnlockApp(game);
	DELETE_APP_CHECK(unlockResult, 0, 498, "killGameApplication");
	if (result != 0)
		return result;
	appmgrUtilWaitObjectWait(appmgrUtilGetWaitObject(2), NULL);
	return 0;
}

/* queues an application event carrying one word */
static inline __attribute__((always_inline)) int utilSendEvent(struct AppState *app,
		int type, int argument)
{
	SceAppMgrSystemEvent event;
	int result;

	memset(&event, 0, sizeof(event));
	event.systemEvent = type;
	__builtin_memcpy(event.data.reserved, &argument, sizeof(argument));
	result = _appListPushEvent(app, &event);
	DELETE_APP_CHECK(result, 0, 50, "utilSendEvent");
	return result;
}

/*
 * Starts the reserved application appId: a running one is handed the launch parameter
 * instead; a launch the memory does not fit kills the game or mini applications and is
 * tried again. Returns the process id.
 */
int _launchApp4(void *argument)
{
	const AppmgrLaunchApp4Param *param = argument;
	int appId = param->appId;
	const struct SceKernelProcessOpt *opt = param->opt;
	const AppmgrLaunchOptions *options = param->options;
	int sendParamToRunning = param->sendParamToRunning;
	int startByUpdater = param->startByUpdater;
	const AppmgrCreateProcessParam *createParam = param->createParam;
	SceAppMgrAppStatus *status = param->status;
	unsigned int *paramResult = param->paramResult;
	int *error = param->error;
	AppmgrSpawnAppParam spawn;
	struct AppState *app, *target;
	SceUInt32 budgetType, attributes, flags;
	SceUID pid;
	int result, unlockResult, maxValue;

	for (;;) {
		result = _appListFindAndLockAppByAppId(appId, &app);
		if (result != 0)
			return result;
		if (appmgrGetAppRunStatus(app) != 0x300 && appmgrGetAppRunStatus(app) != 0x600) {
			unlockResult = _appListUnlockApp(app);
			DELETE_APP_CHECK(unlockResult, 0, 3395, "_launchApp4");
			return 0x80802013;
		}
		if (sendParamToRunning) {
			result = _sendParam(app->process.appName,
				(const AppmgrParamQueueEntry *)appmgrParamQueuePeek(&app->params),
				paramResult, appId);
			if (result >= 0) {
				unlockResult = _appListFindAndLockAppByProcessId(result, &target);
				DELETE_APP_CHECK(unlockResult, 0, 3408, "_launchApp4");
				if (status != NULL) {
					DELETE_APP_MAIN_LOCK(3411, "_launchApp4");
					_appListGetAppStatus(target, status);
					DELETE_APP_MAIN_UNLOCK(3413, "_launchApp4");
				}
				unlockResult = _appListUnlockApp(target);
				DELETE_APP_CHECK(unlockResult, 0, 3420, "_launchApp4");
				unlockResult = _appListUnlockApp(app);
				DELETE_APP_CHECK(unlockResult, 0, 3423, "_launchApp4");
				return result;
			}
			if (result != (int)0x80802012) {
				unlockResult = _appListUnlockApp(app);
				DELETE_APP_CHECK(unlockResult, 0, 3428, "_launchApp4");
				return result;
			}
		}
		spawn.app = app;
		spawn.opt = opt;
		spawn.options = options;
		spawn.started = startByUpdater;
		spawn.createParam = createParam;
		spawn.error = error;
		result = spawnApp(&spawn);
		if (result >= 0)
			break;
		budgetType = app->launchInfo.budgetType;
		attributes = app->launchInfo.processAttr & 4;
		unlockResult = _appListUnlockApp(app);
		DELETE_APP_CHECK(unlockResult, 0, 3478, "_launchApp4");
		if (result == (int)0x8080202a) {
			if (killAllMiniApplication() != 0)
				return result;
			continue;
		}
		if (attributes)
			return result;
		if (result == (int)0x80802019) {
			result = killGameApplication();
			if (result != 0)
				return result;
			continue;
		}
		if (budgetType != 0)
			return result;
		if (result != SCE_KERNEL_ERROR_CANNOT_ALLOC_PHYMEMLOW && result != (int)0x80802018)
			return result;
		if (app->evictionThresholdClass - 1 <= 3)
			maxValue = launchEvictionThresholds[app->evictionThresholdClass - 1];
		else
			maxValue = 999999;
		if (killLeastValuableMiniApplication2(maxValue) != 0) {
			DELETE_APP_ASSERT((unsigned int)(result + 0x7ffd7fd4) > 1, 3514, "_launchApp4");
			return SCE_KERNEL_ERROR_CANNOT_ALLOC_PHYMEMLOW;
		}
	}
	if (status != NULL) {
		DELETE_APP_MAIN_LOCK(3447, "_launchApp4");
		_appListGetAppStatus(app, status);
		DELETE_APP_MAIN_UNLOCK(3449, "_launchApp4");
	}
	pid = app->process.processId;
	result = utilSendEvent(app, 0x20000004, app->launchSourceProcessId);
	DELETE_APP_CHECK(result, 0, 3454, "_launchApp4");
	flags = app->launchInfo.processAttr & 3;
	if (app->launchInfo.budgetType == 0)
		flags |= 4;
	if (strncmp(app->process.content.devicePrefix, "gro0:", 5) == 0 ||
			strncmp(app->process.content.devicePrefix, "grw0:", 5) == 0)
		flags |= 8;
	if (g_appmgrDebugNotifier != NULL)
		g_appmgrDebugNotifier(1, app->appId, app->process.processId, flags);
	unlockResult = _appListUnlockApp(app);
	DELETE_APP_CHECK(unlockResult, 0, 3468, "_launchApp4");
	return pid;
}

int sceAppMgrQuitApp(int appId)
{
	SceUInt32 syscallState = sceSyscallEnter();
	struct AppState *app;
	int result, unlockResult;

	if (!sceSblACMgrIsSystemProgram(0)) {
		sceSyscallLeave(syscallState);
		return SCE_APPMGR_ERROR_NOEXEC;
	}
	result = _appListFindAndLockAppByAppId(appId, &app);
	if (result != 0) {
		sceSyscallLeave(syscallState);
		return result;
	}
	result = _appmgrProcessQuitProcess(app, 1, 0, 0);
	unlockResult = _appListUnlockApp(app);
	DELETE_APP_CHECK(unlockResult, 0, 3550, "sceAppMgrQuitApp");
	sceSyscallLeave(syscallState);
	return result;
}

/*
 * Quits every application process but the shell's, then waits up to 15 s for all of them
 * to be gone.
 */
int _destroyAllAppForAbortHandler(void)
{
	struct AppState *app;
	SceUInt32 remaining;
	int index, alive, count, result, unlockResult;

	SCE_DBG_PRINTF_FUNC(0, 3561, "_destroyAllAppForAbortHandler",
		"[SceAppMgr] Kill all apps\n");
	KILL_CHECK_APP_OWNERS(3562, "_destroyAllAppForAbortHandler");
	result = sceKernelLockMutex(g_appmgrCore2Mutex, 1, NULL);
	if (result != 0) {
		if ((unsigned int)(result + 0x7ffd7fd4) > 1)
			LAUNCH_CORE2_LOCK_FAILURE(result, 3562, "_destroyAllAppForAbortHandler");
	} else {
		g_appmgrCore2MutexOwner = sceKernelGetThreadId();
	}
	DELETE_APP_CHECK(result, 0, 3563, "_destroyAllAppForAbortHandler");
	GC_MAIN_LOCK(3565, "_destroyAllAppForAbortHandler");
	index = appmgrGetShellApp() != NULL;
	app = &g_appmgrAppStates[index];
	do {
		if (app->process.processId >= 0 && (app->process.processId & 1) != 0) {
			DELETE_APP_MAIN_UNLOCK(3571, "_destroyAllAppForAbortHandler");
			GC_APP_LOCK_CHECK(3574, "_destroyAllAppForAbortHandler");
			unlockResult = sceKernelLockMutex(app->mutex, 1, NULL);
			if (unlockResult != 0) {
				if ((unsigned int)(unlockResult + 0x7ffd7fd4) > 1)
					LAUNCH_CORE2_LOCK_FAILURE(unlockResult, 3574,
						"_destroyAllAppForAbortHandler");
			} else {
				app->mutexOwner = sceKernelGetThreadId();
			}
			DELETE_APP_CHECK(result, 0, 3575, "_destroyAllAppForAbortHandler");
			result = _appmgrProcessQuitProcess(app, 2, 0, 0);
			DELETE_APP_CHECK(result, 0, 3578, "_destroyAllAppForAbortHandler");
			unlockResult = _appListUnlockApp(app);
			DELETE_APP_CHECK(unlockResult, 0, 3581, "_destroyAllAppForAbortHandler");
			GC_MAIN_LOCK(3583, "_destroyAllAppForAbortHandler");
		}
		++index;
		++app;
	} while (index <= 19);
	DELETE_APP_MAIN_UNLOCK(3588, "_destroyAllAppForAbortHandler");
	SCE_DBG_PRINTF_FUNC(0, 3590, "_destroyAllAppForAbortHandler",
		"[SceAppMgr] Waiting for the completion of kill of all apps\n");
	count = 15;
	do {
		GC_MAIN_LOCK(3595, "_destroyAllAppForAbortHandler");
		index = appmgrGetShellApp() != NULL;
		alive = 0;
		app = &g_appmgrAppStates[index];
		do {
			if (app->process.processId >= 0 && (app->process.processId & 1) != 0) {
				alive = 1;
				SCE_DBG_PRINTF_FUNC(0, 3602, "_destroyAllAppForAbortHandler",
					"[SceAppMgr] PID=0x%08x is not kill yet.\n",
					app->process.processId);
			}
			++index;
			++app;
		} while (index <= 19);
		DELETE_APP_MAIN_UNLOCK(3605, "_destroyAllAppForAbortHandler");
		if (!alive)
			break;
		SceThreadmgrForDriver_E9A99C50(1000000, &remaining);
	} while (--count != 0);
	SCE_DBG_PRINTF_FUNC(0, 3617, "_destroyAllAppForAbortHandler",
		"[SceAppMgr] Apps were completedly killed.\n");
	LAUNCH_CORE2_UNLOCK(3618, "_destroyAllAppForAbortHandler");
	return 0;
}

/* Quits every application process but the shell's and app's own, then tells the shell. */
int _destroyOtherApp(struct AppState *app)
{
	struct AppState *other;
	int index, result;

	if (app == NULL)
		return (int)0x80802012;
	index = appmgrGetShellApp() != NULL;
	other = &g_appmgrAppStates[index];
	do {
		if (other->process.processId >= 0 && (other->process.processId & 1) != 0 &&
				app->process.processId != other->process.processId) {
			DELETE_APP_MAIN_UNLOCK(3638, "_destroyOtherApp");
			GC_APP_LOCK_CHECK(3641, "_destroyOtherApp");
			result = sceKernelLockMutex(other->mutex, 1, NULL);
			if (result != 0) {
				if ((unsigned int)(result + 0x7ffd7fd4) > 1)
					LAUNCH_CORE2_LOCK_FAILURE(result, 3641, "_destroyOtherApp");
				if (result < 0)
					return result;
			} else {
				other->mutexOwner = sceKernelGetThreadId();
			}
			result = _appmgrProcessQuitProcess(other, 2, 0, 0);
			DELETE_APP_CHECK(result, 0, 3647, "_destroyOtherApp");
			result = _appListUnlockApp(other);
			DELETE_APP_CHECK(result, 0, 3650, "_destroyOtherApp");
			GC_MAIN_LOCK(3652, "_destroyOtherApp");
		}
		++index;
		++other;
	} while (index <= 19);
	_shellEventDestroyOtherAppQueuePush(app->process.processId, app->appId);
	return 0;
}

/* A system call's core2 lock: a lock error other than a stopped or suspended thread fails. */
#define SYSCALL_CORE2_LOCK(_line, _func) do { \
	GC_CHECK_APP_OWNERS(_line, _func); \
	result = sceKernelLockMutex(g_appmgrCore2Mutex, 1, NULL); \
	if (result != 0) { \
		if (result != SCE_KERNEL_ERROR_THREAD_STOPPED && \
				result != SCE_KERNEL_ERROR_THREAD_SUSPENDED) \
			LAUNCH_CORE2_LOCK_FAILURE(result, _line, _func); \
		if (result < 0) { \
			sceSyscallLeave(syscallState); \
			return result; \
		} \
	} else { \
		g_appmgrCore2MutexOwner = sceKernelGetThreadId(); \
	} \
} while (0)

/* Takes the arbitration locks, quits every application but app, and gives the locks back. */
#define DESTROY_OTHER_APP(_find, _lockLine, _mainLine, _unlockLine, _func) do { \
	SYSCALL_CORE2_LOCK(_lockLine, _func); \
	GC_MAIN_LOCK(_mainLine, _func); \
	result = _destroyOtherApp(_find); \
	DELETE_APP_MAIN_UNLOCK(_unlockLine, _func); \
	LAUNCH_CORE2_UNLOCK((_unlockLine) + 1, _func); \
} while (0)

int sceAppMgrDestroyOtherAppByPidForShell(SceUID pid)
{
	SceUInt32 syscallState = sceSyscallEnter();
	int result;

	if (!isShellProcess()) {
		sceSyscallLeave(syscallState);
		return (int)0x8080201c;
	}
	DESTROY_OTHER_APP(appmgrFindAppByProcessId(g_appmgrAppStates, pid), 3673, 3680, 3687,
		"sceAppMgrDestroyOtherAppByPidForShell");
	sceSyscallLeave(syscallState);
	return result;
}

int sceAppMgrDestroyOtherAppByAppIdForShell(int appId)
{
	SceUInt32 syscallState = sceSyscallEnter();
	int result;

	if (!isShellProcess()) {
		sceSyscallLeave(syscallState);
		return (int)0x8080201c;
	}
	DESTROY_OTHER_APP(appmgrFindAppById(appId), 3702, 3709, 3716,
		"sceAppMgrDestroyOtherAppByAppIdForShell");
	sceSyscallLeave(syscallState);
	return result;
}

int sceAppMgrDestroyOtherApp(void)
{
	SceUInt32 syscallState = sceSyscallEnter();
	int result;

	if (!sceSblACMgrIsSystemProgram(0) && sceKernelGetProcessId() != SCE_KERNEL_PROCESS_ID) {
		sceSyscallLeave(syscallState);
		return SCE_APPMGR_ERROR_NOEXEC;
	}
	DESTROY_OTHER_APP(appmgrFindAppByProcessId(g_appmgrAppStates, sceKernelGetProcessId()),
		3736, 3743, 3751, "sceAppMgrDestroyOtherApp");
	sceSyscallLeave(syscallState);
	return result;
}

/*
 * Grows the game's budget by size; with killMiniApps set, mini applications are killed,
 * least valuable first, while the budget cannot grow.
 */
int sceAppMgrGrowMemory(SceSize size, int killMiniApps)
{
	SceUInt32 syscallState = sceSyscallEnter();
	struct AppState *app;
	SceUID budgetId;
	int result;

	if (!SceSblACMgrForDriver_0139FC20(0) && !sceSblACMgrIsSystemProgram(0)) {
		sceSyscallLeave(syscallState);
		return SCE_APPMGR_ERROR_NOEXEC;
	}
	SYSCALL_CORE2_LOCK(3769, "sceAppMgrGrowMemory");
	GC_MAIN_LOCK(3776, "sceAppMgrGrowMemory");
	app = appmgrFindAppByName(g_appmgrAppStates, NULL);
	if (app->launchInfo.budgetType != 0) {
		DELETE_APP_MAIN_UNLOCK(3781, "sceAppMgrGrowMemory");
		LAUNCH_CORE2_UNLOCK(3782, "sceAppMgrGrowMemory");
		sceSyscallLeave(syscallState);
		return (int)0x8080201c;
	}
	DELETE_APP_MAIN_UNLOCK(3786, "sceAppMgrGrowMemory");
	budgetId = app->process.budgetId;
	if (killMiniApps) {
		do {
			result = sceKernelGrowBudget(budgetId, 0, size);
			if (result != SCE_KERNEL_ERROR_CANNOT_GROW_PHYMEMPART)
				break;
			result = killLeastValuableMiniApplication();
		} while (result >= 0);
	} else {
		result = sceKernelGrowBudget(budgetId, 0, size);
	}
	LAUNCH_CORE2_UNLOCK(3807, "sceAppMgrGrowMemory");
	sceSyscallLeave(syscallState);
	return result;
}

/*
 * Grows the game's budget by size, killing mini applications worth less than the eviction
 * threshold of thresholdClass while the budget cannot grow.
 */
int sceAppMgrGrowMemory3(SceSize size, unsigned int thresholdClass)
{
	SceUInt32 syscallState;
	struct AppState *app;
	SceUID budgetId;
	int maxValue, result;

	if (thresholdClass > 4)
		return (int)0x8080201a;
	if (thresholdClass - 1 <= 3)
		maxValue = launchEvictionThresholds[thresholdClass - 1];
	else
		maxValue = 999999;
	syscallState = sceSyscallEnter();
	if (!SceSblACMgrForDriver_0139FC20(0) && !sceSblACMgrIsSystemProgram(0)) {
		sceSyscallLeave(syscallState);
		return SCE_APPMGR_ERROR_NOEXEC;
	}
	SYSCALL_CORE2_LOCK(3837, "sceAppMgrGrowMemory3");
	GC_MAIN_LOCK(3844, "sceAppMgrGrowMemory3");
	app = appmgrFindAppByName(g_appmgrAppStates, NULL);
	if (app->launchInfo.budgetType != 0) {
		DELETE_APP_MAIN_UNLOCK(3849, "sceAppMgrGrowMemory3");
		LAUNCH_CORE2_UNLOCK(3850, "sceAppMgrGrowMemory3");
		sceSyscallLeave(syscallState);
		return (int)0x8080201c;
	}
	DELETE_APP_MAIN_UNLOCK(3854, "sceAppMgrGrowMemory3");
	budgetId = app->process.budgetId;
	if (maxValue > 0) {
		do {
			result = sceKernelGrowBudget(budgetId, 0, size);
			if (result != SCE_KERNEL_ERROR_CANNOT_GROW_PHYMEMPART)
				break;
			result = killLeastValuableMiniApplication2(maxValue);
		} while (result >= 0);
	} else {
		result = sceKernelGrowBudget(budgetId, 0, size);
	}
	LAUNCH_CORE2_UNLOCK(3875, "sceAppMgrGrowMemory3");
	sceSyscallLeave(syscallState);
	return result;
}

#if FW_DEBUG
#define LOAD_EXEC_WAIT_CHECK(_result, _line, _info) \
	DELETE_APP_FAILURE(_result, 0, _line, "sceAppMgrLoadExec")
#else
#define LOAD_EXEC_WAIT_CHECK(_result, _line, _info) \
	SceDebugForDriver_00CCE39C(&_info, __builtin_return_address(0), "0x%08x,0x%08x\n", \
		_result, 0)
#endif

/*
 * Relaunches the calling game with another of its executables (app0:<name>) and argv: the
 * process quits for the relaunch and this call does not return when that succeeds.
 */
int sceAppMgrLoadExec(const char *path, const char *const *argv, const void *option)
{
	SceUInt32 syscallState;
	struct AppState *app;
	char launchPath[256];
	SceUInt32 remaining;
	const char *colon;
	int i, c, result, unlockResult;

	if (path == NULL)
		return (int)0x80802016;
	if (option != NULL)
		return (int)0x8080201a;
	syscallState = sceSyscallEnter();
	_appListFindAndLockAppByAppId(-2, &app);
	if (app == NULL) {
		sceSyscallLeave(syscallState);
		return (int)0x8080201c;
	}
	if (app->launchInfo.budgetType != 1) {
		unlockResult = _appListUnlockApp(app);
		DELETE_APP_CHECK(unlockResult, 0, 3951, "sceAppMgrLoadExec");
		sceSyscallLeave(syscallState);
		return (int)0x8080201c;
	}
	if (app->quitRequested) {
		unlockResult = _appListUnlockApp(app);
		DELETE_APP_CHECK(unlockResult, 0, 3959, "sceAppMgrLoadExec");
		result = sceKernelWaitSema(g_appmgrBottomSemaphore, 1, NULL);
		if (result != SCE_KERNEL_ERROR_THREAD_STOPPED &&
				result != SCE_KERNEL_ERROR_THREAD_SUSPENDED && result != 0)
			LOAD_EXEC_WAIT_CHECK(result, 3963, loadExecQuitWaitPanicInfo);
		sceSyscallLeave(syscallState);
		return (int)0x80802013;
	}
	if (app->relaunchPending) {
		unlockResult = _appListUnlockApp(app);
		DELETE_APP_CHECK(unlockResult, 0, 3973, "sceAppMgrLoadExec");
		result = sceKernelWaitSema(g_appmgrBottomSemaphore, 1, NULL);
		if (result != SCE_KERNEL_ERROR_THREAD_STOPPED &&
				result != SCE_KERNEL_ERROR_THREAD_SUSPENDED && result != 0)
			LOAD_EXEC_WAIT_CHECK(result, 3977, loadExecRelaunchWaitPanicInfo);
		sceSyscallLeave(syscallState);
		return (int)0x80802013;
	}

	/* app0:, one optional separator, then a name without separators */
	i = 0;
	do {
		if ("app0:"[i] == '\0')
			break;
		if ("app0:"[i] != path[i])
			goto invalid;
		++i;
	} while (i != 256);
	if (path[i] == '/' || path[i] == '\\')
		++i;
	if (i > 255)
		goto invalid;
	for (c = path[i]; c != '\0'; c = path[i])
		if (c == '/' || c == '\\' || c == ':' || ++i == 256)
			goto invalid;

	result = copyArgv(app->launchInfo.launchArgs, argv);
	if (result < 0) {
		unlockResult = _appListUnlockApp(app);
		DELETE_APP_CHECK(unlockResult, 0, 3999, "sceAppMgrLoadExec");
		sceSyscallLeave(syscallState);
		return result;
	}
	app->launchInfo.launchArgsSize = result;
	colon = strchr(path, ':');
	if (colon == NULL) {
		unlockResult = _appListUnlockApp(app);
		DELETE_APP_CHECK(unlockResult, 0, 4015, "sceAppMgrLoadExec");
		sceSyscallLeave(syscallState);
		return (int)0x8080201e;
	}
	if (app->launchInfo.launchOrigin != 2) {
		snprintf(launchPath, sizeof(launchPath), "%s/%s",
			app->process.content.appDirectory, colon + 1);
	} else if (appmgrResolvePath(path, 0, launchPath, sizeof(launchPath)) < 0 ||
			(strncmp(launchPath, "ux0:", 4) == 0 &&
			 strncmp(launchPath, "ux0:/data/", 10) != 0)) {
		unlockResult = _appListUnlockApp(app);
		DELETE_APP_CHECK(unlockResult, 0, 4031, "sceAppMgrLoadExec");
		sceSyscallLeave(syscallState);
		return (int)0x8080201e;
	}
	appmgrResolveLaunchPath(launchPath, app->launchInfo.appPath,
		sizeof(app->launchInfo.appPath));
	app->relaunchPending = 1;
	result = _appmgrProcessQuitProcess(app, 5, 1, 0);
	unlockResult = _appListUnlockApp(app);
	DELETE_APP_CHECK(unlockResult, 0, 4054, "sceAppMgrLoadExec");
	if (result == 0)
		while (SceThreadmgrForDriver_E9A99C50(15000000, &remaining) == 0)
			;
	sceSyscallLeave(syscallState);
	return result;

invalid:
	unlockResult = _appListUnlockApp(app);
	DELETE_APP_CHECK(unlockResult, 0, 3990, "sceAppMgrLoadExec");
	sceSyscallLeave(syscallState);
	return (int)0x8080201e;
}

/* A background application that may not be suspended quits itself, once. */
int sceAppMgrQuitForNonSuspendableApp(void)
{
	SceUInt32 syscallState = sceSyscallEnter();
	struct AppState *app;
	int result, unlockResult;

	if (!SceSblACMgrForDriver_6C5AB07F(0)) {
		sceSyscallLeave(syscallState);
		return SCE_APPMGR_ERROR_NOEXEC;
	}
	result = _appListFindAndLockAppByProcessId(sceKernelGetProcessId(), &app);
	if (result != 0) {
		sceSyscallLeave(syscallState);
		return result;
	}
	if (!(app->launchInfo.processAttr & 2)) {
		unlockResult = _appListUnlockApp(app);
		DELETE_APP_CHECK(unlockResult, 0, 4092, "sceAppMgrQuitForNonSuspendableApp");
		sceSyscallLeave(syscallState);
		return (int)0x8080201c;
	}
	GC_MAIN_LOCK(4095, "sceAppMgrQuitForNonSuspendableApp");
	g_appmgrMainMutexOwner = -1;
	app->quitRequested = 0;
	app->launchInfo.processAttr &= ~2;
	unlockResult = sceKernelUnlockMutex(g_appmgrMainMutex, 1);
	DELETE_APP_KM_UNLOCK_CHECK(unlockResult, 4098, "sceAppMgrQuitForNonSuspendableApp");
	result = _appmgrProcessQuitProcess(app, 2, 0, 0);
	unlockResult = _appListUnlockApp(app);
	DELETE_APP_CHECK(unlockResult, 0, 4103, "sceAppMgrQuitForNonSuspendableApp");
	sceSyscallLeave(syscallState);
	return result;
}

/* The calling game quits to the LiveArea. */
int sceAppMgrExitToLiveboardForGameApp(void)
{
	SceUInt32 syscallState = sceSyscallEnter();
	struct AppState *app;
	int result, unlockResult;

	result = _appListFindAndLockAppByProcessId(sceKernelGetProcessId(), &app);
	if (result != 0) {
		sceSyscallLeave(syscallState);
		return result;
	}
	if (app->launchInfo.budgetType != 1) {
		unlockResult = _appListUnlockApp(app);
		DELETE_APP_CHECK(unlockResult, 0, 4128, "sceAppMgrExitToLiveboardForGameApp");
		sceSyscallLeave(syscallState);
		return (int)0x8080201c;
	}
	result = _appmgrProcessQuitProcess(app, 1, 0, 0);
	unlockResult = _appListUnlockApp(app);
	DELETE_APP_CHECK(unlockResult, 0, 4139, "sceAppMgrExitToLiveboardForGameApp");
	sceSyscallLeave(syscallState);
	return result;
}

int sceAppMgrDestroyAppByName(const char *name)
{
	SceUInt32 syscallState = sceSyscallEnter();
	struct AppState *app;
	int result, unlockResult;

	if (!sceSblACMgrIsSystemProgram(0)) {
		sceSyscallLeave(syscallState);
		return SCE_APPMGR_ERROR_NOEXEC;
	}
	result = _appListFindAndLockAppByName2(name, &app, -1);
	if (result != 0) {
		sceSyscallLeave(syscallState);
		return result;
	}
	if (g_appmgrDebugNotifier != NULL)
		g_appmgrDebugNotifier(4, app->appId, app->process.processId, 0);
	result = _appmgrProcessQuitProcess(app, 2, 0, 0);
	unlockResult = _appListUnlockApp(app);
	DELETE_APP_CHECK(unlockResult, 0, 4174, "sceAppMgrDestroyAppByName");
	sceSyscallLeave(syscallState);
	return result;
}

int sceAppMgrDestroyAppByAppId(int appId)
{
	SceUInt32 syscallState = sceSyscallEnter();
	struct AppState *app;
	int result, unlockResult;

	if (!sceSblACMgrIsSystemProgram(0)) {
		sceSyscallLeave(syscallState);
		return SCE_APPMGR_ERROR_NOEXEC;
	}
	result = _appListFindAndLockAppByAppId(appId, &app);
	if (result != 0) {
		sceSyscallLeave(syscallState);
		return result;
	}
	if (g_appmgrDebugNotifier != NULL)
		g_appmgrDebugNotifier(4, app->appId, app->process.processId, 0);
	result = _appmgrProcessQuitProcess(app, 2, 0, 0);
	unlockResult = _appListUnlockApp(app);
	DELETE_APP_CHECK(unlockResult, 0, 4208, "sceAppMgrDestroyAppByAppId");
	sceSyscallLeave(syscallState);
	return result;
}

/* A background or system application sets what happens to it when memory runs short. */
int sceAppMgrSetAppProtectionModeOnMemoryShortage(unsigned int mode)
{
	SceUInt32 syscallState = sceSyscallEnter();
	struct AppState *app;
	int result, unlockResult;

	if (!sceSblACMgrIsSystemProgram(0) && !SceSblACMgrForDriver_6C5AB07F(0)) {
		sceSyscallLeave(syscallState);
		return SCE_APPMGR_ERROR_NOEXEC;
	}
	result = _appListFindAndLockAppByProcessId(sceKernelGetProcessId(), &app);
	if (result != 0) {
		sceSyscallLeave(syscallState);
		return result;
	}
	GC_MAIN_LOCK(4233, "sceAppMgrSetAppProtectionModeOnMemoryShortage");
	if (app->launchInfo.budgetType == 0 && (app->launchInfo.processAttr & 3) != 0)
		app->protectionMode = mode;
	else
		result = (int)0x8080201c;
	DELETE_APP_MAIN_UNLOCK(4245, "sceAppMgrSetAppProtectionModeOnMemoryShortage");
	unlockResult = _appListUnlockApp(app);
	DELETE_APP_CHECK(unlockResult, 0, 4248, "sceAppMgrSetAppProtectionModeOnMemoryShortage");
	sceSyscallLeave(syscallState);
	return result;
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

/* The active application leaves the activation when the power tick state stops it. */
static inline __attribute__((always_inline)) void _updatePowerTickStateForActiveApp(void)
{
	struct AppState *app;
	int result;

	if (_appListFindAndLockActiveApp(&app) != 0)
		return;
	if (_updateAppOnPowerTickState(app) != 0) {
		GC_MAIN_LOCK(4351, "_updatePowerTickStateForActiveApp");
		_appListSetActiveness(app, 1);
		g_appmgrActiveAppId = -1;
		DELETE_APP_MAIN_UNLOCK(4354, "_updatePowerTickStateForActiveApp");
		result = _appListUnlockApp(app);
		DELETE_APP_CHECK(result, 0, 4357, "_updatePowerTickStateForActiveApp");
	} else {
		result = _appListUnlockApp(app);
		DELETE_APP_CHECK(result, 0, 4360, "_updatePowerTickStateForActiveApp");
		_updateDeviceArbitration2();
	}
}

/*
 * Applies the power tick state to the active application and, on a PS TV, to every
 * background application that may not be suspended.
 */
void _updatePowerTickStateForNonSuspendableApp(void)
{
	struct AppState *app;
	int index, result;

	_updatePowerTickStateForActiveApp();
	if (!SceSblAIMgrForDriver_71608CA3())
		return;
	index = appmgrGetShellApp() != NULL;
	app = &g_appmgrAppStates[index];
	do {
		GC_APP_LOCK_CHECK(4378, "_updatePowerTickStateForNonSuspendableApp");
		result = sceKernelLockMutex(app->mutex, 1, NULL);
		if (result != 0) {
			if ((unsigned int)(result + 0x7ffd7fd4) > 1)
				LAUNCH_CORE2_LOCK_FAILURE(result, 4378,
					"_updatePowerTickStateForNonSuspendableApp");
		} else {
			app->mutexOwner = sceKernelGetThreadId();
		}
		if ((app->launchInfo.processAttr & 2) && appmgrGetAppRunState(app) != 2)
			_updateAppOnPowerTickState(app);
		GC_APP_UNLOCK_CHECK(4388, "_updatePowerTickStateForNonSuspendableApp");
		app->mutexOwner = -1;
		result = sceKernelUnlockMutex(app->mutex, 1);
		GC_UNLOCK_CHECK(result, 4388, "_updatePowerTickStateForNonSuspendableApp");
		DELETE_APP_CHECK(result, 0, 4389, "_updatePowerTickStateForNonSuspendableApp");
		++index;
		++app;
	} while (index <= 19);
}

int sceAppMgrSendParam(const char *name, const char *param, const void *option)
{
	SceUInt32 syscallState = sceSyscallEnter();
	struct AppState *app;
	int result;

	if (!sceSblACMgrIsSystemProgram(0)) {
		sceSyscallLeave(syscallState);
		return SCE_APPMGR_ERROR_NOEXEC;
	}
	if (name == NULL) {
		sceSyscallLeave(syscallState);
		return SCE_APPMGR_ERROR_NULL_POINTER;
	}
	if (name[0] == '\0' || option != NULL) {
		sceSyscallLeave(syscallState);
		return SCE_APPMGR_ERROR_INVALID;
	}
	DELETE_APP_MAIN_LOCK(4438, "sceAppMgrSendParam");
	app = appmgrFindAppByName(g_appmgrAppStates, name);
	if (app == NULL) {
		DELETE_APP_MAIN_UNLOCK(4443, "sceAppMgrSendParam");
		sceSyscallLeave(syscallState);
		return (int)0x80802012;
	}
	result = appmgrParamQueuePush(&app->params, param, -1,
		sceKernelGetProcessId(), NULL);
	DELETE_APP_MAIN_UNLOCK(4451, "sceAppMgrSendParam");
	sceSyscallLeave(syscallState);
	return result;
}
