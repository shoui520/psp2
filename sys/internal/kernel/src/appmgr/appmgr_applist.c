/* Application-list storage. */

#include <sysmem/debug.h>
#include <threadmgr/threadmgr.h>
#include <string.h>
#include "app_state_internal.h"
#include <appmgr/appmgr.h>
#include "appmgr_device.h"

extern SceUID g_appmgrMainMutexOwner;
extern SceUID g_appmgrMainMutex;
extern SceUID g_appmgrCore2MutexOwner;
extern SceUID g_appmgrShellPid;

static inline SceUInt32 appListLoadStateExclusive(SceUInt32 *state);
static inline unsigned int appListStoreStateExclusive(SceUInt32 *state,
						     SceUInt32 value);

struct AppState g_appmgrAppStates[20];
static SceUInt32 appListGeneration;

_Static_assert(sizeof(g_appmgrAppStates) == 184640,
	       "application list storage");

#if !FW_DEBUG
static const SceKernelDebugInfo appListOwnerPanicInfo = {
	.fileHash = 0xc9656e8e,
	.lineHash = 0xc879dadc,
	.funcHash = 0x11a05ee1,
};

static const SceKernelDebugInfo appListLockPanicInfo = {
	.fileHash = 0xc9656e8e,
	.lineHash = 0x79d3a1f0,
	.funcHash = 0x9d536bce,
};

static const SceKernelDebugInfo appListServicePanicInfo = {
	.fileHash = 0xc9656e8e,
	.lineHash = 0x79d3a1f0,
	.funcHash = 0x9d536bce,
};

_Noreturn void appmgrAppListOwnerPanic(void)
{
	SceDebugForDriver_391B5B74(&appListOwnerPanicInfo, __builtin_return_address(0));
}

_Noreturn void appmgrAppListLockPanic(void)
{
	SceDebugForDriver_391B5B74(&appListLockPanicInfo, __builtin_return_address(0));
}

_Noreturn void appmgrAppListPanic(void)
{
	SceDebugForDriver_391B5B74(&appListServicePanicInfo, __builtin_return_address(0));
}
#endif

void _appListClearAppState(struct AppState *app)
{
	int result;
	SceUInt32 state;

	memset(&app->launchInfo, 0, sizeof(app->launchInfo));
	app->appId = -1;
	app->baseMiniEvictionValue = 0;
	app->state &= 0xfff;
	app->launchSourceProcessId = 0;
	do {
		state = appListLoadStateExclusive(&app->state);
		state = (state & ~0xff0u) | 0x100;
	} while (appListStoreStateExclusive(&app->state, state) != 0);
	do {
		state = appListLoadStateExclusive(&app->state);
		state = (state & ~15u) | 1;
	} while (appListStoreStateExclusive(&app->state, state) != 0);
	app->exitReason = 0;
	memset(app->process.appName, 0, sizeof(app->process.appName));
	result = appmgrParamQueueInit(&app->params);
	if (result != 0) {
#if FW_DEBUG
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_PRINTF(0, 45,
			"[SceAppMgr] The actual value is %d(0x%08x), but expected value is %d(0x%08x)\n",
			result, result, 0, 0);
#else
		appmgrAppListOwnerPanic();
#endif
	}
	app->paramDeliveryHandled = 0;
	appmgrResetProcessState(&app->process);
	app->quitRequested = 0;
	app->relaunchPending = 0;
	app->appInfobar.visibility = 0;
	app->appInfobar.color = 0;
	app->appInfobar.transparency = 0;
	app->commonDialogInfobar.color = 0;
	app->commonDialogInfobar.transparency = 0;
	app->shellInfobar.color = 0;
	app->shellInfobar.transparency = 0;
	app->commonDialogInfobar.visibility = 2;
	app->shellInfobar.visibility = 2;
	app->videoCodecUsage = 0;
	app->evictionThresholdClass = 0;
	app->coredumpReason = 0;
	app->systemOrientation = 1;
	app->recommendedOrientationActivated = 0;
	app->networkDisconnectionWarningDialogEnabled = 0;
	app->relaunchInProgress = 0;
	app->backgroundSuspendRequested = 0;
	app->gameCardRemovalQuitPending = 0;
	app->resumeCreatorOnLaunchFailure = 0;
	memset(&app->launchOptions, 0, sizeof(app->launchOptions));
}

int appmgrInitializeAppState(struct AppState *app)
{
	SceUID mutex;

	_appListClearAppState(app);
	mutex = sceKernelCreateMutex("SceAppMgrAppMutex", 0x8000, 0, NULL);
	if (mutex >= 0) {
		app->mutex = mutex;
		app->mutexOwner = -1;
		return 0;
	}
	return mutex;
}

int _appListGetAppStatus(struct AppState *app, SceAppMgrAppStatus *status)
{
	if (sceKernelGetProcessId() != 0x10005) {
		SceUID owner = g_appmgrMainMutexOwner;

		if (owner != sceKernelGetThreadId()) {
#if FW_DEBUG
			sceAppMgrDump();
			dumpBgmState();
			SCE_DBG_ASSERT_FAIL(99);
#else
			appmgrAppListOwnerPanic();
#endif
		}
	}
	memset(status, 0, sizeof(*status));
	status->launchSourceProcessId = app->launchSourceProcessId;
	status->launchMode = app->launchMode;
	status->protectionMode = app->protectionMode;
	status->paramDeliveryHandled = app->paramDeliveryHandled;
	status->appId = app->appId;
	status->processId = app->process.processId;
	status->activated = (app->state & 15) == 2;
	status->processRunRequested = app->process.requestedProcessState == 2;
	status->launchSourceAppId = app->launchSourceAppId;
	if (app->shellInfobar.visibility != 2)
		status->infobar = app->shellInfobar;
	else if (app->commonDialogInfobar.visibility != 2)
		status->infobar = app->commonDialogInfobar;
	else
		status->infobar = app->appInfobar;
	status->launchArgumentMarker = app->launchInfo.launchArgumentMarker;
	status->launchOrigin = app->launchInfo.launchOrigin;
	strncpy(status->appName, app->process.appName, sizeof(status->appName));
	if (sceKernelGetProcessId() == 0x10005)
		memset(status->bgmProxyAppName, 0, sizeof(status->bgmProxyAppName));
	else
		_deviceGetBgmProxyAppName(app->appId, status->bgmProxyAppName);
	status->budgetType = app->launchInfo.budgetType;
	status->processAttr = app->launchInfo.processAttr;
	status->systemOrientation = app->systemOrientation;
	status->recommendedOrientationActivated = app->recommendedOrientationActivated;
	status->networkDisconnectionWarningDialogEnabled =
		app->networkDisconnectionWarningDialogEnabled;
	return 0;
}

