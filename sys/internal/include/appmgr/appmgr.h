#pragma once

#include <stddef.h>
#include <scetypes.h>
#include <stdint.h>
#include <appmgr/appmgr_launch.h>

/* SceAppMgr's error codes beyond the SDK's (appmgr.h): the henkaku wiki's and vitasdk's names */
#define SCE_APPMGR_ERROR_INVALID_PARAM_SFO	0x80800015
#define SCE_APPMGR_ERROR_PARAM			0x80802002
#define SCE_APPMGR_ERROR_NOEXEC			0x8080201F	/* the caller may not do this */

int _sceAppMgrAcquireSoundOutExclusive3(int priority, SceUInt32 displayEnabled, SceUInt32 displayMode, const void *userArguments);
int _sceAppMgrAppParamGetInt(int pid, int param, int *value);
int _sceAppMgrGetAppMgrState(void *appMgrState);
int _sceAppMgrGetAppParam2(void *appParam);
int _sceAppMgrGetCurrentBgmState2(void *bgmState);
int _sceAppMgrGetFgAppInfo(void *appInfo);
int _sceAppMgrGetRawPathOfApp0ByAppIdForShell(int appId, char *path);
int _sceAppMgrGetRawPathOfApp0ByPidForShell(SceUID pid, char *path);
int _sceAppMgrGetRecommendedScreenOrientation(int *orientation);
int _sceAppMgrGetStatusByAppId(int appId, void *appStatus);
int _sceAppMgrGetStatusById(SceUID id, void *appStatus);
int _sceAppMgrGetVs0UserDataDrive(char *mountPoint);
int _sceAppMgrGetVs0UserModuleDrive(char *mountPoint);
int _sceAppMgrIsCameraActive(int appId, int *active);
int _sceAppMgrReceiveEvent(void *appEvent);
int _sceAppMgrReceiveEventNum(int *eventNum);
int _sceAppMgrSetNetworkDisconnectionWarningDialogState(SceBool enable, const void *userOptParam);
int _sceAppMgrSetPowerSaveMode(int mode, int suspendBackgroundApps);
int _sceAppMgrSetRecommendedScreenOrientationForShell(int orientation);
int _sceAppMgrSetShellScreenOrientation(int orientation);
int _sceAppMgrSetSystemDataFile(SceUInt32 location, const void *userData, SceSize dataSize);
int _sceAppMgrSystemParamDateTimeGetConf(void *conf);
int _sceAppMgrSystemParamGetInt(int parameter, int *value);
int appmgrAcquireBgmPortForMusicPlayerSyscall(void);
int appmgrAcquireBgmPortSyscall(void);
int appmgrAcquireBgmPortWithPrioritySyscall(int priority);
int appmgrAcquireBtrmSyscall(void);
int appmgrDeclareSystemChatAppSyscall(void);
int appmgrEnableCoredumpForTestSyscall(void);
int appmgrEnableDuckingOnSystemChatSyscall(int enabled, SceUInt32 volume);
int appmgrEnablePrioritizingSystemChatSyscall(int enabled);
int appmgrFinishCoredumpForShellSyscall(void);
int appmgrReleaseBgmPortSyscall(void);
int appmgrReleaseBtrmSyscall(void);
int appmgrSetBgmSubPriorityForSystemChatSyscall(int enabled);
int appmgrSetBgmSubPrioritySyscall(int enabled, int priority);
int appmgrSetDisplayMergeConfSyscall(SceUInt32 setting, SceUInt32 alpha);
int sceAppMgrAcquireBgmPort(void);
int sceAppMgrAcquireBgmPortForMusicPlayer(void);
int sceAppMgrAcquireBgmPortWithPriority(int priority);
int sceAppMgrAcquireBtrm(void);
int sceAppMgrAcquireSoundOutExclusive(int priority, SceUInt32 displayEnabled, SceUInt32 displayMode);
int sceAppMgrAcquireSoundOutExclusive2(int priority, SceUInt32 displayEnabled, SceUInt32 displayMode, int displayRequest);
int sceAppMgrDeclareSystemChatApp(void);
int sceAppMgrDump(void);
int sceAppMgrEnableDuckingOnSystemChat(int enabled, SceUInt32 volume);
int sceAppMgrEnablePrioritizingSystemChat(int enabled);
int sceAppMgrGetAppIdByAppId(int appId);
int sceAppMgrNotifyLiveBoardModeForShell(int appId, int mode);
int sceAppMgrReceiveShellEventNum(void);
int sceAppMgrReleaseBgmPort(void);
int sceAppMgrReleaseBtrm(void);
int sceAppMgrReleaseSoundOutExclusive(void);
int sceAppMgrReleaseSoundOutExclusive2(void);
int sceAppMgrReleaseSoundOutExclusive3(void);
int sceAppMgrSendLiveBoardMode(int mode);
int sceAppMgrSetBgmSubPriority(int enabled, int priority);
int sceAppMgrSetBgmSubPriorityForSystemChat(int enabled);
int sceAppMgrSetDisplayMergeConf(SceUInt32 setting, SceUInt32 alpha);
int sceAppMgrSetSystemImposeState2(int state);

