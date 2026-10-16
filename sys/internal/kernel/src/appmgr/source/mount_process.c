/* Process-mount lifecycle; the source-family filename is inferred. */

#include "mount_process_internal.h"
#include "process_content_internal.h"
#include "savedata_params_internal.h"
#include "param_sfo.h"
#include <sce/sysmem.h>
#include <sce/threadmgr.h>
#include <sce/suspend.h>
#include <sce/rtc.h>
#include <sce/regmgr.h>
#include <psp2kern/kernel/dipsw.h>
#include <psp2kern/kernel/iofilemgr.h>
#include <psp2kern/kernel/threadmgr.h>
#include <psp2kern/fios2.h>
#include <psp2kern/kernel/acmgr.h>
#include <psp2kern/kernel/aimgr.h>
#include <psp2kern/kernel/processmgr.h>
#include <psp2kern/pfsmgr.h>
#include <stdio.h>
#include <string.h>

extern SceUID g_appmgrMountHeap;
extern AppmgrPfsMount *g_appmgrPfsMounts;
extern int appmgrGetConsoleType(void);
extern int appmgrReleasePfsMount(SceUID pid, AppmgrPfsMount *mount, int flags);
extern int appmgrGetAccountIdentity(SceUInt64 *accountId, SceOpenPsId *openPsId,
				    SceUInt32 *currentAccount);
extern int appmgrCheckSaveDataFreeSpace(const char *path, SceUInt64 requiredBytes);
extern int appmgrCreateSaveDataSlotFile(SceUID pid, const char *path,
				      SceUInt32 slotCount);
extern int ksceIoSyncByFd2(SceUID fd, int flags);

const char appmgrProgramCategories[6][4] = {
	"gp", "gd", "gpc", "gdc", "gpd", "gdd"
};

const SceFiosOverlay g_appmgrOverlayTemplate = {0};

int appmgrAddInvalidOverlay(SceUID pid, const char *driveId,
			    SceFiosOverlayID *overlayId)
{
	SceFiosOverlay overlay;
	int permission, result;

	memcpy(&overlay, &g_appmgrOverlayTemplate, sizeof(overlay));
	overlay.order = 0x80;
	overlay.type = SCE_FIOS_OVERLAY_TYPE_OPAQUE;
	overlay.dst_len = strnlen(driveId, 16);
	strncpy(overlay.dst, driveId, 16);
	overlay.dst[15] = '\0';
	overlay.src_len = strnlen("invalid:", 16);
	strncpy(overlay.src, "invalid:", 16);
	overlay.src[15] = '\0';
	overlay.pid = pid;
	if (pid == 0)
		pid = sceKernelGetProcessId();
	permission = ksceKernelSetPermission(128);
	result = ksceFiosKernelOverlayAddForProcess(pid, &overlay, overlayId);
	ksceKernelSetPermission(permission);
	return result;
}

int appmgrAddVirtualMountOverlay(SceUID pid, AppmgrVirtualMount *mount)
{
	SceFiosOverlay overlay;
	AppmgrPfsMount *sourceMount, *pfsMount;
	int permission, result;

	memcpy(&overlay, &g_appmgrOverlayTemplate, sizeof(overlay));
	sourceMount = mount->pfsMount;
	pfsMount = sourceMount;
	if (pid == 0) {
		pid = sceKernelGetProcessId();
		pfsMount = mount->pfsMount;
	}
	if (pfsMount->mountId != 1001 && pfsMount->mountId != 1003) {
		overlay.order = 0x81;
		overlay.type = SCE_FIOS_OVERLAY_TYPE_OPAQUE;
	} else {
		overlay.order = 0x82;
		overlay.type = SCE_FIOS_OVERLAY_TYPE_TRANSLUCENT;
	}
	if (sourceMount->mountPoint.drive_id[0] != '\0') {
		unsigned short length;

		length = strnlen(pfsMount->mountPoint.drive_id, 16);
		overlay.src_len = length;
		strncpy(overlay.src, sourceMount->mountPoint.drive_id, 292);
		overlay.src_len = length + 2;
		overlay.src[length] = '0';
		overlay.src[(unsigned short)(length + 1)] = ':';
		overlay.src[(unsigned short)(length + 2)] = '\0';
	} else if (pfsMount->mountId == 600) {
		overlay.src_len = strnlen("lma0:", 292);
		strncpy(overlay.src, "lma0:", 292);
	} else {
		overlay.src_len = strnlen(pfsMount->appPath, 292);
		strncpy(overlay.src, pfsMount->appPath, 292);
	}
	overlay.src[291] = '\0';
	if (pfsMount->mountId == 1001 || pfsMount->mountId == 1003) {
		AppmgrPfsMount *previous = mount->previousPfsMount;

		if (previous->mountPoint.drive_id[0] != '\0') {
			unsigned short length;

			length = strnlen(previous->mountPoint.drive_id, 16);
			overlay.dst_len = length;
			strncpy(overlay.dst, previous->mountPoint.drive_id, 292);
			overlay.dst_len = length + 2;
			overlay.dst[length] = '0';
			overlay.dst[(unsigned short)(length + 1)] = ':';
			overlay.dst[(unsigned short)(length + 2)] = '\0';
		} else {
			overlay.dst_len = strnlen(previous->appPath, 16);
			strncpy(overlay.dst, previous->appPath, 292);
		}
	} else {
		overlay.dst_len = strnlen(mount->driveId.drive_id, 16);
		strncpy(overlay.dst, mount->driveId.drive_id, 292);
	}
	overlay.dst[291] = '\0';
	overlay.pid = pid;
	permission = ksceKernelSetPermission(128);
	result = ksceFiosKernelOverlayAddForProcess(pid, &overlay, &mount->overlayId);
	ksceKernelSetPermission(permission);
	return result;
}

