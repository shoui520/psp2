#include "appmgr_savedata_slot.h"
#include "appmgr_event.h"

#include <appmgr.h>
#include <iofilemgr/iofilemgr.h>
#include <sysmem/sysmem.h>
#include <rtc/rtc.h>
#include <intrmgr/syscall.h>
#include <threadmgr/threadmgr.h>
#include <sysmem/suspend.h>
#include <regmgr/regmgr.h>
#include <sysmem/sblaimgr.h>
#include <stdio.h>
#include <string.h>
#include <sysmem/sysclib.h>
#include <processmgr/processmgr.h>
#include <acmgr/acmgr.h>
#include <pfsmgr/pfsmgr.h>
#include <appmgr/appmgr_rdrfp.h>
#include "mount.h"
#include "debug_setting.h"
#include "app_state_internal.h"
#include "appmgr_device.h"
#include <sysmem/debug.h>
#include <gcauthmgr/sblgcauthmgrmssavebb.h>
#include "param_sfo.h"

extern SceUID g_appmgrSaveDataHeap;
extern SceUID g_appmgrCoreHeap;
extern SceUID g_appmgrAppMutex;
extern SceUID g_appmgrSaveDataMutex;
extern SceUID g_appmgrMountMutex;
extern int g_appmgrSafeMemoryKey;
extern int appmgrWriteSaveDataSlot(unsigned int slotId,
				 const SceAppMgrSaveDataSlotParam *param,
				 int status, const char *mountPoint);
extern int appmgrDeleteSaveDataSlot(unsigned int slotId, const char *mountPoint);
extern int appmgrCheckSaveDataMount(const char *titleId, const char *mountPoint,
				    const AppmgrSaveDataMount *list);
extern int appmgrFindSaveDataMount(const char *mountPoint, const AppmgrSaveDataMount *list);
extern int appmgrValidateSaveDataRemoveItem(const SceAppUtilSaveDataDataRemoveItem *item,
					    const char *path);
extern int appmgrRemoveSaveDataPath(const char *path, unsigned int missingPolicy,
				    unsigned int *entryCount);
extern int appmgrOverwriteSaveDataSlot(unsigned int slotId,
				     const SceAppMgrSaveDataSlotParam *param,
				     int status, const char *mountPoint);
extern int appmgrSetSaveDataSlotStatus(unsigned int slotId, int status,
				     const char *mountPoint);
extern int appmgrValidateSaveDataMountPoint(const char *mountPoint,
					  int requirePrimary);
extern int appmgrCheckSaveDataSlot(unsigned int slotId, const char *mountPoint,
				  int status);

extern int appmgrValidateSaveDataFile(const SceAppUtilSaveDataDataSaveItem *file,
				     const char *path);
extern int appmgrAccumulateSaveDataWriteSize(SceUInt32 writtenKiB);
extern void appmgrNotifySaveDataSpaceError(int error, SceUInt32 requiredKiB);
extern void appmgrFreeSaveDataDirectories(char **directories);
extern int appmgrSetRequiredSaveDataSize(SceUInt32 requiredKiB);
extern int appmgrCalculateSaveDataFileSpace(const SceAppUtilSaveDataDataSaveItem *userFile,
					  char **directories,
					  SceUInt64 *requiredBytes,
					  SceUInt64 allocationUnit,
					  const char *mountPoint);
extern int appmgrGetSaveDataDeviceSpace(const char *mountPoint,
				      SceUInt64 *allocationUnit,
				      SceOff *totalBytes,
				      SceUInt64 *availableQuota,
				      SceOff *freeBytes);
extern int appmgrGetSaveDataParentPath(const char *path, char *parentPath);
extern int appmgrCreateSaveDataDirectories(const char *path, SceMode mode);
extern int appmgrRollbackSaveDataOperation(const SceAppMgrSaveDataData *operation,
		const char *const *directories, const SceOff *originalSizes,
		const int *createdFiles, SceUInt64 allocationUnit,
		int slotCreated, SceUInt32 *requiredSizeKiB);

int g_appmgrAppInfoCacheKey;

/* A PFS "app" mount a process made with SceAppMgrForDriver_EF0C9533. */
typedef struct AppmgrAppMount {
	char path[128];
	SceUID processId;
	char mountPoint[8];
	struct AppmgrAppMount *next;
} AppmgrAppMount;

_Static_assert(sizeof(AppmgrAppMount) == 144, "app mount");

typedef struct AppmgrAppMountParam {
	SceSize size;
	char path[128];
	char mountPoint[8];
} AppmgrAppMountParam;

_Static_assert(sizeof(AppmgrAppMountParam) == 140, "app mount parameter");

typedef struct AppmgrAppUmountParam {
	SceSize size;
	char mountPoint[8];
} AppmgrAppUmountParam;

_Static_assert(sizeof(AppmgrAppUmountParam) == 12, "app unmount parameter");

AppmgrAppMount *g_appmgrAppMounts;

static __attribute__((noinline)) int appmgrCheckAppMount(const char *path, const char *mountPoint,
			       const AppmgrAppMount *mount);
static __attribute__((noinline)) int appmgrFindAppMount(const char *mountPoint, const AppmgrAppMount *mount);

/* A string of a process's cached application information (100: PSM content ID, 101: PSM title). */
static __attribute__((noinline)) int appmgrGetAppInfoCacheString(int pid, int param, char *string,
								 SceSize length)
{
	SceAppMgrAppInfoCache *info;
	void *storage = NULL;
	int result;

	if (pid != 0 && SceSblACMgrForDriver_8A54DF3A(0) != 0 &&
	    pid != sceKernelGetProcessId())
		return 0x80800009;
	if (string == NULL) {
		result = 0x80801000;
		goto unlock;
	}
	if (length >= 1024)
		length = 1024;
	memset(string, 0, length);
	result = sceKernelLockMutex(g_appmgrAppMutex, 1, NULL);
	if (result < 0)
		return result;
	result = SceProcessmgrForDriver_AF80F39C(pid, g_appmgrAppInfoCacheKey, &storage, 0);
	if (result < 0)
		goto unlock;
	if (storage == NULL) {
		result = 0x80800012;
		goto unlock;
	}
	info = storage;
	if (param == 100) {
		if (length <= 47) {
			result = 0x80801000;
			goto unlock;
		}
		result = 0;
		strncpy(string, info->psmContentId, 47);
	} else if (param == 101) {
		if (length <= 127) {
			result = 0x80801000;
			goto unlock;
		}
		result = 0;
		strncpy(string, info->psmTitle, 127);
	} else {
		result = 0x80800009;
	}
unlock:
	sceKernelUnlockMutex(g_appmgrAppMutex, 1);
	return result;
}

/* Mount a PFS image as the calling process's "app" drive. */
int SceAppMgrForDriver_EF0C9533(const AppmgrAppMountParam *param)
{
	SceUInt32 syscallState;
	char klicensee[16];
	AppmgrAppMount *mount, *entry;
	SceAllocOpt opt;
	int result;

	syscallState = sceSyscallEnter();
	__builtin_memset(klicensee, 0, sizeof(klicensee));
	if (param == NULL || param->size != sizeof(*param)) {
		sceSyscallLeave(syscallState);
		return 0x80800001;
	}
	result = sceKernelLockMutex(g_appmgrAppMutex, 1, NULL);
	if (result < 0) {
		sceSyscallLeave(syscallState);
		return result;
	}
	result = appmgrCheckAppMount(param->path, param->mountPoint, g_appmgrAppMounts);
	if (result != 0) {
		sceKernelUnlockMutex(g_appmgrAppMutex, 1);
		sceSyscallLeave(syscallState);
		return result;
	}
	result = ScePfsMgrForKernel_A772209C(param->path, "app", 0, klicensee, 0);
	if (result != 0) {
		sceKernelUnlockMutex(g_appmgrAppMutex, 1);
		sceSyscallLeave(syscallState);
		return result;
	}
	opt.size = sizeof(opt);
	opt.align = 32;
	mount = sceKernelAllocHeapMemoryWithOption(g_appmgrCoreHeap, sizeof(*mount), &opt);
	if (mount == NULL) {
		ScePfsMgrForKernel_680BC384("app");
		sceKernelUnlockMutex(g_appmgrAppMutex, 1);
		sceSyscallLeave(syscallState);
		return 0x80801006;
	}
	memset(mount, 0, sizeof(*mount));
	strncpy(mount->path, param->path, sizeof(mount->path) - 1);
	mount->processId = sceKernelGetProcessId();
	strncpy(mount->mountPoint, param->mountPoint, sizeof(mount->mountPoint) - 1);
	mount->next = g_appmgrAppMounts;
	g_appmgrAppMounts = mount;
	for (entry = g_appmgrAppMounts; entry != NULL; entry = entry->next)
		;
	sceKernelUnlockMutex(g_appmgrAppMutex, 1);
	sceSyscallLeave(syscallState);
	return 0;
}

/* Unmount the calling process's "app" drive. */
int SceAppMgrForDriver_1DE3B7C4(const AppmgrAppUmountParam *param)
{
	SceUInt32 syscallState;
	AppmgrAppMount *mount, *previous;
	SceUID pid;
	int result;

	syscallState = sceSyscallEnter();
	if (param == NULL || param->size != sizeof(*param)) {
		sceSyscallLeave(syscallState);
		return 0x80800001;
	}
	result = sceKernelLockMutex(g_appmgrAppMutex, 1, NULL);
	if (result < 0) {
		sceSyscallLeave(syscallState);
		return result;
	}
	result = appmgrFindAppMount(param->mountPoint, g_appmgrAppMounts);
	if (result != 0 || (result = ScePfsMgrForKernel_680BC384("app")) != 0) {
		sceKernelUnlockMutex(g_appmgrAppMutex, 1);
		sceSyscallLeave(syscallState);
		return result;
	}
	pid = sceKernelGetProcessId();
	previous = NULL;
	for (mount = g_appmgrAppMounts; mount != NULL; mount = mount->next) {
		if (mount->processId == pid &&
		    strncmp(param->mountPoint, mount->mountPoint, 7) == 0) {
			if (previous != NULL)
				previous->next = mount->next;
			else
				g_appmgrAppMounts = mount->next;
			sceKernelFreeHeapMemory(g_appmgrCoreHeap, mount);
			break;
		}
		previous = mount;
	}
	for (mount = g_appmgrAppMounts; mount != NULL; mount = mount->next)
		;
	sceKernelUnlockMutex(g_appmgrAppMutex, 1);
	sceSyscallLeave(syscallState);
	return 0;
}

/* An integer parameter of a process's application. */
int SceAppMgrForDriver_12CE29F7(int pid, int param, int *value)
{
	SceUInt32 syscallState = sceSyscallEnter();
	struct AppState *app;
	AppmgrProcessContent *content;
	int setting;
	int result;

	if ((pid != 0 && SceSblACMgrForDriver_8A54DF3A(0) != 0 &&
	     pid != sceKernelGetProcessId()) ||
	    ((param == 15 || param == 17) && SceSblACMgrForDriver_991FDC15(0) == 0)) {
		result = 0x80800009;
		goto out;
	}
	if (value == NULL) {
		result = 0x80800001;
		goto out;
	}
	app = appmgrFindAppByProcessId(g_appmgrAppStates,
				       pid != 0 ? pid : sceKernelGetProcessId());
	if (app == NULL) {
		result = 0x80802012;
		goto out;
	}
	content = &app->process.content;
	result = 0;
	switch (param) {
	case 1:
		if (strncmp(content->category, "gdb", 4) == 0) {
			*value = content->license.flags & ~1;
			break;
		}
		setting = appmgrGetDebugUpgradable();
		if (setting == 0)
			*value = content->license.flags;
		else if (setting == 1)
			*value = content->license.flags | 1;
		else
			*value = content->license.flags & ~1;
		break;
	case 2:
		setting = appmgrGetDebugUpgradable();
		if (setting == 0)
			setting = content->license.skuFlags;
		*value = setting;
		break;
	case 3:
		*value = content->appVersion;
		break;
	case 4:
		*value = content->savedataVersion;
		break;
	case 5:
		*value = content->savedataOwnershipFlags;
		break;
	case 11:
		*value = content->savedataMaxSize;
		break;
	case 13:
		*value = content->savedataEnvironmentFlags;
		break;
	case 15:
		*value = content->attribute;
		break;
	case 16:
		if (content->attributeMinorMode == 1)
			*value = 0;
		else if (content->attributeMinorMode != 0)
			*value = 1;
		else
			*value = appmgrGetButtonAssign() == 1;
		break;
	case 17:
		*value = content->attribute2;
		break;
	default:
		result = 0x80801042;
		break;
	}
out:
	sceSyscallLeave(syscallState);
	return result;
}

/* A string parameter of a process's application. */
int sceAppMgrAppParamGetString(int pid, int param, char *string, SceSize length)
{
	SceUInt32 syscallState = sceSyscallEnter();
	struct AppState *app;
	AppmgrProcessContent *content;
	int result;

	if ((param == 14 && SceThreadmgrForDriver_332E127C() != 128) ||
	    (pid != 0 && SceSblACMgrForDriver_8A54DF3A(0) != 0 &&
	     pid != sceKernelGetProcessId())) {
		result = 0x80800009;
		goto out;
	}
	if (string == NULL) {
		result = 0x80800001;
		goto out;
	}
	if (param == 100) {
		result = appmgrGetAppInfoCacheString(pid, 100, string, length);
		if (result != 0x80800012) {
			if (result < 0)
				goto out;
			if (string[0] != '\0') {
				sceSyscallLeave(syscallState);
				return 0;
			}
		}
		param = 6;
	} else if (param == 101) {
		result = appmgrGetAppInfoCacheString(pid, 101, string, length);
		if (result != 0x80800012) {
			if (result < 0)
				goto out;
			if (string[0] != '\0') {
				sceSyscallLeave(syscallState);
				return 0;
			}
		}
		param = 9;
	}
	app = appmgrFindAppByProcessId(g_appmgrAppStates,
				       pid != 0 ? pid : sceKernelGetProcessId());
	if (app == NULL) {
		result = 0x80802012;
		goto out;
	}
	if (length >= 1024)
		length = 1024;
	memset(string, 0, length);
	content = &app->process.content;
	switch (param) {
	case 6:
		if (length < sizeof(content->contentId))
			goto tooSmall;
		strncpy(string, content->contentId, length - 1);
		break;
	case 7:
		if (length < sizeof(content->npCommunicationId))
			goto tooSmall;
		strncpy(string, content->npCommunicationId, length - 1);
		break;
	case 8:
		if (length < sizeof(content->category))
			goto tooSmall;
		strncpy(string, content->category, length - 1);
		break;
	case 9:
		if (length < sizeof(content->title))
			goto tooSmall;
		strncpy(string, content->title, length - 1);
		break;
	case 10:
		if (length < sizeof(content->shortTitle))
			goto tooSmall;
		strncpy(string, content->shortTitle, length - 1);
		break;
	case 12:
		if (length < 10)
			goto tooSmall;
		strncpy(string, content->titleId, length - 1);
		break;
	case 14:
		if (length < sizeof(content->appDirectory))
			goto tooSmall;
		strncpy(string, content->appDirectory, length - 1);
		break;
	default:
	tooSmall:
		sceSyscallLeave(syscallState);
		return 0x80801000;
	}
	result = 0;
out:
	sceSyscallLeave(syscallState);
	return result;
}

/* A PSM content ID: XXYYYY-XXXXNNNNN_NN-ZZZZZZZZZZZZZZZZ (X upper case, N digit, Y and Z either). */
static inline int appmgrIsPsmContentId(const unsigned char *id)
{
	int i;

	if (!APPMGR_IS_UPPER(id[0]) || !APPMGR_IS_UPPER(id[1]))
		return 0;
	for (i = 2; i < 6; i++)
		if (!APPMGR_IS_UPPER(id[i]) && !APPMGR_IS_DIGIT(id[i]))
			return 0;
	if (id[6] != '-')
		return 0;
	for (i = 7; i < 11; i++)
		if (!APPMGR_IS_UPPER(id[i]))
			return 0;
	for (i = 11; i < 16; i++)
		if (!APPMGR_IS_DIGIT(id[i]))
			return 0;
	if (id[16] != '_' || !APPMGR_IS_DIGIT(id[17]) || !APPMGR_IS_DIGIT(id[18]) ||
	    id[19] != '-')
		return 0;
	for (i = 20; i < 36; i++)
		if (!APPMGR_IS_DIGIT(id[i]) && !APPMGR_IS_UPPER(id[i]))
			return 0;
	return id[36] == '\0';
}

/* Set a string of the calling PSM process's cached application information. */
int sceAppMgrAppParamSetString(int param, const char *string)
{
	SceUInt32 syscallState = sceSyscallEnter();
	SceAppMgrAppInfoCache *info;
	void *storage;
	int result;

	if ((unsigned int)(param - 100) > 1) {
		result = 0x80800009;
		goto out;
	}
	storage = NULL;
	if (SceSblACMgrForDriver_091F7247(0) == 0) {
		result = 0x80800009;
		goto out;
	}
	result = sceKernelLockMutex(g_appmgrAppMutex, 1, NULL);
	if (result < 0)
		goto out;
	result = SceProcessmgrForDriver_AF80F39C(0, g_appmgrAppInfoCacheKey, &storage, 0);
	if (result < 0)
		goto unlock;
	if (storage == NULL) {
		result = 0x80800012;
		goto unlock;
	}
	info = storage;
	if (param == 101) {
		if (string != NULL && strnlen(string, 128) != 128) {
			result = 0;
			memset(info->psmTitle, 0, sizeof(info->psmTitle));
			strncpy(info->psmTitle, string, sizeof(info->psmTitle) - 1);
			goto unlock;
		}
	} else if (string != NULL && appmgrIsPsmContentId((const unsigned char *)string)) {
		result = 0;
		memset(info->psmContentId, 0, sizeof(info->psmContentId));
		strncpy(info->psmContentId, string, sizeof(info->psmContentId) - 1);
		goto unlock;
	}
	result = 0x80801000;
unlock:
	sceKernelUnlockMutex(g_appmgrAppMutex, 1);
out:
	sceSyscallLeave(syscallState);
	return result;
}

/* 0 when the path exists and neither it nor the mount point is mounted yet. */
static __attribute__((noinline)) int appmgrCheckAppMount(const char *path, const char *mountPoint,
			       const AppmgrAppMount *mount)
{
	SceIoStat stat;
	SceUID pid;

	if (sceIoGetstat(path, &stat) < 0)
		return 0x80801005;
	pid = sceKernelGetProcessId();
	for (; mount != NULL; mount = mount->next) {
		if (strncmp(path, mount->path, 127) == 0)
			return 0x80801040;
		if (mount->processId == pid &&
		    strncmp(mountPoint, mount->mountPoint, 7) == 0)
			return 0x80801040;
	}
	return 0;
}

/* 0 when the calling process has an app mount at this mount point. */
static __attribute__((noinline)) int appmgrFindAppMount(const char *mountPoint, const AppmgrAppMount *mount)
{
	SceUID pid = sceKernelGetProcessId();

	for (; mount != NULL; mount = mount->next) {
		if (mount->processId == pid &&
		    strncmp(mountPoint, mount->mountPoint, 7) == 0)
			return 0;
	}
	return 0x80801041;
}

AppmgrSaveDataMount *g_appmgrSaveDataMountList;
AppmgrSaveDataSlotCache g_appmgrSaveDataSlotCache;

int appmgrSetRequiredSaveDataSize(SceUInt32 requiredKiB)
{
	SceAppMgrAppInfoCache *info;
	void *storage = NULL;
	int result;

	result = SceProcessmgrForDriver_AF80F39C(0,
			g_appmgrAppInfoCacheKey, &storage, 0);
	if (result < 0)
		return result;
	if (!storage)
		return (int)0x80800012;
	info = storage;
	info->neededSaveDataKiB = requiredKiB;
	return 0;
}

