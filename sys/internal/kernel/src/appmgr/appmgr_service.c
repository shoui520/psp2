/* Shell services and application path queries. */

#include <sysmem/debug.h>
#include <intrmgr/syscall.h>
#include <threadmgr/threadmgr.h>
#include <appmgr.h>
#include <acmgr/acmgr.h>
#include <string.h>
#include "app_state_internal.h"
#include "appmgr_shell_event.h"
#include <sysmem/sysroot.h>

extern SceUID g_appmgrMainMutex;
extern SceUID g_appmgrMainMutexOwner;
extern SceUID g_appmgrShellPid;
extern int appmgrResolvePath(const char *path, SceUID pid, char *output,
			     SceSize size);
extern void _appListShowAppState(struct AppState *app);
extern int dumpBgmState(void);
extern int sceAppMgrSendEventInternal(const SceAppMgrSystemEvent *event,
				     SceUID pid);
extern int _appListPushEvent(struct AppState *app,
			     const SceAppMgrSystemEvent *event);
extern void _shellEventRecommendedScreenOrientationStatusChangedQueuePush(void);

#if FW_DEBUG
SCE_DBGINFO_DEFINE(static, appmgrDumpSeparatorInfo, 31, "sceAppMgrDump");
SCE_DBGINFO_DEFINE(static, appmgrDumpTitleInfo, 32, "sceAppMgrDump");
SCE_DBGINFO_DEFINE(static, appmgrDumpHeaderInfo, 33, "sceAppMgrDump");
SCE_DBGINFO_DEFINE(static, appmgrDumpIndexInfo, 37, "sceAppMgrDump");

/* Sony inlines the complete dump at each failed service mutex operation. */
#define sceAppMgrDumpInline() do { \
	int index; \
	SceDebugForDriver_D9703808(1, 0, &appmgrDumpSeparatorInfo, \
		"------------------------------------------\n"); \
	SceDebugForDriver_D9703808(1, 0, &appmgrDumpTitleInfo, "AppMgrDump\n"); \
	SceDebugForDriver_D9703808(1, 0, &appmgrDumpHeaderInfo, \
		"------------------------------------------\n"); \
	for (index = 0; index != 20; ++index) { \
		struct AppState *app = &g_appmgrAppStates[index]; \
		if (app->appId > 0 || app->process.processId > 0) { \
			SceDebugForDriver_D9703808(1, 0, &appmgrDumpIndexInfo, \
				"[%d]\n", index); \
			_appListShowAppState(app); \
		} \
	} \
	dumpBgmState(); \
} while (0)
#endif

#if FW_DEBUG
/* Release builds inline this method into its two service callers. */
int _sceAppMgrSendSysOrientation(int orientation)
{
	struct AppState *app;
	int result;

	result = sceKernelLockMutex(g_appmgrMainMutex, 1, NULL);
	if (result != 0) {
		SCE_DBG_PRINTF(0, 1141, "Failed to lock ret=0x%08x\n", result);
		sceAppMgrDumpInline();
		SCE_DBG_ASSERT_FAIL(1141);
	} else {
		g_appmgrMainMutexOwner = sceKernelGetThreadId();
	}
	app = appmgrFindAppByProcessId(g_appmgrAppStates, sceKernelGetProcessId());
	if (!app) {
		g_appmgrMainMutexOwner = -1;
		result = sceKernelUnlockMutex(g_appmgrMainMutex, 1);
		if (result != 0) {
			sceAppMgrDumpInline();
			SCE_DBG_ASSERT_PRINTF(0, 1145,
				"[SceAppMgr] The actual value is %d(0x%08x), but expected value is %d(0x%08x)\n",
				result, result, 0, 0);
		}
		return 0x8080201c;
	}
	app->systemOrientation = orientation;
	g_appmgrMainMutexOwner = -1;
	result = sceKernelUnlockMutex(g_appmgrMainMutex, 1);
	if (result != 0) {
		sceAppMgrDumpInline();
		SCE_DBG_ASSERT_PRINTF(0, 1151,
			"[SceAppMgr] The actual value is %d(0x%08x), but expected value is %d(0x%08x)\n",
			result, result, 0, 0);
	}
	_shellEventRecommendedScreenOrientationStatusChangedQueuePush();
	return 0;
}
#endif

