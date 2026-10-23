/* Application state and its complete per-process record. */
#pragma once

#include <stddef.h>
#include <scetypes.h>
#include <appmgr/appmgr_launch.h>
#include <appmgr/appmgr_savedata.h>
#include <appmgr.h>
#include <acmgr/acmgr.h>
#include <threadmgr/threadmgr.h>
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
	SceInt16 relaunchPending;
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
int _changeDeviceArbitrationForImpose2(int imposed);
int _changeDeviceArbitrationForImpose3(SceUInt32 mode);
struct SceDisplayFrameBuf;
struct SceKernelBudgetInfo;
int sceAppMgrCaptureFrameBufIFTUByAppId(int appId, struct SceDisplayFrameBuf *frameBuf);
int sceAppMgrCaptureFrameBufDMACByAppId(int appId, struct SceDisplayFrameBuf *frameBuf);
int sceAppMgrGetBudgetInfo(struct SceKernelBudgetInfo *info);
int sceAppMgrIsCameraActive(SceUID pid, int *devices);
int sceAppMgrSetInfobarState(int visibility, int color, int transparency);
int sceAppMgrSetInfobarStateForCommonDialog(int visibility, int color, int transparency);
int sceAppMgrSetInfobarStateForShellByAppId(int appId, int visibility, int color,
		int transparency);
int sceAppMgrGetPidListForShell(SceUID *pidList, unsigned int count);
int sceAppMgrIsOtherAppPresent(void);
int sceAppMgrIsGameBudgetAppPresent(void);
int sceAppMgrSetShellScreenOrientation(int orientation);
int sceAppMgrSetRecommendedScreenOrientationForShell(int orientation);
int sceAppMgrGetRecommendedScreenOrientation(int *orientation);
int sceAppMgrSetRecommendedScreenOrientationActivated(int activated);
int sceAppMgrGetRunningAppIdListForShell(SceInt32 *appIds, unsigned int count);
int sceAppMgrSetNetworkDisconnectionWarningDialogState(SceBool enable, const void *optParam);
int sceAppMgrSetSystemImposeState(int state);
int sceAppMgrSetSystemImposeState2(int state);

/* the caller is the declared shell process */
static inline int isShellProcess(void)
{
	struct AppState *shell;

	return SceSblACMgrForDriver_991FDC15(0) && (shell = appmgrGetShellApp()) != NULL &&
	       shell->process.processId == sceKernelGetProcessId();
}
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
int _sceAppMgrActivateAppInternal(struct AppState *app);
int _sceAppMgrDeactivateAppInternal(struct AppState *app, int suspend, int flags,
		struct AppmgrWaitObject *waiter);
void updateActiveAppState(void);
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
extern void (*g_appmgrProcTerminatedCallback)(struct AppState *app);

/* SceSysroot's debug notifier for the application manager (SceSysrootForDriver_525B24C5) */
typedef int (*AppmgrDebugNotifier)(int event, int arg1, int arg2, int arg3);

extern AppmgrDebugNotifier g_appmgrDebugNotifier;

/* appmgr.c */
int appmgrReturnSuccess(void);

/* appmgr_aborthandler.c */
void _sceAppMgrNotifyAppMgrHandler(void);
int _sceAppMgrAbortHandlerInitialize(void);

/* appmgr_control.c */
int appmgrSetBackRenderPortOwnerSyscall(int appId);
int appmgrIsCameraActiveSyscall(int appId, int *active);
int appmgrSetPowerSaveModeSyscall(int mode, int suspendBackgroundApps);

/* appmgr_shell.c */
int appmgrRestoreDisplaySettingForShell(void);
int appmgrRestoreBgmSettingForShell(void);
int appmgrReturnLiveAreaOperationResultForShell(int operationResult);
int appmgrActivateApp(int appId);
int appmgrDeactivateApp(int appId);
int appmgrSetPowerSaveMode(int mode, int suspendBackgroundApps);
int appmgrSuspendBgAppByShell(int appId);
int appmgrResumeBgAppByShell(int appId);
int appmgrIsPidShellAndCrashed(SceUID pid);
int appmgrSetShellScreenOrientationSyscall(int orientation);
int appmgrSetRecommendedScreenOrientationForShellSyscall(int orientation);
int _sceAppMgrSetPowerSaveModeInternal(int mode, int suspendBackgroundApps);

