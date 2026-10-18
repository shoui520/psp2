/* Application query and event syscall dispatch. */

#include <psp2/appmgr.h>
#include "appmgr_event.h"
#include <appmgr/appmgr.h>

extern int SceAppMgrForDriver_12CE29F7(int pid, int param, int *value);
extern int SceAppMgrForDriver_D2541B4A(SceUInt32 request, void *bootParam);
extern int sceAppMgrPeekLaunchParamForShell(int appId, char *param);
extern int sceAppMgrOverwriteLaunchParamForShell(int appId, const char *param);

extern int sceAppMgrSetNetworkDisconnectionWarningDialogState(SceBool enable,
							   const void *optParam);
extern int sceAppMgrGetStatusById(SceUID id, void *appStatus);
extern int sceAppMgrGetStatusByAppId(int appId, void *appStatus);
extern int sceAppMgrGetStatusByName(const char *appName, void *appStatus);
extern int sceAppMgrGetRecommendedScreenOrientation(int *orientation);
extern int sceAppMgrGetCurrentBgmState(void *currentBgmId, void *currentBgmState);
extern int sceAppMgrGetCurrentBgmState2(void *bgmState);
extern int _sceAppMgrGetParam(char *param);
extern int sceAppMgrGetAppParam2(void *appParam);
extern int sceAppMgrGetAppMgrState(void *appMgrState);
extern int sceAppMgrGetFgAppInfo(void *appInfo);
extern int appmgrReadPlayReadySystemData(SceUInt32 location, void *information,
				       void *userData, SceSize dataSize);
extern int appmgrWritePlayReadySystemData(SceUInt32 location,
		const void *information, const void *userData, SceSize dataSize);
extern int appmgrSetSystemDataFile(SceUInt32 location, const void *userData,
				 SceSize dataSize);

int appmgrGetBootParamSyscall(SceUInt32 request, void *bootParam)
{
	return SceAppMgrForDriver_D2541B4A(request, bootParam);
}

int appmgrAppParamGetIntSyscall(int pid, int param, int *value)
{
	return SceAppMgrForDriver_12CE29F7(pid, param, value);
}

int appmgrAppParamGetStringSyscall(int pid, int param, char *string, SceSize length)
{
	return sceAppMgrAppParamGetString(pid, param, string, length);
}

int appmgrGetSystemDataFilePlayReadySyscall(SceUInt32 location, void *information,
					  void *userData, SceSize dataSize)
{
	return appmgrReadPlayReadySystemData(location, information, userData, dataSize);
}

int appmgrSetSystemDataFilePlayReadySyscall(SceUInt32 location,
		const void *information, const void *userData, SceSize dataSize)
{
	return appmgrWritePlayReadySystemData(location, information, userData, dataSize);
}

int appmgrSetSystemDataFileSyscall(SceUInt32 location, const void *userData,
				 SceSize dataSize)
{
	return appmgrSetSystemDataFile(location, userData, dataSize);
}

int appmgrGetNameByIdSyscall(SceUID id, char *appName)
{
	return sceAppMgrGetNameById(id, appName);
}

int appmgrGetIdByNameSyscall(SceUID *id, const char *appName)
{
	return sceAppMgrGetIdByName(id, appName);
}

int appmgrGetAppStateSyscall(SceAppMgrAppState *appState, SceSize size,
			   SceUInt32 buildVersion)
{
	return _sceAppMgrGetAppState(appState, size, buildVersion);
}

int appmgrReceiveSystemEventSyscall(SceAppMgrSystemEvent *event)
{
	return sceAppMgrReceiveSystemEvent(event);
}

int appmgrSetNetworkDisconnectionWarningDialogStateSyscall(SceBool enable,
							const void *optParam)
{
	return sceAppMgrSetNetworkDisconnectionWarningDialogState(enable, optParam);
}

int appmgrGetCurrentBgmStateSyscall(void *currentBgmId, void *currentBgmState)
{
	return sceAppMgrGetCurrentBgmState(currentBgmId, currentBgmState);
}

int appmgrGetCurrentBgmState2Syscall(void *bgmState)
{
	return sceAppMgrGetCurrentBgmState2(bgmState);
}

int appmgrSendLiveBoardModeSyscall(int mode)
{
	return sceAppMgrSendLiveBoardMode(mode);
}

int appmgrNotifyLiveBoardModeForShellSyscall(int appId, int mode)
{
	return sceAppMgrNotifyLiveBoardModeForShell(appId, mode);
}

SceUID appmgrGetProcessIdByAppIdForShellSyscall(SceInt32 appId)
{
	return sceAppMgrGetProcessIdByAppIdForShell(appId);
}

int appmgrGetStatusByIdSyscall(SceUID id, void *appStatus)
{
	return sceAppMgrGetStatusById(id, appStatus);
}

int appmgrGetStatusByAppIdSyscall(int appId, void *appStatus)
{
	return sceAppMgrGetStatusByAppId(appId, appStatus);
}

int appmgrGetStatusByNameSyscall(const char *appName, void *appStatus)
{
	return sceAppMgrGetStatusByName(appName, appStatus);
}

int appmgrReceiveNotificationRequestForShellSyscall(AppmgrNotificationRequest *request)
{
	return _appmgrQueuePop(request);
}

int appmgrPeekLaunchParamForShellSyscall(int appId, char *param)
{
	return sceAppMgrPeekLaunchParamForShell(appId, param);
}

int appmgrOverwriteLaunchParamForShellSyscall(int appId, const char *param)
{
	return sceAppMgrOverwriteLaunchParamForShell(appId, param);
}

int appmgrGetAppIdByAppIdSyscall(int appId)
{
	return sceAppMgrGetAppIdByAppId(appId);
}

int appmgrGetAppParamSyscall(char *param)
{
	return _sceAppMgrGetParam(param);
}

int appmgrGetAppParam2Syscall(void *appParam)
{
	return sceAppMgrGetAppParam2(appParam);
}

int appmgrGetAppMgrStateSyscall(void *appMgrState)
{
	return sceAppMgrGetAppMgrState(appMgrState);
}

int appmgrGetAppInfoSyscall(const char *appName, SceAppMgrAppState *appInfo)
{
	return sceAppMgrGetAppInfo(appName, appInfo);
}

int appmgrGetFgAppInfoSyscall(void *appInfo)
{
	return sceAppMgrGetFgAppInfo(appInfo);
}

int appmgrGetRecommendedScreenOrientationSyscall(int *orientation)
{
	return sceAppMgrGetRecommendedScreenOrientation(orientation);
}

int appmgrGetRunningAppIdListForShellSyscall(SceInt32 *appIds, int count)
{
	return sceAppMgrGetRunningAppIdListForShell(appIds, count);
}
