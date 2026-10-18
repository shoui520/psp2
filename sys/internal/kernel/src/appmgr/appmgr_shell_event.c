/* Events delivered to the shell. */

#include <sysmem/debug.h>
#include <intrmgr/syscall.h>
#include <acmgr/acmgr.h>
#include <threadmgr/threadmgr.h>
#include <string.h>
#include "app_state_internal.h"
#include "appmgr_shell_event.h"
#include <appmgr/appmgr.h>

extern SceUID g_appmgrMainMutex;
extern SceUID g_appmgrMainMutexOwner;
extern SceUID g_appmgrDebugShellEventSemaphore;
extern int dumpBgmState(void);
extern SceInt64 appmgrUpdateShellWatchDogActivity(void);
extern int appmgrCoalesceShellEvent(const AppmgrShellEvent *event);
extern int appmgrHasLoadExecShellEvents(int appId);
extern int _deleteDuplicatedShellEventQueueByLoadExec(int appId);

AppmgrShellEventQueue g_appmgrShellEventQueue;

#if !FW_DEBUG
static const SceKernelDebugInfo shellEventMutexPanicInfo = {
	.fileHash = 0xc9656e8e,
	.lineHash = 0x79d3a1f0,
	.funcHash = 0x9d536bce,
};

static const SceKernelDebugInfo shellEventOwnerPanicInfo = {
	.fileHash = 0xc9656e8e,
	.lineHash = 0xc879dadc,
	.funcHash = 0x11a05ee1,
};

_Noreturn void appmgrShellEventOwnerPanic(void)
{
	SceDebugForDriver_391B5B74(&shellEventOwnerPanicInfo, __builtin_return_address(0));
}

_Noreturn void appmgrShellEventMutexPanic(void)
{
	SceDebugForDriver_391B5B74(&shellEventMutexPanicInfo, __builtin_return_address(0));
}
#endif

int appmgrInitializeShellEventQueue(void)
{
	g_appmgrShellEventQueue.count = 0;
	g_appmgrShellEventQueue.writeIndex = 0;
	memset(g_appmgrShellEventQueue.events, 0,
	       sizeof(g_appmgrShellEventQueue.events));
	return 0;
}

int appmgrRemoveShellEventAtIndex(int index)
{
	int count = g_appmgrShellEventQueue.count;
	int writeIndex;
	int oldest;
	int position;

	if (count == 0)
		return (int)0x80802015;
	if (count == 1) {
		g_appmgrShellEventQueue.count = 0;
		return 0;
	}
	writeIndex = g_appmgrShellEventQueue.writeIndex;
	if (count <= writeIndex) {
		oldest = writeIndex - count;
		for (position = writeIndex - 2; position >= oldest; --position) {
			if (position + 1 <= index)
				g_appmgrShellEventQueue.events[position + 1] =
					g_appmgrShellEventQueue.events[position];
		}
	} else {
		oldest = writeIndex + 180 - count;
		if (index > oldest) {
			for (position = 178; position >= oldest; --position) {
				if (index >= position + 1)
					g_appmgrShellEventQueue.events[position + 1] =
						g_appmgrShellEventQueue.events[position];
			}
		} else if (index < oldest) {
			for (position = writeIndex - 2; position >= 0; --position) {
				if (index >= position + 1)
					g_appmgrShellEventQueue.events[position + 1] =
						g_appmgrShellEventQueue.events[position];
			}
			g_appmgrShellEventQueue.events[0] =
				g_appmgrShellEventQueue.events[179];
			for (position = 178; position >= oldest; --position)
				g_appmgrShellEventQueue.events[position + 1] =
					g_appmgrShellEventQueue.events[position];
		}
	}
	--g_appmgrShellEventQueue.count;
	return 0;
}

static void _shellEventQueueRemoveEventAll(int eventType)
{
	SceUID owner = g_appmgrMainMutexOwner;

	if (owner != sceKernelGetThreadId()) {
#if FW_DEBUG
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_FAIL(215);
#else
		appmgrShellEventOwnerPanic();
#endif
	}
	for (;;) {
		int writeIndex = g_appmgrShellEventQueue.writeIndex;
		int oldest = writeIndex - g_appmgrShellEventQueue.count;
		int found = -1;
		int index;
		int result;

		for (index = writeIndex - 1; index >= oldest; --index) {
			int position = index;

			if (position < 0)
				position += 180;
			if (g_appmgrShellEventQueue.events[position].event == eventType) {
				found = position;
				break;
			}
		}
		if (found < 0) {
			if (found != -1) {
#if FW_DEBUG
				sceAppMgrDump();
				dumpBgmState();
				SCE_DBG_ASSERT_PRINTF(0, 229,
					"[SceAppMgr] The actual value is %d(0x%08x), but expected value is %d(0x%08x)\n",
					found, found, -1, -1);
#else
				appmgrShellEventOwnerPanic();
#endif
			}
			return;
		}
		result = appmgrRemoveShellEventAtIndex(found);
		if (result != 0) {
#if FW_DEBUG
			sceAppMgrDump();
			dumpBgmState();
			SCE_DBG_ASSERT_PRINTF(0, 225,
				"[SceAppMgr] The actual value is %d(0x%08x), but expected value is %d(0x%08x)\n",
				result, result, 0, 0);
#else
			appmgrShellEventOwnerPanic();
#endif
		}
	}
}

