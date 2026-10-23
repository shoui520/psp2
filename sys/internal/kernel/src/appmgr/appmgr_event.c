/* Application events and launch-parameter queues. */

#include <sysmem/debug.h>
#include <intrmgr/syscall.h>
#include <threadmgr/threadmgr.h>
#include <sysmem/sysclib.h>
#include <sysmem/sysmem.h>
#include <acmgr/acmgr.h>
#include <appmgr.h>
#include "app_state_internal.h"
#include "appmgr_internal_util.h"
#include "appmgr_event.h"
#include <appmgr/appmgr.h>
#include "appmgr_app_event.h"
#include "debug_setting.h"
#include "appmgr_device.h"

extern SceUID g_appmgrMainMutex;
extern SceUID g_appmgrMainMutexOwner;

AppmgrNotificationQueue g_appmgrNotificationQueue;
unsigned char g_appmgrNotificationBuffer[4][64];

#if !FW_DEBUG
static const SceKernelDebugInfo notificationOverflowPanicInfo = {
	.fileHash = 0xc9656e8e,
	.lineHash = 0xc879dadc,
	.funcHash = 0x11a05ee1,
};

static const SceKernelDebugInfo notificationUnlockPanicInfo = {
	.fileHash = 0xeba8d78f,
	.lineHash = 0x6301b66f,
	.funcHash = 0xf9bbda9f,
};

static const SceKernelDebugInfo notificationWriteSemaphorePanicInfo = {
	.fileHash = 0xeba8d78f,
	.lineHash = 0x81f72573,
	.funcHash = 0xc08853dd,
};

static const SceKernelDebugInfo notificationMutexPanicInfo = {
	.fileHash = 0xeba8d78f,
	.lineHash = 0x4e6666f6,
	.funcHash = 0xc08853dd,
};

static const SceKernelDebugInfo notificationReadSemaphorePanicInfo = {
	.fileHash = 0xeba8d78f,
	.lineHash = 0x94817049,
	.funcHash = 0xc08853dd,
};

static const SceKernelDebugInfo notificationLockPanicInfo = {
	.fileHash = 0xeba8d78f,
	.lineHash = 0xcf3b207c,
	.funcHash = 0xf9bbda9f,
};

static const SceKernelDebugInfo notificationRestoreSlotPanicInfo = {
	.fileHash = 0xeba8d78f,
	.lineHash = 0x1412d790,
	.funcHash = 0xf9bbda9f,
};

static const SceKernelDebugInfo notificationWaitSlotPanicInfo = {
	.fileHash = 0xeba8d78f,
	.lineHash = 0x4c8e56bf,
	.funcHash = 0xf9bbda9f,
};

static const SceKernelDebugInfo notificationPollSlotPanicInfo = {
	.fileHash = 0xeba8d78f,
	.lineHash = 0x0e4f5492,
	.funcHash = 0xf9bbda9f,
};

static const SceKernelDebugInfo notificationPollEntryPanicInfo = {
	.fileHash = 0xeba8d78f,
	.lineHash = 0xb2309eed,
	.funcHash = 0x7304705f,
};

static const SceKernelDebugInfo notificationPopLockPanicInfo = {
	.fileHash = 0xeba8d78f,
	.lineHash = 0x5c73386d,
	.funcHash = 0x7304705f,
};

static const SceKernelDebugInfo notificationReleaseSlotPanicInfo = {
	.fileHash = 0xeba8d78f,
	.lineHash = 0x146956f5,
	.funcHash = 0x7304705f,
};

static const SceKernelDebugInfo notificationRestoreEntryPanicInfo = {
	.fileHash = 0xeba8d78f,
	.lineHash = 0x783b0802,
	.funcHash = 0x7304705f,
};

static const SceKernelDebugInfo notificationPopUnlockPanicInfo = {
	.fileHash = 0xeba8d78f,
	.lineHash = 0x7fc9eb8a,
	.funcHash = 0x7304705f,
};

static const SceKernelDebugInfo notificationSignalPanicInfo = {
	.fileHash = 0xeba8d78f,
	.lineHash = 0x9c45d46a,
	.funcHash = 0xf9bbda9f,
};

void appmgrNotificationOverflowPanic(void)
{
	SceDebugForDriver_391B5B74(&notificationOverflowPanicInfo, __builtin_return_address(0));
}

static const SceKernelDebugInfo paramQueueMutexPanicInfo = {
	.fileHash = 0xc9656e8e,
	.lineHash = 0x79d3a1f0,
	.funcHash = 0x9d536bce,
};

