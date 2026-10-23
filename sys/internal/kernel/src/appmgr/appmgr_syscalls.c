/* Kernel entry adapters called by generated user-copy exports. */

#include "appmgr_device.h"
#include "appmgr_event.h"
#include "mount.h"
#include <appmgr/appmgr.h>
#include "app_state_internal.h"

int appmgrSystemParamDateTimeGetConfSyscall(void *conf)
{
	return appmgrSystemParamDateTimeGetConf(conf);
}

int appmgrSystemParamGetIntSyscall(int parameter, int *value)
{
	return appmgrSystemParamGetInt(parameter, value);
}

int appmgrSystemParamGetStringSyscall(int parameter, char *value, SceSize size)
{
	return appmgrSystemParamGetString(parameter, value, size);
}

int appmgrGameDataMountSyscall(const char *appPath, const char *patchPath,
			       const char *rifPath, ScePfsRndDriveId *mountPoint)
{
	return SceAppMgrForDriver_CE356B2D(appPath, patchPath, rifPath, mountPoint);
}

int appmgrAppDataMountSyscall(unsigned int mountId, ScePfsRndDriveId *mountPoint)
{
	return SceAppMgrForDriver_B1D3C287(mountId, mountPoint);
}

int appmgrWorkDirMountSyscall(unsigned int mountId, ScePfsRndDriveId *mountPoint)
{
	return SceAppMgrForDriver_3A0A9B82(mountId, mountPoint);
}

int appmgrPspSaveDataRootMountSyscall(ScePfsRndDriveId *mountPoint)
{
	return sceAppMgrPspSaveDataRootMount(mountPoint);
}

int appmgrTrophyMountSyscall(unsigned int mountId, SceUID pid, ScePfsRndDriveId *mountPoint)
{
	return SceAppMgrForDriver_E977A833(mountId, pid, mountPoint);
}

int appmgrMmsMountSyscall(unsigned int mountId, ScePfsRndDriveId *mountPoint)
{
	return SceAppMgrForDriver_13D54107(mountId, mountPoint);
}

int appmgrAppDataMountByIdSyscall(unsigned int mountId, const char *id,
				  ScePfsRndDriveId *mountPoint)
{
	return SceAppMgrForDriver_5E311F71(mountId, id, mountPoint);
}

int appmgrInstallDirMountSyscall(unsigned int mountId, const char *id, const void *keystone,
				 ScePfsRndDriveId *mountPoint)
{
	return sceAppMgrInstallDirMount(mountId, id, keystone, mountPoint);
}

int appmgrLoopBackMountSyscall(unsigned int mountId, const char *id,
			       ScePfsRndDriveId *mountPoint)
{
	return sceAppMgrLoopBackMount(mountId, id, mountPoint);
}

int appmgrCountPfsMountReferencesSyscall(unsigned int mountId, int flags)
{
	return appmgrCountPfsMountReferences(mountId, flags);
}

int appmgrGetVs0UserModuleDriveSyscall(char *mountPoint)
{
	return appmgrGetVs0UserModuleDrive(mountPoint);
}

int appmgrGetVs0UserDataDriveSyscall(char *mountPoint)
{
	return appmgrGetVs0UserDataDrive(mountPoint);
}

int appmgrCheckRifGDSyscall(const char *path, AppmgrLicenseInfo *info)
{
	return appmgrCheckRifGD(path, info);
}

int appmgrDestroyOtherAppByPidForShellSyscall(SceUID pid)
{
	return sceAppMgrDestroyOtherAppByPidForShell(pid);
}

int appmgrDestroyOtherAppByAppIdForShellSyscall(int appId)
{
	return sceAppMgrDestroyOtherAppByAppIdForShell(appId);
}

int appmgrSuspendUntilActivatedSyscall(void)
{
	return sceAppMgrSuspendUntilActivated();
}

int appmgrAcquireBgmPortSyscall(void)
{
	return sceAppMgrAcquireBgmPort();
}

int appmgrAcquireBgmPortForMusicPlayerSyscall(void)
{
	return sceAppMgrAcquireBgmPortForMusicPlayer();
}

int appmgrReleaseBgmPortSyscall(void)
{
	return sceAppMgrReleaseBgmPort();
}

int appmgrAcquireBgmPortWithPrioritySyscall(int priority)
{
	return sceAppMgrAcquireBgmPortWithPriority(priority);
}

int appmgrSetBgmSubPrioritySyscall(int enabled, int priority)
{
	return sceAppMgrSetBgmSubPriority(enabled, priority);
}

int appmgrSetBgmSubPriorityForSystemChatSyscall(int enabled)
{
	return sceAppMgrSetBgmSubPriorityForSystemChat(enabled);
}

