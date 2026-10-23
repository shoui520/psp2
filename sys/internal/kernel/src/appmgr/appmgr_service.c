/* Shell services and application path queries. */

#include <fw.h>
#include <sysmem/debug.h>
#include <intrmgr/syscall.h>
#include <threadmgr/threadmgr.h>
#include <appmgr.h>
#include <acmgr/acmgr.h>
#include <regmgr/regmgr.h>
#include <string.h>
#include "app_state_internal.h"
#include "appmgr_shell_event.h"
#include <sysmem/sysroot.h>
#include <appmgr/appmgr_rdrfp.h>
#include "appmgr_app_event.h"
#include "appmgr_device.h"
#include <display/display.h>
#include <camera/camera.h>
#include <processmgr/processmgr.h>
#include <processmgr/process_info.h>
#include <processmgr/process_budget.h>

extern SceUID g_appmgrMainMutex;
extern SceUID g_appmgrMainMutexOwner;
extern SceUID g_appmgrShellPid;
extern int g_appmgrPowerSaveMode;

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
#define SERVICE_MAIN_LOCK(_line) do { \
	int lockResult = sceKernelLockMutex(g_appmgrMainMutex, 1, NULL); \
	if (lockResult != 0) { \
		SCE_DBG_PRINTF(0, _line, "Failed to lock ret=0x%08x\n", lockResult); \
		sceAppMgrDumpInline(); \
		SCE_DBG_ASSERT_FAIL(_line); \
	} else { \
		g_appmgrMainMutexOwner = sceKernelGetThreadId(); \
	} \
} while (0)
#define SERVICE_MAIN_UNLOCK(_line) do { \
	int unlockResult; \
	g_appmgrMainMutexOwner = -1; \
	unlockResult = sceKernelUnlockMutex(g_appmgrMainMutex, 1); \
	if (unlockResult != 0) { \
		sceAppMgrDumpInline(); \
		SCE_DBG_ASSERT_PRINTF(0, _line, \
			"[SceAppMgr] The actual value is %d(0x%08x), but expected value is %d(0x%08x)\n", \
			unlockResult, unlockResult, 0, 0); \
	} \
} while (0)
#else
#define SERVICE_MAIN_LOCK(_line) do { \
	if (sceKernelLockMutex(g_appmgrMainMutex, 1, NULL) != 0) \
		appmgrAppListPanic(); \
	g_appmgrMainMutexOwner = sceKernelGetThreadId(); \
} while (0)
#define SERVICE_MAIN_UNLOCK(_line) do { \
	g_appmgrMainMutexOwner = -1; \
	if (sceKernelUnlockMutex(g_appmgrMainMutex, 1) != 0) \
		appmgrAppListPanic(); \
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

