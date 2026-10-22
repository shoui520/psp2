/* Application launch and arbitration helpers. */

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
#include "appmgr_device.h"

extern SceUID g_appmgrMainMutexOwner;

struct AppState;
extern int _processUpdateThreadEntry(SceSize args, void *argp);
extern int _gcListenerThreadEntry(SceSize args, void *argp);
extern SceUID g_appmgrMainMutex;
extern SceUID g_appmgrCore2Mutex;
extern SceUID g_appmgrCore2MutexOwner;

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

void appmgrArbitrationPanic(void)
{
	SceDebugForDriver_391B5B74(&arbitrationPanicInfo, __builtin_return_address(0));
}

_Noreturn void appmgrGcListenerMutexPanic(void)
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

void sendResumeToPid(SceUID pid)
{
	struct AppState *app = NULL;
	struct AppState *lockedApp;
	SceAppMgrSystemEvent event;

	if (_appListFindAndLockAppByProcessId(pid, &app) != 0)
		return;
	lockedApp = app;
	if (lockedApp == NULL) {
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_CHECK_FUNC(0, 134, "sendResumeToPid");
		lockedApp = app;
	}
	memset(&event, 0, sizeof(event));
	event.systemEvent = SCE_APPMGR_SYSTEMEVENT_ON_RESUME;
	if (_appListPushEvent(lockedApp, &event) != 0) {
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_CHECK_FUNC(0, 139, "sendResumeToPid");
	}
	if (_appListUnlockApp(app) != 0) {
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_CHECK_FUNC(0, 142, "sendResumeToPid");
	}
}
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
