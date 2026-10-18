/*
 * SceAppMgr remaining export stubs.
 *
 * Generated (T2). Every body is a placeholder.
 */

#include <skeleton.h>
#include <psp2kern/types.h>
#include <intrmgr/syscall_copy.h>
#include <string.h>
#include "appmgr_device.h"
#include "appmgr_event.h"
#include <appmgr/appmgr.h>

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
	return copyResult(ret, SceSysmemForDriver_6D88EF8A(value, buffer, length));
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

STUB(SceAppMgrForDriver_088670A6)
STUB(SceAppMgrForDriver_0B94BB90)
STUB(sceAppMgrKillAppByNameForDebugger)
STUB(SceAppMgrForDriver_1286AF64)
STUB(SceAppMgrForDriver_12CE29F7)
STUB(sceAppMgrFakeSaveDataCreateMount)
STUB(SceAppMgrForDriver_13D54107)
STUB(SceAppMgrForDriver_1574E1A4)
STUB(SceAppMgrForDriver_193784A9)
STUB(sceAppMgrLaunchAppByNameForDebugger)
STUB(SceAppMgrForDriver_1DE3B7C4)
STUB(sceAppMgrGetPidByNameForDebugger)
STUB(SceAppMgrForDriver_2B080268)
STUB(SceAppMgrForDriver_2FAA6ABD)
STUB(SceAppMgrForDriver_30947DF4)
STUB(SceAppMgrForDriver_3A0A9B82)
STUB(SceAppMgrForDriver_3B4F305B)
STUB(SceAppMgrForDriver_43A35729)
STUB(sceAppMgrAcInstGetAcdirParam)
STUB(sceAppMgrGetAppIdByNameForDebugger)
STUB(sceAppMgrLaunchAppForTest2)
STUB(SceAppMgrForDriver_55D48190)
STUB(SceAppMgrForDriver_5E311F71)
STUB(SceAppMgrForDriver_61B13981)
STUB(SceAppMgrForDriver_62E98C42)
STUB(sceAppMgrSaveDataNotifyBackupFinished)
STUB(sceAppMgrCloudDataSrcMount)
STUB(sceAppMgrUpdateRifInfo)
STUB(SceAppMgrForDriver_99AA37C1)
STUB(SceAppMgrForDriver_9B3F7D66)
STUB(SceAppMgrForDriver_A714BB35)
STUB(sceAppMgrIsExclusiveProcessRunning)
STUB(sceAppMgrGetSystemDataFile)
STUB_FUNC(SceUID, sceAppMgrLaunchAppForDebugger, (const char *path, const char *args, SceSize argSize,
	  unsigned int flags, const SceAppMgrLaunchParam *param, void *opt))
STUB(SceAppMgrForDriver_B1D3C287)
STUB(sceAppMgrCloudDataDstCreateMount)
STUB(SceAppMgrForDriver_B53FB98E)
STUB(SceAppMgrForDriver_C4EF95BB)
STUB(SceAppMgrForDriver_CE356B2D)
STUB(SceAppMgrForDriver_D137D486)
STUB(SceAppMgrForDriver_D2541B4A)
STUB(sceAppMgrSaveSafeMemory)
STUB_FUNC(int, sceAppMgrKillAppForDebugger, (SceUID pid))
STUB(SceAppMgrForDriver_DC13160A)
STUB(SceAppMgrForDriver_DCADB9FE)
STUB(SceAppMgrForDriver_DD5BEEE3)
STUB(SceAppMgrForDriver_E104C37E)
STUB(sceAppMgrGetPfsProcessStatus)
STUB(SceAppMgrForDriver_E73C9516)
STUB(SceAppMgrForDriver_E977A833)
STUB(SceAppMgrForDriver_EA75D157)
STUB(SceAppMgrForDriver_EBFE1BE2)
STUB(SceAppMgrForDriver_EC408FEF)
STUB(SceAppMgrForDriver_EC9D5A35)
STUB(SceAppMgrForDriver_ED87F191)
STUB(SceAppMgrForDriver_EF0C9533)
STUB(sceAppMgrLoadSafeMemory)
STUB(sceAppMgrDestroyAppForTest)
STUB(_sceAppMgrGetCoredumpStateForShell)
STUB(_sceAppMgrInitSafeMemoryById)
STUB(_sceAppMgrAddContMount)

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

STUB(_sceAppMgrAppUmount)
STUB(sceAppMgrEnableCoredumpForTest)
STUB(_sceAppMgrLoopBackMount)
STUB(sceAppMgrIsOtherAppPresent)
STUB(_sceAppMgrLaunchAppByUri)
extern int appmgrReleaseSoundOutExclusive(void);

STUB(_sceAppMgrTrophyMountById)

STUB(sceAppMgrDestroyOtherApp)
STUB(_sceAppMgrSaveDataDataRemove2)
STUB(sceAppMgrActivateApp)
STUB(sceAppMgrGrowMemory3)

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

STUB(sceAppMgrSuspendBgAppByShell)
STUB(sceAppMgrReturnLiveAreaOperationResultForShell)
STUB(sceAppMgrIsNonGameProgram)
extern int appmgrReleaseSoundOutExclusive3(void);

STUB(sceAppMgrResumeBgAppByShell)
STUB(_sceAppMgrGetSaveDataInfo)
STUB(_sceAppMgrAcidDirSet)

STUB(sceAppMgrSetRecommendedScreenOrientationActivated)
STUB(_sceAppMgrPspSaveDataGetParams)

STUB(sceAppMgrIsDevelopmentMode)
STUB(sceAppMgrGrowMemory)
STUB(_sceAppMgrSaveDataSlotFileOpen)
STUB(sceAppMgrExitToLiveboardForGameApp)
STUB(_sceAppMgrLaunchAppByPath4)

STUB(_sceAppMgrAppDataMountById)
STUB(sceAppMgrGetExtraAppParam)
STUB(_sceAppMgrSaveDataMount)
STUB(sceAppMgrIsPsNowClient)
STUB(_sceAppMgrSaveDataDataRemove)
STUB(_sceAppMgrLoadSaveDataSystemFile)
STUB(_sceAppMgrPspSaveDataRootMount)
STUB(_sceAppMgrDrmOpen)

extern int appmgrAcquireSoundOutExclusive(int priority, SceUInt32 displayEnabled, SceUInt32 displayMode);

STUB(sceAppMgrSetInfobarState)
STUB(_sceAppMgrLaunchAppByName2)
STUB(sceAppMgrSetAppProtectionModeOnMemoryShortage)

STUB(_sceAppMgrPhotoUmount)
STUB(_sceAppMgrSaveDataDataSave)
extern int appmgrAcquireSoundOutExclusive2(int priority, SceUInt32 displayEnabled, SceUInt32 displayMode, int displayRequest);

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