_Noreturn void appmgrParamQueueMutexPanic(void)
{
	SceDebugForDriver_391B5B74(&paramQueueMutexPanicInfo, __builtin_return_address(0));
}
#endif

#if FW_DEBUG
#define PARAM_UNLOCK_APP(_app, _line, _func) do { \
	int unlockResult = _appListUnlockApp(_app); \
	if (unlockResult != 0) { \
		sceAppMgrDump(); \
		dumpBgmState(); \
		SCE_DBG_ASSERT_PRINTF_FUNC(0, _line, _func, \
			"[SceAppMgr] The actual value is %d(0x%08x), but expected value is %d(0x%08x)\n", \
			unlockResult, unlockResult, 0, 0); \
	} \
} while (0)
#else
#define PARAM_UNLOCK_APP(_app, _line, _func) do { \
	if (_appListUnlockApp(_app) != 0) \
		appmgrNotificationOverflowPanic(); \
} while (0)
#endif

int _sendParam(const char *name, const AppmgrParamQueueEntry *entry,
	       unsigned int *flags, int excludedAppId)
{
	struct AppState *app;
	struct AppState *exitingApp;
	struct AppmgrWaitObject *waitObject;
	SceAppMgrSystemEvent event;
	SceUInt32 timeout;
	SceUID callbackId, sourcePid;
	int result, processId, sourceProcessId;

	if (g_appmgrMainMutexOwner == sceKernelGetThreadId()) {
#if FW_DEBUG
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_FAIL(56);
#else
		appmgrNotificationOverflowPanic();
#endif
	}
	if (flags == NULL) {
#if FW_DEBUG
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_FAIL(59);
#else
		appmgrNotificationOverflowPanic();
#endif
	}
	for (;;) {
		app = NULL;
		waitObject = appmgrUtilGetWaitObject(3);
		for (;;) {
			exitingApp = NULL;
			result = _appListFindAndLockAppByName2(name, &exitingApp,
							 excludedAppId);
			if (result != 0)
				break;
			if (exitingApp->quitRequested == 0) {
				PARAM_UNLOCK_APP(exitingApp, 47, "cleaningUpExittingApp");
				break;
			}
			result = _appmgrProcessQuitProcess(exitingApp,
						 exitingApp->exitReason, 0, 0);
			if (result != 0) {
				PARAM_UNLOCK_APP(exitingApp, 34, "cleaningUpExittingApp");
				return result;
			}
			PARAM_UNLOCK_APP(exitingApp, 40, "cleaningUpExittingApp");
			timeout = 15000000;
			appmgrUtilWaitObjectWait(waitObject, &timeout);
		}
		result = _appListFindAndLockAppByName2(name, &app, excludedAppId);
		if (result != 0)
			return result;
		if (app->quitRequested == 0)
			break;
		PARAM_UNLOCK_APP(app, 76, "_sendParam");
	}
	result = sceKernelLockMutex(g_appmgrMainMutex, 1, NULL);
	if (result != 0) {
#if FW_DEBUG
		SCE_DBG_PRINTF(0, 82, "Failed to lock ret=0x%08x\n", result);
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_FAIL(82);
#else
		appmgrParamQueueMutexPanic();
#endif
	} else {
		g_appmgrMainMutexOwner = sceKernelGetThreadId();
	}
	if (app->launchInfo.budgetType == 1 && entry->param[0] == 0) {
		if (entry->callbackId > 0 &&
		    sceKernelNotifyCallback(entry->callbackId, 0) != 0)
			SCE_DBG_PRINTF(0, 90,
				"[SceAppMgr:Error] Notifying param pop callback failed\n");
	} else {
		callbackId = entry->callbackId;
		sourcePid = entry->sourcePid;
		if (app->params.count == 4) {
			if (callbackId > 0 &&
			    sceKernelNotifyCallback(callbackId, (int)0x8080201b) != 0)
				SCE_DBG_PRINTF(0, 104,
					"[SceAppMgr:Error] Notifying param pop callback failed\n");
			*flags |= 1;
		} else {
			appmgrParamQueuePush(&app->params, entry->param, callbackId,
					    sourcePid, entry->sourceAppName);
			sourceProcessId = app->launchSourceProcessId;
			memset(&event, 0, sizeof(event));
			event.systemEvent = 0x20000004;
			memcpy(event.data.reserved, &sourceProcessId, sizeof(sourceProcessId));
			result = _appListPushEvent(app, &event);
			if (result != 0) {
#if FW_DEBUG
				sceAppMgrDump();
				dumpBgmState();
				SCE_DBG_ASSERT_PRINTF_FUNC(0, 50, "utilSendEvent",
					"[SceAppMgr] The actual value is %d(0x%08x), but expected value is %d(0x%08x)\n",
					result, result, 0, 0);
				sceAppMgrDump();
				dumpBgmState();
				SCE_DBG_ASSERT_PRINTF(0, 98,
					"[SceAppMgr] The actual value is %d(0x%08x), but expected value is %d(0x%08x)\n",
					result, result, 0, 0);
#else
				appmgrNotificationOverflowPanic();
#endif
			}
		}
	}
	app->paramDeliveryHandled = 1;
	processId = app->process.processId;
	g_appmgrMainMutexOwner = -1;
	result = sceKernelUnlockMutex(g_appmgrMainMutex, 1);
	if (result != 0) {
#if FW_DEBUG
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_PRINTF(0, 121,
			"[SceAppMgr] The actual value is %d(0x%08x), but expected value is %d(0x%08x)\n",
			result, result, 0, 0);
#else
		appmgrParamQueueMutexPanic();
#endif
	}
	_appListUnlockApp(app);
	return processId;
}