int appmgrClearFailedAppLaunch(struct AppState *app, int launchResult,
			     SceUID *creatorPid)
{
	SceUID callback;

	if (app->state != 0x301)
		return 0x80802013;
	callback = app->launchCallbackId;
	if ((SceUInt32)callback + 1 > 1 &&
	    (SceUInt32)launchResult + 0x7ffd7fd4u > 1 && callback > 0)
		sceKernelNotifyCallback(callback, launchResult);
	if (creatorPid != NULL && app->resumeCreatorOnLaunchFailure != 0)
		*creatorPid = app->creatorPid;
	_appListClearAppState(app);
	return 0;
}

void _appListShowAppState(struct AppState *app)
{
#if FW_DEBUG
	SCE_DBG_PRINTF(0, 200, "\tappId           = 0x%08x\n", app->appId);
	SCE_DBG_PRINTF(0, 201, "\tappName         = %s\n", app->process.appName);
	SCE_DBG_PRINTF(0, 202, "\tappPath         = %s\n", app->launchInfo.appPath);
	SCE_DBG_PRINTF(0, 203, "\tbudgetSize      = 0x%08x\n", app->launchInfo.budgetSize);
	SCE_DBG_PRINTF(0, 204, "\tpycontSize      = 0x%08x\n", app->launchInfo.pycontSize);
	SCE_DBG_PRINTF(0, 205, "\tlaunchMode      = 0x%08x\n", app->launchMode);
	SCE_DBG_PRINTF(0, 206, "\tbudgetType      = 0x%08x\n", app->launchInfo.budgetType);
	SCE_DBG_PRINTF(0, 207, "\tprocessAttr     = 0x%08x\n", app->launchInfo.processAttr);
	SCE_DBG_PRINTF(0, 208, "\tstate           = 0x%08x\n", app->state);
	SCE_DBG_PRINTF(0, 209, "\tprocessId       = 0x%08x\n", app->process.processId);
	SCE_DBG_PRINTF(0, 210, "\tbudgetId        = 0x%08x\n", app->process.budgetId);
	SCE_DBG_PRINTF(0, 211, "\tlaunchOrigin    = 0x%08x\n",
			   (signed char)app->launchInfo.launchOrigin);
	SCE_DBG_PRINTF(0, 212, "\trequestedProcessState = 0%x08x\n",
			   app->process.requestedProcessState);
#else
	(void)app;
#endif
}

/* The release lookups inline Sony's complete _appListGetBigApp method. */
struct AppState *appmgrFindCurrentApp(struct AppState *apps)
{
	SceUID pid = sceKernelGetProcessId();
	int index;

	for (index = 0; index < 20; index++) {
		if (apps[index].process.processId == pid)
			return &apps[index];
	}
	return NULL;
}

#define APP_LIST_BIG_APP_BODY() do { \
	struct AppState *app; \
	SceUID shellPid; \
	int index; \
	appListCheckOwner(); \
	shellPid = g_appmgrAppStates[0].process.processId; \
	if (shellPid < 0) { \
		index = 0; \
	} else { \
		index = shellPid & 1; \
		if (index) \
			index = shellPid == g_appmgrShellPid; \
	} \
	app = &apps[index]; \
	do { \
		SceUID processId = app->process.processId; \
		if (processId >= 0 && (processId & 1) && \
		    app->launchInfo.budgetType == 1 && (app->state & 0xff0) != 0x800) \
			return app; \
		++index; \
		++app; \
	} while (index <= 19); \
	return NULL; \
} while (0)

#if FW_DEBUG
#define appListCheckOwner() do { \
	SceUID owner = g_appmgrMainMutexOwner; \
	if (owner != sceKernelGetThreadId()) { \
		sceAppMgrDump(); \
		dumpBgmState(); \
		SCE_DBG_ASSERT_FAIL(339); \
	} \
} while (0)

struct AppState *_appListGetBigApp(struct AppState *apps)
{
	APP_LIST_BIG_APP_BODY();
}
#define appListReturnBigApp() return _appListGetBigApp(apps)
#else
#define appListCheckOwner() do { \
	SceUID owner = g_appmgrMainMutexOwner; \
	if (owner != sceKernelGetThreadId()) \
		appmgrAppListOwnerPanic(); \
} while (0)
#define appListReturnBigApp() APP_LIST_BIG_APP_BODY()
#endif

