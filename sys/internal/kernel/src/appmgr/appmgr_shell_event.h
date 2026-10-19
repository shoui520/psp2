/* Application lifecycle events queued for the shell. */
#pragma once

#include <stddef.h>
#include <scetypes.h>
#include <appmgr/appmgr.h>

typedef struct AppmgrShellEvent {
	int event;
	union {
		struct {
			SceUID processId;
			int exitReason;
			SceUID sourcePid;
			int selfQuit;
			int appId;
			int sourceAppId;
			int relaunchPending;
			char appName[32];
		} lifecycle;
		struct {
			SceUID processId;
			int appId;
			unsigned char reserved[52];
		} loaded;
		struct {
			SceUID processId;
			int appId;
			int mode;
			unsigned char reserved[48];
		} liveareaMode;
		struct {
			int appIds[2];
			unsigned char reserved[52];
		} launchByName;
		struct {
			SceUID processId;
			int liveareaType;
			int appId;
			unsigned char reserved[48];
		} debuggerStart;
		struct {
			SceUID processId;
			int result;
			int appId;
			unsigned char reserved[48];
		} debuggerResult;
		struct {
			SceUID processId;
			int appId;
			int progressNotification;
			unsigned char reserved[48];
		} coredump;
		struct {
			int oldAppId;
			int newAppId;
			SceUID oldProcessId;
			SceUID newProcessId;
			unsigned char reserved[44];
		} voiceChat;
		struct {
			int appId;
			int error;
			unsigned char reserved[52];
		} fatalError;
		struct {
			int appId;
			unsigned char reserved[56];
		} appRequest;
		struct {
			int operation;
			char appName[32];
			unsigned char reserved[24];
		} liveArea;
	} data;
} AppmgrShellEvent;

typedef struct AppmgrShellEventQueue {
	int count;
	int writeIndex;
	AppmgrShellEvent events[180];
} AppmgrShellEventQueue;

_Static_assert(sizeof(AppmgrShellEvent) == 64, "shell event size");
_Static_assert(offsetof(AppmgrShellEvent, data) == 4 &&
	       offsetof(AppmgrShellEvent, data.lifecycle.appId) == 20 &&
	       offsetof(AppmgrShellEvent, data.lifecycle.relaunchPending) == 28 &&
	       offsetof(AppmgrShellEvent, data.lifecycle.appName) == 32 &&
	       offsetof(AppmgrShellEvent, data.loaded.appId) == 8 &&
	       offsetof(AppmgrShellEvent, data.launchByName.appIds[1]) == 8 &&
	       offsetof(AppmgrShellEvent, data.debuggerStart.appId) == 12 &&
	       offsetof(AppmgrShellEvent, data.debuggerResult.result) == 8 &&
	       offsetof(AppmgrShellEvent, data.debuggerResult.appId) == 12 &&
	       offsetof(AppmgrShellEvent, data.coredump.progressNotification) == 12 &&
	       offsetof(AppmgrShellEvent, data.voiceChat.newProcessId) == 16 &&
	       offsetof(AppmgrShellEvent, data.fatalError.error) == 8 &&
	       offsetof(AppmgrShellEvent, data.appRequest.appId) == 4 &&
	       offsetof(AppmgrShellEvent, data.liveareaMode.appId) == 8 &&
	       offsetof(AppmgrShellEvent, data.liveareaMode.mode) == 12 &&
	       offsetof(AppmgrShellEvent, data.liveArea.appName) == 8,
	       "shell event variants");
_Static_assert(sizeof(AppmgrShellEventQueue) == 11528 &&
	       offsetof(AppmgrShellEventQueue, writeIndex) == 4 &&
	       offsetof(AppmgrShellEventQueue, events) == 8,
	       "shell event queue");

extern AppmgrShellEventQueue g_appmgrShellEventQueue;

int _shellEventQueuePush(const AppmgrShellEvent *event);
void _shellEventRecommendedScreenOrientationStatusChangedQueuePush(void);
void _shellEventOnAppLaunchByNameQueuePush(int appId, int secondAppId);
void _shellEventDestroyOtherAppQueuePush(SceUID processId, int appId);
void _shellEventStartLaunchAppFromDebuggerQueuePush(SceUID processId,
		int liveareaType, int appId);
void _shellEventResultLaunchAppFromDebuggerQueuePush(SceUID processId,
		int appId, int launchResult);
void _shellEventCoredumpStartQueuePush(SceUID processId, int appId,
		int progressNotification);
void _shellEventVoiceChatAppChangedQueuePush(int oldAppId, int newAppId,
		SceUID oldProcessId, SceUID newProcessId);
void _shellEventLiveareaModeChangedQueuePush(SceUID processId, int appId, int mode);
void _shellEventFatalErrorQueuePush(int appId, int error);
void _shellEventActivateAppRequestQueuePush(int appId);
void _shellEventDeactivateAppRequestQueuePush(int appId);
void _shellEventLiveAreaOperationQueuePush(int operation, const char *appName);
void _shellEventAppRunningStatusChangedQueuePush(void);
void _shellEventOnAppLoadedQueuePush(SceUID pid, int appId);
void _shellEventOnAppUnloadingQueuePush(SceUID pid, int appId, int exitReason,
		int selfQuit, SceUID sourcePid, int sourceAppId,
		int relaunchPending, const char *appName);
void _shellEventOnAppUnloadedQueuePush(SceUID pid, int appId, int exitReason,
		SceUID sourcePid, int sourceAppId, int relaunchPending,
		const char *appName);

int sceAppMgrCheckAppLaunchLimitToAvoidQueueOverflow(void);
int sceAppMgrReceiveShellEvent(AppmgrShellEvent *event);

/* appmgr_shell.c */
SceInt64 appmgrUpdateShellWatchDogActivity(void);