int appmgrLoadSaveDataSlotCache(const char *mountPoint)
{
	char path[32] = {0};
	char resolvedPath[292];
	SceUID fd;
	int result;
	int bytes;

	snprintf(path, sizeof(path), "%s%s/%s", mountPoint,
		 "sce_sys", "sdslot.dat");
	memset(resolvedPath, 0, sizeof(resolvedPath));
	result = appmgrResolvePath(path, 0, resolvedPath, sizeof(resolvedPath));
	if (result < 0)
		return result;
	fd = sceIoOpen(resolvedPath, 1, 6);
	if (fd < 0)
		return fd == (int)0x80010013 ? fd : (int)0x80801087;
	bytes = sceIoRead(fd, g_appmgrSaveDataSlotCache.readWindow, 32768);
	if (bytes != 32768) {
		sceIoClose(fd);
		return (int)0x80801088;
	}
	g_appmgrSaveDataSlotCache.windowIndex = 0;
	g_appmgrSaveDataSlotCache.windowLength = bytes;
	g_appmgrSaveDataSlotCache.fd = fd;
	result = 0;
	memcpy(g_appmgrSaveDataSlotCache.header,
	       g_appmgrSaveDataSlotCache.readWindow, 1024);
	return result;
}

int sceAppMgrSaveDataGetCachedRequiredSizeKiB(void)
{
	SceUInt32 syscallState;
	SceAppMgrAppInfoCache *info;
	void *storage = NULL;
	int result;

	syscallState = sceSyscallEnter();
	result = sceKernelLockMutex(g_appmgrSaveDataMutex, 1, NULL);
	if (result < 0)
		goto out;
	result = SceProcessmgrForDriver_AF80F39C(0, g_appmgrAppInfoCacheKey, &storage, 0);
	if (result >= 0) {
		if (storage != NULL) {
			info = storage;
			result = info->neededSaveDataKiB;
			if (result < 0)
				result = 0x7fffffff;
		} else {
			result = 0x80800012;
		}
	}
	sceKernelUnlockMutex(g_appmgrSaveDataMutex, 1);
out:
	sceSyscallLeave(syscallState);
	return result;
}

int appmgrInitializeSaveDataSlotCache(void)
{
	SceAllocOpt option;

	g_appmgrSaveDataSlotCache = (AppmgrSaveDataSlotCache){0};
	option.mmapKind = 0;
	option.mmapBase = NULL;
	option.mmapSize = 0;
	g_appmgrSaveDataSlotCache.fd = -1;
	option.size = sizeof(option);
	option.align = 32;
	g_appmgrSaveDataSlotCache.header = sceKernelAllocHeapMemoryWithOption(
		g_appmgrSaveDataHeap, 1024, &option);
	if (!g_appmgrSaveDataSlotCache.header)
		return (int)0x80801006;
	g_appmgrSaveDataSlotCache.readWindow = sceKernelAllocHeapMemoryWithOption(
		g_appmgrSaveDataHeap, 32768, &option);
	if (!g_appmgrSaveDataSlotCache.readWindow) {
		sceKernelFreeHeapMemory(g_appmgrSaveDataHeap,
					g_appmgrSaveDataSlotCache.header);
		g_appmgrSaveDataSlotCache.header = NULL;
		return (int)0x80801006;
	}
	memset(g_appmgrSaveDataSlotCache.header, 0, 1024);
	memset(g_appmgrSaveDataSlotCache.readWindow, 0, 32768);
	return 0;
}

int appmgrResetSaveDataSlotCache(SceUID pid)
{
	if (g_appmgrSaveDataSlotCache.processId != pid)
		return 0;
	if (g_appmgrSaveDataSlotCache.fd != -1) {
		sceIoClose(g_appmgrSaveDataSlotCache.fd);
		g_appmgrSaveDataSlotCache.fd = -1;
	}
	g_appmgrSaveDataSlotCache.windowIndex = 0;
	g_appmgrSaveDataSlotCache.windowLength = 0;
	memset(g_appmgrSaveDataSlotCache.header, 0, 1024);
	memset(g_appmgrSaveDataSlotCache.readWindow, 0, 32768);
	g_appmgrSaveDataSlotCache.processId = 0;
	return 0;
}

int appmgrCreateSaveDataSlotFile(SceUID pid, const char *path,
				SceUInt32 slotCount)
{
	void *buffer = NULL;
	AppmgrSaveDataSlotFileHeader *header;
	SceAllocOpt option;
	SceIoStat stat;
	char *filePath;
	SceUID fd;
	SceUInt32 total;
	SceUInt32 remaining;
	SceUInt32 chunk;
	int bytes;
	int result;

	SceKernelSuspendForDriver_4DF40893(0);
	result = sceKernelLockMutex(g_appmgrSaveDataMutex, 1, NULL);
	if (result < 0)
		goto unlockPower;
	option.mmapKind = 0;
	option.mmapBase = NULL;
	option.mmapSize = 0;
	option.size = sizeof(option);
	option.align = 32;
	filePath = sceKernelAllocHeapMemoryWithOption(g_appmgrSaveDataHeap,
						     292, &option);
	if (!filePath) {
		result = (int)0x80801006;
		goto unlockMutex;
	}
	memset(filePath, 0, 292);
	snprintf(filePath, 292, "%s/%s/%s", path, "sce_sys", "sdslot.dat");
	result = sceIoGetstat(filePath, &stat);
	if (result == 0)
		goto freePath;
	result = SceProcessmgrForDriver_AF80F39C(
		pid, g_appmgrSafeMemoryKey, &buffer, 1);
	if (result < 0)
		goto freePath;
	if (!buffer) {
		result = (int)0x80801006;
		goto freePath;
	}
	memset(buffer, 0, 65536);
	header = buffer;
	header->version = 0;
	header->slotCount = slotCount;
	*(SceUInt32 *)header->magic = *(const SceUInt32 *)"SDSL";
	fd = sceIoOpen(filePath, 1538, 390);
	if (fd < 0) {
		result = fd;
		goto freePath;
	}
	total = (slotCount + 1) << 10;
	chunk = total < 65536 ? total : 65536;
	bytes = sceIoWrite(fd, buffer, chunk);
	if (bytes < 0) {
		result = bytes;
		goto closeFile;
	}
	if (bytes != chunk) {
		result = (int)0x80801089;
		goto closeFile;
	}
	remaining = total - chunk;
	memset(buffer, 0, 65536);
	while (remaining) {
		chunk = remaining < 65536 ? remaining : 65536;
		bytes = sceIoWrite(fd, buffer, chunk);
		if (bytes < 0) {
			result = bytes;
			goto closeFile;
		}
		if (bytes != chunk) {
			result = (int)0x80801089;
			goto closeFile;
		}
		remaining -= chunk;
	}
	result = 0;
closeFile:
	sceIoClose(fd);
freePath:
	sceKernelFreeHeapMemory(g_appmgrSaveDataHeap, filePath);
unlockMutex:
	sceKernelUnlockMutex(g_appmgrSaveDataMutex, 1);
unlockPower:
	SceKernelSuspendForDriver_2BB92967(0);
	return result;
}

int SceAppMgrForDriver_2277F736(void *parameters)
{
	AppmgrSaveDataSlotInitParam *param = parameters;
	AppmgrSaveDataSlotFileHeader *header;
	SceAllocOpt option;
	SceIoStat stat;
	char directory[128];
	char resolvedDirectory[292];
	char path[292];
	const char *mountPoint;
	void *zeroSlots;
	SceUInt32 syscallState;
	SceUInt32 block;
	SceUID fd;
	int permission;
	int result;

	syscallState = sceSyscallEnter();
	if (!param || param->size != sizeof(*param)) {
		result = (int)0x80800001;
		goto leave;
	}
	mountPoint = (const char *)param->mountPoint.data;
	result = appmgrValidateSaveDataMountPoint(mountPoint, 1);
	if (result < 0)
		goto leave;
	SceKernelSuspendForDriver_4DF40893(0);
	result = sceKernelLockMutex(g_appmgrSaveDataMutex, 1, NULL);
	if (result < 0)
		goto unlockPower;
	memset(directory, 0, sizeof(directory));
	snprintf(directory, sizeof(directory), "%s%s", mountPoint, "sce_sys");
	appmgrResolvePath(directory, 0, resolvedDirectory,
			  sizeof(resolvedDirectory));
	permission = sceKernelSetThreadAccessLevel(128);
	if (g_appmgrSaveDataSlotCache.processId == sceKernelGetProcessId() &&
	    g_appmgrSaveDataSlotCache.fd != -1 &&
	    strncmp(g_appmgrSaveDataSlotCache.mountPoint, mountPoint, 16) == 0) {
		result = sceIoClose(g_appmgrSaveDataSlotCache.fd);
		if (result < 0)
			goto restorePermission;
		g_appmgrSaveDataSlotCache.fd = -1;
	}
	if (sceIoGetstat(resolvedDirectory, &stat) < 0) {
		result = sceIoMkdir(resolvedDirectory, 6);
		if (result < 0)
			goto restorePermission;
	}
	snprintf(path, sizeof(path), "%s/%s", resolvedDirectory, "sdslot.dat");
	if (sceIoGetstat(path, &stat) >= 0) {
		if (!(param->flags & 1)) {
			fd = sceIoOpen(path, 1, 390);
			if (fd < 0) {
				result = fd == (int)0x80010013 ? fd : (int)0x80801087;
				goto restorePermission;
			}
			option.size = sizeof(option);
			option.align = 32;
			header = sceKernelAllocHeapMemoryWithOption(
				g_appmgrSaveDataHeap, sizeof(*header), &option);
			if (!header) {
				sceIoClose(fd);
				result = (int)0x80801006;
				goto restorePermission;
			}
			memset(header, 0, sizeof(*header));
			if (sceIoRead(fd, header, sizeof(*header)) == sizeof(*header)) {
				sceIoClose(fd);
				result = header->slotCount == param->slotCount ?
					0 : (int)0x80801085;
				sceKernelFreeHeapMemory(g_appmgrSaveDataHeap, header);
			} else {
				sceKernelFreeHeapMemory(g_appmgrSaveDataHeap, header);
				sceIoClose(fd);
				result = (int)0x80801088;
			}
			goto restorePermission;
		}
		result = sceIoRemove(path);
		if (result < 0)
			goto restorePermission;
	}
	option.size = sizeof(option);
	option.align = 32;
	header = sceKernelAllocHeapMemoryWithOption(g_appmgrSaveDataHeap,
						   sizeof(*header), &option);
	if (!header) {
		result = (int)0x80801006;
		goto restorePermission;
	}
	memset(header, 0, sizeof(*header));
	header->version = 0;
	header->magic[0] = 'S';
	header->magic[1] = 'D';
	header->magic[2] = 'S';
	header->magic[3] = 'L';
	header->slotCount = param->slotCount;
	fd = sceIoOpen(path, 1538, 390);
	if (fd < 0) {
		sceKernelFreeHeapMemory(g_appmgrSaveDataHeap, header);
		result = fd == (int)0x80010013 ? fd : (int)0x80801087;
		goto restorePermission;
	}
	if (sceIoWrite(fd, header, sizeof(*header)) != sizeof(*header)) {
		sceIoClose(fd);
		sceKernelFreeHeapMemory(g_appmgrSaveDataHeap, header);
		result = (int)0x80801089;
		goto restorePermission;
	}
	sceKernelFreeHeapMemory(g_appmgrSaveDataHeap, header);
	option.size = sizeof(option);
	option.align = 32;
	zeroSlots = sceKernelAllocHeapMemoryWithOption(g_appmgrSaveDataHeap,
						      4096, &option);
	if (!zeroSlots) {
		sceIoClose(fd);
		result = (int)0x80801006;
		goto restorePermission;
	}
	memset(zeroSlots, 0, 4096);
	if ((param->slotCount >> 2) != 0) {
		block = 0;
		do {
			if (sceIoWrite(fd, zeroSlots, 4096) != 4096) {
				sceIoClose(fd);
				sceKernelFreeHeapMemory(g_appmgrSaveDataHeap, zeroSlots);
				result = (int)0x80801089;
				goto restorePermission;
			}
			++block;
		} while (block < (param->slotCount >> 2));
	}
	sceIoClose(fd);
	sceKernelFreeHeapMemory(g_appmgrSaveDataHeap, zeroSlots);
	result = 0;
restorePermission:
	sceKernelSetThreadAccessLevel(permission);
	sceKernelUnlockMutex(g_appmgrSaveDataMutex, 1);
unlockPower:
	SceKernelSuspendForDriver_2BB92967(0);
leave:
	sceSyscallLeave(syscallState);
	return result;
}

int SceAppMgrForDriver_E6F389B8(void *parameters)
{
	const SceAppMgrSaveDataSlot *slot = parameters;
	SceAppMgrAppInfoCache *info;
	SceIoStat stat;
	char path[64];
	void *storage;
	SceUInt32 syscallState;
	const char *mountPoint;
	SceUID fd;
	int permission;
	int result;

	syscallState = sceSyscallEnter();
	if (!slot || slot->size != sizeof(*slot)) {
		result = (int)0x80800001;
		goto leaveSyscall;
	}
	mountPoint = (const char *)slot->mountPoint.data;
	result = appmgrValidateSaveDataMountPoint(mountPoint, 1);
	if (result < 0)
		goto leaveSyscall;
	SceKernelSuspendForDriver_4DF40893(0);
	result = sceKernelLockMutex(g_appmgrSaveDataMutex, 1, NULL);
	if (result < 0)
		goto unlockPower;
	permission = sceKernelSetThreadAccessLevel(128);
	if (g_appmgrSaveDataSlotCache.processId == sceKernelGetProcessId()) {
		fd = g_appmgrSaveDataSlotCache.fd;
		if (fd != -1 &&
		    strncmp(g_appmgrSaveDataSlotCache.mountPoint, mountPoint, 16) == 0) {
			result = sceIoClose(fd);
			if (result < 0)
				goto restorePermission;
			g_appmgrSaveDataSlotCache.fd = -1;
		}
	}
	result = appmgrCheckSaveDataSlot(slot->slotId, mountPoint, 0);
	if (result != 0)
		goto restorePermission;
	result = appmgrWriteSaveDataSlot(slot->slotId, &slot->slotParam, -1,
					mountPoint);
	if (result != 0)
		goto restorePermission;
	storage = NULL;
	if (SceProcessmgrForDriver_AF80F39C(0, g_appmgrAppInfoCacheKey,
						    &storage, 0) >= 0 && storage) {
		info = storage;
		info->saveDataFlags |= 2;
		if (SceRtcForDriver_A930CD1A(&stat.st_mtime, 0) >= 0 &&
		    appmgrResolvePath(mountPoint, 0, path, sizeof(path)) >= 0)
			sceIoChstat(path, &stat, 32);
	}
	result = 0;
restorePermission:
	sceKernelSetThreadAccessLevel(permission);
	sceKernelUnlockMutex(g_appmgrSaveDataMutex, 1);
unlockPower:
	SceKernelSuspendForDriver_2BB92967(0);
leaveSyscall:
	sceSyscallLeave(syscallState);
	return result;
}

int SceAppMgrForDriver_B544F2A0(SceAppMgrSaveDataSlotDelete *parameters)
{
	const SceAppMgrSaveDataSlotDelete *slot = parameters;
	SceAppMgrAppInfoCache *info;
	SceIoStat stat;
	char path[64];
	void *storage;
	SceUInt32 syscallState;
	const char *mountPoint;
	SceUID fd;
	int permission;
	int result;

	syscallState = sceSyscallEnter();
	if (!slot || slot->size != sizeof(*slot)) {
		result = (int)0x80800001;
		goto leaveSyscall;
	}
	mountPoint = (const char *)slot->mountPoint.data;
	result = appmgrValidateSaveDataMountPoint(mountPoint, 1);
	if (result < 0)
		goto leaveSyscall;
	SceKernelSuspendForDriver_4DF40893(0);
	result = sceKernelLockMutex(g_appmgrSaveDataMutex, 1, NULL);
	if (result < 0)
		goto unlockPower;
	permission = sceKernelSetThreadAccessLevel(128);
	if (g_appmgrSaveDataSlotCache.processId == sceKernelGetProcessId()) {
		fd = g_appmgrSaveDataSlotCache.fd;
		if (fd != -1 &&
		    strncmp(g_appmgrSaveDataSlotCache.mountPoint, mountPoint, 16) == 0) {
			result = sceIoClose(fd);
			if (result < 0)
				goto restorePermission;
			g_appmgrSaveDataSlotCache.fd = -1;
		}
	}
	result = appmgrCheckSaveDataSlot(slot->slotId, mountPoint, 1);
	if (result != 0)
		goto restorePermission;
	result = appmgrDeleteSaveDataSlot(slot->slotId, mountPoint);
	if (result != 0)
		goto restorePermission;
	storage = NULL;
	if (SceProcessmgrForDriver_AF80F39C(0, g_appmgrAppInfoCacheKey,
						    &storage, 0) >= 0 && storage) {
		info = storage;
		info->saveDataFlags |= 2;
		if (SceRtcForDriver_A930CD1A(&stat.st_mtime, 0) >= 0 &&
		    appmgrResolvePath(mountPoint, 0, path, sizeof(path)) >= 0)
			sceIoChstat(path, &stat, 32);
	}
	result = 0;
restorePermission:
	sceKernelSetThreadAccessLevel(permission);
	sceKernelUnlockMutex(g_appmgrSaveDataMutex, 1);
unlockPower:
	SceKernelSuspendForDriver_2BB92967(0);
leaveSyscall:
	sceSyscallLeave(syscallState);
	return result;
}

int SceAppMgrForDriver_7E247E47(void *parameters)
{
	const SceAppMgrSaveDataSlot *slot = parameters;
	SceAppMgrAppInfoCache *info;
	SceIoStat stat;
	char path[64];
	void *storage;
	SceUInt32 syscallState;
	const char *mountPoint;
	SceUID fd;
	int permission;
	int result;

	syscallState = sceSyscallEnter();
	if (!slot || slot->size != sizeof(*slot)) {
		result = (int)0x80800001;
		goto leaveSyscall;
	}
	mountPoint = (const char *)slot->mountPoint.data;
	result = appmgrValidateSaveDataMountPoint(mountPoint, 1);
	if (result < 0)
		goto leaveSyscall;
	SceKernelSuspendForDriver_4DF40893(0);
	result = sceKernelLockMutex(g_appmgrSaveDataMutex, 1, NULL);
	if (result < 0)
		goto unlockPower;
	permission = sceKernelSetThreadAccessLevel(128);
	if (g_appmgrSaveDataSlotCache.processId == sceKernelGetProcessId()) {
		fd = g_appmgrSaveDataSlotCache.fd;
		if (fd != -1 &&
		    strncmp(g_appmgrSaveDataSlotCache.mountPoint, mountPoint, 16) == 0) {
			result = sceIoClose(fd);
			if (result < 0)
				goto restorePermission;
			g_appmgrSaveDataSlotCache.fd = -1;
		}
	}
	result = appmgrCheckSaveDataSlot(slot->slotId, mountPoint, 1);
	if (result != 0)
		goto restorePermission;
	result = appmgrOverwriteSaveDataSlot(slot->slotId, &slot->slotParam, -1,
					    mountPoint);
	if (result != 0)
		goto restorePermission;
	storage = NULL;
	if (SceProcessmgrForDriver_AF80F39C(0, g_appmgrAppInfoCacheKey,
						    &storage, 0) >= 0 && storage) {
		info = storage;
		info->saveDataFlags |= 2;
		if (SceRtcForDriver_A930CD1A(&stat.st_mtime, 0) >= 0 &&
		    appmgrResolvePath(mountPoint, 0, path, sizeof(path)) >= 0)
			sceIoChstat(path, &stat, 32);
	}
	result = 0;
restorePermission:
	sceKernelSetThreadAccessLevel(permission);
	sceKernelUnlockMutex(g_appmgrSaveDataMutex, 1);
unlockPower:
	SceKernelSuspendForDriver_2BB92967(0);
leaveSyscall:
	sceSyscallLeave(syscallState);
	return result;
}