/* app_arbitration.c */

/* what _spawnApp takes on the stack SceThreadmgrForDriver_E54FD746 gives it */
typedef struct AppmgrSpawnAppParam {
	struct AppState *app;
	const struct SceKernelProcessOpt *opt;
	const AppmgrLaunchOptions *options;
	int started;			/* the application ran before: start it again */
	const AppmgrCreateProcessParam *createParam;
	int *error;
} AppmgrSpawnAppParam;

/* What a launch asks for: AppmgrLaunchInfo with the path and the arguments by reference */
typedef struct AppmgrLaunchRequest {
	const char *appPath;
	char proxyAppName[32];
	SceUInt32 budgetSize;
	SceUInt32 pycontSize;
	SceUInt32 processCreateFlags;
	SceUInt32 budgetType;
	SceUInt32 processAttr;
	AppmgrInfobarState defaultInfobar;
	const void *launchArgs;
	SceSize launchArgsSize;
	SceUInt32 launchArgumentMarker;
	signed char launchOrigin;
	unsigned char useCommonDialog;
	signed char notifyBootComplete;
	unsigned char reservedLaunch;
	SceUInt32 enlargeGameMemorySize;	/* MiB */
} AppmgrLaunchRequest;

_Static_assert(sizeof(AppmgrLaunchRequest) == 80, "launch request");

/* The launcher's options (sceAppMgrLaunchAppByName2ndStage): the status and the parameter
 * delivery result are copied out to its user pointers */
typedef struct AppmgrLaunchAppOption {
	SceSize size;
	SceUInt32 flags;
	char appName[32];			/* taken when flags has 0x100000 */
	SceAppMgrAppStatus *status;
	SceUInt32 pycontSize;
	AppmgrInfobarState defaultInfobar;
	SceUInt32 launchArgumentMarker;
	char proxyAppName[32];
	SceUInt32 launchOrigin;
	SceUInt32 reserved;
	SceUInt32 *paramResult;			/* four words */
	SceUInt32 useCommonDialog;
	SceUInt32 enlargeGameMemorySize;	/* MiB, at most 109 */
} AppmgrLaunchAppOption;

_Static_assert(sizeof(AppmgrLaunchAppOption) == 108 &&
	       offsetof(AppmgrLaunchAppOption, status) == 40 &&
	       offsetof(AppmgrLaunchAppOption, proxyAppName) == 56 &&
	       offsetof(AppmgrLaunchAppOption, paramResult) == 96, "launch option fields");

/* The option block of the launches by name (the system calls copy it in whole) */
typedef struct AppmgrLaunchAppByNameOption {
	SceSize size;
	SceUInt32 launchMode;
	SceUInt32 reserved;
	SceUID launchCallbackId;		/* the caller's own UIDs */
	SceUID paramCallbackId;
	int creatorAppId;			/* taken when launchMode has 0x840000 */
	SceUInt32 evictionThresholdClass;
	SceUInt32 resumeCreatorOnLaunchFailure;
	signed char reservedTail[32];		/* must be zero */
} AppmgrLaunchAppByNameOption;

_Static_assert(sizeof(AppmgrLaunchAppByNameOption) == 64 &&
	       offsetof(AppmgrLaunchAppByNameOption, launchCallbackId) == 12 &&
	       offsetof(AppmgrLaunchAppByNameOption, reservedTail) == 32,
	       "launch by name option fields");

/* _launchApp4's argument: the launch, run on a stack of its own */
typedef struct AppmgrLaunchApp4Param {
	int appId;
	const struct SceKernelProcessOpt *opt;
	const AppmgrLaunchOptions *options;
	int sendParamToRunning;
	int startByUpdater;
	const AppmgrCreateProcessParam *createParam;
	SceAppMgrAppStatus *status;
	unsigned int *paramResult;
	int *error;
} AppmgrLaunchApp4Param;

int _sceAppMgrDeleteAppInternal(struct AppState *app);
int _applicationShouldMakeRun(struct AppState *app);
int relaunchApp(struct AppState *app, SceUID pid);
int _updateAppOnPowerTickState(struct AppState *app);
int findAndLockLeastValuableMiniApp2(struct AppState **out, int maxValue);
int killLeastValuableMiniApplication2(int maxValue);
int killAllMiniApplication(void);
int killLeastValuableMiniApplication(void);
int _sceAppMgrAppStateReserve(struct AppState **out, const char *name, const char *param,
		SceUID paramCallbackId, SceUInt32 launchMode, SceUID launchCallbackId,
		int creatorAppId, SceUInt32 evictionThresholdClass,
		SceUInt32 resumeCreatorOnLaunchFailure);