#if FW_DEBUG
#define PARAM_LOCK(_line) do { \
	int lockResult = sceKernelLockMutex(g_appmgrMainMutex, 1, NULL); \
	if (lockResult != 0) { \
		SCE_DBG_PRINTF(0, _line, "Failed to lock ret=0x%08x\n", lockResult); \
		sceAppMgrDump(); \
		dumpBgmState(); \
		SCE_DBG_ASSERT_FAIL(_line); \
	} else { \
		g_appmgrMainMutexOwner = sceKernelGetThreadId(); \
	} \
} while (0)
#define PARAM_UNLOCK(_line) do { \
	int unlockResult; \
	g_appmgrMainMutexOwner = -1; \
	unlockResult = sceKernelUnlockMutex(g_appmgrMainMutex, 1); \
	if (unlockResult != 0) { \
		sceAppMgrDump(); \
		dumpBgmState(); \
		SCE_DBG_ASSERT_PRINTF(0, _line, \
			"[SceAppMgr] The actual value is %d(0x%08x), but expected value is %d(0x%08x)\n", \
			unlockResult, unlockResult, 0, 0); \
	} \
} while (0)
#else
#define PARAM_LOCK(_line) do { \
	if (sceKernelLockMutex(g_appmgrMainMutex, 1, NULL) != 0) \
		appmgrParamQueueMutexPanic(); \
	g_appmgrMainMutexOwner = sceKernelGetThreadId(); \
} while (0)
#define PARAM_UNLOCK(_line) do { \
	g_appmgrMainMutexOwner = -1; \
	if (sceKernelUnlockMutex(g_appmgrMainMutex, 1) != 0) \
		appmgrParamQueueMutexPanic(); \
} while (0)
#endif

int sceAppMgrPeekLaunchParamForShell(int appId, char *param)
{
	SceUInt32 syscallState = sceSyscallEnter();
	struct AppState *shell;
	struct AppState *app;
	const char *source;
	int count;
	int result;

	if (!SceSblACMgrForDriver_991FDC15(0) ||
	    (shell = appmgrGetShellApp()) == NULL ||
	    shell->process.processId != sceKernelGetProcessId()) {
		result = (int)0x8080201f;
		goto leave;
	}
	if (param == NULL) {
		result = (int)0x80802016;
		goto leave;
	}
	PARAM_LOCK(209);
	if (appmgrGetShellApp()->process.processId != sceKernelGetProcessId()) {
		PARAM_UNLOCK(212);
		result = (int)0x8080201c;
		goto leave;
	}
	app = appmgrFindAppById(appId);
	if (app == NULL) {
		PARAM_UNLOCK(220);
		result = (int)0x80802012;
		goto leave;
	}
	if (appmgrGetAppRunStatus(app) != 0x300) {
		PARAM_UNLOCK(227);
		result = (int)0x80802013;
		goto leave;
	}
	count = app->params.count;
	if (count == 0) {
#if FW_DEBUG
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_FAIL(234);
#else
		appmgrNotificationOverflowPanic();
#endif
		source = NULL;
	} else {
		source = app->params.entries[
			(app->params.writeIndex + 4 - count) % 4].param;
	}
	strncpy(param, source, 1024);
	PARAM_UNLOCK(237);
	result = 0;
leave:
	sceSyscallLeave(syscallState);
	return result;
}

