/*
 * SceDriverUser: SceAppMgrUser, the user front end of the AppMgr syscalls.
 *
 * Most calls pass straight through. The others hand the kernel a
 * SceSyscallArgs block: a parameter beyond the fourth, or the size of a
 * parameter block or output list the kernel reads or fills.
 */

#include "driver_user.h"

int __sceAppMgrGetAppState(void *state, SceSize size, SceUInt32 version);
int _sceAppMgrReceiveSystemEvent(void *event);
int _sceAppMgrSetNetworkDisconnectionWarningDialogState(int state, void *param);
int _sceAppMgrLoadExec(const char *path, char *const argv[], const void *optParam, SceSyscallArgs2 *args);
int _sceAppMgrGetAppInfo(const char *name, void *info, SceSyscallArgs2 *args);
int _sceAppMgrGetFgAppInfo(void *info);
int _sceAppMgrGetBootParam(int reserved, SceSizedParam *param, SceSyscallArgs2 *args);
int _sceAppMgrSystemParamDateTimeGetConf(void *conf);
int _sceAppMgrSystemParamGetInt(int id, int *value);
int _sceAppMgrSystemParamGetString(int id, char *buf, SceSize bufSize, SceSyscallArgs2 *args);
int _sceAppMgrAppMount(SceSizedParam *param, SceSyscallArgs2 *args);
int _sceAppMgrAppUmount(SceSizedParam *param, SceSyscallArgs2 *args);
int _sceAppMgrAppParamGetInt(SceUID pid, int id, int *value);
int _sceAppMgrAppParamGetString(SceUID pid, int id, char *buf, SceSyscallArgs4 *args);
int _sceAppMgrAppParamSetString(SceUID pid, int id, SceSyscallArgs2 *args);
int _sceAppMgrSaveDataSlotInit(SceSizedParam *param, SceSyscallArgs2 *args);
int _sceAppMgrSaveDataSlotCreate(SceSizedParam *param, SceSyscallArgs2 *args);
int _sceAppMgrSaveDataSlotDelete(SceSizedParam *param, SceSyscallArgs2 *args);
int _sceAppMgrSaveDataSlotSetParam(SceSizedParam *param, SceSyscallArgs2 *args);
int _sceAppMgrSaveDataSlotGetParam(SceSizedParam *param, SceSyscallArgs2 *args);
int _sceAppMgrSaveDataSlotSetStatus(SceSizedParam *param, SceSyscallArgs2 *args);
int _sceAppMgrSaveDataSlotGetStatus(SceSizedParam *param, SceSyscallArgs2 *args);
int _sceAppMgrSaveDataDataSave(SceSizedParam *param, SceSyscallArgs2 *args);
int _sceAppMgrSaveDataDataSave2(SceSizedParam *param, SceSyscallArgs2 *args);
int _sceAppMgrSaveDataDataRemove(SceSizedParam *param, SceSyscallArgs2 *args);
int _sceAppMgrSaveDataDataRemove2(SceSizedParam *param, SceSyscallArgs2 *args);
int _sceAppMgrSaveDataGetQuota(SceSizedParam *param, SceSyscallArgs2 *args);
int _sceAppMgrSaveDataMount(SceSizedParam *param, SceSyscallArgs2 *args);
int _sceAppMgrSaveDataUmount(SceSizedParam *param, SceSyscallArgs2 *args);
int _sceAppMgrSaveDataSlotFileOpen(void *param, void *fd);
int _sceAppMgrSaveDataSlotFileGetParam(void *param, void *out);
int _sceAppMgrSaveDataSlotFileClose(void *param);
int _sceAppMgrSaveDataAddMount(void *param);
int _sceAppMgrPhotoMount(SceSizedParam *param, SceSyscallArgs2 *args);
int _sceAppMgrPhotoUmount(SceSizedParam *param, SceSyscallArgs2 *args);
int _sceAppMgrAppDataMount(int id, char *mountPoint);
int _sceAppMgrAppDataMountById(int id, const char *titleId, char *mountPoint, SceSyscallArgs2 *args);
int _sceAppMgrWorkDirMount(int id, char *mountPoint);
int _sceAppMgrPspSaveDataRootMount(char *mountPoint);
int _sceAppMgrWorkDirMountById(int id, const char *titleId, char *mountPoint, SceSyscallArgs2 *args);
int _sceAppMgrTrophyMount(int id, const char *titleId, char *mountPoint);
int _sceAppMgrTrophyMountById(int id, const char *titleId, char *mountPoint, SceSyscallArgs2 *args);
int _sceAppMgrMmsMount(int id, char *mountPoint);
int _sceAppMgrInstallDirMount(int id, char *mountPoint);
int _sceAppMgrAcidDirSet(const void *acid, const char *path, SceSyscallArgs2 *args);
int _sceAppMgrGameDataMount(const char *appPath, const char *patchPath, const char *rifPath,
			    SceSyscallArgs6 *args);
