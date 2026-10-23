/*
 * SceAppMgr remaining export stubs.
 *
 * Generated (T2). Every body is a placeholder.
 */

#include <skeleton.h>
#include <scetypes.h>
#include <intrmgr/syscall_copy.h>
#include <string.h>
#include <sysmem/sysclib.h>
#include "appmgr_device.h"
#include "appmgr_event.h"
#include <appmgr/appmgr.h>
#include <appmgr/appmgr_savedata.h>
#include "app_state_internal.h"

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
	return copyResult(ret, sceKernelCopyToUser(value, buffer, length));
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
STUB_FUNC(int, SceAppMgrForDriver_0B94BB90, (int a, const void *id))
STUB_FUNC(int, SceAppMgrForDriver_1286AF64, (int a, void *id))
STUB_FUNC(int, SceAppMgrForDriver_12CE29F7, (int pid, int param, int *value))
STUB_FUNC(int, sceAppMgrFakeSaveDataCreateMount, (const char *path, void *b))
STUB(SceAppMgrForDriver_13D54107)
STUB(sceAppMgrRdrfpMount)
STUB_FUNC(int, SceAppMgrForDriver_193784A9, (void *a))
STUB(SceAppMgrForDriver_1DE3B7C4)
STUB(SceAppMgrForDriver_2B080268)
STUB_FUNC(int, SceAppMgrForDriver_2FAA6ABD, (void *key))
STUB(SceAppMgrForDriver_30947DF4)
STUB(SceAppMgrForDriver_3A0A9B82)
STUB(SceAppMgrForDriver_43A35729)
STUB_FUNC(int, sceAppMgrAcInstGetAcdirParam, (const void *a, const void *b, void *c))
STUB_FUNC(int, SceAppMgrForDriver_55D48190, (const void *header))
STUB(SceAppMgrForDriver_5E311F71)
STUB_FUNC(int, SceAppMgrForDriver_61B13981, (void *id, void *b))
STUB_FUNC(int, SceAppMgrForDriver_62E98C42, (const void *header))
STUB_FUNC(int, sceAppMgrSaveDataNotifyBackupFinished, (const void *a))
STUB_FUNC(int, sceAppMgrCloudDataSrcMount, (int a, const void *b, void *c))
STUB_FUNC(int, sceAppMgrUpdateRifInfo, (const void *info))
STUB_FUNC(int, SceAppMgrForDriver_9B3F7D66, (void *list))
STUB(SceAppMgrForDriver_A714BB35)
STUB_FUNC(int, sceAppMgrIsExclusiveProcessRunning, (const void *a))
STUB(sceAppMgrGetSystemDataFile)
STUB(SceAppMgrForDriver_B1D3C287)
STUB_FUNC(int, sceAppMgrCloudDataDstCreateMount, (const void *a, void *b))
STUB_FUNC(int, SceAppMgrForDriver_B53FB98E, (const char *path, void *a))
STUB_FUNC(int, SceAppMgrForDriver_C4EF95BB, (const void *item))
STUB(SceAppMgrForDriver_CE356B2D)
STUB_FUNC(int, SceAppMgrForDriver_D137D486, (SceAppMgrSaveDataSlot *slot))
STUB(sceAppMgrSaveSafeMemory)
STUB(SceAppMgrForDriver_DCADB9FE)
STUB(SceAppMgrForDriver_E104C37E)
STUB_FUNC(int, sceAppMgrGetPfsProcessStatus, (void))
STUB(SceAppMgrForDriver_E73C9516)
STUB(SceAppMgrForDriver_E977A833)
STUB(SceAppMgrForDriver_EA75D157)
STUB(SceAppMgrForDriver_EBFE1BE2)
STUB_FUNC(int, SceAppMgrForDriver_EC408FEF, (int a))
STUB_FUNC(int, SceAppMgrForDriver_EC9D5A35, (void *status))
STUB_FUNC(int, SceAppMgrForDriver_ED87F191, (const void *a, int b, const void *c, void *d))
STUB(SceAppMgrForDriver_EF0C9533)
STUB(sceAppMgrLoadSafeMemory)
STUB(_sceAppMgrInitSafeMemoryById)
STUB(_sceAppMgrAddContMount)

