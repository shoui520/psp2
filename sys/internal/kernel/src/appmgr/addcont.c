/* Additional content. */

#include <fw.h>
#include <scetypes.h>
#include <string.h>
#include <threadmgr/threadmgr.h>
#include <processmgr/processmgr.h>
#include <iofilemgr/iofilemgr.h>
#include <intrmgr/syscall.h>
#include <acmgr/acmgr.h>
#include <pfsmgr/pfsmgr.h>
#include <sysmem/debug.h>
#include <appmgr/appmgr.h>
#include "app_state_internal.h"
#include "appmgr_savedata_slot.h"
#include "appmgr_device.h"
#include "process_content_internal.h"
#include "addcont_internal.h"
#include "mount.h"

extern SceUID g_appmgrAppMutex;
extern SceUID g_appmgrMountMutex;
extern SceUID g_appmgrMainMutex;
extern SceUID g_appmgrMainMutexOwner;

#if FW_DEBUG
#define ADDCONT_MAIN_LOCK(_line) do { \
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
#define ADDCONT_MAIN_UNLOCK(_line) do { \
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
static const SceKernelDebugInfo addContMainLockPanicInfo = {
	.fileHash = 0xc9656e8e,
	.lineHash = 0x79d3a1f0,
	.funcHash = 0x9d536bce,
};

#define ADDCONT_MAIN_LOCK(_line) do { \
	if (sceKernelLockMutex(g_appmgrMainMutex, 1, NULL) != 0) \
		SceDebugForDriver_391B5B74(&addContMainLockPanicInfo, \
					   __builtin_return_address(0)); \
	g_appmgrMainMutexOwner = sceKernelGetThreadId(); \
} while (0)
#define ADDCONT_MAIN_UNLOCK(_line) do { \
	g_appmgrMainMutexOwner = -1; \
	if (sceKernelUnlockMutex(g_appmgrMainMutex, 1) != 0) \
		SceDebugForDriver_391B5B74(&addContMainLockPanicInfo, \
					   __builtin_return_address(0)); \
} while (0)
#endif

/*
 * 1 when titleId is one of the calling application's other add-on content titles; with add
 * set, any title but its own.
 */
int sceAppMgrIsRelativeAddContAdd(const char *titleId, int add)
{
	struct AppState *app;
	int i, result = 0;

	ADDCONT_MAIN_LOCK(72);
	app = appmgrFindAppByProcessId(g_appmgrAppStates, sceKernelGetProcessId());
	if (app != NULL &&
	    strncmp(app->process.content.addContTitleIds[0], titleId, 16) != 0) {
		if (add != 0) {
			result = 1;
		} else {
			for (i = 1; i < 8; i++) {
				if (app->process.content.addContTitleIds[i][0] == '\0')
					break;
				if (strncmp(app->process.content.addContTitleIds[i], titleId, 16) == 0) {
					result = 1;
					break;
				}
			}
		}
	}
	ADDCONT_MAIN_UNLOCK(107);
	return result;
}

/* The add-on content drive a mount point names: 1 for addcont0:, 2 for addcont1:, else 0. */
static inline int appmgrGetAddContMountSelector(const char *mountPoint)
{
	if (strncmp(mountPoint, "addcont0:", 16) == 0)
		return 1;
	if (strncmp(mountPoint, "addcont1:", 16) == 0)
		return 2;
	return 0;
}

/* Open an add-on content directory of the calling application's drive. */
int SceAppMgrForDriver_EA75D157(const SceAppMgrDrmOpenParam *param)
{
	SceAppMgrAppInfoCache *info = NULL;
	SceUInt32 syscallState = sceSyscallEnter();
	AppmgrAddContentEntry *entries;
	int selector, freeSlot, count, i;
	int result;

	if (param == NULL || param->size != sizeof(*param)) {
		result = 0x80800001;
		goto out;
	}
	selector = appmgrGetAddContMountSelector(param->mountPoint);
	if (selector == 0) {
		result = 0x80800001;
		goto out;
	}
	result = sceKernelLockMutex(g_appmgrMountMutex, 1, NULL);
	if (result < 0)
		goto out;
	result = sceKernelGetRemoteKPLS(0, g_appmgrAppInfoCacheKey, (void **)&info, 0);
	if (result < 0)
		goto unlock;
	if (info == NULL) {
		result = 0x80800012;
		goto unlock;
	}
	entries = info->addContent;
	count = info->addContentCapacity;
	freeSlot = -1;
	for (i = 0; i < count; i++) {
		if (entries[i].mountSelector == 0) {
			if (freeSlot == -1)
				freeSlot = i;
		} else if (entries[i].mountSelector == selector &&
			   strncmp(entries[i].contentId, param->dirName.data, 17) == 0) {
			result = 0x80010078;
			goto unlock;
		}
	}
	if (freeSlot == -1) {
		result = 0x8001001C;
		goto unlock;
	}
	result = appmgrMountPfsAcidDirectory(param->mountPoint, param->dirName.data);
	if (result == 0) {
		if (freeSlot < info->addContentCapacity) {
			entries = &info->addContent[freeSlot];
			memset(entries, 0, sizeof(*entries));
			entries->mountSelector = selector;
			strncpy(entries->contentId, param->dirName.data, 16);
		} else {
			result = 0x80800001;
			appmgrUnmountPfsAcidDirectory(param->mountPoint, param->dirName.data);
		}
	}
unlock:
	sceKernelUnlockMutex(g_appmgrMountMutex, 1);
out:
	sceSyscallLeave(syscallState);
	return result;
}