int appmgrCheckSaveDataFreeSpace(const char *path, SceUInt64 requiredBytes)
{
	SceIoDevInfo info = {0};
	char device[16] = {0};
	SceUInt32 clusters;
	int result;

#define COPY_DEVICE_CHAR(index) \
	if ((device[index] = path[index]) == ':') \
		goto queryDevice
	COPY_DEVICE_CHAR(0);
	COPY_DEVICE_CHAR(1);
	COPY_DEVICE_CHAR(2);
	COPY_DEVICE_CHAR(3);
	COPY_DEVICE_CHAR(4);
	COPY_DEVICE_CHAR(5);
	COPY_DEVICE_CHAR(6);
	COPY_DEVICE_CHAR(7);
	COPY_DEVICE_CHAR(8);
	COPY_DEVICE_CHAR(9);
	COPY_DEVICE_CHAR(10);
	COPY_DEVICE_CHAR(11);
	COPY_DEVICE_CHAR(12);
	COPY_DEVICE_CHAR(13);
	COPY_DEVICE_CHAR(14);
	COPY_DEVICE_CHAR(15);
#undef COPY_DEVICE_CHAR
	result = (int)0x80800001;
	goto out;
queryDevice:
	result = ksceIoDevctl(device, 0x3001, NULL, 0, &info, sizeof(info));
	if (result < 0) {
		if (result == (int)0x80010030)
			result = 0;
	} else {
		clusters = (SceUInt64)info.free_size / info.cluster_size;
		result = clusters * (SceUInt64)info.cluster_size >= requiredBytes ?
			0 : (int)0x80800016;
	}
out:
	return result;
}

int appmgrReadKeystone(SceUID pid, const char *driveId, void *keystone)
{
	SceAllocOpt opt;
	char keyPath[64];
	char *resolvedPath;
	SceUID fd;
	int permission, result, count;

	permission = ksceKernelSetPermission(128);
	SceKernelSuspendForDriver_4DF40893(0);
	opt = (SceAllocOpt) { .size = sizeof(opt), .align = 32 };
	resolvedPath = ksceKernelAllocHeapMemoryWithOption(g_appmgrMountHeap, 292,
							 &opt);
	if (resolvedPath == NULL) {
		result = (int)0x80801006;
		goto out;
	}
	memset(resolvedPath, 0, 292);
	snprintf(keyPath, sizeof(keyPath), "%ssce_sys/%s", driveId, "keystone");
	if (pid == 0)
		pid = sceKernelGetProcessId();
	result = ksceFiosKernelOverlayResolveSync(pid, 0, keyPath, resolvedPath, 292);
	if (result == 0) {
		fd = ksceIoOpen(resolvedPath, 1, 0);
		result = fd;
		if (fd >= 0) {
			count = ksceIoRead(fd, keystone, 96);
			ksceIoClose(fd);
			result = count == 96 ? 0 : (int)0x80800005;
		}
	}
	ksceKernelFreeHeapMemory(g_appmgrMountHeap, resolvedPath);
out:
	SceKernelSuspendForDriver_2BB92967(0);
	ksceKernelSetPermission(permission);
	return result;
}

int appmgrCloneVirtualMount(SceUID pid, const AppmgrVirtualMount *source,
			    AppmgrPfsMount *const *previousPfsMount,
			    const ScePfsRndDriveId *driveId,
			    AppmgrMountHolder *holder,
			    AppmgrVirtualMount **outMount)
{
	SceAllocOpt opt = { .size = sizeof(opt), .align = 32 };
	SceSelfAuthInfo authInfo;
	AppmgrVirtualMount *mount, *tail;
	AppmgrPfsMount *pfsMount;
	int result, status, index;

	mount = ksceKernelAllocHeapMemoryWithOption(g_appmgrMountHeap,
						 sizeof(*mount), &opt);
	if (mount == NULL)
		return (int)0x80801006;
	memset(mount, 0, sizeof(*mount));
	mount->pfsMount = source->pfsMount;
	mount->driveId = *driveId;
	mount->previousPfsMount = *previousPfsMount;
	result = appmgrAddVirtualMountOverlay(pid, mount);
	if (result != 0)
		goto freeMount;
	tail = holder->firstMount;
	if (tail != NULL) {
		if (tail == mount)
			goto duplicateMount;
		while (tail->next != NULL) {
			if (tail == mount)
				goto duplicateMount;
			tail = tail->next;
		}
		tail->next = mount;
		mount->prev = tail;
		mount->next = NULL;
	} else {
		holder->firstMount = mount;
		mount->prev = NULL;
		mount->next = NULL;
	}
	pfsMount = source->pfsMount;
	if (pfsMount->mountPoint.drive_id[0] != '\0') {
		status = ksceKernelGetProcessSelfAuthInfo(pid, &authInfo);
		if (status < 0)
			goto authError;
		status = kscePfsApprove(&pfsMount->mountPoint,
				       authInfo.program_authority_id);
		if (status != (int)0x80010011 && status < 0)
			goto authError;
	}
	status = ksceKernelGetProcessSelfAuthInfo(pid, &authInfo);
	if (status < 0)
		goto authError;
	pfsMount = mount->pfsMount;
#define FIND_AUTH_SLOT(slot) \
	if (pfsMount->programAuthorityIds[slot] == 0) { \
		index = slot; \
		goto addAuthority; \
	}
	FIND_AUTH_SLOT(0);
	FIND_AUTH_SLOT(1);
	FIND_AUTH_SLOT(2);
	FIND_AUTH_SLOT(3);
	FIND_AUTH_SLOT(4);
	FIND_AUTH_SLOT(5);
	FIND_AUTH_SLOT(6);
	FIND_AUTH_SLOT(7);
	FIND_AUTH_SLOT(8);
	FIND_AUTH_SLOT(9);
	FIND_AUTH_SLOT(10);
	FIND_AUTH_SLOT(11);
	FIND_AUTH_SLOT(12);
	FIND_AUTH_SLOT(13);
	FIND_AUTH_SLOT(14);
	FIND_AUTH_SLOT(15);
#undef FIND_AUTH_SLOT
	result = (int)0x8080001e;
	goto freeMount;
addAuthority:
	pfsMount->programAuthorityIds[index] = authInfo.program_authority_id;
	if (outMount != NULL)
		*outMount = mount;
	return result;
duplicateMount:
	result = (int)0x80800003;
	goto freeMount;
authError:
	result = status;
freeMount:
	ksceKernelFreeHeapMemory(g_appmgrMountHeap, mount);
	return result;
}

