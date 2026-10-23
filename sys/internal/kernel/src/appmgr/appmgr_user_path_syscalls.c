/* User-memory arguments and outputs for application path queries. */

#include <intrmgr/syscall_copy.h>
#include <string.h>
#include <sysmem/sysclib.h>
#include <appmgr/appmgr.h>
#include "app_state_internal.h"
#include "appmgr_savedata_slot.h"
#include "mount.h"
#include <threadmgr/threadmgr.h>

typedef struct AppmgrRawPathArguments {
	unsigned char reserved[16];
} AppmgrRawPathArguments;

_Static_assert(sizeof(AppmgrRawPathArguments) == 16,
	"raw path argument block");

typedef struct AppmgrUserDirPathArguments {
	SceSize capacity;
	unsigned char reserved[12];
} AppmgrUserDirPathArguments;

_Static_assert(sizeof(AppmgrUserDirPathArguments) == 16,
	"user directory argument block");

typedef struct AppmgrInitSafeMemoryArguments {
	unsigned char reserved[8];
} AppmgrInitSafeMemoryArguments;

int _sceAppMgrForceUmount(int mountId)
{
	return appmgrForceUmountSyscall(mountId);
}

int _sceAppMgrUmount(const char *driveId)
{
	char *buffer = NULL;
	int ret;

	BLOCK_IN(buffer, driveId, 16);
	return appmgrUmountSyscall(buffer);
}

int _sceAppMgrUmountByPid(SceUID pid, const char *driveId)
{
	char *buffer = (char *)driveId;
	int ret;

	BLOCK_IN(buffer, driveId, 16);
	return appmgrUmountByPidSyscall(pid, buffer);
}

int _sceAppMgrAcidDirSet(const char *userPath, const char *userDirectory,
			 const SyscallUnusedArgs *userArguments)
{
	SyscallUnusedArgs arguments;
	char *path = (char *)userPath;
	char *directory = (char *)userDirectory;
	int ret;

	ARGS_IN(arguments, userArguments);
	BLOCK_IN(path, userPath, 16);
	STRING_IN(directory, userDirectory, 292);
	return appmgrSetPfsAcidDirectorySyscall(path, directory);
}

int _sceAppMgrConvertVs0UserDrivePath(const char *path, char *convertedPath,
	SceSize capacity, const AppmgrRawPathArguments *userArguments)
{
	AppmgrRawPathArguments arguments;
	char *name = NULL;
	char *buffer;
	SceSize length;
	int ret;

	ARGS_IN(arguments, userArguments);
	STRING_IN(name, path, 292);
	if (convertedPath == NULL)
		return appmgrConvertVs0UserDrivePathSyscall(name, NULL, capacity);
	buffer = __builtin_alloca(292);
	__builtin_memset(buffer, 0, 292);
	ret = appmgrConvertVs0UserDrivePathSyscall(name, buffer, capacity);
	length = strnlen(buffer, 292) + 1;
	if (length >= 292)
		length = 292;
	return copyResult(ret, sceKernelCopyToUser(convertedPath, buffer, length));
}

int _sceAppMgrGetRawPath(char *path, char *resolvedPath, int capacity,
	const AppmgrRawPathArguments *userArguments)
{
	AppmgrRawPathArguments arguments;
	char *name = path;
	char *buffer;
	SceSize length;
	int ret;

	ARGS_IN(arguments, userArguments);
	STRING_IN(name, path, 292);
	if (resolvedPath == NULL)
		return appmgrGetRawPathSyscall(name, NULL, capacity);
	buffer = __builtin_alloca(292);
	__builtin_memset(buffer, 0, 292);
	ret = appmgrGetRawPathSyscall(name, buffer, capacity);
	length = strnlen(buffer, 292) + 1;
	if (length >= 292)
		length = 292;
	return copyResult(ret, sceKernelCopyToUser(resolvedPath, buffer, length));
}

int _sceAppMgrGetSaveDataInfo(const char *appPath, const char *titleId, SceUInt32 attributes,
			      const AppmgrSaveDataInfoArguments *userArguments)
{
	AppmgrGetSaveDataInfoArguments arguments = { appPath, titleId, attributes, userArguments };

	return SceThreadmgrForDriver_E54FD746(0x2000, appmgrGetSaveDataInfoOnStack, &arguments);
}

int _sceAppMgrLoadSaveDataSystemFile(const char *path, void *buffer, SceSize size,
				     const SyscallUnusedArgs *userArguments)
{
	SyscallUnusedArgs arguments;
	char *kernelPath = (char *)path;
	int ret;

	ARGS_IN(arguments, userArguments);
	STRING_IN(kernelPath, path, 292);
	return appmgrLoadSaveDataSystemFileSyscall(kernelPath, buffer, size);
}