int sceAppMgrLaunchAppByNameInternal(SceUInt32 launchMode, const SceChar8 *name,
		const char *param, SceUID launchCallbackId, SceUID paramCallbackId,
		int creatorAppId, SceUInt32 evictionThresholdClass, const void *extraParam,
		SceSize extraParamSize, SceUInt32 resumeCreatorOnLaunchFailure);
int _sceAppMgrLaunchApp2(const char *appPath, const char *name, SceUInt32 budgetSize,
		const char *param, SceUInt32 processAttr, const AppmgrLaunchAppOption *option);
int sceAppMgrLaunchAppByName2ndStage(int appId, const char *appPath, SceUInt32 budgetSize,
		SceUInt32 processAttr, const AppmgrLaunchAppOption *option);
int appmgrLaunchAppByName2(const SceChar8 *name, const char *param,
		const AppmgrLaunchAppByNameOption *option);
int appmgrLaunchVideoStreamingApp(const void *extraParam, SceSize extraParamSize,
		const AppmgrLaunchAppByNameOption *option);
int appmgrLaunchAppByName2ForShell(const SceChar8 *name, const char *param,
		const AppmgrLaunchAppByNameOption *option);
int appmgrLaunchAppByName(SceUInt32 launchMode, const SceChar8 *name, const char *param);
int appmgrLaunchAppByNameForShell(SceUInt32 launchMode, const SceChar8 *name,
		const char *param);
int appmgrLaunchAppByUri(SceUInt32 launchMode, const char *uri);
int appmgrLaunchAppByUri2(const char *uri, const AppmgrLaunchAppByNameOption *option);
int sceAppMgrLaunchAppCancel(int appId);
int sceAppMgrQuitApp(int appId);
int _destroyAllAppForAbortHandler(void);
int _destroyOtherApp(struct AppState *app);
int sceAppMgrDestroyOtherAppByPidForShell(SceUID pid);
int sceAppMgrDestroyOtherAppByAppIdForShell(int appId);
int sceAppMgrDestroyOtherApp(void);
int sceAppMgrGrowMemory(SceSize size, int killMiniApps);
int sceAppMgrGrowMemory3(SceSize size, unsigned int thresholdClass);
int sceAppMgrLoadExec(const char *path, const char *const *argv, const void *option);
int sceAppMgrQuitForNonSuspendableApp(void);
int sceAppMgrExitToLiveboardForGameApp(void);
int sceAppMgrDestroyAppByName(const char *name);
int sceAppMgrDestroyAppByAppId(int appId);
int sceAppMgrSetAppProtectionModeOnMemoryShortage(unsigned int mode);
void _updatePowerTickStateForNonSuspendableApp(void);
int sceAppMgrSendParam(const char *name, const char *param, int option);
void _appArbitrationOnProcessUnloaded(struct AppState *app);
int appmgrStopShellWatchDog(void);
int sceAppMgrLaunchAppByName2ndStage_internal(int appId, const char *name,
		SceAppMgrAppStatus *statusOut, const AppmgrLaunchRequest *request,
		int sendParamToRunning, int startByUpdater, const AppmgrCreateProcessParam *createParam,
		const struct SceKernelProcessOpt *opt, const AppmgrLaunchOptions *options,
		const AppmgrLaunchAppOption *option, int *error);
int _launchApp4(void *argument);
int _spawnApp(void *argument);
void initProcessStateUpdaterThread(void);

/* appmgr_system.c */
int _sceAppMgrSystemSuspend(void);
int _sceAppMgrSystemResume(void);

/* appmgr_for_debugger.c */
void appmgrRegisterDebuggerCallbacks(void);

/* appmgr_lock.c */
int appMgrLockInitialize(void);

