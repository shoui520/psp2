/* Application abort handling and coredump controls. */

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <sceerror.h>
#include <display.h>
#include <kernel/error.h>
#include <sysmem/debug.h>
#include <sysmem/barrier.h>
#include <sysmem/sysclib.h>
#include <sysmem/sysmem.h>
#include <sysmem/sysroot.h>
#include <sysmem/sblaimgr.h>
#include <threadmgr/threadmgr.h>
#include <processmgr/processmgr.h>
#include <intrmgr/syscall.h>
#include <acmgr/acmgr.h>
#include <regmgr/regmgr.h>
#include <coredump/coredump.h>
#include <display/display.h>
#include <appmgr/appmgr.h>
#include <appmgr.h>
#include "app_state_internal.h"
#include "appmgr_device.h"
#include "appmgr_shell_event.h"

/* the coredump of an aborted application, as the shell reads it */
typedef struct AppmgrCoredumpState {
	SceUID processId;		/* -1: none */
	int appId;
	int progress;			/* percent */
	int taskId;			/* the coredump request */
	int finished;
	int startNotified;		/* the shell was told the coredump started */
	int result;
	SceSize pathLength;
	char path[1024];
	int finishArg;			/* the finish callback's last argument */
} AppmgrCoredumpState;

_Static_assert(sizeof(AppmgrCoredumpState) == 0x424, "coredump state");

/* the debug console the GPU crash screen is drawn with */
extern int appmgrDebugConsoleInitialize(void);
extern void appmgrDebugConsoleSetBuffer(void *buffer, int columns, int rows, int format);
extern void appmgrDebugConsoleSetForeColor(SceUInt32 color);
extern void appmgrDebugConsoleSetBackColor(SceUInt32 color);
extern void appmgrDebugConsoleSetCursor(int x, int y);
extern void appmgrDebugConsoleClear(void);
extern void appmgrDebugConsolePuts(const char *string);
extern void appmgrDebugConsoleFlush(void);

extern int appmgrStopShellWatchDog(void);
extern void _destroyAllAppForAbortHandler(void);
extern void appmgrArbitrationAfterAbort(void);
extern int _updatePowerTickStateForNonSuspendableApp(void);
extern int sharedFbCancelProcessUpdate(SceUID processId);
extern SceUID sharedFbGetRenderMemBlock(int *size);

extern SceUID g_appmgrMainMutex;
extern SceUID g_appmgrMainMutexOwner;
extern SceUID g_appmgrCore2MutexOwner;

AppmgrCoredumpState g_appmgrCoredumpState = {
	.processId = -1,
	.appId = -1,
	.startNotified = 1,
};

SceUID g_appmgrAbortEventFlag;
SceUID g_appmgrCoredumpEventFlag;
SceUID g_appmgrAbortThreadId;
int g_appmgrShellCrashed;

#if FW_DEBUG
int g_appmgrShellAborted;
int g_fakeCoredumpStartFailure;
SceUInt32 g_coredumpTexture[16][88];
SceDisplayFrameBuf g_coredumpFrameBuf;
int g_coredumpEnabledForTest;
int g_abortTargetUndetermined;
int g_fakeAbortReasonOne;
int g_fakeAbortCurrentProcess;
#else
int g_fakeAbortCurrentProcess;
int g_abortTargetUndetermined;
int g_fakeCoredumpStartFailure;
int g_appmgrShellAborted;
int g_fakeAbortReasonOne;
SceUInt32 g_coredumpTexture[16][88];
SceDisplayFrameBuf g_coredumpFrameBuf;
int g_coredumpEnabledForTest;
#endif

#if FW_DEBUG
/* records of _sceAppMgrAbortHandlerFinalize, which the release keeps no code of */
SCE_DBGINFO_DEFINE(static, finalizeDeleteAbortInfo, 1080, "_sceAppMgrAbortHandlerFinalize");
SCE_DBGINFO_DEFINE(static, finalizeDeleteCoredumpInfo, 1081, "_sceAppMgrAbortHandlerFinalize");
SCE_DBGINFO_DEFINE(static, finalizeDeleteThreadInfo, 1087, "_sceAppMgrAbortHandlerFinalize");
#endif