int _sceAppMgrGetMountProcessNum(const char *mountPoint, int *num, SceSyscallArgs2 *args);
int _sceAppMgrLoopBackMount(int mountId, const char *path, char *mountPoint, SceSyscallArgs2 *args);
int _sceAppMgrThemeDataMount(const char *path0, const char *path1, const char *path2,
			     SceSyscallArgs6 *args);
int _sceAppMgrForceUmount(const char *mountPoint);
int _sceAppMgrUmount(const char *mountPoint);
int _sceAppMgrUmountByPid(SceUID pid, const char *mountPoint);
int _sceAppMgrLoopBackFormat(int mountId, const char *path, const char *name, SceSyscallArgs4 *args);
int _sceAppMgrGetVs0UserModuleDrive(char *drive);
int _sceAppMgrGetVs0UserDataDrive(char *drive);
int _sceAppMgrConvertVs0UserDrivePath(const char *path, char *out, int outSize, SceSyscallArgs4 *args);
int _sceAppMgrGetRawPath(const char *path, char *out, int outSize, SceSyscallArgs4 *args);
int _sceAppMgrGetSaveDataInfo(const char *path, char *titleId, int forceMount, SceSyscallArgs4 *args);
int _sceAppMgrLoadSaveDataSystemFile(const char *path, void *buf, SceSize size, SceSyscallArgs2 *args);
int _sceAppMgrGetSaveDataInfoForSpecialExport(const char *path, void *info, SceSyscallArgs2 *args);
int _sceAppMgrGetMediaTypeFromDrive(const char *drive, int *type);
int _sceAppMgrGetMediaTypeFromDriveByPid(SceUID pid, const char *drive, int *type);
int _sceAppMgrCheckRifGD(const char *path, void *dest, SceSyscallArgs2 *args);
int _sceAppMgrUpdateSaveDataParam(void *param, SceSyscallArgs2 *args);
int _sceAppMgrGetDevInfo(const char *dev, SceUInt64 *maxSize, SceUInt64 *freeSize, SceSyscallArgs2 *args);
int _sceAppMgrGetPfsDrive(const char *path, char *drive);
int _sceAppMgrGetUserDirPath(int partitionId, char *path, SceSize size, SceSyscallArgs2 *args);
int _sceAppMgrGetUserDirPathById(int id, int type, char *path, SceSyscallArgs4 *args);
int _sceAppMgrInitSafeMemoryById(int id, SceSyscallArgs2 *args);
int _sceAppMgrAddContMount(int id, char *mountPoint);
int _sceAppMgrAddContAddMount(int id, char *mountPoint);
int _sceAppMgrDrmOpen(SceSizedParam *param, SceSyscallArgs2 *args);
int _sceAppMgrDrmClose(SceSizedParam *param, SceSyscallArgs2 *args);
int _sceAppMgrContentInstallPeriodStart(void);
int _sceAppMgrContentInstallPeriodStop(void);
int _sceAppMgrBgdlGetQueueStatus(void *status);
int _sceAppMgrPspSaveDataGetParams(void *param);
int _sceAppMgrPspSaveDataRead(void *param);
int _sceAppMgrGetSystemDataFilePlayReady(void *buf, SceSize size);
int _sceAppMgrSetSystemDataFilePlayReady(const void *buf, SceSize size);
int _sceAppMgrSetSystemDataFile(int id, const void *buf, SceSize size);
int _sceAppMgrLaunchAppByName(int flags, const char *name, const char *param, SceSyscallArgs4 *args);
int _sceAppMgrLaunchAppByNameForShell(int flags, const char *name, const char *param, SceSyscallArgs4 *args);
int _sceAppMgrLaunchAppByName2(const char *name, const char *param, void *optParam, SceSyscallArgs4 *args);
int _sceAppMgrLaunchAppByName2ForShell(const char *name, const char *param, void *optParam,
				       SceSyscallArgs4 *args);
