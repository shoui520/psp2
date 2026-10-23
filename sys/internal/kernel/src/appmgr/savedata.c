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
#include <ss_mgr/ss_mgr.h>

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

extern int g_appmgrAppInfoCacheKey;

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

/*
 * The cloud data MC ID, kept either in tm0:/clouddata/mcid.dat or in the memory card's
 * media ID sector; NVS area 5 bit 0 chooses the memory card when the caller does not.
 */
typedef struct AppmgrCloudDataMcIdFile {
	SceUInt32 magic;	/* "CDMI" */
	SceUInt32 version;
	SceUInt64 mcId;
	SceUInt8 reserved[48];
} AppmgrCloudDataMcIdFile;

typedef struct AppmgrMediaIdSector {
	SceUInt32 magic;	/* "\0DIM" */
	SceUInt32 version;
	SceUInt32 flags;	/* bit 0: the MC ID is set */
	SceUInt8 mcId[8];
	SceUInt8 unused[8];
	SceUInt8 reserved[484];
} AppmgrMediaIdSector;

_Static_assert(sizeof(AppmgrCloudDataMcIdFile) == 64, "mcid.dat size");
_Static_assert(sizeof(AppmgrMediaIdSector) == 512, "media ID sector size");

#define MCID_LOCATION_MEMORY_CARD	1
#define MCID_LOCATION_CLOUD_DATA	2
#define CLOUD_DATA_MCID_MAGIC		0x494D4443
#define MEDIA_ID_SECTOR_MAGIC		0x4D494400
#define MEDIA_ID_SECTOR_OFFSET		512

static const unsigned char encodedMemoryCardMediaId[32] = {
	(unsigned char)~'s', (unsigned char)~'d', (unsigned char)~'s',
	(unsigned char)~'t', (unsigned char)~'o', (unsigned char)~'r',
	(unsigned char)~'0', (unsigned char)~':', (unsigned char)~'x',
	(unsigned char)~'m', (unsigned char)~'c', (unsigned char)~'-',
	(unsigned char)~'l', (unsigned char)~'p', (unsigned char)~'-',
	(unsigned char)~'a', (unsigned char)~'c', (unsigned char)~'t',
	(unsigned char)~'-', (unsigned char)~'m', (unsigned char)~'e',
	(unsigned char)~'d', (unsigned char)~'i', (unsigned char)~'a',
	(unsigned char)~'i', (unsigned char)~'d', 0
};
static const unsigned char encodedCloudDataMcIdFile[24] = {
	(unsigned char)~'t', (unsigned char)~'m', (unsigned char)~'0',
	(unsigned char)~':', (unsigned char)~'/', (unsigned char)~'c',
	(unsigned char)~'l', (unsigned char)~'o', (unsigned char)~'u',
	(unsigned char)~'d', (unsigned char)~'d', (unsigned char)~'a',
	(unsigned char)~'t', (unsigned char)~'a', (unsigned char)~'/',
	(unsigned char)~'m', (unsigned char)~'c', (unsigned char)~'i',
	(unsigned char)~'d', (unsigned char)~'.', (unsigned char)~'d',
	(unsigned char)~'a', (unsigned char)~'t', 0
};
static const unsigned char encodedCloudDataDirectory[16] = {
	(unsigned char)~'t', (unsigned char)~'m', (unsigned char)~'0',
	(unsigned char)~':', (unsigned char)~'/', (unsigned char)~'c',
	(unsigned char)~'l', (unsigned char)~'o', (unsigned char)~'u',
	(unsigned char)~'d', (unsigned char)~'d', (unsigned char)~'a',
	(unsigned char)~'t', (unsigned char)~'a', 0
};

static SceUInt64 g_appmgrCloudDataMcId;
static SceUInt64 g_appmgrMemoryCardMcId;