STUB(_sceAppMgrUmountByPid)
STUB(_sceAppMgrWorkDirMount)

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

STUB(_sceAppMgrAppUmount)
STUB(_sceAppMgrLoopBackMount)
STUB(sceAppMgrIsOtherAppPresent)
STUB(_sceAppMgrLaunchAppByUri)

STUB(_sceAppMgrTrophyMountById)

STUB(_sceAppMgrSaveDataDataRemove2)

STUB(_sceAppMgrGameDataMount)
STUB(_sceAppMgrGetPfsDrive)
STUB(_sceAppMgrLaunchAppByUri2)
STUB(_sceAppMgrLaunchAppByNameForShell)
STUB(_sceAppMgrBgdlGetQueueStatus)
STUB(_sceAppMgrGetDevInfo)
STUB(_sceAppMgrSaveDataUmount)
STUB(sceAppMgrSaveDataGetCachedRequiredSizeKiB)
STUB(_sceAppMgrAddContAddMount)
STUB(_sceAppMgrWorkDirMountById)
STUB(_sceAppMgrAppParamSetString)

STUB(sceAppMgrIsNonGameProgram)

STUB(_sceAppMgrGetSaveDataInfo)
STUB(_sceAppMgrAcidDirSet)

STUB(sceAppMgrSetRecommendedScreenOrientationActivated)
STUB(_sceAppMgrPspSaveDataGetParams)

STUB(sceAppMgrIsDevelopmentMode)
STUB(_sceAppMgrSaveDataSlotFileOpen)
STUB(_sceAppMgrLaunchAppByPath4)

STUB(_sceAppMgrAppDataMountById)
STUB(sceAppMgrGetExtraAppParam)
STUB(_sceAppMgrSaveDataMount)
STUB(sceAppMgrIsPsNowClient)
STUB(_sceAppMgrSaveDataDataRemove)
STUB(_sceAppMgrLoadSaveDataSystemFile)
STUB(_sceAppMgrPspSaveDataRootMount)
STUB(_sceAppMgrDrmOpen)

STUB(sceAppMgrSetInfobarState)
STUB(_sceAppMgrLaunchAppByName2)

STUB(_sceAppMgrPhotoUmount)
STUB(_sceAppMgrSaveDataDataSave)

STUB(_sceAppMgrDeclareShellProcess2)
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
STUB(_sceAppMgrLaunchAppByName)
STUB(_sceAppMgrLoadExec)
STUB(_sceAppMgrContentInstallPeriodStart)
STUB(_sceAppMgrGetSaveDataInfoForSpecialExport)
STUB(_sceAppMgrLaunchAppByName2ndStage)
STUB(sceAppMgrIsGameBudgetAppPresent)
STUB(_sceAppMgrDrmClose)
STUB(_sceAppMgrPspSaveDataRead)
STUB(_sceAppMgrMmsMount)
STUB(_sceAppMgrPhotoMount)
STUB(_sceAppMgrLoopBackFormat)
STUB(_sceAppMgrGetBudgetInfo)
STUB(_sceAppMgrDestroyAppByName)
STUB(_sceAppMgrInstallDirMount)
STUB(_sceAppMgrAppDataMount)
STUB(_sceAppMgrSaveDataGetQuota)
STUB(_sceAppMgrCaptureFrameBufIFTUByAppId)

int _sceAppMgrSendNotificationRequest(const AppmgrNotificationRequest *request, int wait)
{
	AppmgrNotificationRequest *kernelRequest = NULL;
	int ret;

	BLOCK_IN(kernelRequest, request, sizeof(*kernelRequest));
	return appmgrSendNotificationRequest(kernelRequest, wait);
}
STUB(_sceAppMgrUpdateSaveDataParam)

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
STUB(_sceAppMgrUmount)
STUB(sceAppMgrIsGameProgram)
STUB(sceSharedFbClose)
STUB(sceSharedFbBegin)
STUB(sceSharedFbGetInfo)
STUB(_sceSharedFbOpen)
STUB(sceSharedFbEnd)