int appmgrGetAddContPath(const AppmgrProcessContent *content, const char *titleId,
			char *path, SceSize size)
{
	const char *directory = content->appDirectory;

	memset(path, 0, size);
	if (content->contentFlags & 2)
		return (int)0x8080000d;
	if (titleId == NULL)
		titleId = content->addContTitleIds[0];
	if (content->attribute & 1)
		goto internalContent;
	if (strncmp(directory, "host0:", 6) == 0 ||
	    strncmp(directory, "ux0:data", 8) == 0 ||
	    strncmp(directory, "ux0:\\data", 9) == 0 ||
	    strncmp(directory, "ux0:/data", 9) == 0) {
		if (ksceSblAimgrIsCEX() || ksceSblAimgrIsDEX())
			return (int)0x8080000e;
		ksceSblACMgrIsDevelopmentMode();
		goto internalContent;
	}
	if (strncmp(directory, "gro0:", 5) == 0) {
		snprintf(path, size, "grw0:addcont/%s", titleId);
		return 0;
	}
	if (strncmp(directory, "ux0:", 4) == 0 ||
	    strncmp(directory, "pd0:", 4) == 0)
		goto internalContent;
	return (int)0x8080000e;

internalContent:
	snprintf(path, size, "ux0:addcont/%s", titleId);
	return 0;
}

int appmgrReadSaveDataMetadata(const char *path, AppmgrProcessContent *content,
			     SceUInt32 *maxSize, SceUInt64 *accountId)
{
	AppmgrSfoFile file = {0};
	void *data = NULL;
	SceSize length = 0, capacity = 0;
	int fakeOwner = 0;
	SceAllocOpt opt;
	SceOpenPsId openPsId;
	SceUInt64 currentAccountId, savedAccountId;
	AppmgrSavedataParams *params;
	char *sfoPath, *directory, *backslash, *colon;
	const char *parentDirectory;
	SceUInt32 attribute = 0;
	int permission, accountIndex, result;

	SceKernelSuspendForDriver_4DF40893(0);
	opt = (SceAllocOpt) { .size = sizeof(opt), .align = 32 };
	sfoPath = ksceKernelAllocHeapMemoryWithOption(g_appmgrMountHeap, 292, &opt);
	if (sfoPath == NULL) {
		result = (int)0x80801006;
		goto out;
	}
	memset(sfoPath, 0, 292);
	snprintf(sfoPath, 292, "%s/%s/%s", path, "sce_sys", "param.sfo");
	result = appmgrLoadSfoFile(sfoPath, &file);
	if (result < 0)
		goto freePath;
	result = appmgrGetSfoEntry(&file, "CATEGORY", &data, &length, &capacity);
	if (result < 0)
		goto freePath;
	if (length <= 2 || capacity != 4)
		goto badData;
	attribute = strncmp("sd", data, 3);
	if (attribute != 0)
		goto badData;
	result = appmgrGetSfoEntry(&file, "PARENT_DIRECTORY", &data,
				 &length, &capacity);
	if (result < 0)
		goto freePath;
	if (length == 0 || capacity != 64)
		goto badData;
	if (strncmp(path, "host0:", 6) != 0 &&
	    strncmp(path, "ux0:data", 8) != 0 &&
	    strncmp(path, "ux0:\\data", 9) != 0 &&
	    strncmp(path, "ux0:/data", 9) != 0) {
		directory = strrchr(path, '/');
		backslash = strrchr(path, '\\');
		colon = strrchr(path, ':');
		if (directory <= backslash) {
			if (backslash > colon) {
				directory = backslash;
			} else {
				if (colon == NULL)
					goto readParams;
				directory = colon + 1;
			}
		}
		if (directory != NULL) {
			parentDirectory = data;
			if (*directory == '/')
				++directory;
			if (*(const char *)data == '/')
				++parentDirectory;
			if (strncmp(directory, parentDirectory, 64) != 0)
				goto badData;
		}
	}
readParams:
	result = appmgrGetSfoEntry(&file, "PARAMS", &data, &length, &capacity);
	if (result < 0)
		goto freePath;
	if (length < sizeof(*params) || capacity != sizeof(*params))
		goto badData;
	params = data;
	if (params->cexUseCount != 0)
		content->savedataEnvironmentFlags |= 1;
	if (params->dexUseCount != 0)
		content->savedataEnvironmentFlags |= 2;
	if (params->otherUseCount != 0)
		content->savedataEnvironmentFlags |= 4;
	if (maxSize != NULL)
		*maxSize = params->maxSize;
	result = appmgrGetSfoEntry(&file, "ACCOUNT_ID", &data, &length, &capacity);
	if (result < 0)
		goto freePath;
	if (length < sizeof(savedAccountId) || capacity != sizeof(savedAccountId))
		goto badData;
	savedAccountId = *(const SceUInt64 *)data;
	if (accountId != NULL)
		*accountId = savedAccountId;
	permission = ksceKernelSetPermission(64);
	result = sceRegMgrGetKeyBin("/CONFIG/NP", "account_id",
				 &currentAccountId, sizeof(currentAccountId));
	if (result < 0)
		goto restorePermission;
	result = ksceSblAimgrGetOpenPsId(&openPsId);
	if (result < 0)
		goto restorePermission;
	if (sceRegMgrGetKeyInt("/CONFIG/NP2", "current_account2", &accountIndex) < 0 ||
	    (unsigned int)accountIndex > 63)
		accountIndex = 0;
	ksceKernelSetPermission(permission);
	if (savedAccountId != 0) {
		if (savedAccountId == currentAccountId)
			goto readFakeOwner;
	} else {
		content->savedataOwnershipFlags |= 2;
		if (timingsafe_memcmp(&params->openPsId, &openPsId, sizeof(openPsId)) == 0 &&
		    params->accountIndex == (unsigned int)accountIndex)
			goto readFakeOwner;
	}
	content->savedataOwnershipFlags |= 1;
readFakeOwner:
	if (sceRegMgrGetKeyInt("/CONFIG/GAME", "fake_savedata_owner", &fakeOwner) < 0)
		fakeOwner = 0;
	else if (fakeOwner == 1) {
		content->savedataOwnershipFlags &= ~1U;
		content->savedataEnvironmentFlags |= 8;
	}
	if (appmgrGetSfoEntry(&file, "ATTRIBUTE", &data, &length, &capacity) >= 0 &&
	    length > 3 && capacity == 4)
		attribute = *(const SceUInt32 *)data;
	content->savedataAttribute = attribute;
	if (appmgrGetSfoEntry(&file, "TITLE", &data, &length, &capacity) >= 0 &&
	    (length == 0 || capacity != 128))
		goto badData;
	if (appmgrGetSfoEntry(&file, "DETAIL", &data, &length, &capacity) >= 0 &&
	    (length == 0 || capacity != 1024))
		goto badData;
	if (appmgrGetSfoEntry(&file, "SAVEDATA_VER", &data, &length, &capacity) < 0 ||
	    length <= 5 || capacity != 8)
		content->savedataVersion = 0;
	result = 0;
	content->savedataVersion = appmgrParseSfoVersion(data);
	goto freePath;
restorePermission:
	ksceKernelSetPermission(permission);
	goto freePath;
badData:
	result = (int)0x80800034;
freePath:
	ksceKernelFreeHeapMemory(g_appmgrMountHeap, sfoPath);
out:
	appmgrReleaseSfoFile(&file);
	SceKernelSuspendForDriver_2BB92967(0);
	return result;
}