int SceAppMgrForDriver_D137D486(SceAppMgrSaveDataSlot *slot)
{
	SceUInt32 syscallState;
	const char *mountPoint;
	char *path, *resolvedPath;
	SceUID fd;
	int permission;
	int result;

	syscallState = sceSyscallEnter();
	if (!slot || slot->size != sizeof(*slot)) {
		result = (int)0x80800001;
		goto leaveSyscall;
	}
	mountPoint = (const char *)slot->mountPoint.data;
	result = appmgrValidateSaveDataMountPoint(mountPoint, 0);
	if (result < 0)
		goto leaveSyscall;
	result = sceKernelLockMutex(g_appmgrSaveDataMutex, 1, NULL);
	if (result < 0)
		goto leaveSyscall;
	SceKernelSuspendForDriver_4DF40893(0);
	permission = sceKernelSetThreadAccessLevel(128);
	path = appmgrAllocSaveDataHeapMemory(128);
	if (!path) {
		result = (int)0x80801006;
		goto restorePermission;
	}
	resolvedPath = appmgrAllocSaveDataHeapMemory(292);
	if (!resolvedPath) {
		result = (int)0x80801006;
		appmgrFreeSaveDataHeapMemory(path);
		goto restorePermission;
	}
	if (g_appmgrSaveDataSlotCache.processId == sceKernelGetProcessId()) {
		fd = g_appmgrSaveDataSlotCache.fd;
		if (fd != -1 &&
		    strncmp(g_appmgrSaveDataSlotCache.mountPoint, mountPoint, 16) == 0) {
			result = sceIoClose(fd);
			if (result < 0)
				goto freePaths;
			g_appmgrSaveDataSlotCache.fd = -1;
		}
	}
	result = appmgrCheckSaveDataSlot(slot->slotId, mountPoint, 1);
	if (result != 0)
		goto freePaths;
	appmgrNotifySaveDataSlotRead(slot->slotId);
	memset(path, 0, 128);
	memset(resolvedPath, 0, 292);
	snprintf(path, 128, "%s%s/%s", mountPoint, "sce_sys", "sdslot.dat");
	appmgrResolvePath(path, 0, resolvedPath, 292);
	fd = sceIoOpen(resolvedPath, 1, 390);
	if (fd < 0) {
		result = fd == (int)0x80010013 ? fd : (int)0x80801087;
		goto freePaths;
	}
	if (sceIoLseek(fd, (SceOff)((slot->slotId + 1) << 10), 0) < 0 ||
	    sceIoRead(fd, &slot->slotParam, sizeof(slot->slotParam)) != sizeof(slot->slotParam)) {
		result = (int)0x80801088;
		sceIoClose(fd);
		goto freePaths;
	}
	sceIoClose(fd);
	if (slot->slotId == (SceUInt32)appmgrGetFakeSdSlotBroken())
		slot->slotParam.status = 1;
freePaths:
	appmgrFreeSaveDataHeapMemory(path);
	appmgrFreeSaveDataHeapMemory(resolvedPath);
restorePermission:
	sceKernelSetThreadAccessLevel(permission);
	SceKernelSuspendForDriver_2BB92967(0);
	sceKernelUnlockMutex(g_appmgrSaveDataMutex, 1);
leaveSyscall:
	sceSyscallLeave(syscallState);
	return result;
}

int SceAppMgrForDriver_9ADB00FA(void *parameters)
{
	const AppmgrSaveDataSlotStatusParam *slot = parameters;
	SceAppMgrAppInfoCache *info;
	SceIoStat stat;
	char path[64];
	void *storage;
	SceUInt32 syscallState;
	const char *mountPoint;
	SceUID fd;
	int permission;
	int result;

	syscallState = sceSyscallEnter();
	if (!slot || slot->size != sizeof(*slot)) {
		result = (int)0x80800001;
		goto leaveSyscall;
	}
	mountPoint = (const char *)slot->mountPoint.data;
	result = appmgrValidateSaveDataMountPoint(mountPoint, 1);
	if (result < 0)
		goto leaveSyscall;
	SceKernelSuspendForDriver_4DF40893(0);
	result = sceKernelLockMutex(g_appmgrSaveDataMutex, 1, NULL);
	if (result < 0)
		goto unlockPower;
	permission = sceKernelSetThreadAccessLevel(128);
	if (g_appmgrSaveDataSlotCache.processId == sceKernelGetProcessId()) {
		fd = g_appmgrSaveDataSlotCache.fd;
		if (fd != -1 &&
		    strncmp(g_appmgrSaveDataSlotCache.mountPoint, mountPoint, 16) == 0) {
			result = sceIoClose(fd);
			if (result < 0)
				goto restorePermission;
			g_appmgrSaveDataSlotCache.fd = -1;
		}
	}
	result = appmgrCheckSaveDataSlot(slot->slotId, mountPoint, 1);
	if (result != 0)
		goto restorePermission;
	result = appmgrSetSaveDataSlotStatus(slot->slotId, slot->status, mountPoint);
	if (result != 0)
		goto restorePermission;
	storage = NULL;
	if (SceProcessmgrForDriver_AF80F39C(0, g_appmgrAppInfoCacheKey,
						    &storage, 0) >= 0 && storage) {
		info = storage;
		info->saveDataFlags |= 2;
		if (SceRtcForDriver_A930CD1A(&stat.st_mtime, 0) >= 0 &&
		    appmgrResolvePath(mountPoint, 0, path, sizeof(path)) >= 0)
			sceIoChstat(path, &stat, 32);
	}
	result = 0;
restorePermission:
	sceKernelSetThreadAccessLevel(permission);
	sceKernelUnlockMutex(g_appmgrSaveDataMutex, 1);
unlockPower:
	SceKernelSuspendForDriver_2BB92967(0);
leaveSyscall:
	sceSyscallLeave(syscallState);
	return result;
}

int SceAppMgrForDriver_EC9D5A35(void *parameters)
{
	AppmgrSaveDataSlotStatusParam *slot = parameters;
	char path[128];
	char resolvedPath[292];
	SceUInt32 syscallState;
	const char *mountPoint;
	SceUID fd;
	int permission;
	int result;

	syscallState = sceSyscallEnter();
	if (!slot || slot->size != sizeof(*slot)) {
		result = (int)0x80800001;
		goto leaveSyscall;
	}
	mountPoint = (const char *)slot->mountPoint.data;
	result = appmgrValidateSaveDataMountPoint(mountPoint, 0);
	if (result < 0)
		goto leaveSyscall;
	SceKernelSuspendForDriver_4DF40893(0);
	result = sceKernelLockMutex(g_appmgrSaveDataMutex, 1, NULL);
	if (result < 0)
		goto unlockPower;
	permission = sceKernelSetThreadAccessLevel(128);
	if (g_appmgrSaveDataSlotCache.processId == sceKernelGetProcessId()) {
		fd = g_appmgrSaveDataSlotCache.fd;
		if (fd != -1 &&
		    strncmp(g_appmgrSaveDataSlotCache.mountPoint, mountPoint, 16) == 0) {
			result = sceIoClose(fd);
			if (result < 0)
				goto restorePermission;
			g_appmgrSaveDataSlotCache.fd = -1;
		}
	}
	result = appmgrCheckSaveDataSlot(slot->slotId, mountPoint, 1);
	if (result != 0)
		goto restorePermission;
	appmgrNotifySaveDataSlotRead(slot->slotId);
	memset(path, 0, sizeof(path));
	snprintf(path, sizeof(path), "%s%s/%s", mountPoint, "sce_sys", "sdslot.dat");
	appmgrResolvePath(path, 0, resolvedPath, sizeof(resolvedPath));
	fd = sceIoOpen(resolvedPath, 1, 390);
	if (fd < 0) {
		result = fd == (int)0x80010013 ? fd : (int)0x80801087;
		goto restorePermission;
	}
	if (sceIoLseek(fd, (SceOff)((slot->slotId + 1) << 10), 0) < 0 ||
	    sceIoRead(fd, &slot->status, sizeof(slot->status)) != sizeof(slot->status)) {
		sceIoClose(fd);
		result = (int)0x80801088;
		goto restorePermission;
	}
	sceIoClose(fd);
	if (slot->slotId == (SceUInt32)appmgrGetFakeSdSlotBroken())
		slot->status = 1;
	result = 0;
restorePermission:
	sceKernelSetThreadAccessLevel(permission);
	sceKernelUnlockMutex(g_appmgrSaveDataMutex, 1);
unlockPower:
	SceKernelSuspendForDriver_2BB92967(0);
leaveSyscall:
	sceSyscallLeave(syscallState);
	return result;
}

int appmgrOpenSaveDataSlotFile(const char *mountPoint,
			      AppmgrSaveDataSlotFileInfo *info)
{
	SceUInt32 syscallState;
	SceUInt32 version;
	SceUInt32 slotCount;
	int permission;
	int result;

	syscallState = sceSyscallEnter();
	SceKernelSuspendForDriver_4DF40893(0);
	if (!mountPoint) {
		result = (int)0x80800001;
		goto unlockPower;
	}
	result = sceKernelLockMutex(g_appmgrSaveDataMutex, 1, NULL);
	if (result < 0)
		goto unlockPower;
	result = (int)0x80801091;
	permission = sceKernelSetThreadAccessLevel(128);
	if (g_appmgrSaveDataSlotCache.processId != 0)
		goto restorePermission;
	if (sceKernelGetProcessId() == 0 &&
	    g_appmgrSaveDataSlotCache.fd != -1 &&
	    strncmp(g_appmgrSaveDataSlotCache.mountPoint, mountPoint, 16) == 0) {
		result = sceIoClose(g_appmgrSaveDataSlotCache.fd);
		if (result < 0)
			goto restorePermission;
		g_appmgrSaveDataSlotCache.fd = -1;
	}
	result = appmgrLoadSaveDataSlotCache(mountPoint);
	if (result >= 0) {
		g_appmgrSaveDataSlotCache.processId = sceKernelGetProcessId();
		memset(g_appmgrSaveDataSlotCache.mountPoint, 0, 16);
		strncpy(g_appmgrSaveDataSlotCache.mountPoint, mountPoint, 15);
		if (info) {
			memset(info, 0, sizeof(*info));
			version = g_appmgrSaveDataSlotCache.header->version;
			slotCount = g_appmgrSaveDataSlotCache.header->slotCount;
			info->version = version;
			info->slotCount = slotCount;
			memcpy(info->occupied,
			       g_appmgrSaveDataSlotCache.header->occupied,
			       sizeof(info->occupied));
		}
	}
restorePermission:
	sceKernelSetThreadAccessLevel(permission);
	sceKernelUnlockMutex(g_appmgrSaveDataMutex, 1);
unlockPower:
	SceKernelSuspendForDriver_2BB92967(0);
	sceSyscallLeave(syscallState);
	return result;
}

int appmgrSaveDataSlotFileGetParam(unsigned int slotId, void *param)
{
	SceAppMgrSaveDataSlotParam *slot = param;
	SceUInt32 syscallState;
	SceUInt32 windowIndex;
	SceSize windowLength;
	SceSize end;
	SceOff offset;
	int permission;
	int result;
	SceUID fd;

	syscallState = sceSyscallEnter();
	if (slotId > 255 || !slot) {
		result = (int)0x80800001;
		goto leaveSyscall;
	}
	result = sceKernelLockMutex(g_appmgrSaveDataMutex, 1, NULL);
	if (result < 0)
		goto leaveSyscall;
	if (g_appmgrSaveDataSlotCache.processId != sceKernelGetProcessId()) {
		result = (int)0x80801090;
		goto unlockMutex;
	}
	if (!g_appmgrSaveDataSlotCache.header->occupied[slotId]) {
		result = (int)0x8080108b;
		goto unlockMutex;
	}
	windowIndex = (slotId + 1) >> 5;
	if (g_appmgrSaveDataSlotCache.windowIndex == windowIndex) {
		windowLength = g_appmgrSaveDataSlotCache.windowLength;
	} else {
		if (g_appmgrSaveDataSlotCache.fd == -1) {
			permission = sceKernelSetThreadAccessLevel(128);
			if (g_appmgrSaveDataSlotCache.processId == sceKernelGetProcessId()) {
				fd = g_appmgrSaveDataSlotCache.fd;
				if (fd != -1 &&
				    strncmp(g_appmgrSaveDataSlotCache.mountPoint,
					    g_appmgrSaveDataSlotCache.mountPoint, 16) == 0) {
					result = sceIoClose(fd);
					if (result < 0) {
						sceKernelSetThreadAccessLevel(permission);
						goto unlockMutex;
					}
					g_appmgrSaveDataSlotCache.fd = -1;
				}
			}
			result = appmgrLoadSaveDataSlotCache(
				g_appmgrSaveDataSlotCache.mountPoint);
			sceKernelSetThreadAccessLevel(permission);
			if (result < 0)
				goto unlockMutex;
		}
		permission = sceKernelSetThreadAccessLevel(128);
		offset = sceIoLseek(g_appmgrSaveDataSlotCache.fd,
				     (SceOff)(windowIndex << 15), 0);
		sceKernelSetThreadAccessLevel(permission);
		if (offset < 0)
			goto readError;
		permission = sceKernelSetThreadAccessLevel(128);
		result = sceIoRead(g_appmgrSaveDataSlotCache.fd,
				    g_appmgrSaveDataSlotCache.readWindow, 32768);
		sceKernelSetThreadAccessLevel(permission);
		if (result < 0)
			goto readError;
		windowLength = result;
		g_appmgrSaveDataSlotCache.windowIndex = windowIndex;
		g_appmgrSaveDataSlotCache.windowLength = result;
	}
	end = (((slotId + 1) & 31) + 1) << 10;
	if (end > windowLength)
		goto readError;
	memcpy(slot, (const char *)g_appmgrSaveDataSlotCache.readWindow +
	       end - sizeof(*slot), sizeof(*slot));
	if (appmgrGetFakeSdSlotBroken() == slotId)
		slot->status = 1;
	result = 0;
	goto unlockMutex;
readError:
	result = (int)0x80801088;
unlockMutex:
	sceKernelUnlockMutex(g_appmgrSaveDataMutex, 1);
leaveSyscall:
	sceSyscallLeave(syscallState);
	return result;
}

int appmgrCloseSaveDataSlotFile(const char *mountPoint)
{
	SceUInt32 syscallState;
	int permission;
	int result;

	syscallState = sceSyscallEnter();
	SceKernelSuspendForDriver_4DF40893(0);
	if (!mountPoint) {
		result = (int)0x80800001;
		goto unlockPower;
	}
	result = sceKernelLockMutex(g_appmgrSaveDataMutex, 1, NULL);
	if (result < 0)
		goto unlockPower;
	if (g_appmgrSaveDataSlotCache.processId != sceKernelGetProcessId() ||
	    strncmp(g_appmgrSaveDataSlotCache.mountPoint, mountPoint, 16) != 0) {
		result = (int)0x80801090;
		goto unlockMutex;
	}
	if (g_appmgrSaveDataSlotCache.fd != -1) {
		permission = sceKernelSetThreadAccessLevel(128);
		result = sceIoClose(g_appmgrSaveDataSlotCache.fd);
		sceKernelSetThreadAccessLevel(permission);
		if (result < 0)
			goto unlockMutex;
	}
	g_appmgrSaveDataSlotCache.windowIndex = 0;
	g_appmgrSaveDataSlotCache.windowLength = 0;
	g_appmgrSaveDataSlotCache.processId = 0;
	g_appmgrSaveDataSlotCache.fd = -1;
	memset(g_appmgrSaveDataSlotCache.header, 0, 1024);
	memset(g_appmgrSaveDataSlotCache.readWindow, 0, 32768);
unlockMutex:
	sceKernelUnlockMutex(g_appmgrSaveDataMutex, 1);
unlockPower:
	SceKernelSuspendForDriver_2BB92967(0);
	sceSyscallLeave(syscallState);
	return result;
}

