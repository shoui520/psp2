#pragma once

#include <psp2kern/types.h>
#include <stdint.h>

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