int _sceAppMgrReceiveEventNum(int *num);
int _sceAppMgrReceiveEvent(void *event);
int _sceAppMgrDestroyAppByName(const char *name, SceSyscallArgs2 *args);
int _sceAppMgrGetAppParam(char *param, SceSyscallArgs2 *args);
int _sceAppMgrGetAppParam2(void *param);
int _sceAppMgrSendSystemEvent(SceUID pid, int event, SceSyscallArgs2 *args);
int _sceAppMgrSendSystemEvent2(SceUID pid, int event, const void *param, SceSyscallArgs4 *args);
int _sceAppMgrGetStatusByName(const char *name, void *status, SceSyscallArgs2 *args);
int _sceAppMgrGetStatusByAppId(int appId, void *status);
int _sceAppMgrSendParam(const char *name, const void *param, SceSize size, SceSyscallArgs4 *args);
int _sceAppMgrLaunchAppByUri(int flags, const char *uri, SceSyscallArgs2 *args);
int _sceAppMgrLaunchAppByUri2(const char *uri, void *optParam, SceSyscallArgs2 *args);
int _sceAppMgrDeclareShellProcess2(const char *name, SceSyscallArgs2 *args);
int _sceAppMgrGetCurrentBgmState(void *state, SceSize size);
int _sceAppMgrGetCurrentBgmState2(void *state);
int _sceAppMgrCaptureFrameBufIFTUByAppId(int appId, void *param);
int _sceAppMgrCaptureFrameBufDMACByAppId(int appId, void *param);
int _sceAppMgrGetAppMgrState(void *state);
int _sceAppMgrSetBgmProxyApp(const char *name, SceSyscallArgs2 *args);
int _sceAppMgrLaunchAppByPath4(const char *path, const char *titleId, int flags, SceSyscallArgs8 *args);
int _sceAppMgrLaunchAppByName2ndStage(int flags, const char *path, const char *param, SceSyscallArgs4 *args);
int _sceAppMgrLaunchVideoStreamingApp(int flags, const char *name, void *param);
int _sceAppMgrReceiveShellEvent(void *event);
int _sceAppMgrPeekLaunchParamForShell(int appId, void *param, SceSyscallArgs2 *args);
int _sceAppMgrOverwriteLaunchParamForShell(int appId, const void *param, SceSyscallArgs2 *args);
int _sceAppMgrSetBackRenderPortOwner(SceUID owner, int reserved);
int _sceAppMgrGetCoredumpStateForShell(void *state);
int _sceAppMgrGetPidListForShell(SceUID *pids, int count, SceSyscallArgs2 *args);
int _sceAppMgrReceiveNotificationRequestForShell(void *request);
int _sceAppMgrSendNotificationRequest(const void *request, int flags);
int _sceAppMgrGetRawPathOfApp0ByAppIdForShell(int appId, char *path);
int _sceAppMgrGetRawPathOfApp0ByPidForShell(SceUID pid, char *path);
int _sceAppMgrAcquireSoundOutExclusive3(int type, int reserved0, int reserved1, SceSyscallArgs4 *args);
int _sceAppMgrSetPowerSaveMode(int mode, int param);
int _sceAppMgrIsCameraActive(int device, int *active);
int _sceAppMgrSetShellScreenOrientation(int orientation);
int _sceAppMgrSetRecommendedScreenOrientationForShell(int orientation);
int _sceAppMgrGetRecommendedScreenOrientation(int *orientation);
int _sceAppMgrGetRunningAppIdListForShell(int *appIds, int count, SceSyscallArgs2 *args);
int _sceAppMgrGetBudgetInfo(void *info);
int _sceAppMgrGetStatusById(SceUID pid, void *status);
int _sceAppMgrGetNameById(SceUID pid, char *name, SceSyscallArgs2 *args);
int _sceAppMgrGetIdByName(SceUID *pid, const char *name, SceSyscallArgs2 *args);

/* the size word of an optional user parameter block */
static inline __attribute__((always_inline)) SceUInt32 ParamSize(const SceSizedParam *param)
{
	return param != NULL ? param->size : 0;
}