int appmgrHasLoadExecShellEvents(int appId)
{
	int writeIndex = g_appmgrShellEventQueue.writeIndex;
	int oldest = writeIndex - g_appmgrShellEventQueue.count;
	int state = 0;
	int position;

	for (position = writeIndex - 1; position >= oldest; --position) {
		int index = position < 0 ? position + 180 : position;
		const AppmgrShellEvent *event = &g_appmgrShellEventQueue.events[index];

		if (event->event == 3) {
			if (event->data.lifecycle.appId == appId &&
			    event->data.lifecycle.relaunchPending == 1 && state == 0)
				state = 1;
		} else if (event->event == 2) {
			if (event->data.loaded.appId == appId && state == 1)
				state = 2;
		} else if (event->event == 4) {
			if (event->data.lifecycle.appId == appId &&
			    event->data.lifecycle.relaunchPending == 1 && state == 2)
				state = 3;
		}
	}
	return state == 3;
}

int _deleteDuplicatedShellEventQueueByLoadExec(int appId)
{
	int writeIndex = g_appmgrShellEventQueue.writeIndex;
	int oldest = writeIndex - g_appmgrShellEventQueue.count;
	int found = -1;
	int position;

	for (position = writeIndex - 1; position >= oldest; --position) {
		int index = position < 0 ? position + 180 : position;
		const AppmgrShellEvent *event = &g_appmgrShellEventQueue.events[index];

		if (event->event == 3 && event->data.lifecycle.appId == appId &&
		    event->data.lifecycle.relaunchPending == 1) {
			found = index;
			break;
		}
	}
	if (found < 0) {
#if FW_DEBUG
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_FAIL(237);
#else
		appmgrShellEventOwnerPanic();
#endif
	}
	appmgrRemoveShellEventAtIndex(found);
	writeIndex = g_appmgrShellEventQueue.writeIndex;
	oldest = writeIndex - g_appmgrShellEventQueue.count;
	found = -1;
	for (position = writeIndex - 1; position >= oldest; --position) {
		int index = position < 0 ? position + 180 : position;
		const AppmgrShellEvent *event = &g_appmgrShellEventQueue.events[index];

		if (event->event == 2 && event->data.loaded.appId == appId) {
			found = index;
			break;
		}
	}
	if (found < 0) {
#if FW_DEBUG
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_FAIL(241);
#else
		appmgrShellEventOwnerPanic();
#endif
	}
	return appmgrRemoveShellEventAtIndex(found);
}

int appmgrCoalesceShellEvent(const AppmgrShellEvent *event)
{
	int type = event->event;
	int count;
	int start;
	int end;
	int position;

	if ((type & ~4) != 8 && type != 13 && type != 17 &&
	    type != 18 && type != 19)
		return 0;
	count = g_appmgrShellEventQueue.count;
	start = g_appmgrShellEventQueue.writeIndex;
	if (start < count)
		start += 180;
	start -= count;
	end = start + count;
	for (position = start; position < end; ++position) {
		AppmgrShellEvent *queued =
			&g_appmgrShellEventQueue.events[position % 180];

		if (queued->event != type)
			continue;
		if ((type & ~4) == 8) {
			*queued = *event;
			return 1;
		}
		if (type == 13) {
			if (queued->data.liveareaMode.processId !=
			    event->data.liveareaMode.processId ||
			    queued->data.liveareaMode.appId != event->data.liveareaMode.appId)
				continue;
			queued->data.liveareaMode.mode = event->data.liveareaMode.mode;
		}
		return 1;
	}
	return 0;
}

