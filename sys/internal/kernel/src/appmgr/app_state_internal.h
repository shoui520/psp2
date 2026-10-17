/* Application state and its complete per-process record. */
#pragma once

#include <stddef.h>
#include <psp2kern/types.h>
#include <appmgr/appmgr_launch.h>
#include "appmgr_event.h"
#include "appmgr_internal_util.h"
#include "mount_holder_internal.h"
#include "process_content_internal.h"

typedef struct AppmgrInfobarState {
	unsigned char visibility;
	unsigned char color;
	unsigned char transparency;
	unsigned char reserved;
} AppmgrInfobarState;

struct SceAppMgrAppInfo {
	SceUInt32 processRunRequested;
	SceUInt8 reserved[124];
};

_Static_assert(sizeof(struct SceAppMgrAppInfo) == 128 &&
	       offsetof(struct SceAppMgrAppInfo, processRunRequested) == 0 &&
	       offsetof(struct SceAppMgrAppInfo, reserved) == 4,
	       "application run request output");

typedef struct SceAppMgrAppStatus {
	SceUID launchSourceProcessId;
	SceUInt32 launchMode;
	SceUInt32 protectionMode;
	char appName[32];
	SceUInt32 paramDeliveryHandled;
	int appId;
	SceUID processId;
	SceBool activated;
	SceBool processRunRequested;
	SceUInt32 networkDisconnectionWarningDialogEnabled;
	int launchSourceAppId;
	AppmgrInfobarState infobar;
	SceUInt32 launchArgumentMarker;
	char bgmProxyAppName[32];
	unsigned char launchOrigin;
	unsigned char budgetType;
	SceUInt32 processAttr;
	int systemOrientation;
	SceUInt32 recommendedOrientationActivated;
} SceAppMgrAppStatus;

_Static_assert(sizeof(SceAppMgrAppStatus) == 128 &&
	       offsetof(SceAppMgrAppStatus, appName) == 12 &&
	       offsetof(SceAppMgrAppStatus, appId) == 48 &&
	       offsetof(SceAppMgrAppStatus, infobar) == 72 &&
	       offsetof(SceAppMgrAppStatus, bgmProxyAppName) == 80 &&
	       offsetof(SceAppMgrAppStatus, launchOrigin) == 112 &&
	       offsetof(SceAppMgrAppStatus, budgetType) == 113 &&
	       offsetof(SceAppMgrAppStatus, processAttr) == 116 &&
	       offsetof(SceAppMgrAppStatus, recommendedOrientationActivated) == 124,
	       "application status output");

typedef struct AppmgrProcessState {
	char appName[32];
	SceUID processId;
	SceUID budgetId;
	AppmgrMountHolder mounts;
	AppmgrProcessContent content;
	SceUInt32 maxFD;
	SceUInt32 maxDIR;
	SceUInt32 primaryResourceQuota;
	SceUInt32 secondaryResourceQuota;
	SceUInt32 requestedProcessState;
	SceUInt32 notifiedProcessState;
	SceUInt32 runWhenUiOverlay;
	struct AppmgrWaitObject *runningWaiters;
	struct AppmgrWaitObject *suspendWaiters;
	struct AppmgrWaitObject *unloadWaiter;
} AppmgrProcessState;

struct AppState {
	int appId;
	int baseMiniEvictionValue;
	SceUInt32 launchMode;
	char appPath[256];
	char proxyAppName[32];
	SceUInt32 budgetSize;
	SceUInt32 pycontSize;
	SceUInt32 processCreateFlags;
	SceUInt32 budgetType;
	SceUInt32 processAttr;
	AppmgrInfobarState defaultInfobar;
	char launchArgs[1024];
	SceSize launchArgsSize;
	SceUInt32 launchArgumentMarker;
	unsigned char launchOrigin;
	unsigned char useCommonDialog;
	unsigned char notifyBootComplete;
	unsigned char reservedLaunch;
	SceUInt32 killMiniBeforeLaunch;
	SceUInt32 reservedProcess;
	AppmgrProcessState process;
	SceUID launchSourceProcessId;
	int launchSourceAppId;
	SceUInt32 state;
	int exitReason;
	SceUInt32 selfQuit;
	SceUInt32 paramDeliveryHandled;
	SceUInt32 pendingActivate;
	SceUInt32 pendingDeactivate;
	SceUInt32 pendingResume;
	SceUInt32 pendingQuit;
	SceUInt32 pendingLaunchParam;
	SceUInt32 pendingExtendedEvent;
	SceUInt32 pendingStorePurchase;
	SceUInt32 pendingStoreRedemption;
	SceUInt32 pendingNpMessage;
	SceUInt32 pendingOrientationChanged;
	SceUInt32 reservedEvent;
	AppmgrParamQueue params;
	AppmgrInfobarState appInfobar;
	AppmgrInfobarState commonDialogInfobar;
	AppmgrInfobarState shellInfobar;
	SceUID creatorPid;
	SceUID launchCallbackId;
	SceUInt64 quitStartTime;
	SceUInt16 quitRequested;
	SceUInt16 relaunchPending;
	SceUInt32 unusedLaunchMarker;
	SceUInt32 protectionMode;
	SceUID mutex;
	SceUID mutexOwner;
	AppmgrLaunchOptions launchOptions;
	SceUInt32 videoCodecUsage;
	SceUInt32 evictionThresholdClass;
	int coredumpReason;
	SceUInt32 triggerData;
	int systemOrientation;
	SceUInt32 recommendedOrientationActivated;
	SceUInt32 networkDisconnectionWarningDialogEnabled;
	SceUInt32 relaunchInProgress;
	SceUInt32 resumeCreatorOnLaunchFailure;
	unsigned char backgroundSuspendRequested;
	unsigned char extraAppParamAllowed;
	unsigned char gameCardRemovalQuitPending;
	unsigned char reservedTail;
};