/* Both original selector entry points carry the same complete algorithm. */
#define APP_LIST_LOOKUP_BODY() do { \
	struct AppState *app; \
	SceUID shellPid; \
	int index; \
	if (pid == -1) \
		return NULL; \
	if (pid == -2) { \
		pid = sceKernelGetProcessId(); \
	} else if (pid == -3) { \
		appListReturnBigApp(); \
	} else if (pid == -4) { \
		shellPid = g_appmgrAppStates[0].process.processId; \
		if (shellPid < 0) { \
			index = 0; \
		} else { \
			index = shellPid & 1; \
			if (index) \
				index = shellPid == g_appmgrShellPid; \
		} \
		app = &apps[index]; \
		do { \
			if (app->process.processId != -1 && (app->state & 15) == 2) \
				return app; \
			++index; \
			++app; \
		} while (index <= 19); \
		return NULL; \
	} else if (pid == -5) { \
		shellPid = g_appmgrAppStates[0].process.processId; \
		if (shellPid < 0) { \
			index = 0; \
		} else { \
			index = shellPid & 1; \
			if (index) \
				index = shellPid == g_appmgrShellPid; \
		} \
		app = &apps[index]; \
		do { \
			if (app->process.processId != -1 && (app->state & 15) == 2) { \
				if (app->launchInfo.budgetType == 1) \
					return app; \
				break; \
			} \
			++index; \
			++app; \
		} while (index <= 19); \
		if (shellPid < 0 || !(shellPid & 1)) \
			return NULL; \
		return shellPid == g_appmgrShellPid ? g_appmgrAppStates : NULL; \
	} else if (pid == -6) { \
		shellPid = g_appmgrAppStates[0].process.processId; \
		if (shellPid < 0) { \
			index = 0; \
		} else { \
			index = shellPid & 1; \
			if (index) \
				index = shellPid == g_appmgrShellPid; \
		} \
		app = &apps[index]; \
		do { \
			if (app->process.processId != -1 && (app->state & 15) == 2) \
				return app; \
			++index; \
			++app; \
		} while (index <= 19); \
		if (shellPid < 0 || !(shellPid & 1)) \
			return NULL; \
		return shellPid == g_appmgrShellPid ? g_appmgrAppStates : NULL; \
	} \
	for (index = 0; index < 20; index++) { \
		if (apps[index].process.processId == pid) \
			return &apps[index]; \
	} \
	return NULL; \
} while (0)

struct AppState *appmgrFindSpecialApp(struct AppState *apps, SceUID pid)
{
	APP_LIST_LOOKUP_BODY();
}

#if !FW_DEBUG
struct AppState *appmgrFindAppByProcessId(struct AppState *apps, SceUID pid)
{
	APP_LIST_LOOKUP_BODY();
}
#endif

struct AppState *appmgrFindAppById(int appId)
{
	int index;
	struct AppState *app;

	if (appId < 0)
		return appmgrFindSpecialApp(g_appmgrAppStates, appId);
	index = ((appId & 0xfff) - 2) >> 1;
	if (index == -1)
		return NULL;
	if (index > 19)
		return NULL;
	app = &g_appmgrAppStates[index];
	return app->appId == appId ? app : NULL;
}

#if FW_DEBUG
struct AppState *appmgrFindAppByProcessId(struct AppState *apps, SceUID pid)
{
	APP_LIST_LOOKUP_BODY();
}
#endif

#if !FW_DEBUG
struct AppState *_appListGetBigApp(struct AppState *apps)
{
	APP_LIST_BIG_APP_BODY();
}
#endif

struct AppState *appmgrFindActiveApp(struct AppState *apps)
{
	SceUID shellPid = g_appmgrAppStates[0].process.processId;
	struct AppState *app;
	int index;

	if (shellPid < 0) {
		index = 0;
	} else {
		index = shellPid & 1;
		if (index != 0)
			index = shellPid == g_appmgrShellPid;
	}
	for (app = &apps[index]; ; ++app) {
		++index;
		if (app->process.processId != -1 && (app->state & 15) == 2)
			return app;
		if (index > 19)
			return NULL;
	}
}

#define APP_LIST_FIND_NAME(output, apps, name) do { \
	int slot; \
	(output) = NULL; \
	if ((name) != NULL) { \
		int found = 0; \
		for (slot = 0; slot < 20; ++slot) { \
			struct AppState *candidate = &(apps)[slot]; \
			int id = candidate->appId; \
			if ((id & 0x1000) && id > 0 && !(id & 1) && \
			    strncmp(candidate->process.appName, (name), 32) == 0) { \
				(output) = candidate; \
				found = 1; \
				break; \
			} \
		} \
		if (found) \
			break; \
		for (slot = 0; slot < 20; ++slot) { \
			struct AppState *candidate = &(apps)[slot]; \
			if (candidate->appId > 0 && \
			    strncmp(candidate->process.appName, (name), 32) == 0) { \
				(output) = candidate; \
				break; \
			} \
		} \
	} else { \
		SceUID pid = sceKernelGetProcessId(); \
		for (slot = 0; slot < 20; ++slot) { \
			struct AppState *candidate = &(apps)[slot]; \
			if (candidate->process.processId == pid) { \
				(output) = candidate; \
				break; \
			} \
		} \
		if ((output) != NULL) \
			break; \
		pid = sceKernelGetProcessId(); \
		for (slot = 0; slot < 20; ++slot) { \
			struct AppState *candidate = &(apps)[slot]; \
			if (candidate->process.processId == pid) { \
				(output) = candidate; \
				break; \
			} \
		} \
	} \
} while (0)

struct AppState *appmgrFindAppByName(struct AppState *apps, const char *name)
{
	struct AppState *app;

	APP_LIST_FIND_NAME(app, apps, name);
	return app;
}

void _appListDeclareShellProcess(struct AppState *apps)
{
	struct AppState *app;
	SceUID owner = g_appmgrMainMutexOwner;
	SceUID pid;
	int index;

	if (owner != sceKernelGetThreadId()) {
#if FW_DEBUG
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_FAIL(371);
#else
		appmgrAppListOwnerPanic();
#endif
	}
	for (index = 1; index < 20; index++) {
		app = &apps[index];
		if (app->process.processId == sceKernelGetProcessId()) {
			appmgrClearPendingAppEvents(app);
			_appListClearAppState(app);
			break;
		}
	}
	apps->appId = 0x1002;
	pid = sceKernelGetProcessId();
	apps->state = 0x12;
	apps->launchInfo.budgetType = 2;
	apps->process.processId = pid;
	g_appmgrShellPid = pid;
	memset(apps->process.content.titleId, 0,
	       sizeof(apps->process.content.titleId));
	__builtin_memcpy(apps->process.content.titleId, "NPXS19999", 9);
}

void _appListUndeclareShellProcess(struct AppState *app)
{
	int result;

	g_appmgrShellPid = -1;
	_appListClearAppState(app);
	result = appmgrInitializeShellEventQueue();
	if (result != 0) {
#if FW_DEBUG
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_PRINTF(0, 397,
			"[SceAppMgr] The actual value is %d(0x%08x), but expected value is %d(0x%08x)\n",
			result, result, 0, 0);
#else
		appmgrAppListOwnerPanic();
#endif
	}
}