int appmgrSaveDataDataSave(SceAppMgrSaveDataData *operation, int normalApplication)
{
	SceAppUtilSaveDataDataSaveItem file;
	SceAppMgrAppInfoCache *info;
	SceAllocOpt option;
	SceIoStat stat;
	SceIoStat clockStat;
	char path[210];
	char resolvedPath[292];
	char mountDevice[32];
	char mountPath[64];
	char *directories[20];
	int createdFiles[5];
	SceOff originalSizes[5];
	const char *mountPoint;
	char *filePath;
	char *parentPath;
	void *storage;
	void *mappedPage;
	unsigned char *buffer;
	SceUInt64 allocationUnit;
	SceOff totalBytes;
	SceUInt64 quotaBytes;
	SceOff freeBytes;
	SceUInt64 requiredBytes;
	SceUInt64 totalRequiredBytes;
	SceOff quotaRemaining;
	SceUInt64 writtenBytes;
	SceUInt64 growth;
	SceOff fileEnd;
	SceSize mappedSize;
	SceUInt32 mappedOffset;
	SceUInt32 syscallState;
	SceUID mapping;
	SceUID fd;
	unsigned int index;
	unsigned int directoryIndex;
	unsigned int missingFilesystemKiB;
	int permission;
	int allFilesEmpty;
	int quotaExceeded;
	int slotCreated;
	int changed;
	int setting;
	int value;
	int result;
	int bytes;

	syscallState = sceSyscallEnter();
	if (normalApplication) {
		if (SceSblACMgrForDriver_0924896F(0)) {
			result = (int)0x80800009;
			goto leaveSyscall;
		}
	} else if (!SceSblACMgrForDriver_0924896F(0)) {
		result = (int)0x80800009;
		goto leaveSyscall;
	}
	if (!operation || operation->size != sizeof(*operation)) {
		result = (int)0x80800001;
		goto leaveSyscall;
	}
	mountPoint = (const char *)operation->mountPoint.data;
	result = appmgrValidateSaveDataMountPoint(mountPoint, 1);
	if (result < 0)
		goto leaveSyscall;
	SceKernelSuspendForDriver_4DF40893(0);
	result = sceKernelLockMutex(g_appmgrSaveDataMutex, 1, NULL);
	if (result < 0)
		goto unlockPower;
	permission = sceKernelSetThreadAccessLevel(128);
	memset(path, 0, sizeof(path));
	memset(resolvedPath, 0, sizeof(resolvedPath));
	memset(mountDevice, 0, sizeof(mountDevice));
	allocationUnit = 0;
	totalBytes = 0;
	quotaBytes = 0;
	freeBytes = 0;
	snprintf(path, sizeof(path), "%s%s", mountPoint, "sce_sys");
	appmgrResolvePath(path, 0, resolvedPath, sizeof(resolvedPath));
	appmgrExtractSaveDataDeviceName(resolvedPath, mountDevice, sizeof(mountDevice));
	operation->requiredSizeKiB = 0;
	if (strncmp(mountDevice, "host0:", 6) != 0) {
		result = appmgrGetSaveDataDeviceSpace(mountDevice, &allocationUnit,
						     &totalBytes, &quotaBytes, &freeBytes);
		if (result != 0)
			goto restorePermission;
	} else {
		allocationUnit = 32768;
		totalBytes = 0;
		quotaBytes = 0xFFFFFFFFULL;
		freeBytes = 0xFFFFFFFFLL;
	}
	value = SceSblAIMgrForDriver_D78B04A2();
	if (!value) {
		if (sceRegMgrSystemParamGetInt(0x154A2C, &setting) == 0 && setting == 1) {
			missingFilesystemKiB = 2048;
			freeBytes = 0;
			goto checkQuota;
		}
		value = SceSblAIMgrForDriver_D78B04A2();
	}
	if (freeBytes <= 0x200000) {
		missingFilesystemKiB = (0x200000 - freeBytes) >> 10;
checkQuota:
		value = SceSblAIMgrForDriver_D78B04A2();
	} else {
		missingFilesystemKiB = 0;
	}
	if (!value &&
	    sceRegMgrSystemParamGetInt(0x25CE9A, &value) == 0 && value == 1)
		quotaBytes = 0;
	quotaRemaining = quotaBytes;
	totalRequiredBytes = 0;
	quotaExceeded = 0;
	allFilesEmpty = 1;
	memset(directories, 0, sizeof(directories));
	for (index = 0; index < 5 && operation->itemNum > index; index++) {
		requiredBytes = 0;
		result = appmgrCalculateSaveDataFileSpace(operation->items + index,
			directories, &requiredBytes, 32768, mountPoint);
		if (result != 0)
			goto freeDirectories;
		if (quotaRemaining < (SceOff)requiredBytes)
			quotaExceeded = 1;
		if ((SceOff)requiredBytes >= 1)
			allFilesEmpty = 0;
		if ((SceOff)requiredBytes < quotaRemaining)
			quotaRemaining -= requiredBytes;
		else
			quotaRemaining = 0;
		totalRequiredBytes += requiredBytes;
	}
	if (quotaExceeded) {
		operation->requiredSizeKiB = (totalRequiredBytes - quotaBytes) >> 10;
		storage = NULL;
		if (SceProcessmgrForDriver_AF80F39C(0,
			g_appmgrAppInfoCacheKey, &storage, 0) >= 0 && storage) {
			info = storage;
			info->neededSaveDataKiB = operation->requiredSizeKiB;
		}
		appmgrNotifySaveDataSpaceError((int)0x8080108C,
					      operation->requiredSizeKiB);
		result = (int)0x8080108C;
		goto freeDirectories;
	}
	if (missingFilesystemKiB && !allFilesEmpty) {
		operation->requiredSizeKiB = missingFilesystemKiB;
		storage = NULL;
		if (SceProcessmgrForDriver_AF80F39C(0,
			g_appmgrAppInfoCacheKey, &storage, 0) >= 0 && storage) {
			info = storage;
			info->neededSaveDataKiB = missingFilesystemKiB;
		}
		appmgrNotifySaveDataSpaceError((int)0x8080108D,
					      operation->requiredSizeKiB);
		result = (int)0x8080108D;
		goto freeDirectories;
	}
	slotCreated = 0;
	changed = 0;
	if (operation->slotId != (unsigned int)-1) {
		if (g_appmgrSaveDataSlotCache.processId == sceKernelGetProcessId()) {
			fd = g_appmgrSaveDataSlotCache.fd;
			if (fd != -1 &&
			    strncmp(g_appmgrSaveDataSlotCache.mountPoint, mountPoint, 16) == 0) {
				result = sceIoClose(fd);
				if (result < 0)
					goto freeDirectories;
				g_appmgrSaveDataSlotCache.fd = -1;
			}
		}
		result = appmgrCheckSaveDataSlot(operation->slotId, mountPoint, 1);
		if (result != 0) {
			if (result != (int)0x8080108B)
				goto freeDirectories;
			if (!operation->slotParam) {
				result = (int)0x8080108E;
				goto freeDirectories;
			}
			mappedPage = NULL;
			mappedSize = 0;
			mappedOffset = 0;
			mapping = sceKernelUserMap("SceAppMgrSaveDataSlotParam", 1,
				operation->slotParam, 1024, &mappedPage, &mappedSize,
				&mappedOffset);
			if (mapping < 0) {
				result = mapping;
				goto freeDirectories;
			}
			result = appmgrWriteSaveDataSlot(operation->slotId,
				(SceAppMgrSaveDataSlotParam *)((char *)mappedPage + mappedOffset),
				1, mountPoint);
			sceKernelUserUnmap(mapping, operation->slotParam, 1024);
			if (result != 0)
				goto freeDirectories;
			slotCreated = 1;
		} else {
			result = appmgrSetSaveDataSlotStatus(operation->slotId, 1, mountPoint);
			if (result != 0)
				goto freeDirectories;
		}
		storage = NULL;
		if (SceProcessmgrForDriver_AF80F39C(0,
			g_appmgrAppInfoCacheKey, &storage, 0) >= 0 && storage) {
			info = storage;
			info->saveDataFlags |= 2;
			if (SceRtcForDriver_A930CD1A(&clockStat.st_mtime, 0) >= 0 &&
			    appmgrResolvePath(mountPoint, 0, mountPath, sizeof(mountPath)) >= 0)
				sceIoChstat(mountPath, &clockStat, 32);
		}
		if (!(operation->flags & 1)) {
			result = sceIoSync(mountDevice, 0);
			if (result != 0)
				goto freeDirectories;
		}
		changed = 1;
	}
	for (index = 0; index < 5 && operation->itemNum > index; index++) {
		createdFiles[index] = 0;
		memset(&file, 0, sizeof(file));
		result = sceKernelCopyFromUser(&file, operation->items + index, sizeof(file));
		if (result != 0)
			goto freeDirectories;
		option.size = sizeof(option);
		option.align = 32;
		filePath = sceKernelAllocHeapMemoryWithOption(g_appmgrSaveDataHeap, 200, &option);
		if (!filePath) {
			result = (int)0x80801006;
			goto freeDirectories;
		}
		memset(filePath, 0, 200);
		result = sceKernelStrncpyFromUser(filePath, (const char *)file.dataPath, 199);
		if (result < 0)
			goto freeFirstFilePath;
		result = appmgrValidateSaveDataFile(&file, filePath);
		if (result < 0)
			goto freeFirstFilePath;
		memset(path, 0, sizeof(path));
		memset(resolvedPath, 0, sizeof(resolvedPath));
		option.size = sizeof(option);
		option.align = 32;
		parentPath = sceKernelAllocHeapMemoryWithOption(g_appmgrSaveDataHeap, 292, &option);
		if (!parentPath) {
			result = (int)0x80801006;
			goto freeFirstFilePath;
		}
		memset(parentPath, 0, 292);
		snprintf(path, sizeof(path), "%s%s", mountPoint, filePath);
		appmgrResolvePath(path, 0, resolvedPath, sizeof(resolvedPath));
		if (file.mode == 2) {
			strncpy(parentPath, resolvedPath, 292);
			value = 1;
		} else {
			value = appmgrGetSaveDataParentPath(resolvedPath, parentPath);
		}
		if (value != 0) {
			memset(&stat, 0, sizeof(stat));
			result = sceIoGetstat(parentPath, &stat);
			originalSizes[index] = stat.st_size;
			if (result < 0) {
				result = appmgrCreateSaveDataDirectories(parentPath, 390);
				if (result < 0)
					goto firstWriteError;
				if (!changed) {
					storage = NULL;
					if (SceProcessmgrForDriver_AF80F39C(0,
						g_appmgrAppInfoCacheKey, &storage, 0) >= 0 && storage) {
						info = storage;
						info->saveDataFlags |= 2;
						if (SceRtcForDriver_A930CD1A(&clockStat.st_mtime, 0) >= 0 &&
						    appmgrResolvePath(mountPoint, 0, mountPath, sizeof(mountPath)) >= 0)
							sceIoChstat(mountPath, &clockStat, 32);
					}
				}
				changed = 1;
			}
		}
		if (file.mode != 2) {
			memset(&stat, 0, sizeof(stat));
			result = sceIoGetstat(resolvedPath, &stat);
			originalSizes[index] = stat.st_size;
			if (result < 0)
				createdFiles[index] = 1;
			fd = sceIoOpen(resolvedPath, 514, 390);
			if (fd < 0) {
				result = fd;
				goto firstWriteError;
			}
			if (file.bufSize) {
				if (file.buf) {
					mappedPage = NULL;
					mappedSize = 0;
					mappedOffset = 0;
					mapping = sceKernelUserMap("SceAppMgrSaveDataBuf", 1,
						file.buf, file.bufSize, &mappedPage, &mappedSize, &mappedOffset);
					if (mapping < 0) {
						result = mapping;
						sceIoClose(fd);
						goto freeFirstParentPath;
					}
					buffer = (unsigned char *)mappedPage + mappedOffset;
					fileEnd = file.offset + file.bufSize;
					bytes = file.bufSize;
					if (createdFiles[index] == 1 ||
					    originalSizes[index] <= file.offset) {
						bytes = sceIoPwrite(fd, buffer, file.bufSize, file.offset);
						value = file.bufSize;
					} else if (originalSizes[index] < fileEnd) {
						value = file.offset + file.bufSize - originalSizes[index];
						bytes = sceIoPwrite(fd,
							buffer + originalSizes[index] - file.offset,
							value, originalSizes[index]);
					} else {
						value = bytes;
					}
					sceKernelUserUnmap(mapping, file.buf, file.bufSize);
					if (bytes != value) {
						result = bytes;
						sceIoClose(fd);
						if (result != (int)0x8001001C)
							result = (int)0x8080108F;
						goto firstWriteError;
					}
				}
			} else if (createdFiles[index] == 1) {
				memset(&stat, 0, sizeof(stat));
				stat.st_size = file.offset;
				result = sceIoChstat(resolvedPath, &stat, 4);
				if (result < 0) {
					sceIoClose(fd);
					goto firstWriteError;
				}
			}
			sceIoClose(fd);
			if (!changed) {
				storage = NULL;
				if (SceProcessmgrForDriver_AF80F39C(0,
					g_appmgrAppInfoCacheKey, &storage, 0) >= 0 && storage) {
					info = storage;
					info->saveDataFlags |= 2;
					if (SceRtcForDriver_A930CD1A(&clockStat.st_mtime, 0) >= 0 &&
					    appmgrResolvePath(mountPoint, 0, mountPath, sizeof(mountPath)) >= 0)
						sceIoChstat(mountPath, &clockStat, 32);
				}
			}
			changed = 1;
		}
		sceKernelFreeHeapMemory(g_appmgrSaveDataHeap, parentPath);
		sceKernelFreeHeapMemory(g_appmgrSaveDataHeap, filePath);
	}
	writtenBytes = 0;
	for (directoryIndex = 0; directoryIndex < 20; directoryIndex++) {
		if (directories[directoryIndex]) {
			sceKernelFreeHeapMemory(g_appmgrSaveDataHeap, directories[directoryIndex]);
			directories[directoryIndex] = NULL;
			writtenBytes += allocationUnit;
		}
	}
	for (index = 0; index < 5 && operation->itemNum > index; index++) {
		memset(&file, 0, sizeof(file));
		result = sceKernelCopyFromUser(&file, operation->items + index, sizeof(file));
		if (result != 0)
			goto restorePermission;
		option.size = sizeof(option);
		option.align = 32;
		filePath = sceKernelAllocHeapMemoryWithOption(g_appmgrSaveDataHeap, 200, &option);
		if (!filePath) {
			result = (int)0x80801006;
			goto restorePermission;
		}
		memset(filePath, 0, 200);
		result = sceKernelStrncpyFromUser(filePath, (const char *)file.dataPath, 199);
		if (result < 0)
			goto freeSecondFilePath;
		result = appmgrValidateSaveDataFile(&file, filePath);
		if (result < 0)
			goto freeSecondFilePath;
		memset(path, 0, sizeof(path));
		memset(resolvedPath, 0, sizeof(resolvedPath));
		snprintf(path, sizeof(path), "%s%s", mountPoint, filePath);
		appmgrResolvePath(path, 0, resolvedPath, sizeof(resolvedPath));
		if (file.mode != 2) {
			fd = sceIoOpen(resolvedPath, 2, 390);
			if (fd < 0) {
				result = fd;
				goto freeSecondFilePath;
			}
			if (file.buf && file.bufSize) {
				mappedPage = NULL;
				mappedSize = 0;
				mappedOffset = 0;
				mapping = sceKernelUserMap("SceAppMgrSaveDataBuf", 1,
					file.buf, file.bufSize, &mappedPage, &mappedSize, &mappedOffset);
				if (mapping < 0) {
					result = mapping;
					sceIoClose(fd);
					goto freeSecondFilePath;
				}
				if (!createdFiles[index]) {
					buffer = (unsigned char *)mappedPage + mappedOffset;
					fileEnd = file.offset + file.bufSize;
					bytes = file.bufSize;
					value = file.bufSize;
					if (originalSizes[index] >= fileEnd) {
						bytes = sceIoPwrite(fd, buffer, file.bufSize, file.offset);
					} else if (file.offset < originalSizes[index]) {
						value = originalSizes[index] - file.offset;
						bytes = sceIoPwrite(fd, buffer, value, file.offset);
					}
					if (bytes != value) {
						sceKernelUserUnmap(mapping, file.buf, file.bufSize);
						sceIoClose(fd);
						result = (int)0x8080108F;
						goto freeSecondFilePath;
					}
					if (!changed) {
						storage = NULL;
						if (SceProcessmgrForDriver_AF80F39C(0,
							g_appmgrAppInfoCacheKey, &storage, 0) >= 0 && storage) {
							info = storage;
							info->saveDataFlags |= 2;
							if (SceRtcForDriver_A930CD1A(&clockStat.st_mtime, 0) >= 0 &&
							    appmgrResolvePath(mountPoint, 0, mountPath, sizeof(mountPath)) >= 0)
								sceIoChstat(mountPath, &clockStat, 32);
						}
					}
					changed = 1;
				}
				sceKernelUserUnmap(mapping, file.buf, file.bufSize);
			}
			sceIoClose(fd);
			if (file.mode == 1) {
				memset(&stat, 0, sizeof(stat));
				stat.st_size = file.offset + file.bufSize;
				result = sceIoChstat(resolvedPath, &stat, 4);
				if (result < 0)
					goto freeSecondFilePath;
			}
			__builtin_prefetch(originalSizes + index + 1);
			if (originalSizes[index] < file.offset && file.bufSize) {
				growth = file.offset - originalSizes[index] + file.bufSize;
				if (growth % allocationUnit)
					growth = (SceUInt32)((growth / allocationUnit + 1) * allocationUnit);
				else
					growth = (SceUInt32)growth;
			} else if (file.bufSize) {
				growth = file.bufSize;
				if (growth % allocationUnit)
					growth = (SceUInt32)((growth / allocationUnit + 1) * allocationUnit);
			} else {
				growth = allocationUnit;
			}
			writtenBytes += growth;
		}
		sceKernelFreeHeapMemory(g_appmgrSaveDataHeap, filePath);
		appmgrAccumulateSaveDataWriteSize(writtenBytes >> 10);
	}
	if (operation->slotId != (unsigned int)-1) {
		if (!slotCreated && operation->slotParam) {
			mappedPage = NULL;
			mappedSize = 0;
			mappedOffset = 0;
			mapping = sceKernelUserMap("SceAppMgrSaveDataSlotParam", 1,
				operation->slotParam, 1024, &mappedPage, &mappedSize, &mappedOffset);
			if (mapping < 0) {
				result = mapping;
				goto restorePermission;
			}
			result = appmgrOverwriteSaveDataSlot(operation->slotId,
				(SceAppMgrSaveDataSlotParam *)((char *)mappedPage + mappedOffset),
				0, mountPoint);
			sceKernelUserUnmap(mapping, operation->slotParam, 1024);
			if (result != 0)
				goto restorePermission;
		} else {
			result = appmgrSetSaveDataSlotStatus(operation->slotId, 0, mountPoint);
			if (result != 0)
				goto restorePermission;
		}
	}
	result = 0;
	if (!(operation->flags & 1))
		result = sceIoSync(mountDevice, 0);
	goto restorePermission;
firstWriteError:
	if (result == (int)0x8001001C) {
		appmgrRollbackSaveDataOperation(operation, (const char *const *)directories,
			originalSizes, createdFiles, allocationUnit, slotCreated,
			&operation->requiredSizeKiB);
		appmgrSetRequiredSaveDataSize(operation->requiredSizeKiB);
		appmgrNotifySaveDataSpaceError((int)0x8080108D, operation->requiredSizeKiB);
		result = (int)0x8080108D;
	}
freeFirstParentPath:
	sceKernelFreeHeapMemory(g_appmgrSaveDataHeap, parentPath);
freeFirstFilePath:
	sceKernelFreeHeapMemory(g_appmgrSaveDataHeap, filePath);
freeDirectories:
	appmgrFreeSaveDataDirectories(directories);
	goto restorePermission;
freeSecondFilePath:
	sceKernelFreeHeapMemory(g_appmgrSaveDataHeap, filePath);
restorePermission:
	sceKernelSetThreadAccessLevel(permission);
	sceKernelUnlockMutex(g_appmgrSaveDataMutex, 1);
unlockPower:
	SceKernelSuspendForDriver_2BB92967(0);
leaveSyscall:
	sceSyscallLeave(syscallState);
	return result;
}

int SceAppMgrForDriver_30947DF4(SceAppMgrSaveDataData *operation)
{
	return appmgrSaveDataDataSave(operation, 0);
}

int appmgrSaveDataDataRemove(SceAppMgrSaveDataDataDelete *operation, int normalApplication)
{
	SceAppUtilSaveDataDataRemoveItem item;
	SceAppMgrAppInfoCache *info;
	SceIoDevInfo deviceInfo;
	SceIoStat clockStat;
	char filePath[200];
	char path[210];
	char resolvedPath[292];
	char mountDevice[32];
	char mountPath[64];
	const char *mountPoint;
	void *storage;
	void *mappedPage;
	SceSize mappedSize;
	SceUInt32 mappedOffset;
	SceUInt32 syscallState;
	SceUInt32 clusterSize;
	SceUInt32 removedBytes;
	unsigned int entryCount;
	unsigned int index;
	SceUID mapping;
	SceUID fd;
	int permission;
	int slotChanged;
	int keepSlot;
	int changed;
	int result;

	syscallState = sceSyscallEnter();
	if (normalApplication) {
		if (SceSblACMgrForDriver_0924896F(0)) {
			result = (int)0x80800009;
			goto leaveSyscall;
		}
	} else if (!SceSblACMgrForDriver_0924896F(0)) {
		result = (int)0x80800009;
		goto leaveSyscall;
	}
	if (!operation || operation->size != sizeof(*operation)) {
		result = (int)0x80800001;
		goto leaveSyscall;
	}
	mountPoint = (const char *)operation->mountPoint.data;
	result = appmgrValidateSaveDataMountPoint(mountPoint, 1);
	if (result < 0)
		goto leaveSyscall;
	SceKernelSuspendForDriver_4DF40893(0);
	result = sceKernelLockMutex(g_appmgrSaveDataMutex, 1, NULL);
	if (result < 0)
		goto unlockPower;
	permission = sceKernelSetThreadAccessLevel(128);
	memset(path, 0, sizeof(path));
	memset(resolvedPath, 0, sizeof(resolvedPath));
	memset(mountDevice, 0, sizeof(mountDevice));
	snprintf(path, sizeof(path), "%s%s", mountPoint, "sce_sys");
	appmgrResolvePath(path, 0, resolvedPath, sizeof(resolvedPath));
	appmgrExtractSaveDataDeviceName(resolvedPath, mountDevice, sizeof(mountDevice));
	if (strncmp(mountDevice, "host0:", 6) != 0) {
		memset(&deviceInfo, 0, sizeof(deviceInfo));
		result = sceIoDevctl(mountDevice, 0x3001, NULL, 0, &deviceInfo, sizeof(deviceInfo));
		if (result != 0)
			goto restorePermission;
		clusterSize = deviceInfo.clusterSize;
	} else {
		clusterSize = 0x8000;
	}
	changed = 0;
	slotChanged = 0;
	if (operation->slotId != (unsigned int)-1) {
		if (g_appmgrSaveDataSlotCache.processId == sceKernelGetProcessId()) {
			fd = g_appmgrSaveDataSlotCache.fd;
			if (fd != -1 &&
			    strncmp(g_appmgrSaveDataSlotCache.mountPoint, mountPoint, 16) == 0) {
				result = sceIoClose(fd);
				if (result < 0)
					goto restorePermission;
				g_appmgrSaveDataSlotCache.fd = -1;
			}
		}
		result = appmgrCheckSaveDataSlot(operation->slotId, mountPoint, 1);
		if (result != 0) {
			if (result != (int)0x8080108B)
				goto restorePermission;
		} else {
			result = appmgrSetSaveDataSlotStatus(operation->slotId, 1, mountPoint);
			if (result != 0)
				goto restorePermission;
			storage = NULL;
			if (SceProcessmgrForDriver_AF80F39C(0,
				g_appmgrAppInfoCacheKey, &storage, 0) >= 0 && storage) {
				info = storage;
				info->saveDataFlags |= 2;
				if (SceRtcForDriver_A930CD1A(&clockStat.st_mtime, 0) >= 0 &&
				    appmgrResolvePath(mountPoint, 0, filePath, 64) >= 0)
					sceIoChstat(filePath, &clockStat, 32);
			}
			slotChanged = 1;
			changed = 1;
		}
	}
	keepSlot = 0;
	removedBytes = 0;
	for (index = 0; index < 5 && operation->itemNum > index; index++) {
		memset(&item, 0, sizeof(item));
		memset(filePath, 0, sizeof(filePath));
		entryCount = 0;
		result = sceKernelCopyFromUser(&item, operation->items + index, sizeof(item));
		if (result != 0)
			goto restorePermission;
		result = sceKernelStrncpyFromUser(filePath, (const char *)item.dataPath, 199);
		if (result < 0)
			goto restorePermission;
		result = appmgrValidateSaveDataRemoveItem(&item, filePath);
		if (result < 0)
			goto restorePermission;
		if (item.mode == 1)
			keepSlot = 1;
		memset(path, 0, sizeof(path));
		memset(resolvedPath, 0, sizeof(resolvedPath));
		snprintf(path, sizeof(path), "%s%s", mountPoint, filePath);
		appmgrResolvePath(path, 0, resolvedPath, sizeof(resolvedPath));
		result = appmgrRemoveSaveDataPath(resolvedPath, item.mode, &entryCount);
		if (result < 0)
			goto restorePermission;
		if (!changed) {
			storage = NULL;
			if (SceProcessmgrForDriver_AF80F39C(0,
				g_appmgrAppInfoCacheKey, &storage, 0) >= 0 && storage) {
				info = storage;
				info->saveDataFlags |= 2;
				if (SceRtcForDriver_A930CD1A(&clockStat.st_mtime, 0) >= 0 &&
				    appmgrResolvePath(mountPoint, 0, mountPath, sizeof(mountPath)) >= 0)
					sceIoChstat(mountPath, &clockStat, 32);
			}
		}
		changed = 1;
		removedBytes += entryCount * clusterSize;
	}
	appmgrAccumulateSaveDataWriteSize(removedBytes >> 10);
	result = 0;
	if (!slotChanged)
		goto restorePermission;
	if (!keepSlot) {
		result = appmgrDeleteSaveDataSlot(operation->slotId, mountPoint);
	} else if (!operation->slotParam || operation->slotId == (unsigned int)-1) {
		result = appmgrSetSaveDataSlotStatus(operation->slotId, 0, mountPoint);
	} else {
		mappedPage = NULL;
		mappedSize = 0;
		mappedOffset = 0;
		mapping = sceKernelUserMap("SceAppMgrSaveDataSlotParam", 1,
			operation->slotParam, 1024, &mappedPage, &mappedSize, &mappedOffset);
		if (mapping < 0) {
			result = mapping;
			goto restorePermission;
		}
		result = appmgrOverwriteSaveDataSlot(operation->slotId,
			(SceAppMgrSaveDataSlotParam *)((char *)mappedPage + mappedOffset),
			0, mountPoint);
		sceKernelUserUnmap(mapping, operation->slotParam, 1024);
	}
restorePermission:
	sceKernelSetThreadAccessLevel(permission);
	sceKernelUnlockMutex(g_appmgrSaveDataMutex, 1);
unlockPower:
	SceKernelSuspendForDriver_2BB92967(0);
leaveSyscall:
	sceSyscallLeave(syscallState);
	return result;
}