int sceAppMgrDump(void)
{
	int index;

#if FW_DEBUG
	SceDebugForDriver_D9703808(1, 0, &appmgrDumpSeparatorInfo,
		"------------------------------------------\n");
	SceDebugForDriver_D9703808(1, 0, &appmgrDumpTitleInfo, "AppMgrDump\n");
	SceDebugForDriver_D9703808(1, 0, &appmgrDumpHeaderInfo,
		"------------------------------------------\n");
#endif
	for (index = 0; index != 20; ++index) {
		struct AppState *app = &g_appmgrAppStates[index];

		if (app->appId > 0 || app->process.processId > 0) {
#if FW_DEBUG
			SceDebugForDriver_D9703808(1, 0, &appmgrDumpIndexInfo,
				"[%d]\n", index);
#endif
			_appListShowAppState(app);
		}
	}
	return 0;
}

SceUID sceAppMgrGetShellPidForDriver(void)
{
	SceUID pid;
	int result;

	result = sceKernelLockMutex(g_appmgrMainMutex, 1, NULL);
	if (result != 0) {
#if FW_DEBUG
		SCE_DBG_PRINTF(0, 47, "Failed to lock ret=0x%08x\n", result);
		sceAppMgrDumpInline();
		SCE_DBG_ASSERT_FAIL(47);
#else
		appmgrAppListPanic();
#endif
	} else {
		(void)sceKernelGetThreadId();
	}
	g_appmgrMainMutexOwner = -1;
	pid = g_appmgrShellPid;
	result = sceKernelUnlockMutex(g_appmgrMainMutex, 1);
	if (result != 0) {
#if FW_DEBUG
		sceAppMgrDumpInline();
		SCE_DBG_ASSERT_PRINTF(0, 52,
			"[SceAppMgr] The actual value is %d(0x%08x), but expected value is %d(0x%08x)\n",
			result, result, 0, 0);
#else
		appmgrAppListPanic();
#endif
	}
	return pid;
}

int sceAppMgrGetNameById(SceUID pid, char *appName)
{
	SceUInt32 syscall = sceSyscallEnter();
	struct AppState *app;
	int result;

	if (!sceSblACMgrIsSystemProgram(0)) {
		sceSyscallLeave(syscall);
		return 0x8080201f;
	}
	if (!appName) {
		sceSyscallLeave(syscall);
		return 0x80802016;
	}
	result = sceKernelLockMutex(g_appmgrMainMutex, 1, NULL);
	if (result != 0) {
#if FW_DEBUG
		SCE_DBG_PRINTF(0, 83, "Failed to lock ret=0x%08x\n", result);
		sceAppMgrDumpInline();
		SCE_DBG_ASSERT_FAIL(83);
#else
		appmgrAppListPanic();
#endif
	} else {
		g_appmgrMainMutexOwner = sceKernelGetThreadId();
	}
	app = appmgrFindAppByProcessId(g_appmgrAppStates, pid);
	if (!app) {
		g_appmgrMainMutexOwner = -1;
		result = sceKernelUnlockMutex(g_appmgrMainMutex, 1);
		if (result != 0) {
#if FW_DEBUG
			sceAppMgrDumpInline();
			SCE_DBG_ASSERT_PRINTF(0, 87,
				"[SceAppMgr] The actual value is %d(0x%08x), but expected value is %d(0x%08x)\n",
				result, result, 0, 0);
#else
			appmgrAppListPanic();
#endif
		}
		sceSyscallLeave(syscall);
		return 0x80802002;
	}
	strncpy(appName, app->process.appName, 32);
	g_appmgrMainMutexOwner = -1;
	result = sceKernelUnlockMutex(g_appmgrMainMutex, 1);
	if (result != 0) {
#if FW_DEBUG
		sceAppMgrDumpInline();
		SCE_DBG_ASSERT_PRINTF(0, 95,
			"[SceAppMgr] The actual value is %d(0x%08x), but expected value is %d(0x%08x)\n",
			result, result, 0, 0);
#else
		appmgrAppListPanic();
#endif
	}
	sceSyscallLeave(syscall);
	return 0;
}

int sceAppMgrGetAppIdByAppId(int appId)
{
	SceUInt32 syscall = sceSyscallEnter();
	struct AppState *app;
	SceUID pid;
	int result;
	int id;

	sceKernelApiDeclareTypeUInt32();
	if (!SceSblACMgrForDriver_991FDC15(0))
		goto denied;
	app = appmgrGetShellApp();
	if (app == NULL)
		goto denied;
	pid = app->process.processId;
	if (pid != sceKernelGetProcessId())
		goto denied;
	result = sceKernelLockMutex(g_appmgrMainMutex, 1, NULL);
	if (result != 0) {
#if FW_DEBUG
		SCE_DBG_PRINTF(0, 117, "Failed to lock ret=0x%08x\n", result);
		sceAppMgrDumpInline();
		SCE_DBG_ASSERT_FAIL(117);
#else
		appmgrAppListPanic();
#endif
	} else {
		g_appmgrMainMutexOwner = sceKernelGetThreadId();
	}
	id = -1;
	app = appmgrFindAppById(appId);
	if (app != NULL)
		id = app->appId;
	g_appmgrMainMutexOwner = -1;
	result = sceKernelUnlockMutex(g_appmgrMainMutex, 1);
	if (result != 0) {
#if FW_DEBUG
		sceAppMgrDumpInline();
		SCE_DBG_ASSERT_PRINTF(0, 126,
			"[SceAppMgr] The actual value is %d(0x%08x), but expected value is %d(0x%08x)\n",
			result, result, 0, 0);
#else
		appmgrAppListPanic();
#endif
	}
	sceSyscallLeave(syscall);
	return id;

denied:
	sceSyscallLeave(syscall);
	return 0x8080201f;
}