int appmgrUpdateSaveDataParams(const char *path, const char *titleId,
			     SceUInt32 maxSize, SceUInt32 *flag)
{
	AppmgrSfoFile file = {0};
	void *data = NULL;
	SceSize length = 0, capacity = 0;
	SceAllocOpt opt;
	SceOpenPsId openPsId;
	SceUInt64 currentAccountId;
	AppmgrSavedataParams *params;
	char *sfoPath;
	SceUID fd;
	int permission, accountIndex, modified, count, result;

	SceKernelSuspendForDriver_4DF40893(0);
	opt = (SceAllocOpt) { .size = sizeof(opt), .align = 32 };
	sfoPath = ksceKernelAllocHeapMemoryWithOption(g_appmgrMountHeap, 292, &opt);
	if (sfoPath == NULL) {
		result = (int)0x80801006;
		goto out;
	}
	memset(sfoPath, 0, 292);
	snprintf(sfoPath, 292, "%s/%s/%s", path, "sce_sys", "param.sfo");
	result = appmgrLoadSfoFile(sfoPath, &file);
	if (result < 0)
		goto freePath;
	ksceKernelFreeHeapMemory(g_appmgrMountHeap, sfoPath);
	opt = (SceAllocOpt) { .size = sizeof(opt), .align = 32 };
	params = ksceKernelAllocHeapMemoryWithOption(g_appmgrMountHeap,
						   sizeof(*params), &opt);
	if (params == NULL) {
		result = (int)0x80801006;
		goto out;
	}
	result = appmgrGetSfoEntry(&file, "PARAMS", &data, &length, &capacity);
	if (result < 0)
		goto freeParams;
	if (length < sizeof(*params) || capacity != sizeof(*params))
		goto badData;
	memcpy(params, data, sizeof(*params));
	result = appmgrGetSfoEntry(&file, "ACCOUNT_ID", &data, &length, &capacity);
	if (result < 0)
		goto freeParams;
	if (length < sizeof(currentAccountId) || capacity != sizeof(currentAccountId))
		goto badData;
	if (*(const SceUInt64 *)data != 0)
		goto notModified;
	permission = ksceKernelSetPermission(64);
	result = sceRegMgrGetKeyBin("/CONFIG/NP", "account_id",
				 &currentAccountId, sizeof(currentAccountId));
	if (result < 0)
		goto restorePermission;
	result = ksceSblAimgrGetOpenPsId(&openPsId);
	if (result < 0)
		goto restorePermission;
	result = sceRegMgrGetKeyInt("/CONFIG/NP2", "current_account2", &accountIndex);
	if (result < 0) {
		accountIndex = 0;
		result = 0;
	} else if ((unsigned int)accountIndex > 63) {
		accountIndex = 0;
	}
	ksceKernelSetPermission(permission);
	if (currentAccountId == 0 ||
	    timingsafe_memcmp(&params->openPsId, &openPsId, sizeof(openPsId)) != 0 ||
	    params->accountIndex != (unsigned int)accountIndex)
		goto notModified;
	result = appmgrSetSfoAccountId(&file, currentAccountId);
	if (result < 0)
		goto freeParams;
	modified = 1;
	goto updateParams;
notModified:
	modified = 0;
updateParams:
	if (maxSize != 0 && maxSize != params->maxSize) {
		modified = 1;
		params->maxSize = maxSize;
	}
	if (titleId != NULL) {
		if (ksceSblAimgrIsCEX())
			++params->cexUseCount;
		else if (ksceSblAimgrIsDEX())
			++params->dexUseCount;
		else {
			ksceSblACMgrIsDevelopmentMode();
			++params->otherUseCount;
		}
		params->lastTitleId[0] = 0;
		params->lastTitleId[1] = 0;
		params->lastTitleId[2] = 0;
		params->lastTitleId[3] = 0;
		params->lastTitleId[4] = 0;
		params->lastTitleId[5] = 0;
		params->lastTitleId[6] = 0;
		params->lastTitleId[7] = 0;
		params->lastTitleId[8] = 0;
		params->lastTitleId[9] = 0;
		params->lastTitleId[10] = 0;
		params->lastTitleId[11] = 0;
		strncpy(params->lastTitleId, titleId, 11);
		if (flag != NULL)
			*flag = !(params->flags & 1);
		result = appmgrSetSfoParams(&file, params);
		if (result < 0)
			goto freeParams;
		ksceKernelFreeHeapMemory(g_appmgrMountHeap, params);
		goto writeFile;
	}
	ksceKernelFreeHeapMemory(g_appmgrMountHeap, params);
	if (modified == 0)
		goto out;
writeFile:
	opt = (SceAllocOpt) { .size = sizeof(opt), .align = 32 };
	sfoPath = ksceKernelAllocHeapMemoryWithOption(g_appmgrMountHeap, 292, &opt);
	if (sfoPath == NULL) {
		result = (int)0x80801006;
		goto out;
	}
	memset(sfoPath, 0, 292);
	snprintf(sfoPath, 292, "%s/%s/%s", path, "sce_sys", "param.sfo");
	fd = ksceIoOpen(sfoPath, 1027, 1048582);
	result = fd;
	if (fd >= 0) {
		count = ksceIoWrite(fd, file.buffer, file.size);
		if (count >= 0 && (SceSize)count == file.size) {
			result = ksceIoClose(fd);
			if (result >= 0) {
				result = ksceIoSync(sfoPath, 0);
				if (result < 0)
					result = 0;
			}
		} else {
			result = count;
			ksceIoClose(fd);
		}
	}
	goto freePath;
restorePermission:
	ksceKernelSetPermission(permission);
	goto freeParams;
badData:
	result = (int)0x80800034;
freeParams:
	ksceKernelFreeHeapMemory(g_appmgrMountHeap, params);
	goto out;
freePath:
	ksceKernelFreeHeapMemory(g_appmgrMountHeap, sfoPath);
out:
	appmgrReleaseSfoFile(&file);
	SceKernelSuspendForDriver_2BB92967(0);
	return result;
}

