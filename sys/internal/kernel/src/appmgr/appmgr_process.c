/* Process event adapters. */

#include <processmgr/processmgr.h>
#include <sysmem/debug.h>
#include <threadmgr/threadmgr.h>
#include <appmgr.h>
#include <power/power.h>
#include <sysmem/procevent.h>
#include <string.h>
#include "appmgr_internal_util.h"
#include "app_state_internal.h"
#include <sysmem/sysroot.h>
#include <sysmem/sysclib.h>
#include <appmgr/appmgr.h>
#include <npdrm/npdrm.h>
#include <avcodec/avcodec.h>

extern int appmgrUsesGxmDebugger(SceUID pid);

extern int _appListFindAndLockAppByProcessId(SceUID pid, struct AppState **app);
extern int _appListFindAndLockAppByName(const char *appName, struct AppState **app);
extern int _suspendAppForDebugger(struct AppState *app,
				 struct AppmgrWaitObject **waitObject);
extern int _resumeAppForDebugger(struct AppState *app,
				struct AppmgrWaitObject **waitObject);
extern int _appListUnlockApp(struct AppState *app);
extern int dumpBgmState(void);
extern SceUID g_appmgrMainMutex;
extern SceUID g_appmgrMainMutexOwner;
extern void _shellEventOnAppUnloadedQueuePush(SceUID pid, int appId,
		int exitReason, SceUID sourcePid, int sourceAppId,
		int relaunchPending, const char *appName);
extern int _updateDeviceArbitration2(void);
extern int appmgrReturnSuccess(void);
extern int _sceAppMgrProcExitHandler(SceUID pid, ProcEventParam *param, void *common);
int appmgrProcKillHandler(SceUID pid, ProcEventParam *param, void *common);
extern int _sceAppMgrProcSuspendHandler(SceUID pid, int eventType,
		ProcEventParam *param, void *common);
extern int _sceAppMgrProcResumeHandler(SceUID pid, int eventType,
		ProcEventParam *param, void *common);
extern int appmgrSaveSafeMemoryForProcess(SceUID pid);
extern int appmgrUpdateSaveDataOnSuspend(SceUID pid, AppmgrProcessContent *content);
extern int appmgrPrepareSafeMemoryForProcess(SceUID pid);
extern int _appListPushEvent(struct AppState *app, const SceAppMgrSystemEvent *event);
extern void _deviceRefreshAudioPort(SceUID pid);
extern SceUID g_appmgrCore2Mutex;
extern SceUID g_appmgrCore2MutexOwner;
extern int appmgrResetSaveDataSlotCache(SceUID pid);
extern int appmgrRemoveContentCache(SceUID pid, const AppmgrProcessContent *content);
extern int appmgrRemoveTemporaryPhoto(SceUID pid);
extern int appmgrRemoveAppWorkDirectory(const AppmgrProcessContent *content);
extern int appmgrUnmountProcessContent(SceUID pid);
extern int _paramQueueCancelAll(AppmgrParamQueue *queue);
extern int _deviceSetRunStatus(int appId, int status);
extern int _deviceSetProcessId(int appId, SceUID pid);
extern int _destroyOtherApp(struct AppState *app);
extern void _shellEventAppRunningStatusChangedQueuePush(void);
extern void _shellEventDeactivateAppRequestQueuePush(int appId);
extern void _shellEventActivateAppRequestQueuePush(int appId);
extern void _shellEventOnAppUnloadingQueuePush(SceUID processId, int appId,
		int exitReason, int selfQuit, SceUID sourcePid, int sourceAppId,
		int relaunchPending, const char *appName);
extern int _sceAppMgrDeactivateAppInternal(struct AppState *app, int, int, int);
extern int _appmgrProcessKillProcess(struct AppState *app, int flags);
extern int _appListPushEvent(struct AppState *app,
			     const SceAppMgrSystemEvent *event);
extern int appmgrUsesDefaultLibrary(void);

SceUID g_appmgrVideoCodecPid = -1;
int (*g_appmgrProcTerminatedCallback)(struct AppState *app);
SceUID g_appmgrProcEventHandlerUid;

static const SceKernelProcEventHandler processEventHandlers = {
	.size = sizeof(SceKernelProcEventHandler),
	.create = (ProcEventCreateHandler)appmgrReturnSuccess,
	.exit = _sceAppMgrProcExitHandler,
	.kill = appmgrProcKillHandler,
	.suspend = _sceAppMgrProcSuspendHandler,
	.resume = _sceAppMgrProcResumeHandler,
	.notify = NULL,
};

#if !FW_DEBUG
static const SceKernelDebugInfo processPanicInfo = {
	.fileHash = 0xc9656e8e,
	.lineHash = 0xc879dadc,
	.funcHash = 0x11a05ee1,
};

void appmgrProcessPanic(void)
{
	SceDebugForDriver_391B5B74(&processPanicInfo, __builtin_return_address(0));
}

static const SceKernelDebugInfo videoCodecPanicInfo = {
	.fileHash = 0xc9656e8e,
	.lineHash = 0x79d3a1f0,
	.funcHash = 0x9d536bce,
};

static const SceKernelDebugInfo shellTerminatedPanicInfo = {
	.fileHash = 0xb763dfa2,
	.lineHash = 0xfd1bf80b,
	.funcHash = 0x2f19101f,
};

static const SceKernelDebugInfo terminatedRegistrationPanicInfo = {
	.fileHash = 0xb763dfa2, .lineHash = 0xa24ad712, .funcHash = 0x3b1076e5,
};
static const SceKernelDebugInfo processEventRegistrationPanicInfo = {
	.fileHash = 0xb763dfa2, .lineHash = 0x37587865, .funcHash = 0x3b1076e5,
};
static const SceKernelDebugInfo processWaitInitializationPanicInfo = {
	.fileHash = 0xb763dfa2, .lineHash = 0x101a3281, .funcHash = 0x3b1076e5,
};
static const SceKernelDebugInfo videoCodecRegistrationPanicInfo = {
	.fileHash = 0xb763dfa2, .lineHash = 0xc5987cf1, .funcHash = 0x3b1076e5,
};
static const SceKernelDebugInfo suspendGameExistPanicInfo = {
	.fileHash = 0xb763dfa2, .lineHash = 0xbad1fb69, .funcHash = 0x93d1f70f,
};
static const SceKernelDebugInfo resumeGameExistPanicInfo = {
	.fileHash = 0xb763dfa2, .lineHash = 0x4ca24511, .funcHash = 0x416820e1,
};

void vcStateChangedPanic(void)
{
	SceDebugForDriver_391B5B74(&videoCodecPanicInfo, __builtin_return_address(0));
}
#endif

#if FW_DEBUG
#define VC_LOCK_CHECK(_result, _line, _func) do { \
	if ((_result) != 0) { \
		SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, _line, _func, \
			"Failed to lock ret=0x%08x\n", (_result)); \
		sceAppMgrDump(); \
		dumpBgmState(); \
		SCE_DBG_ASSERT_CHECK_FUNC(0, _line, _func); \
	} else { \
		g_appmgrMainMutexOwner = sceKernelGetThreadId(); \
	} \
} while (0)

#define VC_UNLOCK_CHECK(_result, _line, _func) do { \
	if ((_result) != 0) { \
		sceAppMgrDump(); \
		dumpBgmState(); \
		SCE_DBG_ASSERT_PRINTF_FUNC((_result) == 0, _line, _func, \
			"[SceAppMgr] The actual value is %d(0x%08x), but expected value is %d(0x%08x)\n", \
			(_result), (_result), 0, 0); \
	} \
} while (0)

#define VC_RETURN_UNLOCK(_result, _line) do { \
	if ((_result) != 0) { \
		int assertLevel; \
		sceAppMgrDump(); \
		dumpBgmState(); \
		assertLevel = SceDebugForDriver_374B7868(); \
		if (assertLevel > 0) \
			return SceDebugForDriver_821A2D59(1, 0, \
				SCE_DBGINFO_FUNC(_line, "vcStateChanged"), \
				__builtin_return_address(0), \
				"[SceAppMgr] The actual value is %d(0x%08x), but expected value is %d(0x%08x)\n", \
				(_result), (_result), 0, 0); \
		return assertLevel; \
	} \
	return (_result); \
} while (0)
#else
#define VC_LOCK_CHECK(_result, _line, _func) do { \
	if ((_result) != 0) \
		vcStateChangedPanic(); \
	else \
		g_appmgrMainMutexOwner = sceKernelGetThreadId(); \
} while (0)
#define VC_UNLOCK_CHECK(_result, _line, _func) do { \
	if ((_result) != 0) \
		vcStateChangedPanic(); \
} while (0)
#define VC_RETURN_UNLOCK(_result, _line) do { \
	VC_UNLOCK_CHECK(_result, _line, "vcStateChanged"); \
	return (_result); \
} while (0)
#endif