/* Close an add-on content directory SceAppMgrForDriver_EA75D157 opened. */
int SceAppMgrForDriver_088670A6(const SceAppMgrDrmOpenParam *param)
{
	SceAppMgrAppInfoCache *info = NULL;
	SceUInt32 syscallState = sceSyscallEnter();
	AppmgrAddContentEntry *entries;
	int selector, count, i;
	int result;

	if (param == NULL || param->size != sizeof(*param)) {
		result = 0x80800001;
		goto out;
	}
	selector = appmgrGetAddContMountSelector(param->mountPoint);
	if (selector == 0) {
		result = 0x80800001;
		goto out;
	}
	result = sceKernelLockMutex(g_appmgrMountMutex, 1, NULL);
	if (result < 0)
		goto out;
	result = sceKernelGetRemoteKPLS(0, g_appmgrAppInfoCacheKey, (void **)&info, 0);
	if (result < 0)
		goto unlock;
	if (info == NULL) {
		result = 0x80800012;
		goto unlock;
	}
	entries = info->addContent;
	count = info->addContentCapacity;
	for (i = 0; i < count; i++) {
		if (entries[i].mountSelector != 0 && entries[i].mountSelector == selector &&
		    strncmp(entries[i].contentId, param->dirName.data, 17) == 0)
			break;
	}
	if (i >= count) {
		result = 0x80010002;
		goto unlock;
	}
	result = appmgrUnmountPfsAcidDirectory(param->mountPoint, param->dirName.data);
	if (result < 0)
		goto unlock;
	result = 0;
	entries = info->addContent;
	count = info->addContentCapacity;
	for (i = 0; i < count; i++) {
		if (entries[i].mountSelector == selector &&
		    strncmp(entries[i].contentId, param->dirName.data, 17) == 0) {
			memset(&entries[i], 0, sizeof(entries[i]));
			break;
		}
	}
unlock:
	sceKernelUnlockMutex(g_appmgrMountMutex, 1);
out:
	sceSyscallLeave(syscallState);
	return result;
}

/* Unmount the calling application's add-on content drives while content is installed. */
int sceAppMgrContentInstallPeriodStart(void)
{
	const char drives[3][16] = { "addcont0:", "addcont1:", "addcont2:" };
	SceUID pid = sceKernelGetProcessId();
	SceUInt32 syscallState = sceSyscallEnter();
	struct AppState *app;
	int i, result;

	result = sceKernelLockMutex(g_appmgrAppMutex, 1, NULL);
	if (result < 0)
		goto out;
	app = appmgrFindAppByProcessId(g_appmgrAppStates, pid);
	if (app == NULL) {
		result = 0x80802012;
		goto unlock;
	}
	if ((app->process.content.contentFlags & 0x1000000) != 0) {
		result = 0x80800041;
		goto unlock;
	}
	for (i = 2; i >= 0; i--) {
		result = sceAppMgrUmount(drives[i]);
		if (result < 0) {
			if (result != (int)0x80800002 && result != (int)0x80010013) {
				if (result == (int)0x80010010)
					result = 0x80800041;
				goto unlock;
			}
			result = 0;
		}
	}
	app->process.content.contentFlags |= 0x1000000;
unlock:
	sceKernelUnlockMutex(g_appmgrAppMutex, 1);
out:
	sceSyscallLeave(syscallState);
	return result;
}