int _shellEventQueuePush(const AppmgrShellEvent *event)
{
	SceUID owner = g_appmgrMainMutexOwner;

	if (owner != sceKernelGetThreadId()) {
#if FW_DEBUG
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_FAIL(28);
#else
		appmgrShellEventOwnerPanic();
#endif
	}
	if (!appmgrGetShellApp() || appmgrCoalesceShellEvent(event) == 1)
		return 0;
	if (event->event == 4 && event->data.lifecycle.relaunchPending == 1 &&
	    appmgrHasLoadExecShellEvents(event->data.lifecycle.appId) == 1) {
		_deleteDuplicatedShellEventQueueByLoadExec(event->data.lifecycle.appId);
		return 0;
	}
	if (g_appmgrShellEventQueue.count == 180)
		return (int)0x80802014;
	g_appmgrShellEventQueue.events[g_appmgrShellEventQueue.writeIndex] = *event;
	++g_appmgrShellEventQueue.count;
	g_appmgrShellEventQueue.writeIndex =
		(g_appmgrShellEventQueue.writeIndex + 1) % 180;
	return 0;
}

void _shellEventOnAppLoadedQueuePush(SceUID processId, int appId)
{
	AppmgrShellEvent event;
	SceUID mutex;
	int result;

	if (!appmgrGetShellApp())
		return;
	memset(&event, 0, sizeof(event));
	event.data.loaded.processId = processId;
	event.data.loaded.appId = appId;
	event.event = 2;
	result = sceKernelLockMutex(g_appmgrMainMutex, 1, NULL);
	if (result != 0) {
#if FW_DEBUG
		SCE_DBG_PRINTF(0, 356, "Failed to lock ret=0x%08x\n", result);
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_FAIL(356);
#else
		appmgrShellEventMutexPanic();
#endif
	} else {
		g_appmgrMainMutexOwner = sceKernelGetThreadId();
	}
	result = _shellEventQueuePush(&event);
	if (result != 0) {
#if FW_DEBUG
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_PRINTF(0, 358,
			"[SceAppMgr] The actual value is %d(0x%08x), but expected value is %d(0x%08x)\n",
			result, result, 0, 0);
#else
		appmgrShellEventOwnerPanic();
#endif
	}
	mutex = g_appmgrMainMutex;
	g_appmgrMainMutexOwner = -1;
	result = sceKernelUnlockMutex(mutex, 1);
	if (result != 0) {
#if FW_DEBUG
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_PRINTF(0, 359,
			"[SceAppMgr] The actual value is %d(0x%08x), but expected value is %d(0x%08x)\n",
			result, result, 0, 0);
#else
		appmgrShellEventMutexPanic();
#endif
	}
}

void _shellEventOnAppUnloadingQueuePush(SceUID processId, int appId,
		int exitReason, int selfQuit, SceUID sourcePid, int sourceAppId,
		int relaunchPending, const char *appName)
{
	AppmgrShellEvent event;
	int result;

	if (!appmgrGetShellApp())
		return;
	memset(&event, 0, sizeof(event));
	event.data.lifecycle.processId = processId;
	event.data.lifecycle.appId = appId;
	memcpy(event.data.lifecycle.appName, appName,
	       sizeof(event.data.lifecycle.appName));
	event.data.lifecycle.exitReason = exitReason;
	event.data.lifecycle.sourcePid = sourcePid;
	event.data.lifecycle.selfQuit = selfQuit;
	event.data.lifecycle.sourceAppId = sourceAppId;
	event.data.lifecycle.relaunchPending = relaunchPending;
	event.event = 3;
	result = _shellEventQueuePush(&event);
	if (result != 0) {
#if FW_DEBUG
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_PRINTF(0, 382,
			"[SceAppMgr] The actual value is %d(0x%08x), but expected value is %d(0x%08x)\n",
			result, result, 0, 0);
#else
		appmgrShellEventOwnerPanic();
#endif
	}
}

void _shellEventOnAppUnloadedQueuePush(SceUID processId, int appId,
		int exitReason, SceUID sourcePid, int sourceAppId,
		int relaunchPending, const char *appName)
{
	AppmgrShellEvent event;
	int result;

	if (!appmgrGetShellApp())
		return;
	memset(&event, 0, sizeof(event));
	event.data.lifecycle.processId = processId;
	event.data.lifecycle.appId = appId;
	memcpy(event.data.lifecycle.appName, appName,
	       sizeof(event.data.lifecycle.appName));
	event.data.lifecycle.exitReason = exitReason;
	event.data.lifecycle.sourcePid = sourcePid;
	event.data.lifecycle.sourceAppId = sourceAppId;
	event.data.lifecycle.relaunchPending = relaunchPending;
	event.event = 4;
	result = _shellEventQueuePush(&event);
	if (result != 0) {
#if FW_DEBUG
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_PRINTF(0, 404,
			"[SceAppMgr] The actual value is %d(0x%08x), but expected value is %d(0x%08x)\n",
			result, result, 0, 0);
#else
		appmgrShellEventOwnerPanic();
#endif
	}
}