int vcStateChanged(SceUID pid, int enabled)
{
	struct AppState *app;
	SceUID ownerPid, evictPid;
	unsigned int count;
	int index, value, leastValue, result, usage, retry;

	/* Sony inlines vcUsingProcessCountCheck here. */
	result = sceKernelLockMutex(g_appmgrMainMutex, 1, NULL);
	VC_LOCK_CHECK(result, 1330, "vcUsingProcessCountCheck");
	app = appmgrFindAppByProcessId(g_appmgrAppStates, pid);
	if (app != NULL && app->videoCodecUsage != 0) {
		index = appmgrGetShellApp() != NULL;
		count = 0;
		leastValue = 0;
		evictPid = -1;
		do {
			struct AppState *candidate = &g_appmgrAppStates[index];

			__builtin_prefetch(&candidate[7].process.processId);
			if (candidate->process.processId >= 0 &&
			    candidate->process.processId != pid &&
			    candidate->videoCodecUsage != 0) {
				++count;
				if (candidate->budgetType == 0) {
					value = getMiniAppValue(candidate);
					if (evictPid == -1 || leastValue > value) {
						leastValue = value;
						evictPid = candidate->process.processId;
					}
				}
			}
		} while (++index <= 19);
		g_appmgrMainMutexOwner = -1;
		result = sceKernelUnlockMutex(g_appmgrMainMutex, 1);
		VC_UNLOCK_CHECK(result, 1369, "vcUsingProcessCountCheck");
		if (count > 3 && evictPid != -1) {
			SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 1372,
				"vcUsingProcessCountCheck",
				"[SceAppMgr] Killing Process 0x%08x beacuse more than 4 app using video codec.\n",
				evictPid);
			sceKernelKillProcess(evictPid, 0);
		}
	} else {
		g_appmgrMainMutexOwner = -1;
		result = sceKernelUnlockMutex(g_appmgrMainMutex, 1);
		if (app == NULL)
			VC_UNLOCK_CHECK(result, 1333, "vcUsingProcessCountCheck");
		else
			VC_UNLOCK_CHECK(result, 1338, "vcUsingProcessCountCheck");
	}

	for (;;) {
		result = sceKernelLockMutex(g_appmgrMainMutex, 1, NULL);
		VC_LOCK_CHECK(result, 1385, "vcStateChanged");
		app = appmgrFindAppByProcessId(g_appmgrAppStates, pid);
		if (app == NULL) {
			g_appmgrMainMutexOwner = -1;
			result = sceKernelUnlockMutex(g_appmgrMainMutex, 1);
			VC_RETURN_UNLOCK(result, 1388);
		}
		usage = app->videoCodecUsage;
		if (app->budgetType == 1) {
			if ((app->processAttr & 1) == 0)
				usage = enabled;
		} else if (app->budgetType == 0) {
			if ((app->processAttr & 3) == 0)
				usage = enabled != 0 && usage != 0;
		} else {
			usage = 0;
		}
		if (usage == 0) {
			if (g_appmgrVideoCodecPid == pid)
				g_appmgrVideoCodecPid = -1;
			g_appmgrMainMutexOwner = -1;
			result = sceKernelUnlockMutex(g_appmgrMainMutex, 1);
			VC_RETURN_UNLOCK(result, 1433);
		}
		ownerPid = g_appmgrVideoCodecPid;
		if (ownerPid == -1) {
			g_appmgrVideoCodecPid = pid;
			g_appmgrMainMutexOwner = -1;
			result = sceKernelUnlockMutex(g_appmgrMainMutex, 1);
			VC_RETURN_UNLOCK(result, 1426);
		}
		g_appmgrMainMutexOwner = -1;
		if (ownerPid == pid) {
			result = sceKernelUnlockMutex(g_appmgrMainMutex, 1);
			VC_RETURN_UNLOCK(result, 1420);
		}
		result = sceKernelUnlockMutex(g_appmgrMainMutex, 1);
		VC_UNLOCK_CHECK(result, 1436, "vcStateChanged");
		retry = 9;
		sceKernelDelayThread(100000);
		while (ownerPid == g_appmgrVideoCodecPid) {
			sceKernelDelayThread(100000);
			if (--retry != 0)
				continue;
			if (ownerPid != g_appmgrVideoCodecPid)
				break;
			SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 1451, "vcStateChanged",
				"-------------------------------------------------------------------------------\n");
			SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 1452, "vcStateChanged",
				"-------------------------------------------------------------------------------\n");
			SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 1453, "vcStateChanged",
				"[SceAppMgr:Error] ProcessID 0x%08x is killed because of Video Codec arbitration\n",
				ownerPid);
			SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 1454, "vcStateChanged",
				"-------------------------------------------------------------------------------\n");
			SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 1455, "vcStateChanged",
				"-------------------------------------------------------------------------------\n");
			sceKernelKillProcess(ownerPid, 0);
			result = sceKernelLockMutex(g_appmgrMainMutex, 1, NULL);
			if (result != 0) {
				VC_LOCK_CHECK(result, 1457, "vcStateChanged");
			} else {
				sceKernelGetThreadId();
			}
			if (ownerPid == g_appmgrVideoCodecPid)
				g_appmgrVideoCodecPid = -1;
			g_appmgrMainMutexOwner = -1;
			result = sceKernelUnlockMutex(g_appmgrMainMutex, 1);
			VC_UNLOCK_CHECK(result, 1461, "vcStateChanged");
			break;
		}
	}
}

#undef VC_LOCK_CHECK
#undef VC_UNLOCK_CHECK
#undef VC_RETURN_UNLOCK

int _sceAppMgrProcTerminatedHandler(SceUID pid)
{
	struct AppState *app, *shell;
	SceUID shellPid;
	int alreadyLocked, result;

	app = appmgrFindAppByProcessId(g_appmgrAppStates, pid);
	if (app != NULL && app->mutexOwner == sceKernelGetThreadId()) {
		alreadyLocked = 1;
	} else {
		result = _appListFindAndLockAppByProcessId(pid, &app);
		if (result == (int)0x80802012)
			return 0;
#if FW_DEBUG
		if (result != 0) {
			sceAppMgrDump();
			dumpBgmState();
			SCE_DBG_ASSERT_PRINTF_FUNC(result == 0, 324,
				"_sceAppMgrProcTerminatedHandler",
				"[SceAppMgr] The actual value is %d(0x%08x), but expected value is %d(0x%08x)\n",
				result, result, 0, 0);
		}
#else
		if (result != 0)
			appmgrProcessPanic();
#endif
		alreadyLocked = 0;
	}
	if (app == NULL || app->mutexOwner != sceKernelGetThreadId()) {
#if FW_DEBUG
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_CHECK_FUNC(0, 328, "_sceAppMgrProcTerminatedHandler");
#else
		appmgrProcessPanic();
#endif
	}
	result = sceKernelLockMutex(g_appmgrMainMutex, 1, NULL);
#if FW_DEBUG
	if (result != 0) {
		SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 330, "_sceAppMgrProcTerminatedHandler",
			"Failed to lock ret=0x%08x\n", result);
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_CHECK_FUNC(0, 330, "_sceAppMgrProcTerminatedHandler");
	} else {
		g_appmgrMainMutexOwner = sceKernelGetThreadId();
	}
#else
	if (result != 0)
		vcStateChangedPanic();
	g_appmgrMainMutexOwner = sceKernelGetThreadId();
#endif
	shell = appmgrGetShellApp();
	shellPid = shell != NULL ? shell->process.processId : 0;
	_appListSetRunStatus(app, 0x500);
	_appListSetRunStatus(app, 0x800);
	_shellEventOnAppUnloadedQueuePush(app->process.processId, app->appId,
		app->exitReason,
		(app->launchMode & 0x40000) ? app->launchSourceProcessId : -1,
		(app->launchMode & 0x40000) ? app->launchSourceAppId : -1,
		(short)app->relaunchPending, app->process.appName);
	g_appmgrMainMutexOwner = -1;
	result = sceKernelUnlockMutex(g_appmgrMainMutex, 1);
#if FW_DEBUG
	if (result != 0) {
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_PRINTF_FUNC(result == 0, 356,
			"_sceAppMgrProcTerminatedHandler",
			"[SceAppMgr] The actual value is %d(0x%08x), but expected value is %d(0x%08x)\n",
			result, result, 0, 0);
	}
#else
	if (result != 0)
		vcStateChangedPanic();
#endif
	if (g_appmgrProcTerminatedCallback != NULL)
		g_appmgrProcTerminatedCallback(app);
	if (!alreadyLocked) {
		result = _appListUnlockApp(app);
#if FW_DEBUG
		if (result != 0) {
			sceAppMgrDump();
			dumpBgmState();
			SCE_DBG_ASSERT_PRINTF_FUNC(result == 0, 365,
				"_sceAppMgrProcTerminatedHandler",
				"[SceAppMgr] The actual value is %d(0x%08x), but expected value is %d(0x%08x)\n",
				result, result, 0, 0);
		}
#else
		if (result != 0)
			appmgrProcessPanic();
#endif
	}
	_updateDeviceArbitration2();
	if (pid == shellPid) {
		SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 372, "_sceAppMgrProcTerminatedHandler",
			"Shell is terminated! Reset.\n");
		result = scePowerRequestStandby();
		if (result != 0) {
#if FW_DEBUG
			sceAppMgrDump();
			dumpBgmState();
			SCE_DBG_ASSERT_CHECK_FUNC(0, 375, "_sceAppMgrProcTerminatedHandler");
#else
			SceDebugForDriver_391B5B74(&shellTerminatedPanicInfo,
				__builtin_return_address(0));
#endif
		}
	}
	return 0;
}