int sceAppMgrGetIdByName(SceUID *pid, const char *appName)
{
	SceUInt32 syscall = sceSyscallEnter();
	struct AppState *app;
	int result;
	int status;

	if (!sceSblACMgrIsSystemProgram(0)) {
		sceSyscallLeave(syscall);
		return 0x8080201f;
	}
	if (appName == NULL || pid == NULL) {
		sceSyscallLeave(syscall);
		return 0x80802016;
	}
	result = sceKernelLockMutex(g_appmgrMainMutex, 1, NULL);
	if (result != 0) {
#if FW_DEBUG
		SCE_DBG_PRINTF(0, 155, "Failed to lock ret=0x%08x\n", result);
		sceAppMgrDumpInline();
		SCE_DBG_ASSERT_FAIL(155);
#else
		appmgrAppListPanic();
#endif
	} else {
		g_appmgrMainMutexOwner = sceKernelGetThreadId();
	}
	app = appmgrFindAppByName(g_appmgrAppStates, appName);
	if (app != NULL) {
		status = 0;
		*pid = app->process.processId;
	} else {
		status = 0x80802012;
		*pid = -1;
	}
	g_appmgrMainMutexOwner = -1;
	result = sceKernelUnlockMutex(g_appmgrMainMutex, 1);
	if (result != 0) {
#if FW_DEBUG
		sceAppMgrDumpInline();
		SCE_DBG_ASSERT_PRINTF(0, 167,
			"[SceAppMgr] The actual value is %d(0x%08x), but expected value is %d(0x%08x)\n",
			result, result, 0, 0);
#else
		appmgrAppListPanic();
#endif
	}
	sceSyscallLeave(syscall);
	return status;
}

SceUID sceAppMgrGetProcessIdByAppIdForShell(SceInt32 appId)
{
	SceUInt32 syscall = sceSyscallEnter();
	struct AppState *app;
	SceUID pid;
	int result;

	if (!SceSblACMgrForDriver_991FDC15(0))
		goto denied;
	app = appmgrGetShellApp();
	if (app == NULL)
		goto denied;
	pid = app->process.processId;
	if (pid != sceKernelGetProcessId())
		goto denied;
	result = sceKernelLockMutex(g_appmgrMainMutex, 1, NULL);
	if (result != 0) {
#if FW_DEBUG
		SCE_DBG_PRINTF(0, 204, "Failed to lock ret=0x%08x\n", result);
		sceAppMgrDumpInline();
		SCE_DBG_ASSERT_FAIL(204);
#else
		appmgrAppListPanic();
#endif
	} else {
		g_appmgrMainMutexOwner = sceKernelGetThreadId();
	}
	app = appmgrFindAppById(appId);
	if (app == NULL) {
		g_appmgrMainMutexOwner = -1;
		result = sceKernelUnlockMutex(g_appmgrMainMutex, 1);
		if (result != 0) {
#if FW_DEBUG
			sceAppMgrDump();
			dumpBgmState();
			SCE_DBG_ASSERT_PRINTF(0, 208,
				"[SceAppMgr] The actual value is %d(0x%08x), but expected value is %d(0x%08x)\n",
				result, result, 0, 0);
#else
			appmgrAppListPanic();
#endif
		}
		sceSyscallLeave(syscall);
		return 0x80802012;
	}
	pid = app->process.processId;
	if (pid == -1)
		sceKernelApiDeclareTypeRaw();
	g_appmgrMainMutexOwner = -1;
	result = sceKernelUnlockMutex(g_appmgrMainMutex, 1);
	if (result != 0) {
#if FW_DEBUG
		sceAppMgrDumpInline();
		SCE_DBG_ASSERT_PRINTF(0, 217,
			"[SceAppMgr] The actual value is %d(0x%08x), but expected value is %d(0x%08x)\n",
			result, result, 0, 0);
#else
		appmgrAppListPanic();
#endif
	}
	sceSyscallLeave(syscall);
	return pid;

denied:
	sceSyscallLeave(syscall);
	return 0x8080201f;
}