#if !FW_DEBUG
static const SceKernelDebugInfo abortHandlerPanicInfo = {
	.fileHash = 0xc9656e8e,
	.lineHash = 0xc879dadc,
	.funcHash = 0x11a05ee1,
};
static const SceKernelDebugInfo abortHandlerLockPanicInfo = {
	.fileHash = 0xc9656e8e,
	.lineHash = 0x79d3a1f0,
	.funcHash = 0x9d536bce,
};
static const SceKernelDebugInfo cancelUpdatePanicInfo = {
	.fileHash = 0x0e824785, .lineHash = 0xf4f00936, .funcHash = 0xb78554f7,
};
static const SceKernelDebugInfo waitShellPanicInfo = {
	.fileHash = 0x0e824785, .lineHash = 0x1bc18446, .funcHash = 0xb78554f7,
};
static const SceKernelDebugInfo releaseNotAvailablePanicInfo = {
	.fileHash = 0x0e824785, .lineHash = 0x74b4b1e2, .funcHash = 0xb78554f7,
};
static const SceKernelDebugInfo waitCoredumpPanicInfo = {
	.fileHash = 0x0e824785, .lineHash = 0x2b3906d1, .funcHash = 0xb78554f7,
};
static const SceKernelDebugInfo releaseCoredumpPanicInfo = {
	.fileHash = 0x0e824785, .lineHash = 0x58ff4f52, .funcHash = 0xb78554f7,
};
static const SceKernelDebugInfo deleteAbortPanicInfo = {
	.fileHash = 0x0e824785, .lineHash = 0xa554bf80, .funcHash = 0xd2ba855d,
};
static const SceKernelDebugInfo resultDonePanicInfo = {
	.fileHash = 0x0e824785, .lineHash = 0x0b3fa44a, .funcHash = 0x6bfd44ee,
};
static const SceKernelDebugInfo startAbortPanicInfo = {
	.fileHash = 0x0e824785, .lineHash = 0xdd9b0480, .funcHash = 0xd2ba855d,
};
static const SceKernelDebugInfo registerAbortPanicInfo = {
	.fileHash = 0x0e824785, .lineHash = 0x05d80b17, .funcHash = 0xd2ba855d,
};
static const SceKernelDebugInfo resultNoShellPanicInfo = {
	.fileHash = 0x0e824785, .lineHash = 0xa53d1751, .funcHash = 0x6bfd44ee,
};
static const SceKernelDebugInfo resultShellPanicInfo = {
	.fileHash = 0x0e824785, .lineHash = 0x09a4d363, .funcHash = 0x6bfd44ee,
};
static const SceKernelDebugInfo notifyAbortPanicInfo = {
	.fileHash = 0x0e824785, .lineHash = 0xe06fd284, .funcHash = 0xa0c1f7c0,
};
static const SceKernelDebugInfo finishCoredumpPanicInfo = {
	.fileHash = 0x0e824785, .lineHash = 0xf1d4667d, .funcHash = 0x94b4b81c,
};
static const SceKernelDebugInfo referAbortedPanicInfo = {
	.fileHash = 0x0e824785, .lineHash = 0x51e65ee5, .funcHash = 0xe6c6be02,
};
static const SceKernelDebugInfo notifyAbortedPanicInfo = {
	.fileHash = 0x0e824785, .lineHash = 0xe6980de7, .funcHash = 0xe6c6be02,
};
static const SceKernelDebugInfo waitAbortPanicInfo = {
	.fileHash = 0x0e824785, .lineHash = 0xd69c2257, .funcHash = 0x9f5bb3fc,
};
static const SceKernelDebugInfo deleteAbortAfterCreatePanicInfo = {
	.fileHash = 0x0e824785, .lineHash = 0xf37c4d09, .funcHash = 0xd2ba855d,
};
static const SceKernelDebugInfo undeterminedAbortPanicInfo = {
	.fileHash = 0x0e824785, .lineHash = 0xd543f013, .funcHash = 0xb78554f7,
};
static const SceKernelDebugInfo deleteCoredumpPanicInfo = {
	.fileHash = 0x0e824785, .lineHash = 0xb0aa5ea0, .funcHash = 0xd2ba855d,
};
static const SceKernelDebugInfo releaseUnavailablePanicInfo = {
	.fileHash = 0x0e824785, .lineHash = 0x4e2b3c64, .funcHash = 0xb78554f7,
};
static const SceKernelDebugInfo waitForcedPanicInfo = {
	.fileHash = 0x0e824785, .lineHash = 0xe4e722be, .funcHash = 0xb78554f7,
};
static const SceKernelDebugInfo releaseForcedPanicInfo = {
	.fileHash = 0x0e824785, .lineHash = 0xb3510897, .funcHash = 0xb78554f7,
};

_Noreturn void appmgrAbortHandlerPanic(void)
{
	SceDebugForDriver_391B5B74(&abortHandlerPanicInfo, __builtin_return_address(0));
}

_Noreturn void appmgrAbortHandlerLockPanic(void)
{
	SceDebugForDriver_391B5B74(&abortHandlerLockPanicInfo, __builtin_return_address(0));
}
#endif

#if FW_DEBUG
#define ABORT_FAILURE(_value, _line, _func) do { \
	sceAppMgrDump(); \
	dumpBgmState(); \
	SCE_DBG_ASSERT_PRINTF_FUNC(0, _line, _func, \
		"[SceAppMgr] The actual value is %d(0x%08x), but expected value is %d(0x%08x)\n", \
		_value, _value, 0, 0); \
} while (0)
#define ABORT_FAIL(_line, _func, _info) do { \
	sceAppMgrDump(); \
	dumpBgmState(); \
	SCE_DBG_ASSERT_CHECK_FUNC(0, _line, _func); \
} while (0)
#define ABORT_FAIL_PANIC(_line, _func) ABORT_FAIL(_line, _func, _)
#define ABORT_RESULT(_result, _line, _func) do { \
	if ((_result) != 0) \
		ABORT_FAILURE(_result, _line, _func); \
} while (0)
#define ABORT_UNLOCK_RESULT(_result, _line, _func) ABORT_RESULT(_result, _line, _func)
#define ABORT_MAIN_LOCK(_line, _func) do { \
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
#define ABORT_ASSERT_MAIN_UNOWNED(_line, _func) do { \
	if (SceDebugForDriver_374B7868() > 0) { \
		SceUID owner = g_appmgrMainMutexOwner; \
		SceUID tid = sceKernelGetThreadId(); \
		SCE_DBG_ASSERT_LEVEL_FUNC(1, owner != tid, _line, _func); \
	} \
} while (0)
/* the application locks are taken after the main and core locks, never inside another */
#define ABORT_APP_LOCK_CHECK(_line, _func) do { \
	SceUID owner; \
	ABORT_ASSERT_MAIN_UNOWNED(_line, _func); \
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
/* a lock the caller may already hold (0x8002802c, 0x8002802d) is taken as it is */
#define ABORT_APP_LOCK(_app, _line, _func) do { \
	int lockResult; \
	ABORT_APP_LOCK_CHECK(_line, _func); \
	lockResult = sceKernelLockMutex((_app)->mutex, 1, NULL); \
	if (lockResult != 0) { \
		if ((unsigned int)(lockResult + 0x7ffd7fd4) > 1) \
			ABORT_FAILURE(lockResult, _line, _func); \
	} else { \
		(_app)->mutexOwner = sceKernelGetThreadId(); \
	} \
} while (0)
#define ABORT_APP_LOCK_STRICT(_app, _line, _func) do { \
	int lockResult; \
	ABORT_APP_LOCK_CHECK(_line, _func); \
	lockResult = sceKernelLockMutex((_app)->mutex, 1, NULL); \
	if (lockResult != 0) { \
		if ((unsigned int)(lockResult + 0x7ffd7fd4) > 1) \
			ABORT_FAILURE(lockResult, _line, _func); \
		ABORT_FAILURE(lockResult, (_line) + 1, _func); \
	} else { \
		(_app)->mutexOwner = sceKernelGetThreadId(); \
	} \
} while (0)
#else
#define ABORT_FAILURE(_value, _line, _func) appmgrAbortHandlerPanic()
#define ABORT_FAIL(_line, _func, _info) \
	SceDebugForDriver_391B5B74(&(_info), __builtin_return_address(0))