int SceAppMgrForDriver_0B94BB90(int location, const SceUInt64 *mcId)
{
	AppmgrCloudDataMcIdFile file;
	AppmgrMediaIdSector *sector;
	SceUInt64 newMcId;
	char path[64];
	int useCloudData, permission, fd, written, result;

	if (location == MCID_LOCATION_MEMORY_CARD) {
		useCloudData = 0;
	} else if (location == MCID_LOCATION_CLOUD_DATA) {
		useCloudData = 1;
	} else {
		result = sceSblSsGetNvsData(5, &newMcId, 1);
		if (result < 0)
			return result;
		useCloudData = !(*(SceUInt8 *)&newMcId & 1);
	}

	if (mcId != NULL) {
		newMcId = *mcId;
	} else if (useCloudData && g_appmgrCloudDataMcId != 0) {
		newMcId = g_appmgrCloudDataMcId;
	} else if (!useCloudData && g_appmgrMemoryCardMcId != 0) {
		newMcId = g_appmgrMemoryCardMcId;
	} else {
		result = sceSblRngPseudoRandomNumber(&newMcId, sizeof(newMcId));
		if (result < 0)
			return result;
	}

	if (useCloudData) {
		g_appmgrCloudDataMcId = 0;
		memset(&file, 0, sizeof(file));
		__builtin_memcpy(&file.magic, "CDMI", sizeof(file.magic));
		file.version = 1;
		file.mcId = newMcId;
		permission = sceKernelSetThreadAccessLevel(0x80);
		result = appmgrDecodeString(encodedCloudDataDirectory, path, sizeof(path));
		if (result < 0)
			goto restore;
		result = sceIoMkdir(path, 6);
		if (result < 0 && result != (int)0x80010011)
			goto restore;
		result = appmgrDecodeString(encodedCloudDataMcIdFile, path, sizeof(path));
		if (result < 0)
			goto restore;
		fd = sceIoOpen(path, 0x603, 6);
		if (fd < 0) {
			result = 0x80800087;
			goto restore;
		}
		written = sceIoWrite(fd, &file, sizeof(file));
		sceIoClose(fd);
		if (written != sizeof(file)) {
			result = 0x80800089;
			goto restore;
		}
		sceKernelSetThreadAccessLevel(permission);
		g_appmgrCloudDataMcId = newMcId;
		return 0;
	}

	g_appmgrMemoryCardMcId = 0;
	permission = sceKernelSetThreadAccessLevel(0x80);
	sector = appmgrAllocMountHeapMemory(sizeof(*sector));
	if (sector == NULL) {
		result = 0x80801006;
		goto restore;
	}
	memset(sector, 0xFF, sizeof(*sector));
	sector->magic = MEDIA_ID_SECTOR_MAGIC;
	sector->version = 1;
	__builtin_memcpy(sector->mcId, &newMcId, sizeof(sector->mcId));
	sector->flags = 1;
	memset(sector->reserved, 0, sizeof(sector->reserved));
	result = appmgrDecodeString(encodedMemoryCardMediaId, path, sizeof(path));
	if (result < 0)
		goto free_sector;
	fd = sceIoOpen(path, 3, 0);
	if (fd < 0) {
		result = 0x80800087;
		goto free_sector;
	}
	result = 0;
	if ((int)sceIoLseek(fd, MEDIA_ID_SECTOR_OFFSET, 0) < 0) {
		result = 0x80800089;
		sceIoClose(fd);
		goto free_sector;
	}
	written = sceIoWrite(fd, sector, sizeof(*sector));
	sceIoClose(fd);
	if (written != sizeof(*sector)) {
		result = 0x80800089;
		goto free_sector;
	}
	appmgrFreeMountHeapMemory(sector);
	sceKernelSetThreadAccessLevel(permission);
	g_appmgrMemoryCardMcId = newMcId;
	return result;
free_sector:
	appmgrFreeMountHeapMemory(sector);
restore:
	sceKernelSetThreadAccessLevel(permission);
	return result;
}