int sceAppMgrGetStatusById(SceUID id, void *appStatus)
{
	SceUInt32 syscall = sceSyscallEnter();
	struct AppState *app;
	int result;
	int statusResult;

	if (!sceSblACMgrIsSystemProgram(0)) {
		sceSyscallLeave(syscall);
		return 0x8080201f;
	}
	if (appStatus == NULL) {
		sceSyscallLeave(syscall);
		return 0x80802016;
	}
	result = sceKernelLockMutex(g_appmgrMainMutex, 1, NULL);
	if (result != 0) {
#if FW_DEBUG
		SCE_DBG_PRINTF(0, 244, "Failed to lock ret=0x%08x\n", result);
		sceAppMgrDumpInline();
		SCE_DBG_ASSERT_FAIL(244);
#else
		appmgrAppListPanic();
#endif
	} else {
		g_appmgrMainMutexOwner = sceKernelGetThreadId();
	}
	app = appmgrFindAppByProcessId(g_appmgrAppStates, id);
	if (app == NULL) {
		g_appmgrMainMutexOwner = -1;
		result = sceKernelUnlockMutex(g_appmgrMainMutex, 1);
		if (result != 0) {
#if FW_DEBUG
			sceAppMgrDumpInline();
			SCE_DBG_ASSERT_PRINTF(0, 248,
				"[SceAppMgr] The actual value is %d(0x%08x), but expected value is %d(0x%08x)\n",
				result, result, 0, 0);
#else
			appmgrAppListPanic();
#endif
		}
		sceSyscallLeave(syscall);
		return 0x80802012;
	}
	statusResult = _appListGetAppStatus(app, appStatus);
	g_appmgrMainMutexOwner = -1;
	result = sceKernelUnlockMutex(g_appmgrMainMutex, 1);
	if (result != 0) {
#if FW_DEBUG
		sceAppMgrDumpInline();
		SCE_DBG_ASSERT_PRINTF(0, 256,
			"[SceAppMgr] The actual value is %d(0x%08x), but expected value is %d(0x%08x)\n",
			result, result, 0, 0);
#else
		appmgrAppListPanic();
#endif
	}
	sceSyscallLeave(syscall);
	return statusResult;
}

int sceAppMgrGetStatusByAppId(int appId, void *appStatus)
{
	SceUInt32 syscall = sceSyscallEnter();
	struct AppState *app;
	int result;
	int statusResult;

	if (!sceSblACMgrIsSystemProgram(0)) {
		sceSyscallLeave(syscall);
		return 0x8080201f;
	}
	if (appStatus == NULL) {
		sceSyscallLeave(syscall);
		return 0x80802016;
	}
	result = sceKernelLockMutex(g_appmgrMainMutex, 1, NULL);
	if (result != 0) {
#if FW_DEBUG
		SCE_DBG_PRINTF(0, 284, "Failed to lock ret=0x%08x\n", result);
		sceAppMgrDumpInline();
		SCE_DBG_ASSERT_FAIL(284);
#else
		appmgrAppListPanic();
#endif
	} else {
		g_appmgrMainMutexOwner = sceKernelGetThreadId();
	}
	app = appmgrFindAppById(appId);
	if (app == NULL) {
		g_appmgrMainMutexOwner = -1;
		result = sceKernelUnlockMutex(g_appmgrMainMutex, 1);
		if (result != 0) {
#if FW_DEBUG
			sceAppMgrDumpInline();
			SCE_DBG_ASSERT_PRINTF(0, 288,
				"[SceAppMgr] The actual value is %d(0x%08x), but expected value is %d(0x%08x)\n",
				result, result, 0, 0);
#else
			appmgrAppListPanic();
#endif
		}
		sceSyscallLeave(syscall);
		return 0x80802012;
	}
	statusResult = _appListGetAppStatus(app, appStatus);
	g_appmgrMainMutexOwner = -1;
	result = sceKernelUnlockMutex(g_appmgrMainMutex, 1);
	if (result != 0) {
#if FW_DEBUG
		sceAppMgrDumpInline();
		SCE_DBG_ASSERT_PRINTF(0, 296,
			"[SceAppMgr] The actual value is %d(0x%08x), but expected value is %d(0x%08x)\n",
			result, result, 0, 0);
#else
		appmgrAppListPanic();
#endif
	}
	sceSyscallLeave(syscall);
	return statusResult;
}

