/*
 * SceAppMgr remaining export stubs.
 *
 * Generated (T2). Every body is a placeholder.
 */

#include "skeleton.h"
#include <psp2kern/types.h>
#include <sce/syscall_copy.h>
#include <string.h>
#include "appmgr_device.h"
#include "appmgr_event.h"

extern int appmgrSystemParamGetIntSyscall(int parameter, int *value);
extern int appmgrSystemParamGetStringSyscall(int parameter, char *value, SceSize size);
extern int appmgrSystemParamDateTimeGetConfSyscall(void *conf);
extern int appmgrCountPfsMountReferencesSyscall(unsigned int mountId, int flags);
extern int appmgrGetVs0UserModuleDriveSyscall(char *mountPoint);
extern int appmgrGetVs0UserDataDriveSyscall(char *mountPoint);

int _sceAppMgrSystemParamDateTimeGetConf(void *conf)
{
	int ret;

	BLOCK_OUT(conf, 20,
		  appmgrSystemParamDateTimeGetConfSyscall(NULL),
		  appmgrSystemParamDateTimeGetConfSyscall(k));
}

int _sceAppMgrSystemParamGetInt(int parameter, int *value)
{
	int ret;

	BLOCK_OUT(value, sizeof(*value),
		  appmgrSystemParamGetIntSyscall(parameter, NULL),
		  appmgrSystemParamGetIntSyscall(parameter, (int *)k));
}

int _sceAppMgrSystemParamGetString(int parameter, char *value, SceSize size,
				 const SyscallUnusedArgs *userArguments)
{
	SyscallUnusedArgs arguments;
	char *buffer;
	SceSize length;
	int ret;

	ARGS_IN(arguments, userArguments);
	if (value == NULL)
		return appmgrSystemParamGetStringSyscall(parameter, NULL, size);
	buffer = __builtin_alloca(1024);
	memset(buffer, 0, 1024);
	ret = appmgrSystemParamGetStringSyscall(parameter, buffer, size);
	length = strnlen(buffer, 1024) + 1;
	if (length >= 1024)
		length = 1024;
	return copyResult(ret, ksceKernelCopyToUser(value, buffer, length));
}

int _sceAppMgrGetMountProcessNum(unsigned int mountId, const char *mountPoint,
			       const SyscallUnusedArgs *userArguments)
{
	SyscallUnusedArgs arguments;
	char *path = NULL;
	int ret;

	ARGS_IN(arguments, userArguments);
	STRING_IN(path, mountPoint, 64);
	return appmgrCountPfsMountReferencesSyscall(mountId, (int)(SceUIntPtr)path);
}

int _sceAppMgrGetVs0UserModuleDrive(char *mountPoint)
{
	int ret;

	BLOCK_OUT(mountPoint, 16,
		  appmgrGetVs0UserModuleDriveSyscall(NULL),
		  appmgrGetVs0UserModuleDriveSyscall((char *)k));
}

int _sceAppMgrGetVs0UserDataDrive(char *mountPoint)
{
	int ret;

	BLOCK_OUT(mountPoint, 16,
		  appmgrGetVs0UserDataDriveSyscall(NULL),
		  appmgrGetVs0UserDataDriveSyscall((char *)k));
}