#define ABORT_FAIL_PANIC(_line, _func) appmgrAbortHandlerPanic()
#define ABORT_RESULT(_result, _line, _func) do { \
	if ((_result) != 0) \
		appmgrAbortHandlerPanic(); \
} while (0)
#define ABORT_UNLOCK_RESULT(_result, _line, _func) do { \
	if ((_result) != 0) \
		appmgrAbortHandlerLockPanic(); \
} while (0)
#define ABORT_MAIN_LOCK(_line, _func) do { \
	if (sceKernelLockMutex(g_appmgrMainMutex, 1, NULL) != 0) \
		appmgrAbortHandlerLockPanic(); \
	g_appmgrMainMutexOwner = sceKernelGetThreadId(); \
} while (0)
#define ABORT_ASSERT_MAIN_UNOWNED(_line, _func) do { } while (0)
#define ABORT_APP_LOCK_CHECK(_line, _func) ((void)sceKernelGetThreadId())
#define ABORT_APP_LOCK(_app, _line, _func) do { \
	int lockResult; \
	ABORT_APP_LOCK_CHECK(_line, _func); \
	lockResult = sceKernelLockMutex((_app)->mutex, 1, NULL); \
	if (lockResult != 0) { \
		if ((unsigned int)(lockResult + 0x7ffd7fd4) > 1) \
			appmgrAbortHandlerLockPanic(); \
	} else { \
		(_app)->mutexOwner = sceKernelGetThreadId(); \
	} \
} while (0)
#define ABORT_APP_LOCK_STRICT(_app, _line, _func) do { \
	int lockResult; \
	ABORT_APP_LOCK_CHECK(_line, _func); \
	lockResult = sceKernelLockMutex((_app)->mutex, 1, NULL); \
	if (lockResult != 0) { \
		if ((unsigned int)(lockResult + 0x7ffd7fd4) > 1) \
			appmgrAbortHandlerLockPanic(); \
		appmgrAbortHandlerPanic(); \
	} \
	(_app)->mutexOwner = sceKernelGetThreadId(); \
} while (0)
#endif

#define ABORT_MAIN_UNLOCK(_line, _func) do { \
	int unlockResult; \
	g_appmgrMainMutexOwner = -1; \
	unlockResult = sceKernelUnlockMutex(g_appmgrMainMutex, 1); \
	ABORT_UNLOCK_RESULT(unlockResult, _line, _func); \
} while (0)
#define ABORT_APP_UNLOCK(_app, _line, _func) do { \
	int unlockResult; \
	ABORT_ASSERT_MAIN_UNOWNED(_line, _func); \
	(_app)->mutexOwner = -1; \
	unlockResult = sceKernelUnlockMutex((_app)->mutex, 1); \
	ABORT_UNLOCK_RESULT(unlockResult, _line, _func); \
} while (0)

static void _coredumpNotifyResultCallback(int taskId, SceUID processId, int result,
					  const char *path, SceSize pathLength, int finishArg);
static void _coredumpNotifyProgressCallback(int taskId, SceUID processId, int progress);

/* copies the 16 texture rows, size bytes of each, to the crash screen at (x, y) */
static inline __attribute__((always_inline)) void drawGpuCoredumpTexture(SceUInt32 x, SceUInt32 y, SceSize size)
{
	int row;

	for (row = 0; row < 16; ++row)
		__builtin_memcpy((SceUInt32 *)g_coredumpFrameBuf.base + x +
				 g_coredumpFrameBuf.pitch * (y + row), g_coredumpTexture[row], size);
}

static inline __attribute__((always_inline)) void renderGpuCoredumpText(const char *text)
{
	appmgrDebugConsoleSetBuffer(g_coredumpTexture, 88, 16, 3);
	appmgrDebugConsoleSetCursor(0, 0);
	appmgrDebugConsoleSetForeColor(0xffffffff);
	appmgrDebugConsoleSetBackColor(0xff000000);
	appmgrDebugConsoleClear();
	appmgrDebugConsolePuts(text);
	appmgrDebugConsoleFlush();
}

static inline __attribute__((always_inline)) void drawGpuCoredumpText(const char *text, SceUInt32 x, SceUInt32 y)
{
	renderGpuCoredumpText(text);
	drawGpuCoredumpTexture(x, y, __builtin_strlen(text) * 32);
}

static int startCoredump(struct AppState *app)
{
	SceCoredumpTriggerParam param;
	int result;

	if (g_fakeCoredumpStartFailure) {
#if FW_DEBUG
		SCE_DBG_PRINTF_FUNC(0, 671, "startCoredump",
			"[SceAppMgr] Start coredump PID=0x%08x failed.\n",
			app->process.processId);
#endif
		return -1;
	}
#if FW_DEBUG
	SCE_DBG_PRINTF_FUNC(0, 675, "startCoredump",
		"[SceAppMgr] Start coredump PID=0x%08x (reason=0x%08x)\n",
		app->process.processId, app->coredumpReason);
#endif
	param.size = sizeof(param);
	param.dumpLevel = 0;
	param.triggerDataType = 0;
	param.triggerDataSize = 0;
	param.triggerData = NULL;
	param.titleid_len = 0;
	param.titleid = NULL;
	param.app_name_len = 0;
	param.app_name = NULL;
	param.appVersion = 0;
	param.triggerReason = app->coredumpReason;
	param.crash_thid = app->crashThreadId;
	param.triggerFlags = g_abortTargetUndetermined;
	ABORT_MAIN_LOCK(682, "startCoredump");
	param.titleid_len = strnlen(app->process.content.titleId, 9) + 1;
	param.titleid = app->process.content.titleId;
	param.app_name_len = strnlen(app->process.content.title, 127) + 1;
	param.app_name = app->process.content.title;
	param.appVersion = app->process.content.appVersion;
	g_appmgrMainMutexOwner = -1;
	result = sceKernelUnlockMutex(g_appmgrMainMutex, 1);
	ABORT_UNLOCK_RESULT(result, 688, "startCoredump");
#if FW_DEBUG
	SCE_DBG_PRINTF_FUNC(0, 701, "startCoredump",
		"\ntitleIdSize  : %u\ntitleId      : %s\nappTitleSize : %u\nappTitle     : %s\nappVer       : %u\n",
		param.titleid_len, param.titleid, param.app_name_len, param.app_name,
		param.appVersion);
#endif
	g_appmgrCoredumpState.startNotified = 0;
	return sceCoredumpTrigger(app->process.processId,
		(SceKernelCoredumpStateUpdateCallback)_coredumpNotifyProgressCallback,
		(SceKernelCoredumpStateFinishCallback)_coredumpNotifyResultCallback, &param);
}