/* End the content install period and mount the application's add-on content again. */
int sceAppMgrContentInstallPeriodStop(void)
{
	SceUID pid = sceKernelGetProcessId();
	SceUInt32 syscallState = sceSyscallEnter();
	AppmgrAddContMountParam mountParam;
	struct AppState *app;
	int result;

	result = sceKernelLockMutex(g_appmgrAppMutex, 1, NULL);
	if (result < 0)
		goto out;
	app = appmgrFindAppByProcessId(g_appmgrAppStates, pid);
	if (app == NULL) {
		result = 0x80802012;
		goto unlock;
	}
	if ((app->process.content.contentFlags & 0x1000000) == 0) {
		result = 0x80800042;
		goto unlock;
	}
	app->process.content.contentFlags &= ~0x1000000;
	memset(&mountParam, 0, sizeof(mountParam));
	memcpy(mountParam.titleId, app->process.content.titleId, 16);
	strncpy(mountParam.mountPoint, "addcont0:", 15);
	result = sceAppMgrAddContMount(pid, &mountParam);
	if (result < 0) {
		if (result == (int)0x80010002 || result == (int)0x80010013 ||
		    result == (int)0x8080000D || result == (int)0x8080000E ||
		    result == (int)0x80800006)
			result = 0;
	}
unlock:
	sceKernelUnlockMutex(g_appmgrAppMutex, 1);
out:
	sceSyscallLeave(syscallState);
	return result;
}

/* 0 while the process is in its content install period; others may ask only of themselves. */
int SceAppMgrForDriver_193784A9(SceUID pid)
{
	SceUInt32 syscallState = sceSyscallEnter();
	struct AppState *app;
	int result;

	if (pid != 0 && SceSblACMgrForDriver_8A54DF3A(0) != 0 &&
	    pid != sceKernelGetProcessId()) {
		result = 0x80800009;
		goto out;
	}
	result = sceKernelLockMutex(g_appmgrAppMutex, 1, NULL);
	if (result < 0)
		goto out;
	app = appmgrFindAppByProcessId(g_appmgrAppStates, pid);
	if (app == NULL) {
		result = 0x80802012;
	} else {
		result = 0x80800042;
		if ((app->process.content.contentFlags & 0x1000000) != 0)
			result = 0;
	}
	sceKernelUnlockMutex(g_appmgrAppMutex, 1);
out:
	sceSyscallLeave(syscallState);
	return result;
}

/* Whether the content's license may be read again (content flag 0x1000000). */
int appmgrCheckRifUpdatable(const AppmgrProcessContent *content)
{
	int result;

	if (content == NULL)
		return (int)0x80800001;
	result = sceKernelLockMutex(g_appmgrAppMutex, 1, NULL);
	if (result >= 0) {
		result = (int)0x80800042;
		if ((content->contentFlags & 0x1000000) != 0)
			result = 0;
		sceKernelUnlockMutex(g_appmgrAppMutex, 1);
	}
	return result;
}

/* the add-on content directories of a drive, as the PFS manager lists them */
typedef struct AppmgrAcidDirectoryList {
	SceInt32 count;
	char directories[16][64];
} AppmgrAcidDirectoryList;

/* Forget the add-on content directories opened on a drive. */
static inline void appmgrClearAddContEntries(SceAppMgrAppInfoCache *info, int selector)
{
	int i;

	for (i = 0; i < info->addContentCapacity; i++) {
		if (info->addContent[i].mountSelector == selector)
			memset(&info->addContent[i], 0, sizeof(info->addContent[i]));
	}
}

/* Record an add-on content directory opened on a drive, unless it is recorded already. */
static inline int appmgrAddAddContEntry(SceAppMgrAppInfoCache *info, int selector,
					const char *contentId)
{
	int count = info->addContentCapacity;
	int freeSlot = -1;
	int i;

	for (i = 0; i < count; i++) {
		if (info->addContent[i].mountSelector == 0) {
			if (freeSlot == -1)
				freeSlot = i;
		} else if (info->addContent[i].mountSelector == selector &&
			   strncmp(info->addContent[i].contentId, contentId, 17) == 0) {
			return 0;
		}
	}
	if (freeSlot == -1)
		return (int)0x80800048;
	if (freeSlot >= count)
		return (int)0x80800001;
	memset(&info->addContent[freeSlot], 0, sizeof(info->addContent[freeSlot]));
	info->addContent[freeSlot].mountSelector = selector;
	strncpy(info->addContent[freeSlot].contentId, contentId, 16);
	return 0;
}

/*
 * After an add-on content drive is unmounted: forget the directories opened on it, and keep
 * those the PFS manager still has mounted.
 */