int sceAppMgrGetStatusByName(const char *appName, void *appStatus)
{
	SceUInt32 syscall = sceSyscallEnter();
	struct AppState *app;
	int result;
	int statusResult;

	if (!sceSblACMgrIsSystemProgram(0) && appName != NULL) {
		sceSyscallLeave(syscall);
		return 0x8080201c;
	}
	if (appStatus == NULL) {
		sceSyscallLeave(syscall);
		return 0x80802016;
	}
	result = sceKernelLockMutex(g_appmgrMainMutex, 1, NULL);
	if (result != 0) {
#if FW_DEBUG
		SCE_DBG_PRINTF(0, 327, "Failed to lock ret=0x%08x\n", result);
		sceAppMgrDumpInline();
		SCE_DBG_ASSERT_FAIL(327);
#else
		appmgrAppListPanic();
#endif
	} else {
		g_appmgrMainMutexOwner = sceKernelGetThreadId();
	}
	app = appmgrFindAppByName(g_appmgrAppStates, appName);
	if (app == NULL) {
		g_appmgrMainMutexOwner = -1;
		result = sceKernelUnlockMutex(g_appmgrMainMutex, 1);
		if (result != 0) {
#if FW_DEBUG
			sceAppMgrDumpInline();
			SCE_DBG_ASSERT_PRINTF(0, 332,
				"[SceAppMgr] The actual value is %d(0x%08x), but expected value is %d(0x%08x)\n",
				result, result, 0, 0);
#else
			appmgrAppListPanic();
#endif
		}
		sceSyscallLeave(syscall);
		return 0x80802012;
	}
	statusResult = _appListGetAppStatus(app, appStatus);
	g_appmgrMainMutexOwner = -1;
	result = sceKernelUnlockMutex(g_appmgrMainMutex, 1);
	if (result != 0) {
#if FW_DEBUG
		sceAppMgrDumpInline();
		SCE_DBG_ASSERT_PRINTF(0, 340,
			"[SceAppMgr] The actual value is %d(0x%08x), but expected value is %d(0x%08x)\n",
			result, result, 0, 0);
#else
		appmgrAppListPanic();
#endif
	}
	sceSyscallLeave(syscall);
	return statusResult;
}

SceInt32 sceAppMgrGetAppInfo(const char *appName, SceAppMgrAppState *output)
{
	SceUInt32 syscall = sceSyscallEnter();
	struct SceAppMgrAppInfo *info = (void *)output;
	struct AppState *app;
	SceUID mutex;
	int result;

	if (!SceSblACMgrForDriver_6C5AB07F(0)) {
		sceSyscallLeave(syscall);
		return 0x8080201f;
	}
	if (info == NULL) {
		sceSyscallLeave(syscall);
		return 0x80802016;
	}
	result = sceKernelLockMutex(g_appmgrMainMutex, 1, NULL);
	if (result != 0) {
#if FW_DEBUG
		SCE_DBG_PRINTF(0, 361, "Failed to lock ret=0x%08x\n", result);
		sceAppMgrDumpInline();
		SCE_DBG_ASSERT_FAIL(361);
#else
		appmgrAppListPanic();
#endif
	} else {
		g_appmgrMainMutexOwner = sceKernelGetThreadId();
	}
	app = appmgrFindAppByName(g_appmgrAppStates, appName);
	if (app == NULL) {
		g_appmgrMainMutexOwner = -1;
		result = sceKernelUnlockMutex(g_appmgrMainMutex, 1);
		if (result != 0) {
#if FW_DEBUG
			sceAppMgrDumpInline();
			SCE_DBG_ASSERT_PRINTF(0, 364,
				"[SceAppMgr] The actual value is %d(0x%08x), but expected value is %d(0x%08x)\n",
				result, result, 0, 0);
#else
			appmgrAppListPanic();
#endif
		}
		sceSyscallLeave(syscall);
		return 0x80802012;
	}
	memset(info, 0, sizeof(*info));
	mutex = g_appmgrMainMutex;
	info->processRunRequested = app->process.requestedProcessState == 2;
	g_appmgrMainMutexOwner = -1;
	result = sceKernelUnlockMutex(mutex, 1);
	if (result != 0) {
#if FW_DEBUG
		sceAppMgrDumpInline();
		SCE_DBG_ASSERT_PRINTF(0, 374,
			"[SceAppMgr] The actual value is %d(0x%08x), but expected value is %d(0x%08x)\n",
			result, result, 0, 0);
#else
		appmgrAppListPanic();
#endif
	}
	sceSyscallLeave(syscall);
	return 0;
}