STUB(ksceAppMgrDrmClose)
STUB(ksceAppMgrCloudDataSetMcId)
STUB(sceAppMgrKillAppByNameForDebugger)
STUB(ksceAppMgrCloudDataGetMcId)
STUB(ksceAppMgrAppParamGetInt)
STUB(ksceAppMgrFakeSaveDataCreateMount)
STUB(ksceAppMgrMmsMount)
STUB(SceAppMgrForDriver_1574E1A4)
STUB(ksceAppMgrCheckContentInstallPeriod)
STUB(sceAppMgrLaunchAppByNameForDebugger)
STUB(ksceAppMgrAppUmount)
STUB(sceAppMgrGetPidByNameForDebugger)
STUB(ksceAppMgrSaveDataUmount)
STUB(ksceAppMgrCloudDataSetupKey)
STUB(SceAppMgrForDriver_30947DF4)
STUB(ksceAppMgrWorkDirMount)
STUB(SceAppMgrForDriver_3B4F305B)
STUB(ksceAppMgrPhotoMount)
STUB(ksceAppMgrAcInstGetAcdirParam)
STUB(sceAppMgrGetAppIdByNameForDebugger)
STUB(sceAppMgrLaunchAppForTest2)
STUB(ksceAppMgrCloudDataVerifyHeader)
STUB(ksceAppMgrAppDataMountById)
STUB(ksceAppMgrLocalBackupGetOfflineId)
STUB(ksceAppMgrLocalBackupVerifyOfflineHeader)
STUB(ksceAppMgrSaveDataNotifyBackupFinished)
STUB(ksceAppMgrCloudDataSrcMount)
STUB(ksceAppMgrUpdateRifInfo)
STUB(SceAppMgrForDriver_99AA37C1)
STUB(ksceAppMgrSaveDataLocalBackupTargetGetList)
STUB(ksceAppMgrUmount)
STUB(ksceAppMgrIsExclusiveProcessRunning)
STUB(sceAppMgrGetSystemDataFile)
STUB(ksceAppMgrLaunchAppByPath)
STUB(ksceAppMgrAppDataMount)
STUB(ksceAppMgrCloudDataDstCreateMount)
STUB(ksceAppMgrRegisterPath)
STUB(ksceAppMgrSaveDataLocalBackupTargetRemoveItem)
STUB(ksceAppMgrGameDataMount)
STUB(ksceAppMgrSaveDataSlotGetParam)
STUB(ksceAppMgrGetBootParam)
STUB(sceAppMgrSaveSafeMemory)
STUB(ksceAppMgrKillProcess)
STUB(SceAppMgrForDriver_DC13160A)
STUB(SceAppMgrForDriver_DCADB9FE)
STUB(SceAppMgrForDriver_DD5BEEE3)
STUB(ksceAppMgrSaveDataGetQuota)
STUB(ksceAppMgrGetPfsProcessStatus)
STUB(ksceAppMgrSaveDataMount)
STUB(ksceAppMgrTrophyMount)
STUB(ksceAppMgrDrmOpen)
STUB(SceAppMgrForDriver_EBFE1BE2)
STUB(ksceAppMgrCloudDataClearMcId)
STUB(ksceAppMgrSaveDataSlotGetStatus)
STUB(ksceAppMgrCloudDataCreateHeader)
STUB(ksceAppMgrAppMount)
STUB(sceAppMgrLoadSafeMemory)
STUB(ksceAppMgrDestroyOtherAppByPid)
STUB(_sceAppMgrGetCoredumpStateForShell)
STUB(_sceAppMgrInitSafeMemoryById)
STUB(_sceAppMgrAddContMount)
extern int sceAppMgrSetDisplayMergeConf(SceUInt32 setting, SceUInt32 alpha);

extern int appmgrSetDisplayMergeConfSyscall(SceUInt32 setting, SceUInt32 alpha);
STUB(_sceAppMgrUmountByPid)
STUB(_sceAppMgrWorkDirMount)
STUB(sceAppMgrQuitApp)
extern int appmgrAcquireSoundOutExclusive3(int priority, SceUInt32 displayEnabled,
		SceUInt32 displayMode, int displayBlanked, SceUInt32 unsupportedOptions);

extern int appmgrAcquireSoundOutExclusive3Syscall(int priority, SceUInt32 displayEnabled,
		SceUInt32 displayMode, int displayBlanked, SceUInt32 unsupportedOptions);

int _sceAppMgrAcquireSoundOutExclusive3(int priority, SceUInt32 displayEnabled,
		SceUInt32 displayMode, const void *userArguments)
{
	AppmgrSoundOutExclusiveArguments arguments;
	int ret;

	ARGS_IN(arguments, userArguments);
	return appmgrAcquireSoundOutExclusive3Syscall(priority, displayEnabled,
		displayMode, arguments.displayBlanked, arguments.unsupportedOptions);
}
STUB(_sceAppMgrCheckRifGD)
STUB(sceAppMgrSetSystemImposeState2)
STUB(_sceAppMgrForceUmount)
STUB(_sceAppMgrLaunchVideoStreamingApp)
STUB(_sceAppMgrGetPidListForShell)
extern int appmgrReleaseSoundOutExclusive2(void);

extern int sceAppMgrReleaseSoundOutExclusive2(void);
STUB(_sceAppMgrAppUmount)
STUB(sceAppMgrEnableCoredumpForTest)
STUB(_sceAppMgrLoopBackMount)
STUB(sceAppMgrIsOtherAppPresent)
STUB(_sceAppMgrLaunchAppByUri)
extern int appmgrReleaseSoundOutExclusive(void);

