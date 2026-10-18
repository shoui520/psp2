/* Application system events. */

#include <stddef.h>
#include <sysmem/debug.h>
#include <intrmgr/syscall.h>
#include <acmgr/acmgr.h>
#include <psp2kern/kernel/threadmgr.h>
#include <psp2/appmgr.h>
#include <string.h>
#include "app_state_internal.h"
#include "appmgr_app_event.h"

extern SceUID g_appmgrMainMutex;
extern SceUID g_appmgrMainMutexOwner;
extern int sceAppMgrDump(void);
extern int dumpBgmState(void);
extern struct AppState *appmgrFindCurrentApp(struct AppState *apps);

#if !FW_DEBUG
static const SceKernelDebugInfo appEventMutexPanicInfo = {
	.fileHash = 0xc9656e8e,
	.lineHash = 0x79d3a1f0,
	.funcHash = 0x9d536bce,
};

static const SceKernelDebugInfo appEventArgumentPanicInfo = {
	.fileHash = 0xc9656e8e,
	.lineHash = 0xc879dadc,
	.funcHash = 0x11a05ee1,
};

_Noreturn void appmgrAppEventArgumentPanic(void)
{
	SceDebugForDriver_391B5B74(&appEventArgumentPanicInfo, __builtin_return_address(0));
}

_Noreturn void appmgrAppEventMutexPanic(void)
{
	SceDebugForDriver_391B5B74(&appEventMutexPanicInfo, __builtin_return_address(0));
}
#endif

#if FW_DEBUG
#define APP_EVENT_LOCK(_line) do { \
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

#define APP_EVENT_UNLOCK(_line) do { \
	int unlockResult; \
	SceUID mutex = g_appmgrMainMutex; \
	g_appmgrMainMutexOwner = -1; \
	unlockResult = sceKernelUnlockMutex(mutex, 1); \
	if (unlockResult != 0) { \
		sceAppMgrDump(); \
		dumpBgmState(); \
		SCE_DBG_ASSERT_PRINTF(0, _line, \
			"[SceAppMgr] The actual value is %d(0x%08x), but expected value is %d(0x%08x)\n", \
			unlockResult, unlockResult, 0, 0); \
	} \
} while (0)
#else
#define APP_EVENT_LOCK(_line) do { \
	if (sceKernelLockMutex(g_appmgrMainMutex, 1, NULL) != 0) \
		appmgrAppEventMutexPanic(); \
	g_appmgrMainMutexOwner = sceKernelGetThreadId(); \
} while (0)

#define APP_EVENT_UNLOCK(_line) do { \
	SceUID mutex = g_appmgrMainMutex; \
	g_appmgrMainMutexOwner = -1; \
	if (sceKernelUnlockMutex(mutex, 1) != 0) \
		appmgrAppEventMutexPanic(); \
} while (0)
#endif

extern int sceAppMgrSendSystemEvent2(const char *appName, const char *titleId,
				  int event);

void appmgrClearPendingAppEvents(struct AppState *app)
{
	app->pendingActivate = 0;
	app->pendingDeactivate = 0;
	app->pendingResume = 0;
	app->pendingQuit = 0;
	app->pendingLaunchParam = 0;
	app->pendingExtendedEvent = 0;
	app->pendingOrientationChanged = 0;
}