int appmgrEnablePrioritizingSystemChatSyscall(int enabled)
{
	return sceAppMgrEnablePrioritizingSystemChat(enabled);
}

int appmgrEnableDuckingOnSystemChatSyscall(int enabled, SceUInt32 volume)
{
	return sceAppMgrEnableDuckingOnSystemChat(enabled, volume);
}

int appmgrSendNotificationRequest(const AppmgrNotificationRequest *request, int wait)
{
	return _appmgrQueuePush(request, wait);
}

int sceAppMgrRestoreDisplaySettingForShell(void)
{
	return appmgrRestoreDisplaySettingForShell();
}

int sceAppMgrRestoreBgmSettingForShell(void)
{
	return appmgrRestoreBgmSettingForShell();
}

int appmgrLaunchAppCancelSyscall(int appId)
{
	return sceAppMgrLaunchAppCancel(appId);
}

int sceAppMgrAcquireSoundOutExclusive(int priority, SceUInt32 displayEnabled, SceUInt32 displayMode)
{
	return appmgrAcquireSoundOutExclusive(priority, displayEnabled, displayMode);
}

int sceAppMgrReleaseSoundOutExclusive(void)
{
	return appmgrReleaseSoundOutExclusive();
}

int sceAppMgrAcquireSoundOutExclusive2(int priority, SceUInt32 displayEnabled, SceUInt32 displayMode, int displayRequest)
{
	return appmgrAcquireSoundOutExclusive2(priority, displayEnabled, displayMode, displayRequest);
}

int sceAppMgrReleaseSoundOutExclusive2(void)
{
	return appmgrReleaseSoundOutExclusive2();
}

int appmgrAcquireSoundOutExclusive3Syscall(int priority, SceUInt32 displayEnabled,
		SceUInt32 displayMode, int displayBlanked, SceUInt32 unsupportedOptions)
{
	return appmgrAcquireSoundOutExclusive3(priority, displayEnabled,
		displayMode, displayBlanked, unsupportedOptions);
}

int sceAppMgrReleaseSoundOutExclusive3(void)
{
	return appmgrReleaseSoundOutExclusive3();
}

int sceAppMgrActivateApp(int appId)
{
	return appmgrActivateApp(appId);
}

int sceAppMgrDeactivateApp(int appId)
{
	return appmgrDeactivateApp(appId);
}

int appmgrDeclareSystemChatAppSyscall(void)
{
	return sceAppMgrDeclareSystemChatApp();
}

int appmgrSetBgmProxyAppSyscall(const char *appName)
{
	return sceAppMgrSetBgmProxyApp(appName);
}

int sceAppMgrSuspendBgAppByShell(int appId)
{
	return appmgrSuspendBgAppByShell(appId);
}

int sceAppMgrResumeBgAppByShell(int appId)
{
	return appmgrResumeBgAppByShell(appId);
}

int appmgrAcquireBtrmSyscall(void)
{
	return sceAppMgrAcquireBtrm();
}

int appmgrReleaseBtrmSyscall(void)
{
	return sceAppMgrReleaseBtrm();
}

int appmgrGetCoredumpStateForShellSyscall(AppmgrShellCoredumpState *state)
{
	return sceAppMgrGetCoredumpStateForShell(state);
}

int appmgrFinishCoredumpForShellSyscall(void)
{
	return sceAppMgrFinishCoredumpForShell();
}

int appmgrEnableCoredumpForTestSyscall(void)
{
	return sceAppMgrEnableCoredumpForTest();
}

int sceAppMgrReturnLiveAreaOperationResultForShell(int operationResult)
{
	return appmgrReturnLiveAreaOperationResultForShell(operationResult);
}

int sceAppMgrIsPidShellAndCrashed(SceUID pid)
{
	return appmgrIsPidShellAndCrashed(pid);
}

int appmgrSetDisplayMergeConfSyscall(SceUInt32 setting, SceUInt32 alpha)
{
	return sceAppMgrSetDisplayMergeConf(setting, alpha);
}

int appmgrSetSystemImposeStateSyscall(int state)
{
	return sceAppMgrSetSystemImposeState(state);
}

int appmgrSetSystemImposeState2Syscall(int state)
{
	return sceAppMgrSetSystemImposeState2(state);
}

int appmgrSetInfobarStateSyscall(int visibility, int color, int transparency)
{
	return sceAppMgrSetInfobarState(visibility, color, transparency);
}

int appmgrSetInfobarStateForCommonDialogSyscall(int visibility, int color, int transparency)
{
	return sceAppMgrSetInfobarStateForCommonDialog(visibility, color, transparency);
}

int appmgrSetInfobarStateForShellByAppIdSyscall(int appId, int visibility, int color,
		int transparency)
{
	return sceAppMgrSetInfobarStateForShellByAppId(appId, visibility, color, transparency);
}