int sceAppMgrOverwriteLaunchParamForShell(int appId, const char *param)
{
	SceUInt32 syscallState = sceSyscallEnter();
	struct AppState *shell;
	struct AppState *app;
	int count;
	int result;

	if (!SceSblACMgrForDriver_991FDC15(0) ||
	    (shell = appmgrGetShellApp()) == NULL ||
	    shell->process.processId != sceKernelGetProcessId()) {
		result = (int)0x8080201f;
		goto leave;
	}
	if (param == NULL) {
		result = (int)0x80802016;
		goto leave;
	}
	PARAM_LOCK(264);
	if (appmgrGetShellApp()->process.processId != sceKernelGetProcessId()) {
		PARAM_UNLOCK(267);
		result = (int)0x8080201c;
		goto leave;
	}
	app = appmgrFindAppById(appId);
	if (app == NULL) {
		PARAM_UNLOCK(275);
		result = (int)0x80802012;
		goto leave;
	}
	if (appmgrGetAppRunStatus(app) != 0x300) {
		PARAM_UNLOCK(282);
		result = (int)0x80802013;
		goto leave;
	}
	count = app->params.count;
	if (count != 0)
		strncpy(app->params.entries[
			(app->params.writeIndex + 4 - count) % 4].param, param, 1024);
	else
		appmgrParamQueuePush(&app->params, param, -1, -1, "");
	PARAM_UNLOCK(291);
	result = 0;
leave:
	sceSyscallLeave(syscallState);
	return result;
}

int appmgrParamQueueInit(AppmgrParamQueue *queue)
{
	memset(queue, 0, sizeof(*queue));
	return 0;
}

int appmgrParamQueuePush(AppmgrParamQueue *queue, const char *param,
		SceUID callbackId, SceUID sourcePid, const char *sourceAppName)
{
	AppmgrParamQueueEntry *entry;
	int nextIndex;

	if (queue->count == 4)
		return 0x80802014;
	if (param)
		strncpy(queue->entries[queue->writeIndex].param, param, 1024);
	else
		memset(queue->entries[queue->writeIndex].param, 0, 1024);
	entry = &queue->entries[queue->writeIndex];
	entry->callbackId = callbackId;
	entry->sourcePid = sourcePid;
	if (sourceAppName)
		strncpy(entry->sourceAppName, sourceAppName, 32);
	else
		memset(entry->sourceAppName, 0, 32);
	nextIndex = queue->writeIndex + 1;
	++queue->count;
	queue->writeIndex = nextIndex % 4;
	return 0;
}

const char *appmgrParamQueuePeek(const AppmgrParamQueue *queue)
{
	if (!queue->count)
		return NULL;
	return queue->entries[(queue->writeIndex + 4 - queue->count) % 4].param;
}

int _paramQueueCancelAll(AppmgrParamQueue *queue)
{
	int count = queue->count;
	int first = (queue->writeIndex + 4 - count) % 4;
	int index = 0;
	SceUID callbackId;
	int result;

	if (count > 0) {
		do {
			callbackId = queue->entries[(index + first) % 4].callbackId;
			if (callbackId > 0) {
				result = sceKernelNotifyCallback(callbackId, 0x8080201b);
#if FW_DEBUG
				if (result)
					SCE_DBG_PRINTF_FUNC(0, 427, "_paramQueueCancelAll",
						"[SceAppMgr:Error] Notifying param cancel callback failed\n");
#else
				(void)result;
#endif
				count = queue->count;
			}
			++index;
		} while (index < count);
	}
	queue->count = 0;
	return 0;
}

/* Drops the queued parameters that were taken (their string emptied), oldest first. */
void _paramQueueCompact(AppmgrParamQueue *queue)
{
	int count = queue->count;
	int writeIndex = queue->writeIndex;
	int first, index, slot;

	for (;;) {
		first = (writeIndex + 4 - count) % 4;
		if (count <= 0)
			return;
		index = queue->entries[first % 4].param[0];
		if (index != 0) {
			for (index = 1; index != count; index++) {
				if (queue->entries[(first + index) % 4].param[0] == '\0')
					break;
			}
			if (index == count)
				return;
		}
		count--;
		for (; index < count; index++) {
			slot = (index + first) % 4;
			memcpy(&queue->entries[slot], &queue->entries[slot + 1],
				sizeof(queue->entries[slot]));
		}
		queue->count = count;
		writeIndex = (writeIndex + 3) % 4;
		queue->writeIndex = writeIndex;
	}
}