int SceAppMgrForDriver_1286AF64(int location, SceUInt64 *mcId)
{
	AppmgrCloudDataMcIdFile file;
	AppmgrMediaIdSector *sector;
	SceUInt64 nvsData;
	char path[64];
	int permission, fd, size, result;

	if (mcId == NULL)
		return 0x80800001;
	if (location == MCID_LOCATION_MEMORY_CARD)
		goto memory_card;
	if (location != MCID_LOCATION_CLOUD_DATA) {
		result = sceSblSsGetNvsData(5, &nvsData, 1);
		if (result < 0)
			return result;
		if (*(SceUInt8 *)&nvsData & 1)
			goto memory_card;
	}

	if (g_appmgrCloudDataMcId != 0) {
		*mcId = g_appmgrCloudDataMcId;
		return 0;
	}
	memset(&file, 0, sizeof(file));
	permission = sceKernelSetThreadAccessLevel(0x80);
	result = appmgrDecodeString(encodedCloudDataMcIdFile, path, sizeof(path));
	if (result < 0)
		goto restore;
	fd = sceIoOpen(path, 1, 0);
	if (fd < 0) {
		result = 0x80800087;
		goto restore;
	}
	size = sceIoRead(fd, &file, sizeof(file));
	sceIoClose(fd);
	if (size < 0) {
		result = 0x80800088;
		goto restore;
	}
	if (size == sizeof(file)) {
		appmgrDebugHexDump(&file, sizeof(file));
		if (file.magic == CLOUD_DATA_MCID_MAGIC && file.version == 1) {
			sceKernelSetThreadAccessLevel(permission);
			*mcId = file.mcId;
			g_appmgrCloudDataMcId = file.mcId;
			return 0;
		}
	}
	result = 0x80800086;
	goto restore;

memory_card:
	if (g_appmgrMemoryCardMcId != 0) {
		*mcId = g_appmgrMemoryCardMcId;
		return 0;
	}
	permission = sceKernelSetThreadAccessLevel(0x80);
	sector = appmgrAllocMountHeapMemory(sizeof(*sector));
	if (sector == NULL) {
		result = 0x80801006;
		goto restore;
	}
	memset(sector, 0, sizeof(*sector));
	result = appmgrDecodeString(encodedMemoryCardMediaId, path, sizeof(path));
	if (result < 0)
		goto free_sector;
	fd = sceIoOpen(path, 1, 0);
	if (fd < 0) {
		result = 0x80800087;
		goto free_sector;
	}
	if ((int)sceIoLseek(fd, MEDIA_ID_SECTOR_OFFSET, 0) < 0) {
		result = 0x80800088;
		sceIoClose(fd);
		goto free_sector;
	}
	size = sceIoRead(fd, sector, sizeof(*sector));
	sceIoClose(fd);
	if (size < 0) {
		result = 0x80800088;
		goto free_sector;
	}
	if (size != sizeof(*sector)) {
		result = 0x80800086;
		goto free_sector;
	}
	appmgrDebugHexDump(sector, sizeof(*sector));
	if (sector->magic != MEDIA_ID_SECTOR_MAGIC || sector->version != 1 ||
	    !(sector->flags & 1)) {
		result = 0x80800085;
		goto free_sector;
	}
	__builtin_memcpy(mcId, sector->mcId, sizeof(sector->mcId));
	appmgrFreeMountHeapMemory(sector);
	sceKernelSetThreadAccessLevel(permission);
	g_appmgrMemoryCardMcId = *mcId;
	return 0;
free_sector:
	appmgrFreeMountHeapMemory(sector);
restore:
	sceKernelSetThreadAccessLevel(permission);
	return result;
}