#if FW_VERSION >= 0x04000000
extern int SceQafMgrForDriver_41E04800(void);

int appmgrValidateSaveDataMount(SceUID pid, const char *path, int skipVerification,
			       AppmgrProcessContent *content, SceUInt32 *maxSize)
{
	SceUInt32 saveDataFlag = 0;
	ScePfsRndDriveId driveId = { "savedata0:" };
	unsigned char keystone[96] = {0};
	const char *titleId;
	SceUInt32 requestedSize;
	int result;

	if (skipVerification) {
		titleId = NULL;
		requestedSize = 0;
		goto updateParams;
	}
	if (strncmp(path, "host0:", 6) == 0 ||
	    strncmp(path, "ux0:data", 8) == 0 ||
	    strncmp(path, "ux0:\\data", 9) == 0 ||
	    strncmp(path, "ux0:/data", 9) == 0)
		goto contentParams;
	if (!ksceSblAimgrIsCEX()) {
		if (ksceSblAimgrIsDEX()) {
			if (ksceSblUtMgrHasStoreFlag())
				goto contentParams;
		} else {
			ksceSblACMgrIsDevelopmentMode();
		}
	}
	result = appmgrReadKeystone(pid, driveId.drive_id, keystone);
	if (result >= 0) {
		if ((SceQafMgrForDriver_41E04800() != 1 ||
		     ksceKernelCheckDipsw(218) == 0) &&
		    timingsafe_memcmp(keystone, content->keystone, sizeof(keystone)) != 0)
			return (int)0x80800006;
	} else if (result != (int)0x80010002 && result != (int)0x80010013) {
		return result;
	}
contentParams:
	titleId = content->titleId;
	requestedSize = content->savedataMaxSize;
updateParams:
	result = appmgrUpdateSaveDataParams(path, titleId, requestedSize, &saveDataFlag);
	if (result >= 0) {
		result = appmgrReadSaveDataMetadata(path, content, maxSize, NULL);
		return result < 0 ? result : 0;
	}
	return result;
}
#endif

int appmgrSelectSafeMemoryMode(const AppmgrProcessContent *content)
{
	int mode = content->safeMemoryMode;
	unsigned int size;
	int flags;

	if (mode != 0) {
		size = mode & 0x7fffffff;
		flags = mode >= 0;
		switch (size) {
		case 128:
			return flags | 4;
		case 256:
			return flags | 8;
		case 512:
			return flags | 16;
		case 1024:
			return flags | 32;
		default:
			return flags | 2;
		}
	}
	if (strncmp(content->category, appmgrProgramCategories[1], 3) != 0 &&
	    strncmp(content->category, appmgrProgramCategories[3], 4) != 0)
		return strncmp(content->category, appmgrProgramCategories[5], 4) != 0 ? 2 : 3;
	return 3;
}