struct AppState *appmgrGetShellApp(void)
{
	SceUID pid = g_appmgrAppStates[0].process.processId;

	if (pid < 0)
		return NULL;
	if (!(pid & 1))
		return NULL;
	return pid == g_appmgrShellPid ? g_appmgrAppStates : NULL;
}

/* The exclusive reservation, including guarded returns without CLREX, is
 * architectural state that ordinary C loads and stores cannot express.
 */
static inline SceUInt32 appListLoadStateExclusive(SceUInt32 *state)
{
	SceUInt32 value;

	__asm__ volatile("ldrex %0, [%1]"
		: "=r"(value) : "r"(state) : "memory");
	return value;
}

static inline unsigned int appListStoreStateExclusive(SceUInt32 *state,
						     SceUInt32 value)
{
	unsigned int failed;

	__asm__ volatile("strex %0, %2, [%1]"
		: "=&r"(failed) : "r"(state), "r"(value) : "memory");
	return failed;
}

void _appListSetRunStatus(struct AppState *app, unsigned int status)
{
	SceUInt32 value;
	SceUInt32 updated;
	SceUInt32 runStatus;

	if (status & ~0xff0u) {
#if FW_DEBUG
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_FAIL(442);
#else
		appmgrAppListOwnerPanic();
#endif
	}
	if ((status & 0xff0) == 0x10 || (status & 0xff0) == 0x20) {
		do {
			value = appListLoadStateExclusive(&app->state);
#if FW_DEBUG
			updated = (value & ~0xff0u) | status;
#endif
			runStatus = value & 0xff0;
			if (runStatus == 0x200 || runStatus == 0x700)
				break;
#if !FW_DEBUG
			updated = (value & ~0xff0u) | status;
#endif
		} while (appListStoreStateExclusive(&app->state, updated) != 0);
	} else {
		do {
			value = appListLoadStateExclusive(&app->state);
			updated = (value & ~0xff0u) | status;
		} while (appListStoreStateExclusive(&app->state, updated) != 0);
	}
}

int appmgrGetAppRunStatus(const struct AppState *app)
{
	return app->state & 0xff0;
}

void _appListSetDebuggerRunStatus(struct AppState *app, unsigned int status)
{
	SceUInt32 value;

	if (status & ~0xf0000u) {
#if FW_DEBUG
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_FAIL(463);
#else
		appmgrAppListOwnerPanic();
#endif
	}
	do {
		value = appListLoadStateExclusive(&app->state);
		value = (value & ~0xf0000u) | status;
	} while (appListStoreStateExclusive(&app->state, value) != 0);
}

void _appListSetActiveness(struct AppState *app, unsigned int status)
{
	SceUInt32 value;

	if (status >> 4) {
#if FW_DEBUG
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_FAIL(480);
#else
		appmgrAppListOwnerPanic();
#endif
	}
	do {
		value = appListLoadStateExclusive(&app->state);
		value = (value & ~15u) | status;
	} while (appListStoreStateExclusive(&app->state, value) != 0);
}

int appmgrGetAppRunState(const struct AppState *app)
{
	return app->state & 15;
}

void appmgrSetAppStateFlags(struct AppState *app, unsigned int flags)
{
	SceUInt32 value;

	do {
		value = appListLoadStateExclusive(&app->state);
		value |= flags;
	} while (appListStoreStateExclusive(&app->state, value) != 0);
}

void appmgrMaskAppStateFlags(struct AppState *app, unsigned int mask)
{
	SceUInt32 value;

	do {
		value = appListLoadStateExclusive(&app->state);
		value &= mask;
	} while (appListStoreStateExclusive(&app->state, value) != 0);
}

#if FW_DEBUG
#define APP_LIST_LOCK_ERROR(result, line) do { \
	SCE_DBG_PRINTF(0, line, "Failed to lock ret=0x%08x\n", result); \
	sceAppMgrDump(); \
	dumpBgmState(); \
	SCE_DBG_ASSERT_FAIL(line); \
} while (0)
#define APP_LIST_MUTEX_ERROR(result, line) do { \
	sceAppMgrDump(); \
	dumpBgmState(); \
	SCE_DBG_ASSERT_PRINTF(0, line, \
		"[SceAppMgr] The actual value is %d(0x%08x), but expected value is %d(0x%08x)\n", \
		result, result, 0, 0); \
} while (0)
#define APP_LIST_ASSERT_UNLOCKED(line) do { \
	if (sceKernelGetAssertLevel() > 0) { \
		SceUID owner = g_appmgrMainMutexOwner; \
		SCE_DBG_ASSERT_LEVEL(1, owner != sceKernelGetThreadId(), line); \
	} \
} while (0)
#define APP_LIST_ASSERT_SLOT_LOCK_ORDER(line) do { \
	SceUID owner = g_appmgrCore2MutexOwner; \
	if (owner != sceKernelGetThreadId()) { \
		struct AppState *slot; \
		for (slot = g_appmgrAppStates; slot != g_appmgrAppStates + 20; ++slot) { \
			int level = sceKernelGetAssertLevel(); \
			if (level > 0) { \
				SceUID slotOwner = slot->mutexOwner; \
				SCE_DBG_ASSERT_LEVEL(1, slotOwner != sceKernelGetThreadId(), line); \
			} \
		} \
	} \
} while (0)
#else
#define APP_LIST_LOCK_ERROR(result, line) appmgrAppListLockPanic()
#define APP_LIST_MUTEX_ERROR(result, line) appmgrAppListLockPanic()
#define APP_LIST_ASSERT_UNLOCKED(line) do { } while (0)
#define APP_LIST_ASSERT_SLOT_LOCK_ORDER(line) ((void)sceKernelGetThreadId())
#endif