int SceAppMgrForDriver_EC408FEF(int location)
{
	AppmgrMediaIdSector *sector;
	SceUInt32 nvsData;
	char path[64];
	int permission, fd, written, result;

	if (location == MCID_LOCATION_MEMORY_CARD)
		goto memory_card;
	if (location != MCID_LOCATION_CLOUD_DATA) {
		result = sceSblSsGetNvsData(5, &nvsData, 1);
		if (result < 0)
			return result;
		if (*(SceUInt8 *)&nvsData & 1)
			goto memory_card;
	}

	permission = sceKernelSetThreadAccessLevel(0x80);
	result = appmgrDecodeString(encodedCloudDataMcIdFile, path, sizeof(path));
	if (result < 0)
		goto restore;
	result = sceIoRemove(path);
	if (result < 0 && result != (int)0x80010002)
		goto restore;
	result = appmgrDecodeString(encodedCloudDataDirectory, path, sizeof(path));
	if (result < 0)
		goto restore;
	result = sceIoRmdir(path);
	if (result < 0 && result != (int)0x80010002)
		goto restore;
	sceKernelSetThreadAccessLevel(permission);
	g_appmgrCloudDataMcId = 0;
	return 0;

memory_card:
	permission = sceKernelSetThreadAccessLevel(0x80);
	sector = appmgrAllocMountHeapMemory(sizeof(*sector));
	if (sector == NULL) {
		result = 0x80801006;
		goto restore;
	}
	memset(sector, 0xFF, sizeof(*sector));
	result = appmgrDecodeString(encodedMemoryCardMediaId, path, sizeof(path));
	if (result < 0)
		goto free_sector;
	fd = sceIoOpen(path, 3, 0);
	if (fd < 0) {
		result = 0x80800087;
		goto free_sector;
	}
	result = 0;
	if ((int)sceIoLseek(fd, MEDIA_ID_SECTOR_OFFSET, 0) < 0) {
		result = 0x80800089;
		sceIoClose(fd);
		goto free_sector;
	}
	written = sceIoWrite(fd, sector, sizeof(*sector));
	sceIoClose(fd);
	if (written != sizeof(*sector)) {
		result = 0x80800089;
		goto free_sector;
	}
	appmgrFreeMountHeapMemory(sector);
	sceKernelSetThreadAccessLevel(permission);
	g_appmgrMemoryCardMcId = 0;
	return result;
free_sector:
	appmgrFreeMountHeapMemory(sector);
restore:
	sceKernelSetThreadAccessLevel(permission);
	return result;
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

/*
 * Safe memory: a buffer of the application's (its KPLS block or its heap) kept in a file in
 * the save data's sce_sys (PSM and system applications: under ux0:/psm or ux0:/cache), which
 * a working copy "_<file>" shadows while the application runs when flag bit 0 is set.
 */
extern SceUID g_appmgrSafeMemoryMutex;

static const char safeMemoryFileName[] = "safemem.dat";
static const char psmDevAssistantSafeMemoryFileName[] = "pm.dat";

static int appmgrSyncSafeMemoryFile(SceUID pid, SceUInt32 flags, char *directory,
				    void *memory, SceSize size)
{
	SceIoStat stat;
	char path[292] = "";
	const char *fileName = safeMemoryFileName;
	int initialize = 0;
	int working, fd, transferred, result;

	if (SceSblACMgrForDriver_C98D82EE(pid))
		fileName = psmDevAssistantSafeMemoryFileName;
	if (directory == NULL || strnlen(directory, sizeof(path)) >= 0x110)
		return 0x80800001;

	working = flags & 1;
	if (working) {
		snprintf(path, sizeof(path), "%s/_%s", directory, fileName);
		sceIoRemove(path);
		if (sceRegMgrGetKeyInt("/CONFIG/GAME", "init_safememory", &initialize) < 0) {
			initialize = 0;
		} else if (initialize == 1) {
			snprintf(path, sizeof(path), "%s/%s", directory, fileName);
			sceIoRemove(path);
		}
	}

	snprintf(path, sizeof(path), "%s/%s", directory, fileName);
	if (sceIoGetstat(path, &stat) < 0 || stat.st_size != (SceOff)size) {
		if (working)
			snprintf(path, sizeof(path), "%s/_%s", directory, fileName);
		fd = sceIoOpen(path, 0x603, 6);
		if (fd < 0)
			return 0;
		transferred = sceIoWrite(fd, memory, size);
		sceIoClose(fd);
		if (transferred != size)
			return 0x80800014;
		return 0;
	}

	fd = sceIoOpen(path, 1, 6);
	if (fd < 0)
		return fd;
	transferred = sceIoRead(fd, memory, size);
	sceIoClose(fd);
	if (transferred != 0x10000 && transferred != size)
		return 0x80800014;
	if (!working)
		return 0;
	__builtin_strcat(directory, "/_");
	strncat(directory, fileName, 13);
	directory[291] = 0;
	result = sceIoRename(path, directory);
	return result < 0 ? result : 0;
}

static inline int appmgrGetSafeMemoryDirectory(SceUID pid, const SceAppMgrAppInfoCache *info,
					       char *directory)
{
	memset(directory, 0, 292);
	if (SceSblACMgrForDriver_091F7247(pid)) {
		snprintf(directory, 292, "ux0:/psm/%s/RW/System", info->safeMemoryDirectory);
		return 0;
	}
	if (SceSblACMgrForDriver_A67E8E5B(pid)) {
		snprintf(directory, 292, "ux0:/cache/%s/%s/System", info->titleId,
			 info->safeMemoryDirectory);
		return 0;
	}
	return appmgrResolvePath("savedata0:sce_sys", pid, directory, 292);
}

int appmgrSetupSafeMemoryForProcess(SceUID pid, int safeMemoryMode)
{
	SceAppMgrAppInfoCache *info = NULL;
	void *memory = NULL;
	SceAllocOpt opt;
	char directory[292] = "";
	SceSize size;
	int result;

	result = SceProcessmgrForDriver_AF80F39C(pid, g_appmgrAppInfoCacheKey, (void **)&info, 0);
	if (result < 0)
		return result;
	if (info == NULL)
		return 0x80800012;
	info->safeMemoryFlags = safeMemoryMode;
	info->safeMemoryInitialized = 0;

	if (safeMemoryMode & 2) {
		result = SceProcessmgrForDriver_AF80F39C(pid, g_appmgrSafeMemoryKey, &memory, 1);
		if (result < 0)
			return result;
		size = 0x10000;
	} else {
		if (safeMemoryMode & 4)
			size = 0x20000;
		else if (safeMemoryMode & 8)
			size = 0x40000;
		else if (safeMemoryMode & 0x10)
			size = 0x80000;
		else if (safeMemoryMode & 0x20)
			size = 0x100000;
		else
			return 0x80800001;
		opt.size = sizeof(opt);
		opt.mmapKind = 0;
		opt.align = 32;
		opt.mmapBase = NULL;
		opt.mmapSize = 0;
		memory = sceKernelAllocRemoteProcessHeap(pid, size, &opt);
	}
	if (memory == NULL)
		return 0x80800013;

	memset(memory, 0, size);
	info->safeMemory = memory;
	info->safeMemorySize = size;
	if (SceSblACMgrForDriver_C98D82EE(pid))
		return 0;
	result = appmgrGetSafeMemoryDirectory(pid, info, directory);
	if (result < 0)
		return result;
	result = appmgrSyncSafeMemoryFile(pid, safeMemoryMode, directory, memory, size);
	if (result < 0)
		return result;
	info->safeMemoryInitialized = 1;
	return 0;
}

int sceAppMgrInitSafeMemoryById(const char *id)
{
	SceAppMgrAppInfoCache *info = NULL;
	char directory[292] = "";
	const unsigned char *character;
	void *memory;
	SceSize size;
	int syscallState, length, permission, result;

	syscallState = sceSyscallEnter();
	if (!SceSblACMgrForDriver_C98D82EE(0)) {
		result = 0x80800009;
		goto leave;
	}
	if (SceSblACMgrForDriver_091F7247(0) && id == NULL) {
		result = 0x80800001;
		goto leave;
	}
	if (SceSblACMgrForDriver_A67E8E5B(0) && id == NULL) {
		result = 0x80801001;
		goto leave;
	}

	result = sceKernelLockMutex(g_appmgrSafeMemoryMutex, 1, NULL);
	if (result < 0)
		goto leave;
	result = SceProcessmgrForDriver_AF80F39C(0, g_appmgrAppInfoCacheKey, (void **)&info, 0);
	if (result < 0)
		goto unlock;
	if (info == NULL) {
		result = 0x80800012;
		goto unlock;
	}
	if (info->safeMemoryInitialized != 0) {
		result = 0x8080004A;
		goto unlock;
	}
	memory = info->safeMemory;
	size = info->safeMemorySize;

	if (SceSblACMgrForDriver_091F7247(0)) {
		if (!appmgrIsValidTitleId(id, 0)) {
			result = 0x80800001;
			goto unlock;
		}
	} else {
		result = 0x80800009;
		if (!SceSblACMgrForDriver_A67E8E5B(0))
			goto unlock;
		length = 0;
		for (character = (const unsigned char *)id; *character != 0; character++) {
			if ((unsigned int)(*character - '0') > 9 &&
			    (unsigned int)((*character & 0xDF) - 'A') > 25 &&
			    *character != '-' && *character != '_') {
				result = 0x80800001;
				goto unlock;
			}
			if (++length == 64) {
				result = 0x80800001;
				goto unlock;
			}
		}
	}
	memset(info->safeMemoryDirectory, 0, sizeof(info->safeMemoryDirectory));
	strncpy(info->safeMemoryDirectory, id, sizeof(info->safeMemoryDirectory) - 1);
	result = appmgrGetSafeMemoryDirectory(0, info, directory);
	if (result < 0)
		goto unlock;

	permission = sceKernelSetThreadAccessLevel(0x80);
	result = appmgrCreateDirectories(directory, 0);
	sceKernelSetThreadAccessLevel(permission);
	if (result < 0) {
		result = 0x80800049;
		goto unlock;
	}
	SceKernelSuspendForDriver_4DF40893(0);
	permission = sceKernelSetThreadAccessLevel(0x80);
	result = appmgrSyncSafeMemoryFile(0, info->safeMemoryFlags, directory, memory, size);
	sceKernelSetThreadAccessLevel(permission);
	SceKernelSuspendForDriver_2BB92967(0);
	if (result >= 0)
		info->safeMemoryInitialized = 1;
unlock:
	sceKernelUnlockMutex(g_appmgrSafeMemoryMutex, 1);
leave:
	sceSyscallLeave(syscallState);
	return result;
}

int appmgrSaveSafeMemoryForProcess(SceUID pid)
{
	SceAppMgrAppInfoCache *info = NULL;
	SceIoStat stat;
	char root[64];
	char directory[292] = "";
	char path[292] = "";
	const char *fileName;
	void *memory;
	SceSize size;
	int fd, written, result;

	result = SceProcessmgrForDriver_AF80F39C(pid, g_appmgrAppInfoCacheKey, (void **)&info, 0);
	if (result < 0)
		return result;
	if (info == NULL)
		return 0x80800012;
	if (info->safeMemoryInitialized == 0)
		return 0;
	memory = info->safeMemory;
	size = info->safeMemorySize;
	if (memory == NULL)
		return 0x80800013;

	fileName = psmDevAssistantSafeMemoryFileName;
	if (!SceSblACMgrForDriver_C98D82EE(pid))
		fileName = safeMemoryFileName;
	result = appmgrGetSafeMemoryDirectory(pid, info, directory);
	if (result < 0)
		return result;
	snprintf(path, sizeof(path), (info->safeMemoryFlags & 1) ? "%s/_%s" : "%s/%s",
		 directory, fileName);

	if (info->safeMemoryDirty != 0) {
		fd = sceIoOpen(path, 0x603, 6);
		if (fd < 0)
			return fd;
		written = sceIoWrite(fd, memory, size);
		sceIoClose(fd);
		if (written != size)
			return 0x80800014;
		if (info != NULL) {
			info->saveDataFlags |= 2;
			if (SceRtcForDriver_A930CD1A(&stat.st_mtime, 0) >= 0 &&
			    appmgrResolvePath("savedata0:", pid, root, sizeof(root)) >= 0)
				sceIoChstat(root, &stat, 0x20);
		}
	}
	info->safeMemoryDirty = 0;
	if (!(info->safeMemoryFlags & 1))
		return 0;
	APPMGR_STRCAT(directory, "/", sizeof(directory));
	APPMGR_STRNCAT(directory, fileName, 13, sizeof(directory));
	directory[291] = 0;
	sceIoRemove(directory);
	result = sceIoRename(path, directory);
	return result < 0 ? result : 0;
}

int appmgrPrepareSafeMemoryForProcess(SceUID pid)
{
	SceAppMgrAppInfoCache *info = NULL;
	char directory[292] = "";
	char path[292] = "";
	const char *fileName;
	int result;

	result = SceProcessmgrForDriver_AF80F39C(pid, g_appmgrAppInfoCacheKey, (void **)&info, 0);
	if (result < 0)
		return result;
	if (info == NULL)
		return 0x80800012;
	if (info->safeMemoryInitialized == 0)
		return 0;
	if (!(info->safeMemoryFlags & 1))
		return 0;

	if (SceSblACMgrForDriver_C98D82EE(pid))
		fileName = psmDevAssistantSafeMemoryFileName;
	else
		fileName = safeMemoryFileName;
	result = appmgrGetSafeMemoryDirectory(pid, info, directory);
	if (result < 0)
		return result;
	snprintf(path, sizeof(path), "%s/%s", directory, fileName);
	APPMGR_STRCAT(directory, "/_", sizeof(directory));
	APPMGR_STRNCAT(directory, fileName, 13, sizeof(directory));
	directory[291] = 0;
	result = sceIoRename(path, directory);
	return result < 0 ? result : 0;
}

int sceAppMgrSaveSafeMemory(const void *data, SceSize size, SceOff offset)
{
	SceAppMgrAppInfoCache *info;
	int syscallState, result;

	syscallState = sceSyscallEnter();
	result = sceKernelLockMutex(g_appmgrSafeMemoryMutex, 1, NULL);
	if (result < 0) {
		sceSyscallLeave(syscallState);
		return result;
	}
	SceKernelSuspendForDriver_4DF40893(0);
	info = SceProcessmgrForDriver_EE694840(g_appmgrAppInfoCacheKey);
	if (info == NULL) {
		result = 0x80800012;
		goto unlock;
	}
	if (info->safeMemory == NULL) {
		result = 0x808010E0;
		goto unlock;
	}
	if (data == NULL) {
		result = 0x80801001;
		goto unlock;
	}
	if (offset < 0 || (SceOff)info->safeMemorySize < offset || info->safeMemorySize < size ||
	    (SceOff)info->safeMemorySize < offset + size) {
		result = 0x80801000;
		goto unlock;
	}
	result = size;
	if (size != 0) {
		int copied = sceKernelCopyFromUser((char *)info->safeMemory + offset, data, size);

		info->safeMemoryDirty = 1;
		SceKernelSuspendForDriver_2BB92967(0);
		sceKernelUnlockMutex(g_appmgrSafeMemoryMutex, 1);
		sceSyscallLeave(syscallState);
		return copied < 0 ? copied : (int)size;
	}
	info->safeMemoryDirty = 1;
unlock:
	SceKernelSuspendForDriver_2BB92967(0);
	sceKernelUnlockMutex(g_appmgrSafeMemoryMutex, 1);
	sceSyscallLeave(syscallState);
	return result;
}

int sceAppMgrLoadSafeMemory(void *data, SceSize size, SceOff offset)
{
	SceAppMgrAppInfoCache *info;
	int syscallState, copied, result;

	syscallState = sceSyscallEnter();
	result = sceKernelLockMutex(g_appmgrSafeMemoryMutex, 1, NULL);
	if (result < 0) {
		sceSyscallLeave(syscallState);
		return result;
	}
	SceKernelSuspendForDriver_4DF40893(0);
	info = SceProcessmgrForDriver_EE694840(g_appmgrAppInfoCacheKey);
	if (info == NULL) {
		result = 0x80800012;
		goto unlock;
	}
	if (info->safeMemory == NULL) {
		result = 0x808010E0;
		goto unlock;
	}
	if (data == NULL) {
		result = 0x80801001;
		goto unlock;
	}
	if (offset < 0 || (SceOff)info->safeMemorySize < offset || info->safeMemorySize < size ||
	    (SceOff)info->safeMemorySize < offset + size) {
		result = 0x80801000;
		goto unlock;
	}
	result = size;
	if (size == 0)
		goto unlock;
	copied = sceKernelCopyToUser(data, (char *)info->safeMemory + offset, size);
	SceKernelSuspendForDriver_2BB92967(0);
	sceKernelUnlockMutex(g_appmgrSafeMemoryMutex, 1);
	sceSyscallLeave(syscallState);
	return copied < 0 ? copied : (int)size;
unlock:
	SceKernelSuspendForDriver_2BB92967(0);
	sceKernelUnlockMutex(g_appmgrSafeMemoryMutex, 1);
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