/* Takes the oldest parameter, telling its sender's callback it was taken. */
static inline int _paramQueuePop(AppmgrParamQueue *queue, char *param,
				 char *sourceAppName)
{
	AppmgrParamQueueEntry *entry;
	int count = queue->count;
	int result;

	if (count == 0)
		return 0x80802015;
	entry = &queue->entries[(queue->writeIndex + 4 - count) % 4];
	memcpy(param, entry->param, sizeof(entry->param));
	if (sourceAppName != NULL)
		__builtin_memcpy(sourceAppName, entry->sourceAppName,
				 sizeof(entry->sourceAppName));
	if (entry->callbackId > 0) {
		result = sceKernelNotifyCallback(entry->callbackId, 0);
#if FW_DEBUG
		if (result)
			SCE_DBG_PRINTF_FUNC(0, 354, "_paramQueuePop",
				"[SceAppMgr:Error] Notifying param pop callback failed\n");
#else
		(void)result;
#endif
	}
	queue->count--;
	return 0;
}

/*
 * The caller's application takes its oldest launch parameter; while it is in
 * run status 0x300 only the shell may read (not take) it.
 */
int _sceAppMgrGetParam(char *param)
{
	SceUInt32 syscallState = sceSyscallEnter();
	struct AppState *app;
	const char *source;
	int count;
	int result;

	if (param == NULL) {
		sceSyscallLeave(syscallState);
		return 0x80802016;
	}
	memset(param, 0, 1024);
	PARAM_LOCK(145);
	app = appmgrFindAppByProcessId(g_appmgrAppStates, -2);
	if (app == NULL) {
		PARAM_UNLOCK(150);
		sceSyscallLeave(syscallState);
		return 0x80802012;
	}
	if (app->process.processId != sceKernelGetProcessId() &&
	    appmgrGetShellApp()->process.processId != sceKernelGetProcessId()) {
		PARAM_UNLOCK(158);
		sceSyscallLeave(syscallState);
		return 0x8080201c;
	}
	if (appmgrGetAppRunStatus(app) != 0x300) {
		result = _paramQueuePop(&app->params, param, NULL);
	} else if (isShellProcess()) {
		count = app->params.count;
		if (count == 0) {
#if FW_DEBUG
			sceAppMgrDump();
			dumpBgmState();
			SCE_DBG_ASSERT_FAIL(170);
#else
			appmgrNotificationOverflowPanic();
#endif
			source = NULL;
		} else {
			source = app->params.entries[
				(app->params.writeIndex + 4 - count) % 4].param;
		}
		strncpy(param, source, 1024);
		result = 0;
	} else {
		PARAM_UNLOCK(165);
		sceSyscallLeave(syscallState);
		return 0x80802013;
	}
	PARAM_UNLOCK(182);
	sceSyscallLeave(syscallState);
	return result;
}

/* _sceAppMgrGetParam for system programs, with the sender's name. */
int sceAppMgrGetAppParam2(AppmgrAppParam *appParam)
{
	SceUInt32 syscallState = sceSyscallEnter();
	struct AppState *app;
	const AppmgrParamQueueEntry *entry;
	int count;
	int result;

	if (!sceSblACMgrIsSystemProgram(0)) {
		sceSyscallLeave(syscallState);
		return 0x8080201f;
	}
	if (appParam == NULL) {
		sceSyscallLeave(syscallState);
		return 0x80802016;
	}
	memset(appParam, 0, sizeof(*appParam));
	PARAM_LOCK(504);
	app = appmgrFindAppByProcessId(g_appmgrAppStates, -2);
	if (app == NULL) {
		PARAM_UNLOCK(508);
		sceSyscallLeave(syscallState);
		return 0x80802012;
	}
	if (appmgrGetAppRunStatus(app) != 0x300) {
		result = _paramQueuePop(&app->params, appParam->param,
					appParam->sourceAppName);
	} else if (isShellProcess()) {
		count = app->params.count;
		if (count == 0) {
#if FW_DEBUG
			sceAppMgrDump();
			dumpBgmState();
			SCE_DBG_ASSERT_FAIL(521);
#else
			appmgrNotificationOverflowPanic();
#endif
			entry = NULL;
		} else {
			entry = &app->params.entries[
				(app->params.writeIndex + 4 - count) % 4];
		}
		strncpy(appParam->param, entry->param, 1024);
		strncpy(appParam->sourceAppName, entry->sourceAppName, 32);
		result = 0;
	} else {
		PARAM_UNLOCK(516);
		sceSyscallLeave(syscallState);
		return 0x80802013;
	}
	PARAM_UNLOCK(537);
	sceSyscallLeave(syscallState);
	return result;
}