#if FW_DEBUG
#define PROCESS_SUSPEND_RESULT(_result, _line) do { \
	if ((_result) != 0) { \
		sceAppMgrDump(); \
		dumpBgmState(); \
		SCE_DBG_ASSERT_PRINTF_FUNC((_result) == 0, _line, \
			"_sceAppMgrProcSuspendHandler", \
			"[SceAppMgr] The actual value is %d(0x%08x), but expected value is %d(0x%08x)\n", \
			(_result), (_result), 0, 0); \
	} \
} while (0)
#else
#define PROCESS_SUSPEND_RESULT(_result, _line) do { \
	if ((_result) != 0) \
		appmgrProcessPanic(); \
} while (0)
#endif

int _sceAppMgrProcSuspendHandler(SceUID pid, int eventType,
		ProcEventParam *param, void *common)
{
	struct AppState *app, *lockedApp;
	struct AppmgrWaitObject *waiter;
	SceUID owner;
	int result;

	(void)param;
	(void)common;
	if (eventType == 0x800) {
		appmgrSaveSafeMemoryForProcess(pid);
		if (_appListFindAndLockAppByProcessId(pid, &app) == (int)0x80802012)
			return 0;
		appmgrUpdateSaveDataOnSuspend(pid, &app->process.content);
		if ((app->process.content.contentFlags & 0x30) != 0) {
			result = SceNpDrmForDriver_3BFD2850(0);
#if FW_DEBUG
			PROCESS_SUSPEND_RESULT(result, 216);
#else
			if (result != 0)
				SceDebugForDriver_00CCE39C(&suspendGameExistPanicInfo,
					__builtin_return_address(0),
					"0x%08x,0x%08x\n", result, 0);
#endif
		}
		result = _appListUnlockApp(app);
		PROCESS_SUSPEND_RESULT(result, 220);
	} else if (eventType == 0x1000) {
		if (_appListFindAndLockAppByProcessId(pid, &app) == (int)0x80802012)
			return 0;
		result = sceKernelLockMutex(g_appmgrMainMutex, 1, NULL);
#if FW_DEBUG
		if (result != 0) {
			SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 232, "_sceAppMgrProcSuspendHandler",
				"Failed to lock ret=0x%08x\n", result);
			sceAppMgrDump();
			dumpBgmState();
			SCE_DBG_ASSERT_CHECK_FUNC(0, 232, "_sceAppMgrProcSuspendHandler");
			owner = g_appmgrMainMutexOwner;
		} else {
			owner = sceKernelGetThreadId();
			g_appmgrMainMutexOwner = owner;
		}
#else
		if (result != 0)
			vcStateChangedPanic();
		owner = sceKernelGetThreadId();
		g_appmgrMainMutexOwner = owner;
#endif
		lockedApp = app;
		lockedApp->process.notifiedProcessState = 3;
		/* Sony inlines sceAppMgrProcessNotifySuspendWaiter here. */
		if (owner != sceKernelGetThreadId()) {
#if FW_DEBUG
			sceAppMgrDump();
			dumpBgmState();
			SCE_DBG_ASSERT_CHECK_FUNC(0, 1585, "sceAppMgrProcessNotifySuspendWaiter");
#else
			appmgrProcessPanic();
#endif
		}
		for (waiter = lockedApp->process.suspendWaiters; waiter != NULL;
		     waiter = waiter->next)
			appmgrUtilWaitObjectNotify(waiter);
		lockedApp->process.suspendWaiters = NULL;
		g_appmgrMainMutexOwner = -1;
		result = sceKernelUnlockMutex(g_appmgrMainMutex, 1);
#if FW_DEBUG
		PROCESS_SUSPEND_RESULT(result, 237);
#else
		if (result != 0)
			vcStateChangedPanic();
#endif
		result = _appListUnlockApp(app);
		PROCESS_SUSPEND_RESULT(result, 240);
	}
	return 0;
}

#undef PROCESS_SUSPEND_RESULT

int videocodecUsageChangedCallback2(SceUID pid, unsigned int usage)
{
	struct AppState *app;
	int result;

	result = sceKernelLockMutex(g_appmgrMainMutex, 1, NULL);
#if FW_DEBUG
	if (result != 0) {
		SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 1475, "videocodecUsageChangedCallback2",
			"Failed to lock ret=0x%08x\n", result);
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_CHECK_FUNC(0, 1475, "videocodecUsageChangedCallback2");
	} else {
		g_appmgrMainMutexOwner = sceKernelGetThreadId();
	}
#else
	if (result != 0)
		vcStateChangedPanic();
	g_appmgrMainMutexOwner = sceKernelGetThreadId();
#endif
	app = appmgrFindAppByProcessId(g_appmgrAppStates, pid);
	if (app != NULL) {
		app->videoCodecUsage = usage;
		g_appmgrMainMutexOwner = -1;
		result = sceKernelUnlockMutex(g_appmgrMainMutex, 1);
#if FW_DEBUG
		if (result != 0) {
			sceAppMgrDump();
			dumpBgmState();
			SCE_DBG_ASSERT_PRINTF_FUNC(result == 0, 1482,
				"videocodecUsageChangedCallback2",
				"[SceAppMgr] The actual value is %d(0x%08x), but expected value is %d(0x%08x)\n",
				result, result, 0, 0);
		}
#else
		if (result != 0)
			vcStateChangedPanic();
#endif
		return vcStateChanged(pid, 1);
	}
	g_appmgrMainMutexOwner = -1;
	result = sceKernelUnlockMutex(g_appmgrMainMutex, 1);
	if (result != 0) {
#if FW_DEBUG
		int assertLevel;
		sceAppMgrDump();
		dumpBgmState();
		assertLevel = SceDebugForDriver_374B7868();
		if (assertLevel > 0)
			return SceDebugForDriver_821A2D59(1, 0,
				SCE_DBGINFO_FUNC(1478, "videocodecUsageChangedCallback2"),
				__builtin_return_address(0),
				"[SceAppMgr] The actual value is %d(0x%08x), but expected value is %d(0x%08x)\n",
				result, result, 0, 0);
		return assertLevel;
#else
		vcStateChangedPanic();
#endif
	}
	return result;
}

#if FW_DEBUG
#define PROCESS_RESUME_RESULT(_result, _line) do { \
	if ((_result) != 0) { \
		sceAppMgrDump(); \
		dumpBgmState(); \
		SCE_DBG_ASSERT_PRINTF_FUNC((_result) == 0, _line, \
			"_sceAppMgrProcResumeHandler", \
			"[SceAppMgr] The actual value is %d(0x%08x), but expected value is %d(0x%08x)\n", \
			(_result), (_result), 0, 0); \
	} \
} while (0)
#else
#define PROCESS_RESUME_RESULT(_result, _line) do { \
	if ((_result) != 0) \
		appmgrProcessPanic(); \
} while (0)
#endif

int _sceAppMgrProcResumeHandler(SceUID pid, int eventType,
		ProcEventParam *param, void *common)
{
	struct AppState *app, *lockedApp;
	struct AppmgrWaitObject *waiter;
	SceAppMgrSystemEvent event;
	SceUID owner;
	int result;

	(void)param;
	(void)common;
	if (eventType == 0x10000) {
		appmgrPrepareSafeMemoryForProcess(pid);
	} else if (eventType == 0x40000) {
		if (_appListFindAndLockAppByProcessId(pid, &app) == (int)0x80802012)
			return 0;
		result = sceKernelLockMutex(g_appmgrMainMutex, 1, NULL);
#if FW_DEBUG
		if (result != 0) {
			SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 265, "_sceAppMgrProcResumeHandler",
				"Failed to lock ret=0x%08x\n", result);
			sceAppMgrDump();
			dumpBgmState();
			SCE_DBG_ASSERT_CHECK_FUNC(0, 265, "_sceAppMgrProcResumeHandler");
		} else {
			g_appmgrMainMutexOwner = sceKernelGetThreadId();
		}
#else
		if (result != 0)
			vcStateChangedPanic();
		g_appmgrMainMutexOwner = sceKernelGetThreadId();
#endif
		if ((app->process.content.contentFlags & 0x30) != 0) {
			result = SceNpDrmForDriver_3BFD2850(1);
#if FW_DEBUG
			PROCESS_RESUME_RESULT(result, 271);
#else
			if (result != 0)
				SceDebugForDriver_00CCE39C(&resumeGameExistPanicInfo,
					__builtin_return_address(0),
					"0x%08x,0x%08x\n", result, 0);
#endif
		}
		event.systemEvent = SCE_APPMGR_SYSTEMEVENT_ON_RESUME;
		app->process.notifiedProcessState = 2;
		result = _appListPushEvent(app, &event);
		if (result != 0) {
#if FW_DEBUG
			sceAppMgrDump();
			dumpBgmState();
			SCE_DBG_ASSERT_CHECK_FUNC(0, 279, "_sceAppMgrProcResumeHandler");
#else
			appmgrProcessPanic();
#endif
		}
		/* Sony inlines sceAppMgrProcessNotifyRunningWaiter here. */
		owner = g_appmgrMainMutexOwner;
		lockedApp = app;
		if (owner != sceKernelGetThreadId()) {
#if FW_DEBUG
			sceAppMgrDump();
			dumpBgmState();
			SCE_DBG_ASSERT_CHECK_FUNC(0, 1611, "sceAppMgrProcessNotifyRunningWaiter");
#else
			appmgrProcessPanic();
#endif
		}
		for (waiter = lockedApp->process.runningWaiters; waiter != NULL;
		     waiter = waiter->next)
			appmgrUtilWaitObjectNotify(waiter);
		lockedApp->process.runningWaiters = NULL;
		g_appmgrMainMutexOwner = -1;
		result = sceKernelUnlockMutex(g_appmgrMainMutex, 1);
#if FW_DEBUG
		PROCESS_RESUME_RESULT(result, 283);
#else
		if (result != 0)
			vcStateChangedPanic();
#endif
		result = _appListUnlockApp(app);
		PROCESS_RESUME_RESULT(result, 286);
		_deviceRefreshAudioPort(pid);
	}
	return 0;
}