int SceAppMgrForDriver_EBFE1BE2(SceAppMgrSaveDataDataDelete *operation)
{
	return appmgrSaveDataDataRemove(operation, 0);
}

int SceAppMgrForDriver_E104C37E(SceAppMgrSaveDataQuota *quota)
{
	char path[128];
	char resolvedPath[292];
	char mountDevice[32] = {0};
	const char *mountPoint;
	SceUInt32 syscallState;
	SceUInt32 quotaBytes;
	SceUInt32 usedBytes;
	int permission;
	int setting;
	int result;

	syscallState = sceSyscallEnter();
	if (!quota || quota->size != sizeof(*quota)) {
		result = (int)0x80800001;
		goto leaveSyscall;
	}
	mountPoint = (const char *)quota->mountPoint.data;
	result = appmgrValidateSaveDataMountPoint(mountPoint, 0);
	if (result < 0)
		goto leaveSyscall;
	SceKernelSuspendForDriver_4DF40893(0);
	result = sceKernelLockMutex(g_appmgrSaveDataMutex, 1, NULL);
	if (result < 0)
		goto unlockPower;
	permission = sceKernelSetThreadAccessLevel(128);
	memset(path, 0, sizeof(path));
	memset(resolvedPath, 0, sizeof(resolvedPath));
	snprintf(path, sizeof(path), "%s%s", mountPoint, "sce_sys");
	appmgrResolvePath(path, 0, resolvedPath, sizeof(resolvedPath));
	appmgrExtractSaveDataDeviceName(resolvedPath, mountDevice, sizeof(mountDevice));
	quota->quotaSizeKiB = 0;
	quota->usedSizeKiB = 0;
	quotaBytes = 0;
	usedBytes = 0;
	if (strncmp(mountDevice, "host0:", 6) != 0) {
		char partition[32] = {0};

#if FW_VERSION >= 0x04000000
		__memcpy_chk(partition, mountDevice, strnlen(mountDevice, sizeof(mountDevice)) - 2,
			     sizeof(partition));
#else
		memcpy(partition, mountDevice, strnlen(mountDevice, sizeof(mountDevice)) - 2);
#endif
		result = ScePfsMgrForKernel_27FB7618(partition, &quotaBytes, &usedBytes);
		if (result == 0) {
			quota->quotaSizeKiB = quotaBytes >> 10;
			quota->usedSizeKiB = usedBytes >> 10;
			goto checkFull;
		}
		if (result != (int)0x80010030)
			goto restorePermission;
	}
	result = SceAppMgrForDriver_12CE29F7(0, 11, (int *)&quotaBytes);
	if (result != 0)
		goto restorePermission;
	quota->quotaSizeKiB = quotaBytes;
checkFull:
	if (!SceSblAIMgrForDriver_D78B04A2() &&
	    sceRegMgrSystemParamGetInt(0x25CE9A, &setting) == 0 && setting == 1)
		quota->usedSizeKiB = quota->quotaSizeKiB;
	result = 0;
restorePermission:
	sceKernelSetThreadAccessLevel(permission);
	sceKernelUnlockMutex(g_appmgrSaveDataMutex, 1);
unlockPower:
	SceKernelSuspendForDriver_2BB92967(0);
leaveSyscall:
	sceSyscallLeave(syscallState);
	return result;
}

int SceAppMgrForDriver_E73C9516(const SceAppMgrSaveDataMountParam *param)
{
	SceAllocOpt option;
	SceUInt8 klicensee[16];
	char path[128];
	AppmgrSaveDataMount *mount;
	AppmgrSaveDataMount *entry;
	const char *titleId;
	const char *mountPoint;
	SceUInt32 syscallState;
	int result;

	syscallState = sceSyscallEnter();
	if (!param || param->size != sizeof(*param)) {
		result = (int)0x80800001;
		goto leaveSyscall;
	}
	result = sceKernelLockMutex(g_appmgrSaveDataMutex, 1, NULL);
	if (result < 0)
		goto leaveSyscall;
	titleId = param->titleId;
	mountPoint = (const char *)param->mountPoint.data;
	result = appmgrCheckSaveDataMount(titleId, mountPoint, g_appmgrSaveDataMountList);
	if (result != 0)
		goto unlock;
	memset(path, 0, sizeof(path));
	if (strncmp(mountPoint, "savedata0", 10) == 0 ||
	    strncmp(mountPoint, "savedata0:", 11) == 0)
		strncpy(path, "host0:savedata", sizeof(path) - 1);
	result = ScePfsMgrForKernel_A772209C(path, "savedata", 0, klicensee, 0);
	if (result != 0)
		goto unlock;
	option.size = sizeof(option);
	option.align = 32;
	mount = sceKernelAllocHeapMemoryWithOption(g_appmgrSaveDataHeap, sizeof(*mount), &option);
	if (!mount) {
		ScePfsMgrForKernel_680BC384("savedata");
		result = (int)0x80801006;
		goto unlock;
	}
	memset(mount, 0, sizeof(*mount));
	strncpy(mount->titleId, titleId, 9);
	mount->processId = sceKernelGetProcessId();
	strncpy(mount->mountPoint, mountPoint, 15);
	mount->next = g_appmgrSaveDataMountList;
	g_appmgrSaveDataMountList = mount;
	for (entry = mount->next; entry; entry = entry->next)
		;
	result = 0;
unlock:
	sceKernelUnlockMutex(g_appmgrSaveDataMutex, 1);
leaveSyscall:
	sceSyscallLeave(syscallState);
	return result;
}

int SceAppMgrForDriver_2B080268(const SceAppMgrSaveDataUmountParam *param)
{
	AppmgrSaveDataMount *mount;
	AppmgrSaveDataMount *previous;
	AppmgrSaveDataMount *entry;
	const char *mountPoint;
	SceUInt32 syscallState;
	SceUID processId;
	int result;

	syscallState = sceSyscallEnter();
	if (!param || param->size != sizeof(*param)) {
		result = (int)0x80800001;
		goto leaveSyscall;
	}
	result = sceKernelLockMutex(g_appmgrSaveDataMutex, 1, NULL);
	if (result < 0)
		goto leaveSyscall;
	mountPoint = (const char *)param->mountPoint.data;
	result = appmgrFindSaveDataMount(mountPoint, g_appmgrSaveDataMountList);
	if (result != 0)
		goto unlock;
	result = ScePfsMgrForKernel_680BC384("savedata");
	if (result != 0)
		goto unlock;
	mount = g_appmgrSaveDataMountList;
	processId = sceKernelGetProcessId();
	previous = NULL;
	for (; mount; previous = mount, mount = mount->next) {
		if (mount->processId == processId &&
		    strncmp(mountPoint, mount->mountPoint, 15) == 0) {
			if (previous)
				previous->next = mount->next;
			else
				g_appmgrSaveDataMountList = mount->next;
			sceKernelFreeHeapMemory(g_appmgrSaveDataHeap, mount);
			break;
		}
	}
	for (entry = g_appmgrSaveDataMountList; entry; entry = entry->next)
		;
unlock:
	sceKernelUnlockMutex(g_appmgrSaveDataMutex, 1);
leaveSyscall:
	sceSyscallLeave(syscallState);
	return result;
}

extern SceUID g_appmgrMainMutex;
extern SceUID g_appmgrMainMutexOwner;

#if FW_DEBUG
#define SAVEDATA_MAIN_LOCK(_line) do { \
	int lockResult = sceKernelLockMutex(g_appmgrMainMutex, 1, NULL); \
	if (lockResult != 0) { \
		SCE_DBG_PRINTF(0, _line, "Failed to lock ret=0x%08x\n", lockResult); \
		sceAppMgrDump(); \
		dumpBgmState(); \
		SCE_DBG_ASSERT_FAIL(_line); \
	} else { \
		g_appmgrMainMutexOwner = sceKernelGetThreadId(); \
	} \
} while (0)
#define SAVEDATA_MAIN_UNLOCK(_line) do { \
	int unlockResult; \
	g_appmgrMainMutexOwner = -1; \
	unlockResult = sceKernelUnlockMutex(g_appmgrMainMutex, 1); \
	if (unlockResult != 0) { \
		sceAppMgrDump(); \
		dumpBgmState(); \
		SCE_DBG_ASSERT_PRINTF(0, _line, \
			"[SceAppMgr] The actual value is %d(0x%08x), but expected value is %d(0x%08x)\n", \
			unlockResult, unlockResult, 0, 0); \
	} \
} while (0)
#else
static const SceKernelDebugInfo saveDataPanicInfo = {
	.fileHash = 0xc9656e8e,
	.lineHash = 0x79d3a1f0,
	.funcHash = 0x9d536bce,
};

static __attribute__((noinline)) _Noreturn void appmgrSaveDataPanic(void)
{
	SceDebugForDriver_391B5B74(&saveDataPanicInfo, __builtin_return_address(0));
}

#define SAVEDATA_MAIN_LOCK(_line) do { \
	if (sceKernelLockMutex(g_appmgrMainMutex, 1, NULL) != 0) \
		appmgrSaveDataPanic(); \
	g_appmgrMainMutexOwner = sceKernelGetThreadId(); \
} while (0)
#define SAVEDATA_MAIN_UNLOCK(_line) do { \
	g_appmgrMainMutexOwner = -1; \
	if (sceKernelUnlockMutex(g_appmgrMainMutex, 1) != 0) \
		appmgrSaveDataPanic(); \
} while (0)
#endif

/* 1 when titleId is one of the calling application's other save data titles. */
int sceAppMgrIsRelativeSaveDataAdd(const char *titleId)
{
	struct AppState *app;
	int i, result = 0;

	SAVEDATA_MAIN_LOCK(3983);
	app = appmgrFindAppByProcessId(g_appmgrAppStates, sceKernelGetProcessId());
	if (app != NULL &&
	    strncmp(app->process.content.saveDataTitleIds[0], titleId, 16) != 0) {
		for (i = 1; i < 8; i++) {
			if (app->process.content.saveDataTitleIds[i][0] == '\0')
				break;
			if (strncmp(app->process.content.saveDataTitleIds[i], titleId, 16) == 0) {
				result = 1;
				break;
			}
		}
	}
	SAVEDATA_MAIN_UNLOCK(4013);
	return result;
}

static __attribute__((noinline)) int appmgrIsAutoUploadApp(const char *titleId);
static __attribute__((noinline)) int appmgrAddAutoUploadSaveData(const char *saveDataTitleId,
		const char *titleId, const char *shortTitle, int mode);
static __attribute__((noinline)) int appmgrUpdateSaveDataBackupList(const char *saveDataTitleId,
		const char *titleId, int mode);
static __attribute__((noinline)) int appmgrRemoveSaveDataBackupListItem(const char *saveDataTitleId);
static __attribute__((noinline)) int appmgrGetSaveDataBackupList(void *list);

/*
 * The first suspend after the application wrote save data: records the save data in the
 * backup list and, when the application is in the auto-upload list, in the upload list.
 */
int appmgrUpdateSaveDataOnSuspend(SceUID pid, AppmgrProcessContent *content)
{
	SceAppMgrAppInfoCache *info = NULL;
	int mode;
	int result;

	result = sceKernelLockMutex(g_appmgrSaveDataMutex, 1, NULL);
	if (result < 0)
		return result;
	if ((content->contentFlags & 0x31) == 0)
		goto done;
	result = SceProcessmgrForDriver_AF80F39C(pid, g_appmgrAppInfoCacheKey, (void **)&info, 0);
	if (result < 0)
		goto unlock;
	if (info == NULL) {
		result = 0x80800012;
		goto unlock;
	}
	if ((info->saveDataFlags & 4) != 0 || (info->saveDataFlags & 2) == 0)
		goto done;
	info->saveDataFlags |= 4;
	mode = 2;
	if (strncmp(content->appDirectory, "gro0:", 5) == 0)
		mode = content->attribute & 1;
	appmgrUpdateSaveDataBackupList(content->saveDataTitleIds[0], content->titleId, mode);
	result = appmgrIsAutoUploadApp(content->titleId);
	if (result >= 0)
		result = appmgrAddAutoUploadSaveData(content->saveDataTitleIds[0], content->titleId,
						     content->shortTitle, mode);
	goto unlock;
done:
	result = 0;
unlock:
	sceKernelUnlockMutex(g_appmgrSaveDataMutex, 1);
	return result;
}

/* The backup of the save data finished: its applications may mark their next write again. */
int sceAppMgrSaveDataNotifyBackupFinished(const char *saveDataTitleId)
{
	SceAppMgrAppInfoCache *info = NULL;
	SceSelfAuthInfo authInfo;
	struct AppState *app;
	int result;

	result = sceKernelGetProcessSelfAuthInfo(0, &authInfo);
	if (result < 0)
		return result;
	if (authInfo.programAuthorityId != 0x2800000000000001ULL &&
	    authInfo.programAuthorityId != 0x280000000000002DULL &&
	    authInfo.programAuthorityId != 0x2800000000000039ULL)
		return 0x80800009;
	if (saveDataTitleId == NULL || !appmgrIsValidTitleId(saveDataTitleId, 0))
		return 0x80800001;
	result = sceKernelLockMutex(g_appmgrSaveDataMutex, 1, NULL);
	if (result < 0)
		return result;
	SAVEDATA_MAIN_LOCK(4236);
	for (app = &g_appmgrAppStates[1]; app != &g_appmgrAppStates[20]; app++) {
		if ((app->process.content.contentFlags & 0x30) == 0 ||
		    (app->process.content.contentFlags & 1) != 0)
			continue;
		if (strncmp(app->process.content.saveDataTitleIds[0], saveDataTitleId, 16) != 0)
			continue;
		result = SceProcessmgrForDriver_AF80F39C(app->process.processId,
							 g_appmgrAppInfoCacheKey, (void **)&info, 0);
		if (result < 0)
			break;
		if (info == NULL) {
			result = 0x80800012;
			break;
		}
		info->saveDataFlags = 1;
	}
	SAVEDATA_MAIN_UNLOCK(4288);
	sceKernelUnlockMutex(g_appmgrSaveDataMutex, 1);
	return result;
}

int SceAppMgrForDriver_C4EF95BB(const char *saveDataTitleId)
{
	int result;

	if (saveDataTitleId == NULL || !appmgrIsValidTitleId(saveDataTitleId, 0))
		return 0x80800001;
	result = sceKernelLockMutex(g_appmgrSaveDataMutex, 1, NULL);
	if (result < 0)
		return result;
	result = appmgrRemoveSaveDataBackupListItem(saveDataTitleId);
	sceKernelUnlockMutex(g_appmgrSaveDataMutex, 1);
	return result;
}

int SceAppMgrForDriver_9B3F7D66(void *list)
{
	int result;

	if (list == NULL)
		return 0x80800001;
	result = sceKernelLockMutex(g_appmgrSaveDataMutex, 1, NULL);
	if (result < 0)
		return result;
	result = appmgrGetSaveDataBackupList(list);
	sceKernelUnlockMutex(g_appmgrSaveDataMutex, 1);
	return result;
}

int appmgrValidateSaveDataMountPoint(const char *mountPoint, int requirePrimary)
{
	int result;

	result = strncmp(mountPoint, "savedata0:", 16);
	if (result != 0) {
		if (requirePrimary == 1)
			return (int)0x80800001;
		if (strncmp(mountPoint, "savedata1:", 16) != 0)
			return (int)0x80800001;
		return 0;
	}
	return result;
}

int appmgrCheckSaveDataSlot(unsigned int slotId, const char *mountPoint,
			   int requiredStatus)
{
	AppmgrSaveDataSlotFileHeader *header;
	SceAllocOpt option;
	char path[128];
	char resolvedPath[292];
	SceUID fd;
	int result;

	memset(path, 0, sizeof(path));
	snprintf(path, sizeof(path), "%s%s/%s", mountPoint,
		 "sce_sys", "sdslot.dat");
	appmgrResolvePath(path, 0, resolvedPath, sizeof(resolvedPath));
	fd = sceIoOpen(resolvedPath, 1, 390);
	if (fd < 0)
		return fd == (int)0x80010013 ? fd : (int)0x80801087;
	option.mmapKind = 0;
	option.align = 32;
	option.mmapBase = NULL;
	option.mmapSize = 0;
	option.size = sizeof(option);
	header = sceKernelAllocHeapMemoryWithOption(g_appmgrSaveDataHeap,
						   sizeof(*header), &option);
	if (!header)
		return (int)0x80801006;
	memset(header, 0, sizeof(*header));
	if (sceIoRead(fd, header, sizeof(*header)) != sizeof(*header)) {
		sceKernelFreeHeapMemory(g_appmgrSaveDataHeap, header);
		sceIoClose(fd);
		return (int)0x80801088;
	}
	sceIoClose(fd);
	if (timingsafe_memcmp(header->magic, "SDSL", 4) != 0) {
		result = (int)0x80801086;
	} else if (header->slotCount <= slotId) {
		result = (int)0x80801084;
	} else if (requiredStatus == 0 && header->occupied[slotId] != 0) {
		result = (int)0x8080108a;
	} else if (requiredStatus == 1 && header->occupied[slotId] != 1) {
		result = (int)0x8080108b;
	} else {
		result = 0;
	}
	sceKernelFreeHeapMemory(g_appmgrSaveDataHeap, header);
	return result;
}

/*
 * A save data path: at most 199 characters of letters, digits, '-', '_', '.', '/' and '\',
 * no "..", and no "sce_" component (the system's) after the leading separators.
 */
static inline int appmgrIsSaveDataPathValid(const char *path)
{
	const char *name;
	int length, prefix;
	char character;

	for (length = 0; length < 200; length++) {
		character = path[length];
		if (character == '\0')
			break;
		if ((unsigned int)((character & 0xdf) - 'A') <= 'Z' - 'A' ||
		    (unsigned int)(character - '0') <= 9 || character == '-' || character == '_')
			continue;
		if (character == '.') {
			if (length + 1 < 200 && path[length + 1] == '.')
				return 0;
			continue;
		}
		if (character != '/' && character != '\\')
			return 0;
	}
	name = path;
	while (*name == '/' || *name == '\\')
		name++;
	prefix = name - path + 3;
	if (length > prefix && (name[0] & 0xdf) == 'S' && (name[1] & 0xdf) == 'C' &&
	    (name[2] & 0xdf) == 'E' && name[3] == '_')
		return 0;
	return 1;
}