int appmgrCloneTemporaryMounts(SceUID pid, const char *titleId,
			       const AppmgrMountHolder *source,
			       AppmgrMountHolder *destination,
			       ScePfsRndDriveId *outDriveId)
{
	AppmgrVirtualMount *firstClone = NULL, *mount, *next;
	ScePfsRndDriveId appDrive = { .drive_id = "app0:" };
	ScePfsRndDriveId temporaryDrive;
	SceSelfAuthInfo authInfo;
	AppmgrPfsMount *pfsMount, *cursor, *previous;
	SceUInt64 authorityId, entry;
	int result, status, permission, removed, references, index;

	snprintf(temporaryDrive.drive_id, sizeof(temporaryDrive), "tp%s:", titleId);
	mount = source->firstMount;
	temporaryDrive.drive_id[12] = '\0';
	temporaryDrive.drive_id[11] = ':';
	while (mount != NULL) {
		if (mount->pfsMount != NULL && mount->pfsMount->mountId == 1000 &&
		    strncmp(mount->driveId.drive_id, appDrive.drive_id, 16) == 0)
			break;
		mount = mount->next;
	}
	if (mount == NULL) {
		result = (int)0x80800002;
		goto cleanup;
	}
	result = appmgrCloneVirtualMount(pid, mount, &mount->previousPfsMount,
					&temporaryDrive, destination, &firstClone);
	if (result != 0)
		goto cleanup;
	mount = source->firstMount;
	while (mount != NULL) {
		if (mount->pfsMount->mountId == 1001 &&
		    strncmp(mount->pfsMount->titleId, titleId, 10) == 0) {
			result = appmgrCloneVirtualMount(pid, mount,
						&mount->previousPfsMount,
						&temporaryDrive, destination, NULL);
			if (result != 0)
				goto cleanup;
		}
		mount = mount->next;
	}
	*outDriveId = temporaryDrive;
	return result;

cleanup:
	mount = firstClone;
	if (mount == NULL)
		return result;
	if (mount->prev != NULL)
		mount->prev->next = NULL;
	while (mount != NULL) {
		next = mount->next;
		pfsMount = mount->pfsMount;
		/* Reuse appmgrReleasePfsMount at Sony's observed inline site. */
		status = ksceKernelGetProcessSelfAuthInfo(pid, &authInfo);
		if (status < 0)
			goto freeVirtualMount;
		authorityId = authInfo.program_authority_id;
		removed = 0;
		references = 0;
		for (index = 0; index != 16; ++index) {
			entry = pfsMount->programAuthorityIds[index];
			if (!removed && entry == authorityId) {
				pfsMount->programAuthorityIds[index] = 0;
				removed = 1;
			} else if (entry != 0) {
				++references;
			}
		}
		if (references != 0) {
			if (authorityId == pfsMount->programAuthorityIds[0] ||
			    authorityId == pfsMount->programAuthorityIds[1] ||
			    authorityId == pfsMount->programAuthorityIds[2] ||
			    authorityId == pfsMount->programAuthorityIds[3] ||
			    authorityId == pfsMount->programAuthorityIds[4] ||
			    authorityId == pfsMount->programAuthorityIds[5] ||
			    authorityId == pfsMount->programAuthorityIds[6] ||
			    authorityId == pfsMount->programAuthorityIds[7] ||
			    authorityId == pfsMount->programAuthorityIds[8] ||
			    authorityId == pfsMount->programAuthorityIds[9] ||
			    authorityId == pfsMount->programAuthorityIds[10] ||
			    authorityId == pfsMount->programAuthorityIds[11] ||
			    authorityId == pfsMount->programAuthorityIds[12] ||
			    authorityId == pfsMount->programAuthorityIds[13] ||
			    authorityId == pfsMount->programAuthorityIds[14] ||
			    authorityId == pfsMount->programAuthorityIds[15] ||
			    pfsMount->mountPoint.drive_id[0] == 0)
				goto freeVirtualMount;
			status = ksceKernelGetProcessSelfAuthInfo(pid, &authInfo);
			if (status < 0)
				goto freeVirtualMount;
			if (authInfo.program_authority_id == 0x2800000000000001ULL ||
			    authInfo.program_authority_id == 0x2800000000000010ULL ||
			    authInfo.program_authority_id == 0x280000000000002dULL ||
			    (pfsMount->mountId == 1005 &&
			     authInfo.program_authority_id == 0x2800000000000039ULL))
				goto freeVirtualMount;
			kscePfsDisapprove(&pfsMount->mountPoint,
					  authInfo.program_authority_id);
			goto freeVirtualMount;
		}
		if (pfsMount->mountId == 600) {
			permission = ksceKernelSetPermission(128);
			status = ksceIoUmount(0x10000, 1, 0, 0);
			ksceKernelSetPermission(permission);
			if (status < 0)
				goto restoreAuthority;
		} else if (pfsMount->mountPoint.drive_id[0] != 0) {
			permission = ksceKernelSetPermission(128);
			status = kscePfsUnmount(&pfsMount->mountPoint);
			ksceKernelSetPermission(permission);
			if (status != (int)0x80010013 && status != 0)
				goto restoreAuthority;
		}
		cursor = g_appmgrPfsMounts;
		if (cursor == NULL)
			goto freeVirtualMount;
		if (cursor == pfsMount)
			goto removeHead;
		for (;;) {
			cursor = cursor->next;
			if (cursor == NULL)
				goto freeVirtualMount;
			if (cursor == pfsMount) {
				if (g_appmgrPfsMounts == pfsMount)
					goto removeHead;
				previous = pfsMount->prev;
				previous->next = pfsMount->next;
				if (pfsMount->next != NULL)
					pfsMount->next->prev = previous;
				break;
			}
		}
		goto freePfsMount;
removeHead:
		g_appmgrPfsMounts = g_appmgrPfsMounts->next;
		if (g_appmgrPfsMounts != NULL)
			g_appmgrPfsMounts->prev = NULL;
freePfsMount:
		ksceKernelFreeHeapMemory(g_appmgrMountHeap, pfsMount);
		goto freeVirtualMount;
restoreAuthority:
		if (!removed)
			goto freeVirtualMount;
#define RESTORE_AUTH_SLOT(slot) \
		if (pfsMount->programAuthorityIds[slot] == 0) { \
			index = slot; \
			goto restoreAuthorityValue; \
		}
		RESTORE_AUTH_SLOT(0);
		RESTORE_AUTH_SLOT(1);
		RESTORE_AUTH_SLOT(2);
		RESTORE_AUTH_SLOT(3);
		RESTORE_AUTH_SLOT(4);
		RESTORE_AUTH_SLOT(5);
		RESTORE_AUTH_SLOT(6);
		RESTORE_AUTH_SLOT(7);
		RESTORE_AUTH_SLOT(8);
		RESTORE_AUTH_SLOT(9);
		RESTORE_AUTH_SLOT(10);
		RESTORE_AUTH_SLOT(11);
		RESTORE_AUTH_SLOT(12);
		RESTORE_AUTH_SLOT(13);
		RESTORE_AUTH_SLOT(14);
		RESTORE_AUTH_SLOT(15);
#undef RESTORE_AUTH_SLOT
		goto freeVirtualMount;
restoreAuthorityValue:
		pfsMount->programAuthorityIds[index] = authorityId;
freeVirtualMount:
		ksceKernelFreeHeapMemory(g_appmgrMountHeap, mount);
		mount = next;
	}
	return result;
}