int _sceAppMgrGetAppState(void *state, SceSize size, SceUInt32 version)
{
	return __sceAppMgrGetAppState(state, size, version);
}

int sceAppMgrReceiveSystemEvent(void *event)
{
	return _sceAppMgrReceiveSystemEvent(event);
}

int sceAppMgrSetNetworkDisconnectionWarningDialogState(int state, void *param)
{
	return _sceAppMgrSetNetworkDisconnectionWarningDialogState(state, param);
}

int sceAppMgrLoadExec(const char *path, char *const argv[], const void *optParam)
{
	SceSyscallArgs2 args;

	return _sceAppMgrLoadExec(path, argv, optParam, &args);
}

int sceAppMgrGetAppInfo(const char *name, int info)
{
	SceSyscallArgs2 args;

	return _sceAppMgrGetAppInfo(name, info, &args);
}

int sceAppMgrGetFgAppInfo(void *info)
{
	return _sceAppMgrGetFgAppInfo(info);
}

int sceAppMgrGetBootParam(int reserved, SceSizedParam *param)
{
	SceSyscallArgs2 args;

	args.arg[0] = ParamSize(param);
	return _sceAppMgrGetBootParam(reserved, param, &args);
}

int sceAppMgrSystemParamDateTimeGetConf(void *conf)
{
	return _sceAppMgrSystemParamDateTimeGetConf(conf);
}

int sceAppMgrSystemParamGetInt(int id, int *value)
{
	return _sceAppMgrSystemParamGetInt(id, value);
}

int sceAppMgrSystemParamGetString(int id, char *buf, SceSize bufSize)
{
	SceSyscallArgs2 args;

	return _sceAppMgrSystemParamGetString(id, buf, bufSize, &args);
}

int sceAppMgrAppMount(SceSizedParam *param)
{
	SceSyscallArgs2 args;

	args.arg[0] = ParamSize(param);
	return _sceAppMgrAppMount(param, &args);
}

int sceAppMgrAppUmount(SceSizedParam *param)
{
	SceSyscallArgs2 args;

	args.arg[0] = ParamSize(param);
	return _sceAppMgrAppUmount(param, &args);
}

int sceAppMgrAppParamGetInt(SceUID pid, int id, int *value)
{
	return _sceAppMgrAppParamGetInt(pid, id, value);
}

int sceAppMgrAppParamGetString(SceUID pid, int id, char *buf, SceSize bufSize)
{
	SceSyscallArgs4 args;

	args.arg[0] = bufSize;
	return _sceAppMgrAppParamGetString(pid, id, buf, &args);
}

int sceAppMgrAppParamSetString(SceUID pid, int id)
{
	SceSyscallArgs2 args;

	return _sceAppMgrAppParamSetString(pid, id, &args);
}

#define SAVEDATA_CALL(name)						\
int sceAppMgr##name(SceSizedParam *param)				\
{									\
	SceSyscallArgs2 args;						\
									\
	args.arg[0] = ParamSize(param);					\
	return _sceAppMgr##name(param, &args);				\
}

SAVEDATA_CALL(SaveDataSlotInit)
SAVEDATA_CALL(SaveDataSlotCreate)
SAVEDATA_CALL(SaveDataSlotDelete)
SAVEDATA_CALL(SaveDataSlotSetParam)
SAVEDATA_CALL(SaveDataSlotGetParam)
SAVEDATA_CALL(SaveDataSlotSetStatus)
SAVEDATA_CALL(SaveDataSlotGetStatus)
SAVEDATA_CALL(SaveDataDataSave)
SAVEDATA_CALL(SaveDataDataSave2)
SAVEDATA_CALL(SaveDataDataRemove)
SAVEDATA_CALL(SaveDataDataRemove2)
SAVEDATA_CALL(SaveDataGetQuota)
SAVEDATA_CALL(SaveDataMount)
SAVEDATA_CALL(SaveDataUmount)

int sceAppMgrSaveDataSlotFileOpen(void *param, void *fd)
{
	return _sceAppMgrSaveDataSlotFileOpen(param, fd);
}

int sceAppMgrSaveDataSlotFileGetParam(void *param, void *out)
{
	return _sceAppMgrSaveDataSlotFileGetParam(param, out);
}