int _appListReserveAndLockApp(struct AppState **output)
{
	struct AppState *app;
	int result;
	int index;
	int error;
	SceUInt32 generation;

	if (output == NULL) {
#if FW_DEBUG
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_FAIL(571);
#else
		appmgrAppListOwnerPanic();
#endif
	}
	for (;;) {
		result = sceKernelLockMutex(g_appmgrMainMutex, 1, NULL);
		if (result == 0)
			g_appmgrMainMutexOwner = sceKernelGetThreadId();
		else
			APP_LIST_LOCK_ERROR(result, 575);
		error = sceAppMgrCheckAppLaunchLimitToAvoidQueueOverflow();
		if (error != 0) {
			g_appmgrMainMutexOwner = -1;
			result = sceKernelUnlockMutex(g_appmgrMainMutex, 1);
			if (result != 0)
				APP_LIST_MUTEX_ERROR(result, 580);
			return error;
		}
		for (index = 0; index < 20; index++) {
			if (g_appmgrAppStates[index].appId == -1)
				break;
		}
		if (index == 20) {
			g_appmgrMainMutexOwner = -1;
			result = sceKernelUnlockMutex(g_appmgrMainMutex, 1);
			if (result != 0)
				APP_LIST_MUTEX_ERROR(result, 592);
			return (int)0x80802012;
		}
		g_appmgrMainMutexOwner = -1;
		result = sceKernelUnlockMutex(g_appmgrMainMutex, 1);
		app = &g_appmgrAppStates[index];
		if (result != 0) {
#if FW_DEBUG
			sceAppMgrDump();
			dumpBgmState();
			if (sceKernelGetAssertLevel() <= 0)
				goto checkSlotOrder;
			SCE_DBG_ASSERT_PRINTF_LEVEL(1, 0, 596,
				"[SceAppMgr] The actual value is %d(0x%08x), but expected value is %d(0x%08x)\n",
				result, result, 0, 0);
#else
			appmgrAppListLockPanic();
#endif
		}
		APP_LIST_ASSERT_UNLOCKED(598);
#if FW_DEBUG
checkSlotOrder:
#endif
		APP_LIST_ASSERT_SLOT_LOCK_ORDER(598);
		result = sceKernelLockMutex(app->mutex, 1, NULL);
		if (result != 0) {
			if ((SceUInt32)result + 0x7ffd7fd4u > 1)
				APP_LIST_MUTEX_ERROR(result, 598);
			return result;
		}
		app->mutexOwner = sceKernelGetThreadId();
		result = sceKernelLockMutex(g_appmgrMainMutex, 1, NULL);
		if (result == 0)
			g_appmgrMainMutexOwner = sceKernelGetThreadId();
		else
			APP_LIST_LOCK_ERROR(result, 603);
		if (app->appId == -1) {
			_appListClearAppState(app);
			generation = appListGeneration;
			appListGeneration = (generation + 1) & 0x7fff;
			app->appId = ((generation << 16) & 0x7fff0000) | (2 * index + 2);
			g_appmgrMainMutexOwner = -1;
			result = sceKernelUnlockMutex(g_appmgrMainMutex, 1);
			if (result != 0)
				APP_LIST_MUTEX_ERROR(result, 615);
			*output = app;
			return 0;
		}
		g_appmgrMainMutexOwner = -1;
		result = sceKernelUnlockMutex(g_appmgrMainMutex, 1);
		if (result != 0) {
#if FW_DEBUG
			sceAppMgrDump();
			dumpBgmState();
			if (sceKernelGetAssertLevel() <= 0)
				goto unlockSlot;
			SCE_DBG_ASSERT_PRINTF_LEVEL(1, 0, 606,
				"[SceAppMgr] The actual value is %d(0x%08x), but expected value is %d(0x%08x)\n",
				result, result, 0, 0);
#else
			appmgrAppListLockPanic();
#endif
		}
		APP_LIST_ASSERT_UNLOCKED(608);
#if FW_DEBUG
unlockSlot:
#endif
		{
			SceUID mutex = app->mutex;

			app->mutexOwner = -1;
			result = sceKernelUnlockMutex(mutex, 1);
			if (result != 0)
				APP_LIST_MUTEX_ERROR(result, 608);
		}
	}
}

#define APP_LIST_FIND_BY_ID(output, id) do { \
	if ((id) < 0) { \
		(output) = appmgrFindSpecialApp(g_appmgrAppStates, (id)); \
	} else { \
		int slot = (((id) & 0xfff) - 2) >> 1; \
		(output) = NULL; \
		if (slot != -1 && slot <= 19) { \
			struct AppState *candidate = &g_appmgrAppStates[slot]; \
			if (candidate->appId == (id)) \
				(output) = candidate; \
		} \
	} \
} while (0)

int _appListFindAndLockAppByAppId(int appId, struct AppState **output)
{
	struct AppState *app;
	struct AppState *current;
	int result;

	if (output == NULL) {
#if FW_DEBUG
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_FAIL(629);
#else
		appmgrAppListOwnerPanic();
#endif
	}
	result = sceKernelLockMutex(g_appmgrMainMutex, 1, NULL);
	if (result == 0)
		g_appmgrMainMutexOwner = sceKernelGetThreadId();
	else
		APP_LIST_LOCK_ERROR(result, 631);
	APP_LIST_FIND_BY_ID(app, appId);
	if (app == NULL) {
		g_appmgrMainMutexOwner = -1;
		result = sceKernelUnlockMutex(g_appmgrMainMutex, 1);
		if (result != 0)
			APP_LIST_MUTEX_ERROR(result, 635);
		return (int)0x80802012;
	}
	g_appmgrMainMutexOwner = -1;
	result = sceKernelUnlockMutex(g_appmgrMainMutex, 1);
	if (result != 0) {
#if FW_DEBUG
		sceAppMgrDump();
		dumpBgmState();
		if (sceKernelGetAssertLevel() <= 0)
			goto checkSlotOrder;
		SCE_DBG_ASSERT_PRINTF_LEVEL(1, 0, 640,
			"[SceAppMgr] The actual value is %d(0x%08x), but expected value is %d(0x%08x)\n",
			result, result, 0, 0);
#else
		appmgrAppListLockPanic();
#endif
	}
	APP_LIST_ASSERT_UNLOCKED(642);
#if FW_DEBUG
checkSlotOrder:
#endif
	APP_LIST_ASSERT_SLOT_LOCK_ORDER(642);
	result = sceKernelLockMutex(app->mutex, 1, NULL);
	if (result != 0) {
		if ((SceUInt32)result + 0x7ffd7fd4u > 1)
			APP_LIST_MUTEX_ERROR(result, 642);
		return result;
	}
	app->mutexOwner = sceKernelGetThreadId();
	result = sceKernelLockMutex(g_appmgrMainMutex, 1, NULL);
	if (result == 0)
		g_appmgrMainMutexOwner = sceKernelGetThreadId();
	else
		APP_LIST_LOCK_ERROR(result, 648);
	APP_LIST_FIND_BY_ID(current, appId);
	if (app != current || current == NULL) {
		SceUID mutex;

		g_appmgrMainMutexOwner = -1;
		result = sceKernelUnlockMutex(g_appmgrMainMutex, 1);
		if (result != 0) {
#if FW_DEBUG
			sceAppMgrDump();
			dumpBgmState();
			if (sceKernelGetAssertLevel() <= 0)
				goto unlockSlot;
			SCE_DBG_ASSERT_PRINTF_LEVEL(1, 0, 652,
				"[SceAppMgr] The actual value is %d(0x%08x), but expected value is %d(0x%08x)\n",
				result, result, 0, 0);
#else
			appmgrAppListLockPanic();
#endif
		}
		APP_LIST_ASSERT_UNLOCKED(653);
#if FW_DEBUG
unlockSlot:
#endif
		mutex = app->mutex;
		app->mutexOwner = -1;
		result = sceKernelUnlockMutex(mutex, 1);
		if (result != 0)
			APP_LIST_MUTEX_ERROR(result, 653);
		return (int)0x80802012;
	}
	g_appmgrMainMutexOwner = -1;
	result = sceKernelUnlockMutex(g_appmgrMainMutex, 1);
	if (result != 0)
		APP_LIST_MUTEX_ERROR(result, 657);
	*output = app;
	return 0;
}