int appmgrUnmountDrive(SceUID pid, struct AppmgrMountHolder *holder,
		      const char *driveId, int force)
{
	AppmgrVirtualMount *mount, *cursor, *previous;
	SceFiosOverlayID overlayId;
	SceUID targetPid;
	int result, permission, removed = 0;

	for (;;) {
		mount = holder->firstMount;
		if (mount == NULL)
			break;
		while (strncmp(mount->driveId.drive_id, driveId, 16) != 0) {
			mount = mount->next;
			if (mount == NULL)
				goto finished;
		}
		result = appmgrReleasePfsMount(pid, mount->pfsMount, force);
		if (result != 0)
			return result;
		overlayId = mount->overlayId;
		targetPid = pid;
		if (pid == 0)
			targetPid = sceKernelGetProcessId();
		permission = ksceKernelSetPermission(128);
		result = ksceFiosKernelOverlayRemoveForProcess(targetPid, overlayId);
		ksceKernelSetPermission(permission);
		if (result != 0)
			return result;
		cursor = holder->firstMount;
		if (cursor == NULL)
			return (int)0x80800002;
		if (cursor == mount)
			goto removeHead;
		for (;;) {
			cursor = cursor->next;
			if (cursor == NULL)
				return (int)0x80800002;
			if (cursor == mount) {
				if (holder->firstMount == mount)
					goto removeHead;
				previous = mount->prev;
				previous->next = mount->next;
				if (mount->next != NULL)
					mount->next->prev = previous;
				break;
			}
		}
		goto freeMount;
removeHead:
		holder->firstMount = holder->firstMount->next;
		if (holder->firstMount != NULL)
			holder->firstMount->prev = NULL;
freeMount:
		ksceKernelFreeHeapMemory(g_appmgrMountHeap, mount);
		removed = 1;
	}
finished:
	return removed ? 0 : (int)0x80800002;
}

int appmgrCleanupProcessMount(struct AppmgrMountHolder *holder,
			      AppmgrProcessMountCleanup *cleanup)
{
	int result = (signed char)cleanup->driveId[0];

	if (result != 0)
		result = appmgrUnmountDrive(sceKernelGetProcessId(), holder,
					    cleanup->driveId, 0);
	if (cleanup->resolvedPath)
		ksceKernelFreeHeapMemory(g_appmgrMountHeap, cleanup->resolvedPath);
	if (cleanup->mapFile)
		ksceKernelFreeHeapMemory(g_appmgrMountHeap, cleanup->mapFile);
	memset(cleanup, 0, sizeof(*cleanup));
	return result;
}