extern int sceAppMgrReleaseSoundOutExclusive(void);
STUB(_sceAppMgrTrophyMountById)
extern int sceAppMgrEnablePrioritizingSystemChat(int enabled);

extern int appmgrEnablePrioritizingSystemChatSyscall(int enabled);
STUB(sceAppMgrDestroyOtherApp)
STUB(_sceAppMgrSaveDataDataRemove2)
STUB(sceAppMgrActivateApp)
STUB(sceAppMgrGrowMemory3)
extern int sceAppMgrAcquireBgmPortForMusicPlayer(void);

extern int appmgrAcquireBgmPortForMusicPlayerSyscall(void);
STUB(_sceAppMgrGameDataMount)
STUB(_sceAppMgrGetPfsDrive)
STUB(_sceAppMgrLaunchAppByUri2)
STUB(_sceAppMgrLaunchAppByNameForShell)
STUB(_sceAppMgrBgdlGetQueueStatus)
STUB(_sceAppMgrGetDevInfo)
STUB(sceAppMgrFinishCoredumpForShell)
STUB(_sceAppMgrSaveDataUmount)
STUB(sceAppMgrLaunchAppCancel)
STUB(sceAppMgrDestroyOtherAppByPidForShell)
STUB(sceAppMgrDestroyAppByAppId)
STUB(sceAppMgrSaveDataGetCachedRequiredSizeKiB)
STUB(_sceAppMgrAddContAddMount)
STUB(_sceAppMgrWorkDirMountById)
STUB(_sceAppMgrAppParamSetString)
extern int sceAppMgrEnableDuckingOnSystemChat(int enabled, SceUInt32 volume);

extern int appmgrEnableDuckingOnSystemChatSyscall(int enabled, SceUInt32 volume);
STUB(sceAppMgrSuspendBgAppByShell)
STUB(sceAppMgrReturnLiveAreaOperationResultForShell)
STUB(sceAppMgrIsNonGameProgram)
extern int appmgrReleaseSoundOutExclusive3(void);

extern int sceAppMgrReleaseSoundOutExclusive3(void);
STUB(sceAppMgrResumeBgAppByShell)
STUB(_sceAppMgrGetSaveDataInfo)
STUB(_sceAppMgrAcidDirSet)
extern int sceAppMgrSetBgmSubPriority(int enabled, int priority);

extern int appmgrSetBgmSubPrioritySyscall(int enabled, int priority);
STUB(sceAppMgrSetRecommendedScreenOrientationActivated)
STUB(_sceAppMgrPspSaveDataGetParams)
extern int sceAppMgrDeclareSystemChatApp(void);

extern int appmgrDeclareSystemChatAppSyscall(void);
STUB(sceAppMgrIsDevelopmentMode)
STUB(sceAppMgrGrowMemory)
STUB(_sceAppMgrSaveDataSlotFileOpen)
STUB(sceAppMgrExitToLiveboardForGameApp)
STUB(_sceAppMgrLaunchAppByPath4)
extern int sceAppMgrSetBgmSubPriorityForSystemChat(int enabled);

extern int appmgrSetBgmSubPriorityForSystemChatSyscall(int enabled);
STUB(_sceAppMgrAppDataMountById)
STUB(sceAppMgrGetExtraAppParam)
STUB(_sceAppMgrSaveDataMount)
STUB(sceAppMgrIsPsNowClient)
STUB(_sceAppMgrSaveDataDataRemove)
STUB(_sceAppMgrLoadSaveDataSystemFile)
STUB(_sceAppMgrPspSaveDataRootMount)
STUB(_sceAppMgrDrmOpen)
extern int sceAppMgrAcquireBgmPortWithPriority(int priority);

extern int appmgrAcquireBgmPortWithPrioritySyscall(int priority);
extern int appmgrAcquireSoundOutExclusive(int priority, SceUInt32 displayEnabled, SceUInt32 displayMode);

extern int sceAppMgrAcquireSoundOutExclusive(int priority, SceUInt32 displayEnabled, SceUInt32 displayMode);
extern int sceAppMgrReleaseBtrm(void);

extern int appmgrReleaseBtrmSyscall(void);
STUB(sceAppMgrSetInfobarState)
STUB(_sceAppMgrLaunchAppByName2)
STUB(sceAppMgrSetAppProtectionModeOnMemoryShortage)
extern int sceAppMgrAcquireBgmPort(void);

