#pragma once

#include <psp2kern/types.h>
#include <stdint.h>

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
int appmgrEnableDuckingOnSystemChatSyscall(int enabled, SceUInt32 volume);
int appmgrEnablePrioritizingSystemChatSyscall(int enabled);
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

/*
 * SceAppMgrForDriver's debugger launch (deci4p loadp). Not written in C yet: the prototypes
 * and the parameter block follow vitasdk's psp2kern/appmgr.h. Bit 31 of the word after size
 * stops the application at launch.
 */
typedef struct SceAppMgrLaunchParam {
	SceSize size;
	SceUInt32 flags;
	SceUInt32 reserved[11];
} SceAppMgrLaunchParam;

_Static_assert(sizeof(SceAppMgrLaunchParam) == 0x34, "SceAppMgrLaunchParam");

SceUID sceAppMgrLaunchAppForDebugger(const char *path, const char *args, SceSize argSize,
				     unsigned int flags, const SceAppMgrLaunchParam *param,
				     void *opt);
int sceAppMgrKillAppForDebugger(SceUID pid);

/* not written in C yet: the prototypes are their callers' (vshbridge, deci4p_loadp, appmgr's
 * system calls) */
int sceAppMgrLaunchAppByNameForDebugger(const char *titleId, int flags);
int sceAppMgrGetPidByNameForDebugger(const char *titleId, SceUID *pid);
int sceAppMgrKillAppByNameForDebugger(const char *titleId);
int sceAppMgrLaunchAppForTest2(const char *path, const char *titleId, int flags, const char *args,
		int option1, int option2, int option0, const void *data);
int sceAppMgrDestroyAppForTest(SceUID pid);
int SceAppMgrForDriver_3B4F305B(int kind, const char *titleId);	/* tool app control */
int SceAppMgrForDriver_12CE29F7(int pid, int param, int *value);	/* vitasdk: ksceAppMgrAppParamGetInt */
int SceAppMgrForDriver_D2541B4A(SceUInt32 request, void *bootParam);	/* vitasdk: ksceAppMgrGetBootParam */
int SceAppMgrForDriver_EC9D5A35(void *status);	/* vitasdk: ksceAppMgrSaveDataSlotGetStatus */
int SceAppMgrForDriver_193784A9(void *a);	/* vitasdk: ksceAppMgrCheckContentInstallPeriod */
int sceAppMgrUpdateRifInfo(const void *info);
int SceAppMgrForDriver_B53FB98E(const char *path, void *a);	/* vitasdk: ksceAppMgrRegisterPath */
/* vitasdk: ksceAppMgrCloudDataCreateHeader */
int SceAppMgrForDriver_ED87F191(const void *a, int b, const void *c, void *d);
int SceAppMgrForDriver_55D48190(const void *header);	/* vitasdk: ksceAppMgrCloudDataVerifyHeader */
int SceAppMgrForDriver_2FAA6ABD(void *key);	/* vitasdk: ksceAppMgrCloudDataSetupKey */
int sceAppMgrCloudDataSrcMount(int a, const void *b, void *c);
int sceAppMgrCloudDataDstCreateMount(const void *a, void *b);
int sceAppMgrFakeSaveDataCreateMount(const char *path, void *b);
int SceAppMgrForDriver_0B94BB90(int a, const void *id);	/* vitasdk: ksceAppMgrCloudDataSetMcId */
int SceAppMgrForDriver_1286AF64(int a, void *id);	/* vitasdk: ksceAppMgrCloudDataGetMcId */
int SceAppMgrForDriver_EC408FEF(int a);	/* vitasdk: ksceAppMgrCloudDataClearMcId */
int sceAppMgrGetPfsProcessStatus(void);
int sceAppMgrIsExclusiveProcessRunning(const void *a);
int sceAppMgrAcInstGetAcdirParam(const void *a, const void *b, void *c);
/* vitasdk: ksceAppMgrSaveDataLocalBackupTargetGetList */
int SceAppMgrForDriver_9B3F7D66(void *list);
/* vitasdk: ksceAppMgrSaveDataLocalBackupTargetRemoveItem */
int SceAppMgrForDriver_C4EF95BB(const void *item);
int SceAppMgrForDriver_61B13981(void *id, void *b);	/* vitasdk: ksceAppMgrLocalBackupGetOfflineId */
/* vitasdk: ksceAppMgrLocalBackupVerifyOfflineHeader */
int SceAppMgrForDriver_62E98C42(const void *header);
int sceAppMgrSaveDataNotifyBackupFinished(const void *a);