int appmgrRemoveAddcontMountEntries(struct AppState *app, const char *driveId)
{
	SceAppMgrAppInfoCache *info = NULL;
	AppmgrAcidDirectoryList list;
	char path[64];
	int selector, i;
	int result;

	memset(&list, 0, sizeof(list));
	result = appmgrResolvePath(driveId, app->process.processId, path, sizeof(path));
	if (result < 0)
		return result;
	path[15] = '\0';
	result = ScePfsMgrForKernel_7E4B76E6(path, &list);
	if (result < 0) {
		if (result != (int)0x80010013)
			return result;
		memset(&list, 0, sizeof(list));
	}
	result = sceKernelGetRemoteKPLS(app->process.processId, g_appmgrAppInfoCacheKey,
						 (void **)&info, 0);
	if (result < 0)
		return result;
	if (info == NULL)
		return (int)0x80800012;
	selector = appmgrGetAddContMountSelector(driveId);
	if (selector == 0)
		return (int)0x80800001;
	appmgrClearAddContEntries(info, selector);
	for (i = 0; i < list.count; i++) {
		SceAppMgrDrmAddcontId contentId = { { 0 } };

		strncpy(contentId.data, list.directories[i], 16);
		result = appmgrAddAddContEntry(info, selector, contentId.data);
		if (result != 0)
			return result;
	}
	return 0;
}

/* Whether a path below photo0: stays below it: no "..", at most 56 characters. */
static inline int appmgrIsPhotoPathValid(const char *path)
{
	char c, previous;
	int i;

	c = path[0];
	if (c == '\0')
		return 1;
	for (i = 1; i < 57; i++) {
		previous = c;
		c = path[i];
		if (previous == '.' && c == '.')
			return 0;
		if (c == '\0')
			return 1;
	}
	return 0;
}

/* Register a photo0: file of the calling application (an empty path clears it), removed at its exit. */
int SceAppMgrForDriver_B53FB98E(const char *path, int kind)
{
	SceAppMgrAppInfoCache *info = NULL;
	SceUInt32 syscallState = sceSyscallEnter();
	int result;

	result = sceKernelLockMutex(g_appmgrAppMutex, 1, NULL);
	if (result < 0)
		goto out;
	if (path == NULL) {
		result = 0x80800001;
		goto unlock;
	}
	if (path[0] != '\0' && (kind != 0 || strncmp(path, "photo0:", 7) != 0 ||
				!appmgrIsPhotoPathValid(path + 7))) {
		result = 0x80800001;
		goto unlock;
	}
	result = sceKernelGetRemoteKPLS(sceKernelGetProcessId(), g_appmgrAppInfoCacheKey,
						 (void **)&info, 0);
	if (result < 0)
		goto unlock;
	if (info == NULL) {
		result = 0x80800012;
		goto unlock;
	}
	info->registeredPathKind = kind;
	memset(info->registeredPath, 0, sizeof(info->registeredPath));
	strncpy(info->registeredPath, path, sizeof(info->registeredPath) - 1);
unlock:
	sceKernelUnlockMutex(g_appmgrAppMutex, 1);
out:
	sceSyscallLeave(syscallState);
	return result;
}

/* At the application's exit: remove the photo0: file it registered. */
int appmgrRemoveTemporaryPhoto(SceUID pid)
{
	SceAppMgrAppInfoCache *info = NULL;
	char *path;
	int result;

	result = sceKernelLockMutex(g_appmgrAppMutex, 1, NULL);
	if (result < 0)
		return result;
	result = sceKernelGetRemoteKPLS(pid, g_appmgrAppInfoCacheKey, (void **)&info, 0);
	if (result < 0)
		goto unlock;
	if (info == NULL) {
		result = 0x80800012;
		goto unlock;
	}
	result = info->registeredPath[0];
	if (result == 0)
		goto unlock;
	if (info->registeredPathKind != 0 || strncmp(info->registeredPath, "photo0:", 7) != 0 ||
	    !appmgrIsPhotoPathValid(info->registeredPath + 7)) {
		result = 0x80800001;
		goto unlock;
	}
	path = appmgrAllocMountHeapMemory(292);
	if (path == NULL) {
		result = 0x80801006;
		goto unlock;
	}
	memset(path, 0, 292);
	result = appmgrResolvePath(info->registeredPath, pid, path, 292);
	if (result >= 0) {
		result = 0x80800001;
		if (info->registeredPathKind == 0) {
			result = sceIoRemove(path);
			if (result == (int)0x80010015)
				result = appmgrRemoveDirectoryTree(path);
		}
	}
	appmgrFreeMountHeapMemory(path);
unlock:
	sceKernelUnlockMutex(g_appmgrAppMutex, 1);
	return result;
}
