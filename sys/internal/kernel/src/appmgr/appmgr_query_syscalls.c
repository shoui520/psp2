/* Application query and event syscall dispatch. */

#include <appmgr.h>
#include "appmgr_event.h"
#include <appmgr/appmgr.h>
#include "app_state_internal.h"
#include "appmgr_device.h"
#include "appmgr_app_event.h"

extern int sceAppMgrAppParamGetString(int pid, int param, char *string, SceSize length);
extern SceInt32 _sceAppMgrGetAppState(SceAppMgrAppState *appState,
				      SceUInt32 sizeofSceAppMgrAppState, SceUInt32 buildVersion);

extern int _sceAppMgrGetParam(char *param);
extern int sceAppMgrGetAppParam2(void *appParam);

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

int appmgrGrowMemorySyscall(SceSize size, int killMiniApps)
{
	return sceAppMgrGrowMemory(size, killMiniApps);
}

int appmgrGrowMemory3Syscall(SceSize size, unsigned int thresholdClass)
{
	return sceAppMgrGrowMemory3(size, thresholdClass);
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

int appmgrExitToLiveboardForGameAppSyscall(void)
{
	return sceAppMgrExitToLiveboardForGameApp();
}

int appmgrSendLiveBoardModeSyscall(int mode)
{
	return sceAppMgrSendLiveBoardMode(mode);
}

int appmgrNotifyLiveBoardModeForShellSyscall(int appId, int mode)
{
	return sceAppMgrNotifyLiveBoardModeForShell(appId, mode);
}

int appmgrDestroyAppByAppIdSyscall(int appId)
{
	return sceAppMgrDestroyAppByAppId(appId);
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

int appmgrIsOtherAppPresentSyscall(void)
{
	return sceAppMgrIsOtherAppPresent();
}

int appmgrIsGameBudgetAppPresentSyscall(void)
{
	return sceAppMgrIsGameBudgetAppPresent();
}

int appmgrDestroyOtherAppSyscall(void)
{
	return sceAppMgrDestroyOtherApp();
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

int appmgrSetAppProtectionModeOnMemoryShortageSyscall(unsigned int mode)
{
	return sceAppMgrSetAppProtectionModeOnMemoryShortage(mode);
}

int appmgrGetAppInfoSyscall(const char *appName, SceAppMgrAppState *appInfo)
{
	return sceAppMgrGetAppInfo(appName, appInfo);
}

int appmgrGetFgAppInfoSyscall(void *appInfo)
{
	return sceAppMgrGetFgAppInfo(appInfo);
}

int appmgrQuitForNonSuspendableAppSyscall(void)
{
	return sceAppMgrQuitForNonSuspendableApp();
}

int appmgrGetRecommendedScreenOrientationSyscall(int *orientation)
{
	return sceAppMgrGetRecommendedScreenOrientation(orientation);
}

int appmgrGetRunningAppIdListForShellSyscall(SceInt32 *appIds, int count)
{
	return sceAppMgrGetRunningAppIdListForShell(appIds, count);
}

int appmgrSetRecommendedScreenOrientationActivatedSyscall(int activated)
{
	return sceAppMgrSetRecommendedScreenOrientationActivated(activated);
}