int sceAppMgrSaveDataSlotFileClose(void *param)
{
	return _sceAppMgrSaveDataSlotFileClose(param);
}

int sceAppMgrSaveDataAddMount(void *param)
{
	return _sceAppMgrSaveDataAddMount(param);
}

SAVEDATA_CALL(PhotoMount)
SAVEDATA_CALL(PhotoUmount)

int sceAppMgrAppDataMount(int id, char *mountPoint)
{
	return _sceAppMgrAppDataMount(id, mountPoint);
}

int sceAppMgrAppDataMountById(int id, const char *titleId, char *mountPoint)
{
	SceSyscallArgs2 args;

	return _sceAppMgrAppDataMountById(id, titleId, mountPoint, &args);
}

int sceAppMgrWorkDirMount(int id, char *mountPoint)
{
	return _sceAppMgrWorkDirMount(id, mountPoint);
}

int sceAppMgrPspSaveDataRootMount(char *mountPoint)
{
	return _sceAppMgrPspSaveDataRootMount(mountPoint);
}

int sceAppMgrWorkDirMountById(int id, const char *titleId, char *mountPoint)
{
	SceSyscallArgs2 args;

	return _sceAppMgrWorkDirMountById(id, titleId, mountPoint, &args);
}

int sceAppMgrTrophyMount(int id, const char *titleId, char *mountPoint)
{
	return _sceAppMgrTrophyMount(id, titleId, mountPoint);
}

int sceAppMgrTrophyMountById(int id, const char *titleId, char *mountPoint)
{
	SceSyscallArgs2 args;

	return _sceAppMgrTrophyMountById(id, titleId, mountPoint, &args);
}

int sceAppMgrMmsMount(int id, char *mountPoint)
{
	return _sceAppMgrMmsMount(id, mountPoint);
}

int sceAppMgrInstallDirMount(int id, char *mountPoint)
{
	return _sceAppMgrInstallDirMount(id, mountPoint);
}

int sceAppMgrAcidDirSet(const void *acid, const char *path)
{
	SceSyscallArgs2 args;

	return _sceAppMgrAcidDirSet(acid, path, &args);
}

int sceAppMgrGameDataMount(const char *appPath, const char *patchPath, const char *rifPath,
			   char *mountPoint)
{
	SceSyscallArgs6 args;

	args.arg[0] = (SceUInt32)mountPoint;
	return _sceAppMgrGameDataMount(appPath, patchPath, rifPath, &args);
}

int sceAppMgrGetMountProcessNum(const char *mountPoint, int *num)
{
	SceSyscallArgs2 args;

	return _sceAppMgrGetMountProcessNum(mountPoint, num, &args);
}

int sceAppMgrLoopBackMount(int mountId, const char *path, char *mountPoint)
{
	SceSyscallArgs2 args;

	return _sceAppMgrLoopBackMount(mountId, path, mountPoint, &args);
}

int sceAppMgrThemeDataMount(const char *path0, const char *path1, const char *path2,
			    char *mountPoint)
{
	SceSyscallArgs6 args;

	args.arg[0] = (SceUInt32)mountPoint;
	return _sceAppMgrThemeDataMount(path0, path1, path2, &args);
}

int sceAppMgrForceUmount(const char *mountPoint)
{
	return _sceAppMgrForceUmount(mountPoint);
}

int sceAppMgrUmount(const char *mountPoint)
{
	return _sceAppMgrUmount(mountPoint);
}

int sceAppMgrUmountByPid(SceUID pid, const char *mountPoint)
{
	return _sceAppMgrUmountByPid(pid, mountPoint);
}

int sceAppMgrLoopBackFormat(int mountId, const char *path, const char *name, int info)
{
	SceSyscallArgs4 args;

	args.arg[0] = info;
	return _sceAppMgrLoopBackFormat(mountId, path, name, &args);
}

int sceAppMgrGetVs0UserModuleDrive(char *drive)
{
	return _sceAppMgrGetVs0UserModuleDrive(drive);
}

int sceAppMgrGetVs0UserDataDrive(char *drive)
{
	return _sceAppMgrGetVs0UserDataDrive(drive);
}

int sceAppMgrConvertVs0UserDrivePath(const char *path, char *out, int outSize)
{
	SceSyscallArgs4 args;

	return _sceAppMgrConvertVs0UserDrivePath(path, out, outSize, &args);
}