int _sceAppMgrGetSaveDataInfoForSpecialExport(const char *path, SceUInt64 *accountId,
					      const SyscallUnusedArgs *userArguments)
{
	SyscallUnusedArgs arguments;
	char *kernelPath = (char *)path;
	int ret;

	ARGS_IN(arguments, userArguments);
	STRING_IN(kernelPath, path, 292);
	BLOCK_OUT(accountId, sizeof(*accountId),
		  appmgrGetSaveDataInfoForSpecialExportSyscall(kernelPath, NULL),
		  appmgrGetSaveDataInfoForSpecialExportSyscall(kernelPath, (SceUInt64 *)k));
}

int _sceAppMgrGetMediaTypeFromDrive(const char *drive, int *mediaType)
{
	void *kernelDrive = (void *)drive;
	int ret;

	BLOCK_IN(kernelDrive, drive, 16);
	BLOCK_OUT(mediaType, sizeof(*mediaType),
		  appmgrGetMediaTypeFromDriveSyscall(kernelDrive, NULL),
		  appmgrGetMediaTypeFromDriveSyscall(kernelDrive, (int *)k));
}

int _sceAppMgrGetMediaTypeFromDriveByPid(SceUID pid, const char *drive, int *mediaType)
{
	void *kernelDrive = (void *)drive;
	int ret;

	BLOCK_IN(kernelDrive, drive, 16);
	BLOCK_OUT(mediaType, sizeof(*mediaType),
		  appmgrGetMediaTypeFromDriveByPidSyscall(pid, kernelDrive, NULL),
		  appmgrGetMediaTypeFromDriveByPidSyscall(pid, kernelDrive, (int *)k));
}

int _sceAppMgrCheckRifGD(const char *path, AppmgrLicenseInfo *info,
			 const SyscallUnusedArgs *userArguments)
{
	AppmgrCheckRifGDArguments arguments = { path, info, userArguments };

	return SceThreadmgrForDriver_E54FD746(0x2000, appmgrCheckRifGDOnStack, &arguments);
}

int _sceAppMgrUpdateSaveDataParam(const char *path, const SyscallUnusedArgs *userArguments)
{
	AppmgrUpdateSaveDataParamArguments arguments = { path, userArguments };

	return SceThreadmgrForDriver_E54FD746(0x2000, appmgrUpdateSaveDataParamOnStack,
				      &arguments);
}

int _sceAppMgrGetDevInfo(const char *drive, SceOff *maxSize, SceOff *freeSize,
			 const SyscallUnusedArgs *userArguments)
{
	SyscallUnusedArgs arguments;
	char *kernelDrive = (char *)drive;
	SceOff *kernelMaxSize = maxSize;
	SceOff *kernelFreeSize = freeSize;
	int ret, copy;

	ARGS_IN(arguments, userArguments);
	STRING_IN(kernelDrive, drive, 16);
	if (maxSize != NULL) {
		kernelMaxSize = __builtin_alloca(sizeof(*kernelMaxSize));
		*kernelMaxSize = 0;
	}
	if (freeSize != NULL) {
		kernelFreeSize = __builtin_alloca(sizeof(*kernelFreeSize));
		*kernelFreeSize = 0;
	}
	ret = appmgrGetDevInfoSyscall(kernelDrive, kernelMaxSize, kernelFreeSize);
	if (maxSize != NULL) {
		copy = sceKernelCopyToUser(maxSize, kernelMaxSize, sizeof(*maxSize));
		if (copy < 0)
			return copy;
	}
	if (freeSize != NULL)
		return copyResult(ret, sceKernelCopyToUser(freeSize, kernelFreeSize,
							 sizeof(*freeSize)));
	return ret;
}

int _sceAppMgrGetPfsDrive(const char *drive, char *pfsDrive)
{
	void *kernelDrive = (void *)drive;
	int ret;

	BLOCK_IN(kernelDrive, drive, 16);
	BLOCK_OUT(pfsDrive, 16,
		  appmgrGetPfsDriveSyscall(kernelDrive, NULL),
		  appmgrGetPfsDriveSyscall(kernelDrive, (char *)k));
}

int _sceAppMgrGetUserDirPath(int partitionId, char *path, unsigned int capacity,
	const SyscallUnusedArgs *userArguments)
{
	SyscallUnusedArgs arguments;
	char *buffer;
	SceSize length;
	int ret;

	ARGS_IN(arguments, userArguments);
	if (path == NULL)
		return appmgrGetUserDirPathSyscall(partitionId, NULL, capacity);
	buffer = __builtin_alloca(32);
	__builtin_memset(buffer, 0, 32);
	ret = appmgrGetUserDirPathSyscall(partitionId, buffer, capacity);
	length = strnlen(buffer, 32) + 1;
	if (length >= 32)
		length = 32;
	return copyResult(ret, sceKernelCopyToUser(path, buffer, length));
}