int _appListFindAndLockAppByName(const char *name, struct AppState **output)
{
	struct AppState *app;
	struct AppState *current;
	int result;
	int changed;

	if (output == NULL) {
#if FW_DEBUG
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_FAIL(667);
#else
		appmgrAppListOwnerPanic();
#endif
	}
	result = sceKernelLockMutex(g_appmgrMainMutex, 1, NULL);
	if (result == 0)
		g_appmgrMainMutexOwner = sceKernelGetThreadId();
	else
		APP_LIST_LOCK_ERROR(result, 669);
	APP_LIST_FIND_NAME(app, g_appmgrAppStates, name);
	if (app == NULL) {
		g_appmgrMainMutexOwner = -1;
		result = sceKernelUnlockMutex(g_appmgrMainMutex, 1);
		if (result != 0)
			APP_LIST_MUTEX_ERROR(result, 673);
		return (int)0x80802012;
	}
	g_appmgrMainMutexOwner = -1;
	result = sceKernelUnlockMutex(g_appmgrMainMutex, 1);
	if (result != 0) {
#if FW_DEBUG
		sceAppMgrDump();
		dumpBgmState();
		if (sceKernelGetAssertLevel() <= 0)
			goto checkSlotOrder;
		SCE_DBG_ASSERT_PRINTF_LEVEL(1, 0, 677,
			"[SceAppMgr] The actual value is %d(0x%08x), but expected value is %d(0x%08x)\n",
			result, result, 0, 0);
#else
		appmgrAppListLockPanic();
#endif
	}
	APP_LIST_ASSERT_UNLOCKED(680);
#if FW_DEBUG
checkSlotOrder:
#endif
	APP_LIST_ASSERT_SLOT_LOCK_ORDER(680);
	result = sceKernelLockMutex(app->mutex, 1, NULL);
	if (result != 0) {
		if ((SceUInt32)result + 0x7ffd7fd4u > 1)
			APP_LIST_MUTEX_ERROR(result, 680);
		return result;
	}
	app->mutexOwner = sceKernelGetThreadId();
	result = sceKernelLockMutex(g_appmgrMainMutex, 1, NULL);
	if (result == 0)
		g_appmgrMainMutexOwner = sceKernelGetThreadId();
	else
		APP_LIST_LOCK_ERROR(result, 688);
#if FW_DEBUG
	current = appmgrFindAppByName(g_appmgrAppStates, name);
#else
	APP_LIST_FIND_NAME(current, g_appmgrAppStates, name);
#endif
	changed = app != current || current == NULL;
	g_appmgrMainMutexOwner = -1;
	if (changed) {
		SceUID mutex;

		result = sceKernelUnlockMutex(g_appmgrMainMutex, 1);
		if (result != 0) {
#if FW_DEBUG
			sceAppMgrDump();
			dumpBgmState();
			if (sceKernelGetAssertLevel() <= 0)
				goto unlockSlot;
			SCE_DBG_ASSERT_PRINTF_LEVEL(1, 0, 692,
				"[SceAppMgr] The actual value is %d(0x%08x), but expected value is %d(0x%08x)\n",
				result, result, 0, 0);
#else
			appmgrAppListLockPanic();
#endif
		}
		APP_LIST_ASSERT_UNLOCKED(693);
#if FW_DEBUG
unlockSlot:
#endif
		mutex = app->mutex;
		app->mutexOwner = -1;
		result = sceKernelUnlockMutex(mutex, 1);
		if (result != 0)
			APP_LIST_MUTEX_ERROR(result, 693);
		return (int)0x80802012;
	}
	result = sceKernelUnlockMutex(g_appmgrMainMutex, 1);
	if (result != 0)
		APP_LIST_MUTEX_ERROR(result, 698);
	*output = app;
	return 0;
}

#define APP_LIST_FIND_NAME_EXCEPT(output, name, excluded) do { \
	int slot; \
	(output) = NULL; \
	if ((name) != NULL) { \
		for (slot = 0; slot < 20; ++slot) { \
			struct AppState *candidate = &g_appmgrAppStates[slot]; \
			int id = candidate->appId; \
			if ((excluded) != id && (id & 0x1000) && id > 0 && \
			    !(id & 1) && \
			    strncmp(candidate->process.appName, (name), 32) == 0) { \
				(output) = candidate; \
				break; \
			} \
		} \
	} else { \
		SceUID pid = sceKernelGetProcessId(); \
		for (slot = 0; slot < 20; ++slot) { \
			struct AppState *candidate = &g_appmgrAppStates[slot]; \
			if (candidate->process.processId == pid) { \
				(output) = candidate; \
				break; \
			} \
		} \
	} \
} while (0)