/*
 * A system program takes the extra launch parameters (into its own memory),
 * once, when its launch allowed them.
 */
int sceAppMgrGetExtraAppParam(AppmgrExtraAppParam *extraParam)
{
	SceUInt32 syscallState = sceSyscallEnter();
	struct AppState *app;
	int result;

	if (!sceSblACMgrIsSystemProgram(0)) {
		sceSyscallLeave(syscallState);
		return 0x8080201f;
	}
	if (extraParam == NULL) {
		sceSyscallLeave(syscallState);
		return 0x80802016;
	}
	PARAM_LOCK(561);
	app = appmgrFindAppByProcessId(g_appmgrAppStates, -2);
	if (app == NULL) {
		PARAM_UNLOCK(565);
		sceSyscallLeave(syscallState);
		return 0x80802012;
	}
	if (!app->extraAppParamAllowed) {
		PARAM_UNLOCK(571);
		sceSyscallLeave(syscallState);
		return 0x8080201a;
	}
	if (g_appmgrExtraAppParam.argumentSize == 0) {
		PARAM_UNLOCK(577);
		sceSyscallLeave(syscallState);
		return 0x80802013;
	}
	result = sceKernelCopyToUser(extraParam, &g_appmgrExtraAppParam,
				     sizeof(g_appmgrExtraAppParam));
	if (result >= 0)
		g_appmgrExtraAppParam.argumentSize = 0;
	PARAM_UNLOCK(586);
	sceSyscallLeave(syscallState);
	return result;
}

int _appmgrQueueInit(void)
{
	SceUID uid;

	g_appmgrNotificationQueue.buffer = g_appmgrNotificationBuffer[0];
	g_appmgrNotificationQueue.entrySize = 64;
	g_appmgrNotificationQueue.count = 0;
	g_appmgrNotificationQueue.writeIndex = 0;
	g_appmgrNotificationQueue.capacity = 4;
	uid = sceKernelCreateSema("SceAppMgrNtfQ", 0x8000, 0, 4, NULL);
	g_appmgrNotificationQueue.readSemaphore = uid;
	if (uid <= 0) {
#if FW_DEBUG
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_CHECK_FUNC(0, 620, "_appmgrQueueInit");
#else
		SceDebugForDriver_391B5B74(&notificationReadSemaphorePanicInfo,
			__builtin_return_address(0));
#endif
	}
	uid = sceKernelCreateSema("SceAppMgrNtfQ", 0x8000, 4, 4, NULL);
	g_appmgrNotificationQueue.writeSemaphore = uid;
	if (uid <= 0) {
#if FW_DEBUG
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_CHECK_FUNC(0, 622, "_appmgrQueueInit");
#else
		SceDebugForDriver_391B5B74(&notificationWriteSemaphorePanicInfo,
			__builtin_return_address(0));
#endif
	}
	uid = sceKernelCreateMutex("SceAppMgrNtfQ", 0x8000, 0, NULL);
	g_appmgrNotificationQueue.mutex = uid;
	if (uid < 0) {
#if FW_DEBUG
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_CHECK_FUNC(0, 624, "_appmgrQueueInit");
#else
		SceDebugForDriver_391B5B74(&notificationMutexPanicInfo,
			__builtin_return_address(0));
#endif
	}
	return 0;
}