int sceAppMgrGetRawPath(const char *path, char *out, int outSize)
{
	SceSyscallArgs4 args;

	return _sceAppMgrGetRawPath(path, out, outSize, &args);
}

int sceAppMgrGetSaveDataInfo(const char *path, char *titleId, int forceMount, int option)
{
	SceSyscallArgs4 args;

	args.arg[0] = option;
	return _sceAppMgrGetSaveDataInfo(path, titleId, forceMount, &args);
}

int sceAppMgrLoadSaveDataSystemFile(const char *path, void *buf, SceSize size)
{
	SceSyscallArgs2 args;

	return _sceAppMgrLoadSaveDataSystemFile(path, buf, size, &args);
}

int sceAppMgrGetSaveDataInfoForSpecialExport(const char *path, void *info)
{
	SceSyscallArgs2 args;

	return _sceAppMgrGetSaveDataInfoForSpecialExport(path, info, &args);
}

int sceAppMgrGetMediaTypeFromDrive(const char *drive, int *type)
{
	return _sceAppMgrGetMediaTypeFromDrive(drive, type);
}

int sceAppMgrGetMediaTypeFromDriveByPid(SceUID pid, const char *drive, int *type)
{
	return _sceAppMgrGetMediaTypeFromDriveByPid(pid, drive, type);
}

int sceAppMgrCheckRifGD(const char *path, void *dest)
{
	SceSyscallArgs2 args;

	return _sceAppMgrCheckRifGD(path, dest, &args);
}

int sceAppMgrUpdateSaveDataParam(void *param)
{
	SceSyscallArgs2 args;

	return _sceAppMgrUpdateSaveDataParam(param, &args);
}

int sceAppMgrGetDevInfo(const char *dev, SceUInt64 *maxSize, SceUInt64 *freeSize)
{
	SceSyscallArgs2 args;

	return _sceAppMgrGetDevInfo(dev, maxSize, freeSize, &args);
}

int sceAppMgrGetPfsDrive(const char *path, char *drive)
{
	return _sceAppMgrGetPfsDrive(path, drive);
}

int sceAppMgrGetUserDirPath(int partitionId, char *path, SceSize size)
{
	SceSyscallArgs2 args;

	return _sceAppMgrGetUserDirPath(partitionId, path, size, &args);
}

int sceAppMgrGetUserDirPathById(int id, int type, char *path, SceSize size)
{
	SceSyscallArgs4 args;

	args.arg[0] = size;
	return _sceAppMgrGetUserDirPathById(id, type, path, &args);
}

int sceAppMgrInitSafeMemoryById(int id)
{
	SceSyscallArgs2 args;

	return _sceAppMgrInitSafeMemoryById(id, &args);
}

int sceAppMgrAddContMount(int id, char *mountPoint)
{
	return _sceAppMgrAddContMount(id, mountPoint);
}

int sceAppMgrAddContAddMount(int id, char *mountPoint)
{
	return _sceAppMgrAddContAddMount(id, mountPoint);
}

SAVEDATA_CALL(DrmOpen)
SAVEDATA_CALL(DrmClose)

int sceAppMgrContentInstallPeriodStart(void)
{
	return _sceAppMgrContentInstallPeriodStart();
}

int sceAppMgrContentInstallPeriodStop(void)
{
	return _sceAppMgrContentInstallPeriodStop();
}

int sceAppMgrBgdlGetQueueStatus(void *status)
{
	return _sceAppMgrBgdlGetQueueStatus(status);
}

int sceAppMgrPspSaveDataGetParams(void *param)
{
	return _sceAppMgrPspSaveDataGetParams(param);
}

int sceAppMgrPspSaveDataRead(void *param)
{
	return _sceAppMgrPspSaveDataRead(param);
}

int sceAppMgrGetSystemDataFilePlayReady(void *buf, SceSize size)
{
	return _sceAppMgrGetSystemDataFilePlayReady(buf, size);
}

int sceAppMgrSetSystemDataFilePlayReady(const void *buf, SceSize size)
{
	return _sceAppMgrSetSystemDataFilePlayReady(buf, size);
}

int sceAppMgrSetSystemDataFile(int id, const void *buf, SceSize size)
{
	return _sceAppMgrSetSystemDataFile(id, buf, size);
}