/* A driver reads the status of the application of a process, without the lock. */
int SceAppMgrForDriver_DC13160A(SceUID pid, struct SceAppMgrAppStatus *status)
{
	struct AppState *app;

	if (status == NULL)
		return 0x80802016;
	app = appmgrFindSpecialApp(g_appmgrAppStates, pid);
	if (app == NULL)
		return SCE_APPMGR_ERROR_PARAM;
	_appListGetAppStatus(app, status);
	return 0;
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

/* The plane a capture reads: the shell's and the game's are the second one. */
static inline int captureFrameBufPlane(struct AppState *app)
{
	if (app == appmgrGetShellApp())
		return 1;
	return app->launchInfo.budgetType == 0;
}

int sceAppMgrCaptureFrameBufIFTUByAppId(int appId, SceDisplayFrameBuf *frameBuf)
{
	SceUInt32 syscall = sceSyscallEnter();
	struct AppState *app;
	SceUID pid;
	int plane, result;

	if (!sceSblACMgrIsSystemProgram(0)) {
		sceSyscallLeave(syscall);
		return SCE_APPMGR_ERROR_NOEXEC;
	}
	result = _appListFindAndLockAppByAppId(appId, &app);
	if (result != 0) {
		sceSyscallLeave(syscall);
		return result;
	}
	SERVICE_MAIN_LOCK(561);
	pid = app->process.processId;
	plane = captureFrameBufPlane(app);
	SERVICE_MAIN_UNLOCK(571);
	result = SceDisplayForDriver_D4C812E5(pid, SceDisplayForDriver_C8E554C5(), plane, frameBuf);
	if (result >= 0) {
		_appListUnlockApp(app);
		sceSyscallLeave(syscall);
		return 0;
	}
	_appListUnlockApp(app);
	sceSyscallLeave(syscall);
	return result;
}

/* The frame buffer the process shows must fit the screen before and after the copy. */
static inline int checkProcFrameBuf(SceUID pid, int plane)
{
	SceDisplayFrameBufInfo info;
	int result;

	info.size = sizeof(info);
	result = SceDisplayForDriver_3BC165EF(pid, SceDisplayForDriver_C8E554C5(), plane, &info);
	if (result != 0)
		return result;
	if (info.size < sizeof(info))
		return 0x80802001;
	if (info.framebuf.size != 0) {
		if (info.framebuf.size < sizeof(info.framebuf))
			return 0x80802001;
		if (info.framebuf.width > 960 || info.framebuf.height > 544)
			return 0x80802029;
	}
	return 0;
}

int sceAppMgrCaptureFrameBufDMACByAppId(int appId, SceDisplayFrameBuf *frameBuf)
{
	SceUInt32 syscall = sceSyscallEnter();
	struct AppState *app;
	SceUID pid;
	int plane, result;

	if (!sceSblACMgrIsSystemProgram(0)) {
		sceSyscallLeave(syscall);
		return SCE_APPMGR_ERROR_NOEXEC;
	}
	result = _appListFindAndLockAppByAppId(appId, &app);
	if (result != 0) {
		sceSyscallLeave(syscall);
		return result;
	}
	SERVICE_MAIN_LOCK(635);
	pid = app->process.processId;
	plane = captureFrameBufPlane(app);
	SERVICE_MAIN_UNLOCK(645);
	result = checkProcFrameBuf(pid, plane);
	if (result == 0) {
		result = SceDisplayForDriver_707EEE2E(pid, SceDisplayForDriver_C8E554C5(), plane,
			frameBuf);
		if (result < 0) {
			_appListUnlockApp(app);
			sceSyscallLeave(syscall);
			return result;
		}
		result = checkProcFrameBuf(pid, plane);
	}
	_appListUnlockApp(app);
	sceSyscallLeave(syscall);
	return result;
}

#if FW_DEBUG
#define SERVICE_GET_PROCESS_INFO_CHECK(_result) do { \
	sceAppMgrDumpInline(); \
	SCE_DBG_ASSERT_PRINTF(0, 701, \
		"[SceAppMgr] The actual value is %d(0x%08x), but expected value is %d(0x%08x)\n", \
		_result, _result, 0, 0); \
} while (0)
#else
SCE_DBGINFO_DEFINE(static, getBudgetInfoProcessInfoPanicInfo, 701, "sceAppMgrGetBudgetInfo");
#define SERVICE_GET_PROCESS_INFO_CHECK(_result) \
	SceDebugForDriver_00CCE39C(&getBudgetInfoProcessInfoPanicInfo, \
		__builtin_return_address(0), "0x%08x,0x%08x\n", _result, 0)
#endif

int sceAppMgrGetBudgetInfo(SceKernelBudgetInfo *info)
{
	SceUInt32 syscall = sceSyscallEnter();
	SceKernelProcessInfo processInfo;
	struct AppState *app;
	int result;

	if (!SceSblACMgrForDriver_0139FC20(0) && !sceSblACMgrIsSystemProgram(0) &&
	    !isShellProcess()) {
		sceSyscallLeave(syscall);
		return SCE_APPMGR_ERROR_NOEXEC;
	}
	if (info == NULL) {
		sceSyscallLeave(syscall);
		return 0x80802016;
	}
	SERVICE_MAIN_LOCK(691);
	if (isShellProcess()) {
		memset(&processInfo, 0, sizeof(processInfo));
		processInfo.size = sizeof(processInfo);
		result = sceKernelGetProcessInfo(sceKernelGetProcessId(), &processInfo);
		if (result != 0)
			SERVICE_GET_PROCESS_INFO_CHECK(result);
		result = sceKernelGetBudgetInfo(processInfo.budgetId, info);
	} else {
		app = appmgrFindAppByProcessId(g_appmgrAppStates, -2);
		if (app == NULL) {
#if FW_DEBUG
			int unlockResult;

			g_appmgrMainMutexOwner = -1;
			unlockResult = sceKernelUnlockMutex(g_appmgrMainMutex, 1);
			if (unlockResult != 0) {
				sceAppMgrDump();
				dumpBgmState();
				SCE_DBG_ASSERT_PRINTF(0, 707,
					"[SceAppMgr] The actual value is %d(0x%08x), but expected value is %d(0x%08x)\n",
					unlockResult, unlockResult, 0, 0);
			}
#else
			SERVICE_MAIN_UNLOCK(707);
#endif
			sceSyscallLeave(syscall);
			return 0x80802012;
		}
		if (app->launchInfo.budgetType != 0) {
			SERVICE_MAIN_UNLOCK(714);
			sceSyscallLeave(syscall);
			return 0x8080201c;
		}
		result = sceKernelGetBudgetInfo(app->process.budgetId, info);
	}
	SERVICE_MAIN_UNLOCK(722);
	sceSyscallLeave(syscall);
	return result;
}