int appmgrValidateSaveDataFile(const SceAppUtilSaveDataDataSaveItem *file, const char *path)
{
	int index;

	if (file == NULL || file->dataPath == NULL || path == NULL || strnlen(path, 200) > 199)
		return 0x80800001;
	if (!appmgrIsSaveDataPathValid(path))
		return 0x80800001;
	for (index = 0; index < (int)sizeof(file->padding); index++) {
		if (file->padding[index] != 0)
			return 0x80800001;
	}
	if (file->mode > 2)
		return 0x80800001;
	for (index = 0; index < (int)sizeof(file->reserved); index++) {
		if (file->reserved[index] != 0)
			return 0x80800001;
	}
	return 0;
}

int appmgrValidateSaveDataRemoveItem(const SceAppUtilSaveDataDataRemoveItem *item,
				     const char *path)
{
	int index;

	if (item == NULL || item->dataPath == NULL || path == NULL || strnlen(path, 200) > 199)
		return 0x80800001;
	if (!appmgrIsSaveDataPathValid(path))
		return 0x80800001;
	if (item->mode > 1)
		return 0x80800001;
	for (index = 0; index < (int)sizeof(item->reserved); index++) {
		if (item->reserved[index] != 0)
			return 0x80800001;
	}
	return 0;
}

int appmgrValidateSaveDataSlotParam(const SceAppMgrSaveDataSlotParam *param)
{
	unsigned int index;
	unsigned int character;

	if (!param || strnlen(param->title, sizeof(param->title)) > 63 ||
	    strnlen(param->subTitle, sizeof(param->subTitle)) > 127 ||
	    strnlen(param->detail, sizeof(param->detail)) >= 512 ||
	    strnlen(param->iconPath, sizeof(param->iconPath)) > 63)
		return (int)0x80800001;
	for (index = 0; index < sizeof(param->iconPath); index++) {
		character = (unsigned char)param->iconPath[index];
		if (!character)
			break;
		if (((character & ~32u) - 'A') > 25 &&
		    character - '0' > 9 && character != '-' &&
		    character != '_' && character != '.' &&
		    character != '/' && character != ':')
			return (int)0x80800001;
		if (character == '.' && index != 63 &&
		    param->iconPath[index + 1] == '.')
			return (int)0x80800001;
	}
	for (index = 0; index < sizeof(param->reserved); index++) {
		if (param->reserved[index])
			return (int)0x80800001;
	}
	return 0;
}

int appmgrGetSaveDataParentPath(const char *path, char *parentPath)
{
	int slashCount = 0;
	size_t lastSlash = 0;
	size_t lastColon = 0;
	size_t index;
	signed char character;

	for (index = 0; index != 200; index++) {
		character = path[index];
		if (!character)
			break;
		if (character == ':') {
			lastColon = index;
		} else if (character == '/') {
			slashCount++;
			lastSlash = index;
		}
	}
	if (!lastColon)
		return 0;
	if (path[lastColon + 1] == '/')
		slashCount--;
	if (slashCount <= 0)
		return 0;
	strncpy(parentPath, path, lastSlash);
	return 1;
}

int appmgrCreateSaveDataDirectories(const char *path, SceMode mode)
{
	char parentPath[292];
	const char *separator;

	if (path && *path) {
		separator = path;
		while ((separator = strchr(separator + 1, '/')) != NULL) {
			memset(parentPath, 0, sizeof(parentPath));
#if FW_VERSION >= 0x04000000
			__strncpy_chk(parentPath, path, separator - path,
				      sizeof(parentPath));
#else
			strncpy(parentPath, path, separator - path);
#endif
			sceIoMkdir(parentPath, mode);
		}
	}
	return sceIoMkdir(path, mode);
}

int appmgrRemoveSaveDataPath(const char *path, unsigned int missingPolicy,
			     unsigned int *entryCount)
{
	SceIoStat stat;
	SceIoDirent entry;
	char childPath[292];
	SceUID directory;
	int result;

	result = sceIoGetstat(path, &stat);
	if (result < 0) {
		if (result == (int)0x80010002 && missingPolicy <= 1)
			result = 0;
		return result;
	}
	if (!(stat.st_mode & SCE_STM_FDIR)) {
		++*entryCount;
		return sceIoRemove(path);
	}
	directory = sceIoDopen(path);
	if (directory < 0)
		return directory;
	for (;;) {
		memset(&entry, 0, sizeof(entry));
		result = sceIoDread(directory, &entry);
		if (result < 0)
			goto closeDirectory;
		if (!result)
			break;
		memset(childPath, 0, sizeof(childPath));
		strncpy(childPath, path, sizeof(childPath));
#if FW_VERSION >= 0x04000000
		__strcat_chk(childPath, "/", sizeof(childPath));
		__strncat_chk(childPath, entry.d_name, 256, sizeof(childPath));
#else
		*(unsigned short *)(childPath + strlen(childPath)) =
			*(const unsigned short *)"/";
		strncat(childPath, entry.d_name, 256);
#endif
		result = appmgrRemoveSaveDataPath(childPath, missingPolicy, entryCount);
		if (result < 0)
			goto closeDirectory;
	}
	sceIoDclose(directory);
	++*entryCount;
	return sceIoRmdir(path);
closeDirectory:
	sceIoDclose(directory);
	return result;
}

int appmgrGetSaveDataDeviceSpace(const char *mountPoint,
				  SceUInt64 *allocationUnit,
				  SceOff *totalBytes,
				  SceUInt64 *availableQuota,
				  SceOff *freeBytes)
{
	char device[32] = {0};
	SceUInt32 quota;
	SceUInt32 usage;
	SceIoDevInfo info;
	SceOff maxSize;
	SceOff freeSize;
	size_t length;
	int result;

	length = strnlen(mountPoint, sizeof(device));
#if FW_VERSION >= 0x04000000
	__memcpy_chk(device, mountPoint, length - 2, sizeof(device));
#else
	memcpy(device, mountPoint, length - 2);
#endif
	quota = 0;
	usage = 0;
	result = ScePfsMgrForKernel_27FB7618(device, &quota, &usage);
	if (result != (int)0x80010030 && result != 0)
		return result;
	if (!quota || result == (int)0x80010030)
		quota = (SceUInt32)-1;
	if (quota > usage)
		*availableQuota = quota - usage;
	else
		*availableQuota = 0;
	memset(&info, 0, sizeof(info));
	result = sceIoDevctl(mountPoint, 0x3001, NULL, 0, &info, sizeof(info));
	if (result == 0) {
		maxSize = info.maxSize;
		freeSize = info.freeSize;
		*allocationUnit = info.clusterSize;
		*totalBytes = maxSize;
		*freeBytes = freeSize;
	}
	return result;
}

int appmgrCalculateSaveDataFileSpace(const SceAppUtilSaveDataDataSaveItem *userFile,
				     char **directories,
				     SceUInt64 *requiredBytes,
				     SceUInt64 allocationUnit,
				     const char *mountPoint)
{
	SceAppUtilSaveDataDataSaveItem file;
	SceIoStat stat;
	SceAllocOpt option;
	char path[210];
	char resolvedPath[292];
	char *filePath;
	char *prefix;
	char *directory;
	SceOff totalSize;
	SceUInt64 growth;
	SceUInt64 directorySize;
	size_t index;
	size_t length;
	unsigned int slot;
	int result;

	memset(&file, 0, sizeof(file));
	memset(path, 0, sizeof(path));
	memset(resolvedPath, 0, sizeof(resolvedPath));
	result = sceKernelCopyFromUser(&file, userFile, sizeof(file));
	if (result != 0)
		return result;
	option.size = sizeof(option);
	option.mmapKind = 0;
	option.align = 32;
	option.mmapBase = NULL;
	option.mmapSize = 0;
	filePath = sceKernelAllocHeapMemoryWithOption(
		g_appmgrSaveDataHeap, 200, &option);
	if (!filePath)
		return (int)0x80801006;
	memset(filePath, 0, 200);
	result = sceKernelStrncpyFromUser(filePath, (const char *)file.dataPath, 199);
	if (result < 0)
		goto freeFilePath;
	snprintf(path, sizeof(path), "%s%s", mountPoint, filePath);
	appmgrResolvePath(path, 0, resolvedPath, sizeof(resolvedPath));
	if (sceIoGetstat(resolvedPath, &stat) != 0) {
		directorySize = allocationUnit * 3;
		for (index = 0; index < 200 && resolvedPath[index]; index++) {
			if (resolvedPath[index] != '/' ||
			    resolvedPath[index - 1] == ':')
				continue;
			option.size = sizeof(option);
			option.mmapKind = 0;
			option.align = 32;
			option.mmapBase = NULL;
			option.mmapSize = 0;
			prefix = sceKernelAllocHeapMemoryWithOption(
				g_appmgrSaveDataHeap, 292, &option);
			if (!prefix) {
				result = (int)0x80801006;
				goto freeFilePath;
			}
			memset(prefix, 0, 292);
#if FW_VERSION >= 0x04000000
			__strncpy_chk(prefix, resolvedPath, index, 292);
#else
			strncpy(prefix, resolvedPath, index);
#endif
			if (sceIoGetstat(prefix, &stat) != 0) {
				for (slot = 0; slot < 20; slot++) {
					if (directories[slot] &&
					    strncmp(directories[slot], prefix, 291) == 0)
						break;
				}
				if (slot == 20) {
					length = strnlen(prefix, 291);
					if (length > 7 &&
					    strncmp(prefix + length - 8, "savedata", 8) != 0) {
						option.size = sizeof(option);
						option.mmapKind = 0;
						option.align = 32;
						option.mmapBase = NULL;
						option.mmapSize = 0;
						directory = sceKernelAllocHeapMemoryWithOption(
							g_appmgrSaveDataHeap, 292, &option);
						if (!directory) {
							sceKernelFreeHeapMemory(
								g_appmgrSaveDataHeap, prefix);
							result = (int)0x80801006;
							goto freeFilePath;
						}
						memset(directory, 0, 292);
						strncpy(directory, prefix, 291);
						for (slot = 0; slot < 20; slot++) {
							if (!directories[slot]) {
								directories[slot] = directory;
								break;
							}
						}
						*requiredBytes += directorySize;
					}
				}
			}
			sceKernelFreeHeapMemory(g_appmgrSaveDataHeap, prefix);
		}
		if (file.mode <= 1) {
			totalSize = file.offset + file.bufSize;
			growth = (SceUInt64)totalSize;
			if (growth % allocationUnit)
				growth = (growth / allocationUnit + 1) * allocationUnit;
			*requiredBytes += growth + allocationUnit * 5;
		} else {
			option.size = sizeof(option);
			option.mmapKind = 0;
			option.align = 32;
			option.mmapBase = NULL;
			option.mmapSize = 0;
			directory = sceKernelAllocHeapMemoryWithOption(
				g_appmgrSaveDataHeap, 292, &option);
			if (!directory) {
				result = (int)0x80801006;
				goto freeFilePath;
			}
			memset(directory, 0, 292);
			strncpy(directory, resolvedPath, 291);
			for (slot = 0; slot < 20; slot++) {
				if (!directories[slot]) {
					directories[slot] = directory;
					break;
				}
			}
			*requiredBytes += directorySize;
		}
	} else if (file.mode <= 1) {
		totalSize = file.offset + file.bufSize;
		if (stat.st_size < totalSize) {
			growth = (SceUInt64)(totalSize - stat.st_size);
			if (growth % allocationUnit)
				growth = (growth / allocationUnit + 1) * allocationUnit;
			*requiredBytes = growth + allocationUnit;
		} else {
			*requiredBytes = 0;
		}
	} else {
		*requiredBytes = 0;
	}
	result = 0;
freeFilePath:
	sceKernelFreeHeapMemory(g_appmgrSaveDataHeap, filePath);
	return result;
}

int appmgrWriteSaveDataSlot(unsigned int slotId,
			   const SceAppMgrSaveDataSlotParam *param,
			   int status, const char *mountPoint)
{
	SceAppMgrSaveDataSlotParam *copy;
	SceAllocOpt option;
	char path[128];
	char resolvedPath[292];
	unsigned char occupied;
	SceUID fd;
	int result;

	memset(path, 0, sizeof(path));
	snprintf(path, sizeof(path), "%s%s/%s", mountPoint,
		 "sce_sys", "sdslot.dat");
	appmgrResolvePath(path, 0, resolvedPath, sizeof(resolvedPath));
	fd = sceIoOpen(resolvedPath, 2, 390);
	if (fd < 0)
		return fd == (int)0x80010013 ? fd : (int)0x80801087;
	result = (int)sceIoLseek(fd, (SceOff)((slotId + 1) << 10), 0);
	if (result < 0)
		goto closeFile;
	option.mmapKind = 0;
	option.mmapBase = NULL;
	option.mmapSize = 0;
	option.size = sizeof(option);
	option.align = 32;
	copy = sceKernelAllocHeapMemoryWithOption(g_appmgrSaveDataHeap,
						 sizeof(*copy), &option);
	if (!copy) {
		result = (int)0x80801006;
		goto closeFile;
	}
	memcpy(copy, param, sizeof(*copy));
	result = appmgrValidateSaveDataSlotParam(copy);
	if (result < 0)
		goto closeAndFree;
	if (status >= 0)
		copy->status = status;
	if (sceIoWrite(fd, copy, sizeof(*copy)) != sizeof(*copy)) {
		result = (int)0x80801089;
		goto closeAndFree;
	}
	sceKernelFreeHeapMemory(g_appmgrSaveDataHeap, copy);
	result = (int)sceIoLseek(fd, (SceOff)(slotId + 512), 0);
	if (result < 0)
		goto closeFile;
	occupied = 1;
	if (sceIoWrite(fd, &occupied, sizeof(occupied)) != sizeof(occupied))
		result = (int)0x80801089;
	else
		result = 0;
closeFile:
	sceIoClose(fd);
	return result;
closeAndFree:
	sceIoClose(fd);
	sceKernelFreeHeapMemory(g_appmgrSaveDataHeap, copy);
	return result;
}

int appmgrDeleteSaveDataSlot(unsigned int slotId, const char *mountPoint)
{
	SceAppMgrSaveDataSlotParam *record;
	SceAllocOpt option;
	char path[128];
	char resolvedPath[292];
	unsigned char occupied;
	SceUID fd;
	int result;

	memset(path, 0, sizeof(path));
	snprintf(path, sizeof(path), "%s%s/%s", mountPoint,
		 "sce_sys", "sdslot.dat");
	appmgrResolvePath(path, 0, resolvedPath, sizeof(resolvedPath));
	fd = sceIoOpen(resolvedPath, 2, 390);
	if (fd < 0)
		return fd == (int)0x80010013 ? fd : (int)0x80801087;
	result = (int)sceIoLseek(fd, (SceOff)((slotId + 1) << 10), 0);
	if (result < 0)
		goto closeFile;
	option.mmapKind = 0;
	option.mmapBase = NULL;
	option.size = sizeof(option);
	option.align = 32;
	option.mmapSize = 0;
	record = sceKernelAllocHeapMemoryWithOption(g_appmgrSaveDataHeap,
						   sizeof(*record), &option);
	if (!record) {
		result = (int)0x80801006;
		goto closeFile;
	}
	memset(record, 0, sizeof(*record));
	if (sceIoWrite(fd, record, sizeof(*record)) != sizeof(*record)) {
		result = (int)0x80801089;
		goto freeRecord;
	}
	result = (int)sceIoLseek(fd, (SceOff)(slotId + 512), 0);
	if (result < 0)
		goto freeRecord;
	occupied = 0;
	if (sceIoWrite(fd, &occupied, sizeof(occupied)) != sizeof(occupied))
		result = (int)0x80801089;
	else
		result = 0;
freeRecord:
	sceKernelFreeHeapMemory(g_appmgrSaveDataHeap, record);
closeFile:
	sceIoClose(fd);
	return result;
}

int appmgrOverwriteSaveDataSlot(unsigned int slotId,
				const SceAppMgrSaveDataSlotParam *param,
				int status, const char *mountPoint)
{
	SceAllocOpt option;
	char *path;
	char *resolvedPath;
	SceAppMgrSaveDataSlotParam *copy;
	SceUID fd;
	int result;

	option.mmapKind = 0;
	option.mmapBase = NULL;
	option.mmapSize = 0;
	option.size = sizeof(option);
	option.align = 32;
	path = sceKernelAllocHeapMemoryWithOption(g_appmgrSaveDataHeap, 128,
						 &option);
	if (!path)
		return (int)0x80801006;
	option.mmapKind = 0;
	option.mmapBase = NULL;
	option.mmapSize = 0;
	option.size = sizeof(option);
	option.align = 32;
	resolvedPath = sceKernelAllocHeapMemoryWithOption(g_appmgrSaveDataHeap,
							 292, &option);
	if (!resolvedPath) {
		sceKernelFreeHeapMemory(g_appmgrSaveDataHeap, path);
		return (int)0x80801006;
	}
	memset(path, 0, 128);
	memset(resolvedPath, 0, 292);
	snprintf(path, 128, "%s%s/%s", mountPoint, "sce_sys", "sdslot.dat");
	appmgrResolvePath(path, 0, resolvedPath, 292);
	option.mmapKind = 0;
	option.align = 32;
	option.mmapBase = NULL;
	option.mmapSize = 0;
	option.size = sizeof(option);
	copy = sceKernelAllocHeapMemoryWithOption(g_appmgrSaveDataHeap,
						 sizeof(*copy), &option);
	if (!copy) {
		result = (int)0x80801006;
		goto freeBuffers;
	}
	memcpy(copy, param, sizeof(*copy));
	result = appmgrValidateSaveDataSlotParam(copy);
	if (result < 0)
		goto freeBuffers;
	fd = sceIoOpen(resolvedPath, 2, 390);
	if (fd < 0) {
		result = fd == (int)0x80010013 ? fd : (int)0x80801087;
		goto freeBuffers;
	}
	result = (int)sceIoLseek(fd, (SceOff)((slotId + 1) << 10), 0);
	if (result < 0)
		goto closeFile;
	if (status >= 0)
		copy->status = status;
	if (sceIoWrite(fd, copy, sizeof(*copy)) == sizeof(*copy))
		result = 0;
	else
		result = (int)0x80801089;
closeFile:
	sceIoClose(fd);
freeBuffers:
	sceKernelFreeHeapMemory(g_appmgrSaveDataHeap, path);
	sceKernelFreeHeapMemory(g_appmgrSaveDataHeap, resolvedPath);
	if (copy)
		sceKernelFreeHeapMemory(g_appmgrSaveDataHeap, copy);
	return result;
}

int appmgrSetSaveDataSlotStatus(unsigned int slotId, int status,
				const char *mountPoint)
{
	char path[128];
	char resolvedPath[292];
	int recordStatus = status;
	SceUID fd;
	int result;

	memset(path, 0, sizeof(path));
	snprintf(path, sizeof(path), "%s%s/%s", mountPoint,
		 "sce_sys", "sdslot.dat");
	appmgrResolvePath(path, 0, resolvedPath, sizeof(resolvedPath));
	fd = sceIoOpen(resolvedPath, 2, 390);
	if (fd < 0)
		return fd == (int)0x80010013 ? fd : (int)0x80801087;
	result = (int)sceIoLseek(fd, (SceOff)((slotId + 1) << 10), 0);
	if (result < 0) {
		sceIoClose(fd);
		return result;
	}
	if (sceIoWrite(fd, &recordStatus, sizeof(recordStatus)) ==
	    sizeof(recordStatus)) {
		sceIoClose(fd);
		return 0;
	}
	sceIoClose(fd);
	return (int)0x80801089;
}