static void killAppsAndReboot(void)
{
	struct AppState *shell;
	int result;

#if FW_DEBUG
	SCE_DBG_PRINTF_FUNC(0, 90, "killAppsAndReboot", "Shell is crashed!.\n");
#endif
	g_appmgrShellCrashed = 1;
	result = appmgrStopShellWatchDog();
	ABORT_RESULT(result, 95, "killAppsAndReboot");
	shell = appmgrGetShellApp();
	ABORT_APP_LOCK_STRICT(shell, 100, "killAppsAndReboot");
	result = _appmgrProcessSuspendProcess(shell, 2, NULL);
	ABORT_RESULT(result, 104, "killAppsAndReboot");
	ABORT_APP_UNLOCK(shell, 106, "killAppsAndReboot");
	_destroyAllAppForAbortHandler();
#if FW_DEBUG
	SCE_DBG_PRINTF_FUNC(0, 114, "killAppsAndReboot", "Kill Shell!.\n");
#endif
	ABORT_MAIN_LOCK(116, "killAppsAndReboot");
	_appListSetRunStatus(shell, 0x700);
	ABORT_MAIN_UNLOCK(118, "killAppsAndReboot");
	ABORT_APP_LOCK_STRICT(shell, 123, "killAppsAndReboot");
	while ((result = _appmgrProcessQuitProcess(shell, 1, 0, 0)) ==
	       SCE_KERNEL_ERROR_PROCESS_EVENT_INHIBITED)
		sceKernelDelayThread(100000);
	ABORT_RESULT(result, 133, "killAppsAndReboot");
	ABORT_APP_UNLOCK(shell, 135, "killAppsAndReboot");
}

static void _coredumpNotifyResultCallback(int taskId, SceUID processId, int result,
					  const char *path, SceSize pathLength, int finishArg)
{
	struct AppState *app, *shell;
	SceSize length;

	(void)taskId;
#if FW_DEBUG
	SCE_DBG_PRINTF_FUNC(0, 190, "_coredumpNotifyResultCallback",
		"[SceAppMgr] Coredump finished (Return code: 0x%08x)\n",
		g_appmgrCoredumpState.result);
#endif
	if (result == SCE_ERROR_ERRNO_ECANCELED)
		return;
	ABORT_MAIN_LOCK(195, "_coredumpNotifyResultCallback");
	app = appmgrFindAppByProcessId(g_appmgrAppStates, processId);
	if (app == NULL) {
		ABORT_MAIN_UNLOCK(200, "_coredumpNotifyResultCallback");
		return;
	}
	if (app->coredumpReason == 1) {
#if FW_DEBUG
		SCE_DBG_PRINTF_FUNC(0, 205, "_coredumpNotifyResultCallback",
			"[SceAppMgr] Done\n");
#endif
		if (sceKernelSetEventFlag(g_appmgrCoredumpEventFlag, 1) != 0)
			ABORT_FAIL(208, "_coredumpNotifyResultCallback", resultDonePanicInfo);
		ABORT_MAIN_UNLOCK(209, "_coredumpNotifyResultCallback");
		return;
	}
	if (g_appmgrCoredumpState.processId == -1) {
		ABORT_MAIN_UNLOCK(214, "_coredumpNotifyResultCallback");
		return;
	}
	if (processId != g_appmgrCoredumpState.processId) {
#if FW_DEBUG
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_PRINTF_FUNC(0, 218, "_coredumpNotifyResultCallback",
			"[SceAppMgr] The actual value is %d(0x%08x), but expected value is %d(0x%08x)\n",
			g_appmgrCoredumpState.processId, g_appmgrCoredumpState.processId,
			processId, processId);
#else
		appmgrAbortHandlerPanic();
#endif
	}
	length = pathLength < sizeof(g_appmgrCoredumpState.path) - 1 ?
		 pathLength : sizeof(g_appmgrCoredumpState.path) - 1;
	g_appmgrCoredumpState.result = result;
	g_appmgrCoredumpState.pathLength = length;
	g_appmgrCoredumpState.finished = 1;
#if FW_DEBUG
	__memcpy_chk(g_appmgrCoredumpState.path, path, length,
		     __builtin_object_size(g_appmgrCoredumpState.path, 0));
#else
	memcpy(g_appmgrCoredumpState.path, path, length);
#endif
	g_appmgrCoredumpState.path[sizeof(g_appmgrCoredumpState.path) - 1] = '\0';
	g_appmgrCoredumpState.finishArg = finishArg;
	ABORT_MAIN_UNLOCK(228, "_coredumpNotifyResultCallback");
	if (appmgrGetShellApp() == NULL) {
#if FW_DEBUG
		SCE_DBG_PRINTF_FUNC(0, 232, "_coredumpNotifyResultCallback",
			"[SceAppMgr] Done\n");
#endif
		if (sceKernelSetEventFlag(g_appmgrCoredumpEventFlag, 1) != 0)
			ABORT_FAIL(235, "_coredumpNotifyResultCallback", resultNoShellPanicInfo);
		return;
	}
	shell = appmgrGetShellApp();
	if (processId == (shell != NULL ? shell->process.processId : 0)) {
		if (sceKernelSetEventFlag(g_appmgrCoredumpEventFlag, 1) != 0)
			ABORT_FAIL(241, "_coredumpNotifyResultCallback", resultShellPanicInfo);
		return;
	}
#if FW_DEBUG
	SCE_DBG_PRINTF_FUNC(0, 244, "_coredumpNotifyResultCallback",
		"[SceAppMgr] Waiting for shell notification\n");
#endif
}

static inline void _sceAppMgrAbort(SceUID processId, SceUID threadId, int reason)
{
	struct AppState *app;
	SceUInt32 state;

	if (g_fakeAbortCurrentProcess) {
		reason = 1;
		processId = sceKernelGetProcessId();
	}
	if (g_fakeAbortReasonOne)
		reason = 1;
	if (sceKernelGetProcessId() == processId) {
#if FW_DEBUG
		SCE_DBG_PRINTF_FUNC(0, 961, "_sceAppMgrAbort",
			"[SceAppMgr] Aborted process was not determined.\n");
#endif
		g_abortTargetUndetermined = 1;
	} else {
		app = appmgrFindAppByProcessId(g_appmgrAppStates, processId);
		if (app == NULL)
			return;
		state = app->state;
		if ((state & 0xff0) != 0x800 && (state & 0xff0) != 0x200 &&
		    (state & 0xff0) != 0x700 && (state & 0xbf0) != 0x100) {
			_appListSetRunStatus(app, 0x200);
			app->coredumpReason = reason;
			app->crashThreadId = threadId;
			app->process.requestedProcessState = 5;
#if FW_DEBUG
			SCE_DBG_PRINTF_FUNC(0, 987, "_sceAppMgrAbort",
				"[SceAppMgr] 0x%08x ABORTED\n", processId);
			SCE_DBG_PRINTF_FUNC(0, 988, "_sceAppMgrAbort",
				"[SceAppMgr] sceGUIDReferObject(0x%08x)\n", processId);
#endif
			if (sceGUIDReferObject(processId, NULL) < 0)
				ABORT_FAIL(990, "_sceAppMgrAbort", referAbortedPanicInfo);
		}
	}
	dmb();
	if (sceKernelSetEventFlag(g_appmgrAbortEventFlag, 1) != 0)
		ABORT_FAIL(1002, "_sceAppMgrAbort", notifyAbortedPanicInfo);
}