struct SceKernelProcessOpt;

/*
 * SceAppMgrForDriver's debugger launch (deci4p loadp): opt goes on to process creation, its
 * flags limited to those a debugger may give.
 */
SceUID sceAppMgrLaunchAppForDebugger(const char *path, const char *args, SceSize argSize,
				     SceUInt32 processCreateFlags,
				     const struct SceKernelProcessOpt *opt,
				     const AppmgrLaunchOptions *options);
int sceAppMgrKillAppForDebugger(SceUID pid);
int sceAppMgrKillAppByNameForDebugger(const char *titleId);
int sceAppMgrGetPidByNameForDebugger(const char *titleId, SceUID *pid);
int sceAppMgrGetAppIdByNameForDebugger(const char *titleId, int *appId);

/* a boot parameter: SceAppMgrForDriver_D2541B4A clears the attribute and version (the words
 * of the SDK's SceAppUtilBootParam) behind the size */
typedef struct AppmgrBootParam {
	SceSize size;
	SceUInt32 attribute;
	SceUInt32 appVersion;
} AppmgrBootParam;

/* vitasdk: ksceAppMgrGetBootParam */
int SceAppMgrForDriver_D2541B4A(SceUInt32 request, AppmgrBootParam *bootParam);

struct AppmgrLaunchAppOption;
int sceAppMgrLaunchAppForTest2(const char *appPath, const char *name, SceUInt32 budgetSize,
		const char *param, SceUInt32 processAttr, int startByUpdater,
		const char *const *argv, const struct AppmgrLaunchAppOption *option);

/* the status SceAppMgrForDriver_DC13160A reports of a process's application */
typedef struct AppmgrInfobarState {
	unsigned char visibility;
	unsigned char color;
	unsigned char transparency;
	unsigned char reserved;
} AppmgrInfobarState;

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

/* not written in C yet: the prototypes are their callers' (vshbridge, deci4p_loadp, appmgr's
 * system calls) */
int sceAppMgrLaunchAppByNameForDebugger(const char *name, const char *param);
int SceAppMgrForDriver_99AA37C1(const char *name);
int SceAppMgrForDriver_DC13160A(SceUID pid, struct SceAppMgrAppStatus *status);
int sceAppMgrUmount(const char *driveId);
int sceAppMgrDestroyAppForTest(SceUID pid);
int sceAppMgrLiveAreaOperationByNameForLoadp(int operation, const char *appName);
int sceAppMgrLiveAreaOperationForLoadp(int operation);
int SceAppMgrForDriver_12CE29F7(int pid, int param, int *value);	/* vitasdk: ksceAppMgrAppParamGetInt */
struct AppmgrAppMountParam;
struct AppmgrAppUmountParam;
/* vitasdk: ksceAppMgrAppMount */
int SceAppMgrForDriver_EF0C9533(const struct AppmgrAppMountParam *param);
/* vitasdk: ksceAppMgrAppUmount */
int SceAppMgrForDriver_1DE3B7C4(const struct AppmgrAppUmountParam *param);
struct ScePfsRndDriveId;
/* vitasdk: ksceAppMgrGameDataMount */
int SceAppMgrForDriver_CE356B2D(const char *appPath, const char *patchPath,
				const char *rifPath, struct ScePfsRndDriveId *mountPoint);
int sceAppMgrAppDataMount(unsigned int mountId, struct ScePfsRndDriveId *mountPoint);
int sceAppMgrWorkDirMount(unsigned int mountId, struct ScePfsRndDriveId *mountPoint);

/* photo0: to mount for the caller */
typedef struct SceAppMgrPhotoMountParam {
	SceSize size;				/* sizeof(SceAppMgrPhotoMountParam) */
	SceUInt8 reserved[128];
} SceAppMgrPhotoMountParam;

_Static_assert(sizeof(SceAppMgrPhotoMountParam) == 0x84, "SceAppMgrPhotoMountParam");

/* photo0: to unmount */
typedef struct SceAppMgrPhotoUmountParam {
	SceSize size;				/* sizeof(SceAppMgrPhotoUmountParam) */
} SceAppMgrPhotoUmountParam;

/* vitasdk: ksceAppMgrPhotoMount */
int SceAppMgrForDriver_43A35729(const SceAppMgrPhotoMountParam *param);
int SceAppMgrForDriver_DCADB9FE(const SceAppMgrPhotoUmountParam *param);
/* vitasdk: ksceAppMgrTrophyMount */
int SceAppMgrForDriver_E977A833(unsigned int mountId, SceUID pid,
				struct ScePfsRndDriveId *mountPoint);