int _sceAppMgrGetUserDirPathById(int userId, int partitionId, char *path,
	const AppmgrUserDirPathArguments *userArguments)
{
	AppmgrUserDirPathArguments arguments;
	unsigned int capacity;
	char *buffer;
	SceSize length;
	int ret;

	ARGS_IN(arguments, userArguments);
	capacity = arguments.capacity;
	if (path == NULL)
		return appmgrGetUserDirPathByIdSyscall(userId, partitionId, NULL,
			capacity);
	buffer = __builtin_alloca(32);
	__builtin_memset(buffer, 0, 32);
	ret = appmgrGetUserDirPathByIdSyscall(userId, partitionId, buffer,
		capacity);
	length = strnlen(buffer, 32) + 1;
	if (length >= 32)
		length = 32;
	return copyResult(ret, sceKernelCopyToUser(path, buffer, length));
}

int _sceAppMgrInitSafeMemoryById(const char *id, const AppmgrInitSafeMemoryArguments *userArguments)
{
	AppmgrInitSafeMemoryArguments arguments;
	char *name = (char *)id;
	int ret;

	ARGS_IN(arguments, userArguments);
	STRING_IN(name, id, 64);
	return appmgrInitSafeMemoryByIdSyscall(name);
}

int _sceAppMgrAddContMount(SceUID pid, const AppmgrAddContMountParam *userParam)
{
	void *param = (void *)userParam;
	int ret;

	BLOCK_IN(param, userParam, sizeof(AppmgrAddContMountParam));
	return appmgrAddContMountSyscall(pid, param);
}

int _sceAppMgrAddContAddMount(const AppmgrAddMountParam *userParam, int add)
{
	void *param = (void *)userParam;
	int ret;

	BLOCK_IN(param, userParam, sizeof(AppmgrAddMountParam));
	return appmgrAddContAddMountSyscall(param, add);
}

int _sceAppMgrDrmOpen(const SceAppMgrDrmOpenParam *param,
		      const SyscallSizeArgs *userArguments)
{
	AppmgrDrmOpenArguments arguments;

	arguments.param = param;
	arguments.userArguments = userArguments;
	return SceThreadmgrForDriver_E54FD746(0x4000, appmgrDrmOpenOnStack, &arguments);
}

int _sceAppMgrDrmClose(const SceAppMgrDrmOpenParam *userParam,
		       const SyscallSizeArgs *userArguments)
{
	SyscallSizeArgs arguments;
	SceAppMgrDrmOpenParam *param = (SceAppMgrDrmOpenParam *)userParam;
	int ret;

	ARGS_IN(arguments, userArguments);
	BLOCK_IN_SIZED(param, userParam, arguments.size, sizeof(*param));
	return appmgrDrmCloseSyscall(param);
}

int _sceAppMgrContentInstallPeriodStart(void)
{
	return appmgrContentInstallPeriodStartSyscall();
}

int _sceAppMgrContentInstallPeriodStop(void)
{
	return appmgrContentInstallPeriodStopSyscall();
}

int _sceAppMgrPspSaveDataGetParams(AppmgrPspSaveDataParamSfo *userParamSfo)
{
	AppmgrPspSaveDataParamSfo paramSfo;
	int ret;

	if (userParamSfo == NULL)
		return appmgrPspSaveDataGetParamsSyscall(NULL);
	memset(&paramSfo, 0, sizeof(paramSfo));
	ret = sceKernelCopyFromUser(&paramSfo, userParamSfo, sizeof(paramSfo));
	if (ret < 0)
		return ret;
	ret = appmgrPspSaveDataGetParamsSyscall(&paramSfo);
	return copyResult(ret, sceKernelCopyToUser(userParamSfo, &paramSfo, sizeof(paramSfo)));
}

int _sceAppMgrPspSaveDataRead(AppmgrPspSaveDataParams *userParams)
{
	AppmgrPspSaveDataParams params;
	int ret;

	if (userParams == NULL)
		return appmgrPspSaveDataReadSyscall(NULL);
	memset(&params, 0, sizeof(params));
	ret = sceKernelCopyFromUser(&params, userParams, sizeof(params));
	if (ret < 0)
		return ret;
	ret = appmgrPspSaveDataReadSyscall(&params);
	return copyResult(ret, sceKernelCopyToUser(userParams, &params, sizeof(params)));
}

int _sceAppMgrGetRawPathOfApp0ByAppIdForShell(int appId, char *path)
{
	char *buffer;
	int result;

	if (path == NULL)
		return appmgrGetRawPathOfApp0ByAppIdForShellSyscall(appId, NULL);
	buffer = __builtin_alloca(292);
	__builtin_memset(buffer, 0, 292);
	result = appmgrGetRawPathOfApp0ByAppIdForShellSyscall(appId, buffer);
	return copyResult(result, sceKernelCopyToUser(path, buffer, 292));
}

int _sceAppMgrGetRawPathOfApp0ByPidForShell(SceUID pid, char *path)
{
	char *buffer;
	int result;

	if (path == NULL)
		return appmgrGetRawPathOfApp0ByPidForShellSyscall(pid, NULL);
	buffer = __builtin_alloca(292);
	__builtin_memset(buffer, 0, 292);
	result = appmgrGetRawPathOfApp0ByPidForShellSyscall(pid, buffer);
	return copyResult(result, sceKernelCopyToUser(path, buffer, 292));
}