static void _coredumpNotifyProgressCallback(int taskId, SceUID processId, int progress)
{
	struct AppState *app;
	SceUInt32 percent;

	ABORT_MAIN_LOCK(547, "_coredumpNotifyProgressCallback");
#if FW_DEBUG
	SCE_DBG_PRINTF_FUNC(0, 548, "_coredumpNotifyProgressCallback",
		"[SceAppMgr] Coredump progress %d %%\n", progress);
#endif
	app = appmgrFindAppByProcessId(g_appmgrAppStates, processId);
	if (app == NULL) {
		ABORT_MAIN_UNLOCK(552, "_coredumpNotifyProgressCallback");
		return;
	}
	if (app->coredumpReason == 1) {
		if (g_coredumpFrameBuf.base != NULL && g_coredumpFrameBuf.width >= 264 &&
		    g_coredumpFrameBuf.height >= 32) {
			percent = progress;
			if (percent > 100)
				percent = 100;
			if (SceSblAIMgrForDriver_D78B04A2())
				drawGpuCoredumpTexture(g_coredumpFrameBuf.width / 2 - 40,
					g_coredumpFrameBuf.height / 2 + 16, 32 * (percent / 10));
			else
				drawGpuCoredumpTexture(g_coredumpFrameBuf.width / 2 + 44,
					g_coredumpFrameBuf.height / 2 + 16, 32 * (percent / 10));
		}
	} else if (g_appmgrCoredumpState.processId != -1 ||
		   g_appmgrCoredumpState.startNotified) {
		g_appmgrCoredumpState.progress = progress;
	} else {
		g_appmgrCoredumpState.taskId = taskId;
		g_appmgrCoredumpState.finished = 0;
		g_appmgrCoredumpState.progress = progress;
		g_appmgrCoredumpState.processId = app->process.processId;
		g_appmgrCoredumpState.appId = app->appId;
		g_appmgrCoredumpState.startNotified = 1;
		_shellEventCoredumpStartQueuePush(app->process.processId, app->appId, 1);
	}
	ABORT_MAIN_UNLOCK(580, "_coredumpNotifyProgressCallback");
}

static inline struct AppState *findAbortedApp(void)
{
	struct AppState *app;
	int index;
	int result;

	ABORT_MAIN_LOCK(590, "findAbortedApp");
	if (g_abortTargetUndetermined) {
		app = appmgrFindActiveApp(g_appmgrAppStates);
		if (app == NULL)
			app = appmgrGetShellApp();
		if (app != NULL) {
#if FW_DEBUG
			SCE_DBG_PRINTF_FUNC(0, 598, "findAbortedApp",
				"[SceAppMgr]:%d sceGUIDReferObject(0x%08x)\n", 598,
				app->process.processId);
#endif
			if (sceGUIDReferObject(app->process.processId, NULL) >= 0) {
				if (!g_fakeCoredumpStartFailure && SceCoredumpForDriver_097AA37D()) {
					result = sceKernelProcessDebugSuspend(app->process.processId, 17);
#if FW_DEBUG
					SCE_DBG_PRINTF_FUNC(0, 605, "findAbortedApp",
						"DEBUG SUSPEND pid=0x%08x => 0x%08x\n",
						app->process.processId, result);
#else
					(void)result;
#endif
				}
				_appListSetRunStatus(app, 0x200);
				app->coredumpReason = 1;
			} else {
				app = NULL;
			}
		}
	} else {
		app = NULL;
		for (index = 0; index != 20; ++index) {
			if ((g_appmgrAppStates[index].state & 0xff0) == 0x200) {
				app = &g_appmgrAppStates[index];
				break;
			}
		}
	}
	ABORT_MAIN_UNLOCK(621, "findAbortedApp");
	return app;
}

static inline int isCoredumpEnabled(void)
{
	int value;
	int result;

	if (sceKernelSysrootIsSafeMode() || SceSysrootForDriver_B0E1FC67())
		return 0;
	if (SceSblACMgrForDriver_E87D1777() && !g_coredumpEnabledForTest) {
		value = 0;
		result = sceRegMgrGetKeyInt("/CONFIG/COREDUMP", "force_dump_on_devmode", &value);
#if FW_DEBUG
		SCE_DBG_PRINTF_FUNC(0, 658, "isCoredumpEnabled",
			"force_dump_on_devmode=0x%08x\n", value);
#endif
		if (result != 0 || value == 0)
			return 0;
	}
	return 1;
}

/* the GPU crash screen: its message, and the progress bar's frame */
static inline void drawGpuCrashScreen(void)
{
	SceUInt32 x, y;

	if (SceSblAIMgrForDriver_D78B04A2()) {
		x = g_coredumpFrameBuf.width / 2;
		y = g_coredumpFrameBuf.height / 2 - 32;
		drawGpuCoredumpText("An error", x - 156, y);
		drawGpuCoredumpText("has", x - 84, y);
		drawGpuCoredumpText("occurred.", x - 52, y);
		x = g_coredumpFrameBuf.width / 2;
		y = g_coredumpFrameBuf.height / 2 - 16;
		drawGpuCoredumpText("The PS", x - 156, y);
		drawGpuCoredumpText("Vita", x - 100, y);
		drawGpuCoredumpText("system", x - 60, y);
		drawGpuCoredumpText("will be", x - 4, y);
		drawGpuCoredumpText("powered", x + 60, y);
		drawGpuCoredumpText("off.", x + 124, y);
		x = g_coredumpFrameBuf.width / 2;
		y = g_coredumpFrameBuf.height / 2 + 16;
		drawGpuCoredumpText("[", x - 48, y);
		drawGpuCoredumpText("]", x + 40, y);
	} else {
		x = g_coredumpFrameBuf.width / 2;
		y = g_coredumpFrameBuf.height / 2 - 16;
		drawGpuCoredumpText("GPU Driver", x - 116, y);
		drawGpuCoredumpText("detects", x - 28, y);
		drawGpuCoredumpText("GPU Crash.", x + 36, y);
		x = g_coredumpFrameBuf.width / 2;
		y = g_coredumpFrameBuf.height / 2 + 16;
		drawGpuCoredumpText("Generating", x - 132, y);
		drawGpuCoredumpText("CoreDump:", x - 44, y);
		drawGpuCoredumpText("[", x + 36, y);
		drawGpuCoredumpText("]", x + 124, y);
	}
	/* the progress callback draws the bar from it */
	renderGpuCoredumpText("**********");
}