int sceAppMgrLaunchAppByName(int flags, const char *name, const char *param)
{
	SceSyscallArgs4 args;

	return _sceAppMgrLaunchAppByName(flags, name, param, &args);
}

int sceAppMgrLaunchAppByNameForShell(int flags, const char *name, const char *param)
{
	SceSyscallArgs4 args;

	return _sceAppMgrLaunchAppByNameForShell(flags, name, param, &args);
}

int sceAppMgrLaunchAppByName2(const char *name, const char *param, void *optParam)
{
	SceSyscallArgs4 args;

	return _sceAppMgrLaunchAppByName2(name, param, optParam, &args);
}

int sceAppMgrLaunchAppByName2ForShell(const char *name, const char *param, void *optParam)
{
	SceSyscallArgs4 args;

	return _sceAppMgrLaunchAppByName2ForShell(name, param, optParam, &args);
}

int sceAppMgrReceiveEventNum(int *num)
{
	return _sceAppMgrReceiveEventNum(num);
}

int sceAppMgrReceiveEvent(void *event)
{
	return _sceAppMgrReceiveEvent(event);
}

int sceAppMgrDestroyAppByName(const char *name)
{
	SceSyscallArgs2 args;

	return _sceAppMgrDestroyAppByName(name, &args);
}

int sceAppMgrGetAppParam(char *param)
{
	SceSyscallArgs2 args;

	return _sceAppMgrGetAppParam(param, &args);
}

int sceAppMgrGetAppParam2(void *param)
{
	return _sceAppMgrGetAppParam2(param);
}

int sceAppMgrSendSystemEvent(SceUID pid, int event)
{
	SceSyscallArgs2 args;

	return _sceAppMgrSendSystemEvent(pid, event, &args);
}

int sceAppMgrSendSystemEvent2(SceUID pid, int event, const void *param)
{
	SceSyscallArgs4 args;

	return _sceAppMgrSendSystemEvent2(pid, event, param, &args);
}

int sceAppMgrGetStatusByName(const char *name, void *status)
{
	SceSyscallArgs2 args;

	return _sceAppMgrGetStatusByName(name, status, &args);
}

int sceAppMgrGetStatusByAppId(int appId, void *status)
{
	return _sceAppMgrGetStatusByAppId(appId, status);
}

int sceAppMgrSendParam(const char *name, const void *param, SceSize size)
{
	SceSyscallArgs4 args;

	return _sceAppMgrSendParam(name, param, size, &args);
}

int sceAppMgrLaunchAppByUri(int flags, const char *uri)
{
	SceSyscallArgs2 args;

	return _sceAppMgrLaunchAppByUri(flags, uri, &args);
}

int sceAppMgrLaunchAppByUri2(const char *uri, void *optParam)
{
	SceSyscallArgs2 args;

	return _sceAppMgrLaunchAppByUri2(uri, optParam, &args);
}

int sceAppMgrDeclareShellProcess2(const char *name)
{
	SceSyscallArgs2 args;

	return _sceAppMgrDeclareShellProcess2(name, &args);
}

int sceAppMgrGetCurrentBgmState(void *state, SceSize size)
{
	return _sceAppMgrGetCurrentBgmState(state, size);
}

int sceAppMgrGetCurrentBgmState2(void *state)
{
	return _sceAppMgrGetCurrentBgmState2(state);
}

int sceAppMgrCaptureFrameBufIFTUByAppId(int appId, void *param)
{
	return _sceAppMgrCaptureFrameBufIFTUByAppId(appId, param);
}

int sceAppMgrCaptureFrameBufDMACByAppId(int appId, void *param)
{
	return _sceAppMgrCaptureFrameBufDMACByAppId(appId, param);
}

int sceAppMgrGetAppMgrState(void *state)
{
	return _sceAppMgrGetAppMgrState(state);
}

int sceAppMgrSetBgmProxyApp(const char *name)
{
	SceSyscallArgs2 args;

	return _sceAppMgrSetBgmProxyApp(name, &args);
}

int sceAppMgrLaunchAppByPath4(const char *path, const char *titleId, int flags, int param,
			      int option, int opt)
{
	SceSyscallArgs8 args;

	args.arg[0] = param;
	args.arg[1] = option;
	args.arg[2] = opt;
	return _sceAppMgrLaunchAppByPath4(path, titleId, flags, &args);
}