int appmgrCreateSaveDataMetadata(SceUID pid, const char *path,
				const char *parentDirectory, unsigned int mountId,
				const void *sealedKey, const void *keystone,
				const char *titleId)
{
	AppmgrSfoFile file = {0};
	AppmgrSavedataSfoParam input;
	AppmgrSavedataParams *params;
	SceAllocOpt opt;
	SceIoStat stat;
	SceOpenPsId openPsId;
	SceUInt64 accountId;
	SceUInt32 accountIndex, attribute;
	char emptyTitle, emptyDetail;
	char *metadataPath;
	SceUID fd;
	int permission, result, count, mode, consoleType;
#if FW_VERSION < 0x04000000
	int accountPermission;
#endif

	memset(&stat, 0, sizeof(stat));
	stat.st_mode = 4;
	permission = ksceKernelSetPermission(128);
	SceKernelSuspendForDriver_4DF40893(0);
	opt = (SceAllocOpt) { .size = sizeof(opt), .align = 32 };
	metadataPath = ksceKernelAllocHeapMemoryWithOption(g_appmgrMountHeap, 292, &opt);
	if (metadataPath == NULL) {
		result = (int)0x80801006;
		goto out;
	}
	result = appmgrCheckSaveDataFreeSpace(path, 0x200000);
	if (result < 0)
		goto freePath;
	snprintf(metadataPath, 292, "%s/sce_sys", path);
	if (strncmp(metadataPath, "host0:", 6) != 0 &&
	    strncmp(metadataPath, "ux0:data", 8) != 0 &&
	    strncmp(metadataPath, "ux0:\\data", 9) != 0 &&
	    strncmp(metadataPath, "ux0:/data", 9) != 0) {
		result = ksceIoMkdir(metadataPath, 6);
		if (result != (int)0x80010011 && result != 0)
			goto freePath;
		if (mountId == 1006) {
			mode = 1048582;
			goto buildSfo;
		}
	} else {
		if (mountId == 1006) {
			result = ksceIoMkdir(metadataPath, 390);
			if (result != 0 && result != (int)0x80010011)
				goto freePath;
			mode = 1048966;
			goto buildSfo;
		}
		result = ksceIoMkdir(metadataPath, 6);
		if (result != 0 && result != (int)0x80010011)
			goto freePath;
	}
	if (sealedKey != NULL) {
		snprintf(metadataPath, 292, "%s/sce_sys/sealedkey", path);
		fd = ksceIoOpen(metadataPath, 1539, 1048582);
		result = fd;
		if (fd < 0)
			goto freePath;
		count = ksceIoWrite(fd, sealedKey, 80);
		if (count != 80)
			goto closeError;
		count = ksceIoChstatByFd(fd, &stat, 1);
		if (count < 0)
			goto closeError;
		count = ksceIoSyncByFd2(fd, 0);
		if (count < 0)
			goto closeError;
		result = ksceIoClose(fd);
		if (result < 0)
			goto freePath;
	}
	if (mountId == 1005 || mountId - 500 <= 1) {
		if (keystone == NULL) {
			result = (int)0x80800001;
			goto freePath;
		}
		snprintf(metadataPath, 292, "%s/sce_sys/keystone", path);
		fd = ksceIoOpen(metadataPath, 1539, 6);
		result = fd;
		if (fd < 0)
			goto freePath;
		count = ksceIoWrite(fd, keystone, 96);
		if (count != 96)
			goto closeError;
		count = ksceIoChstatByFd(fd, &stat, 1);
		if (count < 0)
			goto closeError;
		count = ksceIoSyncByFd2(fd, 0);
		if (count < 0)
			goto closeError;
		result = ksceIoClose(fd);
		if (result < 0)
			goto freePath;
	}
	mode = 1048582;
buildSfo:
	if (mountId - 1005 > 1)
		goto freePath;
	if (strncmp(path, "ur0:", 4) == 0)
		goto freePath;
	emptyTitle = 0;
	emptyDetail = 0;
	attribute = 0;
	if (parentDirectory == NULL) {
		result = (int)0x8080000a;
		goto freePath;
	}
#if FW_VERSION >= 0x04000000
	result = appmgrGetAccountIdentity(&accountId, &openPsId, &accountIndex);
	if (result < 0)
		goto freePath;
#else
	accountPermission = ksceKernelSetPermission(64);
	result = sceRegMgrGetKeyBin("/CONFIG/NP", "account_id", &accountId, 8);
	if (result >= 0)
		result = ksceSblAimgrGetOpenPsId(&openPsId);
	if (result < 0) {
		ksceKernelSetPermission(accountPermission);
		goto freePath;
	}
	if (sceRegMgrGetKeyInt("/CONFIG/NP2", "current_account2",
			       (int *)&accountIndex) < 0 || accountIndex > 63)
		accountIndex = 0;
	ksceKernelSetPermission(accountPermission);
#endif
	input = (AppmgrSavedataSfoParam) {0};
	opt = (SceAllocOpt) { .size = sizeof(opt), .align = 32 };
	params = ksceKernelAllocHeapMemoryWithOption(g_appmgrMountHeap,
						  sizeof(*params), &opt);
	if (params == NULL) {
		result = (int)0x80801006;
		goto freePath;
	}
	memset(params, 0, sizeof(*params));
	consoleType = appmgrGetConsoleType();
	if (consoleType == 0)
		params->consoleType = 1;
	else if (consoleType == 1)
		params->consoleType = 2;
	else
		params->consoleType = 3;
	strncpy(params->createdTitleId, titleId != NULL ? titleId : "", 11);
	params->creationMarker = 1;
	params->openPsId = openPsId;
	params->accountIndex = accountIndex;
	params->flags = 1;
	input.parentDirectory = parentDirectory;
	input.title = &emptyTitle;
	input.detail = &emptyDetail;
	input.attribute = &attribute;
	input.accountId = &accountId;
	input.params = params;
	result = appmgrBuildSavedataSfo(&file, &input);
	if (result >= 0) {
		snprintf(metadataPath, 292, "%s/sce_sys/param.sfo", path);
		fd = ksceIoOpen(metadataPath, 1539, mode);
		result = fd;
		if (fd >= 0) {
			count = ksceIoWrite(fd, file.buffer, file.size);
			if (count >= 0 && (SceSize)count == file.size) {
				count = ksceIoSyncByFd2(fd, 0);
				if (count >= 0) {
					result = ksceIoClose(fd);
					if (result >= 0) {
						ksceKernelFreeHeapMemory(g_appmgrMountHeap, params);
						result = appmgrCreateSaveDataSlotFile(pid, path, 256);
						goto freePath;
					}
				} else {
					result = count;
					ksceIoClose(fd);
				}
			} else {
				result = count;
				ksceIoClose(fd);
			}
		}
	}
	ksceKernelFreeHeapMemory(g_appmgrMountHeap, params);
	goto freePath;
closeError:
	result = count;
	ksceIoClose(fd);
freePath:
	ksceKernelFreeHeapMemory(g_appmgrMountHeap, metadataPath);
out:
	appmgrReleaseSfoFile(&file);
	SceKernelSuspendForDriver_2BB92967(0);
	ksceKernelSetPermission(permission);
	return result;
}
