/* Dispatch for application and user-directory path queries. */

#include <scetypes.h>
#include <appmgr/appmgr.h>
#include "mount.h"
#include "app_state_internal.h"
#include "addcont_internal.h"

int appmgrForceUmountSyscall(int mountId)
{
	return sceAppMgrForceUmount(mountId);
}

int appmgrUmountSyscall(const char *driveId)
{
	return sceAppMgrUmount(driveId);
}

int appmgrUmountByPidSyscall(SceUID pid, const char *driveId)
{
	return sceAppMgrUmountByPid(pid, driveId);
}

int appmgrThemeDataMountSyscall(const char *contentId, const char *themePath,
				const char *rifPath, ScePfsRndDriveId *mountPoint)
{
	return sceAppMgrThemeDataMount(contentId, themePath, rifPath, mountPoint);
}

int appmgrConvertVs0UserDrivePathSyscall(const char *path, char *output,
	SceSize size)
{
	return appmgrConvertVs0UserDrivePath(path, output, size);
}

int appmgrGetRawPathSyscall(char *path, char *resolvedPath, int capacity)
{
	return sceAppMgrGetRawPath(path, resolvedPath, capacity);
}

int appmgrGetSaveDataInfoSyscall(const char *appPath, const char *titleId,
				 SceUInt32 attributes, AppmgrSaveDataInfo *info)
{
	return sceAppMgrGetSaveDataInfo(appPath, titleId, attributes, info);
}

int appmgrGetSaveDataInfoForSpecialExportSyscall(const char *path, SceUInt64 *accountId)
{
	return sceAppMgrGetSaveDataInfoForSpecialExport(path, accountId);
}

int appmgrLoadSaveDataSystemFileSyscall(const char *path, void *buffer, SceSize size)
{
	return sceAppMgrLoadSaveDataSystemFile(path, buffer, size);
}

int appmgrGetMediaTypeFromDriveSyscall(const char *drive, int *mediaType)
{
	return sceAppMgrGetMediaTypeFromDrive(drive, mediaType);
}

int appmgrGetMediaTypeFromDriveByPidSyscall(SceUID pid, const char *drive, int *mediaType)
{
	return sceAppMgrGetMediaTypeFromDriveByPid(pid, drive, mediaType);
}

int appmgrUpdateSaveDataParamSyscall(const char *path)
{
	return sceAppMgrUpdateSaveDataParam(path);
}

int appmgrGetDevInfoSyscall(const char *path, SceOff *maxSize, SceOff *freeSize)
{
	return sceAppMgrGetDevInfo(path, maxSize, freeSize);
}

int appmgrGetPfsDriveSyscall(const char *drive, char *pfsDrive)
{
	return sceAppMgrGetPfsDrive(drive, pfsDrive);
}

int appmgrGetUserDirPathSyscall(int partitionId, char *path,
	unsigned int capacity)
{
	return appmgrGetUserDirPathById(-1, partitionId, path, capacity);
}

int appmgrGetUserDirPathByIdSyscall(int userId, int partitionId, char *path,
	unsigned int capacity)
{
	return appmgrGetUserDirPathById(userId, partitionId, path, capacity);
}

int appmgrInitSafeMemoryByIdSyscall(const char *id)
{
	return sceAppMgrInitSafeMemoryById(id);
}

int appmgrGetSystemDataFileSyscall(SceUInt32 location, void *buffer, SceSize size)
{
	return sceAppMgrGetSystemDataFile(location, buffer, size);
}

int appmgrSaveSafeMemorySyscall(const void *data, SceSize size, SceOff offset)
{
	return sceAppMgrSaveSafeMemory(data, size, offset);
}

int appmgrLoadSafeMemorySyscall(void *data, SceSize size, SceOff offset)
{
	return sceAppMgrLoadSafeMemory(data, size, offset);
}

int appmgrAddContMountSyscall(SceUID pid, const AppmgrAddContMountParam *param)
{
	return sceAppMgrAddContMount(pid, param);
}

int appmgrAddContAddMountSyscall(const AppmgrAddMountParam *param, int add)
{
	return sceAppMgrAddContAddMount(param, add);
}

int appmgrDrmOpenSyscall(const SceAppMgrDrmOpenParam *param)
{
	return SceAppMgrForDriver_EA75D157(param);
}

int appmgrDrmCloseSyscall(const SceAppMgrDrmOpenParam *param)
{
	return SceAppMgrForDriver_088670A6(param);
}

int appmgrContentInstallPeriodStartSyscall(void)
{
	return sceAppMgrContentInstallPeriodStart();
}

int appmgrContentInstallPeriodStopSyscall(void)
{
	return sceAppMgrContentInstallPeriodStop();
}

int appmgrQuitAppSyscall(int appId)
{
	return sceAppMgrQuitApp(appId);
}

int appmgrDumpSyscall(void)
{
	return sceAppMgrDump();
}

int appmgrLoadExecSyscall(const char *path, const char *const *argv, const void *option)
{
	return sceAppMgrLoadExec(path, argv, option);
}

int appmgrGetRawPathOfApp0ByAppIdForShellSyscall(int appId, char *path)
{
	return sceAppMgrGetRawPathOfApp0ByAppIdForShell(appId, path);
}

int appmgrGetRawPathOfApp0ByPidForShellSyscall(SceUID pid, char *path)
{
	return sceAppMgrGetRawPathOfApp0ByPidForShell(pid, path);
}