/* quits an aborted application that is still stopping */
#define QUIT_ABORTED_APP(_app, _exitReason, _lockLine, _quitLine, _unlockLine) do { \
	int quitResult; \
	ABORT_APP_LOCK(_app, _lockLine, "handleAbortedApp"); \
	if (((_app)->state & 0xff0) == 0x700) { \
		quitResult = _appmgrProcessQuitProcess(_app, _exitReason, 0, 0); \
		ABORT_RESULT(quitResult, _quitLine, "handleAbortedApp"); \
	} \
	ABORT_APP_UNLOCK(_app, _unlockLine, "handleAbortedApp"); \
} while (0)

static inline void handleAbortedApp(struct AppState *app)
{
	SceKernelAllocMemBlockKernelOpt opt;
	struct AppState *shell;
	SceUID renderMemBlock, memBlock;
	void *base;
	int isShell, forced;
	int result, size;

	ABORT_MAIN_LOCK(733, "handleAbortedApp");
	if (app->launchInfo.budgetType == 0) {
		result = sharedFbCancelProcessUpdate(app->process.processId);
		if (result != 0) {
#if FW_DEBUG
			ABORT_FAILURE(result, 737, "handleAbortedApp");
#else
			SceDebugForDriver_00CCE39C(&cancelUpdatePanicInfo,
				__builtin_return_address(0), "0x%08x,0x%08x\n", result, 0);
#endif
		}
	}
	shell = appmgrGetShellApp();
	isShell = app->process.processId == (shell != NULL ? shell->process.processId : 0);
	if (isShell)
		g_appmgrShellAborted = 1;
	forced = app->coredumpReason == 1;
	ABORT_MAIN_UNLOCK(746, "handleAbortedApp");

	if (!isCoredumpEnabled()) {
		ABORT_MAIN_LOCK(752, "handleAbortedApp");
		_appListSetRunStatus(app, 0x700);
		ABORT_MAIN_UNLOCK(754, "handleAbortedApp");
#if FW_DEBUG
		SCE_DBG_PRINTF_FUNC(0, 755, "handleAbortedApp",
			"[SceAppMgr] sceGUIDReleaseObject(0x%08x)\n", app->process.processId);
#endif
		result = sceGUIDReleaseObject(app->process.processId);
		if (result < 0) {
#if FW_DEBUG
			SCE_DBG_PRINTF_FUNC(0, 758, "handleAbortedApp",
				"[SceAppMgr] [sceGUIDReleaseObject(0x%08x) => 0x%08x\n",
				app->process.processId, result);
#endif
		}
		if (g_abortTargetUndetermined && forced && SceSblACMgrForDriver_E87D1777()) {
#if FW_DEBUG
			SceDebugForDriver_391B5B74(SCE_DBGINFO_FUNC(764, "handleAbortedApp"),
				__builtin_return_address(0));
#else
			SceDebugForDriver_391B5B74(&undeterminedAbortPanicInfo,
				__builtin_return_address(0));
#endif
		}
		goto end;
	}

	if (forced) {
		if (!g_fakeCoredumpStartFailure && SceCoredumpForDriver_097AA37D()) {
			SceDebugForDriver_391B74B7("appmgr_aborthandler.c(%d) render gpu crash\n", 793);
			renderMemBlock = sharedFbGetRenderMemBlock(&size);
			memset(&opt, 0, sizeof(opt));
			opt.size = sizeof(opt);
			opt.attr = 0x1000040;
			opt.mirror_blockid = renderMemBlock;
			memBlock = sceKernelAllocMemBlockForKernel("SceSharedFbShare",
				SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_ROOT_NC_RW, size, &opt);
			base = NULL;
			if (sceKernelGetMemBlockBase(memBlock, &base) == 0) {
				memset(base, 0, 0x1fe000);
				g_coredumpFrameBuf.size = sizeof(g_coredumpFrameBuf);
				g_coredumpFrameBuf.base = base;
				g_coredumpFrameBuf.pitch = 960;
				g_coredumpFrameBuf.pixelformat = 0;
				g_coredumpFrameBuf.width = 480;
				g_coredumpFrameBuf.height = 272;
				_SetDisplayOwner(1, sceKernelGetProcessId());
				_SetDisplayOwner(0, sceKernelGetProcessId());
				if (SceDisplayForDriver_289D82FE(&g_coredumpFrameBuf, 1) == 0 &&
				    g_coredumpFrameBuf.width >= 264 && g_coredumpFrameBuf.height >= 32)
					drawGpuCrashScreen();
			}
			if (startCoredump(app) < 0)
				ABORT_FAIL_PANIC(801, "handleAbortedApp");
			if (sceKernelWaitEventFlag(g_appmgrCoredumpEventFlag, 1, 3, NULL, NULL) != 0)
				ABORT_FAIL(805, "handleAbortedApp", waitForcedPanicInfo);
			ABORT_MAIN_LOCK(807, "handleAbortedApp");
			_appListSetRunStatus(app, 0x700);
			ABORT_MAIN_UNLOCK(809, "handleAbortedApp");
#if FW_DEBUG
			SCE_DBG_PRINTF_FUNC(0, 811, "handleAbortedApp",
				"[SceAppMgr] sceGUIDReleaseObject(0x%08x)\n", app->process.processId);
#endif
			if (sceGUIDReleaseObject(app->process.processId) < 0)
				ABORT_FAIL(813, "handleAbortedApp", releaseForcedPanicInfo);
			if (g_abortTargetUndetermined)
				killAppsAndReboot();
			else if (isShell)
				killAppsAndReboot();
			else
				QUIT_ABORTED_APP(app, 7, 818, 821, 823);
		} else {
			ABORT_MAIN_LOCK(772, "handleAbortedApp");
			_appListSetRunStatus(app, 0x700);
			ABORT_MAIN_UNLOCK(775, "handleAbortedApp");
#if FW_DEBUG
			SCE_DBG_PRINTF_FUNC(0, 777, "handleAbortedApp",
				"[SceAppMgr] sceGUIDReleaseObject(0x%08x)\n", app->process.processId);
#endif
			if (sceGUIDReleaseObject(app->process.processId) < 0)
				ABORT_FAIL(779, "handleAbortedApp", releaseUnavailablePanicInfo);
			if (g_abortTargetUndetermined)
				killAppsAndReboot();
			else if (isShell)
				killAppsAndReboot();
			else
				QUIT_ABORTED_APP(app, 7, 784, 787, 789);
		}
		goto done;
	}

	if (!g_fakeCoredumpStartFailure && SceCoredumpForDriver_097AA37D()) {
		if (startCoredump(app) < 0)
			ABORT_FAIL_PANIC(869, "handleAbortedApp");
		ABORT_MAIN_LOCK(871, "handleAbortedApp");
		_appListSetRunStatus(app, 0x700);
		ABORT_MAIN_UNLOCK(873, "handleAbortedApp");
		if (sceKernelWaitEventFlag(g_appmgrCoredumpEventFlag, 1, 3, NULL, NULL) != 0)
			ABORT_FAIL(876, "handleAbortedApp", waitCoredumpPanicInfo);
#if FW_DEBUG
		SCE_DBG_PRINTF_FUNC(0, 879, "handleAbortedApp",
			"[SceAppMgr] sceGUIDReleaseObject(0x%08x)\n", app->process.processId);
#endif
		if (sceGUIDReleaseObject(app->process.processId) < 0)
			ABORT_FAIL(881, "handleAbortedApp", releaseCoredumpPanicInfo);
	} else {
#if FW_DEBUG
		SCE_DBG_PRINTF_FUNC(0, 831, "handleAbortedApp",
			"sceCoredump is not available.\n");
#endif
		ABORT_APP_LOCK_STRICT(app, 833, "handleAbortedApp");
		result = _appmgrProcessSuspendProcess(app, 2, NULL);
		ABORT_RESULT(result, 837, "handleAbortedApp");
		ABORT_APP_UNLOCK(app, 839, "handleAbortedApp");
		ABORT_MAIN_LOCK(841, "handleAbortedApp");
		g_appmgrCoredumpState.processId = -1;
		g_appmgrCoredumpState.appId = -1;
		g_appmgrCoredumpState.progress = 0;
		g_appmgrCoredumpState.taskId = -1;
		g_appmgrCoredumpState.finished = 1;
		_shellEventCoredumpStartQueuePush(app->process.processId, app->appId, 0);
		ABORT_MAIN_UNLOCK(851, "handleAbortedApp");
		if (!isShell &&
		    sceKernelWaitEventFlag(g_appmgrCoredumpEventFlag, 1, 3, NULL, NULL) != 0)
			ABORT_FAIL(855, "handleAbortedApp", waitShellPanicInfo);
		ABORT_MAIN_LOCK(858, "handleAbortedApp");
		_appListSetRunStatus(app, 0x700);
		ABORT_MAIN_UNLOCK(860, "handleAbortedApp");
#if FW_DEBUG
		SCE_DBG_PRINTF_FUNC(0, 862, "handleAbortedApp",
			"[SceAppMgr] sceGUIDReleaseObject(0x%08x)\n", app->process.processId);
#endif
		if (sceGUIDReleaseObject(app->process.processId) < 0)
			ABORT_FAIL(864, "handleAbortedApp", releaseNotAvailablePanicInfo);
	}
	if (isShell)
		killAppsAndReboot();
	else
		QUIT_ABORTED_APP(app, 1, 889, 893, 896);
done:
#if FW_DEBUG
	SCE_DBG_PRINTF_FUNC(0, 900, "handleAbortedApp", "[SceAppMgr] Done.\n");
#endif
end:
#if FW_DEBUG
	SCE_DBG_PRINTF_FUNC(0, 905, "handleAbortedApp",
		"appmgr_aborthandler.c(%d) handleAbortedApp() End\n", 905);
#endif
	return;
}