int _appListFindAndLockAppByName2(const char *name, struct AppState **output,
				int excludedAppId)
{
	struct AppState *app;
	struct AppState *current;
	int result;

	if (output == NULL) {
#if FW_DEBUG
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_FAIL(709);
#else
		appmgrAppListOwnerPanic();
#endif
	}
	result = sceKernelLockMutex(g_appmgrMainMutex, 1, NULL);
	if (result == 0)
		g_appmgrMainMutexOwner = sceKernelGetThreadId();
	else
		APP_LIST_LOCK_ERROR(result, 711);
	APP_LIST_FIND_NAME_EXCEPT(app, name, excludedAppId);
	if (app == NULL) {
		g_appmgrMainMutexOwner = -1;
		result = sceKernelUnlockMutex(g_appmgrMainMutex, 1);
		if (result != 0)
			APP_LIST_MUTEX_ERROR(result, 715);
		return (int)0x80802012;
	}
	g_appmgrMainMutexOwner = -1;
	result = sceKernelUnlockMutex(g_appmgrMainMutex, 1);
	if (result != 0) {
#if FW_DEBUG
		sceAppMgrDump();
		dumpBgmState();
		if (sceKernelGetAssertLevel() <= 0)
			goto checkSlotOrder;
		SCE_DBG_ASSERT_PRINTF_LEVEL(1, 0, 719,
			"[SceAppMgr] The actual value is %d(0x%08x), but expected value is %d(0x%08x)\n",
			result, result, 0, 0);
#else
		appmgrAppListLockPanic();
#endif
	}
	APP_LIST_ASSERT_UNLOCKED(722);
#if FW_DEBUG
checkSlotOrder:
#endif
	APP_LIST_ASSERT_SLOT_LOCK_ORDER(722);
	result = sceKernelLockMutex(app->mutex, 1, NULL);
	if (result != 0) {
		if ((SceUInt32)result + 0x7ffd7fd4u > 1)
			APP_LIST_MUTEX_ERROR(result, 722);
		return result;
	}
	app->mutexOwner = sceKernelGetThreadId();
	result = sceKernelLockMutex(g_appmgrMainMutex, 1, NULL);
	if (result == 0)
		g_appmgrMainMutexOwner = sceKernelGetThreadId();
	else
		APP_LIST_LOCK_ERROR(result, 730);
	APP_LIST_FIND_NAME_EXCEPT(current, name, excludedAppId);
	if (app != current || current == NULL) {
		SceUID mutex;

		g_appmgrMainMutexOwner = -1;
		result = sceKernelUnlockMutex(g_appmgrMainMutex, 1);
		if (result != 0) {
#if FW_DEBUG
			sceAppMgrDump();
			dumpBgmState();
			if (sceKernelGetAssertLevel() <= 0)
				goto unlockSlot;
			SCE_DBG_ASSERT_PRINTF_LEVEL(1, 0, 734,
				"[SceAppMgr] The actual value is %d(0x%08x), but expected value is %d(0x%08x)\n",
				result, result, 0, 0);
#else
			appmgrAppListLockPanic();
#endif
		}
		APP_LIST_ASSERT_UNLOCKED(735);
#if FW_DEBUG
unlockSlot:
#endif
		mutex = app->mutex;
		app->mutexOwner = -1;
		result = sceKernelUnlockMutex(mutex, 1);
		if (result != 0)
			APP_LIST_MUTEX_ERROR(result, 735);
		return (int)0x80802012;
	}
	g_appmgrMainMutexOwner = -1;
	result = sceKernelUnlockMutex(g_appmgrMainMutex, 1);
	if (result != 0)
		APP_LIST_MUTEX_ERROR(result, 740);
	*output = app;
	return 0;
}

#define APP_LIST_FIND_ACTIVE(output, scanIndex) do { \
	SceUID shellPid = g_appmgrAppStates[0].process.processId; \
	struct AppState *candidate; \
	int slot; \
	if (shellPid < 0) { \
		slot = 0; \
	} else { \
		slot = shellPid & 1; \
		if (slot) \
			slot = shellPid == g_appmgrShellPid; \
	} \
	candidate = &g_appmgrAppStates[slot]; \
	while (candidate->process.processId == -1 || \
	       (candidate->state & 15) != 2) { \
		++slot; \
		++candidate; \
		if (slot > 19) \
			break; \
	} \
	(output) = candidate; \
	(scanIndex) = slot; \
} while (0)