#undef PROCESS_RESUME_RESULT

#if FW_DEBUG
#define PROCESS_EXIT_RESULT(_result, _line, _func) do { \
	if ((_result) != 0) { \
		sceAppMgrDump(); \
		dumpBgmState(); \
		SCE_DBG_ASSERT_PRINTF_FUNC((_result) == 0, _line, _func, \
			"[SceAppMgr] The actual value is %d(0x%08x), but expected value is %d(0x%08x)\n", \
			(_result), (_result), 0, 0); \
	} \
} while (0)
#define PROCESS_EXIT_ASSERT(_condition, _line, _func) do { \
	if (!(_condition)) { \
		sceAppMgrDump(); \
		dumpBgmState(); \
		SCE_DBG_ASSERT_CHECK_FUNC(0, _line, _func); \
	} \
} while (0)
#define PROCESS_EXIT_LOCK(_result, _line, _func) do { \
	if ((_result) != 0) { \
		SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, _line, _func, \
			"Failed to lock ret=0x%08x\n", (_result)); \
		PROCESS_EXIT_ASSERT(0, _line, _func); \
	} else { \
		g_appmgrMainMutexOwner = sceKernelGetThreadId(); \
	} \
} while (0)
#else
#define PROCESS_EXIT_RESULT(_result, _line, _func) do { \
	if ((_result) != 0) \
		appmgrProcessPanic(); \
} while (0)
#define PROCESS_EXIT_ASSERT(_condition, _line, _func) do { \
	if (!(_condition)) \
		appmgrProcessPanic(); \
} while (0)
#define PROCESS_EXIT_LOCK(_result, _line, _func) do { \
	if ((_result) != 0) \
		vcStateChangedPanic(); \
	else \
		g_appmgrMainMutexOwner = sceKernelGetThreadId(); \
} while (0)
#endif

int _sceAppMgrProcExitHandler(SceUID pid, ProcEventParam *param, void *common)
{
	struct AppState *app, *codecApp, *lockedApp, *shellApp;
	struct AppmgrWaitObject *waitObject;
	SceUID owner;
	int result, appId;
#if FW_DEBUG
	int index;
#endif

	(void)param;
	(void)common;
	/* Sony inlines videocodecUsageChangedCallback2 here. */
	result = sceKernelLockMutex(g_appmgrMainMutex, 1, NULL);
	PROCESS_EXIT_LOCK(result, 1475, "videocodecUsageChangedCallback2");
	codecApp = appmgrFindAppByProcessId(g_appmgrAppStates, pid);
	if (codecApp != NULL) {
		codecApp->videoCodecUsage = 0;
		g_appmgrMainMutexOwner = -1;
		result = sceKernelUnlockMutex(g_appmgrMainMutex, 1);
#if FW_DEBUG
		PROCESS_EXIT_RESULT(result, 1482, "videocodecUsageChangedCallback2");
#else
		if (result != 0)
			vcStateChangedPanic();
#endif
		vcStateChanged(pid, 0);
	} else {
		g_appmgrMainMutexOwner = -1;
		result = sceKernelUnlockMutex(g_appmgrMainMutex, 1);
#if FW_DEBUG
		PROCESS_EXIT_RESULT(result, 1478, "videocodecUsageChangedCallback2");
#else
		if (result != 0)
			vcStateChangedPanic();
#endif
	}
	result = _appListFindAndLockAppByProcessId(pid, &app);
	if (result == (int)0x80802012)
		return 0;
	PROCESS_EXIT_RESULT(result, 92, "_sceAppMgrProcExitHandler");
	appmgrResetSaveDataSlotCache(pid);
	appmgrRemoveContentCache(pid, &app->process.content);
	appmgrRemoveTemporaryPhoto(pid);
	appmgrRemoveAppWorkDirectory(&app->process.content);
	result = appmgrUnmountProcessContent(pid);
	if (result != 0) {
		PROCESS_EXIT_ASSERT(result >= 0, 122, "_sceAppMgrProcExitHandler");
		result = _appListUnlockApp(app);
		PROCESS_EXIT_RESULT(result, 125, "_sceAppMgrProcExitHandler");
		return result;
	}
	result = sceKernelLockMutex(g_appmgrMainMutex, 1, NULL);
	PROCESS_EXIT_LOCK(result, 130, "_sceAppMgrProcExitHandler");
	result = _paramQueueCancelAll(&app->params);
	PROCESS_EXIT_RESULT(result, 133, "_sceAppMgrProcExitHandler");
	_appListSetRunStatus(app, 0x80);
	_deviceSetRunStatus(app->appId, 0);
	owner = g_appmgrMainMutexOwner;
	lockedApp = app;
	PROCESS_EXIT_ASSERT(owner == sceKernelGetThreadId(), 1611,
			    "sceAppMgrProcessNotifyRunningWaiter");
	for (waitObject = lockedApp->process.runningWaiters;
	     waitObject != NULL; waitObject = waitObject->next)
		appmgrUtilWaitObjectNotify(waitObject);
	owner = g_appmgrMainMutexOwner;
	lockedApp->process.runningWaiters = NULL;
	lockedApp = app;
	PROCESS_EXIT_ASSERT(owner == sceKernelGetThreadId(), 1585,
			    "sceAppMgrProcessNotifySuspendWaiter");
	for (waitObject = lockedApp->process.suspendWaiters;
	     waitObject != NULL; waitObject = waitObject->next)
		appmgrUtilWaitObjectNotify(waitObject);
	appId = app->appId;
	lockedApp->process.suspendWaiters = NULL;
	g_appmgrMainMutexOwner = -1;
	result = sceKernelUnlockMutex(g_appmgrMainMutex, 1);
#if FW_DEBUG
	PROCESS_EXIT_RESULT(result, 146, "_sceAppMgrProcExitHandler");
#else
	if (result != 0)
		vcStateChangedPanic();
#endif
	_deviceSetProcessId(appId, -1);
	_updateDeviceArbitration2();
	result = _appListUnlockApp(app);
	PROCESS_EXIT_RESULT(result, 152, "_sceAppMgrProcExitHandler");
	result = sceKernelLockMutex(g_appmgrMainMutex, 1, NULL);
	PROCESS_EXIT_LOCK(result, 155, "_sceAppMgrProcExitHandler");
	shellApp = appmgrGetShellApp();
	g_appmgrMainMutexOwner = -1;
	result = sceKernelUnlockMutex(g_appmgrMainMutex, 1);
#if FW_DEBUG
	PROCESS_EXIT_RESULT(result, 157, "_sceAppMgrProcExitHandler");
#else
	if (result != 0)
		vcStateChangedPanic();
#endif
	if (app != shellApp)
		return 0;
#if FW_DEBUG
	for (index = 0; index != 20; ++index) {
		if (SceDebugForDriver_374B7868() > 0) {
			__builtin_prefetch(&g_appmgrAppStates[index + 5].mutexOwner);
			owner = g_appmgrAppStates[index].mutexOwner;
			SCE_DBG_ASSERT_CHECK_FUNC(owner != sceKernelGetThreadId(),
				161, "_sceAppMgrProcExitHandler");
		}
	}
#endif
	result = sceKernelLockMutex(g_appmgrCore2Mutex, 1, NULL);
	if (result != 0) {
#if FW_DEBUG
		PROCESS_EXIT_RESULT((unsigned int)(result + 0x7ffd7fd4) <= 1 ?
				    0 : result, 161, "_sceAppMgrProcExitHandler");
		PROCESS_EXIT_RESULT(result, 162, "_sceAppMgrProcExitHandler");
#else
		if ((unsigned int)(result + 0x7ffd7fd4) > 1)
			vcStateChangedPanic();
		appmgrProcessPanic();
#endif
	} else {
		g_appmgrCore2MutexOwner = sceKernelGetThreadId();
	}
	result = sceKernelLockMutex(g_appmgrMainMutex, 1, NULL);
	PROCESS_EXIT_LOCK(result, 163, "_sceAppMgrProcExitHandler");
	result = _destroyOtherApp(app);
	PROCESS_EXIT_RESULT(result, 166, "_sceAppMgrProcExitHandler");
	g_appmgrMainMutexOwner = -1;
	result = sceKernelUnlockMutex(g_appmgrMainMutex, 1);
#if FW_DEBUG
	PROCESS_EXIT_RESULT(result, 168, "_sceAppMgrProcExitHandler");
#else
	if (result != 0)
		vcStateChangedPanic();
#endif
	g_appmgrCore2MutexOwner = -1;
	result = sceKernelUnlockMutex(g_appmgrCore2Mutex, 1);
#if FW_DEBUG
	PROCESS_EXIT_RESULT(result, 169, "_sceAppMgrProcExitHandler");
#else
	if (result != 0)
		vcStateChangedPanic();
#endif
	return 0;
}