int sceAppMgrGetFgAppInfo(void *output)
{
	SceUInt32 syscall = sceSyscallEnter();
	struct SceAppMgrAppInfo *info = output;
	struct AppState *app;
	SceUID mutex;
	int result;

	if (!SceSblACMgrForDriver_6C5AB07F(0)) {
		sceSyscallLeave(syscall);
		return 0x8080201f;
	}
	if (info == NULL) {
		sceSyscallLeave(syscall);
		return 0x80802016;
	}
	result = sceKernelLockMutex(g_appmgrMainMutex, 1, NULL);
	if (result != 0) {
#if FW_DEBUG
		SCE_DBG_PRINTF(0, 395, "Failed to lock ret=0x%08x\n", result);
		sceAppMgrDumpInline();
		SCE_DBG_ASSERT_FAIL(395);
#else
		appmgrAppListPanic();
#endif
	} else {
		g_appmgrMainMutexOwner = sceKernelGetThreadId();
	}
	app = appmgrFindAppByProcessId(g_appmgrAppStates, -2);
	if (app == NULL) {
		g_appmgrMainMutexOwner = -1;
		result = sceKernelUnlockMutex(g_appmgrMainMutex, 1);
		if (result != 0) {
#if FW_DEBUG
			sceAppMgrDumpInline();
			SCE_DBG_ASSERT_PRINTF(0, 398,
				"[SceAppMgr] The actual value is %d(0x%08x), but expected value is %d(0x%08x)\n",
				result, result, 0, 0);
#else
			appmgrAppListPanic();
#endif
		}
		sceSyscallLeave(syscall);
		return 0x80802012;
	}
	app = appmgrFindAppByName(g_appmgrAppStates,
				app->process.content.foregroundLookupName);
	if (app == NULL) {
		g_appmgrMainMutexOwner = -1;
		result = sceKernelUnlockMutex(g_appmgrMainMutex, 1);
		if (result != 0) {
#if FW_DEBUG
			sceAppMgrDumpInline();
			SCE_DBG_ASSERT_PRINTF(0, 404,
				"[SceAppMgr] The actual value is %d(0x%08x), but expected value is %d(0x%08x)\n",
				result, result, 0, 0);
#else
			appmgrAppListPanic();
#endif
		}
		sceSyscallLeave(syscall);
		return 0x80802012;
	}
	memset(info, 0, sizeof(*info));
	mutex = g_appmgrMainMutex;
	info->processRunRequested = app->process.requestedProcessState == 2;
	g_appmgrMainMutexOwner = -1;
	result = sceKernelUnlockMutex(mutex, 1);
	if (result != 0) {
#if FW_DEBUG
		sceAppMgrDumpInline();
		SCE_DBG_ASSERT_PRINTF(0, 415,
			"[SceAppMgr] The actual value is %d(0x%08x), but expected value is %d(0x%08x)\n",
			result, result, 0, 0);
#else
		appmgrAppListPanic();
#endif
	}
	sceSyscallLeave(syscall);
	return 0;
}

int sceAppMgrSendLiveBoardMode(int mode)
{
	SceUInt32 syscall = sceSyscallEnter();
	SceAppMgrSystemEvent event;
	struct AppState *shell;
	struct AppState *app;
	SceUID shellPid;
	SceUID callerPid;
	int result;

	memset(&event, 0, sizeof(event));
	if (!sceSblACMgrIsSystemProgram(0)) {
		sceSyscallLeave(syscall);
		return 0x8080201f;
	}
	result = sceKernelLockMutex(g_appmgrMainMutex, 1, NULL);
	if (result != 0) {
#if FW_DEBUG
		SCE_DBG_PRINTF(0, 446, "Failed to lock ret=0x%08x\n", result);
		sceAppMgrDumpInline();
		SCE_DBG_ASSERT_FAIL(446);
#else
		appmgrAppListPanic();
#endif
	} else {
		g_appmgrMainMutexOwner = sceKernelGetThreadId();
	}
	shell = appmgrGetShellApp();
	if (shell == NULL) {
		g_appmgrMainMutexOwner = -1;
		result = sceKernelUnlockMutex(g_appmgrMainMutex, 1);
		if (result != 0) {
#if FW_DEBUG
			sceAppMgrDumpInline();
			SCE_DBG_ASSERT_PRINTF(0, 450,
				"[SceAppMgr] The actual value is %d(0x%08x), but expected value is %d(0x%08x)\n",
				result, result, 0, 0);
#else
			appmgrAppListPanic();
#endif
		}
		sceSyscallLeave(syscall);
		return 0x80802013;
	}
	app = appmgrFindAppByProcessId(g_appmgrAppStates,
				     sceKernelGetProcessId());
	if (app == NULL) {
		g_appmgrMainMutexOwner = -1;
		result = sceKernelUnlockMutex(g_appmgrMainMutex, 1);
		if (result != 0) {
#if FW_DEBUG
			sceAppMgrDumpInline();
			SCE_DBG_ASSERT_PRINTF(0, 457,
				"[SceAppMgr] The actual value is %d(0x%08x), but expected value is %d(0x%08x)\n",
				result, result, 0, 0);
#else
			appmgrAppListPanic();
#endif
		}
		/* Sony returns here without restoring the syscall state. */
		return 0x8080201c;
	}
	shellPid = shell->process.processId;
	g_appmgrMainMutexOwner = -1;
	result = sceKernelUnlockMutex(g_appmgrMainMutex, 1);
	if (result != 0) {
#if FW_DEBUG
		sceAppMgrDumpInline();
		SCE_DBG_ASSERT_PRINTF(0, 464,
			"[SceAppMgr] The actual value is %d(0x%08x), but expected value is %d(0x%08x)\n",
			result, result, 0, 0);
#else
		appmgrAppListPanic();
#endif
	}
	if (mode == 0) {
		event.systemEvent = 0x10000101;
	} else if (mode == 1) {
		event.systemEvent = 0x10000102;
	} else {
		sceSyscallLeave(syscall);
		return 0x80802002;
	}
	callerPid = sceKernelGetProcessId();
	__builtin_memcpy(event.data.reserved, &callerPid, sizeof(callerPid));
	result = sceAppMgrSendEventInternal(&event, shellPid);
	callerPid = sceKernelGetProcessId();
	_shellEventLiveareaModeChangedQueuePush(callerPid, app->appId, mode);
	sceSyscallLeave(syscall);
	return result;
}