void _shellEventOnAppLaunchByNameQueuePush(int appId, int secondAppId)
{
	AppmgrShellEvent event;
	int result;

	if (!appmgrGetShellApp())
		return;
	memset(&event, 0, sizeof(event));
	event.data.launchByName.appIds[0] = appId;
	event.data.launchByName.appIds[1] = secondAppId;
	event.event = 6;
	result = _shellEventQueuePush(&event);
	if (result != 0) {
#if FW_DEBUG
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_PRINTF(0, 421,
			"[SceAppMgr] The actual value is %d(0x%08x), but expected value is %d(0x%08x)\n",
			result, result, 0, 0);
#else
		appmgrShellEventOwnerPanic();
#endif
	}
}

void _shellEventDestroyOtherAppQueuePush(SceUID processId, int appId)
{
	AppmgrShellEvent event;
	int result;

	if (!appmgrGetShellApp())
		return;
	memset(&event, 0, sizeof(event));
	event.data.loaded.processId = processId;
	event.data.loaded.appId = appId;
	event.event = 8;
	result = _shellEventQueuePush(&event);
	if (result != 0) {
#if FW_DEBUG
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_PRINTF(0, 438,
			"[SceAppMgr] The actual value is %d(0x%08x), but expected value is %d(0x%08x)\n",
			result, result, 0, 0);
#else
		appmgrShellEventOwnerPanic();
#endif
	}
}

static const SceKernelDebugInfo debuggerShellEventCountPanicInfo = {
	.fileHash = 0x38fc9b3e,
	.lineHash = 0xe4bf4f91,
	.funcHash = 0x7c920bfa,
#if FW_DEBUG
	.file = "appmgr_core_internal.h",
	.line = 176,
	.func = "signalSemaForDebuggerShellEventCount",
#endif
};

static inline void signalSemaForDebuggerShellEventCount(void)
{
	if (sceKernelSignalSema(g_appmgrDebugShellEventSemaphore, 1) == 0)
		return;
#if FW_DEBUG
	sceAppMgrDump();
	dumpBgmState();
	if (SceDebugForDriver_374B7868() > 0)
		SceDebugForDriver_FD753E7A(1, 0, &debuggerShellEventCountPanicInfo,
				     __builtin_return_address(0));
#else
	SceDebugForDriver_391B5B74(&debuggerShellEventCountPanicInfo,
		       __builtin_return_address(0));
#endif
}

void _shellEventStartLaunchAppFromDebuggerQueuePush(SceUID processId,
		int liveareaType, int appId)
{
	AppmgrShellEvent event;
	SceUID mutex;
	int result;

	if (!appmgrGetShellApp()) {
		signalSemaForDebuggerShellEventCount();
		return;
	}
	memset(&event, 0, sizeof(event));
	event.data.debuggerStart.processId = processId;
	event.data.debuggerStart.liveareaType = liveareaType;
	event.data.debuggerStart.appId = appId;
	event.event = 9;
	result = sceKernelLockMutex(g_appmgrMainMutex, 1, NULL);
	if (result != 0) {
#if FW_DEBUG
		SCE_DBG_PRINTF(0, 455, "Failed to lock ret=0x%08x\n", result);
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_FAIL(455);
#else
		appmgrShellEventMutexPanic();
#endif
	} else {
		g_appmgrMainMutexOwner = sceKernelGetThreadId();
	}
	result = _shellEventQueuePush(&event);
	if (result != 0) {
#if FW_DEBUG
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_PRINTF(0, 457,
			"[SceAppMgr] The actual value is %d(0x%08x), but expected value is %d(0x%08x)\n",
			result, result, 0, 0);
#else
		appmgrShellEventOwnerPanic();
#endif
	}
	mutex = g_appmgrMainMutex;
	g_appmgrMainMutexOwner = -1;
	result = sceKernelUnlockMutex(mutex, 1);
	if (result != 0) {
#if FW_DEBUG
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_PRINTF(0, 458,
			"[SceAppMgr] The actual value is %d(0x%08x), but expected value is %d(0x%08x)\n",
			result, result, 0, 0);
#else
		appmgrShellEventMutexPanic();
#endif
	}
}