int _appmgrQueuePop(AppmgrNotificationRequest *request)
{
	SceUInt32 syscallState = sceSyscallEnter();
	struct AppState *app;
	SceUInt32 count, index;
	int result, lockResult;

	if (!SceSblACMgrForDriver_991FDC15(0) || !(app = appmgrGetShellApp()) ||
	    app->process.processId != sceKernelGetProcessId()) {
		sceSyscallLeave(syscallState);
		return 0x8080201c;
	}
	if (!request) {
		sceSyscallLeave(syscallState);
		return 0x80802016;
	}
	result = sceKernelPollSema(g_appmgrNotificationQueue.readSemaphore, 1);
	if (result) {
		if (result != (int)0x80028102) {
#if FW_DEBUG
			sceAppMgrDump();
			dumpBgmState();
			SCE_DBG_ASSERT_PRINTF_FUNC(0, 691, "_appmgrQueuePop",
				"[SceAppMgr] The actual value is %d(0x%08x), but expected value is %d(0x%08x)\n",
				result, result, (int)0x80028102, (int)0x80028102);
#else
			SceDebugForDriver_00CCE39C(&notificationPollEntryPanicInfo,
				__builtin_return_address(0), "0x%08x,0x%08x\n",
				result, (int)0x80028102);
#endif
		}
		sceSyscallLeave(syscallState);
		return 0x80802024;
	}
	lockResult = sceKernelLockMutex(g_appmgrNotificationQueue.mutex, 1, NULL);
	if (lockResult) {
		if ((SceUInt32)lockResult + 0x7ffd7fd4 > 1) {
#if FW_DEBUG
			sceAppMgrDump();
			dumpBgmState();
			SCE_DBG_ASSERT_PRINTF_FUNC(0, 699, "_appmgrQueuePop",
				"[SceAppMgr] The actual value is %d(0x%08x), but expected value is %d(0x%08x)\n",
				lockResult, lockResult, 0, 0);
#else
			SceDebugForDriver_00CCE39C(&notificationPopLockPanicInfo,
				__builtin_return_address(0), "0x%08x,0x%08x\n", lockResult, 0);
#endif
		}
		result = sceKernelSignalSema(g_appmgrNotificationQueue.readSemaphore, 1);
		if (result) {
#if FW_DEBUG
			sceAppMgrDump();
			dumpBgmState();
			SCE_DBG_ASSERT_PRINTF_FUNC(0, 702, "_appmgrQueuePop",
				"[SceAppMgr] The actual value is %d(0x%08x), but expected value is %d(0x%08x)\n",
				result, result, 0, 0);
#else
			SceDebugForDriver_00CCE39C(&notificationRestoreEntryPanicInfo,
				__builtin_return_address(0), "0x%08x,0x%08x\n", result, 0);
#endif
		}
		sceSyscallLeave(syscallState);
		return lockResult;
	}
	count = g_appmgrNotificationQueue.count;
	if (!count) {
#if FW_DEBUG
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_CHECK_FUNC(0, 706, "_appmgrQueuePop");
		count = g_appmgrNotificationQueue.count;
#else
		appmgrNotificationOverflowPanic();
#endif
	}
	index = g_appmgrNotificationQueue.writeIndex;
	if (index < count)
		index += g_appmgrNotificationQueue.capacity;
	memcpy(request, g_appmgrNotificationQueue.buffer +
		g_appmgrNotificationQueue.entrySize * (index - count),
		g_appmgrNotificationQueue.entrySize);
	--g_appmgrNotificationQueue.count;
	result = sceKernelUnlockMutex(g_appmgrNotificationQueue.mutex, 1);
	if (result) {
#if FW_DEBUG
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_PRINTF_FUNC(0, 719, "_appmgrQueuePop",
			"[SceAppMgr] The actual value is %d(0x%08x), but expected value is %d(0x%08x)\n",
			result, result, 0, 0);
#else
		SceDebugForDriver_00CCE39C(&notificationPopUnlockPanicInfo,
			__builtin_return_address(0), "0x%08x,0x%08x\n", result, 0);
#endif
	}
	result = sceKernelSignalSema(g_appmgrNotificationQueue.writeSemaphore, 1);
	if (result) {
#if FW_DEBUG
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_PRINTF_FUNC(0, 722, "_appmgrQueuePop",
			"[SceAppMgr] The actual value is %d(0x%08x), but expected value is %d(0x%08x)\n",
			result, result, 0, 0);
#else
		SceDebugForDriver_00CCE39C(&notificationReleaseSlotPanicInfo,
			__builtin_return_address(0), "0x%08x,0x%08x\n", result, 0);
#endif
	}
	sceSyscallLeave(syscallState);
	return 0;
}