int appmgrRollbackSaveDataOperation(const SceAppMgrSaveDataData *operation,
				    const char *const *directories,
				    const SceOff *originalSizes,
				    const int *createdFiles,
				    SceUInt64 allocationUnit, int slotCreated,
				    SceUInt32 *requiredSizeKiB)
{
	SceIoStat stat;
	SceAppUtilSaveDataDataSaveItem file;
	SceAllocOpt option;
	SceOff totalSize;
	char path[210];
	char resolvedPath[292];
	char *filePath;
	unsigned int index;
	unsigned int entryCount;
	int result;

	memset(&stat, 0, sizeof(stat));
	index = 0;
	if (operation->itemNum) {
		do {
			memset(&file, 0, sizeof(file));
			result = sceKernelCopyFromUser(&file,
					operation->items + index, sizeof(file));
			if (result != 0)
				return result;
			option.mmapKind = 0;
			option.mmapBase = NULL;
			option.size = sizeof(option);
			option.mmapSize = 0;
			option.align = 32;
			filePath = sceKernelAllocHeapMemoryWithOption(
				g_appmgrSaveDataHeap, 200, &option);
			if (!filePath)
				return (int)0x80801006;
			memset(filePath, 0, 200);
			result = sceKernelStrncpyFromUser(filePath, (const char *)file.dataPath, 199);
			if (result < 0) {
				sceKernelFreeHeapMemory(g_appmgrSaveDataHeap, filePath);
				return result;
			}
			memset(path, 0, sizeof(path));
			memset(resolvedPath, 0, sizeof(resolvedPath));
			snprintf(path, sizeof(path), "%s%s",
				 (const char *)operation->mountPoint.data, filePath);
			appmgrResolvePath(path, 0, resolvedPath, sizeof(resolvedPath));
			if (createdFiles[index] == 1) {
				sceIoRemove(resolvedPath);
			} else if (file.mode != 2) {
				stat.st_size = originalSizes[index];
				__builtin_prefetch(originalSizes + index + 1);
				sceIoChstat(resolvedPath, &stat, 4);
			}
			sceKernelFreeHeapMemory(g_appmgrSaveDataHeap, filePath);
			totalSize = file.offset + file.bufSize;
			if ((SceUInt64)totalSize % allocationUnit != 0) {
				*requiredSizeKiB +=
					(((SceUInt64)totalSize / allocationUnit + 1) *
					 allocationUnit) >> 10;
			} else {
				*requiredSizeKiB += totalSize / 1024;
			}
			index++;
		} while (operation->itemNum > index);
	}
	for (index = 0; index < 20; index++) {
		if (directories[index]) {
			appmgrRemoveSaveDataPath(directories[index], 0, &entryCount);
			*requiredSizeKiB += allocationUnit >> 10;
		}
	}
	if (operation->slotId == (SceUInt32)-1)
		return 0;
	if (slotCreated)
		return appmgrDeleteSaveDataSlot(operation->slotId,
					      (const char *)operation->mountPoint.data);
	return appmgrSetSaveDataSlotStatus(operation->slotId, 0,
					  (const char *)operation->mountPoint.data);
}

int appmgrCheckSaveDataMount(const char *titleId, const char *mountPoint,
			     const AppmgrSaveDataMount *list)
{
	SceIoStat stat;
	char name[10] = {0};
	char path[128];
	char resolvedPath[292];
	SceUID processId;

	memset(path, 0, sizeof(path));
	if (strncmp(mountPoint, "savedata0", 10) == 0 ||
	    strncmp(mountPoint, "savedata0:", 11) == 0)
		strncpy(path, "host0:savedata", sizeof(path) - 1);
	appmgrResolvePath(path, 0, resolvedPath, sizeof(resolvedPath));
	if (sceIoGetstat(resolvedPath, &stat) < 0)
		return (int)0x80801005;
	if (titleId)
		strncpy(name, titleId, 9);
	processId = sceKernelGetProcessId();
	for (; list; list = list->next) {
		if (strncmp(name, list->titleId, 9) == 0 ||
		    (list->processId == processId &&
		     strncmp(mountPoint, list->mountPoint, 15) == 0))
			return (int)0x80801080;
	}
	return 0;
}

int appmgrFindSaveDataMount(const char *mountPoint, const AppmgrSaveDataMount *list)
{
	SceUID processId;

	processId = sceKernelGetProcessId();
	for (; list; list = list->next) {
		if (list->processId == processId &&
		    strncmp(mountPoint, list->mountPoint, 15) == 0)
			return 0;
	}
	return (int)0x80801081;
}

int appmgrAccumulateSaveDataWriteSize(SceUInt32 writtenKiB)
{
	AppmgrNotificationRequest notification;
	SceAppMgrAppInfoCache *info;
	void *storage = NULL;
	SceUInt32 sinceNotice;
	SceUInt32 seconds;
	SceUInt32 threshold;
	SceUInt32 rate;
	float elapsed;
	int result;

	result = SceProcessmgrForDriver_AF80F39C(sceKernelGetProcessId(),
		g_appmgrAppInfoCacheKey, &storage, 0);
	if (result < 0)
		return result;
	if (!storage)
		return (int)0x80800012;
	info = storage;
	sinceNotice = info->writtenSinceNoticeKiB + writtenKiB;
	info->writtenSinceNoticeKiB += writtenKiB;
	info->totalWrittenKiB += writtenKiB;
	if (sinceNotice < 1024)
		return 0;
	seconds = SceProcessmgrForDriver_82D94BE9() / 1000000;
	if (seconds) {
		elapsed = seconds;
		threshold = 20480 * (seconds / 3600 + 1);
	} else {
		threshold = 20480;
		elapsed = 1.0f;
		seconds = 1;
	}
	if (info->totalWrittenKiB < threshold)
		return 0;
	rate = (SceUInt32)(((float)info->totalWrittenKiB / elapsed) * 60.0f * 60.0f);
	memset(&notification, 0, sizeof(notification));
	snprintf((char *)notification.payload, sizeof(notification.payload),
		 "AppUtil:\nViolate TRC R3074\nWrite %d.%02d MiB/h\nPlay %02d:%02d",
		 (int)(rate >> 10), (int)((rate & 0x3FF) / 10),
		 (int)(seconds / 60), (int)(seconds % 60));
	_appmgrQueuePush(&notification, 1);
	info->writtenSinceNoticeKiB -= 1024;
	return 0;
}

void appmgrNotifySaveDataSpaceError(int error, SceUInt32 requiredKiB)
{
	AppmgrNotificationRequest notification;

	memset(&notification, 0, sizeof(notification));
	if (error == (int)0x8080108C) {
		snprintf((char *)notification.payload, sizeof(notification.payload),
			 "AppUtil:\nFree Space Error - \nQuota : %u KiB", (unsigned int)requiredKiB);
	} else if (error == (int)0x8080108D) {
		snprintf((char *)notification.payload, sizeof(notification.payload),
			 "AppUtil:\nFree Space Error - \nFS : %u KiB", requiredKiB);
	} else {
		return;
	}
	_appmgrQueuePush(&notification, 1);
}

/* With the debug information shown, tells the user that a slot's status was read. */
void appmgrNotifySaveDataSlotRead(SceUInt32 slotId)
{
	AppmgrNotificationRequest notification;

	memset(&notification, 0, sizeof(notification));
	if (appmgrIsShowDebugInfoEnabled() == 0)
		return;
	snprintf((char *)notification.payload, sizeof(notification.payload),
		 "AppUtil:\nRead Slot Status %03d", slotId);
	_appmgrQueuePush(&notification, 1);
}

void appmgrFreeSaveDataDirectories(char **directories)
{
	char **end = directories + 20;
	char *directory;

	do {
		directory = *directories++;
		if (directory) {
			sceKernelFreeHeapMemory(g_appmgrSaveDataHeap, directory);
			directories[-1] = NULL;
		}
	} while (directories != end);
}

/* savedata_plus/auto_upload/up_app_list.dat: the applications whose save data is uploaded */
typedef struct AppmgrAutoUploadAppListHeader {
	char magic[4];
	SceUInt8 reserved[8];
	SceUInt32 count;
	SceUInt8 reserved2[40];
} AppmgrAutoUploadAppListHeader;

typedef struct AppmgrAutoUploadAppListEntry {
	char titleId[16];
	SceInt32 enabled;
	SceUInt8 reserved[44];
} AppmgrAutoUploadAppListEntry;

_Static_assert(sizeof(AppmgrAutoUploadAppListHeader) == 56, "auto-upload application list header");
_Static_assert(sizeof(AppmgrAutoUploadAppListEntry) == 64, "auto-upload application list entry");

/* savedata_plus/auto_upload/up_savedata_list.dat: the save data waiting for upload */
typedef struct AppmgrAutoUploadSaveDataListHeader {
	char magic[4];
	SceUInt8 reserved[4];
	SceUInt32 count;
	SceUInt8 reserved2[44];
} AppmgrAutoUploadSaveDataListHeader;

typedef struct AppmgrAutoUploadSaveDataListEntry {
	char saveDataTitleId[16];
	char titleId[16];
	char shortTitle[64];
	SceInt32 mode;
	SceUInt8 reserved[64];
} AppmgrAutoUploadSaveDataListEntry;

_Static_assert(sizeof(AppmgrAutoUploadSaveDataListHeader) == 56, "auto-upload save data list header");
_Static_assert(sizeof(AppmgrAutoUploadSaveDataListEntry) == 164, "auto-upload save data list entry");

/* savedata_backup/list.dat: the three save data written last, newest first */
typedef struct AppmgrSaveDataBackupListEntry {
	char saveDataTitleId[16];
	char titleId[16];
	SceInt32 mode;
	SceUInt8 reserved[28];
} AppmgrSaveDataBackupListEntry;

_Static_assert(sizeof(AppmgrSaveDataBackupListEntry) == 64, "save data backup list entry");

/* 0 when titleId is enabled in the auto-upload application list. */
static __attribute__((noinline)) int appmgrIsAutoUploadApp(const char *titleId)
{
	char path[64] = "";
	AppmgrAutoUploadAppListHeader header = {{0}};
	AppmgrAutoUploadAppListEntry entry;
	SceUInt32 index;
	SceUID fd;
	int result;

	result = SceAppMgrForDriver_0813AFCA(2, path, sizeof(path));
	if (result < 0)
		return result;
	APPMGR_STRCAT(path, "/savedata_plus/auto_upload/up_app_list.dat", sizeof(path));
	if (titleId == NULL)
		return 0x80800001;
	fd = sceIoOpen(path, 0x10001, 6);
	if (fd < 0)
		return fd;
	result = sceIoRead(fd, &header, sizeof(header));
	if (result < 0)
		goto close;
	if (result != sizeof(header) || memcmp(header.magic, "SDUT", 4) != 0) {
		result = 0x80801092;
		goto close;
	}
	index = 0;
	while (index < header.count) {
		memset(&entry, 0, sizeof(entry));
		if (sceIoRead(fd, &entry, sizeof(entry)) != sizeof(entry)) {
			result = 0x80801093;
			goto close;
		}
		if (entry.titleId[0] == '\0')
			continue;
		if (entry.enabled != 0 && strncmp(entry.titleId, titleId, 16) == 0) {
			result = 0;
			goto close;
		}
		index++;
	}
	result = 0x80801094;
close:
	sceIoClose(fd);
	return result;
}

/* Adds the save data to the upload list, in the first free entry or at the end. */
static __attribute__((noinline)) int appmgrAddAutoUploadSaveData(const char *saveDataTitleId,
		const char *titleId, const char *shortTitle, int mode)
{
	char path[64] = "";
	AppmgrAutoUploadSaveDataListHeader header = {{0}};
	AppmgrAutoUploadSaveDataListEntry entry;
	SceUInt32 index, used;
	int freeIndex;
	SceUID fd;
	int result;

	result = SceAppMgrForDriver_0813AFCA(2, path, sizeof(path));
	if (result < 0)
		return result;
	APPMGR_STRCAT(path, "/savedata_plus/auto_upload/up_savedata_list.dat", sizeof(path));
	if (titleId == NULL || saveDataTitleId == NULL || shortTitle == NULL)
		return 0x80800001;
	fd = sceIoOpen(path, 0x10003, 6);
	if (fd < 0)
		return fd;
	result = sceIoRead(fd, &header, sizeof(header));
	if (result < 0)
		goto close;
	if (result != sizeof(header) || memcmp(header.magic, "SDUS", 4) != 0) {
		result = 0x80801095;
		goto close;
	}
	if (header.count > 99) {
		result = 0x80801096;
		goto close;
	}
	index = 0;
	freeIndex = -1;
	for (used = 0; used < header.count; index++) {
		memset(&entry, 0, sizeof(entry));
		if (sceIoRead(fd, &entry, sizeof(entry)) != sizeof(entry)) {
			result = 0x80801097;
			goto close;
		}
		if (entry.titleId[0] == '\0') {
			if (freeIndex == -1)
				freeIndex = index;
			continue;
		}
		if (strncmp(entry.titleId, titleId, 16) == 0) {
			result = 0x80801098;
			goto close;
		}
		used++;
	}
	if (freeIndex != -1) {
		result = sceIoLseek(fd, sizeof(header) + freeIndex * sizeof(entry), SCE_SEEK_SET);
		if (result < 0)
			goto close;
	}
	memset(&entry, 0, sizeof(entry));
	strncpy(entry.saveDataTitleId, saveDataTitleId, 15);
	strncpy(entry.titleId, titleId, 15);
	strncpy(entry.shortTitle, shortTitle, 63);
	entry.mode = mode;
	result = sceIoWrite(fd, &entry, sizeof(entry));
	if (result < 0)
		goto close;
	if (result != sizeof(entry)) {
		result = 0x80801099;
		goto close;
	}
	header.count++;
	result = sceIoLseek(fd, 0, SCE_SEEK_SET);
	if (result < 0)
		goto close;
	result = sceIoWrite(fd, &header, sizeof(header));
	if (result < 0)
		goto close;
	result = result != sizeof(header) ? 0x80801099 : 0;
close:
	sceIoClose(fd);
	return result;
}

/* Moves the save data to the front of the backup list; mode 0 keeps no backup. */
static __attribute__((noinline)) int appmgrUpdateSaveDataBackupList(const char *saveDataTitleId,
		const char *titleId, int mode)
{
	char base[32] = "";
	char path[64] = "";
	AppmgrSaveDataBackupListEntry list[3];
	SceIoStat stat;
	int index;
	SceUID fd;
	int result;

	memset(list, 0, sizeof(list));
	if (titleId == NULL || saveDataTitleId == NULL)
		return 0x80800001;
	if (mode == 0)
		return 0;
	if ((unsigned int)(mode - 1) > 1)
		return 0x80800001;
	result = SceAppMgrForDriver_0813AFCA(2, base, sizeof(base));
	if (result < 0)
		return result;
	if ((unsigned int)snprintf(path, sizeof(path), "%s/savedata_backup", base) >= sizeof(path))
		return 0x8080000A;
	result = appmgrCreateDirectories(path, 1);
	if (result < 0)
		return result;
	if ((unsigned int)snprintf(path, sizeof(path), "%s/savedata_backup/list.dat", base) >=
	    sizeof(path))
		return 0x8080000A;
	if (sceIoGetstat(path, &stat) >= 0) {
		fd = sceIoOpen(path, 0x10003, 6);
		if (fd < 0)
			return fd;
		result = sceIoRead(fd, list, sizeof(list));
		if (result < 0)
			goto remove;
		if (result != sizeof(list)) {
			result = 0x8080008A;
			goto remove;
		}
	} else {
		fd = sceIoOpen(path, 0x10203, 6);
		if (fd < 0)
			return fd;
	}
	index = strncmp(list[0].saveDataTitleId, saveDataTitleId, 10) != 0 ? 2 : 0;
	if (strncmp(list[1].saveDataTitleId, saveDataTitleId, 10) == 0)
		index = 1;
	if (strncmp(list[2].saveDataTitleId, saveDataTitleId, 10) == 0)
		index = 2;
	for (; index != 0; index--)
		list[index] = list[index - 1];
	memset(&list[0], 0, sizeof(list[0]));
	strncpy(list[0].saveDataTitleId, saveDataTitleId, 9);
	strncpy(list[0].titleId, titleId, 9);
	list[0].mode = mode;
	result = sceIoLseek(fd, 0, SCE_SEEK_SET);
	if (result < 0)
		goto remove;
	result = sceIoWrite(fd, list, sizeof(list));
	if (result < 0)
		goto remove;
	if (result != sizeof(list)) {
		result = 0x8080008B;
		goto remove;
	}
	stat.st_size = sizeof(list);
	result = sceIoChstatByFd(fd, &stat, SCE_CST_SIZE);
	if (result < 0)
		goto remove;
	sceIoClose(fd);
	return 0;
remove:
	sceIoClose(fd);
	sceIoRemove(path);
	return result;
}

/* Removes the save data from the backup list, moving the later entries up. */
static __attribute__((noinline)) int appmgrRemoveSaveDataBackupListItem(const char *saveDataTitleId)
{
	char base[32] = "";
	char path[64] = "";
	AppmgrSaveDataBackupListEntry list[3];
	SceIoStat stat;
	int index;
	SceUID fd;
	int result;

	memset(list, 0, sizeof(list));
	if (saveDataTitleId == NULL)
		return 0x80800001;
	result = SceAppMgrForDriver_0813AFCA(2, base, sizeof(base));
	if (result < 0)
		return result;
	if ((unsigned int)snprintf(path, sizeof(path), "%s/savedata_backup/list.dat", base) >=
	    sizeof(path))
		return 0x8080000A;
	fd = sceIoOpen(path, 0x10003, 6);
	if (fd < 0)
		return fd;
	result = sceIoRead(fd, list, sizeof(list));
	if (result < 0)
		goto close;
	if (result != sizeof(list)) {
		result = 0x8080008A;
		goto close;
	}
	if (strncmp(list[0].saveDataTitleId, saveDataTitleId, 10) == 0)
		index = 0;
	else if (strncmp(list[1].saveDataTitleId, saveDataTitleId, 10) == 0)
		index = 1;
	else if (strncmp(list[2].saveDataTitleId, saveDataTitleId, 10) == 0)
		index = 2;
	else {
		result = 0x8080008C;
		goto close;
	}
	memset(&list[index], 0, sizeof(list[index]));
	for (; index < 2; index++)
		list[index] = list[index + 1];
	memset(&list[2], 0, sizeof(list[2]));
	result = sceIoLseek(fd, 0, SCE_SEEK_SET);
	if (result < 0)
		goto close;
	result = sceIoWrite(fd, list, sizeof(list));
	if (result < 0)
		goto close;
	if (result != sizeof(list)) {
		result = 0x8080008B;
		goto close;
	}
	stat.st_size = sizeof(list);
	result = sceIoChstatByFd(fd, &stat, SCE_CST_SIZE);
	if (result > 0)
		result = 0;
close:
	sceIoClose(fd);
	return result;
}

/* Reads the three backup list entries into list. */
static __attribute__((noinline)) int appmgrGetSaveDataBackupList(void *list)
{
	char base[32] = "";
	char path[64] = "";
	SceUID fd;
	int result;

	if (list == NULL)
		return 0x80800001;
	result = SceAppMgrForDriver_0813AFCA(2, base, sizeof(base));
	if (result < 0)
		return result;
	if ((unsigned int)snprintf(path, sizeof(path), "%s/savedata_backup/list.dat", base) >=
	    sizeof(path))
		return 0x8080000A;
	fd = sceIoOpen(path, 0x10001, 4);
	if (fd < 0)
		return fd;
	memset(list, 0, sizeof(AppmgrSaveDataBackupListEntry[3]));
	result = sceIoRead(fd, list, sizeof(AppmgrSaveDataBackupListEntry[3]));
	if (result >= 0)
		result = result == sizeof(AppmgrSaveDataBackupListEntry[3]) ? 0 : 0x8080008A;
	sceIoClose(fd);
	return result;
}

/* The MAC key type of each PSP save data format (SceAppUtilPspSaveDataVersion). */
static const int pspSaveDataKeyTypes[3] = { 1, 3, 5 };