int appmgrProcKillHandler(SceUID pid, ProcEventParam *param, void *common)
{
	(void)param;
	(void)common;
	return _sceAppMgrProcExitHandler(pid, NULL, NULL);
}

#if FW_DEBUG
#define PROCESS_SUSPEND_RESULT(_result, _line) \
	PROCESS_EXIT_RESULT(_result, _line, "_appmgrProcessSuspendProcess")
#else
#define PROCESS_SUSPEND_RESULT(_result, _line) do { \
	if ((_result) != 0) \
		vcStateChangedPanic(); \
} while (0)
#endif

int _appmgrProcessSuspendProcess(struct AppState *app, int flags,
				struct AppmgrWaitObject *waiter)
{
	struct AppmgrWaitObject **tail;
	SceUID owner;
	int result;
	int suspendResult;
	SceUInt32 state;

	if (app == NULL) {
		PROCESS_EXIT_ASSERT(0, 681, "_appmgrProcessSuspendProcess");
	} else {
		owner = app->mutexOwner;
		PROCESS_EXIT_ASSERT(owner == sceKernelGetThreadId(), 681,
			"_appmgrProcessSuspendProcess");
	}
	owner = g_appmgrMainMutexOwner;
	PROCESS_EXIT_ASSERT(owner != sceKernelGetThreadId(), 682,
		"_appmgrProcessSuspendProcess");
	result = sceKernelLockMutex(g_appmgrMainMutex, 1, NULL);
	PROCESS_EXIT_LOCK(result, 684, "_appmgrProcessSuspendProcess");
	state = app->process.requestedProcessState;
	if (state == 1) {
		g_appmgrMainMutexOwner = -1;
		result = sceKernelUnlockMutex(g_appmgrMainMutex, 1);
		PROCESS_SUSPEND_RESULT(result, 688);
		return 0;
	}
	_deviceSetRunStatus(app->appId, 0);
	app->process.requestedProcessState = 3;
	g_appmgrMainMutexOwner = -1;
	result = sceKernelUnlockMutex(g_appmgrMainMutex, 1);
	PROCESS_SUSPEND_RESULT(result, 696);
	if (app->launchOrigin == 2) {
		result = sceKernelWaitProcessEventCompletion(app->process.processId,
			2, NULL);
#if FW_DEBUG
		if (result < 0)
			SCE_DBG_PRINTF_FUNC(0, 702, "_appmgrProcessSuspendProcess",
				"[SceAppMgr] Suspend process : sceKernelWaitProcessEventCompletion() was faild with return_code(0x%08x)\n",
				result);
#endif
	}
	owner = app->process.processId;
	PROCESS_EXIT_ASSERT(owner != sceKernelGetProcessId(), 706,
		"_appmgrProcessSuspendProcess");
	suspendResult = sceKernelSuspendProcess(app->process.processId, flags, NULL);
	if (suspendResult != 0 &&
	    ((unsigned int)suspendResult & ~8u) != 0x80029004 &&
	    suspendResult != (int)0x80029001) {
#if FW_DEBUG
		PROCESS_EXIT_ASSERT(0, 709, "_appmgrProcessSuspendProcess");
#else
		SCE_PANIC_FUNC(709, "_appmgrProcessSuspendProcess");
#endif
	}
	SCE_DBG_PRINTF_FUNC(0, 710, "_appmgrProcessSuspendProcess",
		"[SceAppMgr] Suspend process : %s, appid=0x%08x, pid=0x%08x.\n",
		app->process.appName, app->appId, app->process.processId);
	if (waiter != NULL) {
		if (suspendResult != 0) {
			appmgrUtilWaitObjectNotify(waiter);
		} else {
			result = sceKernelLockMutex(g_appmgrMainMutex, 1, NULL);
			PROCESS_EXIT_LOCK(result, 713, "_appmgrProcessSuspendProcess");
			owner = g_appmgrMainMutexOwner;
			PROCESS_EXIT_ASSERT(owner == sceKernelGetThreadId(), 1572,
				"sceAppMgrProcessAddSuspendWaiter");
			waiter->next = NULL;
			tail = &app->process.suspendWaiters;
			while (*tail != NULL)
				tail = &(*tail)->next;
			*tail = waiter;
			g_appmgrMainMutexOwner = -1;
			result = sceKernelUnlockMutex(g_appmgrMainMutex, 1);
			PROCESS_SUSPEND_RESULT(result, 715);
		}
	}
	if (app->process.requestedProcessState == 2)
		_shellEventAppRunningStatusChangedQueuePush();
	vcStateChanged(app->process.processId, 0);
	return 0;
}

int _suspendAppForDebugger(struct AppState *app,
			   struct AppmgrWaitObject **waitObject)
{
	struct AppmgrWaitObject *waiter;
	struct AppmgrWaitObject **tail;
	SceUInt32 attributes;
	SceUInt32 state;
	SceUID owner;
	int result;
	int suspendResult;

	*waitObject = NULL;
	if (appmgrGetShellApp() == NULL) {
		result = _appmgrProcessSuspendProcess(app, 0, NULL);
		PROCESS_EXIT_RESULT(result, 1068, "_suspendAppForDebugger");
		return 0;
	}
	attributes = app->processAttr;
	if ((attributes & 3) != 0) {
		/* Sony inlines the existing suspend operation with no waiter. */
		owner = app->mutexOwner;
		PROCESS_EXIT_ASSERT(owner == sceKernelGetThreadId(), 681,
			"_appmgrProcessSuspendProcess");
		owner = g_appmgrMainMutexOwner;
		PROCESS_EXIT_ASSERT(owner != sceKernelGetThreadId(), 682,
			"_appmgrProcessSuspendProcess");
		result = sceKernelLockMutex(g_appmgrMainMutex, 1, NULL);
		PROCESS_EXIT_LOCK(result, 684, "_appmgrProcessSuspendProcess");
		state = app->process.requestedProcessState;
		if (state == 1) {
			g_appmgrMainMutexOwner = -1;
			result = sceKernelUnlockMutex(g_appmgrMainMutex, 1);
			PROCESS_SUSPEND_RESULT(result, 688);
		} else {
			_deviceSetRunStatus(app->appId, 0);
			app->process.requestedProcessState = 3;
			g_appmgrMainMutexOwner = -1;
			result = sceKernelUnlockMutex(g_appmgrMainMutex, 1);
			PROCESS_SUSPEND_RESULT(result, 696);
			if (app->launchOrigin == 2) {
				result = sceKernelWaitProcessEventCompletion(
					app->process.processId, 2, NULL);
#if FW_DEBUG
				if (result < 0)
					SCE_DBG_PRINTF_FUNC(0, 702,
						"_appmgrProcessSuspendProcess",
						"[SceAppMgr] Suspend process : sceKernelWaitProcessEventCompletion() was faild with return_code(0x%08x)\n",
						result);
#endif
			}
			owner = app->process.processId;
			PROCESS_EXIT_ASSERT(owner != sceKernelGetProcessId(), 706,
				"_appmgrProcessSuspendProcess");
			suspendResult = sceKernelSuspendProcess(
				app->process.processId, 0, NULL);
			if (suspendResult != 0 &&
			    ((SceUInt32)suspendResult & ~8u) != 0x80029004 &&
			    suspendResult != (int)0x80029001) {
#if FW_DEBUG
				PROCESS_EXIT_ASSERT(0, 709,
					"_appmgrProcessSuspendProcess");
#else
				SCE_PANIC_FUNC(709, "_appmgrProcessSuspendProcess");
#endif
			}
			SCE_DBG_PRINTF_FUNC(0, 710, "_appmgrProcessSuspendProcess",
				"[SceAppMgr] Suspend process : %s, appid=0x%08x, pid=0x%08x.\n",
				app->process.appName, app->appId,
				app->process.processId);
			if (app->process.requestedProcessState == 2)
				_shellEventAppRunningStatusChangedQueuePush();
			vcStateChanged(app->process.processId, 0);
		}
		attributes = app->processAttr;
	}
	if ((attributes & 1) == 0 && appmgrGetAppRunState(app) == 2) {
		result = sceKernelLockMutex(g_appmgrMainMutex, 1, NULL);
		PROCESS_EXIT_LOCK(result, 1076, "_suspendAppForDebugger");
		if (app->process.notifiedProcessState != 3) {
			*waitObject = appmgrUtilGetWaitObject(5);
			appmgrUtilWaitObjectClear(*waitObject);
			owner = g_appmgrMainMutexOwner;
			waiter = *waitObject;
			PROCESS_EXIT_ASSERT(owner == sceKernelGetThreadId(), 1572,
				"sceAppMgrProcessAddSuspendWaiter");
			waiter->next = NULL;
			tail = &app->process.suspendWaiters;
			while (*tail != NULL)
				tail = &(*tail)->next;
			*tail = waiter;
		}
		g_appmgrMainMutexOwner = -1;
		result = sceKernelUnlockMutex(g_appmgrMainMutex, 1);
#if FW_DEBUG
		PROCESS_EXIT_RESULT(result, 1082, "_suspendAppForDebugger");
#else
		if (result != 0)
			vcStateChangedPanic();
#endif
		_shellEventDeactivateAppRequestQueuePush(app->appId);
	}
	return 0;
}