int _appListFindAndLockActiveApp(struct AppState **output)
{
	struct AppState *app;
	struct AppState *current;
	int result;
	int index;

	if (output == NULL) {
#if FW_DEBUG
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_FAIL(754);
#else
		appmgrAppListOwnerPanic();
#endif
	}
	result = sceKernelLockMutex(g_appmgrMainMutex, 1, NULL);
	if (result == 0)
		g_appmgrMainMutexOwner = sceKernelGetThreadId();
	else
		APP_LIST_LOCK_ERROR(result, 756);
	APP_LIST_FIND_ACTIVE(app, index);
	if (index > 19) {
		g_appmgrMainMutexOwner = -1;
		result = sceKernelUnlockMutex(g_appmgrMainMutex, 1);
		if (result != 0)
			APP_LIST_MUTEX_ERROR(result, 760);
		return (int)0x80802012;
	}
	g_appmgrMainMutexOwner = -1;
	result = sceKernelUnlockMutex(g_appmgrMainMutex, 1);
	if (result != 0) {
#if FW_DEBUG
		sceAppMgrDump();
		dumpBgmState();
		if (sceKernelGetAssertLevel() <= 0)
			goto checkSlotOrder;
		SCE_DBG_ASSERT_PRINTF_LEVEL(1, 0, 764,
			"[SceAppMgr] The actual value is %d(0x%08x), but expected value is %d(0x%08x)\n",
			result, result, 0, 0);
#else
		appmgrAppListLockPanic();
#endif
	}
	APP_LIST_ASSERT_UNLOCKED(766);
#if FW_DEBUG
checkSlotOrder:
#endif
	APP_LIST_ASSERT_SLOT_LOCK_ORDER(766);
	result = sceKernelLockMutex(app->mutex, 1, NULL);
	if (result != 0) {
		if ((SceUInt32)result + 0x7ffd7fd4u > 1)
			APP_LIST_MUTEX_ERROR(result, 766);
		return result;
	}
	app->mutexOwner = sceKernelGetThreadId();
	result = sceKernelLockMutex(g_appmgrMainMutex, 1, NULL);
	if (result == 0)
		g_appmgrMainMutexOwner = sceKernelGetThreadId();
	else
		APP_LIST_LOCK_ERROR(result, 772);
	APP_LIST_FIND_ACTIVE(current, index);
	if (index > 19 || app != current || current == NULL) {
		SceUID mutex;

		g_appmgrMainMutexOwner = -1;
		result = sceKernelUnlockMutex(g_appmgrMainMutex, 1);
		if (result != 0) {
#if FW_DEBUG
			sceAppMgrDump();
			dumpBgmState();
			if (sceKernelGetAssertLevel() <= 0)
				goto unlockSlot;
			SCE_DBG_ASSERT_PRINTF_LEVEL(1, 0, 776,
				"[SceAppMgr] The actual value is %d(0x%08x), but expected value is %d(0x%08x)\n",
				result, result, 0, 0);
#else
			appmgrAppListLockPanic();
#endif
		}
		APP_LIST_ASSERT_UNLOCKED(777);
#if FW_DEBUG
unlockSlot:
#endif
		mutex = app->mutex;
		app->mutexOwner = -1;
		result = sceKernelUnlockMutex(mutex, 1);
		if (result != 0)
			APP_LIST_MUTEX_ERROR(result, 777);
		return (int)0x80802012;
	}
	g_appmgrMainMutexOwner = -1;
	result = sceKernelUnlockMutex(g_appmgrMainMutex, 1);
	if (result != 0)
		APP_LIST_MUTEX_ERROR(result, 781);
	*output = app;
	return 0;
}

int _appListFindAndLockAppByProcessId(SceUID pid, struct AppState **output)
{
	struct AppState *app;
	struct AppState *current;
	int result;
	int changed;

	if (output == NULL) {
#if FW_DEBUG
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_FAIL(791);
#else
		appmgrAppListOwnerPanic();
#endif
	}
	result = sceKernelLockMutex(g_appmgrMainMutex, 1, NULL);
	if (result == 0)
		g_appmgrMainMutexOwner = sceKernelGetThreadId();
	else
		APP_LIST_LOCK_ERROR(result, 793);
	app = appmgrFindSpecialApp(g_appmgrAppStates, pid);
	g_appmgrMainMutexOwner = -1;
	if (app == NULL) {
		result = sceKernelUnlockMutex(g_appmgrMainMutex, 1);
		if (result != 0)
			APP_LIST_MUTEX_ERROR(result, 797);
		return (int)0x80802012;
	}
	result = sceKernelUnlockMutex(g_appmgrMainMutex, 1);
	if (result != 0) {
#if FW_DEBUG
		sceAppMgrDump();
		dumpBgmState();
		if (sceKernelGetAssertLevel() <= 0)
			goto checkSlotOrder;
		SCE_DBG_ASSERT_PRINTF_LEVEL(1, 0, 801,
			"[SceAppMgr] The actual value is %d(0x%08x), but expected value is %d(0x%08x)\n",
			result, result, 0, 0);
#else
		appmgrAppListLockPanic();
#endif
	}
	APP_LIST_ASSERT_UNLOCKED(803);
#if FW_DEBUG
checkSlotOrder:
#endif
	APP_LIST_ASSERT_SLOT_LOCK_ORDER(803);
	result = sceKernelLockMutex(app->mutex, 1, NULL);
	if (result != 0) {
		if ((SceUInt32)result + 0x7ffd7fd4u > 1)
			APP_LIST_MUTEX_ERROR(result, 803);
		return result;
	}
	app->mutexOwner = sceKernelGetThreadId();
	result = sceKernelLockMutex(g_appmgrMainMutex, 1, NULL);
	if (result == 0)
		g_appmgrMainMutexOwner = sceKernelGetThreadId();
	else
		APP_LIST_LOCK_ERROR(result, 809);
	current = appmgrFindSpecialApp(g_appmgrAppStates, pid);
	changed = app != current || current == NULL;
	g_appmgrMainMutexOwner = -1;
	if (changed) {
		SceUID mutex;

		result = sceKernelUnlockMutex(g_appmgrMainMutex, 1);
		if (result != 0) {
#if FW_DEBUG
			sceAppMgrDump();
			dumpBgmState();
			if (sceKernelGetAssertLevel() <= 0)
				goto unlockSlot;
			SCE_DBG_ASSERT_PRINTF_LEVEL(1, 0, 813,
				"[SceAppMgr] The actual value is %d(0x%08x), but expected value is %d(0x%08x)\n",
				result, result, 0, 0);
#else
			appmgrAppListLockPanic();
#endif
		}
		APP_LIST_ASSERT_UNLOCKED(814);
#if FW_DEBUG
unlockSlot:
#endif
		mutex = app->mutex;
		app->mutexOwner = -1;
		result = sceKernelUnlockMutex(mutex, 1);
		if (result != 0)
			APP_LIST_MUTEX_ERROR(result, 814);
		return (int)0x80802012;
	}
	result = sceKernelUnlockMutex(g_appmgrMainMutex, 1);
	if (result != 0)
		APP_LIST_MUTEX_ERROR(result, 818);
	*output = app;
	return 0;
}

int _appListUnlockApp(struct AppState *app)
{
	int result;

	APP_LIST_ASSERT_UNLOCKED(928);
	app->mutexOwner = -1;
	result = sceKernelUnlockMutex(app->mutex, 1);
	if (result != 0)
		APP_LIST_MUTEX_ERROR(result, 928);
	return result;
}
