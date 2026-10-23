/* Application query and event syscall dispatch. */

#include <appmgr.h>
#include "appmgr_event.h"
#include <appmgr/appmgr.h>
#include "app_state_internal.h"
#include "appmgr_device.h"
#include "appmgr_app_event.h"
#include "mount.h"
#include "appmgr_savedata_slot.h"

extern int sceAppMgrAppParamGetString(int pid, int param, char *string, SceSize length);
extern SceInt32 _sceAppMgrGetAppState(SceAppMgrAppState *appState,
				      SceUInt32 sizeofSceAppMgrAppState, SceUInt32 buildVersion);


int appmgrIsGameProgramSyscall(void)
{
	return sceAppMgrIsGameProgram();
}

int appmgrIsNonGameProgramSyscall(void)
{
	return sceAppMgrIsNonGameProgram();
}

int appmgrIsPsNowClientSyscall(void)
{
	return sceAppMgrIsPsNowClient();
}

int appmgrIsDevelopmentModeSyscall(void)
{
	return sceAppMgrIsDevelopmentMode();
}

int appmgrGetBootParamSyscall(SceUInt32 request, void *bootParam)
{
	return SceAppMgrForDriver_D2541B4A(request, bootParam);
}

int appmgrAppMountSyscall(const struct AppmgrAppMountParam *param)
{
	return SceAppMgrForDriver_EF0C9533(param);
}

int appmgrAppUmountSyscall(const struct AppmgrAppUmountParam *param)
{
	return SceAppMgrForDriver_1DE3B7C4(param);
}

int appmgrAppParamGetIntSyscall(int pid, int param, int *value)
{
	return SceAppMgrForDriver_12CE29F7(pid, param, value);
}

int appmgrAppParamGetStringSyscall(int pid, int param, char *string, SceSize length)
{
	return sceAppMgrAppParamGetString(pid, param, string, length);
}

int appmgrAppParamSetStringSyscall(int param, const char *string)
{
	return sceAppMgrAppParamSetString(param, string);
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

int appmgrPspSaveDataGetParamsSyscall(struct AppmgrPspSaveDataParamSfo *paramSfo)
{
	return sceAppMgrPspSaveDataGetParams(paramSfo);
}

int appmgrPspSaveDataReadSyscall(struct AppmgrPspSaveDataParams *params)
{
	return sceAppMgrPspSaveDataRead(params);
}

int appmgrLaunchApp2Syscall(const char *appPath, const char *name, SceUInt32 budgetSize,
			    const char *param, SceUInt32 processAttr,
			    const AppmgrLaunchAppOption *option)
{
	return _sceAppMgrLaunchApp2(appPath, name, budgetSize, param, processAttr, option);
}

int appmgrLaunchAppByNameSyscall(SceUInt32 launchMode, const SceChar8 *name,
				 const char *param)
{
	return appmgrLaunchAppByName(launchMode, name, param);
}

int appmgrLaunchAppByNameForShellSyscall(SceUInt32 launchMode, const SceChar8 *name,
					 const char *param)
{
	return appmgrLaunchAppByNameForShell(launchMode, name, param);
}

int appmgrLaunchAppByUriSyscall(SceUInt32 launchMode, const char *uri)
{
	return appmgrLaunchAppByUri(launchMode, uri);
}

int appmgrLaunchAppByUri2Syscall(const char *uri, const AppmgrLaunchAppByNameOption *option)
{
	return appmgrLaunchAppByUri2(uri, option);
}

int appmgrLaunchVideoStreamingAppSyscall(const void *extraParam, SceSize extraParamSize,
					  const AppmgrLaunchAppByNameOption *option)
{
	return appmgrLaunchVideoStreamingApp(extraParam, extraParamSize, option);
}

int appmgrLaunchAppByName2Syscall(const SceChar8 *name, const char *param,
				  const AppmgrLaunchAppByNameOption *option)
{
	return appmgrLaunchAppByName2(name, param, option);
}

int appmgrLaunchAppByName2ForShellSyscall(const SceChar8 *name, const char *param,
					  const AppmgrLaunchAppByNameOption *option)
{
	return appmgrLaunchAppByName2ForShell(name, param, option);
}

int appmgrLaunchAppByName2ndStageSyscall(int appId, const char *appPath, SceUInt32 budgetSize,
					 SceUInt32 processAttr, const AppmgrLaunchAppOption *option)
{
	return sceAppMgrLaunchAppByName2ndStage(appId, appPath, budgetSize, processAttr, option);
}

int appmgrBgdlGetQueueStatusSyscall(void *status)
{
	return appmgrBgdlGetQueueStatus(status);
}

int appmgrGetNameByIdSyscall(SceUID id, char *appName)
{
	return sceAppMgrGetNameById(id, appName);
}

int appmgrGetIdByNameSyscall(SceUID *id, const char *appName)
{
	return sceAppMgrGetIdByName(id, appName);
}

int appmgrSendParamSyscall(const char *name, const char *param, const void *option)
{
	return sceAppMgrSendParam(name, param, option);
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

int appmgrDestroyAppByNameSyscall(const char *name)
{
	return sceAppMgrDestroyAppByName(name);
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

int appmgrCaptureFrameBufIFTUByAppIdSyscall(int appId, struct SceDisplayFrameBuf *frameBuf)
{
	return sceAppMgrCaptureFrameBufIFTUByAppId(appId, frameBuf);
}

int appmgrCaptureFrameBufDMACByAppIdSyscall(int appId, struct SceDisplayFrameBuf *frameBuf)
{
	return sceAppMgrCaptureFrameBufDMACByAppId(appId, frameBuf);
}

int appmgrGetAppParamSyscall(char *param)
{
	return _sceAppMgrGetParam(param);
}

int appmgrGetAppParam2Syscall(void *appParam)
{
	return sceAppMgrGetAppParam2(appParam);
}

int appmgrGetExtraAppParamSyscall(AppmgrExtraAppParam *extraParam)
{
	return sceAppMgrGetExtraAppParam(extraParam);
}

int appmgrGetAppMgrStateSyscall(void *appMgrState)
{
	return sceAppMgrGetAppMgrState(appMgrState);
}

int appmgrGetBudgetInfoSyscall(struct SceKernelBudgetInfo *info)
{
	return sceAppMgrGetBudgetInfo(info);
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
