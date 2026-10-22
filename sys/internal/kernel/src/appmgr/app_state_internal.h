/* Application state and its complete per-process record. */
#pragma once

#include <stddef.h>
#include <scetypes.h>
#include <appmgr/appmgr_launch.h>
#include <appmgr/appmgr_savedata.h>
#include <appmgr.h>
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

/* What a launch asks for: Sony's process creation takes it whole (the game and mini
 * creators read the budget, attributes and launch origin through it) */
typedef struct AppmgrLaunchInfo {
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
	signed char launchOrigin;
	unsigned char useCommonDialog;
	signed char notifyBootComplete;
	unsigned char reservedLaunch;
	SceUInt32 enlargeGameMemorySize;	/* MiB */
	SceUInt32 reserved;
} AppmgrLaunchInfo;

_Static_assert(sizeof(AppmgrLaunchInfo) == 1356, "launch information");
_Static_assert(offsetof(AppmgrLaunchInfo, processAttr) == 304 &&
	       offsetof(AppmgrLaunchInfo, launchOrigin) == 1344 &&
	       offsetof(AppmgrLaunchInfo, enlargeGameMemorySize) == 1348,
	       "launch information fields");

struct AppState {
	int appId;
	int baseMiniEvictionValue;
	SceUInt32 launchMode;
	AppmgrLaunchInfo launchInfo;
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
	SceInt16 quitRequested;
	SceUInt16 relaunchPending;
	SceUInt32 unusedLaunchMarker;
	SceUInt32 protectionMode;
	SceUID mutex;
	SceUID mutexOwner;
	AppmgrLaunchOptions launchOptions;
	SceUInt32 videoCodecUsage;
	SceUInt32 evictionThresholdClass;
	int coredumpReason;
	SceUID crashThreadId;		/* the coredump trigger's crash_thid */
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

struct SceKernelProcessOpt;

/* what a launch asks of process creation */
typedef struct AppmgrCreateProcessParam {
	int useTitleIdAsName;		/* the process is named by its content's title ID */
	int enableActivationEvents;	/* processAttr 2, where the content's attribute allows it */
} AppmgrCreateProcessParam;

SceUID _appmgrProcessCreateProcess(struct AppState *app, const char *name,
				   const AppmgrLaunchOptions *options,
				   const struct SceKernelProcessOpt *opt,
				   const AppmgrCreateProcessParam *param, int *error);
int vcStateChanged(SceUID pid, int enabled);
void appmgrProcessIdleCallback(SceUID pid, unsigned int elapsed, void *argument);
int videocodecUsageChangedCallback2(SceUID pid, unsigned int usage);
int _sceAppMgrProcTerminatedHandler(SceUID pid);
extern int (*g_appmgrProcTerminatedCallback)(struct AppState *app);

/* app_arbitration.c */
void initProcessStateUpdaterThread(void);

/* appmgr.c */
int appmgrReturnSuccess(void);

/* appmgr_aborthandler.c */
void _sceAppMgrNotifyAppMgrHandler(void);
int _sceAppMgrAbortHandlerInitialize(void);

/* appmgr_control.c */
int appmgrSetBackRenderPortOwnerSyscall(int appId);
int appmgrIsCameraActiveSyscall(int appId, int *active);
int appmgrSetPowerSaveModeSyscall(int mode, int suspendBackgroundApps);
int appmgrSetShellScreenOrientationSyscall(int orientation);
int appmgrSetRecommendedScreenOrientationForShellSyscall(int orientation);

/* appmgr_for_debugger.c */
void appmgrRegisterDebuggerCallbacks(void);

/* appmgr_lock.c */
int appMgrLockInitialize(void);

/* appmgr_path_syscalls.c */
int appmgrConvertVs0UserDrivePathSyscall(const char *path, char *output, SceSize size);
int appmgrGetRawPathSyscall(char *path, char *resolvedPath, int capacity);
int appmgrGetUserDirPathSyscall(int partitionId, char *path, unsigned int capacity);
int appmgrGetUserDirPathByIdSyscall(int userId, int partitionId, char *path, unsigned int capacity);
int appmgrGetRawPathOfApp0ByAppIdForShellSyscall(int appId, char *path);
int appmgrGetRawPathOfApp0ByPidForShellSyscall(SceUID pid, char *path);

/* appmgr_process.c */
int sceAppMgrSuspendUntilActivated(void);
int _appmgrProcessInitialize(int (*callback)(struct AppState *app));

/* appmgr_query_syscalls.c */
int appmgrGetBootParamSyscall(SceUInt32 request, void *bootParam);
int appmgrAppParamGetIntSyscall(int pid, int param, int *value);
int appmgrAppParamGetStringSyscall(int pid, int param, char *string, SceSize length);
int appmgrGetSystemDataFilePlayReadySyscall(SceUInt32 location, void *information,
					  void *userData, SceSize dataSize);
int appmgrSetSystemDataFilePlayReadySyscall(SceUInt32 location,
		const void *information, const void *userData, SceSize dataSize);
int appmgrSetSystemDataFileSyscall(SceUInt32 location, const void *userData, SceSize dataSize);
int appmgrGetNameByIdSyscall(SceUID id, char *appName);
int appmgrGetIdByNameSyscall(SceUID *id, const char *appName);
int appmgrGetAppStateSyscall(SceAppMgrAppState *appState, SceSize size, SceUInt32 buildVersion);
int appmgrReceiveSystemEventSyscall(SceAppMgrSystemEvent *event);
int appmgrSetNetworkDisconnectionWarningDialogStateSyscall(SceBool enable, const void *optParam);
int appmgrGetCurrentBgmStateSyscall(void *currentBgmId, void *currentBgmState);
int appmgrGetCurrentBgmState2Syscall(void *bgmState);
int appmgrGetStatusByIdSyscall(SceUID id, void *appStatus);
int appmgrGetStatusByAppIdSyscall(int appId, void *appStatus);
int appmgrGetStatusByNameSyscall(const char *appName, void *appStatus);
int appmgrReceiveNotificationRequestForShellSyscall(AppmgrNotificationRequest *request);
int appmgrPeekLaunchParamForShellSyscall(int appId, char *param);
int appmgrOverwriteLaunchParamForShellSyscall(int appId, const char *param);
int appmgrGetAppParamSyscall(char *param);
int appmgrGetAppParam2Syscall(void *appParam);
int appmgrGetAppMgrStateSyscall(void *appMgrState);
int appmgrGetAppInfoSyscall(const char *appName, SceAppMgrAppState *appInfo);
int appmgrGetFgAppInfoSyscall(void *appInfo);
int appmgrGetRecommendedScreenOrientationSyscall(int *orientation);
int appmgrGetRunningAppIdListForShellSyscall(SceInt32 *appIds, int count);

/* appmgr_savedata_syscalls.c */
int appmgrSaveDataSlotInitSyscall(void *slot);
int appmgrSaveDataSlotCreateSyscall(SceAppMgrSaveDataSlot *slot);
int appmgrSaveDataSlotDeleteSyscall(SceAppMgrSaveDataSlotDelete *slot);
int appmgrSaveDataSlotSetParamSyscall(SceAppMgrSaveDataSlot *slot);
int appmgrSaveDataSlotGetParamSyscall(SceAppMgrSaveDataSlot *slot);
int appmgrSaveDataSlotSetStatusSyscall(void *status);
int appmgrSaveDataSlotGetStatusSyscall(void *status);
int appmgrSaveDataSlotFileGetParamSyscall(unsigned int slotId, void *param);

/* appmgr_service.c */
int sceAppMgrGetNameById(SceUID pid, char *appName);
int sceAppMgrGetIdByName(SceUID *pid, const char *appName);
SceUID sceAppMgrGetProcessIdByAppIdForShell(SceInt32 appId);
int sceAppMgrGetStatusById(SceUID id, void *appStatus);
int sceAppMgrGetStatusByAppId(int appId, void *appStatus);
int sceAppMgrGetStatusByName(const char *appName, void *appStatus);
SceInt32 sceAppMgrGetAppInfo(const char *appName, SceAppMgrAppState *output);
int sceAppMgrGetFgAppInfo(void *output);
int sceAppMgrGetRawPathOfApp0ByAppIdForShell(int appId, char path[292]);
int sceAppMgrGetRawPathOfApp0ByPidForShell(SceUID pid, char *path);

/* appmgr_syscalls.c */
int appmgrSystemParamDateTimeGetConfSyscall(void *conf);
int appmgrSystemParamGetIntSyscall(int parameter, int *value);
int appmgrSystemParamGetStringSyscall(int parameter, char *value, SceSize size);
int appmgrCountPfsMountReferencesSyscall(unsigned int mountId, int flags);
int appmgrGetVs0UserModuleDriveSyscall(char *mountPoint);
int appmgrGetVs0UserDataDriveSyscall(char *mountPoint);
int appmgrSendNotificationRequest(const AppmgrNotificationRequest *request, int wait);
int appmgrAcquireSoundOutExclusive3Syscall(int priority, SceUInt32 displayEnabled,
		SceUInt32 displayMode, int displayBlanked, SceUInt32 unsupportedOptions);
int appmgrSetBgmProxyAppSyscall(const char *appName);

/* appmgr_system_data.c */
int appmgrReadPlayReadySystemData(SceUInt32 location, void *information,
				void *userData, SceSize dataSize);
int appmgrWritePlayReadySystemData(SceUInt32 location, const void *information,
				 const void *userData, SceSize dataSize);
int appmgrSetSystemDataFile(SceUInt32 location, const void *userData, SceSize dataSize);

/* bgdl.c */
int appmgrInitializeBgdlQueueStorage(void);

/* savedata.c */
int appmgrCreateSaveDataSlotFile(SceUID pid, const char *path, SceUInt32 slotCount);
int appmgrSaveDataSlotFileGetParam(unsigned int slotId, void *param);

/* sharedfb.c */
int sharedFbInitialize(void);

/* system_param.c */
int appmgrSystemParamDateTimeGetConf(void *conf);
int appmgrSystemParamGetInt(int parameter, int *value);
int appmgrSystemParamGetString(int parameter, char *value, SceSize size);

/* appmgr_aborthandler.c: sceAppMgrGetCoredumpStateForShell's output */
typedef struct AppmgrShellCoredumpState {
	SceUID processId;
	int appId;
	int progress;		/* percent */
	int finished;
	int result;
	SceSize pathLength;
	char path[1024];
	int finishArg;
	int reserved[3];
} AppmgrShellCoredumpState;

_Static_assert(sizeof(AppmgrShellCoredumpState) == 0x428, "shell coredump state");

int sceAppMgrGetCoredumpStateForShell(AppmgrShellCoredumpState *state);
int sceAppMgrFinishCoredumpForShell(void);
int sceAppMgrEnableCoredumpForTest(void);
int appmgrGetCoredumpStateForShellSyscall(AppmgrShellCoredumpState *state);
