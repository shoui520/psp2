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
#include <pfsmgr/pfsmgr.h>
#include <threadmgr/threadmgr.h>
#include "mount.h"

int _sceAppMgrSystemParamDateTimeGetConf(void *conf)
{
	int ret;

	BLOCK_OUT(conf, 20,
		  appmgrSystemParamDateTimeGetConfSyscall(NULL),
		  appmgrSystemParamDateTimeGetConfSyscall(k));
}

int _sceAppMgrBgdlGetQueueStatus(void *status)
{
	int ret;

	BLOCK_OUT(status, 0x700,
		  appmgrBgdlGetQueueStatusSyscall(NULL),
		  appmgrBgdlGetQueueStatusSyscall(k));
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

/* the stack arguments of the mounts by ID (application data, trophy) */
typedef struct AppmgrMountByIdArguments {
	unsigned int mountId;
	const char *id;
	ScePfsRndDriveId *mountPoint;
	const SyscallUnusedArgs *userArguments;
} AppmgrMountByIdArguments;

/* the stack arguments of the trophy mount */
typedef struct AppmgrTrophyMountArguments {
	unsigned int mountId;
	SceUID pid;
	ScePfsRndDriveId *mountPoint;
} AppmgrTrophyMountArguments;

/* the stack arguments of the install directory mount */
typedef struct AppmgrInstallDirMountArguments {
	unsigned int mountId;
	const char *id;
	const void *keystone;
	ScePfsRndDriveId *mountPoint;
} AppmgrInstallDirMountArguments;

/* the argument block of the loop back format: where the image information is */
typedef struct AppmgrLoopBackFormatArguments {
	AppmgrLoopBackImageInfo *info;
	SceUInt32 reserved[3];
} AppmgrLoopBackFormatArguments;

_Static_assert(sizeof(AppmgrLoopBackFormatArguments) == 16,
	       "loop back format argument block");

static int appDataMountById(unsigned int mountId, const char *id, ScePfsRndDriveId *mountPoint,
			    const SyscallUnusedArgs *userArguments)
{
	SyscallUnusedArgs arguments;
	char *kernelId = (char *)id;
	int ret;

	ARGS_IN(arguments, userArguments);
	STRING_IN(kernelId, id, 64);
	BLOCK_OUT(mountPoint, sizeof(ScePfsRndDriveId),
		  appmgrAppDataMountByIdSyscall(mountId, kernelId, NULL),
		  appmgrAppDataMountByIdSyscall(mountId, kernelId, (ScePfsRndDriveId *)k));
}

static int appDataMountByIdOnStack(void *argp)
{
	const AppmgrMountByIdArguments *arguments = argp;

	return appDataMountById(arguments->mountId, arguments->id, arguments->mountPoint,
				arguments->userArguments);
}

static int trophyMount(unsigned int mountId, SceUID pid, ScePfsRndDriveId *mountPoint)
{
	int ret;

	BLOCK_OUT(mountPoint, sizeof(ScePfsRndDriveId),
		  appmgrTrophyMountSyscall(mountId, pid, NULL),
		  appmgrTrophyMountSyscall(mountId, pid, (ScePfsRndDriveId *)k));
}

static int trophyMountOnStack(void *argp)
{
	const AppmgrTrophyMountArguments *arguments = argp;

	return trophyMount(arguments->mountId, arguments->pid, arguments->mountPoint);
}

static int trophyMountById(unsigned int mountId, const char *id, ScePfsRndDriveId *mountPoint,
			   const SyscallUnusedArgs *userArguments)
{
	SyscallUnusedArgs arguments;
	char *kernelId = (char *)id;
	int ret;

	ARGS_IN(arguments, userArguments);
	STRING_IN(kernelId, id, 64);
	BLOCK_OUT(mountPoint, sizeof(ScePfsRndDriveId),
		  appmgrTrophyMountByIdSyscall(mountId, kernelId, NULL),
		  appmgrTrophyMountByIdSyscall(mountId, kernelId, (ScePfsRndDriveId *)k));
}

static int trophyMountByIdOnStack(void *argp)
{
	const AppmgrMountByIdArguments *arguments = argp;

	return trophyMountById(arguments->mountId, arguments->id, arguments->mountPoint,
			       arguments->userArguments);
}

static int installDirMount(unsigned int mountId, const char *id, const void *keystone,
			   ScePfsRndDriveId *mountPoint)
{
	void *kernelId = (void *)id;
	void *kernelKeystone = (void *)keystone;
	int ret;

	BLOCK_IN(kernelId, id, 16);
	BLOCK_IN(kernelKeystone, keystone, 96);
	BLOCK_OUT(mountPoint, sizeof(ScePfsRndDriveId),
		  appmgrInstallDirMountSyscall(mountId, kernelId, kernelKeystone, NULL),
		  appmgrInstallDirMountSyscall(mountId, kernelId, kernelKeystone,
					       (ScePfsRndDriveId *)k));
}

static int installDirMountOnStack(void *argp)
{
	const AppmgrInstallDirMountArguments *arguments = argp;

	return installDirMount(arguments->mountId, arguments->id, arguments->keystone,
			       arguments->mountPoint);
}

/* the argument block of the data mounts (game, theme): where the drive is returned */
typedef struct AppmgrDataMountArguments {
	ScePfsRndDriveId *mountPoint;
	SceUInt32 reserved[5];
} AppmgrDataMountArguments;

_Static_assert(sizeof(AppmgrDataMountArguments) == 24,
	       "data mount argument block");

typedef struct AppmgrDataMountParam {
	const char *appPath;
	const char *patchPath;
	const char *rifPath;
	const AppmgrDataMountArguments *userArguments;
} __attribute__((aligned(8))) AppmgrDataMountParam;

static int gameDataMount(const char *appPath, const char *patchPath, const char *rifPath,
			 const AppmgrDataMountArguments *userArguments)
{
	AppmgrDataMountArguments arguments;
	ScePfsRndDriveId *mountPoint;
	char *kernelAppPath = (char *)appPath;
	char *kernelPatchPath = (char *)patchPath;
	char *kernelRifPath = (char *)rifPath;
	int ret;

	ARGS_IN(arguments, userArguments);
	mountPoint = arguments.mountPoint;
	STRING_IN(kernelAppPath, appPath, 292);
	STRING_IN(kernelPatchPath, patchPath, 292);
	STRING_IN(kernelRifPath, rifPath, 292);
	BLOCK_OUT(mountPoint, sizeof(ScePfsRndDriveId),
		  appmgrGameDataMountSyscall(kernelAppPath, kernelPatchPath, kernelRifPath, NULL),
		  appmgrGameDataMountSyscall(kernelAppPath, kernelPatchPath, kernelRifPath,
					     (ScePfsRndDriveId *)k));
}

static int gameDataMountOnStack(void *argp)
{
	const AppmgrDataMountParam *param = argp;

	return gameDataMount(param->appPath, param->patchPath, param->rifPath,
			     param->userArguments);
}

int _sceAppMgrGameDataMount(const char *appPath, const char *patchPath, const char *rifPath,
			    const AppmgrDataMountArguments *userArguments)
{
	AppmgrDataMountParam param = { appPath, patchPath, rifPath, userArguments };

	return SceThreadmgrForDriver_E54FD746(0x2000, gameDataMountOnStack, &param);
}

static int themeDataMount(const char *contentId, const char *themePath, const char *rifPath,
			  const AppmgrDataMountArguments *userArguments)
{
	AppmgrDataMountArguments arguments;
	ScePfsRndDriveId *mountPoint;
	char *kernelContentId = (char *)contentId;
	char *kernelThemePath = (char *)themePath;
	char *kernelRifPath = (char *)rifPath;
	int ret;

	ARGS_IN(arguments, userArguments);
	mountPoint = arguments.mountPoint;
	STRING_IN(kernelContentId, contentId, 27);
	STRING_IN(kernelThemePath, themePath, 128);
	STRING_IN(kernelRifPath, rifPath, 128);
	BLOCK_OUT(mountPoint, sizeof(ScePfsRndDriveId),
		  appmgrThemeDataMountSyscall(kernelContentId, kernelThemePath, kernelRifPath, NULL),
		  appmgrThemeDataMountSyscall(kernelContentId, kernelThemePath, kernelRifPath,
					      (ScePfsRndDriveId *)k));
}

static int themeDataMountOnStack(void *argp)
{
	const AppmgrDataMountParam *param = argp;

	return themeDataMount(param->appPath, param->patchPath, param->rifPath,
			      param->userArguments);
}

int _sceAppMgrThemeDataMount(const char *contentId, const char *themePath, const char *rifPath,
			     const AppmgrDataMountArguments *userArguments)
{
	AppmgrDataMountParam param = { contentId, themePath, rifPath, userArguments };

	return SceThreadmgrForDriver_E54FD746(0x2000, themeDataMountOnStack, &param);
}

int _sceAppMgrAppDataMount(unsigned int mountId, ScePfsRndDriveId *mountPoint)
{
	int ret;

	BLOCK_OUT(mountPoint, sizeof(ScePfsRndDriveId),
		  appmgrAppDataMountSyscall(mountId, NULL),
		  appmgrAppDataMountSyscall(mountId, (ScePfsRndDriveId *)k));
}

int _sceAppMgrAppDataMountById(unsigned int mountId, const char *id, ScePfsRndDriveId *mountPoint,
			       const SyscallUnusedArgs *userArguments)
{
	AppmgrMountByIdArguments arguments = { mountId, id, mountPoint, userArguments };

	return SceThreadmgrForDriver_E54FD746(0x2000, appDataMountByIdOnStack, &arguments);
}

int _sceAppMgrWorkDirMount(unsigned int mountId, ScePfsRndDriveId *mountPoint)
{
	int ret;

	BLOCK_OUT(mountPoint, sizeof(ScePfsRndDriveId),
		  appmgrWorkDirMountSyscall(mountId, NULL),
		  appmgrWorkDirMountSyscall(mountId, (ScePfsRndDriveId *)k));
}

int _sceAppMgrPspSaveDataRootMount(ScePfsRndDriveId *mountPoint)
{
	int ret;

	BLOCK_OUT(mountPoint, sizeof(ScePfsRndDriveId),
		  appmgrPspSaveDataRootMountSyscall(NULL),
		  appmgrPspSaveDataRootMountSyscall((ScePfsRndDriveId *)k));
}

int _sceAppMgrWorkDirMountById(unsigned int mountId, const char *id, ScePfsRndDriveId *mountPoint,
       const SyscallUnusedArgs *userArguments)
{
	SyscallUnusedArgs arguments;
	char *kernelId = (char *)id;
	int ret;

	ARGS_IN(arguments, userArguments);
	STRING_IN(kernelId, id, 64);
	BLOCK_OUT(mountPoint, sizeof(ScePfsRndDriveId),
		  appmgrWorkDirMountByIdSyscall(mountId, kernelId, NULL),
		  appmgrWorkDirMountByIdSyscall(mountId, kernelId, (ScePfsRndDriveId *)k));
}

int _sceAppMgrTrophyMount(unsigned int mountId, SceUID pid, ScePfsRndDriveId *mountPoint)
{
	AppmgrTrophyMountArguments arguments = { mountId, pid, mountPoint };

	return SceThreadmgrForDriver_E54FD746(0x2000, trophyMountOnStack, &arguments);
}

int _sceAppMgrTrophyMountById(unsigned int mountId, const char *id, ScePfsRndDriveId *mountPoint,
			      const SyscallUnusedArgs *userArguments)
{
	AppmgrMountByIdArguments arguments = { mountId, id, mountPoint, userArguments };

	return SceThreadmgrForDriver_E54FD746(0x2000, trophyMountByIdOnStack, &arguments);
}

int _sceAppMgrMmsMount(unsigned int mountId, ScePfsRndDriveId *mountPoint)
{
	int ret;

	BLOCK_OUT(mountPoint, sizeof(ScePfsRndDriveId),
		  appmgrMmsMountSyscall(mountId, NULL),
		  appmgrMmsMountSyscall(mountId, (ScePfsRndDriveId *)k));
}

int _sceAppMgrInstallDirMount(unsigned int mountId, const char *id, const void *keystone,
			      ScePfsRndDriveId *mountPoint)
{
	AppmgrInstallDirMountArguments arguments = { mountId, id, keystone, mountPoint };

	return SceThreadmgrForDriver_E54FD746(0x2000, installDirMountOnStack, &arguments);
}

int _sceAppMgrLoopBackMount(unsigned int mountId, const char *id, ScePfsRndDriveId *mountPoint,
       const SyscallUnusedArgs *userArguments)
{
	SyscallUnusedArgs arguments;
	char *kernelId = (char *)id;
	int ret;

	ARGS_IN(arguments, userArguments);
	STRING_IN(kernelId, id, 64);
	BLOCK_OUT(mountPoint, sizeof(ScePfsRndDriveId),
		  appmgrLoopBackMountSyscall(mountId, kernelId, NULL),
		  appmgrLoopBackMountSyscall(mountId, kernelId, (ScePfsRndDriveId *)k));
}

int _sceAppMgrLoopBackFormat(unsigned int mountId, unsigned int command, const char *id,
			     const AppmgrLoopBackFormatArguments *userArguments)
{
	AppmgrLoopBackFormatArguments arguments;
	AppmgrLoopBackImageInfo *info;
	AppmgrLoopBackImageInfo *kernelInfo;
	char *kernelId = (char *)id;
	int ret;

	ARGS_IN(arguments, userArguments);
	info = arguments.info;
	STRING_IN(kernelId, id, 64);
	if (info == NULL)
		return appmgrLoopBackFormatSyscall(mountId, command, kernelId, NULL);
	kernelInfo = __builtin_alloca(sizeof(*kernelInfo));
	__builtin_memset(kernelInfo, 0, sizeof(*kernelInfo));
	ret = sceKernelCopyFromUser(kernelInfo, info, sizeof(*kernelInfo));
	if (ret < 0)
		return ret;
	ret = appmgrLoopBackFormatSyscall(mountId, command, kernelId, kernelInfo);
	return copyResult(ret, sceKernelCopyToUser(info, kernelInfo, sizeof(*kernelInfo)));
}

int appmgrGetSaveDataInfoForUser(const char *appPath, const char *titleId,
				 SceUInt32 attributes,
				 const AppmgrSaveDataInfoArguments *userArguments)
{
	AppmgrSaveDataInfoArguments arguments;
	AppmgrSaveDataInfo *info;
	char *kernelAppPath = (char *)appPath;
	void *kernelTitleId = (void *)titleId;
	int ret;

	ARGS_IN(arguments, userArguments);
	info = arguments.info;
	STRING_IN(kernelAppPath, appPath, 292);
	BLOCK_IN(kernelTitleId, titleId, 16);
	BLOCK_OUT(info, sizeof(AppmgrSaveDataInfo),
		  appmgrGetSaveDataInfoSyscall(kernelAppPath, kernelTitleId, attributes, NULL),
		  appmgrGetSaveDataInfoSyscall(kernelAppPath, kernelTitleId, attributes,
					       (AppmgrSaveDataInfo *)k));
}

int appmgrGetSaveDataInfoOnStack(void *argp)
{
	const AppmgrGetSaveDataInfoArguments *arguments = argp;

	return appmgrGetSaveDataInfoForUser(arguments->appPath, arguments->titleId,
					    arguments->attributes, arguments->userArguments);
}

int appmgrCheckRifGDForUser(const char *path, AppmgrLicenseInfo *info,
			    const SyscallUnusedArgs *userArguments)
{
	SyscallUnusedArgs arguments;
	char *kernelPath = (char *)path;
	int ret;

	ARGS_IN(arguments, userArguments);
	STRING_IN(kernelPath, path, 292);
	BLOCK_OUT(info, sizeof(AppmgrLicenseInfo),
		  appmgrCheckRifGDSyscall(kernelPath, NULL),
		  appmgrCheckRifGDSyscall(kernelPath, (AppmgrLicenseInfo *)k));
}

int appmgrCheckRifGDOnStack(void *argp)
{
	const AppmgrCheckRifGDArguments *arguments = argp;

	return appmgrCheckRifGDForUser(arguments->path, arguments->info,
				       arguments->userArguments);
}

int appmgrUpdateSaveDataParamForUser(const char *path, const SyscallUnusedArgs *userArguments)
{
	SyscallUnusedArgs arguments;
	char *kernelPath = (char *)path;
	int ret;

	ARGS_IN(arguments, userArguments);
	STRING_IN(kernelPath, path, 292);
	return appmgrUpdateSaveDataParamSyscall(kernelPath);
}

int appmgrUpdateSaveDataParamOnStack(void *argp)
{
	const AppmgrUpdateSaveDataParamArguments *arguments = argp;

	return appmgrUpdateSaveDataParamForUser(arguments->path, arguments->userArguments);
}

int appmgrDrmOpenForUser(const SceAppMgrDrmOpenParam *userParam,
			 const SyscallSizeArgs *userArguments)
{
	SyscallSizeArgs arguments;
	SceAppMgrDrmOpenParam *param = (SceAppMgrDrmOpenParam *)userParam;
	int ret;

	ARGS_IN(arguments, userArguments);
	BLOCK_IN_SIZED(param, userParam, arguments.size, sizeof(*param));
	return appmgrDrmOpenSyscall(param);
}

int appmgrDrmOpenOnStack(void *argp)
{
	const AppmgrDrmOpenArguments *arguments = argp;

	return appmgrDrmOpenForUser(arguments->param, arguments->userArguments);
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



int _sceAppMgrAcquireSoundOutExclusive3(int priority, SceUInt32 displayEnabled,
		SceUInt32 displayMode, const void *userArguments)
{
	AppmgrSoundOutExclusiveArguments arguments;
	int ret;

	ARGS_IN(arguments, userArguments);
	return appmgrAcquireSoundOutExclusive3Syscall(priority, displayEnabled,
		displayMode, arguments.displayBlanked, arguments.unsupportedOptions);
}














int _sceAppMgrSendNotificationRequest(const AppmgrNotificationRequest *request, int wait)
{
	AppmgrNotificationRequest *kernelRequest = NULL;
	int ret;

	BLOCK_IN(kernelRequest, request, sizeof(*kernelRequest));
	return appmgrSendNotificationRequest(kernelRequest, wait);
}

int _sceAppMgrSetBgmProxyApp(const char *appName, const SyscallUnusedArgs *userArguments)
{
	SyscallUnusedArgs arguments;
	char *name = NULL;
	int ret;

	ARGS_IN(arguments, userArguments);
	STRING_IN(name, appName, 32);
	return appmgrSetBgmProxyAppSyscall(name);
}