int _appListPushEvent(struct AppState *app, const SceAppMgrSystemEvent *event)
{
	if (!app) {
#if FW_DEBUG
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_FAIL(115);
#else
		appmgrAppEventArgumentPanic();
#endif
	}
	switch (event->systemEvent) {
	case 0x10000001:
		if (app->pendingDeactivate)
			app->pendingDeactivate = 0;
		else
			app->pendingActivate = 1;
		break;
	case 0x10000002:
		if (app->pendingActivate)
			app->pendingActivate = 0;
		else
			app->pendingDeactivate = 1;
		break;
	case 0x10000003:
		app->pendingResume = 1;
		break;
	case 0x20000001:
		app->pendingQuit = 1;
		break;
	case 0x20000004:
		app->pendingLaunchParam = 1;
		break;
	case 0x10000004:
		app->pendingStorePurchase = 1;
		break;
	case 0x10000006:
		app->pendingStoreRedemption = 1;
		break;
	case 0x10000005:
		app->pendingNpMessage = 1;
		break;
	case 0x10000101:
		app->pendingExtendedEvent = 1;
		break;
	case 0x10000102:
		app->pendingExtendedEvent = 2;
		break;
	case 0x20000010:
		app->pendingOrientationChanged = 1;
		break;
	default:
		return (int)0x8080201a;
	}
	return 0;
}

int sceAppMgrReceiveSystemEvent(SceAppMgrSystemEvent *event)
{
	SceUInt32 syscall = sceSyscallEnter();
	struct AppState *app;
	SceUInt32 clear;
	int received = 1;
	int result;

	if (event == NULL) {
		result = (int)0x80802016;
		goto leave;
	}
	APP_EVENT_LOCK(194);
	app = appmgrFindCurrentApp(g_appmgrAppStates);
	if (app == NULL) {
		APP_EVENT_UNLOCK(198);
		result = (int)0x80802012;
		goto leave;
	}
	if (app->pendingActivate && SceSblACMgrForDriver_6C5AB07F(0) &&
	    (app->processAttr & 2)) {
		memset(event, 0, sizeof(*event));
		event->systemEvent = 0x10000001;
		app->pendingActivate = 0;
	} else if (app->pendingDeactivate && SceSblACMgrForDriver_6C5AB07F(0) &&
		   (app->processAttr & 2)) {
		memset(event, 0, sizeof(*event));
		event->systemEvent = 0x10000002;
		app->pendingDeactivate = 0;
	} else if (app->pendingQuit && SceSblACMgrForDriver_6C5AB07F(0)) {
		memset(event, 0, sizeof(*event));
		event->systemEvent = 0x20000001;
		app->pendingQuit = 0;
	} else if (app->pendingResume) {
		memset(event, 0, sizeof(*event));
		event->systemEvent = 0x10000003;
		app->pendingResume = 0;
	} else if (app->pendingStorePurchase) {
		clear = app->pendingResume;
		memset(event, clear, sizeof(*event));
		event->systemEvent = 0x10000004;
		app->pendingStorePurchase = clear;
	} else if (app->pendingStoreRedemption) {
		clear = app->pendingStorePurchase;
		memset(event, clear, sizeof(*event));
		event->systemEvent = 0x10000006;
		app->pendingStoreRedemption = clear;
	} else if (app->pendingNpMessage) {
		clear = app->pendingStoreRedemption;
		memset(event, clear, sizeof(*event));
		event->systemEvent = 0x10000005;
		app->pendingNpMessage = clear;
	} else {
		received = 0;
	}
	APP_EVENT_UNLOCK(244);
	result = received ? 0 : (int)0x80802013;
leave:
	sceSyscallLeave(syscall);
	return result;
}

int sceAppMgrReceiveEventNum(int *eventNum)
{
	SceUInt32 syscall = sceSyscallEnter();
	struct AppState *app;
	int count;

	if (!sceSblACMgrIsSystemProgram(0)) {
		sceSyscallLeave(syscall);
		return (int)0x8080201f;
	}
	if (!eventNum) {
		sceSyscallLeave(syscall);
		return (int)0x80802016;
	}
	APP_EVENT_LOCK(268);
	app = appmgrFindCurrentApp(g_appmgrAppStates);
	if (!app) {
		APP_EVENT_UNLOCK(272);
		sceSyscallLeave(syscall);
		return (int)0x80802012;
	}
	count = app->pendingLaunchParam != 0;
	if (app->pendingQuit)
		++count;
	if (app->pendingResume)
		++count;
	if (app->pendingDeactivate)
		++count;
	if (app->pendingActivate)
		++count;
	if (app->pendingExtendedEvent)
		++count;
	if (app->pendingOrientationChanged)
		++count;
	*eventNum = count;
	APP_EVENT_UNLOCK(280);
	sceSyscallLeave(syscall);
	return 0;
}