int _appmgrProcessResumeProcess(struct AppState *app)
{
	SceUID owner;
	SceUID mutex;
	int result;
	int resumeResult;

	if (app == NULL ||
	    (owner = app->mutexOwner) != sceKernelGetThreadId())
		PROCESS_EXIT_ASSERT(0, 736, "_appmgrProcessResumeProcess");
	owner = g_appmgrMainMutexOwner;
	PROCESS_EXIT_ASSERT(owner != sceKernelGetThreadId(), 737,
		"_appmgrProcessResumeProcess");
	result = sceKernelLockMutex(g_appmgrMainMutex, 1, NULL);
	PROCESS_EXIT_LOCK(result, 739, "_appmgrProcessResumeProcess");
	_appListSetRunStatus(app, 16);
	_deviceSetRunStatus(app->appId, 1);
#if FW_DEBUG
	mutex = g_appmgrMainMutex;
	app->process.requestedProcessState = 2;
#else
	app->process.requestedProcessState = 2;
	mutex = g_appmgrMainMutex;
#endif
	g_appmgrMainMutexOwner = -1;
	result = sceKernelUnlockMutex(mutex, 1);
#if FW_DEBUG
	PROCESS_EXIT_RESULT(result, 746, "_appmgrProcessResumeProcess");
#else
	if (result != 0)
		vcStateChangedPanic();
#endif
	resumeResult = sceKernelResumeProcess(app->process.processId);
	SCE_DBG_PRINTF_FUNC(0, 750, "_appmgrProcessResumeProcess",
		"[SceAppMgr] Resume process : %s, appid=0x%08x, pid=0x%08x.\n",
		app->process.appName, app->appId, app->process.processId);
	if (resumeResult == 0)
		_shellEventAppRunningStatusChangedQueuePush();
	return 0;
}

#if FW_DEBUG
#define PROCESS_START_RESULT(_result, _line) \
	PROCESS_EXIT_RESULT(_result, _line, "_appmgrProcessStartProcess")
#else
#define PROCESS_START_RESULT(_result, _line) do { \
	if ((_result) != 0) \
		vcStateChangedPanic(); \
} while (0)
#endif

int _appmgrProcessStartProcess(struct AppState *app)
{
	struct AppmgrWaitObject *waiter;
	SceUID owner;
	SceUInt32 state;
	SceUInt32 startFlags;
	int result;
	int startResult;

	if (app == NULL ||
	    (owner = app->mutexOwner) != sceKernelGetThreadId())
		PROCESS_EXIT_ASSERT(0, 761, "_appmgrProcessRunProcess");
	owner = g_appmgrMainMutexOwner;
	PROCESS_EXIT_ASSERT(owner != sceKernelGetThreadId(), 762,
		"_appmgrProcessRunProcess");
	vcStateChanged(app->process.processId, 1);
	result = sceKernelLockMutex(g_appmgrMainMutex, 1, NULL);
	PROCESS_EXIT_LOCK(result, 767, "_appmgrProcessRunProcess");
	g_appmgrMainMutexOwner = -1;
	state = app->process.requestedProcessState;
	result = sceKernelUnlockMutex(g_appmgrMainMutex, 1);
#if FW_DEBUG
	PROCESS_EXIT_RESULT(result, 769, "_appmgrProcessRunProcess");
#else
	if (result != 0)
		vcStateChangedPanic();
#endif
	if (state == 3)
		return _appmgrProcessResumeProcess(app);
	if (state != 1)
		return 0;
	owner = app->mutexOwner;
	PROCESS_EXIT_ASSERT(owner == sceKernelGetThreadId(), 621,
		"_appmgrProcessStartProcess");
	owner = g_appmgrMainMutexOwner;
	PROCESS_EXIT_ASSERT(owner != sceKernelGetThreadId(), 622,
		"_appmgrProcessStartProcess");
	if ((app->process.content.contentFlags & 0x30) != 0) {
		result = SceNpDrmForDriver_3BFD2850(1);
#if FW_DEBUG
		PROCESS_START_RESULT(result, 628);
#else
		if (result != 0)
			SCE_PANIC_PRINTF_FUNC(628, "_appmgrProcessStartProcess",
				"0x%08x,0x%08x\n", result, 0);
#endif
	}
	startFlags = appmgrUsesGxmDebugger(app->process.processId) != 0;
	startFlags |= appmgrUsesDefaultLibrary() ? 0 : 2;
	startResult = sceKernelStartProcessExt(app->process.processId,
		app->processCreateFlags & 0x80000000, app->launchArgsSize,
		app->launchArgs, startFlags);
	if (startResult != 0) {
		if (app->launchOrigin == 2 && app->relaunchInProgress == 0 &&
		    app->notifyBootComplete != 0) {
			result = SceSysrootForDriver_421EFC96(startResult);
#if FW_DEBUG
			PROCESS_START_RESULT(result, 669);
#else
			if (result != 0)
				SCE_PANIC_PRINTF_FUNC(669, "_appmgrProcessStartProcess",
					"0x%08x,0x%08x\n", result, 0);
#endif
			app->notifyBootComplete = 0;
		}
		return startResult;
	}
	result = sceKernelLockMutex(g_appmgrMainMutex, 1, NULL);
	PROCESS_EXIT_LOCK(result, 640, "_appmgrProcessStartProcess");
	SCE_DBG_PRINTF_FUNC(0, 641, "_appmgrProcessStartProcess",
		"[SceAppMgr] Start process : %s, appid=0x%08x, pid=0x%08x.\n",
		app->process.appName, app->appId, app->process.processId);
	app->process.notifiedProcessState = 2;
	app->process.requestedProcessState = 2;
	g_appmgrMainMutexOwner = -1;
	result = sceKernelUnlockMutex(g_appmgrMainMutex, 1);
	PROCESS_START_RESULT(result, 644);
	_shellEventAppRunningStatusChangedQueuePush();
	result = sceKernelLockMutex(g_appmgrMainMutex, 1, NULL);
	PROCESS_EXIT_LOCK(result, 649, "_appmgrProcessStartProcess");
	_appListSetRunStatus(app, 16);
	owner = g_appmgrMainMutexOwner;
	PROCESS_EXIT_ASSERT(owner == sceKernelGetThreadId(), 1611,
		"sceAppMgrProcessNotifyRunningWaiter");
	for (waiter = app->process.runningWaiters; waiter != NULL;
	     waiter = waiter->next)
		appmgrUtilWaitObjectNotify(waiter);
	app->process.runningWaiters = NULL;
	g_appmgrMainMutexOwner = -1;
	result = sceKernelUnlockMutex(g_appmgrMainMutex, 1);
	PROCESS_START_RESULT(result, 652);
	if (app->launchOrigin == 2 && app->relaunchInProgress == 0 &&
	    app->notifyBootComplete != 0) {
		result = SceSysrootForDriver_421EFC96(0);
#if FW_DEBUG
		PROCESS_START_RESULT(result, 656);
#else
		if (result != 0)
			SCE_PANIC_PRINTF_FUNC(656, "_appmgrProcessStartProcess",
				"0x%08x,0x%08x\n", result, 0);
#endif
		app->notifyBootComplete = 0;
	}
	result = sceKernelLockMutex(g_appmgrMainMutex, 1, NULL);
	PROCESS_EXIT_LOCK(result, 661, "_appmgrProcessStartProcess");
	_deviceSetRunStatus(app->appId, 1);
	g_appmgrMainMutexOwner = -1;
	result = sceKernelUnlockMutex(g_appmgrMainMutex, 1);
	PROCESS_START_RESULT(result, 665);
	return startResult;
}

int _resumeAppForDebugger(struct AppState *app,
			 struct AppmgrWaitObject **waitObject)
{
	struct AppmgrWaitObject *waiter, **tail;
	SceUInt32 attributes;
	SceUID owner;
	int started, result;