int sceAppMgrIsCameraActive(SceUID pid, int *devices)
{
	SceUInt32 syscall = sceSyscallEnter();
	struct AppState *app;
	int result;

	if (!isShellProcess()) {
		sceSyscallLeave(syscall);
		return 0x8080201c;
	}
	if (!sceSblACMgrIsSystemProgram(0)) {
		sceSyscallLeave(syscall);
		return SCE_APPMGR_ERROR_NOEXEC;
	}
	SERVICE_MAIN_LOCK(743);
	app = appmgrFindAppByProcessId(g_appmgrAppStates, pid);
	if (app == NULL) {
		SERVICE_MAIN_UNLOCK(747);
		sceSyscallLeave(syscall);
		return 0x80802012;
	}
	result = SceCameraForDriver_EB1CC2CA(app->process.processId, devices);
	SERVICE_MAIN_UNLOCK(754);
	sceSyscallLeave(syscall);
	return result;
}

/* The game application sets how the information bar shows over it. */
int sceAppMgrSetInfobarState(int visibility, int color, int transparency)
{
	SceUInt32 syscall = sceSyscallEnter();
	struct AppState *app;

	if (visibility > 1 || color > 1 || transparency > 1) {
		sceSyscallLeave(syscall);
		return 0x8080201a;
	}
	if (visibility < 0 || color < 0 || transparency < 0) {
		sceSyscallLeave(syscall);
		return 0x8080201a;
	}
	SERVICE_MAIN_LOCK(772);
	app = appmgrFindAppByProcessId(g_appmgrAppStates, -2);
	if (app == NULL) {
		SERVICE_MAIN_UNLOCK(776);
		sceSyscallLeave(syscall);
		return 0x80802012;
	}
	app->appInfobar.visibility = visibility;
	app->appInfobar.color = color;
	app->appInfobar.transparency = transparency;
	SERVICE_MAIN_UNLOCK(785);
	sceSyscallLeave(syscall);
	return 0;
}

int sceAppMgrSetInfobarStateForCommonDialog(int visibility, int color, int transparency)
{
	SceUInt32 syscall = sceSyscallEnter();
	struct AppState *app;

	if (visibility > 2 || color > 1 || transparency > 1) {
		sceSyscallLeave(syscall);
		return 0x8080201a;
	}
	if (visibility < 0 || color < 0 || transparency < 0) {
		sceSyscallLeave(syscall);
		return 0x8080201a;
	}
	SERVICE_MAIN_LOCK(803);
	app = appmgrFindAppByProcessId(g_appmgrAppStates, -2);
	if (app == NULL) {
		SERVICE_MAIN_UNLOCK(807);
		sceSyscallLeave(syscall);
		return 0x80802012;
	}
	app->commonDialogInfobar.visibility = visibility;
	app->commonDialogInfobar.color = color;
	app->commonDialogInfobar.transparency = transparency;
	SERVICE_MAIN_UNLOCK(816);
	sceSyscallLeave(syscall);
	return 0;
}

int sceAppMgrSetInfobarStateForShellByAppId(int appId, int visibility, int color,
		int transparency)
{
	SceUInt32 syscall = sceSyscallEnter();
	struct AppState *app;

	if (!isShellProcess()) {
		sceSyscallLeave(syscall);
		return 0x8080201c;
	}
	if (visibility > 2 || color > 1 || transparency > 1) {
		sceSyscallLeave(syscall);
		return 0x8080201a;
	}
	if (visibility < 0 || color < 0 || transparency < 0) {
		sceSyscallLeave(syscall);
		return 0x8080201a;
	}
	SERVICE_MAIN_LOCK(838);
	app = appmgrFindAppById(appId);
	if (app == NULL) {
		SERVICE_MAIN_UNLOCK(842);
		sceSyscallLeave(syscall);
		return 0x80802012;
	}
	app->shellInfobar.visibility = visibility;
	app->shellInfobar.color = color;
	app->shellInfobar.transparency = transparency;
	SERVICE_MAIN_UNLOCK(851);
	sceSyscallLeave(syscall);
	return 0;
}