int sceAppMgrNotifyLiveBoardModeForShell(int appId, int mode)
{
	SceUInt32 syscall = sceSyscallEnter();
	SceAppMgrSystemEvent event;
	struct AppState *app;
	SceUID shellPid;
	int result;
	int eventResult;

	if (!SceSblACMgrForDriver_991FDC15(0))
		goto denied;
	app = appmgrGetShellApp();
	if (app == NULL)
		goto denied;
	shellPid = app->process.processId;
	if (shellPid != sceKernelGetProcessId())
		goto denied;
	result = sceKernelLockMutex(g_appmgrMainMutex, 1, NULL);
	if (result != 0) {
#if FW_DEBUG
		SCE_DBG_PRINTF(0, 509, "Failed to lock ret=0x%08x\n", result);
		sceAppMgrDumpInline();
		SCE_DBG_ASSERT_FAIL(509);
#else
		appmgrAppListPanic();
#endif
	} else {
		g_appmgrMainMutexOwner = sceKernelGetThreadId();
	}
	app = appmgrFindAppById(appId);
	if (app == NULL) {
		g_appmgrMainMutexOwner = -1;
		result = sceKernelUnlockMutex(g_appmgrMainMutex, 1);
		if (result != 0) {
#if FW_DEBUG
			sceAppMgrDumpInline();
			SCE_DBG_ASSERT_PRINTF(0, 513,
				"[SceAppMgr] The actual value is %d(0x%08x), but expected value is %d(0x%08x)\n",
				result, result, 0, 0);
#else
			appmgrAppListPanic();
#endif
		}
		sceSyscallLeave(syscall);
		return 0x80802012;
	}
	memset(&event, 0, sizeof(event));
	if (mode == 0) {
		event.systemEvent = 0x10000101;
	} else if (mode == 1) {
		event.systemEvent = 0x10000102;
	} else {
		g_appmgrMainMutexOwner = -1;
		result = sceKernelUnlockMutex(g_appmgrMainMutex, 1);
		if (result != 0) {
#if FW_DEBUG
			sceAppMgrDumpInline();
			SCE_DBG_ASSERT_PRINTF(0, 526,
				"[SceAppMgr] The actual value is %d(0x%08x), but expected value is %d(0x%08x)\n",
				result, result, 0, 0);
#else
			appmgrAppListPanic();
#endif
		}
		sceSyscallLeave(syscall);
		return 0x80802002;
	}
	eventResult = _appListPushEvent(app, &event);
	g_appmgrMainMutexOwner = -1;
	result = sceKernelUnlockMutex(g_appmgrMainMutex, 1);
	if (result != 0) {
#if FW_DEBUG
		sceAppMgrDumpInline();
		SCE_DBG_ASSERT_PRINTF(0, 533,
			"[SceAppMgr] The actual value is %d(0x%08x), but expected value is %d(0x%08x)\n",
			result, result, 0, 0);
#else
		appmgrAppListPanic();
#endif
	}
	sceSyscallLeave(syscall);
	return eventResult;

denied:
	sceSyscallLeave(syscall);
	return 0x8080201c;
}