void _shellEventResultLaunchAppFromDebuggerQueuePush(SceUID processId,
		int appId, int launchResult)
{
	AppmgrShellEvent event;
	SceUID mutex;
	int result;

	if (!appmgrGetShellApp())
		return;
	memset(&event, 0, sizeof(event));
	event.data.debuggerResult.processId = processId;
	event.data.debuggerResult.result = launchResult;
	event.data.debuggerResult.appId = appId;
	event.event = 10;
	result = sceKernelLockMutex(g_appmgrMainMutex, 1, NULL);
	if (result != 0) {
#if FW_DEBUG
		SCE_DBG_PRINTF(0, 477, "Failed to lock ret=0x%08x\n", result);
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_FAIL(477);
#else
		appmgrShellEventMutexPanic();
#endif
	} else {
		g_appmgrMainMutexOwner = sceKernelGetThreadId();
	}
	result = _shellEventQueuePush(&event);
	if (result != 0) {
#if FW_DEBUG
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_PRINTF(0, 479,
			"[SceAppMgr] The actual value is %d(0x%08x), but expected value is %d(0x%08x)\n",
			result, result, 0, 0);
#else
		appmgrShellEventOwnerPanic();
#endif
	}
	mutex = g_appmgrMainMutex;
	g_appmgrMainMutexOwner = -1;
	result = sceKernelUnlockMutex(mutex, 1);
	if (result != 0) {
#if FW_DEBUG
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_PRINTF(0, 480,
			"[SceAppMgr] The actual value is %d(0x%08x), but expected value is %d(0x%08x)\n",
			result, result, 0, 0);
#else
		appmgrShellEventMutexPanic();
#endif
	}
}

void _shellEventCoredumpStartQueuePush(SceUID processId, int appId,
		int progressNotification)
{
	AppmgrShellEvent event;
	int result;

	if (!appmgrGetShellApp())
		return;
	memset(&event, 0, sizeof(event));
	event.data.coredump.processId = processId;
	event.data.coredump.appId = appId;
	event.data.coredump.progressNotification = progressNotification;
	event.event = 11;
	result = _shellEventQueuePush(&event);
	if (result != 0) {
#if FW_DEBUG
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_PRINTF(0, 498,
			"[SceAppMgr] The actual value is %d(0x%08x), but expected value is %d(0x%08x)\n",
			result, result, 0, 0);
#else
		appmgrShellEventOwnerPanic();
#endif
	}
}

void _shellEventVoiceChatAppChangedQueuePush(int oldAppId, int newAppId,
		SceUID oldProcessId, SceUID newProcessId)
{
	AppmgrShellEvent event;
	SceUID mutex;
	int result;

	if (!appmgrGetShellApp())
		return;
	memset(&event, 0, sizeof(event));
	event.data.voiceChat.oldAppId = oldAppId;
	event.data.voiceChat.newAppId = newAppId;
	event.data.voiceChat.oldProcessId = oldProcessId;
	event.data.voiceChat.newProcessId = newProcessId;
	event.event = 12;
	result = sceKernelLockMutex(g_appmgrMainMutex, 1, NULL);
	if (result != 0) {
#if FW_DEBUG
		SCE_DBG_PRINTF(0, 516, "Failed to lock ret=0x%08x\n", result);
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_FAIL(516);
#else
		appmgrShellEventMutexPanic();
#endif
	} else {
		g_appmgrMainMutexOwner = sceKernelGetThreadId();
	}
	result = _shellEventQueuePush(&event);
	if (result != 0) {
#if FW_DEBUG
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_PRINTF(0, 518,
			"[SceAppMgr] The actual value is %d(0x%08x), but expected value is %d(0x%08x)\n",
			result, result, 0, 0);
#else
		appmgrShellEventOwnerPanic();
#endif
	}
	mutex = g_appmgrMainMutex;
	g_appmgrMainMutexOwner = -1;
	result = sceKernelUnlockMutex(mutex, 1);
	if (result != 0) {
#if FW_DEBUG
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_PRINTF(0, 519,
			"[SceAppMgr] The actual value is %d(0x%08x), but expected value is %d(0x%08x)\n",
			result, result, 0, 0);
#else
		appmgrShellEventMutexPanic();
#endif
	}
}

void _shellEventLiveareaModeChangedQueuePush(SceUID processId, int appId,
		int mode)
{
	AppmgrShellEvent event;
	SceUID mutex;
	int result;

	if (!appmgrGetShellApp())
		return;
	memset(&event, 0, sizeof(event));
	event.data.liveareaMode.processId = processId;
	event.data.liveareaMode.appId = appId;
	event.data.liveareaMode.mode = mode;
	event.event = 13;
	result = sceKernelLockMutex(g_appmgrMainMutex, 1, NULL);
	if (result != 0) {
#if FW_DEBUG
		SCE_DBG_PRINTF(0, 536, "Failed to lock ret=0x%08x\n", result);
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_FAIL(536);
#else
		appmgrShellEventMutexPanic();
#endif
	} else {
		g_appmgrMainMutexOwner = sceKernelGetThreadId();
	}
	result = _shellEventQueuePush(&event);
	if (result != 0) {
#if FW_DEBUG
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_PRINTF(0, 538,
			"[SceAppMgr] The actual value is %d(0x%08x), but expected value is %d(0x%08x)\n",
			result, result, 0, 0);
#else
		appmgrShellEventOwnerPanic();
#endif
	}
	mutex = g_appmgrMainMutex;
	g_appmgrMainMutexOwner = -1;
	result = sceKernelUnlockMutex(mutex, 1);
	if (result != 0) {
#if FW_DEBUG
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_PRINTF(0, 539,
			"[SceAppMgr] The actual value is %d(0x%08x), but expected value is %d(0x%08x)\n",
			result, result, 0, 0);
#else
		appmgrShellEventMutexPanic();
#endif
	}
}