static inline void appmgrSetAppEvent(SceAppMgrEvent *event, SceUInt32 type)
{
	event->event = type;
	event->appId = -1;
	memset(event->param, 0, sizeof(event->param));
}

int sceAppMgrReceiveEventInternal(void *appEvent)
{
	SceUInt32 syscall = sceSyscallEnter();
	SceAppMgrEvent *event = appEvent;
	struct AppState *app;
	int result = 0;

	if (!sceSblACMgrIsSystemProgram(0)) {
		sceSyscallLeave(syscall);
		return (int)0x8080201f;
	}
	if (!event) {
		sceSyscallLeave(syscall);
		return (int)0x80802016;
	}
	APP_EVENT_LOCK(305);
	app = appmgrFindCurrentApp(g_appmgrAppStates);
	if (!app) {
		APP_EVENT_UNLOCK(309);
		sceSyscallLeave(syscall);
		return (int)0x80802012;
	}
	if (app->pendingActivate) {
		appmgrSetAppEvent(event, 0x10000001);
		app->pendingActivate = 0;
	} else if (app->pendingDeactivate) {
		appmgrSetAppEvent(event, 0x10000002);
		app->pendingDeactivate = 0;
	} else if (app->pendingResume) {
		appmgrSetAppEvent(event, 0x10000003);
		app->pendingResume = 0;
	} else if (app->pendingQuit) {
		appmgrSetAppEvent(event, 0x20000001);
		app->pendingQuit = 0;
	} else if (app->pendingLaunchParam) {
		appmgrSetAppEvent(event, 0x20000004);
		app->pendingLaunchParam = 0;
	} else if (app->pendingExtendedEvent == 1) {
		appmgrSetAppEvent(event, 0x10000101);
		app->pendingExtendedEvent = 0;
	} else if (app->pendingExtendedEvent == 2) {
		appmgrSetAppEvent(event, 0x10000102);
		app->pendingExtendedEvent = 0;
	} else if (app->pendingOrientationChanged) {
		appmgrSetAppEvent(event, 0x20000010);
		app->pendingOrientationChanged = 0;
	} else {
		event->event = 0;
		result = (int)0x80802020;
	}
	APP_EVENT_UNLOCK(317);
	sceSyscallLeave(syscall);
	return result;
}

#if FW_DEBUG
#define APP_EVENT_CHECK_RESULT(_result, _line) do { \
	if ((_result) != 0) { \
		sceAppMgrDump(); \
		dumpBgmState(); \
		SCE_DBG_ASSERT_PRINTF(0, _line, \
			"[SceAppMgr] The actual value is %d(0x%08x), but expected value is %d(0x%08x)\n", \
			(_result), (_result), 0, 0); \
	} \
} while (0)
#else
#define APP_EVENT_CHECK_RESULT(_result, _line) do { \
	if ((_result) != 0) \
		appmgrAppEventArgumentPanic(); \
} while (0)
#endif