extern int appmgrAcquireBgmPortSyscall(void);
STUB(_sceAppMgrPhotoUmount)
STUB(_sceAppMgrSaveDataDataSave)
extern int appmgrAcquireSoundOutExclusive2(int priority, SceUInt32 displayEnabled, SceUInt32 displayMode, int displayRequest);

extern int sceAppMgrAcquireSoundOutExclusive2(int priority, SceUInt32 displayEnabled, SceUInt32 displayMode, int displayRequest);
STUB(_sceAppMgrDeclareShellProcess2)
STUB(sceAppMgrIsPidShellAndCrashed)
STUB(_sceAppMgrCaptureFrameBufDMACByAppId)
STUB(_sceAppMgrLaunchAppByName2ForShell)
STUB(_sceAppMgrGetMediaTypeFromDriveByPid)
STUB(_sceAppMgrGetMediaTypeFromDrive)
STUB(_sceAppMgrContentInstallPeriodStop)
STUB(_sceAppMgrAppMount)
STUB(_sceAppMgrSaveDataAddMount)
STUB(sceAppMgrSetSystemImposeState)
STUB(_sceAppMgrSendParam)
STUB(sceAppMgrSetInfobarStateForCommonDialog)
extern int sceAppMgrAcquireBtrm(void);

extern int appmgrAcquireBtrmSyscall(void);
STUB(_sceAppMgrThemeDataMount)
STUB(sceAppMgrRestoreBgmSettingForShell)
STUB(_sceAppMgrLaunchAppByName)
STUB(_sceAppMgrLoadExec)
STUB(_sceAppMgrContentInstallPeriodStart)
STUB(sceAppMgrDestroyOtherAppByAppIdForShell)
STUB(_sceAppMgrGetSaveDataInfoForSpecialExport)
STUB(_sceAppMgrLaunchAppByName2ndStage)
STUB(sceAppMgrIsGameBudgetAppPresent)
STUB(sceAppMgrQuitForNonSuspendableApp)
STUB(sceAppMgrRestoreDisplaySettingForShell)
STUB(_sceAppMgrDrmClose)
STUB(_sceAppMgrPspSaveDataRead)
STUB(_sceAppMgrMmsMount)
STUB(_sceAppMgrPhotoMount)
STUB(_sceAppMgrLoopBackFormat)
STUB(_sceAppMgrGetBudgetInfo)
STUB(_sceAppMgrDestroyAppByName)
STUB(sceAppMgrSuspendUntilActivated)
STUB(_sceAppMgrInstallDirMount)
STUB(_sceAppMgrAppDataMount)
STUB(_sceAppMgrSaveDataGetQuota)
STUB(_sceAppMgrCaptureFrameBufIFTUByAppId)
extern int sceAppMgrReleaseBgmPort(void);

extern int appmgrReleaseBgmPortSyscall(void);
extern int appmgrSendNotificationRequest(const AppmgrNotificationRequest *request, int wait);

int _sceAppMgrSendNotificationRequest(const AppmgrNotificationRequest *request, int wait)
{
	AppmgrNotificationRequest *kernelRequest = NULL;
	int ret;

	BLOCK_IN(kernelRequest, request, sizeof(*kernelRequest));
	return appmgrSendNotificationRequest(kernelRequest, wait);
}
STUB(_sceAppMgrUpdateSaveDataParam)
extern int sceAppMgrSetBgmProxyApp(const char *appName);

extern int appmgrSetBgmProxyAppSyscall(const char *appName);

int _sceAppMgrSetBgmProxyApp(const char *appName, const SyscallUnusedArgs *userArguments)
{
	SyscallUnusedArgs arguments;
	char *name = NULL;
	int ret;

	ARGS_IN(arguments, userArguments);
	STRING_IN(name, appName, 32);
	return appmgrSetBgmProxyAppSyscall(name);
}
STUB(sceAppMgrSetInfobarStateForShellByAppId)
STUB(_sceAppMgrTrophyMount)
STUB(_sceAppMgrSaveDataDataSave2)
STUB(_sceAppMgrSaveDataSlotFileClose)
STUB(sceAppMgrDeactivateApp)
STUB(_sceAppMgrUmount)
STUB(sceAppMgrIsGameProgram)
STUB(sceSharedFbClose)
STUB(sceSharedFbBegin)
STUB(sceSharedFbGetInfo)
STUB(_sceSharedFbOpen)
STUB(sceSharedFbEnd)