void _shellEventFatalErrorQueuePush(int appId, int error)
{
	AppmgrShellEvent event;
	int result;

	if (!appmgrGetShellApp())
		return;
	memset(&event, 0, sizeof(event));
	event.data.fatalError.appId = appId;
	event.data.fatalError.error = error;
	event.event = 14;
	result = _shellEventQueuePush(&event);
	if (result != 0) {
#if FW_DEBUG
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_PRINTF(0, 556,
			"[SceAppMgr] The actual value is %d(0x%08x), but expected value is %d(0x%08x)\n",
			result, result, 0, 0);
#else
		appmgrShellEventOwnerPanic();
#endif
	}
}

void _shellEventActivateAppRequestQueuePush(int appId)
{
	AppmgrShellEvent event;
	SceUID mutex;
	int result;

	if (!appmgrGetShellApp())
		return;
	memset(&event, 0, sizeof(event));
	event.event = 15;
	event.data.appRequest.appId = appId;
	result = sceKernelLockMutex(g_appmgrMainMutex, 1, NULL);
	if (result != 0) {
#if FW_DEBUG
		SCE_DBG_PRINTF(0, 571, "Failed to lock ret=0x%08x\n", result);
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_FAIL(571);
#else
		appmgrShellEventMutexPanic();
#endif
	} else {
		g_appmgrMainMutexOwner = sceKernelGetThreadId();
	}
	_shellEventQueueRemoveEventAll(15);
	_shellEventQueueRemoveEventAll(16);
	result = _shellEventQueuePush(&event);
	if (result != 0) {
#if FW_DEBUG
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_PRINTF(0, 576,
			"[SceAppMgr] The actual value is %d(0x%08x), but expected value is %d(0x%08x)\n",
			result, result, 0, 0);
#else
		appmgrShellEventOwnerPanic();
#endif
	}
	mutex = g_appmgrMainMutex;
	g_appmgrMainMutexOwner = -1;
	result = sceKernelUnlockMutex(mutex, 1);
	if (result != 0) {
#if FW_DEBUG
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_PRINTF(0, 577,
			"[SceAppMgr] The actual value is %d(0x%08x), but expected value is %d(0x%08x)\n",
			result, result, 0, 0);
#else
		appmgrShellEventMutexPanic();
#endif
	}
}

void _shellEventDeactivateAppRequestQueuePush(int appId)
{
	AppmgrShellEvent event;
	SceUID mutex;
	int result;

	if (!appmgrGetShellApp())
		return;
	memset(&event, 0, sizeof(event));
	event.event = 16;
	event.data.appRequest.appId = appId;
	result = sceKernelLockMutex(g_appmgrMainMutex, 1, NULL);
	if (result != 0) {
#if FW_DEBUG
		SCE_DBG_PRINTF(0, 593, "Failed to lock ret=0x%08x\n", result);
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_FAIL(593);
#else
		appmgrShellEventMutexPanic();
#endif
	} else {
		g_appmgrMainMutexOwner = sceKernelGetThreadId();
	}
	_shellEventQueueRemoveEventAll(15);
	_shellEventQueueRemoveEventAll(16);
	result = _shellEventQueuePush(&event);
	if (result != 0) {
#if FW_DEBUG
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_PRINTF(0, 598,
			"[SceAppMgr] The actual value is %d(0x%08x), but expected value is %d(0x%08x)\n",
			result, result, 0, 0);
#else
		appmgrShellEventOwnerPanic();
#endif
	}
	mutex = g_appmgrMainMutex;
	g_appmgrMainMutexOwner = -1;
	result = sceKernelUnlockMutex(mutex, 1);
	if (result != 0) {
#if FW_DEBUG
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_PRINTF(0, 599,
			"[SceAppMgr] The actual value is %d(0x%08x), but expected value is %d(0x%08x)\n",
			result, result, 0, 0);
#else
		appmgrShellEventMutexPanic();
#endif
	}
}