int sceAppMgrSendEventInternal(const SceAppMgrSystemEvent *event, SceUID pid)
{
	SceUInt32 syscall = sceSyscallEnter();
	struct AppState *app;
	int result;

	if (!sceSblACMgrIsSystemProgram(0) &&
	    sceKernelGetProcessId() != 0x10005) {
		result = (int)0x8080201f;
		goto leave;
	}
	if (event == NULL) {
		result = (int)0x80802016;
		goto leave;
	}
	APP_EVENT_LOCK(351);
	app = appmgrFindAppByProcessId(g_appmgrAppStates, pid);
	if (app == NULL) {
		APP_EVENT_UNLOCK(356);
		result = (int)0x80802012;
		goto leave;
	}
	if (event->systemEvent == 0x20000001) {
#if FW_DEBUG
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_FAIL(364);
#else
		appmgrAppEventArgumentPanic();
#endif
		APP_EVENT_UNLOCK(366);
		result = 0;
	} else if (event->systemEvent == 0x10000001) {
		SCE_DBG_PRINTF(0, 373,
			"[SceAppMgr] Warning: Activate by event is deprecated!!!!!\n");
		APP_EVENT_UNLOCK(374);
		result = 0;
	} else if (event->systemEvent == 0x10000002) {
		SCE_DBG_PRINTF(0, 381,
			"[SceAppMgr] Warning: Deactivate by event is deprecated!!!!!\n");
		APP_EVENT_UNLOCK(382);
		result = 0;
	} else {
		result = _appListPushEvent(app, event);
		APP_EVENT_UNLOCK(389);
	}
leave:
	sceSyscallLeave(syscall);
	return result;
}

int sceAppMgrSendSystemEvent2(const char *appName, const char *titleId, int event)
{
	SceUInt32 syscall = sceSyscallEnter();
	SceAppMgrSystemEvent systemEvent;
	char communicationId[16];
	struct AppState *app;
	int index;
	int result;

	if (!sceSblACMgrIsSystemProgram(0)) {
		sceSyscallLeave(syscall);
		return (int)0x8080201f;
	}
	memset(&systemEvent, 0, sizeof(systemEvent));
	systemEvent.systemEvent = event;
	APP_EVENT_LOCK(416);
	index = appmgrGetShellApp() != NULL;
	app = &g_appmgrAppStates[index];
	if (!appName) {
		do {
			SceUID processId = app->process.processId;
#if FW_DEBUG
			__builtin_prefetch(&app[3].process.processId);
#else
			__builtin_prefetch(&app[4].process.processId);
#endif
			if (processId >= 0 && (processId & 1)) {
				result = _appListPushEvent(app, &systemEvent);
				APP_EVENT_CHECK_RESULT(result, 424);
			}
			++index;
			++app;
		} while (index < 20);
		goto delivered;
	}
	do {
		SceUID processId = app->process.processId;
		__builtin_prefetch(&app[8].process.processId);
		if (processId >= 0 && (processId & 1) &&
		    strncmp(app->process.appName, appName, 32) == 0) {
			result = _appListPushEvent(app, &systemEvent);
			APP_EVENT_CHECK_RESULT(result, 435);
			goto delivered;
		}
		++index;
		++app;
	} while (index < 20);
	if (titleId) {
		index = appmgrGetShellApp() != NULL;
		app = &g_appmgrAppStates[index];
		do {
			SceUID processId = app->process.processId;
#if FW_DEBUG
			__builtin_prefetch(&app[3].process.processId);
#else
			__builtin_prefetch(&app[5].process.processId);
#endif
			if (processId >= 0 && (processId & 1)) {
				result = sceAppMgrAppParamGetString(processId, 7,
						communicationId, 13);
				APP_EVENT_CHECK_RESULT(result, 453);
				if (strncmp(titleId, communicationId, 13) == 0) {
					result = _appListPushEvent(app, &systemEvent);
					APP_EVENT_CHECK_RESULT(result, 456);
					goto delivered;
				}
			}
			++index;
			++app;
		} while (index < 20);
	}
	APP_EVENT_UNLOCK(465);
	sceSyscallLeave(syscall);
	return (int)0x80802012;

delivered:
	APP_EVENT_UNLOCK(471);
	sceSyscallLeave(syscall);
	return 0;
}

int sceAppMgrSendSystemEvent(const char *appName, int event)
{
	return sceAppMgrSendSystemEvent2(appName, NULL, event);
}