static int _AbortHandlerThreadEntry(SceSize args, void *argp)
{
	struct AppState *app;

	(void)args;
	(void)argp;
	for (;;) {
		if (sceKernelWaitEventFlag(g_appmgrAbortEventFlag, 1, 2, NULL, NULL) != 0)
			ABORT_FAIL(920, "_AbortHandlerThreadEntry", waitAbortPanicInfo);
		dmb();
		if (g_appmgrShellCrashed)
			break;
		app = findAbortedApp();
		if (app != NULL)
			handleAbortedApp(app);
		appmgrArbitrationAfterAbort();
		_updatePowerTickStateForNonSuspendableApp();
	}
	return 0;
}

SceUID appmgrGetAbortThreadId(void)
{
	return g_appmgrAbortThreadId;
}

int sceAppMgrSetFakeSettingBug51800ForUser(int abortCurrentProcess,
					 int forceAbortReasonOne, int failCoredumpStart)
{
#if FW_DEBUG
	SCE_DBG_PRINTF_FUNC(0, 155, "sceAppMgrSetFakeSettingBug51800ForUser",
		"sceAppMgrSetFakeSettingBug51800(%d,%d,%d) is Called\n",
		abortCurrentProcess, forceAbortReasonOne, failCoredumpStart);
#endif
	g_fakeAbortCurrentProcess = abortCurrentProcess;
	g_fakeAbortReasonOne = forceAbortReasonOne;
	g_fakeCoredumpStartFailure = failCoredumpStart;
	return 0;
}