/* vitasdk: ksceAppMgrMmsMount */
int SceAppMgrForDriver_13D54107(unsigned int mountId, struct ScePfsRndDriveId *mountPoint);
int sceAppMgrAppDataMountById(unsigned int mountId, const char *id,
				struct ScePfsRndDriveId *mountPoint);
int SceAppMgrForDriver_EC9D5A35(void *status);	/* vitasdk: ksceAppMgrSaveDataSlotGetStatus */
int sceAppMgrGetSystemDataFile(SceUInt32 location, void *buffer, SceSize size);
int SceAppMgrForDriver_193784A9(SceUID pid);	/* vitasdk: ksceAppMgrCheckContentInstallPeriod */
int sceAppMgrUpdateRifInfo(const char *titleId);
int SceAppMgrForDriver_B53FB98E(const char *path, int kind);	/* vitasdk: ksceAppMgrRegisterPath */

/* an add-on content directory name (the SDK's SceAppUtilDrmAddcontId) */
typedef struct SceAppMgrDrmAddcontId {
	char data[17];
	char padding[3];
} SceAppMgrDrmAddcontId;

/* the add-on content directory to open or close (names from sceAppUtilDrmOpen) */
typedef struct SceAppMgrDrmOpenParam {
	SceSize size;
	SceAppMgrDrmAddcontId dirName;
	char mountPoint[16];
} SceAppMgrDrmOpenParam;

/* vitasdk: ksceAppMgrDrmOpen */
int SceAppMgrForDriver_EA75D157(const SceAppMgrDrmOpenParam *param);
/* vitasdk: ksceAppMgrDrmClose */
int SceAppMgrForDriver_088670A6(const SceAppMgrDrmOpenParam *param);
/* vitasdk: ksceAppMgrCloudDataCreateHeader */
/* SceAppMgrForDriver_2FAA6ABD's output (names unknown) */
typedef struct SceAppMgrCloudDataKey {
	char accountId[16];
	SceUInt64 parameters;
} SceAppMgrCloudDataKey;

int SceAppMgrForDriver_ED87F191(const char *accountId, SceSize size, const void *digest,
				void *header);
int SceAppMgrForDriver_55D48190(const void *header);	/* vitasdk: ksceAppMgrCloudDataVerifyHeader */
int SceAppMgrForDriver_2FAA6ABD(SceAppMgrCloudDataKey *key);	/* vitasdk: ksceAppMgrCloudDataSetupKey */
int sceAppMgrCloudDataSrcMount(unsigned int type, const char *titleId,
			       struct ScePfsRndDriveId *mountPoint);
int sceAppMgrCloudDataDstCreateMount(const char *name, struct ScePfsRndDriveId *mountPoint);
int sceAppMgrFakeSaveDataCreateMount(const char *path, struct ScePfsRndDriveId *mountPoint);
int SceAppMgrForDriver_0B94BB90(int location, const SceUInt64 *mcId);	/* vitasdk: ksceAppMgrCloudDataSetMcId */
int SceAppMgrForDriver_1286AF64(int location, SceUInt64 *mcId);	/* vitasdk: ksceAppMgrCloudDataGetMcId */
int SceAppMgrForDriver_EC408FEF(int location);	/* vitasdk: ksceAppMgrCloudDataClearMcId */
int sceAppMgrInitSafeMemoryById(const char *id);
int sceAppMgrSaveSafeMemory(const void *data, SceSize size, SceOff offset);
int sceAppMgrLoadSafeMemory(void *data, SceSize size, SceOff offset);
int sceAppMgrGetPfsProcessStatus(void);
int sceAppMgrIsExclusiveProcessRunning(const char *titleId);
/* an additional content's directory in gro0:acinst: its keystone and title */
typedef struct SceAppMgrAcdirParam {
	unsigned char keystone[96];
	char title[128];
} SceAppMgrAcdirParam;

_Static_assert(sizeof(SceAppMgrAcdirParam) == 0xe0, "acdir parameters");

int sceAppMgrAcInstGetAcdirParam(const char *titleId, const char *entitlementLabel,
				 SceAppMgrAcdirParam *param);
/* vitasdk: ksceAppMgrSaveDataLocalBackupTargetGetList */
int SceAppMgrForDriver_9B3F7D66(void *list);
/* vitasdk: ksceAppMgrSaveDataLocalBackupTargetRemoveItem */
int SceAppMgrForDriver_C4EF95BB(const char *saveDataTitleId);
int SceAppMgrForDriver_61B13981(void *offlineId, SceUInt32 *checkWord);	/* vitasdk: ksceAppMgrLocalBackupGetOfflineId */
/* vitasdk: ksceAppMgrLocalBackupVerifyOfflineHeader */
int SceAppMgrForDriver_62E98C42(const void *header);
int sceAppMgrSaveDataNotifyBackupFinished(const char *saveDataTitleId);