/* appmgr_path_syscalls.c */
int appmgrForceUmountSyscall(int mountId);
int appmgrUmountSyscall(const char *driveId);
int appmgrUmountByPidSyscall(SceUID pid, const char *driveId);
int appmgrConvertVs0UserDrivePathSyscall(const char *path, char *output, SceSize size);
int appmgrGetRawPathSyscall(char *path, char *resolvedPath, int capacity);
int appmgrGetSaveDataInfoSyscall(const char *appPath, const char *titleId,
				 SceUInt32 attributes, struct AppmgrSaveDataInfo *info);
int appmgrLoadSaveDataSystemFileSyscall(const char *path, void *buffer, SceSize size);
int appmgrUpdateSaveDataParamSyscall(const char *path);
int appmgrGetUserDirPathSyscall(int partitionId, char *path, unsigned int capacity);
int appmgrGetUserDirPathByIdSyscall(int userId, int partitionId, char *path, unsigned int capacity);
int appmgrAddContMountSyscall(SceUID pid, const struct AppmgrAddContMountParam *param);
struct ScePfsRndDriveId;
int appmgrGameDataMountSyscall(const char *appPath, const char *patchPath,
			       const char *rifPath, struct ScePfsRndDriveId *mountPoint);
int appmgrAppDataMountSyscall(unsigned int mountId, struct ScePfsRndDriveId *mountPoint);
int appmgrWorkDirMountSyscall(unsigned int mountId, struct ScePfsRndDriveId *mountPoint);
int appmgrPspSaveDataRootMountSyscall(struct ScePfsRndDriveId *mountPoint);
int appmgrWorkDirMountByIdSyscall(unsigned int mountId, const char *id,
				  struct ScePfsRndDriveId *mountPoint);
int appmgrTrophyMountSyscall(unsigned int mountId, SceUID pid, struct ScePfsRndDriveId *mountPoint);
int appmgrTrophyMountByIdSyscall(unsigned int mountId, const char *id,
				 struct ScePfsRndDriveId *mountPoint);
int appmgrMmsMountSyscall(unsigned int mountId, struct ScePfsRndDriveId *mountPoint);
int appmgrAppDataMountByIdSyscall(unsigned int mountId, const char *id,
				  struct ScePfsRndDriveId *mountPoint);
int appmgrInstallDirMountSyscall(unsigned int mountId, const char *id, const void *keystone,
				 struct ScePfsRndDriveId *mountPoint);
int appmgrLoopBackMountSyscall(unsigned int mountId, const char *id,
			       struct ScePfsRndDriveId *mountPoint);
int appmgrThemeDataMountSyscall(const char *contentId, const char *themePath,
				const char *rifPath, struct ScePfsRndDriveId *mountPoint);
int appmgrSaveDataAddMountSyscall(const struct AppmgrAddMountParam *param);
int appmgrAddContAddMountSyscall(const struct AppmgrAddMountParam *param, int add);
int appmgrGetRawPathOfApp0ByAppIdForShellSyscall(int appId, char *path);
int appmgrGetRawPathOfApp0ByPidForShellSyscall(SceUID pid, char *path);

/* appmgr_process.c */
int sceAppMgrSuspendUntilActivated(void);
int _appmgrProcessInitialize(void (*callback)(struct AppState *app));

/* appmgr_query_syscalls.c */
int appmgrIsGameProgramSyscall(void);
int appmgrIsNonGameProgramSyscall(void);
int appmgrIsPsNowClientSyscall(void);
int appmgrIsDevelopmentModeSyscall(void);
int appmgrGetBootParamSyscall(SceUInt32 request, void *bootParam);
int appmgrAppMountSyscall(const struct AppmgrAppMountParam *param);
int appmgrAppUmountSyscall(const struct AppmgrAppUmountParam *param);
int appmgrAppParamGetIntSyscall(int pid, int param, int *value);
int appmgrAppParamGetStringSyscall(int pid, int param, char *string, SceSize length);
int appmgrAppParamSetStringSyscall(int param, const char *string);
int sceAppMgrAppParamSetString(int param, const char *string);
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
int appmgrSetRecommendedScreenOrientationActivatedSyscall(int activated);
int appmgrIsOtherAppPresentSyscall(void);
int appmgrIsGameBudgetAppPresentSyscall(void);

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
int sharedFbGetMemBlockByIndex(int index, SceUID *memBlock);
int sharedFbSetRequestedPids(SceUID memBlock, const SceUID pids[8]);
int sharedFbSetShellRenderPort(unsigned int port);

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