int sceAppMgrGetCoredumpStateForShell(AppmgrShellCoredumpState *state)
{
	SceUInt32 syscallState = sceSyscallEnter();

	if (!isShellProcess()) {
		sceSyscallLeave(syscallState);
		return (int)0x8080201c;
	}
	ABORT_MAIN_LOCK(478, "sceAppMgrGetCoredumpStateForShell");
	if (g_appmgrCoredumpState.processId == -1) {
		ABORT_MAIN_UNLOCK(481, "sceAppMgrGetCoredumpStateForShell");
		sceSyscallLeave(syscallState);
		return SCE_APPMGR_ERROR_STATE;
	}
	state->processId = g_appmgrCoredumpState.processId;
	state->appId = g_appmgrCoredumpState.appId;
	state->progress = g_appmgrCoredumpState.progress;
	state->finished = g_appmgrCoredumpState.finished;
	state->result = g_appmgrCoredumpState.result;
	state->pathLength = g_appmgrCoredumpState.pathLength;
	memcpy(state->path, g_appmgrCoredumpState.path, sizeof(state->path));
	state->finishArg = g_appmgrCoredumpState.finishArg;
	ABORT_MAIN_UNLOCK(495, "sceAppMgrGetCoredumpStateForShell");
	sceSyscallLeave(syscallState);
	return 0;
}

int sceAppMgrFinishCoredumpForShell(void)
{
	SceUInt32 syscallState = sceSyscallEnter();
	int result;

	if (!isShellProcess()) {
		sceSyscallLeave(syscallState);
		return (int)0x8080201c;
	}
	ABORT_MAIN_LOCK(511, "sceAppMgrFinishCoredumpForShell");
	if (g_appmgrCoredumpState.processId == -1 && g_appmgrCoredumpState.finished) {
		result = (int)0x80802028;
	} else {
		sceKernelCoredumpCancelForDriver(g_appmgrCoredumpState.taskId);
		if (g_appmgrCoredumpState.processId == -1) {
			result = (int)0x80802028;
		} else {
			memset(&g_appmgrCoredumpState, 0, sizeof(g_appmgrCoredumpState));
			g_appmgrCoredumpState.processId = -1;
#if FW_DEBUG
			SCE_DBG_PRINTF_FUNC(0, 521, "sceAppMgrFinishCoredumpForShell",
				"[SceAppMgr] Finish coredump.\n");
#endif
			if (sceKernelSetEventFlag(g_appmgrCoredumpEventFlag, 1) != 0)
				ABORT_FAIL(524, "sceAppMgrFinishCoredumpForShell",
					   finishCoredumpPanicInfo);
			result = 0;
		}
	}
	ABORT_MAIN_UNLOCK(531, "sceAppMgrFinishCoredumpForShell");
	sceSyscallLeave(syscallState);
	return result;
}

int sceAppMgrEnableCoredumpForTest(void)
{
	SceUInt32 syscallState = sceSyscallEnter();

	if (!isShellProcess()) {
		sceSyscallLeave(syscallState);
		return SCE_APPMGR_ERROR_NOEXEC;
	}
	g_coredumpEnabledForTest = 1;
	sceSyscallLeave(syscallState);
	return 0;
}

void _sceAppMgrNotifyAppMgrHandler(void)
{
	if (sceKernelSetEventFlag(g_appmgrAbortEventFlag, 1) != 0)
		ABORT_FAIL(940, "_sceAppMgrNotifyAppMgrHandler", notifyAbortPanicInfo);
}

#if FW_DEBUG
#define ABORT_INITIALIZE_CHECK(result, line, panicInfo) do { \
	if ((result) != 0) \
		ABORT_FAIL(line, "_sceAppMgrAbortHandlerInitialize", panicInfo); \
} while (0)
#else
#define ABORT_INITIALIZE_CHECK(result, line, panicInfo) do { \
	if ((result) != 0) \
		SceDebugForDriver_391B5B74(&(panicInfo), __builtin_return_address(0)); \
} while (0)
#endif

int _sceAppMgrAbortHandlerInitialize(void)
{
	int result = appmgrDebugConsoleInitialize();

	if (result < 0)
		return result;
	result = sceKernelCreateEventFlag("SceAppMgrAbort", 0, 0, NULL);
	if (result < 0)
		return result;
	g_appmgrAbortEventFlag = result;
	result = sceKernelCreateEventFlag("SceAppMgrCoredump", 0, 0, NULL);
	if (result < 0) {
		result = sceKernelDeleteEventFlag(g_appmgrAbortEventFlag);
		ABORT_INITIALIZE_CHECK(result, 1032, deleteAbortAfterCreatePanicInfo);
		return result;
	}
	g_appmgrCoredumpEventFlag = result;
	g_appmgrShellCrashed = 0;
	dmb();
	result = sceKernelCreateThread("SceAppMgrAbortHandler", _AbortHandlerThreadEntry,
				       31, 4096, 0, 0x80000, NULL);
	if (result < 0) {
		result = sceKernelDeleteEventFlag(g_appmgrAbortEventFlag);
		ABORT_INITIALIZE_CHECK(result, 1052, deleteAbortPanicInfo);
		result = sceKernelDeleteEventFlag(g_appmgrCoredumpEventFlag);
		ABORT_INITIALIZE_CHECK(result, 1054, deleteCoredumpPanicInfo);
		return result;
	}
	g_appmgrAbortThreadId = result;
	result = sceKernelStartThread(result, 0, NULL);
	ABORT_INITIALIZE_CHECK(result, 1059, startAbortPanicInfo);
	result = SceThreadmgrForDriver_5311CFB5((uintptr_t)_sceAppMgrAbort, 0);
	ABORT_INITIALIZE_CHECK(result, 1063, registerAbortPanicInfo);
	return 0;
}

#undef ABORT_INITIALIZE_CHECK

int _sceAppMgrIsExistAbortedAppOrDebugSuspendedApp(void)
{
	struct AppState *app;

	ABORT_MAIN_LOCK(1104, "_sceAppMgrIsExistAbortedAppOrDebugSuspendedApp");
	for (app = g_appmgrAppStates; app != &g_appmgrAppStates[20]; ++app) {
		if ((app->state & 0xff0) == 0x200 || (app->state & 0xf0000) == 0x10000) {
			ABORT_MAIN_UNLOCK(1108, "_sceAppMgrIsExistAbortedAppOrDebugSuspendedApp");
			return 1;
		}
	}
	ABORT_MAIN_UNLOCK(1112, "_sceAppMgrIsExistAbortedAppOrDebugSuspendedApp");
	return 0;
}

/* sharedfb's abort of a process that stopped updating its frame buffer */
int appmgrAbortProcess(SceUID processId)
{
	int result;

	result = sceKernelProcessDebugSuspend(processId, 17);
	if (result != 0)
		return result;
	if (SceSysrootForDriver_6050A467(processId) == 1) {
		SceSysrootForDriver_84C0BE05(processId);
		return 0;
	}
	_sceAppMgrAbort(processId, -1, 3);
	return 0;
}