/* The shell lists the processes of the applications, at most count of them. */
int sceAppMgrGetPidListForShell(SceUID *pidList, unsigned int count)
{
	SceUInt32 syscall = sceSyscallEnter();
	unsigned int found;
	int index;

	if (!isShellProcess()) {
		sceSyscallLeave(syscall);
		return 0x8080201c;
	}
	if (pidList == NULL) {
		sceSyscallLeave(syscall);
		return 0x80802016;
	}
	SERVICE_MAIN_LOCK(871);
	found = 0;
	for (index = 0; index < 20; index++) {
		SceUID pid = g_appmgrAppStates[index].process.processId;

		if (pid >= 0 && (pid & 1) != 0) {
			pidList[found++] = pid;
			if (found >= count)
				break;
		}
	}
	SERVICE_MAIN_UNLOCK(885);
	sceSyscallLeave(syscall);
	return found;
}

/* System programs impose or release the system's own impose states. */
int sceAppMgrSetSystemImposeState(int state)
{
	SceUInt32 syscall = sceSyscallEnter();

	if (!sceSblACMgrIsSystemProgram(0)) {
		sceSyscallLeave(syscall);
		return SCE_APPMGR_ERROR_NOEXEC;
	}
	_changeDeviceArbitrationForImpose2(state);
	sceSyscallLeave(syscall);
	return 0;
}

int sceAppMgrSetSystemImposeState2(int state)
{
	SceUInt32 syscall = sceSyscallEnter();
	int result;

	if (!sceSblACMgrIsSystemProgram(0)) {
		sceSyscallLeave(syscall);
		return SCE_APPMGR_ERROR_NOEXEC;
	}
	result = _changeDeviceArbitrationForImpose3(state);
	sceSyscallLeave(syscall);
	return result;
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

/* The development tool settings a process starts with. */
int appmgrUsesGxmDebugger(SceUID pid)
{
	int value;

	if (sceSblACMgrIsGameProgram(pid)) {
		if (sceRegMgrGetKeyInt("/DEVENV/TOOL", "game_uses_gxmdbg", &value) != 0)
			return 0;
	} else {
		if (sceRegMgrGetKeyInt("/DEVENV/TOOL", "vsh_uses_gxmdbg", &value) != 0)
			return 0;
	}
	return value != 0;
}

int appmgrUsesDefaultLibrary(void)
{
	int value = 0;

	return sceRegMgrGetKeyInt("/DEVENV/TOOL", "use_default_lib", &value) == 0 && value != 0;
}

/* The calling application's pending event counts. */
SceInt32 _sceAppMgrGetAppState(SceAppMgrAppState *appState, SceUInt32 sizeofSceAppMgrAppState,
		SceUInt32 buildVersion)
{
	SceUInt32 syscall;
	struct AppState *app;
	SceUInt32 count;

#if FW_SINCE(0x04000000)
	if (buildVersion > 0x4000000 || sizeofSceAppMgrAppState != sizeof(*appState))
#else
	if (buildVersion > 0x3650011 || sizeofSceAppMgrAppState != sizeof(*appState))
#endif
		return 0x8080201a;
	if (appState == NULL)
		return 0x80802016;
	syscall = sceSyscallEnter();
	SERVICE_MAIN_LOCK(1049);
	app = appmgrFindCurrentApp(g_appmgrAppStates);
	if (app == NULL) {
		SERVICE_MAIN_UNLOCK(1053);
		sceSyscallLeave(syscall);
		return 0x80802012;
	}
	memset(appState, 0, sizeof(*appState));
	count = app->pendingNpMessage != 0;
	if (app->pendingStoreRedemption != 0)
		count++;
	if (app->pendingStorePurchase != 0)
		count++;
	if (app->pendingResume != 0)
		count++;
	appState->systemEventNum = count;
	if (SceSblACMgrForDriver_6C5AB07F(0)) {
		count = appState->systemEventNum;
		if (app->launchInfo.processAttr & 2)
			count += (app->pendingDeactivate != 0) + (app->pendingActivate != 0);
		if (app->pendingQuit != 0)
			count++;
		appState->systemEventNum = count;
	}
	_paramQueueCompact(&app->params);
	appState->appEventNum = app->params.count;
	appState->isSystemUiOverlaid = _deviceGetAppMgrState()->inputImposed != 0 ||
		g_appmgrPowerSaveMode != 0;
	SERVICE_MAIN_UNLOCK(1079);
	sceSyscallLeave(syscall);
	return 0;
}