/* Copies the PARAM.SFO entries _sceAppMgrPspSaveDataGetParams returns. */
static __attribute__((noinline)) int appmgrGetPspSaveDataParamSfo(AppmgrSfoFile *sfo,
		AppmgrPspSaveDataParamSfo *paramSfo)
{
	void *value = NULL;
	SceSize length = 0;
	SceSize capacity = 0;
	int result;

	result = appmgrGetSfoEntry(sfo, "SAVEDATA_DIRECTORY", &value, &length, &capacity);
	if (result < 0)
		return result;
	if (length == 0 || capacity != sizeof(paramSfo->saveDataDirectory))
		return 0x80800034;
	memcpy(paramSfo->saveDataDirectory, value, length);
	result = appmgrGetSfoEntry(sfo, "TITLE", &value, &length, &capacity);
	if (result < 0)
		return result;
	if (length == 0 || capacity != sizeof(paramSfo->title))
		return 0x80800034;
	memcpy(paramSfo->title, value, length);
	result = appmgrGetSfoEntry(sfo, "SAVEDATA_TITLE", &value, &length, &capacity);
	if (result < 0)
		return result;
	if (length == 0 || capacity != sizeof(paramSfo->saveDataTitle))
		return 0x80800034;
	memcpy(paramSfo->saveDataTitle, value, length);
	result = appmgrGetSfoEntry(sfo, "SAVEDATA_DETAIL", &value, &length, &capacity);
	if (result < 0)
		return result;
	if (length == 0 || capacity != sizeof(paramSfo->detail))
		return 0x80800034;
	memcpy(paramSfo->detail, value, length);
	result = appmgrGetSfoEntry(sfo, "PARENTAL_LEVEL", &value, &length, &capacity);
	if (result < 0)
		return result;
	if (length < 4 || capacity != 4)
		return 0x80800034;
	memcpy(&paramSfo->parentalLev, value, length);
	return result;
}

/*
 * Checks SAVEDATA_PARAMS: the MAC of the whole PARAM.SFO and, with checkVersion, the MAC of
 * the save data format and its flags. Both MACs are zero while computed and put back.
 */
static __attribute__((noinline)) int appmgrVerifyPspSaveDataSfo(AppmgrSfoFile *sfo,
		unsigned int dataVersion, int checkVersion)
{
	unsigned char *params = NULL;
	SceSize length = 0;
	SceSize capacity = 0;
	SceUInt8 macContext[40];
	SceUInt8 sfoMac[16], versionMac[16];
	SceUInt8 digest[16];
	SceUInt8 flags;
	int result;

	result = appmgrGetSfoEntry(sfo, "SAVEDATA_PARAMS", (void **)&params, &length, &capacity);
	if (result < 0)
		return result;
	if (length < 128 || capacity != 128)
		return 0x80800034;
	memcpy(sfoMac, params + 16, sizeof(sfoMac));
	memcpy(versionMac, params + 112, sizeof(versionMac));
	memset(params + 16, 0, 16);
	result = SceSblGcAuthMgrMsSaveBBForDriver_4A249828(macContext, 1);
	if (result < 0)
		goto restore;
	result = SceSblGcAuthMgrMsSaveBBForDriver_79A9BE44(macContext, sfo->buffer, sfo->size);
	if (result < 0)
		goto restore;
	result = SceSblGcAuthMgrMsSaveBBForDriver_8E6626A0(macContext, digest, NULL);
	if (result < 0)
		goto restore;
	if (memcmp(digest, sfoMac, sizeof(digest)) != 0) {
		result = 0x80800060;
		goto restore;
	}
	if (!checkVersion) {
		result = 0;
		goto restore;
	}
	if (dataVersion == 0) {
		if ((params[0] & 0x61) != 0x01)
			goto broken;
		goto checkReserved;
	}
	if (dataVersion > 2) {
		result = 0x80800001;
		goto restore;
	}
	result = pspSaveDataKeyTypes[dataVersion];
	if (result < 0)
		goto restore;
	memset(params + 112, 0, 16);
	result = SceSblGcAuthMgrMsSaveBBForDriver_4A249828(macContext, result);
	if (result < 0)
		goto restore;
	result = SceSblGcAuthMgrMsSaveBBForDriver_79A9BE44(macContext, sfo->buffer, sfo->size);
	if (result < 0)
		goto restore;
	result = SceSblGcAuthMgrMsSaveBBForDriver_8E6626A0(macContext, digest, NULL);
	if (result < 0)
		goto restore;
	if (memcmp(digest, versionMac, sizeof(digest)) != 0) {
		result = 0x80800060;
		goto restore;
	}
	flags = params[0] & 0x61;
	if (dataVersion == 2) {
		if (flags != 0x41)
			goto broken;
	} else if (flags != 0x21) {
		goto broken;
	}
checkReserved:
	result = params[1];
	if (result == 0)
		goto restore;
broken:
	result = 0x80800064;
restore:
	memcpy(params + 16, sfoMac, sizeof(sfoMac));
	memcpy(params + 112, versionMac, sizeof(versionMac));
	return result;
}

/* A PSP save data directory: four capital letters, five digits, then a file name. */
static __attribute__((noinline)) int appmgrIsValidPspSaveDataDirectory(const char *dirName)
{
	const unsigned char *name = (const unsigned char *)dirName;
	unsigned int c;
	int index;

	for (index = 0; index < 4; index++) {
		if (name[index] < 'A' || name[index] > 'Z')
			return 0;
	}
	for (; index < 9; index++) {
		if (name[index] < '0' || name[index] > '9')
			return 0;
	}
	for (index = 9; ; index++) {
		c = name[index];
		if (c == '\0')
			return 1;
		if (c < 0x20 || c > 0x7e || c == '\\' || c == '/' || c == ':' || c == '"' ||
		    c == '*' || c == '<' || c == '|' || c == '>' || c == '?' || index == 63)
			return 0;
	}
}

int sceAppMgrPspSaveDataGetParams(AppmgrPspSaveDataParamSfo *paramSfo)
{
	AppmgrSfoFile sfo = {0};
	char path[104];
	SceUInt32 syscallState;
	int result;

	syscallState = sceSyscallEnter();
	if (paramSfo == NULL || !appmgrIsValidPspSaveDataDirectory(paramSfo->saveDataDirectory)) {
		result = 0x80800001;
		goto leave;
	}
	memset(path, 0, sizeof(path));
	snprintf(path, sizeof(path), "ux0:pspemu/PSP/SAVEDATA/%s/PARAM.SFO",
		 paramSfo->saveDataDirectory);
	result = sceKernelLockMutex(g_appmgrMountMutex, 1, NULL);
	if (result < 0)
		goto leave;
	result = appmgrLoadSfoFile(path, &sfo);
	if (result >= 0) {
		result = appmgrVerifyPspSaveDataSfo(&sfo, 0, 0);
		if (result >= 0) {
			memset(paramSfo, 0, sizeof(*paramSfo));
			result = appmgrGetPspSaveDataParamSfo(&sfo, paramSfo);
		}
	}
	sceKernelUnlockMutex(g_appmgrMountMutex, 1);
leave:
	appmgrReleaseSfoFile(&sfo);
	sceSyscallLeave(syscallState);
	return result;
}

/* A PSP save data file name: up to eight characters, a dot and up to three. */
static inline int appmgrIsValidPspSaveDataFileName(const char *fileName)
{
	int dot = 0, nameLength = 0, extensionLength = -1;
	int index;
	char c;

	for (index = 0; index < 12; index++) {
		c = fileName[index];
		if (c == '\0') {
			if (index == 0)
				return 0;
			goto lengths;
		}
		if ((c < '0' || c > '9') && (c < 'A' || c > 'Z') && c != '-' && c != '_') {
			if (c != '.')
				return 0;
			dot = 1;
		}
		if (dot)
			extensionLength++;
		else
			nameLength++;
	}
	if (fileName[12] != '\0')
		return 0;
lengths:
	return nameLength <= 8 && extensionLength <= 3;
}

/* Reads a plain file of the save data into the caller's buffer. */
static inline int appmgrReadPspSaveDataNormalFile(const char *path,
		const AppmgrPspSaveDataParams *params)
{
	void *page = NULL;
	SceSize pageSize = 0;
	SceUInt32 offset = 0;
	SceIoStat stat;
	SceUID mapping, fd;
	char *buffer;
	int length = 0;
	int result;

	result = sceIoGetstat(path, &stat);
	if (result < 0)
		goto done;
	if (stat.st_size > 0x2000000) {
		result = 0x80800066;
		goto done;
	}
	if (params->dataBufSize < stat.st_size) {
		result = 0x80800063;
		goto done;
	}
	mapping = sceKernelUserMap("SceAppMgrPspSaveDataReadFileBuf", 3, params->dataBuf,
				   params->dataBufSize, &page, &pageSize, &offset);
	if (mapping < 0) {
		result = 0x80800001;
		goto done;
	}
	buffer = (char *)page + offset;
	memset(buffer, 0, params->dataBufSize);
	fd = sceIoOpen(path, 0x10001, 0);
	result = fd;
	if (fd >= 0) {
		length = sceIoRead(fd, buffer, stat.st_size);
		if (length < 0) {
			result = length;
			length = 0;
			goto close;
		}
		if ((SceOff)length != stat.st_size) {
			result = 0x80800065;
			length = 0;
			goto close;
		}
		result = sceIoClose(fd);
		if (result < 0)
			goto close;
	}
	if (result < 0)
		goto clear;
	goto unmap;
close:
	sceIoClose(fd);
clear:
	memset(buffer, 0, 4);
unmap:
	sceKernelUserUnmap(mapping, params->dataBuf, params->dataBufSize);
done:
	return result < 0 ? result : length;
}

/* Reads and decrypts a protected file of the save data, checking it against its MAC. */
static inline int appmgrReadPspSaveDataSecureFile(const char *path,
		const AppmgrPspSaveDataParams *params, const unsigned char *fileEntry)
{
	SceUInt8 header[16] = {0};
	SceUInt8 digest[16] = {0};
	SceUInt8 macContext[40];
	SceUInt8 cipherContext[24];
	void *page = NULL;
	SceSize pageSize = 0;
	SceUInt32 offset = 0;
	SceIoStat stat;
	SceUID mapping, fd;
	SceUInt32 dataVersion = params->dataVersion;
	SceSize alignedSize;
	char *buffer;
	int keyType;
	int length = 0;
	int result;

	result = sceIoGetstat(path, &stat);
	if (result < 0)
		goto done;
	if ((SceUInt64)(stat.st_size - 16) > 0x1fffff0) {
		result = 0x80800066;
		goto done;
	}
	if (params->dataBufSize < (((SceUInt32)stat.st_size - 1) & ~0xfu)) {
		result = 0x80800063;
		goto done;
	}
	mapping = sceKernelUserMap("SceAppMgrPspSaveDataReadFileBuf", 3, params->dataBuf,
				   params->dataBufSize, &page, &pageSize, &offset);
	if (mapping < 0) {
		result = 0x80800001;
		goto done;
	}
	buffer = (char *)page + offset;
	memset(buffer, 0, params->dataBufSize);
	if (dataVersion > 2) {
		result = 0x80800001;
		goto clear;
	}
	keyType = pspSaveDataKeyTypes[dataVersion];
	result = keyType;
	if (result < 0)
		goto clear;
	result = SceSblGcAuthMgrMsSaveBBForDriver_4A249828(macContext, keyType);
	if (result < 0)
		goto clear;
	fd = sceIoOpen(path, 0x10001, 0);
	result = fd;
	if (fd < 0)
		goto clear;
	result = sceIoRead(fd, header, sizeof(header));
	if (result < 0)
		goto close;
	if (result != sizeof(header)) {
		result = 0x80800065;
		goto close;
	}
	result = SceSblGcAuthMgrMsSaveBBForDriver_79A9BE44(macContext, header, sizeof(header));
	if (result < 0)
		goto close;
	result = sceIoRead(fd, buffer, stat.st_size - 16);
	if (result < 0)
		goto close;
	length = result;
	alignedSize = (length + 15) & ~15;
	result = sceIoClose(fd);
	if (result < 0) {
		length = 0;
		goto close;
	}
	result = SceSblGcAuthMgrMsSaveBBForDriver_79A9BE44(macContext, buffer, alignedSize);
	if (result < 0)
		goto clear;
	result = SceSblGcAuthMgrMsSaveBBForDriver_3C185A67(cipherContext, keyType, 2, header,
			dataVersion != 0 ? params->secureFileId : NULL);
	if (result < 0)
		goto clear;
	result = SceSblGcAuthMgrMsSaveBBForDriver_F66D5F76(cipherContext, buffer, alignedSize);
	if (result < 0)
		goto clear;
	result = SceSblGcAuthMgrMsSaveBBForDriver_805C860B(cipherContext);
	if (result < 0)
		goto clear;
	result = SceSblGcAuthMgrMsSaveBBForDriver_8E6626A0(macContext, digest,
			dataVersion != 0 ? params->secureFileId : NULL);
	if (result < 0)
		goto clear;
	if (memcmp(fileEntry + 13, digest, sizeof(digest)) != 0) {
		result = 0x80800067;
		goto clear;
	}
	goto unmap;
close:
	sceIoClose(fd);
clear:
	memset(buffer, 0, 4);
unmap:
	sceKernelUserUnmap(mapping, params->dataBuf, params->dataBufSize);
done:
	return result == 0 ? length : result;
}

int sceAppMgrPspSaveDataRead(AppmgrPspSaveDataParams *params)
{
	AppmgrSfoFile sfo = {0};
	char path[104];
	void *page, *value;
	SceSize pageSize, length, capacity;
	SceUInt32 offset;
	SceIoStat stat;
	AppmgrPspSaveDataParamSfo *paramSfo;
	const unsigned char *fileList;
	SceUInt32 syscallState;
	SceUID mapping;
	int index;
	int result;

	syscallState = sceSyscallEnter();
	if (params == NULL || !appmgrIsValidPspSaveDataDirectory(params->dirName) ||
	    !appmgrIsValidPspSaveDataFileName(params->fileName)) {
		result = 0x80800001;
		goto leave;
	}
	memset(path, 0, sizeof(path));
	snprintf(path, sizeof(path), "ux0:pspemu/PSP/SAVEDATA/%s/PARAM.SFO", params->dirName);
	result = sceKernelLockMutex(g_appmgrMountMutex, 1, NULL);
	if (result < 0)
		goto leave;
	result = appmgrLoadSfoFile(path, &sfo);
	if (result < 0)
		goto unlock;
	result = appmgrVerifyPspSaveDataSfo(&sfo, params->dataVersion, 1);
	if (result < 0)
		goto unlock;
	if (params->paramSfo != NULL) {
		page = NULL;
		pageSize = 0;
		offset = 0;
		mapping = sceKernelUserMap("SceAppMgrPspSaveDataReadSfoBuf", 2, params->paramSfo,
					   sizeof(*paramSfo), &page, &pageSize, &offset);
		if (mapping < 0) {
			result = 0x80800001;
			goto unlock;
		}
		paramSfo = (AppmgrPspSaveDataParamSfo *)((char *)page + offset);
		memset(paramSfo, 0, sizeof(*paramSfo));
		result = appmgrGetPspSaveDataParamSfo(&sfo, paramSfo);
		if (result < 0) {
			memset(paramSfo, 0, sizeof(*paramSfo));
			sceKernelUserUnmap(mapping, params->paramSfo, sizeof(*paramSfo));
			goto unlock;
		}
		sceKernelUserUnmap(mapping, params->paramSfo, sizeof(*paramSfo));
	}
	if (params->dataBuf == NULL) {
		result = 0;
		goto unlock;
	}
	memset(path, 0, sizeof(path));
	snprintf(path, sizeof(path), "ux0:pspemu/PSP/SAVEDATA/%s/%s", params->dirName,
		 params->fileName);
	value = NULL;
	length = 0;
	capacity = 0;
	result = appmgrGetSfoEntry(&sfo, "SAVEDATA_FILE_LIST", &value, &length, &capacity);
	if (result >= 0) {
		if (length < 3168 || capacity != 3168) {
			result = 0x80800034;
			goto unlock;
		}
		fileList = value;
		for (index = 0; index < 99; index++, fileList += 32) {
			if (fileList[0] != '\0' &&
			    strncmp(params->fileName, (const char *)fileList, 13) == 0)
				break;
		}
		if (index < 99) {
			if (params->fileType != 0) {
				result = 0x80800062;
				goto unlock;
			}
			result = appmgrReadPspSaveDataSecureFile(path, params, fileList);
			goto unlock;
		}
	} else if (result != (int)0x80800061) {
		goto unlock;
	}
	if (params->fileType == 0) {
		result = sceIoGetstat(path, &stat);
		if (result >= 0)
			result = 0x80800062;
	} else if (params->fileType == 1) {
		result = appmgrReadPspSaveDataNormalFile(path, params);
	} else {
		result = 0x80800001;
	}
unlock:
	sceKernelUnlockMutex(g_appmgrMountMutex, 1);
leave:
	appmgrReleaseSfoFile(&sfo);
	sceSyscallLeave(syscallState);
	return result;
}

/* PLACEHOLDER: cryptographic routine, deferred (signs a cloud data header: SHA-256, RSA-2048) */
int SceAppMgrForDriver_ED87F191(const char *accountId, SceSize size, const void *digest,
				void *header)
{
	if (digest == NULL || accountId == NULL || size > 0x40000000 || header == NULL)
		return 0x80800001;
	return 0x80800081;
}

/* PLACEHOLDER: cryptographic routine, deferred (verifies a cloud data header's signature) */
int SceAppMgrForDriver_55D48190(const void *header)
{
	if (header == NULL)
		return 0x80800001;
	return 0x80800083;
}

/* The cloud data key: the account ID in hex and the key's parameters. */
int SceAppMgrForDriver_2FAA6ABD(SceAppMgrCloudDataKey *key)
{
	SceUInt64 accountId = 0;
	char accountIdString[17] = "";
	int result;

	if (key == NULL)
		return 0x80800001;
	result = appmgrGetAccountIdentity(&accountId, NULL, NULL);
	if (result < 0)
		return result;
	snprintf(accountIdString, sizeof(accountIdString), "%016llx", accountId);
	if (accountId == 0)
		result = 0x80800080;
	key->parameters = 0x0002000100020000ULL;
	__builtin_memcpy(key->accountId, accountIdString, sizeof(key->accountId));
	return result;
}

/* PLACEHOLDER: cryptographic routine, deferred (derives the offline ID: HMAC-SHA256, embedded key) */
int SceAppMgrForDriver_61B13981(void *offlineId, SceUInt32 *checkWord)
{
	if (checkWord == NULL || offlineId == NULL)
		return 0x80800001;
	return 0x80800081;
}

/* PLACEHOLDER: cryptographic routine, deferred (verifies an offline backup header) */
int SceAppMgrForDriver_62E98C42(const void *header)
{
	if (header == NULL)
		return 0x80800001;
	return 0x80800083;
}

int SceAppMgrForDriver_43A35729(const SceAppMgrPhotoMountParam *param)
{
	ScePfsRndDriveId mountPoint = {{0}};
	SceUInt32 syscallState;
	int result;

	syscallState = sceSyscallEnter();
	if (!param || param->size != sizeof(*param)) {
		result = (int)0x80800001;
		goto leaveSyscall;
	}
	result = SceAppMgrForDriver_B1D3C287(100, &mountPoint);
	if (result == (int)0x80010013)
		result = (int)0x80801005;
	else if (result == (int)0x80800003)
		result = (int)0x808010C0;
leaveSyscall:
	sceSyscallLeave(syscallState);
	return result;
}

int SceAppMgrForDriver_DCADB9FE(const SceAppMgrPhotoUmountParam *param)
{
	SceUInt32 syscallState;
	int result;

	syscallState = sceSyscallEnter();
	if (!param || param->size != sizeof(*param)) {
		result = (int)0x80800001;
		goto leaveSyscall;
	}
	result = SceAppMgrForDriver_A714BB35("photo0:");
	if (result == (int)0x80800002)
		result = (int)0x808010C1;
leaveSyscall:
	sceSyscallLeave(syscallState);
	return result;
}

int appmgrInitAppInfoCache(SceUID pid)
{
	SceAppMgrAppInfoCache *info;
	void *storage = NULL;
	int addContentCapacity = 16;
	int result;

	if (!sceSblACMgrIsGameProgram(pid))
		addContentCapacity = SceSblACMgrForDriver_6C5AB07F(pid) != 0;
	result = SceProcessmgrForDriver_AF80F39C(pid, g_appmgrAppInfoCacheKey, &storage, 1);
	if (result < 0)
		return result;
	if (storage == NULL)
		return (int)0x80800012;
	info = storage;
	memset(info, 0, 1024);
	info->addContentCapacity = addContentCapacity;
	return result;
}