	if (appmgrGetShellApp() == NULL)
		return _appmgrProcessStartProcess(app);
	attributes = app->processAttr;
	started = attributes & 3;
	if (started != 0) {
		started = _appmgrProcessStartProcess(app);
		attributes = app->processAttr;
	}
	if ((attributes & 1) != 0)
		return started;
	if (appmgrGetAppRunState(app) == 2)
		return started;
	result = sceKernelLockMutex(g_appmgrMainMutex, 1, NULL);
	PROCESS_EXIT_LOCK(result, 1152, "_resumeAppForDebugger");
	if (app->process.notifiedProcessState != 2) {
		*waitObject = appmgrUtilGetWaitObject(5);
		appmgrUtilWaitObjectClear(*waitObject);
		owner = g_appmgrMainMutexOwner;
		waiter = *waitObject;
		PROCESS_EXIT_ASSERT(owner == sceKernelGetThreadId(), 1598,
			"sceAppMgrProcessAddRunningWaiter");
		waiter->next = NULL;
		tail = &app->process.runningWaiters;
		while (*tail != NULL)
			tail = &(*tail)->next;
		*tail = waiter;
	}
	g_appmgrMainMutexOwner = -1;
	result = sceKernelUnlockMutex(g_appmgrMainMutex, 1);
#if FW_DEBUG
	PROCESS_EXIT_RESULT(result, 1158, "_resumeAppForDebugger");
#else
	if (result != 0)
		vcStateChangedPanic();
#endif
	_shellEventActivateAppRequestQueuePush(app->appId);
	return started;
}

#if FW_DEBUG
#define PROCESS_QUIT_RESULT(_result, _line) \
	PROCESS_EXIT_RESULT(_result, _line, "_appmgrProcessQuitProcess")
#else
#define PROCESS_QUIT_RESULT(_result, _line) do { \
	if ((_result) != 0) \
		vcStateChangedPanic(); \
} while (0)
#endif

int _appmgrProcessQuitProcess(struct AppState *app, int exitReason,
			     int relaunch, int retry)
{
	SceAppMgrSystemEvent event;
	SceUID owner, sourcePid;
	int sourceAppId, state, result, locked;

	locked = app != NULL;
	if (locked) {
		owner = app->mutexOwner;
		locked = owner == sceKernelGetThreadId();
	}
	PROCESS_EXIT_ASSERT(locked, 835, "_appmgrProcessQuitProcess");
	owner = g_appmgrMainMutexOwner;
	PROCESS_EXIT_ASSERT(owner != sceKernelGetThreadId(), 836,
		"_appmgrProcessQuitProcess");
	if (retry != 0)
		PROCESS_EXIT_ASSERT(app->quitRequested != 0, 842,
			"_appmgrProcessQuitProcess");
	if (relaunch == 0)
		_sceAppMgrDeactivateAppInternal(app, 0, 0, 0);
	result = sceKernelLockMutex(g_appmgrMainMutex, 1, NULL);
	PROCESS_EXIT_LOCK(result, 849, "_appmgrProcessQuitProcess");
	_deviceSetRunStatus(app->appId, 0);
	if (app->process.processId <= 0) {
		g_appmgrMainMutexOwner = -1;
		result = sceKernelUnlockMutex(g_appmgrMainMutex, 1);
		PROCESS_QUIT_RESULT(result, 855);
		return 0;
	}
	state = appmgrGetAppRunStatus(app);
	if (state == 0x300) {
		g_appmgrMainMutexOwner = -1;
		result = sceKernelUnlockMutex(g_appmgrMainMutex, 1);
		PROCESS_QUIT_RESULT(result, 865);
		return 0x80802013;
	}
	if (app->quitRequested != 0 && retry == 0) {
		if ((app->processAttr & 2) != 0) {
			g_appmgrMainMutexOwner = -1;
			result = sceKernelUnlockMutex(g_appmgrMainMutex, 1);
			PROCESS_QUIT_RESULT(result, 871);
			result = _appmgrProcessStartProcess(app);
			if (result != 0)
				SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 874,
					"_appmgrProcessQuitProcess",
					"[SceAppMgr:Error] _appmgrProcessRunProcess() failed 0x%08x.\n",
					result);
			result = sceKernelLockMutex(g_appmgrMainMutex, 1, NULL);
			PROCESS_EXIT_LOCK(result, 876, "_appmgrProcessQuitProcess");
		}
		if ((SceUInt64)(sceKernelGetSystemTimeWide() -
		    app->quitStartTime) <= 14999999 && state != 0x700) {
			if (app->relaunchPending != 0 && relaunch == 0) {
				app->relaunchPending = 0;
				app->exitReason = exitReason;
				app->selfQuit = sceKernelGetProcessId() ==
					app->process.processId;
			}
			g_appmgrMainMutexOwner = -1;
			result = sceKernelUnlockMutex(g_appmgrMainMutex, 1);
			PROCESS_QUIT_RESULT(result, 890);
			return 0;
		}
	} else {
		app->quitRequested = 1;
		app->quitStartTime = sceKernelGetSystemTimeWide();
		app->exitReason = exitReason;
		app->selfQuit = sceKernelGetProcessId() == app->process.processId;
		if (retry != 0)
			SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 900,
				"_appmgrProcessQuitProcess",
				"[SceAppMgr] Quit process : %s, appid=0x%08x, pid=0x%08x (retry).\n",
				app->process.appName, app->appId, app->process.processId);
		else
			SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 902,
				"_appmgrProcessQuitProcess",
				"[SceAppMgr] Quit process : %s, appid=0x%08x, pid=0x%08x.\n",
				app->process.appName, app->appId, app->process.processId);
		_appListSetRunStatus(app, 128);
		if (retry == 0) {
			if ((app->launchMode & 0x40000) != 0) {
				sourcePid = app->launchSourceProcessId;
				sourceAppId = app->launchSourceAppId;
			} else {
				sourcePid = -1;
				sourceAppId = -1;
			}
			_shellEventOnAppUnloadingQueuePush(app->process.processId,
				app->appId, app->exitReason, app->selfQuit,
				sourcePid, sourceAppId, relaunch, app->process.appName);
		}
		if (relaunch == 0)
			app->relaunchPending = 0;
	}
	if (state == 0x700) {
		g_appmgrMainMutexOwner = -1;
		result = sceKernelUnlockMutex(g_appmgrMainMutex, 1);
		PROCESS_QUIT_RESULT(result, 932);
		result = _appmgrProcessKillProcess(app, 1);
		if (result < 0)
			SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 937,
				"_appmgrProcessQuitProcess",
				"[SceAppMgr:Error] _appmgrProcessKillProcess() failed 0x%08x.\n",
				result);
	} else if (state == 128) {
		g_appmgrMainMutexOwner = -1;
		result = sceKernelUnlockMutex(g_appmgrMainMutex, 1);
		PROCESS_QUIT_RESULT(result, 946);
		result = _appmgrProcessKillProcess(app, 0);
		if (result < 0)
			SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 953,
				"_appmgrProcessQuitProcess",
				"[SceAppMgr:Error] _appmgrProcessKillProcess() failed 0x%08x.\n",
				result);
	} else if ((app->processAttr & 2) != 0) {
		memset(&event, 0, sizeof(event));
		event.systemEvent = 0x20000001;
		_appListPushEvent(app, &event);
		g_appmgrMainMutexOwner = -1;
		result = sceKernelUnlockMutex(g_appmgrMainMutex, 1);
		PROCESS_QUIT_RESULT(result, 971);
		result = _appmgrProcessStartProcess(app);
	} else {
		g_appmgrMainMutexOwner = -1;
		result = sceKernelUnlockMutex(g_appmgrMainMutex, 1);
		PROCESS_QUIT_RESULT(result, 983);
		result = _appmgrProcessKillProcess(app, relaunch != 0 ? 2 : 0);
		if (result < 0)
			SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 992,
				"_appmgrProcessQuitProcess",
				"[SceAppMgr:Error] _appmgrProcessKillProcess() failed 0x%08x.\n",
				result);
	}
	return result == (int)0x80029001 ? 0 : result;
}

#undef PROCESS_QUIT_RESULT
#undef PROCESS_START_RESULT
#undef PROCESS_EXIT_RESULT
#undef PROCESS_EXIT_ASSERT
#undef PROCESS_EXIT_LOCK
#undef PROCESS_SUSPEND_RESULT