void _shellEventRecommendedScreenOrientationStatusChangedQueuePush(void)
{
	AppmgrShellEvent event;
	SceUID mutex;
	int result;

	if (!appmgrGetShellApp())
		return;
	memset(&event, 0, sizeof(event));
	event.event = 17;
	result = sceKernelLockMutex(g_appmgrMainMutex, 1, NULL);
	if (result != 0) {
#if FW_DEBUG
		SCE_DBG_PRINTF(0, 614, "Failed to lock ret=0x%08x\n", result);
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_FAIL(614);
#else
		appmgrShellEventMutexPanic();
#endif
	} else {
		g_appmgrMainMutexOwner = sceKernelGetThreadId();
	}
	result = _shellEventQueuePush(&event);
	if (result != 0) {
#if FW_DEBUG
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_PRINTF(0, 616,
			"[SceAppMgr] The actual value is %d(0x%08x), but expected value is %d(0x%08x)\n",
			result, result, 0, 0);
#else
		appmgrShellEventOwnerPanic();
#endif
	}
	mutex = g_appmgrMainMutex;
	g_appmgrMainMutexOwner = -1;
	result = sceKernelUnlockMutex(mutex, 1);
	if (result != 0) {
#if FW_DEBUG
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_PRINTF(0, 617,
			"[SceAppMgr] The actual value is %d(0x%08x), but expected value is %d(0x%08x)\n",
			result, result, 0, 0);
#else
		appmgrShellEventMutexPanic();
#endif
	}
}

void _shellEventLiveAreaOperationQueuePush(int operation, const char *appName)
{
	AppmgrShellEvent event;
	SceUID mutex;
	int result;

	if (!appmgrGetShellApp())
		return;
	memset(&event, 0, sizeof(event));
	event.event = 19;
	event.data.liveArea.operation = operation;
	strncpy(event.data.liveArea.appName, appName,
		sizeof(event.data.liveArea.appName));
	event.data.liveArea.appName[31] = '\0';
	result = sceKernelLockMutex(g_appmgrMainMutex, 1, NULL);
	if (result != 0) {
#if FW_DEBUG
		SCE_DBG_PRINTF(0, 634, "Failed to lock ret=0x%08x\n", result);
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_FAIL(634);
#else
		appmgrShellEventMutexPanic();
#endif
	} else {
		g_appmgrMainMutexOwner = sceKernelGetThreadId();
	}
	result = _shellEventQueuePush(&event);
	if (result != 0) {
#if FW_DEBUG
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_PRINTF(0, 636,
			"[SceAppMgr] The actual value is %d(0x%08x), but expected value is %d(0x%08x)\n",
			result, result, 0, 0);
#else
		appmgrShellEventOwnerPanic();
#endif
	}
	mutex = g_appmgrMainMutex;
	g_appmgrMainMutexOwner = -1;
	result = sceKernelUnlockMutex(mutex, 1);
	if (result != 0) {
#if FW_DEBUG
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_PRINTF(0, 637,
			"[SceAppMgr] The actual value is %d(0x%08x), but expected value is %d(0x%08x)\n",
			result, result, 0, 0);
#else
		appmgrShellEventMutexPanic();
#endif
	}
}

void _shellEventAppRunningStatusChangedQueuePush(void)
{
	AppmgrShellEvent event;
	SceUID mutex;
	int result;

	if (!appmgrGetShellApp())
		return;
	memset(&event, 0, sizeof(event));
	event.event = 18;
	result = sceKernelLockMutex(g_appmgrMainMutex, 1, NULL);
	if (result != 0) {
#if FW_DEBUG
		SCE_DBG_PRINTF(0, 651, "Failed to lock ret=0x%08x\n", result);
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_FAIL(651);
#else
		appmgrShellEventMutexPanic();
#endif
	} else {
		g_appmgrMainMutexOwner = sceKernelGetThreadId();
	}
	result = _shellEventQueuePush(&event);
	if (result != 0) {
#if FW_DEBUG
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_PRINTF(0, 653,
			"[SceAppMgr] The actual value is %d(0x%08x), but expected value is %d(0x%08x)\n",
			result, result, 0, 0);
#else
		appmgrShellEventOwnerPanic();
#endif
	}
	mutex = g_appmgrMainMutex;
	g_appmgrMainMutexOwner = -1;
	result = sceKernelUnlockMutex(mutex, 1);
	if (result != 0) {
#if FW_DEBUG
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_PRINTF(0, 654,
			"[SceAppMgr] The actual value is %d(0x%08x), but expected value is %d(0x%08x)\n",
			result, result, 0, 0);
#else
		appmgrShellEventMutexPanic();
#endif
	}
}