int sceAppMgrLaunchAppByName2ndStage(int flags, const char *path, const char *param, int option,
				     int opt)
{
	SceSyscallArgs4 args;

	args.arg[0] = option;
	args.arg[1] = opt;
	return _sceAppMgrLaunchAppByName2ndStage(flags, path, param, &args);
}

int sceAppMgrLaunchVideoStreamingApp(int flags, const char *name, void *param)
{
	return _sceAppMgrLaunchVideoStreamingApp(flags, name, param);
}

int sceAppMgrReceiveShellEvent(void *event)
{
	return _sceAppMgrReceiveShellEvent(event);
}

int sceAppMgrPeekLaunchParamForShell(int appId, void *param)
{
	SceSyscallArgs2 args;

	return _sceAppMgrPeekLaunchParamForShell(appId, param, &args);
}

int sceAppMgrOverwriteLaunchParamForShell(int appId, const void *param)
{
	SceSyscallArgs2 args;

	return _sceAppMgrOverwriteLaunchParamForShell(appId, param, &args);
}

int sceAppMgrSetBackRenderPortOwner(SceUID owner, int reserved)
{
	return _sceAppMgrSetBackRenderPortOwner(owner, reserved);
}

int sceAppMgrGetCoredumpStateForShell(void *state)
{
	return _sceAppMgrGetCoredumpStateForShell(state);
}

int sceAppMgrGetPidListForShell(SceUID *pids, int count)
{
	SceSyscallArgs2 args;

	args.arg[0] = count * sizeof(SceUID);
	return _sceAppMgrGetPidListForShell(pids, count, &args);
}

int sceAppMgrReceiveNotificationRequestForShell(void *request)
{
	return _sceAppMgrReceiveNotificationRequestForShell(request);
}

int sceAppMgrSendNotificationRequest(const void *request, int flags)
{
	return _sceAppMgrSendNotificationRequest(request, flags);
}

int sceAppMgrGetRawPathOfApp0ByAppIdForShell(int appId, char *path)
{
	return _sceAppMgrGetRawPathOfApp0ByAppIdForShell(appId, path);
}

int sceAppMgrGetRawPathOfApp0ByPidForShell(SceUID pid, char *path)
{
	return _sceAppMgrGetRawPathOfApp0ByPidForShell(pid, path);
}

int sceAppMgrAcquireSoundOutExclusive3(int type, int reserved0, int reserved1, int reserved2,
				       int reserved3)
{
	SceSyscallArgs4 args;

	args.arg[0] = reserved2;
	args.arg[1] = reserved3;
	return _sceAppMgrAcquireSoundOutExclusive3(type, reserved0, reserved1, &args);
}

int sceAppMgrSetPowerSaveMode(int mode, int param)
{
	return _sceAppMgrSetPowerSaveMode(mode, param);
}

int sceAppMgrIsCameraActive(int device, int *active)
{
	return _sceAppMgrIsCameraActive(device, active);
}

int sceAppMgrSetShellScreenOrientation(int orientation)
{
	return _sceAppMgrSetShellScreenOrientation(orientation);
}

int sceAppMgrSetRecommendedScreenOrientationForShell(int orientation)
{
	return _sceAppMgrSetRecommendedScreenOrientationForShell(orientation);
}

int sceAppMgrGetRecommendedScreenOrientation(int *orientation)
{
	return _sceAppMgrGetRecommendedScreenOrientation(orientation);
}

int sceAppMgrGetRunningAppIdListForShell(int *appIds, int count)
{
	SceSyscallArgs2 args;

	args.arg[0] = count * sizeof(int);
	return _sceAppMgrGetRunningAppIdListForShell(appIds, count, &args);
}

int sceAppMgrGetBudgetInfo(void *info)
{
	return _sceAppMgrGetBudgetInfo(info);
}

int sceAppMgrGetStatusById(SceUID pid, void *status)
{
	return _sceAppMgrGetStatusById(pid, status);
}

int sceAppMgrGetNameById(SceUID pid, char *name)
{
	SceSyscallArgs2 args;

	return _sceAppMgrGetNameById(pid, name, &args);
}

int sceAppMgrGetIdByName(SceUID *pid, const char *name)
{
	SceSyscallArgs2 args;

	return _sceAppMgrGetIdByName(pid, name, &args);
}