int _appmgrQueuePush(const AppmgrNotificationRequest *request, int wait)
{
	SceUInt32 syscallState = sceSyscallEnter();
	int result;

	if (!request) {
		sceSyscallLeave(syscallState);
		return 0x80802016;
	}
	if (request->type != 0) {
		sceSyscallLeave(syscallState);
		return 0x8080201a;
	}
	if (appmgrIsTrcCheckNotifyEnabled() != 1)
		goto done;
	if (wait) {
		result = sceKernelWaitSema(g_appmgrNotificationQueue.writeSemaphore,
			1, NULL);
		if (result) {
			if ((SceUInt32)result + 0x7ffd7fd4 > 1) {
#if FW_DEBUG
				sceAppMgrDump();
				dumpBgmState();
				SCE_DBG_ASSERT_PRINTF_FUNC(0, 637, "_appmgrQueuePush",
					"[SceAppMgr] The actual value is %d(0x%08x), but expected value is %d(0x%08x)\n",
					result, result, 0, 0);
#else
				SceDebugForDriver_00CCE39C(&notificationWaitSlotPanicInfo,
					__builtin_return_address(0), "0x%08x,0x%08x\n", result, 0);
#endif
			}
			goto done;
		}
	} else {
		result = sceKernelPollSema(g_appmgrNotificationQueue.writeSemaphore, 1);
		if (result) {
			if (result != (int)0x80028102) {
#if FW_DEBUG
				sceAppMgrDump();
				dumpBgmState();
				SCE_DBG_ASSERT_PRINTF_FUNC(0, 644, "_appmgrQueuePush",
					"[SceAppMgr] The actual value is %d(0x%08x), but expected value is %d(0x%08x)\n",
					result, result, (int)0x80028102, (int)0x80028102);
#else
				SceDebugForDriver_00CCE39C(&notificationPollSlotPanicInfo,
					__builtin_return_address(0), "0x%08x,0x%08x\n",
					result, (int)0x80028102);
#endif
			}
			goto done;
		}
	}
	result = sceKernelLockMutex(g_appmgrNotificationQueue.mutex, 1, NULL);
	if (result) {
		if ((SceUInt32)result + 0x7ffd7fd4 > 1) {
#if FW_DEBUG
			sceAppMgrDump();
			dumpBgmState();
			SCE_DBG_ASSERT_PRINTF_FUNC(0, 652, "_appmgrQueuePush",
				"[SceAppMgr] The actual value is %d(0x%08x), but expected value is %d(0x%08x)\n",
				result, result, 0, 0);
#else
			SceDebugForDriver_00CCE39C(&notificationLockPanicInfo,
				__builtin_return_address(0), "0x%08x,0x%08x\n", result, 0);
#endif
		}
		result = sceKernelSignalSema(g_appmgrNotificationQueue.writeSemaphore, 1);
		if (result) {
#if FW_DEBUG
			sceAppMgrDump();
			dumpBgmState();
			SCE_DBG_ASSERT_PRINTF_FUNC(0, 655, "_appmgrQueuePush",
				"[SceAppMgr] The actual value is %d(0x%08x), but expected value is %d(0x%08x)\n",
				result, result, 0, 0);
#else
			SceDebugForDriver_00CCE39C(&notificationRestoreSlotPanicInfo,
				__builtin_return_address(0), "0x%08x,0x%08x\n", result, 0);
#endif
		}
		goto done;
	}
	if (g_appmgrNotificationQueue.count >= g_appmgrNotificationQueue.capacity) {
#if FW_DEBUG
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_CHECK_FUNC(0, 659, "_appmgrQueuePush");
#else
		appmgrNotificationOverflowPanic();
#endif
	}
	memcpy(g_appmgrNotificationQueue.buffer +
		   g_appmgrNotificationQueue.writeIndex * g_appmgrNotificationQueue.entrySize,
		   request, g_appmgrNotificationQueue.entrySize);
	g_appmgrNotificationQueue.writeIndex =
		(g_appmgrNotificationQueue.writeIndex + 1) % g_appmgrNotificationQueue.capacity;
	++g_appmgrNotificationQueue.count;
	result = sceKernelUnlockMutex(g_appmgrNotificationQueue.mutex, 1);
	if (result) {
#if FW_DEBUG
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_PRINTF_FUNC(0, 668, "_appmgrQueuePush",
			"[SceAppMgr] The actual value is %d(0x%08x), but expected value is %d(0x%08x)\n",
			result, result, 0, 0);
#else
		SceDebugForDriver_00CCE39C(&notificationUnlockPanicInfo,
			__builtin_return_address(0), "0x%08x,0x%08x\n", result, 0);
#endif
	}
	result = sceKernelSignalSema(g_appmgrNotificationQueue.readSemaphore, 1);
	if (result) {
#if FW_DEBUG
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_PRINTF_FUNC(0, 671, "_appmgrQueuePush",
			"[SceAppMgr] The actual value is %d(0x%08x), but expected value is %d(0x%08x)\n",
			result, result, 0, 0);
#else
		SceDebugForDriver_00CCE39C(&notificationSignalPanicInfo,
			__builtin_return_address(0), "0x%08x,0x%08x\n", result, 0);
#endif
	}
done:
	sceSyscallLeave(syscallState);
	return 0;
}