int sceAppMgrSuspendAppForDebugger(SceUID pid)
{
	struct AppmgrWaitObject *waitObject;
	struct AppState *app;
	int result, suspendResult, unlockResult;

	if (sceKernelGetProcessId() == 0x10005) {
		waitObject = NULL;
		result = _appListFindAndLockAppByProcessId(pid, &app);
		if (result == 0) {
			suspendResult = _suspendAppForDebugger(app, &waitObject);
#if FW_DEBUG
			if (suspendResult != 0) {
				sceAppMgrDump();
				dumpBgmState();
				SCE_DBG_ASSERT_PRINTF_FUNC(suspendResult == 0, 1103,
							 "sceAppMgrSuspendAppForDebugger",
					"[SceAppMgr] The actual value is %d(0x%08x), but expected value is %d(0x%08x)\n",
					suspendResult, suspendResult, 0, 0);
			}
#else
			if (suspendResult != 0)
				appmgrProcessPanic();
#endif
			unlockResult = _appListUnlockApp(app);
#if FW_DEBUG
			if (unlockResult != 0) {
				sceAppMgrDump();
				dumpBgmState();
				SCE_DBG_ASSERT_PRINTF_FUNC(unlockResult == 0, 1106,
							 "sceAppMgrSuspendAppForDebugger",
					"[SceAppMgr] The actual value is %d(0x%08x), but expected value is %d(0x%08x)\n",
					unlockResult, unlockResult, 0, 0);
			}
#else
			if (unlockResult != 0)
				appmgrProcessPanic();
#endif
			if (waitObject != NULL)
				appmgrUtilWaitObjectWait(waitObject, NULL);
			result = suspendResult;
		}
	} else {
		result = SCE_APPMGR_ERROR_NOEXEC;
	}
	return result;
}

int sceAppMgrSuspendAppByNameForDebugger(const char *appName)
{
	struct AppmgrWaitObject *waitObject;
	struct AppState *app;
	int result, suspendResult, unlockResult;

	if (sceKernelGetProcessId() == 0x10005) {
		waitObject = NULL;
		result = _appListFindAndLockAppByName(appName, &app);
		if (result == 0) {
			suspendResult = _suspendAppForDebugger(app, &waitObject);
#if FW_DEBUG
			if (suspendResult != 0) {
				sceAppMgrDump();
				dumpBgmState();
				SCE_DBG_ASSERT_PRINTF_FUNC(suspendResult == 0, 1128,
							 "sceAppMgrSuspendAppByNameForDebugger",
					"[SceAppMgr] The actual value is %d(0x%08x), but expected value is %d(0x%08x)\n",
					suspendResult, suspendResult, 0, 0);
			}
#else
			if (suspendResult != 0)
				appmgrProcessPanic();
#endif
			unlockResult = _appListUnlockApp(app);
#if FW_DEBUG
			if (unlockResult != 0) {
				sceAppMgrDump();
				dumpBgmState();
				SCE_DBG_ASSERT_PRINTF_FUNC(unlockResult == 0, 1131,
							 "sceAppMgrSuspendAppByNameForDebugger",
					"[SceAppMgr] The actual value is %d(0x%08x), but expected value is %d(0x%08x)\n",
					unlockResult, unlockResult, 0, 0);
			}
#else
			if (unlockResult != 0)
				appmgrProcessPanic();
#endif
			if (waitObject != NULL)
				appmgrUtilWaitObjectWait(waitObject, NULL);
			result = suspendResult;
		}
	} else {
		result = SCE_APPMGR_ERROR_NOEXEC;
	}
	return result;
}

int sceAppMgrResumeAppForDebugger(SceUID pid)
{
	struct AppmgrWaitObject *waitObject;
	struct AppState *app;
	int result, resumeResult, unlockResult;

	if (sceKernelGetProcessId() == 0x10005) {
		waitObject = NULL;
		result = _appListFindAndLockAppByProcessId(pid, &app);
		if (result == 0) {
			resumeResult = _resumeAppForDebugger(app, &waitObject);
			unlockResult = _appListUnlockApp(app);
#if FW_DEBUG
			if (unlockResult != 0) {
				sceAppMgrDump();
				dumpBgmState();
				SCE_DBG_ASSERT_PRINTF_FUNC(unlockResult == 0, 1182,
							 "sceAppMgrResumeAppForDebugger",
					"[SceAppMgr] The actual value is %d(0x%08x), but expected value is %d(0x%08x)\n",
					unlockResult, unlockResult, 0, 0);
			}
#else
			if (unlockResult != 0)
				appmgrProcessPanic();
#endif
			if (waitObject != NULL)
				appmgrUtilWaitObjectWait(waitObject, NULL);
			result = resumeResult;
		}
	} else {
		result = SCE_APPMGR_ERROR_NOEXEC;
	}
	return result;
}

int sceAppMgrResumeAppByNameForDebugger(const char *appName)
{
	struct AppmgrWaitObject *waitObject;
	struct AppState *app;
	int result, resumeResult, unlockResult;

	if (sceKernelGetProcessId() == 0x10005) {
		waitObject = NULL;
		result = _appListFindAndLockAppByName(appName, &app);
		if (result == 0) {
			resumeResult = _resumeAppForDebugger(app, &waitObject);
			unlockResult = _appListUnlockApp(app);
#if FW_DEBUG
			if (unlockResult != 0) {
				sceAppMgrDump();
				dumpBgmState();
				SCE_DBG_ASSERT_PRINTF_FUNC(unlockResult == 0, 1207,
							 "sceAppMgrResumeAppByNameForDebugger",
					"[SceAppMgr] The actual value is %d(0x%08x), but expected value is %d(0x%08x)\n",
					unlockResult, unlockResult, 0, 0);
			}
#else
			if (unlockResult != 0)
				appmgrProcessPanic();
#endif
			if (waitObject != NULL)
				appmgrUtilWaitObjectWait(waitObject, NULL);
			result = resumeResult;
		}
	} else {
		result = SCE_APPMGR_ERROR_NOEXEC;
	}
	return result;
}

#if FW_DEBUG
#define PROCESS_INITIALIZE_CHECK(_failed, _line, _info) do { \
	if (_failed) { \
		sceAppMgrDump(); \
		dumpBgmState(); \
		SCE_DBG_ASSERT_CHECK_FUNC(0, _line, "_appmgrProcessInitialize"); \
	} \
} while (0)
#else
#define PROCESS_INITIALIZE_CHECK(_failed, _line, _info) do { \
	if (_failed) \
		SceDebugForDriver_391B5B74(&(_info), __builtin_return_address(0)); \
} while (0)
#endif

int _appmgrProcessInitialize(int (*callback)(struct AppState *app))
{
	int result;

	g_appmgrProcTerminatedCallback = callback;
	result = SceProcessmgrForDriver_5E882B60(
		(void (*)(SceUID, int))_sceAppMgrProcTerminatedHandler);
	PROCESS_INITIALIZE_CHECK(result != 0, 1496, terminatedRegistrationPanicInfo);
	g_appmgrProcEventHandlerUid = sceKernelRegisterProcEventHandler(
		"SceAppMgr", &processEventHandlers, NULL);
	PROCESS_INITIALIZE_CHECK(g_appmgrProcEventHandlerUid < 0, 1499,
		processEventRegistrationPanicInfo);
	result = appmgrUtilWaitObjectInitialize();
	PROCESS_INITIALIZE_CHECK(result < 0, 1502, processWaitInitializationPanicInfo);
	/* The provider stores an opaque callback; the actual callback takes pid/usage. */
	result = SceAvcodecForDriver_C84AFFC4(
		(void (*)(void))videocodecUsageChangedCallback2);
	PROCESS_INITIALIZE_CHECK(result != 0, 1506, videoCodecRegistrationPanicInfo);
	return 0;
}

#undef PROCESS_INITIALIZE_CHECK

void appmgrResetProcessState(AppmgrProcessState *process)
{
	process->processId = -1;
	process->budgetId = -1;
	process->notifiedProcessState = 0;
	process->requestedProcessState = 0;
	process->runningWaiters = NULL;
	process->suspendWaiters = NULL;
	process->unloadWaiter = NULL;
	process->runWhenUiOverlay = 0;
	process->maxFD = 0;
	process->maxDIR = 0;
	process->primaryResourceQuota = 0;
	process->secondaryResourceQuota = 0;
	__builtin_memset(&process->mounts, 0, sizeof(process->mounts));
	memset(&process->content, 0, sizeof(process->content));
}

SceUID sceAppMgrSpawnProcessForDriver(const char *name, int type, const char *path,
			   SceSize args, const void *argp, void *option, void *out)
{
	const SceKernelProcessOpt *opt = option;
	SceKernelProcessOpt processOpt;
	SceSize size;
	int useGxmDebugger;

	(void)out;
	if (opt != NULL) {
		size = opt->size;
		if (size >= sizeof(processOpt))
			size = sizeof(processOpt);
#if FW_DEBUG
		__memcpy_chk(&processOpt, opt, size, sizeof(processOpt));
#else
		memcpy(&processOpt, opt, size);
#endif
	} else {
		memset(&processOpt, 0, sizeof(processOpt));
		processOpt.size = sizeof(processOpt);
	}
	if (type == 0x4000000) {
		processOpt.limit0 = 46;
		processOpt.limit1 = 13;
		processOpt.flags |= 0x180000;
	}
	useGxmDebugger = appmgrUsesGxmDebugger(0x10005);
	return sceKernelSpawnProcessExt(name, type, path, args, (void *)argp, &processOpt,
				       useGxmDebugger != 0);
}

SceUID sceAppMgrSpawnProcessExtForDriver(const char *name, SceUInt32 type, const char *path,
			      SceSize args, void *argp, const SceKernelProcessOpt *opt)
{
	int useGxmDebugger = appmgrUsesGxmDebugger(0x10005);

	return sceKernelSpawnProcessExt(name, type, path, args, argp, opt,
				       useGxmDebugger != 0);
}