int sceAppMgrGetRawPathOfApp0ByAppIdForShell(int appId, char path[292])
{
	SceUInt32 syscall = sceSyscallEnter();
	struct AppState *app;
	SceUID pid;
	int result;

	if (!SceSblACMgrForDriver_991FDC15(0)) {
		sceSyscallLeave(syscall);
		return 0x8080201f;
	}
	app = appmgrGetShellApp();
	if (!app || app->process.processId != sceKernelGetProcessId()) {
		sceSyscallLeave(syscall);
		return 0x8080201f;
	}
	if (!path) {
		sceSyscallLeave(syscall);
		return 0x80802016;
	}
	result = sceKernelLockMutex(g_appmgrMainMutex, 1, NULL);
	if (result != 0) {
#if FW_DEBUG
		SCE_DBG_PRINTF(0, 937, "Failed to lock ret=0x%08x\n", result);
		sceAppMgrDumpInline();
		SCE_DBG_ASSERT_FAIL(937);
#else
		appmgrAppListPanic();
#endif
	} else {
		g_appmgrMainMutexOwner = sceKernelGetThreadId();
	}
	app = appmgrFindAppById(appId);
	if (!app) {
		g_appmgrMainMutexOwner = -1;
		result = sceKernelUnlockMutex(g_appmgrMainMutex, 1);
		if (result != 0) {
#if FW_DEBUG
			sceAppMgrDumpInline();
			SCE_DBG_ASSERT_PRINTF(0, 941,
				"[SceAppMgr] The actual value is %d(0x%08x), but expected value is %d(0x%08x)\n",
				result, result, 0, 0);
#else
			appmgrAppListPanic();
#endif
		}
		sceSyscallLeave(syscall);
		return 0x80802012;
	}
#if FW_DEBUG
	g_appmgrMainMutexOwner = -1;
	pid = app->process.processId;
#else
	pid = app->process.processId;
	g_appmgrMainMutexOwner = -1;
#endif
	result = sceKernelUnlockMutex(g_appmgrMainMutex, 1);
	if (result != 0) {
#if FW_DEBUG
		sceAppMgrDumpInline();
		SCE_DBG_ASSERT_PRINTF(0, 946,
			"[SceAppMgr] The actual value is %d(0x%08x), but expected value is %d(0x%08x)\n",
			result, result, 0, 0);
#else
		appmgrAppListPanic();
#endif
	}
	result = appmgrResolvePath("app0:", pid, path, 292);
	sceSyscallLeave(syscall);
	return result;
}

int sceAppMgrGetRawPathOfApp0ByPidForShell(SceUID pid, char *path)
{
	SceUInt32 syscall = sceSyscallEnter();
	struct AppState *app;
	int result;

	if (!SceSblACMgrForDriver_991FDC15(0)) {
		sceSyscallLeave(syscall);
		return 0x8080201f;
	}
	app = appmgrGetShellApp();
	if (!app || app->process.processId != sceKernelGetProcessId()) {
		sceSyscallLeave(syscall);
		return 0x8080201f;
	}
	if (!path) {
		sceSyscallLeave(syscall);
		return 0x80802016;
	}
	result = sceKernelLockMutex(g_appmgrMainMutex, 1, NULL);
	if (result != 0) {
#if FW_DEBUG
		SCE_DBG_PRINTF(0, 968, "Failed to lock ret=0x%08x\n", result);
		sceAppMgrDumpInline();
		SCE_DBG_ASSERT_FAIL(968);
#else
		appmgrAppListPanic();
#endif
	} else {
		g_appmgrMainMutexOwner = sceKernelGetThreadId();
	}
	app = appmgrFindAppByProcessId(g_appmgrAppStates, pid);
	g_appmgrMainMutexOwner = -1;
	if (!app) {
		result = sceKernelUnlockMutex(g_appmgrMainMutex, 1);
		if (result != 0) {
#if FW_DEBUG
			sceAppMgrDumpInline();
			SCE_DBG_ASSERT_PRINTF(0, 972,
				"[SceAppMgr] The actual value is %d(0x%08x), but expected value is %d(0x%08x)\n",
				result, result, 0, 0);
#else
			appmgrAppListPanic();
#endif
		}
		sceSyscallLeave(syscall);
		return 0x80802012;
	}
	result = sceKernelUnlockMutex(g_appmgrMainMutex, 1);
	if (result != 0) {
#if FW_DEBUG
		sceAppMgrDumpInline();
		SCE_DBG_ASSERT_PRINTF(0, 976,
			"[SceAppMgr] The actual value is %d(0x%08x), but expected value is %d(0x%08x)\n",
			result, result, 0, 0);
#else
		appmgrAppListPanic();
#endif
	}
	result = appmgrResolvePath("app0:", pid, path, 292);
	sceSyscallLeave(syscall);
	return result;
}