int sceAppMgrReceiveShellEventNum(void)
{
	SceUInt32 syscall = sceSyscallEnter();
	struct AppState *shell;
	SceUID processId;
	SceUID mutex;
	int result;
	int count;

	if (!SceSblACMgrForDriver_991FDC15(0))
		goto denied;
	shell = appmgrGetShellApp();
	if (!shell)
		goto denied;
	processId = shell->process.processId;
	if (processId != sceKernelGetProcessId())
		goto denied;
	result = sceKernelLockMutex(g_appmgrMainMutex, 1, NULL);
	if (result != 0) {
#if FW_DEBUG
		SCE_DBG_PRINTF(0, 688, "Failed to lock ret=0x%08x\n", result);
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_FAIL(688);
#else
		appmgrShellEventMutexPanic();
#endif
	} else {
		g_appmgrMainMutexOwner = sceKernelGetThreadId();
	}
	count = g_appmgrShellEventQueue.count;
	(void)appmgrUpdateShellWatchDogActivity();
	mutex = g_appmgrMainMutex;
	g_appmgrMainMutexOwner = -1;
	result = sceKernelUnlockMutex(mutex, 1);
	if (result != 0) {
#if FW_DEBUG
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_PRINTF(0, 693,
			"[SceAppMgr] The actual value is %d(0x%08x), but expected value is %d(0x%08x)\n",
			result, result, 0, 0);
#else
		appmgrShellEventMutexPanic();
#endif
	}
	sceSyscallLeave(syscall);
	return count;

denied:
	sceSyscallLeave(syscall);
	return (int)0x8080201f;
}

int sceAppMgrReceiveShellEvent(AppmgrShellEvent *event)
{
	SceUInt32 syscall = sceSyscallEnter();
	struct AppState *shell;
	SceUID processId;
	SceUID mutex;
	int lockResult;
	int unlockResult;
	int result;
	int count;
	int writeIndex;
	int oldest;

	if (!SceSblACMgrForDriver_991FDC15(0))
		goto denied;
	shell = appmgrGetShellApp();
	if (!shell)
		goto denied;
	processId = shell->process.processId;
	if (processId != sceKernelGetProcessId())
		goto denied;
	if (!event) {
		sceSyscallLeave(syscall);
		return (int)0x80802016;
	}
	lockResult = sceKernelLockMutex(g_appmgrMainMutex, 1, NULL);
	if (lockResult != 0) {
#if FW_DEBUG
		SCE_DBG_PRINTF(0, 714, "Failed to lock ret=0x%08x\n", lockResult);
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_FAIL(714);
#else
		appmgrShellEventMutexPanic();
#endif
	} else {
		(void)sceKernelGetThreadId();
	}
	count = g_appmgrShellEventQueue.count;
	if (count != 0) {
		writeIndex = g_appmgrShellEventQueue.writeIndex;
		oldest = count <= writeIndex ? writeIndex - count :
			writeIndex + 180 - count;
		*event = g_appmgrShellEventQueue.events[oldest];
		result = 0;
		--g_appmgrShellEventQueue.count;
	} else {
		result = (int)0x80802015;
	}
	mutex = g_appmgrMainMutex;
	g_appmgrMainMutexOwner = -1;
	unlockResult = sceKernelUnlockMutex(mutex, 1);
	if (unlockResult != 0) {
#if FW_DEBUG
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_PRINTF(0, 718,
			"[SceAppMgr] The actual value is %d(0x%08x), but expected value is %d(0x%08x)\n",
			unlockResult, unlockResult, 0, 0);
#else
		appmgrShellEventMutexPanic();
#endif
	}
	if (event->event == 10)
		signalSemaForDebuggerShellEventCount();
	sceSyscallLeave(syscall);
	return result;

denied:
	sceSyscallLeave(syscall);
	return (int)0x8080201f;
}

int sceAppMgrCheckAppLaunchLimitToAvoidQueueOverflow(void)
{
	SceUID owner = g_appmgrMainMutexOwner;
	int count = 0;
	int cursor;
	int first;
	int index;

	if (owner != sceKernelGetThreadId()) {
#if FW_DEBUG
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_FAIL(732);
#else
		appmgrShellEventOwnerPanic();
#endif
	}
	cursor = g_appmgrShellEventQueue.writeIndex - 1;
	first = g_appmgrShellEventQueue.writeIndex - g_appmgrShellEventQueue.count;
	if (cursor >= first) {
		do {
			AppmgrShellEvent *event;

			index = cursor < 0 ? cursor + 180 : cursor;
			event = &g_appmgrShellEventQueue.events[index];
			if (event->event == 4) {
				if (event->data.lifecycle.relaunchPending == 0)
					++count;
			} else if (event->event == 14) {
				++count;
			}
			--cursor;
		} while (cursor != first - 1);
	}
	for (index = 1; index < 20; ++index) {
		struct AppState *app = &g_appmgrAppStates[index];

		if (app->appId != -1 && app->state != 0x800)
			++count;
	}
	return count <= 18 ? 0 : (int)0x80802026;
}