_Static_assert(sizeof(AppmgrInfobarState) == 4, "infobar state");
_Static_assert(sizeof(AppmgrProcessState) == 1376, "process state");
_Static_assert(offsetof(AppmgrProcessState, mounts) == 40 &&
	       offsetof(AppmgrProcessState, content) == 48 &&
	       offsetof(AppmgrProcessState, suspendWaiters) == 1368,
	       "process owner fields");
_Static_assert(sizeof(struct AppState) == 9232, "application state");
_Static_assert(offsetof(struct AppState, process) == 1368 &&
	       offsetof(struct AppState, params) == 2812 &&
	       offsetof(struct AppState, appInfobar) == 7076 &&
	       offsetof(struct AppState, quitStartTime) == 7096 &&
	       offsetof(struct AppState, launchOptions) == 7124 &&
	       offsetof(struct AppState, resumeCreatorOnLaunchFailure) == 9224,
	       "application owner fields");

extern struct AppState g_appmgrAppStates[20];

struct AppState *appmgrFindAppById(int appId);
struct AppState *appmgrFindSpecialApp(struct AppState *apps, SceUID pid);
struct AppState *appmgrFindCurrentApp(struct AppState *apps);
struct AppState *appmgrFindAppByProcessId(struct AppState *apps, SceUID pid);
struct AppState *appmgrGetShellApp(void);
void _appListShowAppState(struct AppState *app);
void _appListSetRunStatus(struct AppState *app, unsigned int status);
void _appListSetDebuggerRunStatus(struct AppState *app, unsigned int status);
void _appListSetActiveness(struct AppState *app, unsigned int status);
void _appListDeclareShellProcess(struct AppState *apps);
void _appListUndeclareShellProcess(struct AppState *app);
void _appListClearAppState(struct AppState *app);
int appmgrInitializeAppState(struct AppState *app);
int appmgrClearFailedAppLaunch(struct AppState *app, int launchResult,
			     SceUID *creatorPid);
struct AppState *appmgrFindActiveApp(struct AppState *apps);
struct AppState *appmgrFindAppByName(struct AppState *apps, const char *name);
int _appListReserveAndLockApp(struct AppState **output);
int _appListFindAndLockAppByAppId(int appId, struct AppState **output);
int _appListFindAndLockAppByName(const char *name, struct AppState **output);
int _appListFindAndLockAppByName2(const char *name, struct AppState **output,
		int excludedAppId);
int _appListFindAndLockActiveApp(struct AppState **output);
int _appListFindAndLockAppByProcessId(SceUID pid, struct AppState **output);
int _appListUnlockApp(struct AppState *app);
int _appListGetAppStatus(struct AppState *app, SceAppMgrAppStatus *status);
void appmgrResetProcessState(AppmgrProcessState *process);
void appmgrClearPendingAppEvents(struct AppState *app);
int appmgrInitializeShellEventQueue(void);
int appmgrGetAppRunState(const struct AppState *app);
int appmgrGetAppRunStatus(const struct AppState *app);
int _appmgrProcessQuitProcess(struct AppState *app, int exitReason,
			     int relaunch, int retry);
void appmgrSetAppStateFlags(struct AppState *app, unsigned int flags);
void appmgrMaskAppStateFlags(struct AppState *app, unsigned int mask);
_Noreturn void appmgrAppListPanic(void);
struct AppState *_appListGetBigApp(struct AppState *apps);
#if !FW_DEBUG
_Noreturn void appmgrAppListOwnerPanic(void);
#endif
int getMiniAppValue(struct AppState *app);
int _appmgrProcessSuspendProcess(struct AppState *app, int flags,
				struct AppmgrWaitObject *waiter);
int _appmgrProcessResumeProcess(struct AppState *app);
int _appmgrProcessStartProcess(struct AppState *app);
int vcStateChanged(SceUID pid, int enabled);
void appmgrProcessIdleCallback(SceUID pid, unsigned int elapsed, void *argument);
int videocodecUsageChangedCallback2(SceUID pid, unsigned int usage);
int _sceAppMgrProcTerminatedHandler(SceUID pid);
extern int (*g_appmgrProcTerminatedCallback)(struct AppState *app);
