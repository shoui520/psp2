/* Application content mounts. */

#include <fw.h>
#include <scetypes.h>
#include <sysmem/debug.h>
#include <kernel/types.h>
#include <authmgr/sblauthmgr.h>
#include <sysmem/sysmem.h>
#include <ss_mgr/ss_mgr.h>
#include <processmgr/processmgr.h>
#include <sysmem/sblaimgr.h>
#include <npdrm/npdrm.h>
#include <fios2/fios2.h>
#include <acmgr/acmgr.h>
#include <appmgr/appmgr_launch.h>
#include <appmgr/appmgr_rdrfp.h>
#include <regmgr/regmgr.h>
#include <post_ss_mgr/sblpostssmgr.h>
#include <npdrm/npdrm_package.h>
#include <rtc/rtc.h>
#include <sysmem/suspend.h>
#include <intrmgr/syscall.h>
#include <threadmgr/threadmgr.h>
#include <stdio.h>
#include <string.h>
#include "mount.h"
#include "debug_setting.h"
#include "mapfile_internal.h"
#include <stddef.h>
#include "mount_holder_internal.h"
#include "process_content_internal.h"
#include "savedata_params_internal.h"
#include "param_sfo.h"
#include <sysmem/dipsw.h>
#include <sysmem/qafmgr.h>
#include <sysmem/sysclib.h>
#include <iofilemgr/iofilemgr.h>
#include <post_ss_mgr/sblpostssmgr.h>
#include <pfsmgr/pfsmgr.h>
#include <syscon/syscon.h>
#include <sysmem/pmmgr.h>
#include "app_state_internal.h"
#include "fat_format_internal.h"
#include "appmgr_device.h"
#include "appmgr_event.h"
#include "appmgr_savedata_slot.h"
#include "addcont_internal.h"

/* SceSblPostSsMgr (ut_mgr.c): declared here until its exporter's header declares it */

int appmgrUnmountDrive(SceUID pid, struct AppmgrMountHolder *holder,
		      const char *driveId, int force);

extern const SceFiosOverlay g_appmgrOverlayTemplate;
int appmgrAddInvalidOverlay(SceUID pid, const char *driveId,
			    SceFiosOverlayID *overlayId);
int appmgrAddVirtualMountOverlay(SceUID pid, AppmgrVirtualMount *mount);
int appmgrCloneVirtualMount(SceUID pid, const AppmgrVirtualMount *source,
			    AppmgrPfsMount *const *previousPfsMount,
			    const ScePfsRndDriveId *driveId,
			    AppmgrMountHolder *holder,
			    AppmgrVirtualMount **outMount);
int appmgrCloneTemporaryMounts(SceUID pid, const char *titleId,
			       const AppmgrMountHolder *source,
			       AppmgrMountHolder *destination,
			       ScePfsRndDriveId *outDriveId);

SceUID g_appmgrCoreHeap;
SceUID g_appmgrMountHeap = -1;
SceUID g_appmgrMountMutex = -1;
AppmgrPfsMount *g_appmgrPfsMounts;
char g_appmgrAccountDirectory[3];
char g_appmgrDirectoryPath[292];
char g_appmgrLicenseDirectory[32];
unsigned char encodedLicenseDirectory[16] = {
	(unsigned char)~'l', (unsigned char)~'i', (unsigned char)~'c',
	(unsigned char)~'e', (unsigned char)~'n', (unsigned char)~'s',
	(unsigned char)~'e', (unsigned char)~'/', (unsigned char)~'a',
	(unsigned char)~'p', (unsigned char)~'p', 0
};

/* Sony stores these path components with every nonzero byte complemented. */
#define SAVEDATA_COMPONENT { \
	(unsigned char)~'s', (unsigned char)~'a', (unsigned char)~'v', \
	(unsigned char)~'e', (unsigned char)~'d', (unsigned char)~'a', \
	(unsigned char)~'t', (unsigned char)~'a', 0 \
}
#define USER_ROOT_COMPONENT { \
	(unsigned char)~'u', (unsigned char)~'r', (unsigned char)~'0', \
	(unsigned char)~':', (unsigned char)~'u', (unsigned char)~'s', \
	(unsigned char)~'e', (unsigned char)~'r', 0 \
}

#if FW_DEBUG
static const unsigned char encodedSavedataComponent[16] = SAVEDATA_COMPONENT;
static const unsigned char encodedUserRoot[16] = USER_ROOT_COMPONENT;
#else
static const unsigned char encodedUserRoot[16] = USER_ROOT_COMPONENT;
static const unsigned char encodedSavedataComponent[16] = SAVEDATA_COMPONENT;
#endif

#undef USER_ROOT_COMPONENT
#undef SAVEDATA_COMPONENT

/* The caller's program class, for user programs. */
int sceAppMgrIsGameProgram(void)
{
	SceUInt32 syscall = sceSyscallEnter();
	int result = sceSblACMgrIsGameProgram(0);

	sceSyscallLeave(syscall);
	return result;
}

int sceAppMgrIsNonGameProgram(void)
{
	SceUInt32 syscall = sceSyscallEnter();
	int result = SceSblACMgrForDriver_6C5AB07F(0);

	sceSyscallLeave(syscall);
	return result;
}

int sceAppMgrIsPsNowClient(void)
{
	SceUInt32 syscall = sceSyscallEnter();
	int result = SceSblACMgrForDriver_0924896F(0);

	sceSyscallLeave(syscall);
	return result;
}

int sceAppMgrIsDevelopmentMode(void)
{
	SceUInt32 syscall = sceSyscallEnter();
	int result = SceSblACMgrForDriver_E87D1777();

	sceSyscallLeave(syscall);
	return result;
}

/*
 * Of the media drives photo0 (100), music0 (105) and video0 (109) a process
 * mounts at most two: a drive whose content flag is set stays mountable.
 */
int appmgrIsMediaDriveMountable(int mountId, SceUInt32 contentFlags)
{
	if (mountId == 105) {
		if (contentFlags & 0x04000000)
			return 1;
		return (contentFlags & 0x0a000000) != 0x0a000000;
	}
	if (mountId == 109) {
		if (contentFlags & 0x08000000)
			return 1;
		return (contentFlags & 0x06000000) != 0x06000000;
	}
	if (mountId == 100 && (contentFlags & 0x02000000) == 0)
		return (contentFlags & 0x0c000000) != 0x0c000000;
	return 1;
}

/* The title ID of an additional content's param.sfo (category "ac"). */
int appmgrGetAddcontTitleId(const char *sfoPath, char *titleId)
{
	AppmgrSfoFile file = {0};
	void *data = NULL;
	SceSize length = 0, capacity = 0;
	int result;

	if (titleId == NULL) {
		result = (int)0x80800001;
		goto release;
	}
	memset(titleId, 0, 16);
	result = appmgrLoadSfoFile(sfoPath, &file);
	if (result < 0)
		goto release;
	result = appmgrGetSfoEntry(&file, "CATEGORY", &data, &length, &capacity);
	if (result < 0)
		goto release;
	if (length <= 2 || capacity != 4 || strncmp("ac", data, 4) != 0)
		goto badData;
	result = appmgrGetSfoEntry(&file, "TITLE_ID", &data, &length, &capacity);
	if (result < 0)
		goto release;
	if (length <= 9 || capacity != 12)
		goto badData;
	strncpy(titleId, data, 9);
	result = 0;
	goto release;
badData:
	result = (int)0x80800034;
release:
	appmgrReleaseSfoFile(&file);
	return result;
}

/* A drive name of its prefix and six random bytes in hex. */
int appmgrMakeRandomDriveId(const char *prefix, char *driveId)
{
	unsigned char random[8];
	int result;

	result = sceSblRngPseudoRandomNumber(random, sizeof(random));
	if (result < 0)
		return result;
	snprintf(driveId, 16, "%s%02x%02x%02x%02x%02x%02xd", prefix, random[0],
		 random[1], random[2], random[3], random[4], random[5]);
	return 0;
}

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
	permission = sceKernelSetThreadAccessLevel(128);
	result = SceFios2KernelForDriver_17E65A1C(pid, &overlay, overlayId);
	sceKernelSetThreadAccessLevel(permission);
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
	permission = sceKernelSetThreadAccessLevel(128);
	result = SceFios2KernelForDriver_17E65A1C(pid, &overlay, &mount->overlayId);
	sceKernelSetThreadAccessLevel(permission);
	return result;
}

int appmgrGetConsoleType(void)
{
	if (SceSblAIMgrForDriver_D78B04A2())
		return 0;
	if (SceSblAIMgrForDriver_F4B98F66())
		return 1;
	if (SceSblACMgrForDriver_E87D1777())
		return 3;
	return 2;
}

void appmgrUpdateAccountDirectory(void)
{
	SceUInt32 account = 0;
	int permission, result;
	char tens, units;

	permission = sceKernelSetThreadAccessLevel(128);
	result = sceRegMgrGetKeyInt("/CONFIG/NP2", "current_account2", (int *)&account);
	sceKernelSetThreadAccessLevel(permission);
	if (result >= 0 && account <= 63) {
		tens = account / 10 + '0';
		units = account % 10 + '0';
	} else {
		tens = '0';
		units = '0';
	}
	g_appmgrAccountDirectory[0] = tens;
	g_appmgrAccountDirectory[1] = units;
	g_appmgrAccountDirectory[2] = 0;
}

/* A title ID of param.sfo (10 bytes in a 12-byte field) or the application's own. */
#define SFO_TITLE_ID_OK(_length, _capacity) ((_length) > 9 && (_capacity) == 12)

/*
 * Reads an application's param.sfo into its process content: the category and
 * title IDs, the attributes, and for a resolved application path (not an SFO
 * path given directly) the version, save data, install directories, content
 * and NP IDs and the titles in the system language.
 */
int appmgrReadParamSfo(SceUID pid, const char *path, int isSfoPath,
		       AppmgrProcessContent *content)
{
	AppmgrSfoFile file = {0};
	void *data = NULL;
	SceSize length = 0, capacity = 0;
	SceUInt32 language;
	char key[32];
	const char *category;
	SceUInt32 attribute, value;
	int permission, index, result;

	memset(g_appmgrDirectoryPath, 0, sizeof(g_appmgrDirectoryPath));
	if (pid == 0)
		pid = sceKernelGetProcessId();
	SceKernelSuspendForDriver_4DF40893(0);
	if (isSfoPath) {
		strncpy(g_appmgrDirectoryPath, path, sizeof(g_appmgrDirectoryPath));
		result = appmgrLoadSfoFile(g_appmgrDirectoryPath, &file);
		if (result < 0)
			goto out;
	} else {
		permission = sceKernelSetThreadAccessLevel(128);
		result = SceFios2KernelForDriver_0F456345(pid, 0, path, g_appmgrDirectoryPath,
							  sizeof(g_appmgrDirectoryPath));
		sceKernelSetThreadAccessLevel(permission);
		if (result != 0)
			goto out;
		result = appmgrLoadSfoFile(g_appmgrDirectoryPath, &file);
		if (result < 0)
			goto out;
		if ((content->contentFlags & 1) == 0)
			goto readAttributes;
	}
	result = appmgrGetSfoEntry(&file, "CATEGORY", &data, &length, &capacity);
	if (result < 0)
		goto out;
	if (length <= 2 || capacity != 4)
		goto badData;
	category = data;
	if (strncmp(category, appmgrProgramCategories[0], 4) == 0) {
		if (content->contentFlags & 1)
			strncpy(content->category, appmgrProgramCategories[1], 4);
	} else if (strncmp(category, appmgrProgramCategories[2], 4) == 0) {
		if (content->contentFlags & 1)
			__builtin_memcpy(content->category, appmgrProgramCategories[3], 4);
	} else if (strncmp(category, appmgrProgramCategories[4], 4) == 0) {
		if (content->contentFlags & 1)
			__builtin_memcpy(content->category, appmgrProgramCategories[5], 4);
	} else {
		strncpy(content->category, category, 4);
		content->category[3] = '\0';
	}
	result = appmgrGetSfoEntry(&file, "TITLE_ID", &data, &length, &capacity);
	if (result < 0)
		goto out;
	if (!SFO_TITLE_ID_OK(length, capacity))
		goto badData;
	strncpy(content->titleId, data, 10);
	content->titleId[9] = '\0';
	__builtin_memcpy(content->patchTitleId, content->titleId, 16);
	__builtin_memcpy(content->originalTitleId, content->titleId, 16);

readAttributes:
	if (appmgrGetSfoEntry(&file, "ATTRIBUTE", &data, &length, &capacity) >= 0 &&
	    length > 3 && capacity == 4) {
		if (isSfoPath) {
			content->attribute = *(const SceUInt32 *)data;
		} else {
			attribute = content->attribute;
			value = *(const SceUInt32 *)data;
			value = (attribute & 1) ? value | 1 : value & ~1U;
			content->attribute = value;
			value = (attribute & 0x100000) ? value | 0x100000 : value & ~0x100000U;
			content->attribute = value;
		}
	} else {
		content->attribute = 0;
	}
	if (appmgrGetSfoEntry(&file, "ATTRIBUTE2", &data, &length, &capacity) >= 0 &&
	    length > 3 && capacity == 4)
		content->attribute2 = *(const SceUInt32 *)data;
	else
		content->attribute2 = 0;
	if (appmgrGetSfoEntry(&file, "ATTRIBUTE_MINOR", &data, &length, &capacity) >= 0 &&
	    length > 3 && capacity == 4) {
		content->attributeMinorMode = *(const SceUInt32 *)data & 3;
		content->attributeMinor = *(const SceUInt32 *)data;
	} else {
		content->attributeMinorMode = 0;
		content->attributeMinor = 0;
	}
	if (isSfoPath)
		goto done;

	if (appmgrGetSfoEntry(&file, "APP_VER", &data, &length, &capacity) < 0 ||
	    length <= 5 || capacity != 8)
		content->appVersion = 0;
	content->appVersion = appmgrParseSfoVersion(data);
	if (appmgrGetSfoEntry(&file, "SAVEDATA_MAX_SIZE", &data, &length, &capacity) >= 0 &&
	    length > 3 && capacity == 4)
		content->savedataMaxSize = *(const SceUInt32 *)data;
	else
		content->savedataMaxSize = 0;
	if (appmgrGetSfoEntry(&file, "SAFEMEMORY_MODE", &data, &length, &capacity) >= 0 &&
	    length > 3 && capacity == 4)
		content->safeMemoryMode = *(const SceInt32 *)data;
	else
		content->safeMemoryMode = 0;

	if (appmgrGetSfoEntry(&file, "INSTALL_DIR_SAVEDATA", &data, &length, &capacity) >= 0 &&
	    SFO_TITLE_ID_OK(length, capacity)) {
		strncpy(content->saveDataTitleIds[0], data, 10);
		content->saveDataTitleIds[0][9] = '\0';
	} else {
		__builtin_memcpy(content->saveDataTitleIds[0], content->titleId, 16);
	}
	for (index = 1; index != 8; index++) {
		char *titleId = content->saveDataTitleIds[index];

		snprintf(key, sizeof(key), "INSTALL_DIR_SAVEDATA_ADD_%d", index);
		__builtin_memset(titleId, 0, 10);
		if (appmgrGetSfoEntry(&file, key, &data, &length, &capacity) >= 0 &&
		    SFO_TITLE_ID_OK(length, capacity))
			strncpy(titleId, data, 9);
	}
	if (appmgrGetSfoEntry(&file, "INSTALL_DIR_ADDCONT", &data, &length, &capacity) >= 0 &&
	    SFO_TITLE_ID_OK(length, capacity)) {
		strncpy(content->addContTitleIds[0], data, 10);
		content->addContTitleIds[0][9] = '\0';
	} else {
		__builtin_memcpy(content->addContTitleIds[0], content->titleId, 16);
	}
	for (index = 1; index != 8; index++) {
		char *titleId = content->addContTitleIds[index];

		snprintf(key, sizeof(key), "INSTALL_DIR_ADDCONT_ADD_%d", index);
		__builtin_memset(titleId, 0, 10);
		if (appmgrGetSfoEntry(&file, key, &data, &length, &capacity) >= 0 &&
		    SFO_TITLE_ID_OK(length, capacity))
			strncpy(titleId, data, 9);
	}

	if (appmgrGetSfoEntry(&file, "CONTENT_ID", &data, &length, &capacity) >= 0 &&
	    length > 36 && capacity == 48)
		strncpy(content->contentId, data, 48);
	else
		strncpy(content->contentId, "", 48);
	content->contentId[47] = '\0';
	if (appmgrGetSfoEntry(&file, "NP_COMMUNICATION_ID", &data, &length, &capacity) >= 0 &&
	    length > 12 && capacity == 16)
		strncpy(content->npCommunicationId, data, 13);
	else
		strncpy(content->npCommunicationId, "", 13);
	content->npCommunicationId[12] = '\0';

	result = sceRegMgrGetKeyInt("/CONFIG/SYSTEM", "language", (int *)&language);
	if (result != 0 || language > 99)
		goto done;
	snprintf(key, sizeof(key), "TITLE_%02u", language);
	if (!((appmgrGetSfoEntry(&file, key, &data, &length, &capacity) >= 0 &&
	       length != 0 && capacity == 128) ||
	      (appmgrGetSfoEntry(&file, "TITLE", &data, &length, &capacity) >= 0 &&
	       length != 0 && capacity == 128)))
		data = NULL;
	if (data != NULL)
		strncpy(content->title, data, 128);
	else
		strncpy(content->title, "", 128);
	content->title[127] = '\0';
	snprintf(key, sizeof(key), "STITLE_%02u", language);
	if (!((appmgrGetSfoEntry(&file, key, &data, &length, &capacity) >= 0 &&
	       length != 0 && capacity == 52) ||
	      (appmgrGetSfoEntry(&file, "STITLE", &data, &length, &capacity) >= 0 &&
	       length != 0 && capacity == 52)))
		data = NULL;
	if (data != NULL)
		strncpy(content->shortTitle, data, 52);
	else
		strncpy(content->shortTitle, "", 52);
	content->shortTitle[51] = '\0';
done:
	result = 0;
	goto out;
badData:
	result = (int)0x80800034;
out:
	appmgrReleaseSfoFile(&file);
	SceKernelSuspendForDriver_2BB92967(0);
	return result;
}

/*
 * Copies a path (shorter than 292) with each run of slashes made one, a
 * trailing slash dropped and a slash after the drive's colon; ".." is refused.
 * host0: paths are copied as they are.
 */
int appmgrNormalizePath(const char *path, char *normalized)
{
	unsigned int out = 0;
	int in = 0;
	char *slash;
	char c;

	if (normalized == NULL || path == NULL) {
		if (normalized != NULL)
			goto invalid;
		return (int)0x80800001;
	}
	if (strnlen(path, 292) >= 292)
		goto invalid;
	memset(normalized, 0, 292);
	if (strncmp(path, "host0:", 6) == 0) {
		strncpy(normalized, path, 291);
		return 0;
	}
	c = path[0];
	if (c == '\0')
		return 0;
	for (;;) {
		if (c == ':') {
			normalized[out] = c;
			c = path[++in];
			if (c != '/') {
				if (out == 291)
					goto invalid;
				normalized[out + 1] = '/';
				out += 2;
				c = path[in];
			} else {
				out++;
			}
		} else if (c == '/') {
			slash = &normalized[out];
			*slash = '/';
			out++;
			do {
				c = path[++in];
			} while (c == '/');
			if (c == '\0') {
				*slash = '\0';
				c = path[in];
			}
		} else {
			in++;
			if (c == '.') {
				normalized[out++] = '.';
				c = path[in];
				if (c == '.')
					goto invalid;
			} else {
				normalized[out++] = c;
				c = path[in];
			}
		}
		if (c == '\0')
			break;
		if (out >= 292)
			goto invalid;
	}
	if (out < 292)
		return 0;
invalid:
	*normalized = '\0';
	return (int)0x80800001;
}

char *appmgrAllocMountPath(void)
{
	SceAllocOpt opt;

	opt.size = sizeof(opt);
	opt.mmapKind = 0;
	opt.align = 32;
	opt.mmapBase = NULL;
	opt.mmapSize = 0;
	return sceKernelAllocHeapMemoryWithOption(g_appmgrMountHeap, 292, &opt);
}

/* The drive a mount of the calling application with this mount ID is reached by. */
int appmgrGetMountPointById(int mountId, char *mountPoint)
{
	SceUInt32 syscall = sceSyscallEnter();
	AppmgrVirtualMount *mount;
	struct AppState *app;
	int result;

	result = sceKernelLockMutex(g_appmgrMountMutex, 1, NULL);
	if (result < 0)
		goto out;
	app = appmgrFindAppByProcessId(g_appmgrAppStates, sceKernelGetProcessId());
	if (app == NULL) {
		result = 0x80801002;
		goto unlock;
	}
	for (mount = app->process.mounts.firstMount; mount != NULL; mount = mount->next) {
		if (mount->pfsMount != NULL && mount->pfsMount->mountId == mountId)
			break;
	}
	if (mount == NULL) {
		result = 0x80800002;
	} else {
		result = 0;
		if (mountPoint != NULL)
			__builtin_memcpy(mountPoint, &mount->driveId, sizeof(mount->driveId));
	}
unlock:
	sceKernelUnlockMutex(g_appmgrMountMutex, 1);
out:
	sceSyscallLeave(syscall);
	return result;
}

/* Whether the calling process may mount the drive with this mount ID. */
int appmgrIsMountIdAllowed(unsigned int mountId)
{
	SceSelfAuthInfo authInfo;
	SceUInt64 authorityId;

	switch (mountId) {
	case 106:
	case 107:
		if (SceSblACMgrForDriver_C98D82EE(0) != 0)
			return 1;
		return sceSblACMgrIsSystemProgram(0) != 0;
	case 205:
		return SceSblACMgrForDriver_456DA7AC(0);
	case 600:
		return SceSblACMgrForDriver_4DB7F512(0);
	case 110:
	case 111:
	case 112:
	case 302:
	case 303:
	case 304:
	case 500:
	case 501:
	case 502:
	case 503:
	case 504:
	case 505:
	case 900:
	case 901:
		break;
	default:
		return 1;
	}
	if (sceKernelGetProcessSelfAuthInfo(0, &authInfo) < 0)
		return 0;
	authorityId = authInfo.programAuthorityId;
	switch (mountId) {
	case 112:
		return SceSblACMgrForDriver_6C5AB07F(0);
	case 900:
	case 901:
		return authorityId == 0x2808000000000001ULL;
	case 110:
		return authorityId == 0x280000000000002DULL ||
		       authorityId == 0x2800000000000001ULL;
	case 111:
		return SceSblACMgrForDriver_F5AE24AC(0) != 0;
	default:
		return authorityId == 0x2800000000000001ULL;
	}
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
	result = sceIoDevctl(device, 0x3001, NULL, 0, &info, sizeof(info));
	if (result < 0) {
		if (result == (int)0x80010030)
			result = 0;
	} else {
		clusters = (SceUInt64)info.freeSize / info.clusterSize;
		result = clusters * (SceUInt64)info.clusterSize >= requiredBytes ?
			0 : (int)0x80800016;
	}
out:
	return result;
}

int appmgrResolvePfsDevicePath(const char *path, char *output, SceSize size)
{
	char device[33] = { 0 };
	AppmgrPfsMount *mount;
	const char *cursor, *separator;
	unsigned int index = 0;

	for (;;) {
		device[index] = path[index];
		if (device[index] == ':')
			break;
		if (++index == sizeof(device))
			return -1;
	}
	device[15] = 0;
	mount = g_appmgrPfsMounts;
	while (mount != NULL) {
		if (strncmp(mount->mountPoint.drive_id, device, 16) == 0)
			break;
		mount = mount->next;
	}
	if (mount == NULL)
		return (int)0x80800002;
	cursor = path;
	for (;;) {
		if (*cursor == ':')
			break;
		if (*cursor == 0 || ++cursor == path + 33)
			return (int)0x8080000c;
	}
	separator = cursor[1] == '/' ? "" : "/";
	snprintf(output, size, "%s%s%s", mount->appPath, separator, cursor + 1);
	return 0;
}

int appmgrReleasePfsMount(SceUID pid, AppmgrPfsMount *mount, int flags)
{
	SceSelfAuthInfo authInfo;
	SceUInt64 authorityId, entry;
	AppmgrPfsMount *cursor, *previous;
	int permission, result, removed = 0, references = 0, index;

	result = sceKernelGetProcessSelfAuthInfo(pid, &authInfo);
	if (result < 0)
		return result;
	authorityId = authInfo.programAuthorityId;
	for (index = 0; index != 16; ++index) {
		entry = mount->programAuthorityIds[index];
		if (!removed && entry == authorityId) {
			mount->programAuthorityIds[index] = 0;
			removed = 1;
		} else if (entry != 0) {
			++references;
		}
	}
	if (references != 0) {
		if (authorityId == mount->programAuthorityIds[0] ||
		    authorityId == mount->programAuthorityIds[1] ||
		    authorityId == mount->programAuthorityIds[2] ||
		    authorityId == mount->programAuthorityIds[3] ||
		    authorityId == mount->programAuthorityIds[4] ||
		    authorityId == mount->programAuthorityIds[5] ||
		    authorityId == mount->programAuthorityIds[6] ||
		    authorityId == mount->programAuthorityIds[7] ||
		    authorityId == mount->programAuthorityIds[8] ||
		    authorityId == mount->programAuthorityIds[9] ||
		    authorityId == mount->programAuthorityIds[10] ||
		    authorityId == mount->programAuthorityIds[11] ||
		    authorityId == mount->programAuthorityIds[12] ||
		    authorityId == mount->programAuthorityIds[13] ||
		    authorityId == mount->programAuthorityIds[14] ||
		    authorityId == mount->programAuthorityIds[15] ||
		    mount->mountPoint.drive_id[0] == 0)
			return 0;
		result = sceKernelGetProcessSelfAuthInfo(pid, &authInfo);
		if (result < 0)
			return result;
		if (authInfo.programAuthorityId == 0x2800000000000001ULL ||
		    authInfo.programAuthorityId == 0x2800000000000010ULL ||
		    authInfo.programAuthorityId == 0x280000000000002dULL ||
		    (mount->mountId == 1005 &&
		     authInfo.programAuthorityId == 0x2800000000000039ULL))
			return 0;
		result = ScePfsMgrForKernel_2D67D8CA(mount->mountPoint.drive_id,
					  authInfo.programAuthorityId);
		return result == (int)0x80010002 || result >= 0 ? 0 : result;
	}
	if (mount->mountId == 600) {
		permission = sceKernelSetThreadAccessLevel(128);
		result = SceIofilemgrForDriver_20574100(0x10000, flags, 0, 0);
		sceKernelSetThreadAccessLevel(permission);
		if (result < 0)
			goto restore;
	} else if (mount->mountPoint.drive_id[0] != 0) {
		permission = sceKernelSetThreadAccessLevel(128);
		result = ScePfsMgrForKernel_680BC384(mount->mountPoint.drive_id);
		sceKernelSetThreadAccessLevel(permission);
		if (result != (int)0x80010013 && result != 0)
			goto restore;
	}
	cursor = g_appmgrPfsMounts;
	if (cursor == NULL)
		return (int)0x80800002;
	if (cursor == mount)
		goto removeHead;
	for (;;) {
		cursor = cursor->next;
		if (cursor == NULL)
			return (int)0x80800002;
		if (cursor == mount) {
			if (g_appmgrPfsMounts == mount)
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
	g_appmgrPfsMounts = g_appmgrPfsMounts->next;
	if (g_appmgrPfsMounts != NULL)
		g_appmgrPfsMounts->prev = NULL;
freeMount:
	sceKernelFreeHeapMemory(g_appmgrMountHeap, mount);
	return 0;

restore:
	if (!removed)
		return result;
	if (mount->programAuthorityIds[0] == 0)
		index = 0;
	else if (mount->programAuthorityIds[1] == 0)
		index = 1;
	else if (mount->programAuthorityIds[2] == 0)
		index = 2;
	else if (mount->programAuthorityIds[3] == 0)
		index = 3;
	else if (mount->programAuthorityIds[4] == 0)
		index = 4;
	else if (mount->programAuthorityIds[5] == 0)
		index = 5;
	else if (mount->programAuthorityIds[6] == 0)
		index = 6;
	else if (mount->programAuthorityIds[7] == 0)
		index = 7;
	else if (mount->programAuthorityIds[8] == 0)
		index = 8;
	else if (mount->programAuthorityIds[9] == 0)
		index = 9;
	else if (mount->programAuthorityIds[10] == 0)
		index = 10;
	else if (mount->programAuthorityIds[11] == 0)
		index = 11;
	else if (mount->programAuthorityIds[12] == 0)
		index = 12;
	else if (mount->programAuthorityIds[13] == 0)
		index = 13;
	else if (mount->programAuthorityIds[14] == 0)
		index = 14;
	else if (mount->programAuthorityIds[15] == 0)
		index = 15;
	else
		return result;
	mount->programAuthorityIds[index] = authorityId;
	return result;
}

int appmgrReadRif(const char *path, AppmgrLicenseInfo *info)
{
	SceAllocOpt opt;
	SceNpDrmLicense *license;
	char startTime[36], expiryTime[32];
	int fd, length, result;

	opt.size = sizeof(opt);
	opt.mmapKind = 0;
	opt.align = 32;
	opt.mmapBase = NULL;
	opt.mmapSize = 0;
	license = sceKernelAllocHeapMemoryWithOption(g_appmgrCoreHeap, 512, &opt);
	if (license == NULL) {
		result = (int)0x80801006;
		SceKernelSuspendForDriver_2BB92967(0);
		return result;
	}
	SceKernelSuspendForDriver_4DF40893(0);
	fd = sceIoOpen(path, 0x10001, 0);
	if (fd == (int)0x80010002) {
		result = (int)0x80800004;
		goto out;
	}
	result = (int)0x80800010;
	if (fd < 0)
		goto out;
	length = sceIoRead(fd, license, 512);
	sceIoClose(fd);
	if (length != 512)
		goto out;
	result = SceNpDrmForDriver_DB406EAE(license, 512, 1, info->contentId,
		&info->accountId, &info->licenseVersion, &info->licenseFlags,
		&info->flags, &info->skuFlags, &info->licStartTime, &info->licExpTime,
		&info->flags2);
	if (result < 0)
		goto out;
	if ((info->flags & 0x20000) && strncmp(path, "gro0:", 5) != 0) {
		result = (int)0x80800010;
		goto out;
	}
	if (info->licStartTime == 0) {
		startTime[0] = '-';
		startTime[1] = 0;
	} else if (SceRtcForDriver_D32AC698(startTime, (SceRtcTick *)&info->licStartTime,
				     540) >= 0) {
		startTime[10] = ' ';
		startTime[19] = ' ';
		startTime[20] = 'J';
		startTime[21] = 'S';
		startTime[22] = 'T';
		startTime[23] = 0;
	}
	if (info->licExpTime == 0) {
		expiryTime[0] = '-';
		expiryTime[1] = 0;
	} else if (SceRtcForDriver_D32AC698(expiryTime, (SceRtcTick *)&info->licExpTime,
				     540) >= 0) {
		expiryTime[19] = ' ';
		expiryTime[20] = 'J';
		expiryTime[21] = 'S';
		expiryTime[22] = 'T';
		expiryTime[23] = 0;
	}
	result = SceNpDrmForDriver_723322B5(license, info->klicense,
		&info->flags, &info->skuFlags, &info->licStartTime, &info->licExpTime);
	if (result >= 0) {
		if (info->licStartTime == 0) {
			startTime[0] = '-';
			startTime[1] = 0;
		} else if (SceRtcForDriver_D32AC698(startTime,
				(SceRtcTick *)&info->licStartTime, 540) >= 0) {
			startTime[10] = ' ';
			startTime[19] = ' ';
			startTime[20] = 'J';
			startTime[21] = 'S';
			startTime[22] = 'T';
			startTime[23] = 0;
		}
		if (info->licExpTime == 0) {
			expiryTime[0] = '-';
			expiryTime[1] = 0;
		} else if (SceRtcForDriver_D32AC698(expiryTime,
				(SceRtcTick *)&info->licExpTime, 540) >= 0) {
			expiryTime[19] = ' ';
			expiryTime[20] = 'J';
			expiryTime[21] = 'S';
			expiryTime[22] = 'T';
			expiryTime[23] = 0;
		}
	}
out:
	SceKernelSuspendForDriver_2BB92967(0);
	sceKernelFreeHeapMemory(g_appmgrCoreHeap, license);
	return result;
}

int appmgrCheckPackageLicenseFlag(const char *appPath, int *hasLicense)
{
	SceAllocOpt opt;
	unsigned char headerBytes[8];
	char *path;
	int fd, result;

	*hasLicense = 1;
	SceKernelSuspendForDriver_4DF40893(0);
	opt.size = sizeof(opt);
	opt.mmapKind = 0;
	opt.align = 32;
	opt.mmapBase = NULL;
	opt.mmapSize = 0;
	path = sceKernelAllocHeapMemoryWithOption(g_appmgrMountHeap, 292, &opt);
	if (path == NULL) {
		result = (int)0x80801006;
		goto out;
	}
	snprintf(path, 292, "%s/%s/%s", appPath, "sce_sys", "package/head.bin");
	fd = sceIoOpen(path, 0x10001, 0);
	if (fd < 0) {
		snprintf(path, 292, "%s/%s/%s", appPath, "sce_sys", "clearsign");
		fd = sceIoOpen(path, 0x10001, 0);
	}
	result = fd;
	if (fd >= 0) {
		result = sceIoRead(fd, headerBytes, sizeof(headerBytes));
		sceIoClose(fd);
		if (result >= 0) {
			if (result == sizeof(headerBytes)) {
				result = 0;
				if ((signed char)headerBytes[offsetof(
					struct NpDrmPackageMainHeader, revision)] < 0)
					*hasLicense = 0;
			} else {
				result = (int)0x8080001d;
			}
		}
	}
	sceKernelFreeHeapMemory(g_appmgrMountHeap, path);
out:
	SceKernelSuspendForDriver_2BB92967(0);
	return result;
}

int appmgrReadKeystone(SceUID pid, const char *driveId, void *keystone)
{
	SceAllocOpt opt;
	char keyPath[64];
	char *resolvedPath;
	SceUID fd;
	int permission, result, count;

	permission = sceKernelSetThreadAccessLevel(128);
	SceKernelSuspendForDriver_4DF40893(0);
	opt = (SceAllocOpt) { .size = sizeof(opt), .align = 32 };
	resolvedPath = sceKernelAllocHeapMemoryWithOption(g_appmgrMountHeap, 292,
							 &opt);
	if (resolvedPath == NULL) {
		result = (int)0x80801006;
		goto out;
	}
	memset(resolvedPath, 0, 292);
	snprintf(keyPath, sizeof(keyPath), "%ssce_sys/%s", driveId, "keystone");
	if (pid == 0)
		pid = sceKernelGetProcessId();
	result = SceFios2KernelForDriver_0F456345(pid, 0, keyPath, resolvedPath, 292);
	if (result == 0) {
		fd = sceIoOpen(resolvedPath, 1, 0);
		result = fd;
		if (fd >= 0) {
			count = sceIoRead(fd, keystone, 96);
			sceIoClose(fd);
			result = count == 96 ? 0 : (int)0x80800005;
		}
	}
	sceKernelFreeHeapMemory(g_appmgrMountHeap, resolvedPath);
out:
	SceKernelSuspendForDriver_2BB92967(0);
	sceKernelSetThreadAccessLevel(permission);
	return result;
}

static inline int appmgrRemoveOverlay(SceUID pid, SceFiosOverlayID overlayId)
{
	int permission, result;

	if (pid == 0)
		pid = sceKernelGetProcessId();
	permission = sceKernelSetThreadAccessLevel(128);
	result = SceFios2KernelForDriver_23247EFB(pid, overlayId);
	sceKernelSetThreadAccessLevel(permission);
	return result;
}

/*
 * Hide (or show again) the memory card drives a process sees, as the debug
 * setting "fake no memory card" asks: 2 hides savedata0: and addcont0:, any
 * mode hides photo0: and music0:.
 */
int appmgrUpdateFakeNoMemoryCardOverlays(SceUID pid, AppmgrProcessContent *content)
{
	SceFiosOverlayID *overlayIds = content->overlayIds;
	int result = 0;

	if (appmgrGetFakeNoMemoryCard() == 2) {
		if (overlayIds[12] == 0) {
			result = appmgrAddInvalidOverlay(pid, "savedata0:", &overlayIds[12]);
			if (result < 0)
				return result;
		}
		if (overlayIds[13] == 0) {
			result = appmgrAddInvalidOverlay(pid, "addcont0:", &overlayIds[13]);
			if (result < 0)
				return result;
		}
	} else {
		if (overlayIds[12] != 0) {
			result = appmgrRemoveOverlay(pid, overlayIds[12]);
			if (result < 0)
				return result;
			overlayIds[12] = 0;
		}
		if (overlayIds[13] != 0) {
			result = appmgrRemoveOverlay(pid, overlayIds[13]);
			if (result < 0)
				return result;
			overlayIds[13] = 0;
		}
	}
	if (appmgrGetFakeNoMemoryCard() != 0) {
		if (overlayIds[14] == 0) {
			result = appmgrAddInvalidOverlay(pid, "photo0:", &overlayIds[14]);
			if (result < 0)
				return result;
		}
		if (overlayIds[15] == 0)
			return appmgrAddInvalidOverlay(pid, "music0:", &overlayIds[15]);
		return result;
	}
	if (overlayIds[14] != 0) {
		result = appmgrRemoveOverlay(pid, overlayIds[14]);
		if (result < 0)
			return result;
		overlayIds[14] = 0;
	}
	if (overlayIds[15] != 0) {
		result = appmgrRemoveOverlay(pid, overlayIds[15]);
		if (result < 0)
			return result;
		overlayIds[15] = 0;
	}
	return result;
}

extern SceUID g_appmgrMainMutex;
extern SceUID g_appmgrMainMutexOwner;

#if FW_DEBUG
#define MOUNT_MAIN_LOCK(_line) do { \
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
#define MOUNT_MAIN_UNLOCK(_line) do { \
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
static const SceKernelDebugInfo mountPanicInfo = {
	.fileHash = 0xc9656e8e,
	.lineHash = 0x79d3a1f0,
	.funcHash = 0x9d536bce,
};

static __attribute__((noinline)) _Noreturn void appmgrMountPanic(void)
{
	SceDebugForDriver_391B5B74(&mountPanicInfo, __builtin_return_address(0));
}

#define MOUNT_MAIN_LOCK(_line) do { \
	if (sceKernelLockMutex(g_appmgrMainMutex, 1, NULL) != 0) \
		appmgrMountPanic(); \
	g_appmgrMainMutexOwner = sceKernelGetThreadId(); \
} while (0)
#define MOUNT_MAIN_UNLOCK(_line) do { \
	g_appmgrMainMutexOwner = -1; \
	if (sceKernelUnlockMutex(g_appmgrMainMutex, 1) != 0) \
		appmgrMountPanic(); \
} while (0)
#endif

/* Applies a changed debug setting (0: fake no memory card) to the big application. */
int _debugSettingCallBack(int setting, int value, void *context)
{
	struct AppState *app;
	int result = 0;

	if (setting != 0)
		return result;
	result = sceKernelLockMutex(g_appmgrMountMutex, 1, NULL);
	if (result < 0)
		return result;
	MOUNT_MAIN_LOCK(14729);
	app = _appListGetBigApp(g_appmgrAppStates);
	if (app != NULL && (app->process.content.contentFlags & 0x10) != 0)
		result = appmgrUpdateFakeNoMemoryCardOverlays(app->process.processId,
							      &app->process.content);
	MOUNT_MAIN_UNLOCK(14742);
	sceKernelUnlockMutex(g_appmgrMountMutex, 1);
	return result;
}

/* The PFS mount in the list with this mount ID and path, which is normalized first. */
int appmgrFindPfsMount(AppmgrPfsMount *mount, int mountId, const char *path,
		       AppmgrPfsMount **found)
{
	SceAllocOpt opt;
	char *normalized;
	int result;

	opt.size = sizeof(opt);
	opt.mmapKind = 0;
	opt.align = 32;
	opt.mmapBase = NULL;
	opt.mmapSize = 0;
	normalized = sceKernelAllocHeapMemoryWithOption(g_appmgrMountHeap, 292, &opt);
	if (normalized == NULL)
		return 0x80801006;
	result = appmgrNormalizePath(path, normalized);
	if (result >= 0) {
		for (; mount != NULL; mount = mount->next) {
			if (mount->mountId == mountId &&
			    strncmp(mount->appPath, normalized, 292) == 0)
				break;
		}
		if (mount == NULL) {
			result = 0x80800002;
		} else {
			result = 0;
			if (found != NULL)
				*found = mount;
		}
	}
	sceKernelFreeHeapMemory(g_appmgrMountHeap, normalized);
	return result;
}

int appmgrGetPatchPath(const char *appPath, const char *titleId,
		      SceUInt32 attributes, const char *category, char *patchPath)
{
	if (strncmp(appPath, "host0:", 6) == 0 ||
	    strncmp(appPath, "ux0:data", 8) == 0 ||
	    strncmp(appPath, "ux0:\\data", 9) == 0 ||
	    strncmp(appPath, "ux0:/data", 9) == 0)
		return (int)0x8080000f;

	if ((attributes & 1) || strncmp(category, "gdb", 4) == 0)
		goto internalPatch;

	if (strncmp(category, "gd", 3) != 0 &&
	    strncmp(category, "gdc", 4) != 0 &&
	    strncmp(category, "gdd", 4) != 0)
		return (int)0x8080000f;

	if (strncmp(appPath, "gro0:", 5) == 0) {
		snprintf(patchPath, 292, "grw0:patch/%s", titleId);
		return 0;
	}
	if (strncmp(appPath, "ux0:", 4) != 0)
		return (int)0x8080000f;

internalPatch:
	snprintf(patchPath, 292, "ux0:patch/%s", titleId);
	return 0;
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

	mount = sceKernelAllocHeapMemoryWithOption(g_appmgrMountHeap,
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
		status = sceKernelGetProcessSelfAuthInfo(pid, &authInfo);
		if (status < 0)
			goto authError;
		status = ScePfsMgrForKernel_D8D0FEE5(pfsMount->mountPoint.drive_id,
				       authInfo.programAuthorityId);
		if (status != (int)0x80010011 && status < 0)
			goto authError;
	}
	status = sceKernelGetProcessSelfAuthInfo(pid, &authInfo);
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
	pfsMount->programAuthorityIds[index] = authInfo.programAuthorityId;
	if (outMount != NULL)
		*outMount = mount;
	return result;
duplicateMount:
	result = (int)0x80800003;
	goto freeMount;
authError:
	result = status;
freeMount:
	sceKernelFreeHeapMemory(g_appmgrMountHeap, mount);
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
		if (SceSblAIMgrForDriver_D78B04A2() || SceSblAIMgrForDriver_F4B98F66())
			return (int)0x8080000e;
		SceSblACMgrForDriver_E87D1777();
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

int appmgrReadSealedKey(const char *appPath, void *key)
{
	SceAllocOpt opt;
	unsigned char sealedKey[80];
	char *path;
	int permission, result, length;

	permission = sceKernelSetThreadAccessLevel(128);
	SceKernelSuspendForDriver_4DF40893(0);
	opt.size = sizeof(opt);
	opt.mmapKind = 0;
	opt.align = 32;
	opt.mmapBase = NULL;
	opt.mmapSize = 0;
	path = sceKernelAllocHeapMemoryWithOption(g_appmgrMountHeap, 292, &opt);
	if (path != NULL) {
		snprintf(path, 292, "%s/sce_sys/%s", appPath, "sealedkey");
		result = sceIoOpen(path, 0x10001, 4);
		if (result >= 0) {
			length = sceIoRead(result, sealedKey, sizeof(sealedKey));
			sceIoClose(result);
			if (length < 0) {
				result = length;
			} else if (length != sizeof(sealedKey)) {
				result = (int)0x80800043;
			} else {
				result = sceSblSsDecryptSealedKey(sealedKey, key);
				if (result >= 0 &&
				    (strncmp(path, "host0:", 6) == 0 ||
				     strncmp(path, "ux0:data", 8) == 0 ||
				     strncmp(path, "ux0:\\data", 9) == 0 ||
				     strncmp(path, "ux0:/data", 9) == 0))
					memset(key, 0, 16);
			}
		}
		sceKernelFreeHeapMemory(g_appmgrMountHeap, path);
	} else {
		result = (int)0x80801006;
	}
	SceKernelSuspendForDriver_2BB92967(0);
	sceKernelSetThreadAccessLevel(permission);
	return result;
}

int appmgrReadSaveDataMetadata(const char *path, AppmgrProcessContent *content,
			     SceUInt32 *maxSize, SceUInt64 *accountId)
{
	AppmgrSfoFile file = {0};
	void *data = NULL;
	SceSize length = 0, capacity = 0;
	int fakeOwner = 0;
	SceAllocOpt opt;
	SceKernelOpenPsId openPsId;
	SceUInt64 currentAccountId, savedAccountId;
	AppmgrSavedataParams *params;
	char *sfoPath, *directory, *backslash, *colon;
	const char *parentDirectory;
	SceUInt32 attribute = 0;
	int permission, accountIndex, result;

	SceKernelSuspendForDriver_4DF40893(0);
	opt = (SceAllocOpt) { .size = sizeof(opt), .align = 32 };
	sfoPath = sceKernelAllocHeapMemoryWithOption(g_appmgrMountHeap, 292, &opt);
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
	permission = sceKernelSetThreadAccessLevel(64);
	result = sceRegMgrGetKeyBin("/CONFIG/NP", "account_id",
				 &currentAccountId, sizeof(currentAccountId));
	if (result < 0)
		goto restorePermission;
	result = sceSblAimgrGetOpenPsId(&openPsId);
	if (result < 0)
		goto restorePermission;
	if (sceRegMgrGetKeyInt("/CONFIG/NP2", "current_account2", &accountIndex) < 0 ||
	    (unsigned int)accountIndex > 63)
		accountIndex = 0;
	sceKernelSetThreadAccessLevel(permission);
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
	sceKernelSetThreadAccessLevel(permission);
	goto freePath;
badData:
	result = (int)0x80800034;
freePath:
	sceKernelFreeHeapMemory(g_appmgrMountHeap, sfoPath);
out:
	appmgrReleaseSfoFile(&file);
	SceKernelSuspendForDriver_2BB92967(0);
	return result;
}

int appmgrGetSaveDataPath(const char *appPath, const char *titleId,
			 SceUInt32 attributes, const char *category,
			 char *path, int size)
{
	char userRoot[32] = {0};
	char savedata[32] = {0};
	int index, result;

	if (attributes & 1)
		goto internalSavedata;
	if (strncmp(category, "gda", 4) == 0 ||
	    strncmp(category, "gdb", 4) == 0)
		goto systemSavedata;
	if (strncmp(category, "gd", 3) != 0 &&
	    strncmp(category, "gdc", 4) != 0 &&
	    strncmp(category, "gdd", 4) != 0)
		return (int)0x8080000b;
	if (strncmp(appPath, "gro0:", 5) == 0) {
		snprintf(path, size, "grw0:savedata/%s", titleId);
		return 0;
	}
	if (strncmp(appPath, "ux0:", 4) == 0)
		goto internalSavedata;
	if (strncmp(appPath, "pd0:", 4) != 0)
		return (int)0x8080000b;

systemSavedata:
	index = 0;
	while (encodedUserRoot[index] != 0) {
		userRoot[index] = ~encodedUserRoot[index];
		if (++index == sizeof(userRoot))
			return (int)0x80800001;
	}
	userRoot[index] = 0;
	index = 0;
	while (encodedSavedataComponent[index] != 0) {
		savedata[index] = ~encodedSavedataComponent[index];
		if (++index == sizeof(savedata))
			return (int)0x80800001;
	}
	savedata[index] = 0;
	if (g_appmgrAccountDirectory[0] == 0)
		appmgrUpdateAccountDirectory();
	result = snprintf(path, size, "%s/%s/%s/%s", userRoot,
		g_appmgrAccountDirectory, savedata, titleId);
	if (result < 0 || size <= result)
		return (int)0x80800001;
	return 0;

internalSavedata:
	if (g_appmgrAccountDirectory[0] == 0)
		appmgrUpdateAccountDirectory();
	snprintf(path, size, "ux0:/user/%s/savedata/%s",
		 g_appmgrAccountDirectory, titleId);
	return 0;
}

int appmgrUpdateSaveDataParams(const char *path, const char *titleId,
			     SceUInt32 maxSize, SceUInt32 *flag)
{
	AppmgrSfoFile file = {0};
	void *data = NULL;
	SceSize length = 0, capacity = 0;
	SceAllocOpt opt;
	SceKernelOpenPsId openPsId;
	SceUInt64 currentAccountId;
	AppmgrSavedataParams *params;
	char *sfoPath;
	SceUID fd;
	int permission, accountIndex, modified, count, result;

	SceKernelSuspendForDriver_4DF40893(0);
	opt = (SceAllocOpt) { .size = sizeof(opt), .align = 32 };
	sfoPath = sceKernelAllocHeapMemoryWithOption(g_appmgrMountHeap, 292, &opt);
	if (sfoPath == NULL) {
		result = (int)0x80801006;
		goto out;
	}
	memset(sfoPath, 0, 292);
	snprintf(sfoPath, 292, "%s/%s/%s", path, "sce_sys", "param.sfo");
	result = appmgrLoadSfoFile(sfoPath, &file);
	if (result < 0)
		goto freePath;
	sceKernelFreeHeapMemory(g_appmgrMountHeap, sfoPath);
	opt = (SceAllocOpt) { .size = sizeof(opt), .align = 32 };
	params = sceKernelAllocHeapMemoryWithOption(g_appmgrMountHeap,
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
	permission = sceKernelSetThreadAccessLevel(64);
	result = sceRegMgrGetKeyBin("/CONFIG/NP", "account_id",
				 &currentAccountId, sizeof(currentAccountId));
	if (result < 0)
		goto restorePermission;
	result = sceSblAimgrGetOpenPsId(&openPsId);
	if (result < 0)
		goto restorePermission;
	result = sceRegMgrGetKeyInt("/CONFIG/NP2", "current_account2", &accountIndex);
	if (result < 0) {
		accountIndex = 0;
		result = 0;
	} else if ((unsigned int)accountIndex > 63) {
		accountIndex = 0;
	}
	sceKernelSetThreadAccessLevel(permission);
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
		if (SceSblAIMgrForDriver_D78B04A2())
			++params->cexUseCount;
		else if (SceSblAIMgrForDriver_F4B98F66())
			++params->dexUseCount;
		else {
			SceSblACMgrForDriver_E87D1777();
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
		sceKernelFreeHeapMemory(g_appmgrMountHeap, params);
		goto writeFile;
	}
	sceKernelFreeHeapMemory(g_appmgrMountHeap, params);
	if (modified == 0)
		goto out;
writeFile:
	opt = (SceAllocOpt) { .size = sizeof(opt), .align = 32 };
	sfoPath = sceKernelAllocHeapMemoryWithOption(g_appmgrMountHeap, 292, &opt);
	if (sfoPath == NULL) {
		result = (int)0x80801006;
		goto out;
	}
	memset(sfoPath, 0, 292);
	snprintf(sfoPath, 292, "%s/%s/%s", path, "sce_sys", "param.sfo");
	fd = sceIoOpen(sfoPath, 1027, 1048582);
	result = fd;
	if (fd >= 0) {
		count = sceIoWrite(fd, file.buffer, file.size);
		if (count >= 0 && (SceSize)count == file.size) {
			result = sceIoClose(fd);
			if (result >= 0) {
				result = sceIoSync(sfoPath, 0);
				if (result < 0)
					result = 0;
			}
		} else {
			result = count;
			sceIoClose(fd);
		}
	}
	goto freePath;
restorePermission:
	sceKernelSetThreadAccessLevel(permission);
	goto freeParams;
badData:
	result = (int)0x80800034;
freeParams:
	sceKernelFreeHeapMemory(g_appmgrMountHeap, params);
	goto out;
freePath:
	sceKernelFreeHeapMemory(g_appmgrMountHeap, sfoPath);
out:
	appmgrReleaseSfoFile(&file);
	SceKernelSuspendForDriver_2BB92967(0);
	return result;
}

#if FW_VERSION >= 0x04000000
int
#else
static inline __attribute__((always_inline)) int
#endif
appmgrValidateSaveDataMount(SceUID pid, const char *path, int skipVerification,
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
	if (!SceSblAIMgrForDriver_D78B04A2()) {
		if (SceSblAIMgrForDriver_F4B98F66()) {
			if (SceSblPostSsMgrForDriver_9D2E2D39())
				goto contentParams;
		} else {
			SceSblACMgrForDriver_E87D1777();
		}
	}
	result = appmgrReadKeystone(pid, driveId.drive_id, keystone);
	if (result >= 0) {
		if ((SceQafMgrForDriver_41E04800() != 1 ||
		     sceKernelCheckDipsw(DIPSW_IGNORE_KEYSTONE) == 0) &&
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

int appmgrDecodeString(const unsigned char *source, char *destination, SceSize size)
{
	int index = 0;
	unsigned char character;

	if (source == NULL || destination == NULL)
		return (int)0x80800001;
	memset(destination, 0, size);
	if (size == 0)
		return (int)0x80800001;
	character = *source;
	if (character != 0) {
		do {
			destination[index] = ~character;
			++index;
			if (index == size)
				return (int)0x80800001;
			character = *++source;
		} while (character != 0);
	}
	destination[index] = 0;
	return 0;
}

int appmgrRegisterMountDebugSettingCallback(void)
{
	return appmgrSetDebugSettingCallback(_debugSettingCallBack, NULL);
}

int appmgrIsDevelopmentPath(const char *path)
{
	return strncmp(path, "host0:", 6) == 0 ||
		strncmp(path, "ux0:data", 8) == 0 ||
		strncmp(path, "ux0:\\data", 9) == 0 ||
		strncmp(path, "ux0:/data", 9) == 0;
}

int appmgrGetVs0UserModuleDrive(char *mountPoint)
{
	if (SceSblACMgrForDriver_E79C7A8D(0, (const SceChar8 *)"vs0:sys/external"))
		return appmgrGetMountPointById(1007, mountPoint);
	return (int)0x80800009;
}

int appmgrGetVs0UserDataDrive(char *mountPoint)
{
	if (SceSblACMgrForDriver_E79C7A8D(0, (const SceChar8 *)"vs0:data/external"))
		return appmgrGetMountPointById(1008, mountPoint);
	return (int)0x80800009;
}

int appmgrConvertVs0UserDrivePath(const char *path, char *output, SceSize size)
{
	char mountPoint[16];
	SceUInt32 syscallState = sceSyscallEnter();
	SceSize capacity;
	int prefixLength;
	int result = (int)0x80800001;

	if (output == NULL || path == NULL)
		goto out;
	capacity = size < 292 ? size : 292;
	__builtin_memset(mountPoint, 0, sizeof(mountPoint));
	memset(output, 0, capacity);
	if (capacity <= 63)
		goto out;
	if (!SceSblACMgrForDriver_E79C7A8D(0, (const SceChar8 *)path)) {
		result = (int)0x80800009;
		goto out;
	}
	if (strncmp(path, "vs0:sys/external", 16) == 0 ||
	    strncmp(path, "vs0:/sys/external", 17) == 0) {
		result = appmgrGetMountPointById(1007, mountPoint);
		if (result == (int)0x80800002)
			goto unmounted;
		if (result < 0)
			goto out;
		strncpy(output, mountPoint, 64);
		prefixLength = strncmp(path, "vs0:sys/external", 16) == 0 ? 16 : 17;
		strncat(output, path + prefixLength, capacity - 16);
	} else if (strncmp(path, "vs0:data/external", 17) == 0 ||
		   strncmp(path, "vs0:/data/external", 18) == 0) {
		result = appmgrGetMountPointById(1008, mountPoint);
		if (result == (int)0x80800002)
			goto unmounted;
		if (result < 0)
			goto out;
		strncpy(output, mountPoint, 64);
		prefixLength = strncmp(path, "vs0:data/external", 17) == 0 ? 17 : 18;
		strncat(output, path + prefixLength, capacity - 16);
	} else {
		strncpy(output, path, capacity);
	}
	result = output[capacity - 1] != 0 ? (int)0x80800001 : 0;
	goto out;

unmounted:
	strncpy(output, path, capacity);
	output[capacity - 1] = 0;
	result = 0;
out:
	sceSyscallLeave(syscallState);
	return result;
}

int appmgrIsValidTitleId(const char *titleId, int mode)
{
	int systemTitle;

	if ((unsigned int)(unsigned char)titleId[0] - 'A' > 25 ||
	    (unsigned int)(unsigned char)titleId[1] - 'A' > 25 ||
	    (unsigned int)(unsigned char)titleId[2] - 'A' > 25 ||
	    (unsigned int)(unsigned char)titleId[3] - 'A' > 25 ||
	    (unsigned int)(unsigned char)titleId[4] - '0' > 9 ||
	    (unsigned int)(unsigned char)titleId[5] - '0' > 9 ||
	    (unsigned int)(unsigned char)titleId[6] - '0' > 9 ||
	    (unsigned int)(unsigned char)titleId[7] - '0' > 9 ||
	    (unsigned int)(unsigned char)titleId[8] - '0' > 9 ||
	    (signed char)titleId[9] != 0)
		return 0;
	if (mode != 0) {
		if (strncmp(titleId, "NPXS1", 5) != 0) {
			systemTitle = 0;
		} else {
			systemTitle = strncmp(titleId, "NPXS10998", 9);
			if (systemTitle != 0) {
				systemTitle = strncmp(titleId, "NPXS10999", 9);
				if (systemTitle != 0)
					systemTitle = strncmp(titleId, "NPXS108", 7) != 0;
			}
		}
		if (mode == 2)
			return systemTitle;
		if (mode == 1)
			return systemTitle ^ 1;
	}
	return 1;
}

const char appmgrProgramCategories[6][4] = {
	"gp", "gd", "gpc", "gdc", "gpd", "gdd"
};

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

int appmgrResolveLaunchPath(const char *path, char *output, SceSize size)
{
	int result;

	result = sceKernelLockMutex(g_appmgrMountMutex, 1, NULL);
	if (result >= 0) {
		if (strncmp(path, "PD", 2) != 0) {
			strncpy(output, path, size);
			output[size - 1] = 0;
		} else {
			result = appmgrResolvePfsDevicePath(path, output, size);
		}
		sceKernelUnlockMutex(g_appmgrMountMutex, 1);
	}
	return result;
}

/* Release every mount and drive overlay a process holds. */
int appmgrReleaseProcessMounts(SceUID pid)
{
	struct AppState *app;
	AppmgrVirtualMount *mount, *next;
	SceFiosOverlayID *overlayIds;
	int index, result;

	app = appmgrFindAppByProcessId(g_appmgrAppStates, pid);
	if (app == NULL)
		return 0x80801002;
	mount = app->process.mounts.firstMount;
	if (mount != NULL) {
		do {
			appmgrReleasePfsMount(pid, mount->pfsMount, 1);
			appmgrRemoveOverlay(pid, mount->overlayId);
			next = mount->next;
			sceKernelFreeHeapMemory(g_appmgrMountHeap, mount);
			mount = next;
		} while (mount != NULL);
		app->process.mounts.firstMount = NULL;
	}
	app->process.content.contentFlags &= ~0x0e000000;
	overlayIds = app->process.content.overlayIds;
	for (index = 0; index != 16; ++index) {
		if (overlayIds[index] == 0)
			continue;
		result = appmgrRemoveOverlay(pid, overlayIds[index]);
		if (result != 0)
			return result;
		overlayIds[index] = 0;
	}
	return 0;
}

int SceAppMgrForDriver_0B1BEEE0(const char *path)
{
	SceUID directory;
	int result;
#if FW_VERSION < 0x04000000
	int mounted = 0;
#endif

	if (path == NULL || strncmp(path, "ux0:", 4) != 0 ||
	    strnlen(path, 292) >= 292)
		return (int)0x80800001;
	result = sceIoDopen(path);
	if (result >= 0) {
		directory = result;
#if FW_VERSION < 0x04000000
		result = sceIoIoctl(directory, 0x1803, NULL, 0, &mounted, sizeof(mounted));
		if (result == 0)
			result = mounted;
#else
		result = 0;
#endif
		sceIoDclose(directory);
	}
	return result;
}

int appmgrCountPfsMountReferences(unsigned int mountId, int flags)
{
	SceSelfAuthInfo authInfo;
	AppmgrPfsMount *mount;
	SceUInt32 state;
	int result;

	state = sceSyscallEnter();
	result = sceKernelLockMutex(g_appmgrMountMutex, 1, NULL);
	if (result < 0)
		goto out;
	if (mountId <= 304) {
		if (mountId < 302) {
			if (mountId > 112) {
				if (mountId == 205)
					SceSblACMgrForDriver_456DA7AC(0);
				goto count;
			}
			if (mountId < 110) {
				if (mountId - 106 <= 1 &&
				    !SceSblACMgrForDriver_C98D82EE(0))
					sceSblACMgrIsSystemProgram(0);
				goto count;
			}
		}
		goto checkAuth;
	}
	if (mountId == 600) {
		SceSblACMgrForDriver_4DB7F512(0);
	} else if (mountId > 600) {
		if (mountId - 900 <= 1)
			goto checkAuth;
	} else if (mountId - 500 <= 5) {
		goto checkAuth;
	}
	goto count;
checkAuth:
	if (sceKernelGetProcessSelfAuthInfo(0, &authInfo) >= 0) {
		if (mountId == 111)
			SceSblACMgrForDriver_F5AE24AC(0);
		else if (mountId == 112)
			SceSblACMgrForDriver_6C5AB07F(0);
	}
count:
	result = (int)0x80800001;
	if (flags == 0) {
		mount = g_appmgrPfsMounts;
		while (mount != NULL && mount->mountId != mountId)
			mount = mount->next;
		result = 0;
		if (mount != NULL) {
			result = mount->programAuthorityIds[0] != 0;
			if (mount->programAuthorityIds[1] != 0)
				++result;
			if (mount->programAuthorityIds[2] != 0)
				++result;
			if (mount->programAuthorityIds[3] != 0)
				++result;
			if (mount->programAuthorityIds[4] != 0)
				++result;
			if (mount->programAuthorityIds[5] != 0)
				++result;
			if (mount->programAuthorityIds[6] != 0)
				++result;
			if (mount->programAuthorityIds[7] != 0)
				++result;
			if (mount->programAuthorityIds[8] != 0)
				++result;
			if (mount->programAuthorityIds[9] != 0)
				++result;
			if (mount->programAuthorityIds[10] != 0)
				++result;
			if (mount->programAuthorityIds[11] != 0)
				++result;
			if (mount->programAuthorityIds[12] != 0)
				++result;
			if (mount->programAuthorityIds[13] != 0)
				++result;
			if (mount->programAuthorityIds[14] != 0)
				++result;
			if (mount->programAuthorityIds[15] != 0)
				++result;
		}
	}
	sceKernelUnlockMutex(g_appmgrMountMutex, 1);
out:
	sceSyscallLeave(state);
	return result;
}

int sceAppMgrRdrfpUmount(const SceAppMgrRdrfpMountPoint *mountPoint)
{
	union {
		SceSelfAuthInfo authInfo;
		SceAppMgrRdrfpMountPoint mountPoint;
	} work;
	AppmgrPfsMount *mount, *cursor, *previous;
	SceUInt32 state;
	int result;

	state = sceSyscallEnter();
	result = sceKernelLockMutex(g_appmgrMountMutex, 1, NULL);
	if (result < 0)
		goto out;
	if (sceKernelGetProcessSelfAuthInfo(0, &work.authInfo) < 0 ||
	    work.authInfo.programAuthorityId != 0x2808000000000001ULL) {
		result = (int)0x80800009;
		goto unlock;
	}
	if (mountPoint == NULL) {
		result = (int)0x80800001;
		goto unlock;
	}
	work.mountPoint = *mountPoint;
	work.mountPoint.path[13] = 0;
	mount = g_appmgrPfsMounts;
	while (mount != NULL &&
	       strncmp(mount->mountPoint.drive_id, work.mountPoint.path, 16) != 0)
		mount = mount->next;
	result = (int)0x80800002;
	if (mount == NULL)
		goto unlock;
	result = ScePfsMgrForKernel_680BC384(mount->mountPoint.drive_id);
	if (result < 0 && result != (int)0x80010013)
		goto unlock;
	cursor = g_appmgrPfsMounts;
	result = (int)0x80800002;
	if (cursor == NULL)
		goto unlock;
	if (cursor == mount) {
		g_appmgrPfsMounts = mount->next;
		if (mount->next != NULL)
			mount->next->prev = NULL;
	} else {
		do {
			cursor = cursor->next;
			if (cursor == NULL)
				goto unlock;
		} while (cursor != mount);
		previous = mount->prev;
		previous->next = mount->next;
		if (mount->next != NULL)
			mount->next->prev = previous;
	}
	sceKernelFreeHeapMemory(g_appmgrMountHeap, mount);
	result = 0;
unlock:
	sceKernelUnlockMutex(g_appmgrMountMutex, 1);
out:
	sceSyscallLeave(state);
	return result;
}

int appmgrSetPfsAcidDirectory(const char *path, const char *directory)
{
	SceSelfAuthInfo authInfo;
	SceUInt32 state;
	SceUID pid;
	char *resolvedPath;
	int result, permission;

	state = sceSyscallEnter();
	result = sceKernelLockMutex(g_appmgrMountMutex, 1, NULL);
	if (result < 0)
		goto out;
	result = sceKernelGetProcessSelfAuthInfo(0, &authInfo);
	if (result < 0)
		goto unlock;
	if (authInfo.programAuthorityId != 0x2800000000000001ULL) {
		result = (int)0x80800009;
		goto unlock;
	}
	if (directory == NULL || path == NULL) {
		result = (int)0x80800001;
		goto unlock;
	}
	resolvedPath = appmgrAllocMountPath();
	if (resolvedPath == NULL) {
		result = (int)0x80801006;
		goto unlock;
	}
	pid = sceKernelGetProcessId();
	permission = sceKernelSetThreadAccessLevel(128);
	result = SceFios2KernelForDriver_0F456345(pid, 0, path, resolvedPath, 292);
	sceKernelSetThreadAccessLevel(permission);
	if (result >= 0) {
		resolvedPath[15] = 0;
		result = ScePfsMgrForKernel_B0CC0A1A(resolvedPath, directory);
	}
	sceKernelFreeHeapMemory(g_appmgrMountHeap, resolvedPath);
unlock:
	sceKernelUnlockMutex(g_appmgrMountMutex, 1);
out:
	sceSyscallLeave(state);
	return result;
}

/* Unmount the PFS ACID directory of a drive of the calling application. */
int appmgrUnmountPfsAcidDirectory(const char *drive, const char *directory)
{
	char path[64] = "";
	char driveName[33];
	SceAllocOpt opt;
	struct AppState *app;
	char *resolvedPath;
	SceUID pid;
	int index, permission, result;

	if (directory == NULL || drive == NULL)
		return 0x80800001;
	__builtin_memset(driveName, 0, sizeof(driveName));
	memset(path, 0, sizeof(path));
	app = appmgrFindAppByProcessId(g_appmgrAppStates, sceKernelGetProcessId());
	if (app == NULL)
		return 0x80802012;
	snprintf(path, sizeof(path), "%s%s", drive, directory);
	opt.size = sizeof(opt);
	opt.mmapKind = 0;
	opt.align = 32;
	opt.mmapBase = NULL;
	opt.mmapSize = 0;
	resolvedPath = sceKernelAllocHeapMemoryWithOption(g_appmgrMountHeap, 292, &opt);
	if (resolvedPath == NULL)
		return 0x80801006;
	memset(resolvedPath, 0, 292);
	pid = sceKernelGetProcessId();
	permission = sceKernelSetThreadAccessLevel(128);
	result = SceFios2KernelForDriver_0F456345(pid, 0, path, resolvedPath, 292);
	sceKernelSetThreadAccessLevel(permission);
	if (result < 0)
		goto free;
	__builtin_memset(driveName, 0, sizeof(driveName));
	for (index = 0; ; ) {
		driveName[index] = resolvedPath[index];
		if (resolvedPath[index] == ':')
			break;
		if (++index == 33) {
			result = 0x8080000c;
			driveName[32] = '\0';
			goto free;
		}
	}
	driveName[15] = '\0';
	sceKernelFreeHeapMemory(g_appmgrMountHeap, resolvedPath);
	if ((app->process.content.contentFlags & 0x10) != 0 &&
	    appmgrGetFakeNoMemoryCard() == 2)
		return 0x80800002;
	return ScePfsMgrForKernel_00C8AF80(driveName, directory);

free:
	sceKernelFreeHeapMemory(g_appmgrMountHeap, resolvedPath);
	return result;
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
		status = sceKernelGetProcessSelfAuthInfo(pid, &authInfo);
		if (status < 0)
			goto freeVirtualMount;
		authorityId = authInfo.programAuthorityId;
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
			status = sceKernelGetProcessSelfAuthInfo(pid, &authInfo);
			if (status < 0)
				goto freeVirtualMount;
			if (authInfo.programAuthorityId == 0x2800000000000001ULL ||
			    authInfo.programAuthorityId == 0x2800000000000010ULL ||
			    authInfo.programAuthorityId == 0x280000000000002dULL ||
			    (pfsMount->mountId == 1005 &&
			     authInfo.programAuthorityId == 0x2800000000000039ULL))
				goto freeVirtualMount;
			ScePfsMgrForKernel_2D67D8CA(pfsMount->mountPoint.drive_id,
					  authInfo.programAuthorityId);
			goto freeVirtualMount;
		}
		if (pfsMount->mountId == 600) {
			permission = sceKernelSetThreadAccessLevel(128);
			status = SceIofilemgrForDriver_20574100(0x10000, 1, 0, 0);
			sceKernelSetThreadAccessLevel(permission);
			if (status < 0)
				goto restoreAuthority;
		} else if (pfsMount->mountPoint.drive_id[0] != 0) {
			permission = sceKernelSetThreadAccessLevel(128);
			status = ScePfsMgrForKernel_680BC384(pfsMount->mountPoint.drive_id);
			sceKernelSetThreadAccessLevel(permission);
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
		sceKernelFreeHeapMemory(g_appmgrMountHeap, pfsMount);
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
		sceKernelFreeHeapMemory(g_appmgrMountHeap, mount);
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
		permission = sceKernelSetThreadAccessLevel(128);
		result = SceFios2KernelForDriver_23247EFB(targetPid, overlayId);
		sceKernelSetThreadAccessLevel(permission);
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
		sceKernelFreeHeapMemory(g_appmgrMountHeap, mount);
		removed = 1;
	}
finished:
	return removed ? 0 : (int)0x80800002;
}

/* The calling process unmounts one of its drives. */
int SceAppMgrForDriver_A714BB35(const char *driveId)
{
	SceUInt32 syscallState = sceSyscallEnter();
	struct AppState *app;
	int result, mountId;

	if (driveId == NULL) {
		result = 0x80800001;
		goto out;
	}
	result = sceKernelLockMutex(g_appmgrMountMutex, 1, NULL);
	if (result < 0)
		goto out;
	app = appmgrFindAppByProcessId(g_appmgrAppStates, sceKernelGetProcessId());
	if (app == NULL) {
		result = 0x80801002;
		goto unlock;
	}
	mountId = 0;
	if ((app->process.content.contentFlags & 0x30) != 0) {
		if (strncmp(driveId, "photo0:", 8) == 0)
			mountId = 100;
		else if (strncmp(driveId, "music0:", 8) == 0)
			mountId = 105;
		else if (strncmp(driveId, "video0:", 8) == 0)
			mountId = 109;
		if (mountId != 0) {
			result = 0x80800002;
			if (appmgrGetFakeNoMemoryCard() != 0)
				goto unlock;
		}
	}
	result = appmgrUnmountDrive(sceKernelGetProcessId(), &app->process.mounts, driveId, 0);
	if (driveId[0] == 'a' && (strncmp(driveId, "addcont0:", 10) == 0 ||
	    strncmp(driveId, "addcont1:", 10) == 0))
		appmgrRemoveAddcontMountEntries(app, driveId);
	if (result == 0 && (app->process.content.contentFlags & 0x30) != 0) {
		switch (mountId) {
		case 100:
			app->process.content.contentFlags &= ~0x02000000;
			break;
		case 105:
			app->process.content.contentFlags &= ~0x04000000;
			break;
		case 109:
			app->process.content.contentFlags &= ~0x08000000;
			break;
		}
	}
unlock:
	sceKernelUnlockMutex(g_appmgrMountMutex, 1);
out:
	sceSyscallLeave(syscallState);
	return result;
}

/* Unmount a drive of another process; game programs may unmount only their own. */
int sceAppMgrUmountByPid(SceUID pid, const char *driveId)
{
	SceUInt32 syscallState = sceSyscallEnter();
	struct AppState *app;
	SceUInt32 contentFlags;
	int result;

	if (driveId == NULL) {
		result = 0x80800001;
		goto out;
	}
	result = sceKernelLockMutex(g_appmgrMountMutex, 1, NULL);
	if (result < 0)
		goto out;
	if (pid != 0 && SceSblACMgrForDriver_8A54DF3A(0) != 0 &&
	    pid != sceKernelGetProcessId()) {
		result = 0x80800009;
		goto unlock;
	}
	app = appmgrFindAppByProcessId(g_appmgrAppStates, pid);
	if (app == NULL) {
		result = 0x80801002;
		goto unlock;
	}
	result = appmgrUnmountDrive(pid, &app->process.mounts, driveId, 0);
	if (result == 0) {
		contentFlags = app->process.content.contentFlags;
		if ((contentFlags & 0x30) != 0) {
			if (strncmp(driveId, "photo0:", 8) == 0)
				app->process.content.contentFlags = contentFlags & ~0x02000000;
			else if (strncmp(driveId, "music0:", 8) == 0)
				app->process.content.contentFlags = contentFlags & ~0x04000000;
			else if (strncmp(driveId, "video0:", 8) == 0)
				app->process.content.contentFlags = contentFlags & ~0x08000000;
		}
	}
unlock:
	sceKernelUnlockMutex(g_appmgrMountMutex, 1);
out:
	sceSyscallLeave(syscallState);
	return result;
}

/* Unmount the QA drive (mount ID 600) from every process that has it. */
int sceAppMgrForceUmount(int mountId)
{
	SceUInt32 syscallState = sceSyscallEnter();
	struct AppState *app;
	AppmgrVirtualMount *mount;
	SceUID pid;
	int result;

	result = sceKernelLockMutex(g_appmgrMountMutex, 1, NULL);
	if (result < 0)
		goto out;
	MOUNT_MAIN_LOCK(8420);
	if (mountId != 600) {
		result = 0x80800001;
		goto unlock;
	}
	if (SceSblACMgrForDriver_D0E11C89(0) == 0) {
		result = 0x80800009;
		goto unlock;
	}
	for (app = g_appmgrAppStates; app != &g_appmgrAppStates[20]; app++) {
		pid = app->process.processId;
		if (pid < 0 || (pid & 1) == 0)
			continue;
		for (mount = app->process.mounts.firstMount; mount != NULL; mount = mount->next) {
			if (mount->pfsMount != NULL && mount->pfsMount->mountId == 600)
				break;
		}
		if (mount == NULL) {
			result = 0x80800002;
			continue;
		}
		result = appmgrUnmountDrive(pid, &app->process.mounts,
					    mount->driveId.drive_id, 1);
		if (result != 0)
			break;
	}
unlock:
	MOUNT_MAIN_UNLOCK(8460);
	sceKernelUnlockMutex(g_appmgrMountMutex, 1);
out:
	sceSyscallLeave(syscallState);
	return result;
}

int appmgrCleanupProcessMount(struct AppmgrMountHolder *holder,
			      AppmgrProcessMountCleanup *cleanup)
{
	int result = (signed char)cleanup->driveId[0];

	if (result != 0)
		result = appmgrUnmountDrive(sceKernelGetProcessId(), holder,
					    cleanup->driveId, 0);
	if (cleanup->resolvedPath)
		sceKernelFreeHeapMemory(g_appmgrMountHeap, cleanup->resolvedPath);
	if (cleanup->mapFile)
		sceKernelFreeHeapMemory(g_appmgrMountHeap, cleanup->mapFile);
	memset(cleanup, 0, sizeof(*cleanup));
	return result;
}

int appmgrGetAccountIdentity(SceUInt64 *accountId, SceKernelOpenPsId *openPsId,
			    SceUInt32 *currentAccount)
{
	int permission, result = 0;

	permission = sceKernelSetThreadAccessLevel(64);
	if (accountId != NULL) {
		result = sceRegMgrGetKeyBin("/CONFIG/NP", "account_id", accountId, 8);
		if (result < 0)
			goto out;
	}
	if (openPsId != NULL) {
		result = sceSblAimgrGetOpenPsId(openPsId);
		if (result < 0)
			goto out;
	}
	if (currentAccount != NULL) {
		result = sceRegMgrGetKeyInt("/CONFIG/NP2", "current_account2",
					  (int *)currentAccount);
		if (result < 0) {
			result = 0;
			*currentAccount = 0;
		} else if (*currentAccount > 63) {
			*currentAccount = 0;
		}
	}
out:
	sceKernelSetThreadAccessLevel(permission);
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
	SceKernelOpenPsId openPsId;
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
	permission = sceKernelSetThreadAccessLevel(128);
	SceKernelSuspendForDriver_4DF40893(0);
	opt = (SceAllocOpt) { .size = sizeof(opt), .align = 32 };
	metadataPath = sceKernelAllocHeapMemoryWithOption(g_appmgrMountHeap, 292, &opt);
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
		result = sceIoMkdir(metadataPath, 6);
		if (result != (int)0x80010011 && result != 0)
			goto freePath;
		if (mountId == 1006) {
			mode = 1048582;
			goto buildSfo;
		}
	} else {
		if (mountId == 1006) {
			result = sceIoMkdir(metadataPath, 390);
			if (result != 0 && result != (int)0x80010011)
				goto freePath;
			mode = 1048966;
			goto buildSfo;
		}
		result = sceIoMkdir(metadataPath, 6);
		if (result != 0 && result != (int)0x80010011)
			goto freePath;
	}
	if (sealedKey != NULL) {
		snprintf(metadataPath, 292, "%s/sce_sys/sealedkey", path);
		fd = sceIoOpen(metadataPath, 1539, 1048582);
		result = fd;
		if (fd < 0)
			goto freePath;
		count = sceIoWrite(fd, sealedKey, 80);
		if (count != 80)
			goto closeError;
		count = sceIoChstatByFd(fd, &stat, 1);
		if (count < 0)
			goto closeError;
		count = sceIoSyncByFd(fd, 0);
		if (count < 0)
			goto closeError;
		result = sceIoClose(fd);
		if (result < 0)
			goto freePath;
	}
	if (mountId == 1005 || mountId - 500 <= 1) {
		if (keystone == NULL) {
			result = (int)0x80800001;
			goto freePath;
		}
		snprintf(metadataPath, 292, "%s/sce_sys/keystone", path);
		fd = sceIoOpen(metadataPath, 1539, 6);
		result = fd;
		if (fd < 0)
			goto freePath;
		count = sceIoWrite(fd, keystone, 96);
		if (count != 96)
			goto closeError;
		count = sceIoChstatByFd(fd, &stat, 1);
		if (count < 0)
			goto closeError;
		count = sceIoSyncByFd(fd, 0);
		if (count < 0)
			goto closeError;
		result = sceIoClose(fd);
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
	accountPermission = sceKernelSetThreadAccessLevel(64);
	result = sceRegMgrGetKeyBin("/CONFIG/NP", "account_id", &accountId, 8);
	if (result >= 0)
		result = sceSblAimgrGetOpenPsId(&openPsId);
	if (result < 0) {
		sceKernelSetThreadAccessLevel(accountPermission);
		goto freePath;
	}
	if (sceRegMgrGetKeyInt("/CONFIG/NP2", "current_account2",
			       (int *)&accountIndex) < 0 || accountIndex > 63)
		accountIndex = 0;
	sceKernelSetThreadAccessLevel(accountPermission);
#endif
	input = (AppmgrSavedataSfoParam) {0};
	opt = (SceAllocOpt) { .size = sizeof(opt), .align = 32 };
	params = sceKernelAllocHeapMemoryWithOption(g_appmgrMountHeap,
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
		fd = sceIoOpen(metadataPath, 1539, mode);
		result = fd;
		if (fd >= 0) {
			count = sceIoWrite(fd, file.buffer, file.size);
			if (count >= 0 && (SceSize)count == file.size) {
				count = sceIoSyncByFd(fd, 0);
				if (count >= 0) {
					result = sceIoClose(fd);
					if (result >= 0) {
						sceKernelFreeHeapMemory(g_appmgrMountHeap, params);
						result = appmgrCreateSaveDataSlotFile(pid, path, 256);
						goto freePath;
					}
				} else {
					result = count;
					sceIoClose(fd);
				}
			} else {
				result = count;
				sceIoClose(fd);
			}
		}
	}
	sceKernelFreeHeapMemory(g_appmgrMountHeap, params);
	goto freePath;
closeError:
	result = count;
	sceIoClose(fd);
freePath:
	sceKernelFreeHeapMemory(g_appmgrMountHeap, metadataPath);
out:
	appmgrReleaseSfoFile(&file);
	SceKernelSuspendForDriver_2BB92967(0);
	sceKernelSetThreadAccessLevel(permission);
	return result;
}

int appmgrCreateDirectories(const char *path, int create)
{
	SceIoStat stat;
	SceSize cursor = 0, end;
	int mode, permission, result;

	if (strncmp(path, "host0:", 6) != 0 &&
	    strncmp(path, "ux0:data", 8) != 0 &&
	    strncmp(path, "ux0:\\data", 9) != 0)
		mode = strncmp(path, "ux0:/data", 9) != 0 ? 6 : 390;
	else
		mode = 390;
	memset(g_appmgrDirectoryPath, 0, sizeof(g_appmgrDirectoryPath));
	result = sceIoGetstat(path, &stat);
	if (create == 0 || result == 0)
		return result;
	for (;;) {
		end = cursor;
		if (cursor == 0) {
			while (path[end] != ':' && path[end] != '/' && path[end] != 0)
				++end;
			if (path[end] == ':' && path[end + 1] == '/')
				end += 2;
		}
		if (path[end] != 0 && path[end] != '/') {
			do {
				++end;
			} while (path[end] != 0 && path[end] != '/');
		}
		if (cursor == end)
			return 0;
#if FW_VERSION >= 0x04000000
		__strncpy_chk(g_appmgrDirectoryPath, path, end,
			      sizeof(g_appmgrDirectoryPath));
#else
		strncpy(g_appmgrDirectoryPath, path, end);
#endif
		g_appmgrDirectoryPath[end] = 0;
		cursor = end + 1;
		if (path[end] == 0)
			cursor = end;
		permission = sceKernelSetThreadAccessLevel(128);
		result = sceIoMkdir(g_appmgrDirectoryPath, mode);
		sceKernelSetThreadAccessLevel(permission);
		if (result != (int)0x80010011 && result != 0)
			return result;
	}
}

int appmgrReadDirectoryLicense(const char *directory, AppmgrLicenseInfo *info)
{
	char rifName[48], startTime[36], expiryTime[32], path[128];
	int cardPath, permission, result, attempt;

	cardPath = strncmp(directory, "gro0:", 5);
	permission = sceKernelSetThreadAccessLevel(64);
	if (appmgrCreateDirectories(directory, 0) < 0) {
		result = (int)0x80800004;
		goto out;
	}
	if (cardPath != 0) {
		for (attempt = 0; attempt != 2; ++attempt) {
			if (attempt == 1) {
				result = SceNpDrmForDriver_5D73448C(rifName, 0);
				if (result < 0)
					goto out;
			} else {
				result = SceNpDrmForDriver_DF62F3B8(rifName, 0);
				if (result < 0) {
					if (result != (int)0x8087000c)
						goto out;
					continue;
				}
			}
			memset(path, 0, sizeof(path));
			snprintf(path, sizeof(path), "%s/%s", directory, rifName);
			result = appmgrReadRif(path, info);
			if (result != (int)0x80800004)
				goto formatted;
		}
	} else {
		for (attempt = 0; attempt != 2; ++attempt) {
			if (attempt == 1) {
				result = SceNpDrmForDriver_5D73448C(rifName, 0);
				if (result < 0)
					goto out;
			} else {
				result = SceNpDrmForDriver_5D73448C(rifName, 1);
				if (result < 0) {
					if (result != (int)0x8087000c)
						goto out;
					continue;
				}
			}
			memset(path, 0, sizeof(path));
			snprintf(path, sizeof(path), "%s/%s", directory, rifName);
			result = appmgrReadRif(path, info);
			if (result != (int)0x80800004)
				goto formatted;
		}
	}
	goto out;

formatted:
	if (result >= 0) {
		if (info->licStartTime == 0) {
			startTime[0] = '-';
			startTime[1] = 0;
		} else if (SceRtcForDriver_D32AC698(startTime,
				(SceRtcTick *)&info->licStartTime, 540) >= 0) {
			startTime[10] = ' ';
			__builtin_memcpy(startTime + 19, " JST", 5);
		}
		if (info->licExpTime == 0) {
			expiryTime[0] = '-';
			expiryTime[1] = 0;
		} else if (SceRtcForDriver_D32AC698(expiryTime,
				(SceRtcTick *)&info->licExpTime, 540) >= 0) {
			__builtin_memcpy(expiryTime + 19, " JST", 5);
		}
	}
out:
	sceKernelSetThreadAccessLevel(permission);
	return result;
}

int appmgrReadApplicationLicense(const char *path, const char *titleId,
				 AppmgrLicenseInfo *info)
{
	char mountPoint[16] = { 0 }, directory[128];
	unsigned int index;
	int result;

	if ((mountPoint[0] = path[0]) == ':' ||
	    (mountPoint[1] = path[1]) == ':' ||
	    (mountPoint[2] = path[2]) == ':' ||
	    (mountPoint[3] = path[3]) == ':' ||
	    (mountPoint[4] = path[4]) == ':' ||
	    (mountPoint[5] = path[5]) == ':' ||
	    (mountPoint[6] = path[6]) == ':' ||
	    (mountPoint[7] = path[7]) == ':' ||
	    (mountPoint[8] = path[8]) == ':' ||
	    (mountPoint[9] = path[9]) == ':' ||
	    (mountPoint[10] = path[10]) == ':' ||
	    (mountPoint[11] = path[11]) == ':' ||
	    (mountPoint[12] = path[12]) == ':' ||
	    (mountPoint[13] = path[13]) == ':' ||
	    (mountPoint[14] = path[14]) == ':' ||
	    (mountPoint[15] = path[15]) == ':')
		goto deviceFound;
	return (int)0x8080000c;

deviceFound:
	__builtin_memset(g_appmgrLicenseDirectory, 0,
			 sizeof(g_appmgrLicenseDirectory));
	index = 0;
	while (encodedLicenseDirectory[index] != 0) {
		g_appmgrLicenseDirectory[index] = ~encodedLicenseDirectory[index];
		if (++index == sizeof(g_appmgrLicenseDirectory))
			return (int)0x80800001;
	}
	g_appmgrLicenseDirectory[index] = 0;
	if ((unsigned int)snprintf(directory, sizeof(directory), "%s/%s/%s",
				   mountPoint, g_appmgrLicenseDirectory, titleId) > 127)
		return (int)0x80800001;
	result = appmgrReadDirectoryLicense(directory, info);
	return result & (result >> 31);
}

int appmgrCheckRifGD(const char *path, AppmgrLicenseInfo *info)
{
	SceUInt32 syscallState = sceSyscallEnter();
	SceRtcTick currentTick;
	char titleId[16];
	const char *slash, *backslash, *colon;
	int result = (int)0x80800001;

	if (info == NULL || path == NULL)
		goto out;
	result = sceKernelLockMutex(g_appmgrMountMutex, 1, NULL);
	if (result < 0)
		goto out;
	memset(info, 0, sizeof(*info));
	__builtin_memset(titleId, 0, sizeof(titleId));
	slash = strrchr(path, '/');
	backslash = strrchr(path, '\\');
	colon = strrchr(path, ':');
	if (slash <= backslash) {
		if (backslash > colon)
			slash = backslash;
		else if (colon != NULL)
			slash = (const char *)((uintptr_t)colon + 1);
		else
			goto invalidPath;
	}
	if (slash == NULL)
		goto invalidPath;
	strncpy(titleId, slash + 1, 10);
	titleId[9] = 0;
	result = appmgrReadApplicationLicense(path, titleId, info);
	if (result >= 0) {
		result = SceRtcForDriver_401C0954(&currentTick);
		if (result < 0) {
			if ((info->flags & 2) == 0 &&
			    info->licExpTime == 0x7fffffffffffffffULL)
				result = 0;
		} else if (currentTick.tick >= info->licStartTime) {
			result = (int)0x80800011;
			if (info->licExpTime >= currentTick.tick)
				result = 0;
		} else {
			result = (int)0x80800018;
		}
	}
	goto unlock;

invalidPath:
	result = (int)0x8080000c;
unlock:
	sceKernelUnlockMutex(g_appmgrMountMutex, 1);
out:
	sceSyscallLeave(syscallState);
	return result;
}

/* An application directory is PFS content when it has sce_pfs or a signed sce_sys. */
/* Re-reads the license of the running big application with this title ID. */
int sceAppMgrUpdateRifInfo(const char *titleId)
{
	struct AppState *app;
	SceUInt32 syscallState = sceSyscallEnter();
	int result;

	if (titleId == NULL) {
		result = (int)0x80800009;
		goto leave;
	}
	result = sceKernelLockMutex(g_appmgrMountMutex, 1, NULL);
	if (result < 0)
		goto leave;
	MOUNT_MAIN_LOCK(12885);
	app = _appListGetBigApp(g_appmgrAppStates);
	if (app == NULL) {
		result = 0;
	} else if ((app->process.content.contentFlags & 0x10) != 0 &&
		   strncmp(app->process.content.titleId, titleId, 16) == 0) {
		result = appmgrCheckRifUpdatable(&app->process.content);
		if (result >= 0)
			result = appmgrReadApplicationLicense(app->process.content.appDirectory,
							      titleId, &app->process.content.license);
	}
	MOUNT_MAIN_UNLOCK(12912);
	sceKernelUnlockMutex(g_appmgrMountMutex, 1);
leave:
	sceSyscallLeave(syscallState);
	return result;
}

int appmgrIsPfsContent(const char *path)
{
	SceAllocOpt opt;
	SceIoStat stat;
	char *buffer;

	if (strncmp(path, "host0:", 6) == 0 || strncmp(path, "ux0:data", 8) == 0 ||
	    strncmp(path, "ux0:\\data", 9) == 0 || strncmp(path, "ux0:/data", 9) == 0 ||
	    strncmp(path, "vs0:", 4) == 0)
		return 0;
	opt.size = sizeof(opt);
	opt.mmapKind = 0;
	opt.align = 32;
	opt.mmapBase = NULL;
	opt.mmapSize = 0;
	buffer = sceKernelAllocHeapMemoryWithOption(g_appmgrMountHeap, 292, &opt);
	if (buffer == NULL)
		return 0;
	snprintf(buffer, 292, "%s/sce_pfs", path);
	if (appmgrCreateDirectories(buffer, 0) < 0) {
		snprintf(buffer, 292, "%s/sce_sys/clearsign", path);
		if (sceIoGetstat(buffer, &stat) < 0) {
			sceKernelFreeHeapMemory(g_appmgrMountHeap, buffer);
			return 0;
		}
	}
	sceKernelFreeHeapMemory(g_appmgrMountHeap, buffer);
	return 1;
}

/*
 * Mount a drive for a process: the drive name comes from the caller or the mount ID, a PFS
 * mount of the path is shared with every process that mounts it, and the process gets a
 * virtual mount of it in its holder.
 */
int appmgrMount(SceUID pid, AppmgrMountHolder *holder, unsigned int mountId,
		const char *titleId, const char *path, const ScePfsRndDriveId *driveId,
		const ScePfsRndDriveId *klicensee, ScePfsRndDriveId *mountPoint)
{
	ScePfsRndDriveId drive = {{0}};
	ScePfsRndDriveId key;
	SceSelfAuthInfo authInfo;
	SceAllocOpt opt;
	AppmgrVirtualMount *mount, *found;
	AppmgrPfsMount *pfsMount;
	SceUInt64 authorityId;
	char *normalized;
	int result, release, permission, mode, index;
	SceUID target;

	if (mountPoint == NULL)
		return (int)0x80800001;
	if (driveId != NULL) {
		drive = *driveId;
	} else {
		switch (mountId) {
		case 100 ... 112:
		case 400 ... 402:
		case 500:
		case 501:
			result = appmgrMakeRandomDriveId("ad", drive.drive_id);
			if (result != 0)
				return result;
			drive.drive_id[14] = ':';
			drive.drive_id[15] = '\0';
			break;
		case 200:
		case 201:
		case 203 ... 204:
		case 206:
		case 207:
			result = appmgrMakeRandomDriveId("td", drive.drive_id);
			if (result != 0)
				return result;
			drive.drive_id[14] = ':';
			drive.drive_id[15] = '\0';
			break;
		case 202:
			if (SceSblACMgrForDriver_0B6E6CD7(0) != 0) {
				strcpy(drive.drive_id, "ms0:");
				break;
			}
			result = appmgrMakeRandomDriveId("ms", drive.drive_id);
			if (result != 0)
				return result;
			strcpy(&drive.drive_id[14], ":");
			break;
		case 205:
			strcpy(drive.drive_id, "cache0:");
			break;
		case 302:
			strcpy(drive.drive_id, "trophy_sys0:");
			break;
		case 303:
			strcpy(drive.drive_id, "trophy_dat0:");
			break;
		case 304:
			strcpy(drive.drive_id, "trophy_dbk0:");
			break;
		case 504:
			strcpy(drive.drive_id, "sdimport0:");
			break;
		case 505:
			strcpy(drive.drive_id, "sdimport_tmp0:");
			break;
		case 600:
			result = appmgrMakeRandomDriveId("lm", drive.drive_id);
			if (result != 0)
				return result;
			strcpy(&drive.drive_id[14], ":");
			break;
		case 1000:
		case 1001:
			snprintf(drive.drive_id, sizeof(drive), "app%1u:", 0);
			break;
		case 1002:
		case 1003:
			snprintf(drive.drive_id, sizeof(drive), "addcont%1u:", 0);
			break;
		case 1005:
		case 1006:
			snprintf(drive.drive_id, sizeof(drive), "savedata%1u:", 0);
			break;
		case 1007:
		case 1008:
			result = appmgrMakeRandomDriveId("sd", drive.drive_id);
			if (result != 0)
				return result;
			strcpy(&drive.drive_id[14], ":");
			break;
		case 1009:
			result = appmgrMakeRandomDriveId("ud", drive.drive_id);
			if (result != 0)
				return result;
			strcpy(&drive.drive_id[14], ":");
			break;
		default:
			return (int)0x80800001;
		}
	}

	/* the process has the drive already */
	for (mount = holder->firstMount; mount != NULL; mount = mount->next) {
		if (mountId >= 1000) {
			if (mount->pfsMount->mountId == mountId &&
			    strncmp(mount->driveId.drive_id, drive.drive_id, 16) == 0)
				break;
		} else if (mount->pfsMount->mountId == mountId) {
			break;
		}
	}
	if (mount != NULL) {
		*mountPoint = mount->driveId;
		if (driveId != NULL) {
			for (found = holder->firstMount; found != NULL; found = found->next)
				if (found->pfsMount != NULL &&
				    found->pfsMount->mountId == mountId &&
				    strncmp(found->driveId.drive_id, drive.drive_id, 16) == 0)
					break;
		} else {
			for (found = holder->firstMount; found != NULL; found = found->next)
				if (found->pfsMount != NULL &&
				    found->pfsMount->mountId == mountId)
					break;
		}
		if (found == NULL)
			return (int)0x80800002;
		pfsMount = found->pfsMount;
		if (pfsMount->mountPoint.drive_id[0] != '\0') {
			result = sceKernelGetProcessSelfAuthInfo(pid, &authInfo);
			if (result < 0)
				return result;
			result = ScePfsMgrForKernel_D8D0FEE5(pfsMount->mountPoint.drive_id,
							     authInfo.programAuthorityId);
			if (result != (int)0x80010011 && result < 0)
				return result;
		}
		return (int)0x80800003;
	}

	opt.size = sizeof(opt);
	opt.mmapKind = 0;
	opt.align = 32;
	opt.mmapBase = NULL;
	opt.mmapSize = 0;
	mount = sceKernelAllocHeapMemoryWithOption(g_appmgrMountHeap, sizeof(*mount), &opt);
	if (mount == NULL)
		return (int)0x80801006;
	memset(mount, 0, sizeof(*mount));
	mount->driveId = drive;
	opt.size = sizeof(opt);
	opt.mmapKind = 0;
	opt.align = 32;
	opt.mmapBase = NULL;
	opt.mmapSize = 0;
	normalized = sceKernelAllocHeapMemoryWithOption(g_appmgrMountHeap, 292, &opt);
	if (normalized == NULL) {
		result = (int)0x80801006;
		goto cleanup;
	}
	memset(normalized, 0, 292);
	result = appmgrNormalizePath(path, normalized);
	if (result > 0)
		goto cleanup;
	if (strnlen(normalized, 292) >= 292) {
		result = (int)0x80800001;
		goto cleanup;
	}
	result = sceKernelGetProcessSelfAuthInfo(pid, &authInfo);
	if (result < 0) {
		release = result != (int)0x80800003;
		goto checked;
	}
	authorityId = authInfo.programAuthorityId;

	/* another process has the path mounted: share its PFS mount */
	for (pfsMount = g_appmgrPfsMounts; pfsMount != NULL; pfsMount = pfsMount->next)
		if (pfsMount->mountId == mountId &&
		    strncmp(pfsMount->appPath, normalized, 292) == 0)
			break;
	if (pfsMount != NULL) {
		for (index = 0; index < 16; index++)
			if (pfsMount->programAuthorityIds[index] == 0)
				break;
		if (index == 16) {
			result = (int)0x8080001e;
			goto cleanup;
		}
		pfsMount->programAuthorityIds[index] = authorityId;
		if (pfsMount->mountPoint.drive_id[0] == '\0' ||
		    ((result = sceKernelGetProcessSelfAuthInfo(pid, &authInfo)) >= 0 &&
		     ((result = ScePfsMgrForKernel_D8D0FEE5(pfsMount->mountPoint.drive_id,
				authInfo.programAuthorityId)) == (int)0x80010011 || result >= 0))) {
			mount->pfsMount = pfsMount;
			goto attached;
		}
		for (index = 0; index < 16; index++) {
			if (pfsMount->programAuthorityIds[index] == authorityId) {
				pfsMount->programAuthorityIds[index] = 0;
				break;
			}
		}
		release = result != (int)0x80800003;
		goto checked;
	}

	opt.size = sizeof(opt);
	opt.mmapKind = 0;
	opt.align = 32;
	opt.mmapBase = NULL;
	opt.mmapSize = 0;
	pfsMount = sceKernelAllocHeapMemoryWithOption(g_appmgrMountHeap, sizeof(*pfsMount), &opt);
	if (pfsMount == NULL) {
		result = (int)0x80801006;
		release = 1;
		goto stored;
	}
	memset(pfsMount, 0, sizeof(*pfsMount));
	pfsMount->mountId = mountId;
	strncpy(pfsMount->appPath, normalized, sizeof(pfsMount->appPath) - 1);
	memcpy(pfsMount->titleId, titleId, sizeof(pfsMount->titleId));
	if (mountId == 600) {
		pfsMount->mountPoint.drive_id[0] = '\0';
		permission = sceKernelSetThreadAccessLevel(128);
		result = SceIofilemgrForDriver_D070BC48(0x10000, normalized, 0, NULL, 0, 0);
		if (result >= 0) {
			result = sceIoMkdir("lma0:MPE_ROOT", 0606);
			if (result == (int)0x80010011 || result >= 0) {
				sceKernelSetThreadAccessLevel(permission);
				goto authorize;
			}
		}
		sceKernelSetThreadAccessLevel(permission);
		release = result != (int)0x80800003;
		goto freePfsMount;
	}
	result = appmgrMakeRandomDriveId("PD", pfsMount->mountPoint.drive_id);
	if (result < 0)
		goto failed;
	if (strncmp(pfsMount->appPath, "host0:", 6) == 0)
		pfsMount->mountPoint.drive_id[0] = '\0';
	memset(&key, 0, sizeof(key));
	switch (pfsMount->mountId) {
	case 100 ... 106:
	case 109:
	case 400 ... 402:
		mode = 22;
		break;
	case 107:
	case 108:
	case 111:
	case 112:
	case 200 ... 207:
	case 1006:
	case 1009:
		mode = 21;
		break;
	case 110:
	case 1005:
		mode = 5;
		if (klicensee != NULL) {
			key = *klicensee;
			break;
		}
		result = appmgrReadSealedKey(pfsMount->appPath, &key);
		if (result != 0)
			goto failed;
		break;
	case 302 ... 304:
	case 504:
	case 505:
		mode = 6;
		result = appmgrReadSealedKey(pfsMount->appPath, &key);
		if (result != 0)
			goto failed;
		break;
	case 500:
	case 501:
		mode = 4;
		result = appmgrReadSealedKey(pfsMount->appPath, &key);
		if (result != 0)
			goto failed;
		break;
	case 1000:
#if FW_SINCE(0x04000000)
	case 1001:
#endif
		if (appmgrIsPfsContent(pfsMount->appPath)) {
			mode = 2;
			key = *klicensee;
		} else {
			mode = 20;
		}
		break;
#if !FW_SINCE(0x04000000)
	case 1001:
		mode = 2;
		key = *klicensee;
		break;
#endif
	case 1002:
	case 1003:
		mode = strncmp(pfsMount->appPath, "gro0:", 5) != 0 ? 4 : 12;
		result = appmgrReadSealedKey(pfsMount->appPath, &key);
		if (result != 0)
			goto failed;
		break;
	case 1004:
		mode = 13;
		key = *klicensee;
		break;
	case 1007:
	case 1008:
		mode = 20;
		break;
	default:
		result = (int)0x80800001;
		release = 1;
		goto freePfsMount;
	}
	if (pfsMount->mountPoint.drive_id[0] != '\0') {
		permission = sceKernelSetThreadAccessLevel(128);
		result = sceKernelGetProcessSelfAuthInfo(pid, &authInfo);
		if (result < 0)
			goto failed;
		for (index = 0; index < 16; index++)
			if (key.drive_id[index] != 0)
				break;
		result = ScePfsMgrForKernel_A772209C(pfsMount->appPath,
				pfsMount->mountPoint.drive_id, authInfo.programAuthorityId,
				index != 16 ? &key : NULL, mode);
		sceKernelSetThreadAccessLevel(permission);
		if (result != 0)
			goto failed;
		result = ScePfsMgrForKernel_D8D0FEE5(pfsMount->mountPoint.drive_id,
						     0x2800000000000001ULL);
		if (result != (int)0x80010011 && result < 0)
			goto unmount;
		result = ScePfsMgrForKernel_D8D0FEE5(pfsMount->mountPoint.drive_id,
						     0x2800000000007001ULL);
		if (result != (int)0x80010011 && result < 0)
			goto unmount;
		result = ScePfsMgrForKernel_D8D0FEE5(pfsMount->mountPoint.drive_id,
						     0x280000000000002dULL);
		if (result != (int)0x80010011 && result < 0)
			goto unmount;
		if (mode == 4)
			result = ScePfsMgrForKernel_D8D0FEE5(pfsMount->mountPoint.drive_id,
							     0x2800000000000010ULL);
		else if (mode == 5)
			result = ScePfsMgrForKernel_D8D0FEE5(pfsMount->mountPoint.drive_id,
							     0x2800000000000039ULL);
		else
			goto authorize;
		if (result != (int)0x80010011 && result < 0)
			goto unmount;
	}
authorize:
	for (index = 0; index < 16; index++)
		if (pfsMount->programAuthorityIds[index] == 0)
			break;
	if (index == 16) {
		result = (int)0x8080001e;
		release = 1;
		goto unmountRelease;
	}
	pfsMount->programAuthorityIds[index] = authorityId;
	if (g_appmgrPfsMounts == NULL) {
		g_appmgrPfsMounts = pfsMount;
		pfsMount->prev = NULL;
		pfsMount->next = NULL;
	} else {
		AppmgrPfsMount *last = g_appmgrPfsMounts;

		if (last == pfsMount)
			goto duplicate;
		while (last->next != NULL) {
			if (last == pfsMount)
				goto duplicate;
			last = last->next;
		}
		last->next = pfsMount;
		pfsMount->prev = last;
		pfsMount->next = NULL;
	}
	release = 0;
	result = 0;
	goto stored;

duplicate:
	result = (int)0x80800003;
	release = 0;
	goto unmountRelease;
unmount:
	ScePfsMgrForKernel_680BC384(pfsMount->mountPoint.drive_id);
failed:
	release = result != (int)0x80800003;
	goto freePfsMount;
unmountRelease:
	permission = sceKernelSetThreadAccessLevel(128);
	if (mountId == 600)
		SceIofilemgrForDriver_20574100(0x10000, 0, 0, 0);
	else
		ScePfsMgrForKernel_680BC384(pfsMount->mountPoint.drive_id);
	sceKernelSetThreadAccessLevel(permission);
freePfsMount:
	sceKernelFreeHeapMemory(g_appmgrMountHeap, pfsMount);
	pfsMount = NULL;
stored:
	mount->pfsMount = pfsMount;
checked:
	if (release)
		goto cleanup;
attached:
	sceKernelFreeHeapMemory(g_appmgrMountHeap, normalized);
	if (mountId == 1001 || mountId == 1003) {
		for (found = holder->firstMount; found != NULL; found = found->next)
			if (found->pfsMount != NULL &&
			    found->pfsMount->mountId == mountId - 1 &&
			    strncmp(found->driveId.drive_id, drive.drive_id, 16) == 0)
				break;
		if (found == NULL) {
			result = (int)0x80800002;
			normalized = NULL;
			goto cleanup;
		}
		mount->previousPfsMount = found->pfsMount;
	}
	result = appmgrAddVirtualMountOverlay(pid, mount);
	normalized = NULL;
	if (result != 0)
		goto cleanup;
	if (holder->firstMount == NULL) {
		holder->firstMount = mount;
		mount->prev = NULL;
		mount->next = NULL;
	} else {
		found = holder->firstMount;
		if (found == mount) {
			result = (int)0x80800003;
			goto cleanup;
		}
		while (found->next != NULL) {
			if (found == mount) {
				result = (int)0x80800003;
				goto cleanup;
			}
			found = found->next;
		}
		found->next = mount;
		mount->prev = found;
		mount->next = NULL;
	}
	*mountPoint = mount->driveId;
	return 0;

cleanup:
	if (mount->overlayId != 0) {
		target = pid != 0 ? pid : sceKernelGetProcessId();
		permission = sceKernelSetThreadAccessLevel(128);
		SceFios2KernelForDriver_23247EFB(target, mount->overlayId);
		sceKernelSetThreadAccessLevel(permission);
	}
	if (mount->pfsMount != NULL)
		appmgrReleasePfsMount(pid, mount->pfsMount, 1);
	sceKernelFreeHeapMemory(g_appmgrMountHeap, mount);
	if (normalized != NULL)
		sceKernelFreeHeapMemory(g_appmgrMountHeap, normalized);
	return result;
}

/*
 * Mount a process's add-on content at addcont0: and check its keystone. A game card's
 * add-on content (gro0:) is mounted first, and the downloaded one over it.
 */
int appmgrMountAddCont(SceUID pid, const char *titleId, AppmgrMountHolder *holder,
		       const AppmgrProcessContent *content, ScePfsRndDriveId *mountPoint)
{
	unsigned char keystone[96];
	SceAllocOpt opt;
	char *path;
	unsigned int mountId;
	int result, gameCard;

	memset(mountPoint, 0, sizeof(*mountPoint));
	opt.size = sizeof(opt);
	opt.mmapKind = 0;
	opt.align = 32;
	opt.mmapBase = NULL;
	opt.mmapSize = 0;
	path = sceKernelAllocHeapMemoryWithOption(g_appmgrMountHeap, 292, &opt);
	if (path == NULL)
		return (int)0x80801006;
	if (strncmp(content->appDirectory, "gro0:", 5) == 0) {
		gameCard = 1;
		snprintf(path, 292, "gro0:addcont/%s", content->addContTitleIds[0]);
	} else {
		result = appmgrGetAddContPath(content, NULL, path, 292);
		if (result < 0)
			goto out;
		gameCard = 0;
	}
	result = appmgrMount(pid, holder, 1002, titleId, path, NULL, NULL, mountPoint);
	if (result == (int)0x80800003)
		goto out;
	if (result < 0) {
		if (!gameCard)
			goto out;
		result = appmgrGetAddContPath(content, NULL, path, 292);
		if (result < 0)
			goto out;
		mountId = 1002;
	} else {
		ScePfsRndDriveId drive = { .drive_id = "addcont0:" };

		memset(keystone, 0, sizeof(keystone));
		result = appmgrReadKeystone(pid, drive.drive_id, keystone);
		if (result < 0)
			goto unmount;
		if (timingsafe_memcmp(keystone, content->keystone, sizeof(keystone)) != 0) {
			result = (int)0x80800006;
			goto unmount;
		}
		if (!gameCard) {
			result = 0;
			goto out;
		}
		result = appmgrGetAddContPath(content, NULL, path, 292);
		if (result < 0)
			goto unmount;
		mountId = 1003;
	}
	result = appmgrMount(pid, holder, mountId, titleId, path, NULL, NULL, mountPoint);
	if (result == (int)0x80800003)
		goto out;
	if (result < 0) {
		result = 0;
		goto out;
	}
	{
		ScePfsRndDriveId drive = { .drive_id = "addcont0:" };

		memset(keystone, 0, sizeof(keystone));
		result = appmgrReadKeystone(pid, drive.drive_id, keystone);
		if (result < 0)
			goto unmount;
		if (timingsafe_memcmp(keystone, content->keystone, sizeof(keystone)) != 0) {
			result = (int)0x80800006;
			goto unmount;
		}
		result = 0;
		goto out;
	}
unmount:
	appmgrUnmountDrive(pid, holder, mountPoint->drive_id, 0);
	memset(mountPoint, 0, sizeof(*mountPoint));
out:
	sceKernelFreeHeapMemory(g_appmgrMountHeap, path);
	return result;
}

/* Mount the calling application's add-on content at the drive it expects. */
int sceAppMgrAddContMount(SceUID pid, const AppmgrAddContMountParam *param)
{
	SceUInt32 syscallState = sceSyscallEnter();
	ScePfsRndDriveId mountPoint;
	struct AppState *app;
	int result;

	result = sceKernelLockMutex(g_appmgrMountMutex, 1, NULL);
	if (result < 0)
		goto out;
	if (pid != 0 && SceSblACMgrForDriver_8A54DF3A(0) != 0 &&
	    pid != sceKernelGetProcessId()) {
		result = 0x80800009;
	} else if (param == NULL) {
		result = 0x80801001;
	} else {
		result = 0x80800001;
		if (strncmp(param->mountPoint, "addcont0:", 10) == 0) {
			app = appmgrFindAppByProcessId(g_appmgrAppStates, pid);
			if (app == NULL) {
				result = 0x80802012;
			} else {
				mountPoint = (ScePfsRndDriveId){{0}};
				result = appmgrMountAddCont(pid, app->process.content.titleId,
						&app->process.mounts, &app->process.content,
						&mountPoint);
				if (result >= 0 &&
				    strncmp(mountPoint.drive_id, param->mountPoint, 16) != 0) {
					result = 0x80800040;
					appmgrUnmountDrive(pid, &app->process.mounts,
							   mountPoint.drive_id, 0);
				}
			}
		}
	}
	sceKernelUnlockMutex(g_appmgrMountMutex, 1);
out:
	sceSyscallLeave(syscallState);
	return result;
}

int sceAppMgrAddContAddMount(const AppmgrAddMountParam *param, int add)
{
	SceUInt32 syscallState = sceSyscallEnter();
	ScePfsRndDriveId mountPoint = {{0}};
	unsigned char keystone[96];
	char path[64];
	struct AppmgrMountHolder *holder;
	AppmgrProcessContent *content;
	struct AppState *app;
	int result;

	memset(keystone, 0, sizeof(keystone));
	if (param == NULL) {
		result = 0x80800001;
		goto out;
	}
	if (!sceAppMgrIsRelativeAddContAdd(param->titleId, add)) {
		result = 0x8080000E;
		goto out;
	}
	result = sceKernelLockMutex(g_appmgrMountMutex, 1, NULL);
	if (result < 0)
		goto out;

	MOUNT_MAIN_LOCK(5873);
	app = appmgrFindAppByProcessId(g_appmgrAppStates, sceKernelGetProcessId());
	if (app != NULL) {
		holder = &app->process.mounts;
		content = &app->process.content;
	} else {
		holder = NULL;
		content = NULL;
	}
	MOUNT_MAIN_UNLOCK(5882);

	if (app == NULL) {
		result = 0x80801002;
		goto error;
	}
	if (add != 0) {
		snprintf(path, sizeof(path), "ux0:addcont/%s", param->titleId);
	} else {
		result = appmgrGetAddContPath(content, param->titleId, path, sizeof(path));
		if (result < 0)
			goto error;
	}
	result = appmgrMount(0, holder, 1002, content->titleId, path,
			     &param->mountPoint, NULL, &mountPoint);
	if (result < 0)
		goto error;
	result = appmgrReadKeystone(0, mountPoint.drive_id, keystone);
	if (result < 0)
		goto error;
	result = SceSblPostSsMgrForDriver_F86F1452(keystone, param->passcode);
	if (result >= 0)
		goto unlock;
	result = 0x80800006;
error:
	if (result != 0x80800003 && mountPoint.drive_id[0] != '\0')
		appmgrUnmountDrive(0, holder, mountPoint.drive_id, 0);
unlock:
	sceKernelUnlockMutex(g_appmgrMountMutex, 1);
out:
	sceSyscallLeave(syscallState);
	return result;
}

/* The shell's external drives, encoded like every system path appmgr keeps */
static const unsigned char encodedSysExternal[] = {
	(unsigned char)~'v', (unsigned char)~'s', (unsigned char)~'0',
	(unsigned char)~':', (unsigned char)~'s', (unsigned char)~'y',
	(unsigned char)~'s', (unsigned char)~'/', (unsigned char)~'e',
	(unsigned char)~'x', (unsigned char)~'t', (unsigned char)~'e',
	(unsigned char)~'r', (unsigned char)~'n', (unsigned char)~'a',
	(unsigned char)~'l', 0
};
static const unsigned char encodedDataExternal[] = {
	(unsigned char)~'v', (unsigned char)~'s', (unsigned char)~'0',
	(unsigned char)~':', (unsigned char)~'d', (unsigned char)~'a',
	(unsigned char)~'t', (unsigned char)~'a', (unsigned char)~'/',
	(unsigned char)~'e', (unsigned char)~'x', (unsigned char)~'t',
	(unsigned char)~'e', (unsigned char)~'r', (unsigned char)~'n',
	(unsigned char)~'a', (unsigned char)~'l', 0
};

static AppmgrMountHolder g_appmgrShellMounts;

int appmgrMountShellExternalDirectories(SceUID pid)
{
	char titleId[16] = "NPXS19999";
	ScePfsRndDriveId sysDrive = {{0}};
	ScePfsRndDriveId dataDrive = {{0}};
	char path[64];
	int result;

	result = appmgrDecodeString(encodedSysExternal, path, sizeof(path));
	if (result < 0)
		goto error;
	result = appmgrMount(pid, &g_appmgrShellMounts, 1007, titleId, path,
			     NULL, NULL, &sysDrive);
	if (result < 0)
		goto error;
	result = appmgrDecodeString(encodedDataExternal, path, sizeof(path));
	if (result < 0)
		goto error;
	result = appmgrMount(pid, &g_appmgrShellMounts, 1008, titleId, path,
			     NULL, NULL, &dataDrive);
	if (result >= 0)
		return result;
error:
	if (sysDrive.drive_id[0] != '\0')
		appmgrUnmountDrive(pid, &g_appmgrShellMounts, sysDrive.drive_id, 0);
	if (dataDrive.drive_id[0] != '\0')
		appmgrUnmountDrive(pid, &g_appmgrShellMounts, dataDrive.drive_id, 0);
	return result;
}

/* Mounts a game's data drive (gp) for the caller, and its patch over it when there is one. */
int SceAppMgrForDriver_CE356B2D(const char *appPath, const char *patchPath,
				const char *rifPath, ScePfsRndDriveId *mountPoint)
{
	ScePfsRndDriveId driveId = {{0}};
	ScePfsRndDriveId drive = {{0}};
	SceUInt32 syscallState = sceSyscallEnter();
	AppmgrProcessContent *content = NULL;
	AppmgrPfsMount *mount;
	struct AppState *app;
	SceAllocOpt opt;
	SceIoStat stat;
	char *normalized, *path = NULL;
	int permission, hasLicense, result;

	if (mountPoint == NULL || appPath == NULL) {
		result = 0x80800001;
		goto out;
	}
	result = sceKernelLockMutex(g_appmgrMountMutex, 1, NULL);
	if (result < 0)
		goto out;
	app = appmgrFindAppByProcessId(g_appmgrAppStates, sceKernelGetProcessId());
	if (app == NULL) {
		result = 0x80801002;
		goto unlock;
	}

	mount = g_appmgrPfsMounts;
	normalized = appmgrAllocMountPath();
	if (normalized == NULL) {
		result = 0x80801006;
		goto error;
	}
	result = appmgrNormalizePath(appPath, normalized);
	if (result < 0) {
		sceKernelFreeHeapMemory(g_appmgrMountHeap, normalized);
		if (result != 0x80800002)
			goto error;
	} else {
		for (; mount != NULL; mount = mount->next) {
			if (mount->mountId == 900 &&
			    strncmp(mount->appPath, normalized, 292) == 0) {
				result = 0x80800017;
				sceKernelFreeHeapMemory(g_appmgrMountHeap, normalized);
				goto error;
			}
		}
		sceKernelFreeHeapMemory(g_appmgrMountHeap, normalized);
	}

	path = appmgrAllocMountPath();
	if (path == NULL) {
		result = 0x80801006;
		goto error;
	}
	opt.size = sizeof(opt);
	opt.mmapKind = 0;
	opt.align = 32;
	opt.mmapBase = NULL;
	opt.mmapSize = 0;
	content = sceKernelAllocHeapMemoryWithOption(g_appmgrMountHeap, sizeof(*content), &opt);
	if (content == NULL) {
		result = 0x80801006;
		goto error;
	}
	memset(content, 0, sizeof(*content));
	snprintf(path, 292, "%s/%s", appPath, "sce_sys/param.sfo");
	permission = sceKernelSetThreadAccessLevel(0x80);
	result = appmgrReadParamSfo(sceKernelGetProcessId(), path, 1, content);
	sceKernelSetThreadAccessLevel(permission);
	if (result < 0) {
		if (result == 0x80010002)
			result = 0x8080000D;
		goto error;
	}

	permission = sceKernelSetThreadAccessLevel(0x80);
	result = appmgrIsPfsContent(appPath);
	sceKernelSetThreadAccessLevel(permission);
	if (result != 0) {
		if (rifPath != NULL)
			result = appmgrReadRif(rifPath, &content->license);
		else
			result = appmgrReadApplicationLicense(appPath, content->titleId,
							      &content->license);
		if (result < 0)
			goto error;
		hasLicense = 1;
		permission = sceKernelSetThreadAccessLevel(0x80);
		result = appmgrCheckPackageLicenseFlag(appPath, &hasLicense);
		sceKernelSetThreadAccessLevel(permission);
		if (result < 0)
			goto error;
		if (hasLicense != 0)
			memset(content->license.klicense, 0, sizeof(content->license.klicense));
	}

	result = appmgrMakeRandomDriveId("gp", driveId.drive_id);
	if (result < 0)
		goto error;
	strcpy(&driveId.drive_id[13], "d:");
	result = appmgrMount(sceKernelGetProcessId(), &app->process.mounts, 1000,
			     content->titleId, appPath, &driveId,
			     (const ScePfsRndDriveId *)content->license.klicense, &drive);
	if (result < 0)
		goto error;
	result = appmgrReadParamSfo(sceKernelGetProcessId(), path, 0, content);
	if (result < 0)
		goto error;
	*mountPoint = drive;

	if (patchPath != NULL) {
		strncpy(path, patchPath, 292);
		path[291] = '\0';
		permission = sceKernelSetThreadAccessLevel(0x80);
		result = sceIoGetstat(path, &stat);
		sceKernelSetThreadAccessLevel(permission);
		if (result < 0) {
			if (strncmp(path, "invalid:", 9) != 0) {
				result = 0x8080000F;
				goto error;
			}
			result = 0;
			goto done;
		}
	} else {
		if (appmgrGetPatchPath(appPath, content->patchTitleId, content->attribute,
				       content->category, path) < 0) {
			result = 0;
			goto done;
		}
		permission = sceKernelSetThreadAccessLevel(0x80);
		result = sceIoGetstat(path, &stat);
		sceKernelSetThreadAccessLevel(permission);
		if (result < 0) {
			result = 0;
			goto done;
		}
	}
	result = appmgrMount(sceKernelGetProcessId(), &app->process.mounts, 1001,
			     content->titleId, path, &driveId,
			     (const ScePfsRndDriveId *)content->license.klicense, &drive);
	if (result < 0)
		goto error;
	*mountPoint = drive;
	goto done;

error:
	if (drive.drive_id[0] != '\0')
		appmgrUnmountDrive(sceKernelGetProcessId(), &app->process.mounts,
				   drive.drive_id, 0);
	if (path == NULL)
		goto free;
done:
	sceKernelFreeHeapMemory(g_appmgrMountHeap, path);
free:
	if (content != NULL)
		sceKernelFreeHeapMemory(g_appmgrMountHeap, content);
unlock:
	sceKernelUnlockMutex(g_appmgrMountMutex, 1);
out:
	sceSyscallLeave(syscallState);
	return result;
}

static const unsigned char encodedThemeRoot[32] = {
	(unsigned char)~'u', (unsigned char)~'x', (unsigned char)~'0', (unsigned char)~':',
	(unsigned char)~'t', (unsigned char)~'h', (unsigned char)~'e', (unsigned char)~'m',
	(unsigned char)~'e', 0
};
static const unsigned char encodedThemeLicenseRoot[32] = {
	(unsigned char)~'u', (unsigned char)~'x', (unsigned char)~'0', (unsigned char)~':',
	(unsigned char)~'l', (unsigned char)~'i', (unsigned char)~'c', (unsigned char)~'e',
	(unsigned char)~'n', (unsigned char)~'s', (unsigned char)~'e', (unsigned char)~'/',
	(unsigned char)~'t', (unsigned char)~'h', (unsigned char)~'e', (unsigned char)~'m',
	(unsigned char)~'e', 0
};

/* A theme's content ID: XXXXNNNNN-ZZZZZZZZZZZZZZZZ (X upper case, N digit, Z either). */
static inline int appmgrIsThemeContentId(const unsigned char *id)
{
	int i;

	for (i = 0; i < 4; i++)
		if (!APPMGR_IS_UPPER(id[i]))
			return 0;
	for (i = 4; i < 9; i++)
		if (!APPMGR_IS_DIGIT(id[i]))
			return 0;
	if (id[9] != '-')
		return 0;
	for (i = 10; i < 13; i++)
		if (!APPMGR_IS_UPPER(id[i]) && !APPMGR_IS_DIGIT(id[i]))
			return 0;
	for (i = 13; i < 26; i++)
		if (!APPMGR_IS_DIGIT(id[i]) && !APPMGR_IS_UPPER(id[i]))
			return 0;
	return id[26] == '\0';
}

/* Mounts a theme (tm) for the shell, from ux0:theme or the directory given. */
int sceAppMgrThemeDataMount(const char *contentId, const char *themePath,
			    const char *rifPath, ScePfsRndDriveId *mountPoint)
{
	ScePfsRndDriveId driveId = {{0}};
	SceUInt32 syscallState = sceSyscallEnter();
	SceSelfAuthInfo authInfo;
	AppmgrLicenseInfo license;
	struct AppState *app;
	SceAllocOpt opt;
	char titleId[16], root[32];
	char *directory = NULL, *path = NULL;
	int hasLicense = 1, result;
	unsigned int length;

	result = sceKernelGetProcessSelfAuthInfo(0, &authInfo);
	if (result < 0)
		goto out;
	if (authInfo.programAuthorityId != 0x2800000000000001ULL &&
	    authInfo.programAuthorityId != 0x2800000000000010ULL) {
		result = 0x80800009;
		goto out;
	}
	if (contentId == NULL || (themePath != NULL && rifPath == NULL) ||
	    mountPoint == NULL) {
		result = 0x80800001;
		goto out;
	}
	memset(mountPoint, 0, sizeof(*mountPoint));
	if (!appmgrIsThemeContentId((const unsigned char *)contentId)) {
		result = 0x80800001;
		goto out;
	}
	result = sceKernelLockMutex(g_appmgrMountMutex, 1, NULL);
	if (result < 0)
		goto out;
	app = appmgrFindAppByProcessId(g_appmgrAppStates, sceKernelGetProcessId());
	if (app == NULL) {
		result = 0x80801002;
		goto error;
	}

	opt.size = sizeof(opt);
	opt.mmapKind = 0;
	opt.align = 32;
	opt.mmapBase = NULL;
	opt.mmapSize = 0;
	directory = sceKernelAllocHeapMemoryWithOption(g_appmgrMountHeap, 128, &opt);
	if (directory == NULL) {
		result = 0x80801006;
		goto error;
	}
	path = appmgrAllocMountPath();
	if (path == NULL) {
		result = 0x80801006;
		goto error;
	}
	if (themePath != NULL) {
		length = snprintf(directory, 128, "%s", themePath);
	} else {
		result = appmgrDecodeString(encodedThemeRoot, root, sizeof(root));
		if (result < 0)
			goto error;
		length = snprintf(directory, 128, "%s/%s", root, contentId);
	}
	if (length >= 128) {
		result = 0x80800001;
		goto error;
	}
	length = snprintf(path, 292, "%s/%s", directory, "sce_sys/param.sfo");
	if (length >= 292) {
		result = 0x80800001;
		goto error;
	}
	result = appmgrGetAddcontTitleId(path, titleId);
	if (result < 0)
		goto error;

	memset(path, 0, 292);
	if (rifPath != NULL) {
		strncpy(path, rifPath, 291);
		result = appmgrReadRif(path, &license);
	} else {
		result = appmgrDecodeString(encodedThemeLicenseRoot, root, sizeof(root));
		if (result < 0)
			goto error;
		length = snprintf(path, 292, "%s/%s", root, contentId);
		if (length >= 292) {
			result = 0x80800001;
			goto error;
		}
		result = appmgrReadDirectoryLicense(path, &license);
	}
	if (result < 0)
		goto error;
	result = appmgrCheckPackageLicenseFlag(directory, &hasLicense);
	if (result < 0)
		goto error;
	if (hasLicense != 0)
		memset(license.klicense, 0, sizeof(license.klicense));

	result = appmgrMakeRandomDriveId("tm", driveId.drive_id);
	if (result < 0)
		goto error;
	strcpy(&driveId.drive_id[13], "d:");
	result = appmgrMount(sceKernelGetProcessId(), &app->process.mounts, 1004, titleId,
			     directory, &driveId, (const ScePfsRndDriveId *)license.klicense,
			     mountPoint);
	if (result < 0) {
		if (result == 0x80800003)
			goto done;
		goto error;
	}
	snprintf(path, 292, "%s/%s", directory, "sce_sys/param.sfo");
	result = appmgrGetAddcontTitleId(path, titleId);
	if (result >= 0)
		goto done;

error:
	if (mountPoint->drive_id[0] != '\0' && app != NULL)
		appmgrUnmountDrive(sceKernelGetProcessId(), &app->process.mounts,
				   mountPoint->drive_id, 0);
done:
	if (directory != NULL)
		sceKernelFreeHeapMemory(g_appmgrMountHeap, directory);
	if (path != NULL)
		sceKernelFreeHeapMemory(g_appmgrMountHeap, path);
	sceKernelUnlockMutex(g_appmgrMountMutex, 1);
out:
	sceSyscallLeave(syscallState);
	return result;
}

int appmgrResolveLaunchDirectory(const char *path,
				const AppmgrLaunchOptions *options,
				const AppmgrMapFile *mapFile, char *output,
				SceSize size)
{
	const char *slash = strrchr(path, '/');
	const char *backslash = strrchr(path, '\\');
	const char *colon = strrchr(path, ':');
	const char *directory;
	char *resolved = NULL;
	SceAllocOpt opt;
	SceUID pid;
	int permission, result;

	if (slash <= backslash) {
		if (backslash <= colon)
			slash = colon != NULL ? colon + 1 : NULL;
		else
			slash = backslash;
	}
	if (output != NULL) {
		memset(output, 0, size);
		if (slash != NULL)
			strncpy(output, path, slash - path);
	}
	if (SceSblAIMgrForDriver_D78B04A2() || SceSblAIMgrForDriver_F4B98F66() ||
	    !SceSblACMgrForDriver_E87D1777())
		return 0;
	if (mapFile != NULL && mapFile->file > 0 && mapFile->app0[0] != 0) {
		directory = mapFile->app0;
	} else if (options != NULL && options->size == sizeof(*options) &&
		   options->app0Directory[0] != 0) {
		directory = options->app0Directory;
	} else {
		goto validate;
	}
	opt.size = sizeof(opt);
	opt.mmapKind = 0;
	opt.align = 32;
	opt.mmapBase = NULL;
	opt.mmapSize = 0;
	resolved = sceKernelAllocHeapMemoryWithOption(g_appmgrMountHeap, 292, &opt);
	if (resolved == NULL)
		return (int)0x80801006;
	memset(resolved, 0, 292);
	pid = sceKernelGetProcessId();
	permission = sceKernelSetThreadAccessLevel(128);
	result = SceFios2KernelForDriver_0F456345(pid, 0, directory, resolved, 292);
	sceKernelSetThreadAccessLevel(permission);
	if (result < 0)
		goto free_path;
	if ((strncmp(resolved, "host0:", 6) != 0 &&
	     strncmp(resolved, "ux0:data", 8) != 0 &&
	     strncmp(resolved, "ux0:\\data", 9) != 0 &&
	     strncmp(resolved, "ux0:/data", 9) != 0) ||
	    strncmp(resolved, "ux0:/data", 10) == 0 ||
	    strncmp(resolved, "ux0:/data/savedata", 19) == 0 ||
	    strncmp(resolved, "ux0:/data/savedata/", 19) == 0 ||
	    strncmp(resolved, "ux0:/data/userdata", 19) == 0 ||
	    strncmp(resolved, "ux0:/data/userdata/", 19) == 0) {
		result = (int)0x80800022;
		goto free_path;
	}
	memset(output, 0, 292);
	strncpy(output, resolved, 292);
	if (output[291] != 0) {
		result = (int)0x80800022;
		goto free_path;
	}
validate:
	result = appmgrCreateDirectories(output, 0) == 0 ? 0 : (int)0x80800022;
free_path:
	if (resolved != NULL)
		sceKernelFreeHeapMemory(g_appmgrMountHeap, resolved);
	return result;
}

static const unsigned char encodedAddcontLicenseDirectory[16] = {
	(unsigned char)~'l', (unsigned char)~'i', (unsigned char)~'c', (unsigned char)~'e',
	(unsigned char)~'n', (unsigned char)~'s', (unsigned char)~'e', (unsigned char)~'/',
	(unsigned char)~'a', (unsigned char)~'d', (unsigned char)~'d', (unsigned char)~'c',
	(unsigned char)~'o', (unsigned char)~'n', (unsigned char)~'t', 0
};

static char g_appmgrAddcontLicenseDirectory[32];

/*
 * Mounts an add-on content directory (acid) of the PFS drive a path resolves to, once its
 * license under <drive root>/license/addcont is current; a gro0: application retries with
 * the license on gro0:.
 */
int appmgrMountPfsAcidDirectory(const char *drive, const char *directory)
{
	SceUID pid = sceKernelGetProcessId();
	char path[64] = "";
	char driveName[33];
	char root[16];
	char licensePath[128];
	AppmgrLicenseInfo license;
	AppmgrPfsMount *mount;
	struct AppState *app;
	SceAllocOpt opt;
	SceUInt64 tick;
	const char *name, *backslash, *colon;
	const void *klicensee;
	char *resolvedPath;
	int index, permission, isGameCard, result;

	if (directory == NULL || drive == NULL)
		return 0x80800001;
	memset(driveName, 0, sizeof(driveName));
	memset(&license, 0, sizeof(license));
	app = appmgrFindAppByProcessId(g_appmgrAppStates, pid);
	if (app == NULL)
		return 0x80802012;
	opt.size = sizeof(opt);
	opt.mmapKind = 0;
	opt.align = 32;
	opt.mmapBase = NULL;
	opt.mmapSize = 0;
	resolvedPath = sceKernelAllocHeapMemoryWithOption(g_appmgrMountHeap, 292, &opt);
	if (resolvedPath == NULL)
		return 0x80801006;
	snprintf(path, sizeof(path), "%s%s", drive, directory);
	pid = sceKernelGetProcessId();
	permission = sceKernelSetThreadAccessLevel(128);
	result = SceFios2KernelForDriver_0F456345(pid, 0, path, resolvedPath, 292);
	sceKernelSetThreadAccessLevel(permission);
	if (result < 0)
		goto free;
	memset(driveName, 0, sizeof(driveName));
	for (index = 0; ; ) {
		driveName[index] = resolvedPath[index];
		if (resolvedPath[index] == ':')
			break;
		if (++index == 33) {
			result = 0x8080000c;
			goto free;
		}
	}
	driveName[15] = '\0';
	for (mount = g_appmgrPfsMounts; mount != NULL; mount = mount->next)
		if (strncmp(mount->mountPoint.drive_id, driveName, 16) == 0)
			break;
	if (mount == NULL || mount->mountPoint.drive_id[0] == '\0') {
		result = 0x80800002;
		goto free;
	}
	result = appmgrCreateDirectories(resolvedPath, 0);
	if (result < 0)
		goto free;
	sceKernelFreeHeapMemory(g_appmgrMountHeap, resolvedPath);

	isGameCard = strncmp(mount->appPath, "gro0:", 5) == 0;
	if ((app->process.content.contentFlags & 0x10) != 0 &&
	    appmgrGetFakeNoMemoryCard() == 2) {
		if (!isGameCard)
			return 0x80800002;
		isGameCard = 1;
	}
	memset(root, 0, sizeof(root));
	if (isGameCard) {
		strncpy(root, "gro0:", sizeof(root));
	} else {
		memset(root, 0, sizeof(root));
		for (index = 0; ; index++) {
			root[index] = mount->appPath[index];
			if (root[index] == ':')
				break;
			if (index == 15) {
				result = 0x8080000c;
				goto retry;
			}
		}
	}

	for (;;) {
		name = strrchr(mount->appPath, '/');
		backslash = strrchr(mount->appPath, '\\');
		colon = strrchr(mount->appPath, ':');
		if (name <= backslash) {
			if (backslash <= colon)
				name = colon != NULL ? colon + 1 : NULL;
			else
				name = backslash;
		}
		if (name == NULL) {
			result = 0x8080000c;
			goto failed;
		}
		memset(licensePath, 0, sizeof(licensePath));
		if (appmgrDecodeString(encodedAddcontLicenseDirectory,
				       g_appmgrAddcontLicenseDirectory,
				       sizeof(g_appmgrAddcontLicenseDirectory)) < 0 ||
		    (unsigned int)snprintf(licensePath, sizeof(licensePath), "%s/%s/%s/%s", root,
					   g_appmgrAddcontLicenseDirectory, name,
					   directory) >= sizeof(licensePath)) {
			result = 0x80800001;
			goto failed;
		}
		result = appmgrReadDirectoryLicense(licensePath, &license);
		if (result < 0)
			goto failed;
		result = SceRtcForDriver_401C0954((SceRtcTick *)&tick);
		if (result >= 0) {
			if (tick < license.licStartTime)
				result = 0x80800018;
			else if (license.licExpTime < tick)
				result = 0x80800011;
			else
				break;
		} else if ((license.flags & 2) == 0 &&
			   license.licExpTime == 0x7FFFFFFFFFFFFFFFULL) {
			break;
		}
failed:
		if (isGameCard)
			return result;
retry:
		if (strncmp(app->process.content.appDirectory, "gro0:", 5) != 0)
			return result;
		isGameCard = 1;
		memset(root, 0, sizeof(root));
		strncpy(root, "gro0:", sizeof(root));
	}

	permission = sceKernelSetThreadAccessLevel(0x40);
	klicensee = NULL;
	for (index = 0; index < 16; index++) {
		if (license.klicense[index] != 0) {
			klicensee = license.klicense;
			break;
		}
	}
	result = ScePfsMgrForKernel_4A4A8243(mount->mountPoint.drive_id, directory, klicensee);
	sceKernelSetThreadAccessLevel(permission);
	if (result < 0)
		return result;
	return ScePfsMgrForKernel_8CDA249A(mount->mountPoint.drive_id, directory);

free:
	sceKernelFreeHeapMemory(g_appmgrMountHeap, resolvedPath);
	return result;
}

/* sce_sys/param.sfo, its terminator encoded too */
static const unsigned char encodedParamSfo[24] = {
	(unsigned char)~'s', (unsigned char)~'c', (unsigned char)~'e', (unsigned char)~'_',
	(unsigned char)~'s', (unsigned char)~'y', (unsigned char)~'s', (unsigned char)~'/',
	(unsigned char)~'p', (unsigned char)~'a', (unsigned char)~'r', (unsigned char)~'a',
	(unsigned char)~'m', (unsigned char)~'.', (unsigned char)~'s', (unsigned char)~'f',
	(unsigned char)~'o', (unsigned char)~'\0', 0
};
/* gro0:acinst, its terminator encoded too */
static const unsigned char encodedAcinstRoot[16] = {
	(unsigned char)~'g', (unsigned char)~'r', (unsigned char)~'o', (unsigned char)~'0',
	(unsigned char)~':', (unsigned char)~'a', (unsigned char)~'c', (unsigned char)~'i',
	(unsigned char)~'n', (unsigned char)~'s', (unsigned char)~'t', (unsigned char)~'\0', 0
};

static int appmgrIsTitleId(const char *id);

/* An entitlement label: 16 upper case letters and digits. */
static inline int appmgrIsEntitlementLabel(const char *label)
{
	int i;

	for (i = 0; i < 6; i++)
		if (!APPMGR_IS_UPPER(label[i]) && !APPMGR_IS_DIGIT(label[i]))
			return 0;
	for (i = 6; i < 16; i++)
		if (!APPMGR_IS_DIGIT(label[i]) && !APPMGR_IS_UPPER(label[i]))
			return 0;
	return label[16] == '\0';
}

/*
 * Mounts the caller's additional content left in gro0:acinst (title ID and entitlement
 * label), reads its keystone and title, and unmounts it again.
 */
int sceAppMgrAcInstGetAcdirParam(const char *titleId, const char *entitlementLabel,
				 SceAppMgrAcdirParam *param)
{
	ScePfsRndDriveId drive = {{0}};
	AppmgrSfoFile file = {0};
	struct AppState *app;
	AppmgrMountHolder *mounts;
	AppmgrProcessContent *content;
	void *data = NULL;
	SceSize length = 0, capacity = 0;
	SceUInt32 language;
	char key[32], path[64], root[64], sfoName[64];
	int result, failed;

	result = sceKernelLockMutex(g_appmgrMountMutex, 1, NULL);
	if (result < 0)
		return result;
	MOUNT_MAIN_LOCK(5675);
	app = appmgrFindAppByProcessId(g_appmgrAppStates, sceKernelGetProcessId());
	if (app != NULL) {
		mounts = &app->process.mounts;
		content = &app->process.content;
	} else {
		mounts = NULL;
		content = NULL;
	}
	MOUNT_MAIN_UNLOCK(5684);
	if (app == NULL) {
		result = (int)0x80801002;
		goto fail;
	}
	if (titleId == NULL || !appmgrIsTitleId(titleId) || entitlementLabel == NULL ||
	    !appmgrIsEntitlementLabel(entitlementLabel) || param == NULL)
		goto invalid;
	memset(param, 0, sizeof(*param));
	if (appmgrDecodeString(encodedAcinstRoot, root, sizeof(root)) < 0 ||
	    (SceSize)snprintf(path, sizeof(path), "%s/%s", root, titleId) >= sizeof(path))
		goto invalid;
	result = appmgrMount(0, mounts, 1002, content->titleId, path, NULL, NULL, &drive);
	if (result < 0)
		goto fail;
	result = appmgrReadKeystone(0, drive.drive_id, param->keystone);
	if (result < 0)
		goto unmountDrive;
	result = appmgrMountPfsAcidDirectory(drive.drive_id, entitlementLabel);
	if (result < 0)
		goto unmountDrive;
	result = appmgrDecodeString(encodedParamSfo, sfoName, sizeof(sfoName));
	if (result < 0)
		goto unmountAcid;
	if ((SceSize)snprintf(path, sizeof(path), "%s/%s/%s/%s", root, titleId,
			      entitlementLabel, sfoName) >= sizeof(path)) {
		result = (int)0x80800001;
		goto unmountAcid;
	}
	result = appmgrLoadSfoFile(path, &file);
	if (result < 0)
		goto unmountAcid;
	result = sceRegMgrGetKeyInt("/CONFIG/SYSTEM", "language", (int *)&language);
	if (result == 0 && language <= 99) {
		snprintf(key, sizeof(key), "TITLE_%02u", language);
		result = appmgrGetSfoEntry(&file, key, &data, &length, &capacity);
		if (result < 0 || length == 0 || capacity != 128)
			data = NULL;
	}
	if (data == NULL) {
		result = appmgrGetSfoEntry(&file, "TITLE", &data, &length, &capacity);
		if (result < 0 || length == 0 || capacity != 128)
			data = NULL;
	}
	strncpy(param->title, data != NULL ? data : "", sizeof(param->title) - 1);
unmountAcid:
	failed = result < 0;
	appmgrUnmountPfsAcidDirectory(drive.drive_id, entitlementLabel);
	goto unmount;
unmountDrive:
	failed = 1;
unmount:
	appmgrUnmountDrive(0, mounts, drive.drive_id, 0);
	goto release;
invalid:
	result = (int)0x80800001;
fail:
	failed = 1;
release:
	appmgrReleaseSfoFile(&file);
	if (param != NULL && failed)
		memset(param, 0, sizeof(*param));
	sceKernelUnlockMutex(g_appmgrMountMutex, 1);
	return result;
}

int sceAppMgrSaveDataAddMount(const AppmgrAddMountParam *param)
{
	SceUInt32 syscallState = sceSyscallEnter();
	ScePfsRndDriveId mountPoint = {{0}};
	unsigned char keystone[96];
	char path[64];
	struct AppmgrMountHolder *holder;
	AppmgrProcessContent *content;
	AppmgrVirtualMount *virtualMount;
	AppmgrPfsMount *mount;
	struct AppState *app;
	SceAllocOpt opt;
	char *normalized;
	int permission, result;

	memset(keystone, 0, sizeof(keystone));
	permission = sceKernelSetThreadAccessLevel(128);
	if (param == NULL) {
		result = 0x80800001;
		goto out;
	}
	if (!sceAppMgrIsRelativeSaveDataAdd(param->titleId)) {
		result = 0x8080000B;
		goto out;
	}
	result = sceKernelLockMutex(g_appmgrMountMutex, 1, NULL);
	if (result < 0)
		goto out;

	MOUNT_MAIN_LOCK(5521);
	app = appmgrFindAppByProcessId(g_appmgrAppStates, sceKernelGetProcessId());
	if (app != NULL) {
		holder = &app->process.mounts;
		content = &app->process.content;
	} else {
		holder = NULL;
		content = NULL;
	}
	MOUNT_MAIN_UNLOCK(5530);

	if (app == NULL) {
		result = 0x80801002;
		goto error;
	}
	if (g_appmgrAccountDirectory[0] == 0)
		appmgrUpdateAccountDirectory();
	snprintf(path, sizeof(path), "ux0:/user/%s/savedata_vc/%s",
		 g_appmgrAccountDirectory, param->titleId);
	if (appmgrCreateDirectories(path, 0) != 0) {
		if (g_appmgrAccountDirectory[0] == 0)
			appmgrUpdateAccountDirectory();
		snprintf(path, sizeof(path), "ux0:/user/%s/savedata/%s",
			 g_appmgrAccountDirectory, param->titleId);
		result = appmgrCreateDirectories(path, 0);
		if (result < 0)
			goto error;
	}

	for (virtualMount = holder->firstMount; virtualMount != NULL;
	     virtualMount = virtualMount->next) {
		if (strncmp(virtualMount->driveId.drive_id, param->mountPoint.drive_id, 16) == 0) {
			result = 0x80801080;
			if (strncmp(virtualMount->pfsMount->appPath, path, sizeof(path)) == 0)
				result = 0x80800003;
			goto error;
		}
	}

	mount = g_appmgrPfsMounts;
	opt.size = sizeof(opt);
	opt.mmapKind = 0;
	opt.align = 32;
	opt.mmapBase = NULL;
	opt.mmapSize = 0;
	normalized = sceKernelAllocHeapMemoryWithOption(g_appmgrMountHeap, 292, &opt);
	if (normalized == NULL) {
		result = 0x80801006;
		goto error;
	}
	result = appmgrNormalizePath(path, normalized);
	if (result < 0) {
		sceKernelFreeHeapMemory(g_appmgrMountHeap, normalized);
		if (result != 0x80800002)
			goto error;
	} else {
		for (; mount != NULL; mount = mount->next) {
			if (mount->mountId == 901 &&
			    strncmp(mount->appPath, normalized, 292) == 0) {
				result = 0x80800017;
				sceKernelFreeHeapMemory(g_appmgrMountHeap, normalized);
				goto error;
			}
		}
		sceKernelFreeHeapMemory(g_appmgrMountHeap, normalized);
	}

	result = appmgrMount(0, holder, 1005, content->titleId, path,
			     &param->mountPoint, NULL, &mountPoint);
	if (result < 0)
		goto error;
	result = appmgrReadKeystone(0, mountPoint.drive_id, keystone);
	if (result < 0)
		goto error;
	result = SceSblPostSsMgrForDriver_F86F1452(keystone, param->passcode);
	if (result >= 0)
		goto unlock;
	result = 0x80800006;
error:
	if (result != 0x80800003 && mountPoint.drive_id[0] != '\0')
		appmgrUnmountDrive(0, holder, mountPoint.drive_id, 0);
unlock:
	sceKernelUnlockMutex(g_appmgrMountMutex, 1);
out:
	sceKernelSetThreadAccessLevel(permission);
	sceSyscallLeave(syscallState);
	return result;
}

/*
 * Makes a directory a new PFS directory for a mount ID: a new sealed key, the directory
 * mounted with its key and the save data metadata written. A directory that already has
 * sce_pfs or sce_sys is left as it is.
 */
int appmgrCreatePfsDirectory(unsigned int mountId, const char *path, const void *keystone)
{
	unsigned char sealedKey[80];
	ScePfsRndDriveId key, drive;
	SceSelfAuthInfo authInfo;
	SceUInt64 authorityId;
	SceAllocOpt opt;
	SceUInt16 mode;
	char *buffer;
	int result;

	switch (mountId) {
	case 110:
		result = sceKernelGetProcessSelfAuthInfo(0, &authInfo);
		if (result < 0)
			return result;
		opt.size = sizeof(opt);
		opt.mmapKind = 0;
		opt.align = 32;
		opt.mmapBase = NULL;
		opt.mmapSize = 0;
		authorityId = authInfo.programAuthorityId;
		buffer = sceKernelAllocHeapMemoryWithOption(g_appmgrMountHeap, 292, &opt);
		if (buffer == NULL)
			return 0x80801006;
		mode = 5;
		goto create;
	case 300:
	case 302 ... 304:
	case 504:
	case 505:
		mode = 6;
		break;
	case 500:
	case 501:
		mode = 4;
		break;
	default:
		return 0x80800001;
	}
	opt.size = sizeof(opt);
	opt.mmapKind = 0;
	opt.align = 32;
	opt.mmapBase = NULL;
	opt.mmapSize = 0;
	buffer = sceKernelAllocHeapMemoryWithOption(g_appmgrMountHeap, 292, &opt);
	if (buffer == NULL)
		return 0x80801006;
	if (mountId == 504) {
		result = appmgrCreateDirectories(path, 0);
		if (result < 0)
			goto free;
	} else if (mountId == 505 && appmgrCreateDirectories(path, 0) == 0) {
		result = 0x80800045;
		goto free;
	}
	authorityId = 0x2800000000000001ULL;

create:
	result = appmgrCreateDirectories(path, 1);
	if (result < 0)
		goto free;
	snprintf(buffer, 292, "%s/sce_pfs", path);
	result = appmgrCreateDirectories(buffer, 0);
	if (result == 0)
		goto free;
	snprintf(buffer, 292, "%s/sce_sys", path);
	result = appmgrCreateDirectories(buffer, 0);
	if (result == 0)
		goto free;
	sceKernelFreeHeapMemory(g_appmgrMountHeap, buffer);

	memset(sealedKey, 0, sizeof(sealedKey));
	key = (ScePfsRndDriveId){{0}};
	drive = (ScePfsRndDriveId){{0}};
	result = sceSblSsGenerateSealedKey(sealedKey);
	if (result < 0)
		return result;
	result = sceSblSsDecryptSealedKey(sealedKey, &key);
	if (result < 0)
		return result;
	result = appmgrMakeRandomDriveId("ts", drive.drive_id);
	if (result < 0)
		return result;
	result = ScePfsMgrForKernel_A772209C(path, drive.drive_id, authorityId, &key, mode);
	if (result < 0)
		return result;
	result = appmgrCreateSaveDataMetadata(0, path, NULL, mountId, sealedKey, keystone, NULL);
	ScePfsMgrForKernel_680BC384(drive.drive_id);
	return result;

free:
	sceKernelFreeHeapMemory(g_appmgrMountHeap, buffer);
	return result;
}

/* The system paths the mount IDs name, encoded like every system path appmgr keeps */
static const unsigned char encodedMmsMusic[16] = {
	(unsigned char)~'u', (unsigned char)~'x', (unsigned char)~'0', (unsigned char)~':',
	(unsigned char)~'m', (unsigned char)~'m', (unsigned char)~'s', (unsigned char)~'/',
	(unsigned char)~'m', (unsigned char)~'u', (unsigned char)~'s', (unsigned char)~'i',
	(unsigned char)~'c', 0
};
static const unsigned char encodedPspEmu[16] = {
	(unsigned char)~'u', (unsigned char)~'x', (unsigned char)~'0', (unsigned char)~':',
	(unsigned char)~'p', (unsigned char)~'s', (unsigned char)~'p', (unsigned char)~'e',
	(unsigned char)~'m', (unsigned char)~'u', 0
};
static const unsigned char encodedAppMeta[16] = {
	(unsigned char)~'u', (unsigned char)~'x', (unsigned char)~'0', (unsigned char)~':',
	(unsigned char)~'a', (unsigned char)~'p', (unsigned char)~'p', (unsigned char)~'m',
	(unsigned char)~'e', (unsigned char)~'t', (unsigned char)~'a', (unsigned char)~'\0',
	0
};
static const unsigned char encodedGriefReport[24] = {
	(unsigned char)~'u', (unsigned char)~'r', (unsigned char)~'0', (unsigned char)~':',
	(unsigned char)~'t', (unsigned char)~'e', (unsigned char)~'m', (unsigned char)~'p',
	(unsigned char)~'/', (unsigned char)~'g', (unsigned char)~'r', (unsigned char)~'i',
	(unsigned char)~'e', (unsigned char)~'f', (unsigned char)~'_', (unsigned char)~'r',
	(unsigned char)~'e', (unsigned char)~'p', (unsigned char)~'o', (unsigned char)~'r',
	(unsigned char)~'t', 0
};
static const unsigned char encodedWebBrowser[24] = {
	(unsigned char)~'u', (unsigned char)~'r', (unsigned char)~'0', (unsigned char)~':',
	(unsigned char)~'t', (unsigned char)~'e', (unsigned char)~'m', (unsigned char)~'p',
	(unsigned char)~'/', (unsigned char)~'w', (unsigned char)~'e', (unsigned char)~'b',
	(unsigned char)~'b', (unsigned char)~'r', (unsigned char)~'o', (unsigned char)~'w',
	(unsigned char)~'s', (unsigned char)~'e', (unsigned char)~'r', 0
};
static const unsigned char encodedBook[16] = {
	(unsigned char)~'u', (unsigned char)~'x', (unsigned char)~'0', (unsigned char)~':',
	(unsigned char)~'b', (unsigned char)~'o', (unsigned char)~'o', (unsigned char)~'k',
	0
};
static const unsigned char encodedMmsVideo[16] = {
	(unsigned char)~'u', (unsigned char)~'x', (unsigned char)~'0', (unsigned char)~':',
	(unsigned char)~'m', (unsigned char)~'m', (unsigned char)~'s', (unsigned char)~'/',
	(unsigned char)~'v', (unsigned char)~'i', (unsigned char)~'d', (unsigned char)~'e',
	(unsigned char)~'o', 0
};
static const unsigned char encodedGiftReceived[24] = {
	(unsigned char)~'n', (unsigned char)~'e', (unsigned char)~'a', (unsigned char)~'r',
	(unsigned char)~'/', (unsigned char)~'d', (unsigned char)~'a', (unsigned char)~'t',
	(unsigned char)~'a', (unsigned char)~'/', (unsigned char)~'g', (unsigned char)~'i',
	(unsigned char)~'f', (unsigned char)~'t', (unsigned char)~'/', (unsigned char)~'r',
	(unsigned char)~'e', (unsigned char)~'c', (unsigned char)~'e', (unsigned char)~'i',
	(unsigned char)~'v', (unsigned char)~'e', (unsigned char)~'d', 0
};
static const unsigned char encodedCheckout[24] = {
	(unsigned char)~'u', (unsigned char)~'r', (unsigned char)~'0', (unsigned char)~':',
	(unsigned char)~'t', (unsigned char)~'e', (unsigned char)~'m', (unsigned char)~'p',
	(unsigned char)~'/', (unsigned char)~'c', (unsigned char)~'h', (unsigned char)~'e',
	(unsigned char)~'c', (unsigned char)~'k', (unsigned char)~'o', (unsigned char)~'u',
	(unsigned char)~'t', 0
};
static const unsigned char encodedVideo[16] = {
	(unsigned char)~'u', (unsigned char)~'x', (unsigned char)~'0', (unsigned char)~':',
	(unsigned char)~'v', (unsigned char)~'i', (unsigned char)~'d', (unsigned char)~'e',
	(unsigned char)~'o', 0
};
static const unsigned char encodedNearDataSys[16] = {
	(unsigned char)~'n', (unsigned char)~'e', (unsigned char)~'a', (unsigned char)~'r',
	(unsigned char)~'/', (unsigned char)~'d', (unsigned char)~'a', (unsigned char)~'t',
	(unsigned char)~'a', (unsigned char)~'_', (unsigned char)~'s', (unsigned char)~'y',
	(unsigned char)~'s', 0
};
static const unsigned char encodedSqlite[16] = {
	(unsigned char)~'u', (unsigned char)~'r', (unsigned char)~'0', (unsigned char)~':',
	(unsigned char)~'t', (unsigned char)~'e', (unsigned char)~'m', (unsigned char)~'p',
	(unsigned char)~'/', (unsigned char)~'s', (unsigned char)~'q', (unsigned char)~'l',
	(unsigned char)~'i', (unsigned char)~'t', (unsigned char)~'e', 0
};
static const unsigned char encodedTrophySceTrop[24] = {
	(unsigned char)~'t', (unsigned char)~'r', (unsigned char)~'o', (unsigned char)~'p',
	(unsigned char)~'h', (unsigned char)~'y', (unsigned char)~'/', (unsigned char)~'d',
	(unsigned char)~'a', (unsigned char)~'t', (unsigned char)~'a', (unsigned char)~'/',
	(unsigned char)~'s', (unsigned char)~'c', (unsigned char)~'e', (unsigned char)~'_',
	(unsigned char)~'t', (unsigned char)~'r', (unsigned char)~'o', (unsigned char)~'p',
	0
};
static const unsigned char encodedPsnMsg[8] = {
	(unsigned char)~'p', (unsigned char)~'s', (unsigned char)~'n', (unsigned char)~'m',
	(unsigned char)~'s', (unsigned char)~'g', 0
};
static const unsigned char encodedMusic[16] = {
	(unsigned char)~'u', (unsigned char)~'x', (unsigned char)~'0', (unsigned char)~':',
	(unsigned char)~'m', (unsigned char)~'u', (unsigned char)~'s', (unsigned char)~'i',
	(unsigned char)~'c', 0
};
static const unsigned char encodedPsnFriend[16] = {
	(unsigned char)~'p', (unsigned char)~'s', (unsigned char)~'n', (unsigned char)~'f',
	(unsigned char)~'r', (unsigned char)~'i', (unsigned char)~'e', (unsigned char)~'n',
	(unsigned char)~'d', 0
};
static const unsigned char encodedAttach[16] = {
	(unsigned char)~'u', (unsigned char)~'r', (unsigned char)~'0', (unsigned char)~':',
	(unsigned char)~'t', (unsigned char)~'e', (unsigned char)~'m', (unsigned char)~'p',
	(unsigned char)~'/', (unsigned char)~'a', (unsigned char)~'t', (unsigned char)~'t',
	(unsigned char)~'a', (unsigned char)~'c', (unsigned char)~'h', 0
};
static const unsigned char encodedCalendar[16] = {
	(unsigned char)~'u', (unsigned char)~'x', (unsigned char)~'0', (unsigned char)~':',
	(unsigned char)~'c', (unsigned char)~'a', (unsigned char)~'l', (unsigned char)~'e',
	(unsigned char)~'n', (unsigned char)~'d', (unsigned char)~'a', (unsigned char)~'r',
	0
};
static const unsigned char encodedMmsPhoto[16] = {
	(unsigned char)~'u', (unsigned char)~'x', (unsigned char)~'0', (unsigned char)~':',
	(unsigned char)~'m', (unsigned char)~'m', (unsigned char)~'s', (unsigned char)~'/',
	(unsigned char)~'p', (unsigned char)~'h', (unsigned char)~'o', (unsigned char)~'t',
	(unsigned char)~'o', 0
};
static const unsigned char encodedUserData[24] = {
	(unsigned char)~'u', (unsigned char)~'x', (unsigned char)~'0', (unsigned char)~':',
	(unsigned char)~'d', (unsigned char)~'a', (unsigned char)~'t', (unsigned char)~'a',
	(unsigned char)~'/', (unsigned char)~'u', (unsigned char)~'s', (unsigned char)~'e',
	(unsigned char)~'r', (unsigned char)~'d', (unsigned char)~'a', (unsigned char)~'t',
	(unsigned char)~'a', 0
};
static const unsigned char encodedNpHome[16] = {
	(unsigned char)~'t', (unsigned char)~'m', (unsigned char)~'0', (unsigned char)~':',
	(unsigned char)~'n', (unsigned char)~'p', (unsigned char)~'h', (unsigned char)~'o',
	(unsigned char)~'m', (unsigned char)~'e', 0
};
static const unsigned char encodedPicture[16] = {
	(unsigned char)~'u', (unsigned char)~'x', (unsigned char)~'0', (unsigned char)~':',
	(unsigned char)~'p', (unsigned char)~'i', (unsigned char)~'c', (unsigned char)~'t',
	(unsigned char)~'u', (unsigned char)~'r', (unsigned char)~'e', 0
};
static const unsigned char encodedTrophyData[16] = {
	(unsigned char)~'t', (unsigned char)~'r', (unsigned char)~'o', (unsigned char)~'p',
	(unsigned char)~'h', (unsigned char)~'y', (unsigned char)~'/', (unsigned char)~'d',
	(unsigned char)~'a', (unsigned char)~'t', (unsigned char)~'a', 0
};
/* ux0:umass */
static const unsigned char encodedUmass[16] = {
	(unsigned char)~'u', (unsigned char)~'x', (unsigned char)~'0', (unsigned char)~':',
	(unsigned char)~'u', (unsigned char)~'m', (unsigned char)~'a', (unsigned char)~'s',
	(unsigned char)~'s', 0
};

static char g_appmgrMountByIdComponent[32];
static char g_appmgrMountByIdRoot[32];
static char g_appmgrCachePath[32];
static char g_appmgrPsmPath[32];
static char g_appmgrAppWorkPath[32];
static char g_appmgrLoopBackPath[64];
static char g_appmgrInstallDirPath[32];
static SceUID g_appmgrLoopBackOwner = -1;

/* A title ID: XXXXNNNNN (X upper case, N digit). */
static __attribute__((noinline)) int appmgrIsTitleId(const char *id)
{
	return APPMGR_IS_UPPER(id[0]) && APPMGR_IS_UPPER(id[1]) &&
	       APPMGR_IS_UPPER(id[2]) && APPMGR_IS_UPPER(id[3]) &&
	       APPMGR_IS_DIGIT(id[4]) && APPMGR_IS_DIGIT(id[5]) &&
	       APPMGR_IS_DIGIT(id[6]) && APPMGR_IS_DIGIT(id[7]) &&
	       APPMGR_IS_DIGIT(id[8]) && id[9] == '\0';
}

/* A title ID with a two digit suffix: XXXXNNNNN_NN (a trophy set, a gift). */
static inline int appmgrIsTitleIdWithSuffix(const char *id)
{
	return APPMGR_IS_UPPER(id[0]) && APPMGR_IS_UPPER(id[1]) &&
	       APPMGR_IS_UPPER(id[2]) && APPMGR_IS_UPPER(id[3]) &&
	       APPMGR_IS_DIGIT(id[4]) && APPMGR_IS_DIGIT(id[5]) &&
	       APPMGR_IS_DIGIT(id[6]) && APPMGR_IS_DIGIT(id[7]) &&
	       APPMGR_IS_DIGIT(id[8]) && id[9] == '_' &&
	       APPMGR_IS_DIGIT(id[10]) && APPMGR_IS_DIGIT(id[11]) && id[12] == '\0';
}

/* A path in the account's folder of ur0:user: root/account/component[/id]. */
static inline __attribute__((always_inline))
int appmgrMakeAccountPath(char *path, const unsigned char *component, const char *id,
				 const char *format)
{
	int result;

	result = appmgrDecodeString(encodedUserRoot, g_appmgrMountByIdRoot,
				    sizeof(g_appmgrMountByIdRoot));
	if (result < 0)
		return result;
	result = appmgrDecodeString(component, g_appmgrMountByIdComponent,
				    sizeof(g_appmgrMountByIdComponent));
	if (result < 0)
		return result;
	if (g_appmgrAccountDirectory[0] == 0)
		appmgrUpdateAccountDirectory();
	if ((unsigned int)snprintf(path, 64, format, g_appmgrMountByIdRoot,
				   g_appmgrAccountDirectory, g_appmgrMountByIdComponent, id) > 63)
		return 0x80800001;
	return 0;
}

/*
 * Mounts a system directory by its mount ID for the calling process, the ID (a title ID,
 * a trophy set, an image name) naming the directory where the mount ID needs one. Mount
 * ID 300 clones the temporary mounts of process pid instead.
 */
static inline __attribute__((always_inline))
int appmgrMountByIdBody(SceUID pid, unsigned int mountId, const char *id, const void *keystone,
			ScePfsRndDriveId *mountPoint)
{
	SceUInt32 syscallState = sceSyscallEnter();
	char titleId[16] = {0};
	unsigned char currentKeystone[96];
	char buffer[64];
	struct AppState *app, *source;
	const char *path, *drive = NULL;
	int result, i;

	result = sceKernelLockMutex(g_appmgrMountMutex, 1, NULL);
	if (result < 0)
		goto out;
	result = 0x80800009;
	if (!appmgrIsMountIdAllowed(mountId))
		goto unlock;
	if (mountPoint == NULL)
		goto invalid;

	switch (mountId) {
	case 104:
	case 303:
	case 304:
		if (id == NULL || !appmgrIsTitleIdWithSuffix(id))
			goto invalid;
		break;
	case 106:
	case 107:
	case 205:
	case 207:
	case 500 ... 503:
		if (id == NULL || !appmgrIsTitleId(id))
			goto invalid;
		break;
	case 110:
		if (id == NULL)
			goto invalid;
		break;
	case 600:
		if (id == NULL)
			goto invalid;
		for (i = 0; i < 32; i++) {
			if ((unsigned int)(id[i] - 'A') > 'F' - 'A' && !APPMGR_IS_DIGIT(id[i]))
				goto invalid;
		}
		if (id[32] != '\0')
			goto invalid;
		break;
	}

	memset(g_appmgrMountByIdComponent, 0, sizeof(g_appmgrMountByIdComponent));
	memset(g_appmgrMountByIdRoot, 0, sizeof(g_appmgrMountByIdRoot));
	result = 0;
	path = buffer;
	switch (mountId) {
	case 100:
		result = appmgrDecodeString(encodedPicture, buffer, sizeof(buffer));
		drive = "photo0:";
		break;
	case 101:
		result = appmgrMakeAccountPath(buffer, encodedPsnFriend, NULL, "%s/%s/%s");
		break;
	case 102:
		result = appmgrMakeAccountPath(buffer, encodedPsnMsg, NULL, "%s/%s/%s");
		break;
	case 103:
		result = appmgrMakeAccountPath(buffer, encodedNearDataSys, NULL, "%s/%s/%s");
		break;
	case 104:
		result = appmgrMakeAccountPath(buffer, encodedGiftReceived, id, "%s/%s/%s/%s");
		drive = "gift0:";
		break;
	case 105:
		result = appmgrDecodeString(encodedMusic, buffer, sizeof(buffer));
		drive = "music0:";
		break;
	case 106:
		snprintf(g_appmgrPsmPath, sizeof(g_appmgrPsmPath), "ux0:psm/%s/RO", id);
		path = g_appmgrPsmPath;
		break;
	case 107:
		snprintf(g_appmgrPsmPath, sizeof(g_appmgrPsmPath), "ux0:psm/%s/RW", id);
		path = g_appmgrPsmPath;
		break;
	case 108:
		result = appmgrDecodeString(encodedCalendar, buffer, sizeof(buffer));
		break;
	case 109:
		result = appmgrDecodeString(encodedVideo, buffer, sizeof(buffer));
		drive = "video0:";
		break;
	case 110:
		result = appmgrDecodeString(encodedAppMeta, g_appmgrMountByIdRoot,
					    sizeof(g_appmgrMountByIdRoot));
		if (result < 0)
			break;
		if ((unsigned int)snprintf(buffer, sizeof(buffer), "%s/%s",
					   g_appmgrMountByIdRoot, id) > 63)
			goto invalid;
		break;
	case 111:
		result = appmgrDecodeString(encodedNpHome, buffer, sizeof(buffer));
		break;
	case 112:
		result = appmgrDecodeString(encodedBook, buffer, sizeof(buffer));
		break;
	case 200:
		result = appmgrDecodeString(encodedSqlite, buffer, sizeof(buffer));
		break;
	case 201:
		result = appmgrDecodeString(encodedAttach, buffer, sizeof(buffer));
		break;
	case 202:
		result = appmgrDecodeString(encodedPspEmu, buffer, sizeof(buffer));
		break;
	case 203:
		result = appmgrDecodeString(encodedGriefReport, buffer, sizeof(buffer));
		break;
	case 204:
		result = appmgrDecodeString(encodedCheckout, buffer, sizeof(buffer));
		break;
	case 205:
		snprintf(g_appmgrCachePath, sizeof(g_appmgrCachePath), "ux0:cache/%s", id);
		path = g_appmgrCachePath;
		break;
	case 206:
		result = appmgrDecodeString(encodedWebBrowser, buffer, sizeof(buffer));
		break;
	case 207:
		snprintf(g_appmgrAppWorkPath, sizeof(g_appmgrAppWorkPath),
			 "ux0:temp/app_work/%s/rec", id);
		path = g_appmgrAppWorkPath;
		break;
	case 300:
		path = NULL;
		break;
	case 302:
		result = appmgrMakeAccountPath(buffer, encodedTrophySceTrop, NULL, "%s/%s/%s");
		break;
	case 303:
		result = appmgrMakeAccountPath(buffer, encodedTrophyData, id, "%s/%s/%s/%s");
		break;
	case 304:
		result = appmgrMakeAccountPath(buffer, encodedTrophyData, id, "%s/%s/%s/%s_BK");
		break;
	case 400:
		result = appmgrDecodeString(encodedMmsPhoto, buffer, sizeof(buffer));
		break;
	case 401:
		result = appmgrDecodeString(encodedMmsMusic, buffer, sizeof(buffer));
		break;
	case 402:
		result = appmgrDecodeString(encodedMmsVideo, buffer, sizeof(buffer));
		break;
	case 500:
		snprintf(g_appmgrInstallDirPath, sizeof(g_appmgrInstallDirPath),
			 "grw0:addcont/%s", id);
		path = g_appmgrInstallDirPath;
		break;
	case 501:
		snprintf(g_appmgrInstallDirPath, sizeof(g_appmgrInstallDirPath),
			 "ux0:addcont/%s", id);
		path = g_appmgrInstallDirPath;
		break;
	case 502:
		snprintf(g_appmgrInstallDirPath, sizeof(g_appmgrInstallDirPath),
			 "%spatch/%s", "grw0:", id);
		path = g_appmgrInstallDirPath;
		break;
	case 503:
		snprintf(g_appmgrInstallDirPath, sizeof(g_appmgrInstallDirPath),
			 "%spatch/%s", "ux0:", id);
		path = g_appmgrInstallDirPath;
		break;
	case 504:
		if (g_appmgrAccountDirectory[0] == 0)
			appmgrUpdateAccountDirectory();
		snprintf(g_appmgrInstallDirPath, sizeof(g_appmgrInstallDirPath),
			 "ux0:user/%s/savedata_vc", g_appmgrAccountDirectory);
		path = g_appmgrInstallDirPath;
		break;
	case 505:
		if (g_appmgrAccountDirectory[0] == 0)
			appmgrUpdateAccountDirectory();
		snprintf(g_appmgrInstallDirPath, sizeof(g_appmgrInstallDirPath),
			 "ux0:user/%s/_temp_savedata_vc", g_appmgrAccountDirectory);
		path = g_appmgrInstallDirPath;
		break;
	case 600:
		snprintf(g_appmgrLoopBackPath, sizeof(g_appmgrLoopBackPath),
			 "ux0:umass/%s.img", id);
		path = g_appmgrLoopBackPath;
		break;
	case 1007:
		result = appmgrDecodeString(encodedSysExternal, buffer, sizeof(buffer));
		break;
	case 1008:
		result = appmgrDecodeString(encodedDataExternal, buffer, sizeof(buffer));
		break;
	case 1009:
		result = appmgrDecodeString(encodedUserData, buffer, sizeof(buffer));
		break;
	default:
		goto invalid;
	}
	if (result < 0)
		goto unlock;

	app = appmgrFindAppByProcessId(g_appmgrAppStates, sceKernelGetProcessId());
	if (app == NULL)
		goto noApp;
	strncpy(titleId, app->process.appName, 10);
	if ((app->process.content.contentFlags & 0x30) != 0) {
		result = 0x80800048;
		if (!appmgrIsMediaDriveMountable(mountId, app->process.content.contentFlags))
			goto unlock;
	}

	switch (mountId) {
	case 205:
		snprintf(g_appmgrCachePath, sizeof(g_appmgrCachePath), "ux0:cache/%s",
			 app->process.content.originalTitleId);
		path = g_appmgrCachePath;
		goto createDirectories;
	case 300:
		source = appmgrFindAppByProcessId(g_appmgrAppStates, pid);
		if (source == NULL)
			goto noApp;
		strncpy(titleId, source->process.appName, 10);
		result = appmgrCloneTemporaryMounts(sceKernelGetProcessId(), titleId,
						    &source->process.mounts,
						    &app->process.mounts, mountPoint);
		if (result != 0)
			goto unlock;
		result = 0;
		goto unlock;
	case 110:
	case 302 ... 304:
	case 500:
	case 501:
	case 504:
	case 505:
		result = appmgrCreatePfsDirectory(mountId, path, keystone);
		if (result != 0)
			goto unlock;
		break;
	case 600:
		break;
	default:
	createDirectories:
		if ((app->process.content.contentFlags & 0x10) != 0 &&
		    (mountId == 100 || mountId == 105 || mountId == 109)) {
			result = 0x80010013;
			if (appmgrGetFakeNoMemoryCard() != 0)
				goto unlock;
		}
		result = appmgrCreateDirectories(path, 1);
		if (result != 0)
			goto unlock;
		break;
	}

	result = appmgrMount(sceKernelGetProcessId(), &app->process.mounts, mountId, titleId,
			     path, (const ScePfsRndDriveId *)drive, NULL, mountPoint);
	if (result != 0)
		goto unlock;
	if (mountId == 500 || mountId == 501) {
		result = appmgrReadKeystone(sceKernelGetProcessId(), mountPoint->drive_id,
					    currentKeystone);
		if (result < 0)
			goto unmount;
		if (timingsafe_memcmp(keystone, currentKeystone, sizeof(currentKeystone)) != 0) {
			result = 0x80800006;
			goto unmount;
		}
	}
	if ((app->process.content.contentFlags & 0x30) != 0) {
		switch (mountId) {
		case 100:
			app->process.content.contentFlags |= 0x2000000;
			break;
		case 105:
			app->process.content.contentFlags |= 0x4000000;
			break;
		case 109:
			app->process.content.contentFlags |= 0x8000000;
			break;
		}
	}
	result = 0;
	goto unlock;

unmount:
	if (mountPoint->drive_id[0] != '\0')
		appmgrUnmountDrive(sceKernelGetProcessId(), &app->process.mounts,
				   mountPoint->drive_id, 0);
	goto unlock;
noApp:
	result = 0x80801002;
	goto unlock;
invalid:
	result = 0x80800001;
unlock:
	sceKernelUnlockMutex(g_appmgrMountMutex, 1);
out:
	sceSyscallLeave(syscallState);
	return result;
}

int appmgrMountById(SceUID pid, unsigned int mountId, const char *id, const void *keystone,
		    ScePfsRndDriveId *mountPoint)
{
	return appmgrMountByIdBody(pid, mountId, id, keystone, mountPoint);
}

/* A mount ID that names its directory without an ID or keystone. */
int appmgrMountSystemDirectory(SceUID pid, unsigned int mountId, ScePfsRndDriveId *mountPoint)
{
	return appmgrMountByIdBody(pid, mountId, NULL, NULL, mountPoint);
}

/* SceAppMgrForDriver 5E311F71: mounts an application data directory by its mount ID. */
int SceAppMgrForDriver_5E311F71(unsigned int mountId, const char *id,
				ScePfsRndDriveId *mountPoint)
{
	switch (mountId) {
	case 104:
	case 106:
	case 107:
	case 110:
		return appmgrMountById(0, mountId, id, NULL, mountPoint);
	default:
		return 0x80800001;
	}
}

#if !FW_SINCE(0x04000000)
/* Mounts a trophy set's directory, or its backup. */
int sceAppMgrTrophyMountById(unsigned int mountId, const char *id, ScePfsRndDriveId *mountPoint)
{
	if (mountId - 303 > 1)
		return 0x80800001;
	return appmgrMountById(0, mountId, id, NULL, mountPoint);
}
#endif

/* Mounts an install directory: add-on content, a patch or the save data being installed. */
int sceAppMgrInstallDirMount(unsigned int mountId, const char *id, const void *keystone,
			     ScePfsRndDriveId *mountPoint)
{
	if (mountId - 500 > 5)
		return 0x80800001;
	return appmgrMountById(0, mountId, id != NULL ? id : "", keystone, mountPoint);
}

/* Mounts a loopback image, ux0:umass/<id>.img. */
int sceAppMgrLoopBackMount(unsigned int mountId, const char *id, ScePfsRndDriveId *mountPoint)
{
	if (mountId != 600)
		return 0x80800001;
	return appmgrMountById(sceKernelGetProcessId(), 600, id, NULL, mountPoint);
}

/* SceAppMgrForDriver B1D3C287: mounts a system application data directory. */
int SceAppMgrForDriver_B1D3C287(unsigned int mountId, ScePfsRndDriveId *mountPoint)
{
	switch (mountId) {
	case 100 ... 103:
	case 105:
	case 108:
	case 109:
	case 111:
	case 112:
		return appmgrMountSystemDirectory(0, mountId, mountPoint);
	default:
		return 0x80800001;
	}
}

/* SceAppMgrForDriver 3A0A9B82: mounts a work directory; the PSP emulator's only unsandboxed. */
int SceAppMgrForDriver_3A0A9B82(unsigned int mountId, ScePfsRndDriveId *mountPoint)
{
	if (mountId == 202) {
		if (SceSblACMgrForDriver_0B6E6CD7(0) == 0)
			return 0x80800009;
		return appmgrMountSystemDirectory(0, mountId, mountPoint);
	}
	switch (mountId) {
	case 200 ... 204:
	case 206:
		return appmgrMountSystemDirectory(0, mountId, mountPoint);
	default:
		return 0x80800001;
	}
}

/* Mounts the PSP emulator's save data root (ux0:pspemu). */
int sceAppMgrPspSaveDataRootMount(ScePfsRndDriveId *mountPoint)
{
	if (SceSblACMgrForDriver_6D8A88B7(0) == 0)
		return 0x80800009;
	return appmgrMountSystemDirectory(0, 202, mountPoint);
}

/* SceAppMgrForDriver E977A833: mounts process pid's temporary mounts (300) or the trophy data (302). */
int SceAppMgrForDriver_E977A833(unsigned int mountId, SceUID pid, ScePfsRndDriveId *mountPoint)
{
	switch (mountId) {
	case 300:
	case 302:
		return appmgrMountSystemDirectory(pid, mountId, mountPoint);
	default:
		return 0x80800001;
	}
}

/* SceAppMgrForDriver 13D54107: mounts a multimedia directory (ux0:mms). */
int SceAppMgrForDriver_13D54107(unsigned int mountId, ScePfsRndDriveId *mountPoint)
{
	if (mountId - 400 > 2)
		return 0x80800001;
	return appmgrMountSystemDirectory(0, mountId, mountPoint);
}

static char g_appmgrContentPath[1024];

/* the drives a process without access to them sees through an invalid overlay */
static const char *g_appmgrOverlayDrives[] = {
	"gro0:", "grw0:", "os0:", "ur0:", "ux0:", "vd0:", "vs0:", "pd0:", "tm0:", "ud0:",
	NULL
};

int appmgrMountProcessContent(SceUID pid, SceUInt32 mountFlags, const char *path,
			      const char *titleId, const AppmgrLaunchOptions *options,
			      AppmgrMountHolder *mounts, int launchOrigin,
			      AppmgrProcessContent *content)
{
	ScePfsRndDriveId appDrive = {{0}}, saveDataDrive = {{0}}, addContDrive = {{0}};
	ScePfsRndDriveId sysExternalDrive = {{0}}, dataExternalDrive = {{0}};
	ScePfsRndDriveId patchDrive = {{0}}, pfsDrive, userDataDrive;
	AppmgrNotificationRequest request;
	SceSelfAuthInfo authInfo;
	AppmgrSfoFile file;
	SceFiosOverlay overlay;
	SceFiosOverlayID overlayId;
	SceAppMgrAppInfoCache *info;
	AppmgrPfsMount *pfsMount;
	AppmgrVirtualMount *virtualMount;
	const AppmgrMapFile *mapFile;
	const AppmgrMapFileOverlay *mapOverlay;
	const char *const *drive;
	const char *message, *name, *slash, *backslash, *colon;
	const void *klicensee;
	unsigned char sealedKey[80], key[16];
	char decoded[64];
	char *normalized, *patchPath, *sfoPath, *workPath, *pfsPath;
	void *storage, *value;
	SceSize length, capacity;
	SceUInt64 accountId;
	SceUInt32 flags, mountId;
	SceUID owner;
	int result, status, permission, development, hasKlicensee, ownerFlag, consoleType;
	int index, saved;
#if !FW_SINCE(0x04000000)
	SceUInt64 programAuthorityId;
#endif

	memset(g_appmgrContentPath, 0, sizeof(g_appmgrContentPath));
	memset(content->overlayIds, 0, sizeof(content->overlayIds));
	result = sceKernelLockMutex(g_appmgrMountMutex, 1, NULL);
	if (result < 0)
		return result;

	if (SceSblACMgrForDriver_6C5AB07F(pid)) {
		if (!(content->selfControlFlags & 2)) {
			result = 0x8080001c;
			goto failed;
		}
		content->contentFlags |= 0x20;
	}
	if (sceSblACMgrIsGameProgram(pid))
		content->contentFlags |= 0x10;
	if (!SceSblACMgrForDriver_426A4E8C(pid))
		content->contentFlags |= 8;
	if (SceSblACMgrForDriver_A67E8E5B(pid))
		content->contentFlags |= 0x80;
	if (launchOrigin == 2)
		content->launchFlags |= 2;
	result = appmgrInitAppInfoCache(pid);
	if (result < 0)
		goto failed;

	if (mountFlags & 1) {
		/* app0: the application, unless its directory is mounted as an application image */
		result = appmgrResolveLaunchDirectory(path, options, content->mapFile,
						      g_appmgrContentPath,
						      sizeof(g_appmgrContentPath));
		if (result != 0)
			goto failed;
		pfsMount = g_appmgrPfsMounts;
		normalized = appmgrAllocMountPath();
		if (normalized == NULL) {
			result = 0x80801006;
			goto failed;
		}
		result = appmgrNormalizePath(g_appmgrContentPath, normalized);
		if (result < 0) {
			sceKernelFreeHeapMemory(g_appmgrMountHeap, normalized);
			if (result != 0x80800002)
				goto failed;
		} else {
			for (; pfsMount != NULL; pfsMount = pfsMount->next) {
				if (pfsMount->mountId == 900 &&
				    strncmp(pfsMount->appPath, normalized, 292) == 0) {
					sceKernelFreeHeapMemory(g_appmgrMountHeap, normalized);
					goto alreadyMounted;
				}
			}
			sceKernelFreeHeapMemory(g_appmgrMountHeap, normalized);
		}
		result = appmgrMount(pid, mounts, 1000, titleId, g_appmgrContentPath, NULL,
				     (const ScePfsRndDriveId *)content->license.klicense,
				     &appDrive);
		if (result != 0)
			goto failed;
#if !FW_SINCE(0x04000000)
		/* bind the application's PFS image to this program */
		if (sceKernelGetProcessSelfAuthInfo(pid, &authInfo) >= 0) {
			programAuthorityId = authInfo.programAuthorityId;
			pfsMount = g_appmgrPfsMounts;
			normalized = appmgrAllocMountPath();
			if (normalized != NULL) {
				if (appmgrNormalizePath(g_appmgrContentPath, normalized) < 0)
					pfsMount = NULL;
				for (; pfsMount != NULL; pfsMount = pfsMount->next)
					if (strncmp(pfsMount->appPath, normalized, 292) == 0)
						break;
				sceKernelFreeHeapMemory(g_appmgrMountHeap, normalized);
				if (pfsMount != NULL) {
					status = ScePfsMgrForKernel_A772209C(g_appmgrContentPath,
							pfsMount->mountPoint.drive_id,
							programAuthorityId,
							content->license.klicense, 40);
					if (strncmp("ux0:", g_appmgrContentPath, 4) == 0 &&
					    status != 2 && SceSblAIMgrForDriver_D78B04A2() == 1) {
						result = 0x8080004c;
						goto failed;
					}
				}
			}
		}
#endif

		/* the patch, mounted over app0: */
		patchPath = appmgrAllocMountPath();
		if (patchPath == NULL) {
			result = 0x80801006;
			goto failed;
		}
		memset(patchPath, 0, 292);
		if (appmgrGetPatchPath(content->appDirectory, content->patchTitleId,
				       content->attribute, content->category, patchPath) == 0 &&
		    appmgrCreateDirectories(patchPath, 0) >= 0) {
			result = appmgrMount(pid, mounts, 1001, content->patchTitleId, patchPath,
					     NULL,
					     (const ScePfsRndDriveId *)content->license.klicense,
					     &patchDrive);
			sceKernelFreeHeapMemory(g_appmgrMountHeap, patchPath);
			if (result < 0)
				goto failed;
		} else {
			sceKernelFreeHeapMemory(g_appmgrMountHeap, patchPath);
		}

		/* the application's parameters, or what the launcher knows of them */
		appDrive = (ScePfsRndDriveId){ "app0:" };
		result = appmgrReadParamSfo(pid, "app0:/sce_sys/param.sfo", 0, content);
		if (result < 0) {
			if (result == 0x80010002)
				result = 0x80800013;
			if (!(content->launchFlags & 2))
				goto failed;
			flags = content->contentFlags;
			content->contentFlags = flags | 2;
			if (flags & 1) {
				if (flags & 0x10)
					strncpy(content->category, appmgrProgramCategories[1], 4);
				else if (flags & 0x20)
					__builtin_memcpy(content->category, (flags & 0x40) ?
							 appmgrProgramCategories[5] :
							 appmgrProgramCategories[3], 4);
				else
					__builtin_strcpy(content->category, "gda");
				content->category[3] = '\0';
				strncpy(content->titleId, titleId, 10);
				content->titleId[9] = '\0';
				__builtin_memcpy(content->patchTitleId, content->titleId, 16);
				__builtin_memcpy(content->originalTitleId, content->titleId, 16);
			}
			content->attribute = 0;
			content->attribute2 = 0;
			content->appVersion = 0;
			content->savedataMaxSize = 0;
			__builtin_memcpy(content->saveDataTitleIds[0], content->titleId, 16);
			__builtin_memcpy(content->addContTitleIds[0], content->titleId, 16);
			strncpy(content->contentId, "", 48);
			content->contentId[47] = '\0';
			strncpy(content->npCommunicationId, "", 13);
			content->npCommunicationId[12] = '\0';
		}

		/* a non-game program's own category and title from param2.sfo */
		if (content->contentFlags & 0x40) {
			file = (AppmgrSfoFile){ 0 };
			value = NULL;
			length = 0;
			capacity = 0;
			owner = pid ? pid : sceKernelGetProcessId();
			SceKernelSuspendForDriver_4DF40893(0);
			sfoPath = appmgrAllocMountPath();
			if (sfoPath == NULL) {
				result = 0x80801006;
				appmgrReleaseSfoFile(&file);
				SceKernelSuspendForDriver_2BB92967(0);
				goto param2Failed;
			}
			memset(sfoPath, 0, 292);
			permission = sceKernelSetThreadAccessLevel(128);
			result = SceFios2KernelForDriver_0F456345(owner, 0, "app0:/sce_sys/param2.sfo",
								  sfoPath, 292);
			sceKernelSetThreadAccessLevel(permission);
			if (result != 0) {
				sceKernelFreeHeapMemory(g_appmgrMountHeap, sfoPath);
				appmgrReleaseSfoFile(&file);
				SceKernelSuspendForDriver_2BB92967(0);
				if (result >= 0)
					goto checkCategory;
				goto param2Failed;
			}
			result = appmgrLoadSfoFile(sfoPath, &file);
			if (result < 0)
				goto param2Release;
			result = appmgrGetSfoEntry(&file, "CATEGORY", &value, &length, &capacity);
			if (result < 0)
				goto param2Release;
			if (length > 2 && capacity == 4) {
				if (strncmp(value, appmgrProgramCategories[4], 4) != 0)
					strncpy(content->category, value, 4);
				else
					__builtin_strcpy(content->category, "gdd");
				content->category[3] = '\0';
				result = appmgrGetSfoEntry(&file, "TITLE_ID", &value, &length,
							   &capacity);
				if (result < 0)
					goto param2Release;
				if (length > 9 && capacity == 12) {
					strncpy(content->titleId, value, 10);
					content->titleId[9] = '\0';
					sceKernelFreeHeapMemory(g_appmgrMountHeap, sfoPath);
					appmgrReleaseSfoFile(&file);
					SceKernelSuspendForDriver_2BB92967(0);
					goto checkCategory;
				}
			}
			result = 0x80800034;
param2Release:
			sceKernelFreeHeapMemory(g_appmgrMountHeap, sfoPath);
			appmgrReleaseSfoFile(&file);
			SceKernelSuspendForDriver_2BB92967(0);
param2Failed:
			if (!(content->contentFlags & 1))
				goto failed;
		}

checkCategory:
		flags = content->contentFlags;
		if (flags & 0x20) {
			if (flags & 0x40) {
				if (strncmp(content->category, appmgrProgramCategories[5], 4) != 0)
					goto badCategory;
			} else if (strncmp(content->category, appmgrProgramCategories[3], 4) != 0) {
				goto badCategory;
			}
			if (!appmgrIsTitleId(content->titleId) ||
			    !appmgrIsTitleId(content->addContTitleIds[0]) ||
			    !appmgrIsTitleId(content->saveDataTitleIds[0]) ||
			    !appmgrIsTitleId(content->originalTitleId))
				goto badCategory;
		} else if (flags & 0x10) {
			if (strncmp(content->category, appmgrProgramCategories[1], 3) != 0 ||
			    !appmgrIsValidTitleId(content->titleId, 1) ||
			    !appmgrIsValidTitleId(content->addContTitleIds[0], 1) ||
			    !appmgrIsValidTitleId(content->saveDataTitleIds[0], 1))
				goto badCategory;
		}

		/* a game's keystone, checked against its license */
		memset(content->keystone, 0, sizeof(content->keystone));
		if (strncmp(content->category, appmgrProgramCategories[1], 3) == 0 ||
		    strncmp(content->category, appmgrProgramCategories[3], 4) == 0 ||
		    strncmp(content->category, appmgrProgramCategories[5], 4) == 0) {
			result = appmgrReadKeystone(pid, appDrive.drive_id, content->keystone);
			if (result < 0) {
				if (!(content->contentFlags & 1))
					goto failed;
			} else {
				hasKlicensee = 0;
				for (index = 0; index < 16; index++) {
					if (content->license.klicense[index] != 0) {
						hasKlicensee = 1;
						break;
					}
				}
				result = SceSblPostSsMgrForDriver_DDA6FA6D(content->keystone,
									  hasKlicensee);
				if (result < 0)
					goto failed;
			}
		}
	}

	storage = NULL;
	result = SceProcessmgrForDriver_AF80F39C(pid, g_appmgrAppInfoCacheKey, &storage, 0);
	if (result < 0)
		goto failed;
	if (storage == NULL) {
		result = 0x80800012;
		goto failed;
	}
	info = storage;
	memset(info->titleId, 0, sizeof(info->titleId));
	strncpy(info->titleId, content->titleId, 9);

	if (mountFlags & 4)
		appmgrMountAddCont(pid, titleId, mounts, content, &addContDrive);

	if (!(mountFlags & 2))
		goto systemMounts;
	if (sceKernelGetProcessSelfAuthInfo(pid, &authInfo) >= 0 &&
	    (authInfo.programAuthorityId == 0x2800000000000001ULL ||
	     authInfo.programAuthorityId == 0x2800000000007001ULL))
		goto systemMounts;

	/* savedata0: where the save data is */
	if (SceSblACMgrForDriver_5C4BC352(pid)) {
		strncpy(content->saveDataTitleIds[0], titleId, 10);
		content->saveDataTitleIds[0][9] = '\0';
	}
	mapFile = content->mapFile;
	memset(g_appmgrContentPath, 0, sizeof(g_appmgrContentPath));
	if (!(content->contentFlags & 0x80) || !(content->launchFlags & 2) ||
	    SceSblAIMgrForDriver_D78B04A2() || SceSblAIMgrForDriver_F4B98F66() ||
	    !SceSblACMgrForDriver_E87D1777()) {
		if ((content->contentFlags & 0x18) == 0x18)
			goto defaultSaveData;
	}
	if (SceSblAIMgrForDriver_D78B04A2() || SceSblAIMgrForDriver_F4B98F66())
		goto defaultSaveData;
	if (SceSblACMgrForDriver_E87D1777() && mapFile != NULL && mapFile->file > 0 &&
	    mapFile->savedata0[0] != '\0') {
		strncpy(g_appmgrContentPath, mapFile->savedata0, sizeof(g_appmgrContentPath));
		goto findSaveData;
	}
	if ((content->contentFlags & 0x30) == 0)
		goto defaultSaveData;
	if (appmgrCreateDirectories("host0:savedata", 0) == 0) {
		__builtin_strcpy(g_appmgrContentPath, "host0:savedata");
		goto findSaveData;
	}
	__builtin_strcpy(g_appmgrContentPath, "ux0:data/savedata");
	if (content->contentFlags & 1)
		goto findSaveData;
	result = appmgrCreateDirectories("ux0:data/savedata", 0);
	if (result >= 0) {
		if (result != 0)
			goto saveDataPathFailed;
		goto findSaveData;
	}
defaultSaveData:
	result = appmgrGetSaveDataPath(content->appDirectory, content->saveDataTitleIds[0],
				       content->attribute, content->category,
				       g_appmgrContentPath, sizeof(g_appmgrContentPath));
	if (result != 0 && result != 0x80800020) {
saveDataPathFailed:
		if (result != 0x8080000b)
			goto failed;
		goto systemMounts;
	}

findSaveData:
	result = appmgrFindPfsMount(g_appmgrPfsMounts, 901, g_appmgrContentPath, NULL);
	if (result != 0x80800002) {
		if (result != 0)
			goto failed;
		goto alreadyMounted;
	}

	/* a new save data directory becomes a PFS image first */
	if (strncmp(g_appmgrContentPath, "grw0:", 5) == 0)
		mountId = 1005;
	else
		mountId = strncmp(g_appmgrContentPath, "ux0:", 4) == 0 ? 1005 : 1006;
	workPath = appmgrAllocMountPath();
	if (workPath == NULL) {
		result = 0x80801006;
		goto failed;
	}
	if (appmgrIsDevelopmentPath(g_appmgrContentPath)) {
		status = appmgrCreateDirectories(g_appmgrContentPath, 0);
		if (status < 0)
			goto pfsFailed;
		development = 1;
	} else {
		status = appmgrCreateDirectories(g_appmgrContentPath, 1);
		if (status < 0)
			goto pfsFailed;
		development = 0;
	}
	snprintf(workPath, 292, "%s/sce_pfs", g_appmgrContentPath);
	if (appmgrCreateDirectories(workPath, 0) == 0)
		goto pfsExists;
	snprintf(workPath, 292, "%s/sce_sys", g_appmgrContentPath);
	if (appmgrCreateDirectories(workPath, 0) == 0) {
pfsExists:
		sceKernelFreeHeapMemory(g_appmgrMountHeap, workPath);
		goto mountSaveData;
	}
	if (development) {
		pfsPath = g_appmgrContentPath;
	} else {
		snprintf(workPath, 292, "%s_TMP", g_appmgrContentPath);
		appmgrRemoveDirectoryTree(workPath);
		status = sceIoRename(g_appmgrContentPath, workPath);
		if (status < 0)
			goto pfsFailed;
		pfsPath = workPath;
	}
	if (mountId != 1006) {
		memset(sealedKey, 0, sizeof(sealedKey));
		memset(key, 0, sizeof(key));
		memset(&pfsDrive, 0, sizeof(pfsDrive));
		status = sceSblSsGenerateSealedKey(sealedKey);
		if (status < 0)
			goto pfsFailed;
		status = sceSblSsDecryptSealedKey(sealedKey, key);
		if (status < 0)
			goto pfsFailed;
		if (development)
			memset(key, 0, sizeof(key));
		status = appmgrMakeRandomDriveId("ts", pfsDrive.drive_id);
		if (status < 0)
			goto pfsFailed;
		status = sceKernelGetProcessSelfAuthInfo(0, &authInfo);
		if (status < 0)
			goto pfsFailed;
		klicensee = NULL;
		for (index = 0; index < 16; index++) {
			if (key[index] != 0) {
				klicensee = key;
				break;
			}
		}
		status = ScePfsMgrForKernel_A772209C(pfsPath, pfsDrive.drive_id,
						     authInfo.programAuthorityId, klicensee, 5);
		if (status < 0)
			goto pfsFailed;
	}
	slash = strrchr(g_appmgrContentPath, '/');
	backslash = strrchr(g_appmgrContentPath, '\\');
	colon = strrchr(g_appmgrContentPath, ':');
	name = slash;
	if (slash <= backslash) {
		if (backslash > colon)
			name = backslash;
		else if (colon != NULL)
			name = colon + 1;
		else
			name = NULL;
	}
	status = appmgrCreateSaveDataMetadata(pid, pfsPath, name, mountId, sealedKey,
					      content->keystone, content->titleId);
	if (mountId != 1006)
		ScePfsMgrForKernel_680BC384(pfsDrive.drive_id);
	if (!development) {
		if (status < 0) {
			appmgrRemoveDirectoryTree(workPath);
			goto pfsFailed;
		}
		status = sceIoRename(workPath, g_appmgrContentPath);
	}
	sceKernelFreeHeapMemory(g_appmgrMountHeap, workPath);
	if (status < 0)
		goto saveDataFailed;
	if (status != 0)
		goto checkSaveData;

mountSaveData:
	status = appmgrMount(pid, mounts, mountId, titleId, g_appmgrContentPath, NULL, NULL,
			     &saveDataDrive);
	if (status < 0)
		goto saveDataMissing;

checkSaveData:
	if (strncmp(g_appmgrContentPath, "ur0:", 4) != 0) {
		status = appmgrValidateSaveDataMount(pid, g_appmgrContentPath, 0, content, NULL);
		if (status < 0) {
			result = status;
			goto failed;
		}
		for (virtualMount = mounts->firstMount; virtualMount != NULL;
		     virtualMount = virtualMount->next)
			if (strncmp(virtualMount->driveId.drive_id, saveDataDrive.drive_id,
				    16) == 0)
				break;
		if (virtualMount == NULL)
			goto failed;
		pfsMount = virtualMount->pfsMount;
		result = 0;
		if (pfsMount->mountId == 1005 && pfsMount->mountPoint.drive_id[0] != '\0' &&
		    content->savedataMaxSize != 0) {
			result = ScePfsMgrForKernel_806D2EA1(pfsMount->mountPoint.drive_id,
							     content->savedataMaxSize << 10);
			if (result < 0) {
				if (result == 0x8001001c)
					result = 0x80800019;
				goto failed;
			}
		}
		status = result;
	}
	goto notify;

pfsFailed:
	sceKernelFreeHeapMemory(g_appmgrMountHeap, workPath);
saveDataFailed:
	if (status != 0x80010002 && status != 0x80010013) {
		if (status == 0x8001001c) {
			result = 0x80800016;
			goto failed;
		}
		result = status;
		goto failed;
	}
saveDataMissing:
	if ((content->attribute & 0x100001) == 0x100001) {
		/* a fake save data owner (devkits) */
		content->savedataEnvironmentFlags = 0;
		ownerFlag = 0;
		consoleType = appmgrGetConsoleType();
		if (consoleType == 0)
			content->savedataEnvironmentFlags |= 1;
		else if (consoleType == 1)
			content->savedataEnvironmentFlags |= 2;
		else
			content->savedataEnvironmentFlags |= 4;
		if (sceRegMgrGetKeyInt("/CONFIG/GAME", "fake_savedata_owner", &ownerFlag) < 0)
			ownerFlag = 0;
		else if (ownerFlag == 1)
			content->savedataEnvironmentFlags |= 8;
		content->savedataOwnershipFlags = 0;
		result = appmgrGetAccountIdentity(&accountId, NULL, NULL);
		if (result < 0)
			goto failed;
		if (accountId == 0)
			content->savedataOwnershipFlags |= 2;
		content->savedataAttribute = 0;
		content->savedataVersion = 0;
	} else if (!(content->contentFlags & 1)) {
		result = status;
		goto failed;
	}

notify:
	if ((content->contentFlags & 0x30) == 0)
		goto systemMounts;
	memset(&request, 0, sizeof(request));
	switch (status) {
	case 0:
		snprintf((char *)request.payload, sizeof(request.payload),
			 "AppMgr:\nsavedata0: OK -\n%s", g_appmgrContentPath);
		if (strnlen("AppMgr:\nsavedata0: OK -\n", 60) +
		    strnlen(g_appmgrContentPath, 1024) > 59)
			memset(&request.payload[56], '.', 3);
		break;
	case 0x80010002:
		message = "AppMgr:\nsavedata0: NG -\nSCE_ERROR_ERRNO_ENOENT";
		goto copyMessage;
	case 0x80010013:
		message = "AppMgr:\nsavedata0: NG -\nSCE_ERROR_ERRNO_ENODEV";
copyMessage:
		__builtin_memcpy(request.payload, message,
				 sizeof("AppMgr:\nsavedata0: NG -\nSCE_ERROR_ERRNO_ENOENT"));
		break;
	default:
		snprintf((char *)request.payload, sizeof(request.payload),
			 "AppMgr:\nsavedata0: NG -\n0x%08x", status);
		break;
	}
	_appmgrQueuePush(&request, 1);

systemMounts:
	if (SceSblACMgrForDriver_E79C7A8D(pid, (const SceChar8 *)"vs0:sys/external")) {
		result = appmgrDecodeString(encodedSysExternal, decoded, sizeof(decoded));
		if (result != 0)
			goto failed;
		result = appmgrMount(pid, mounts, 1007, titleId, decoded, NULL, NULL,
				     &sysExternalDrive);
		if (result != 0)
			goto failed;
	}
	result = appmgrDecodeString(encodedDataExternal, decoded, sizeof(decoded));
	if (result != 0)
		goto failed;
	result = appmgrMount(pid, mounts, 1008, titleId, decoded, NULL, NULL, &dataExternalDrive);
	if (result != 0)
		goto failed;

	/* the map file's overlays (development) */
	mapFile = content->mapFile;
	if (mapFile != NULL && mapFile->file > 0) {
		for (index = 0; index != 8; index++) {
			if (!sceAppMgrMapFileIsValidOverlay(index, (AppmgrMapFile *)mapFile))
				continue;
			mapOverlay = &mapFile->overlays[index];
			memcpy(&overlay, &g_appmgrOverlayTemplate, sizeof(overlay));
			overlayId = 0;
			owner = pid ? pid : sceKernelGetProcessId();
			overlay.type = mapOverlay->type;
			overlay.order = mapOverlay->order;
			overlay.src_len = strnlen(mapOverlay->src, 292);
			strncpy(overlay.src, mapOverlay->src, 292);
			overlay.src[291] = '\0';
			overlay.dst_len = strnlen(mapOverlay->dst, 292);
			strncpy(overlay.dst, mapOverlay->dst, 292);
			overlay.dst[291] = '\0';
			overlay.pid = owner;
			status = SceFios2KernelForDriver_17E65A1C(owner, &overlay, &overlayId);
			if (status != 0) {
				result = 0x80800021;
				sceAppMgrMapFilePrintMappingError(status);
				goto failed;
			}
		}
		result = 0;
	}

	/* ux0:user/00/userdata for games */
	if (content->contentFlags & 0x10) {
		result = appmgrDecodeString(encodedUserData, decoded, sizeof(decoded));
		if (result != 0)
			goto failed;
		if (appmgrCreateDirectories(decoded, 0) == 0 &&
		    SceSblACMgrForDriver_5F9AF49C(pid, (const SceChar8 *)"ux0:")) {
			status = appmgrMount(pid, mounts, 1009, content->titleId, decoded, NULL,
					     NULL, &userDataDrive);
			if (status < 0) {
				result = status;
				goto failed;
			}
		}
	}

	/* the drives the process may not see */
	memset(content->overlayIds, 0, sizeof(content->overlayIds));
	saved = result;
	for (drive = g_appmgrOverlayDrives, index = 0; *drive != NULL; drive++, index++) {
		if (SceSblACMgrForDriver_5F9AF49C(pid, (const SceChar8 *)*drive))
			continue;
		status = appmgrAddInvalidOverlay(pid, *drive, &content->overlayIds[index]);
		if (status != 0) {
			result = status;
			if (status < 0)
				goto failed;
			goto fakeNoMemoryCard;
		}
	}
	result = saved;
fakeNoMemoryCard:
	if (content->contentFlags & 0x10) {
		appmgrInitializeFakeNoMemoryCard();
		result = appmgrUpdateFakeNoMemoryCardOverlays(pid, content);
		if (result != 0)
			goto failed;
	} else if (result != 0) {
		goto failed;
	}
	goto unlock;

badCategory:
	result = 0x80800015;
	goto failed;
alreadyMounted:
	result = 0x80800017;
	goto failed;
failed:
	if (saveDataDrive.drive_id[0] != '\0')
		appmgrUnmountDrive(pid, mounts, saveDataDrive.drive_id, 0);
	if (addContDrive.drive_id[0] != '\0')
		appmgrUnmountDrive(pid, mounts, addContDrive.drive_id, 0);
	if (appDrive.drive_id[0] != '\0')
		appmgrUnmountDrive(pid, mounts, appDrive.drive_id, 0);
	if (sysExternalDrive.drive_id[0] != '\0')
		appmgrUnmountDrive(pid, mounts, sysExternalDrive.drive_id, 0);
	if (dataExternalDrive.drive_id[0] != '\0')
		appmgrUnmountDrive(pid, mounts, dataExternalDrive.drive_id, 0);
	for (index = 0; index < 16; index++) {
		if (content->overlayIds[index] == 0)
			continue;
		owner = pid ? pid : sceKernelGetProcessId();
		permission = sceKernelSetThreadAccessLevel(128);
		status = SceFios2KernelForDriver_23247EFB(owner, content->overlayIds[index]);
		sceKernelSetThreadAccessLevel(permission);
		if (status != 0)
			break;
		content->overlayIds[index] = 0;
	}
unlock:
	sceKernelUnlockMutex(g_appmgrMountMutex, 1);
	return result;
}

#if FW_SINCE(0x04000000)
/* Mounts a trophy set's directory, or its backup, for the calling process. */
int sceAppMgrTrophyMountById(unsigned int mountId, const char *id, ScePfsRndDriveId *mountPoint)
{
	SceUInt32 syscallState;
	char titleId[16];
	char path[64];
	struct AppState *app;
	SceUInt32 contentFlags;
	int result;

	if (mountId - 303 > 1)
		return 0x80800001;
	__builtin_memset(titleId, 0, sizeof(titleId));
	syscallState = sceSyscallEnter();
	result = sceKernelLockMutex(g_appmgrMountMutex, 1, NULL);
	if (result < 0)
		goto out;
	result = 0x80800009;
	if (!appmgrIsMountIdAllowed(mountId))
		goto unlock;
	if (mountPoint == NULL || id == NULL || !appmgrIsTitleIdWithSuffix(id))
		goto invalid;
	__builtin_memset(g_appmgrMountByIdRoot, 0, sizeof(g_appmgrMountByIdRoot));
	__builtin_memset(g_appmgrMountByIdComponent, 0, sizeof(g_appmgrMountByIdComponent));
	result = appmgrMakeAccountPath(path, encodedTrophyData, id,
				       mountId == 304 ? "%s/%s/%s/%s_BK" : "%s/%s/%s/%s");
	if (result != 0)
		goto unlock;
	app = appmgrFindAppByProcessId(g_appmgrAppStates, sceKernelGetProcessId());
	if (app == NULL) {
		result = 0x80801002;
		goto unlock;
	}
	strncpy(titleId, app->process.appName, 10);
	contentFlags = app->process.content.contentFlags;
	if ((contentFlags & 0x30) != 0) {
		result = 0x80800048;
		if (!appmgrIsMediaDriveMountable(mountId, contentFlags))
			goto unlock;
	}
	result = appmgrCreatePfsDirectory(mountId, path, NULL);
	if (result == 0)
		result = appmgrMount(sceKernelGetProcessId(), &app->process.mounts, mountId,
				     titleId, path, NULL, NULL, mountPoint);
	goto unlock;

invalid:
	result = 0x80800001;
unlock:
	sceKernelUnlockMutex(g_appmgrMountMutex, 1);
out:
	sceSyscallLeave(syscallState);
	return result;
}
#endif

/* Mounts the calling application's cache (205) or its recording work directory (207). */
int sceAppMgrWorkDirMountById(unsigned int mountId, const char *id, ScePfsRndDriveId *mountPoint)
{
	SceUInt32 syscallState;
	char titleId[16];
	char buffer[64];
	struct AppState *app;
	char *path;
	int result;

	if (mountId != 205 && mountId != 207)
		return 0x80800001;
	__builtin_memset(titleId, 0, sizeof(titleId));
	syscallState = sceSyscallEnter();
	result = sceKernelLockMutex(g_appmgrMountMutex, 1, NULL);
	if (result < 0)
		goto out;
	result = 0x80800009;
	if (!appmgrIsMountIdAllowed(mountId))
		goto unlock;
	if (mountPoint == NULL)
		goto invalid;
	if ((mountId == 205 || mountId == 207) &&
	    (id == NULL ||
	     !(APPMGR_IS_UPPER(id[0]) && APPMGR_IS_UPPER(id[1]) &&
	       APPMGR_IS_UPPER(id[2]) && APPMGR_IS_UPPER(id[3]) &&
	       APPMGR_IS_DIGIT(id[4]) && APPMGR_IS_DIGIT(id[5]) &&
	       APPMGR_IS_DIGIT(id[6]) && APPMGR_IS_DIGIT(id[7]) &&
	       APPMGR_IS_DIGIT(id[8]) && id[9] == '\0')))
		goto invalid;
	__builtin_memset(g_appmgrMountByIdRoot, 0, sizeof(g_appmgrMountByIdRoot));
	__builtin_memset(g_appmgrMountByIdComponent, 0, sizeof(g_appmgrMountByIdComponent));
	if (mountId == 206) {
		result = appmgrDecodeString(encodedWebBrowser, buffer, sizeof(buffer));
		if (result < 0)
			goto unlock;
		path = buffer;
	} else if (mountId == 207) {
		path = g_appmgrAppWorkPath;
		snprintf(g_appmgrAppWorkPath, sizeof(g_appmgrAppWorkPath),
			 "ux0:temp/app_work/%s/rec", id);
	} else {
		path = g_appmgrCachePath;
		snprintf(g_appmgrCachePath, sizeof(g_appmgrCachePath), "ux0:cache/%s", id);
	}
	app = appmgrFindAppByProcessId(g_appmgrAppStates, sceKernelGetProcessId());
	if (app == NULL) {
		result = 0x80801002;
		goto unlock;
	}
	strncpy(titleId, app->process.appName, 10);
	if ((app->process.content.contentFlags & 0x30) != 0) {
		result = 0x80800048;
		if (!appmgrIsMediaDriveMountable(mountId, app->process.content.contentFlags))
			goto unlock;
	}
	if (mountId == 205) {
		path = g_appmgrCachePath;
		snprintf(g_appmgrCachePath, sizeof(g_appmgrCachePath), "ux0:cache/%s",
			 app->process.content.originalTitleId);
	}
	result = appmgrCreateDirectories(path, 1);
	if (result == 0)
		result = appmgrMount(sceKernelGetProcessId(), &app->process.mounts, mountId,
				     titleId, path, NULL, NULL, mountPoint);
	goto unlock;

invalid:
	result = 0x80800001;
unlock:
	sceKernelUnlockMutex(g_appmgrMountMutex, 1);
out:
	sceSyscallLeave(syscallState);
	return result;
}

int SceAppMgrForDriver_0813AFCA(int partitionId, char *output,
	unsigned int capacity)
{
	SceUInt32 syscall = sceSyscallEnter();
	char userRoot[64];
	char path[32];
	unsigned int index;
	int ret;

	__builtin_memset(userRoot, 0, sizeof(SceUInt32));
	memset(userRoot + sizeof(SceUInt32), 0,
		sizeof(userRoot) - sizeof(SceUInt32));
	__builtin_memset(path, 0, sizeof(path));
	if (output == NULL || capacity <= 31) {
		ret = (int)0x80801000;
		goto leave;
	}
	if ((unsigned int)partitionId - 1 > 1) {
		ret = (int)0x80800001;
		goto leave;
	}
	ret = sceKernelLockMutex(g_appmgrMountMutex, 1, NULL);
	if (ret < 0)
		goto leave;
	memset(userRoot, 0, sizeof(userRoot));
	index = 0;
	while (encodedUserRoot[index] != 0) {
		userRoot[index] = ~encodedUserRoot[index];
		if (++index == sizeof(userRoot)) {
			ret = (int)0x80800001;
			goto unlock;
		}
	}
	userRoot[index] = 0;
	if (g_appmgrAccountDirectory[0] == 0)
		appmgrUpdateAccountDirectory();
	if ((unsigned int)snprintf(path, sizeof(path), "%s/%s", userRoot,
		g_appmgrAccountDirectory) > 31) {
		ret = (int)0x80800001;
		goto unlock;
	}
	if (partitionId == 2)
		path[1] = 'x';
	memset(output, 0, 32);
	strncpy(output, path, 31);
	appmgrCreateDirectories(output, 1);
	ret = 0;
unlock:
	sceKernelUnlockMutex(g_appmgrMountMutex, 1);
leave:
	sceSyscallLeave(syscall);
	return ret;
}

int appmgrGetUserDirPathById(int userId, int partitionId, char *output,
	unsigned int capacity)
{
	SceUInt32 syscall = sceSyscallEnter();
	char userRoot[64];
	char path[32];
	unsigned int index;
	int ret;

	__builtin_memset(userRoot, 0, sizeof(SceUInt32));
	memset(userRoot + sizeof(SceUInt32), 0,
	       sizeof(userRoot) - sizeof(SceUInt32));
	__builtin_memset(path, 0, sizeof(path));

	if (output == NULL || capacity <= 31) {
		ret = (int)0x80801000;
		goto leave;
	}
	if ((unsigned int)partitionId - 1 > 1 ||
		(unsigned int)userId + 1 > 64) {
		ret = (int)0x80800001;
		goto leave;
	}
	ret = sceKernelLockMutex(g_appmgrMountMutex, 1, NULL);
	if (ret < 0)
		goto leave;
	if (userId == -1) {
		memset(userRoot, 0, sizeof(userRoot));
		index = 0;
		while (encodedUserRoot[index] != 0) {
			userRoot[index] = ~encodedUserRoot[index];
			if (++index == sizeof(userRoot)) {
				ret = (int)0x80800001;
				goto unlock;
			}
		}
		userRoot[index] = 0;
		if (g_appmgrAccountDirectory[0] == 0)
			appmgrUpdateAccountDirectory();
		if ((unsigned int)snprintf(path, sizeof(path), "%s/%s",
			userRoot, g_appmgrAccountDirectory) > 31) {
			ret = (int)0x80800001;
			goto unlock;
		}
		if (partitionId == 2)
			path[1] = 'x';
	} else {
		memset(userRoot, 0, sizeof(userRoot));
		index = 0;
		while (encodedUserRoot[index] != 0) {
			userRoot[index] = ~encodedUserRoot[index];
			if (++index == sizeof(userRoot)) {
				ret = (int)0x80800001;
				goto unlock;
			}
		}
		userRoot[index] = 0;
		if ((unsigned int)snprintf(path, sizeof(path), "%s/%02u",
			userRoot, (unsigned int)userId) > 31) {
			ret = (int)0x80800001;
			goto unlock;
		}
		if (partitionId == 2) {
			ret = appmgrCreateDirectories(path, 0);
			if (ret < 0)
				goto unlock;
			path[1] = 'x';
		}
	}
	memset(output, 0, 32);
	strncpy(output, path, 31);
	appmgrCreateDirectories(output, 1);
	ret = 0;
unlock:
	sceKernelUnlockMutex(g_appmgrMountMutex, 1);
leave:
	sceSyscallLeave(syscall);
	return ret;
}

/*
 * The save data of an application path: its medium, whether it is mounted already or
 * broken, its version and ownership, and its size and free space. The save data is
 * mounted for the query when it is not mounted yet.
 */
int sceAppMgrGetSaveDataInfo(const char *appPath, const char *titleId, SceUInt32 attributes,
			     AppmgrSaveDataInfo *info)
{
	SceUInt32 syscallState;
	SceAllocOpt opt;
	SceProductMode productMode;
	AppmgrPfsMount *mount = NULL, *cursor;
	ScePfsRndDriveId mountPoint = { "" };
	SceUInt32 quota = 0;
	char appName[16];
	union {
		SceSelfAuthInfo authInfo;
		SceIoStat stat;
	} buffer;
	char path[292] = "";
	char sdslotPath[64] = "";
	AppmgrProcessContent *content;
	struct AppState *app;
	char *workPath, *normalized;
	int result;

	productMode = 0;
	syscallState = sceSyscallEnter();
	result = sceKernelGetProcessSelfAuthInfo(0, &buffer.authInfo);
	if (result < 0)
		goto out;
	if (buffer.authInfo.programAuthorityId != 0x2800000000000001ULL &&
	    buffer.authInfo.programAuthorityId != 0x280000000000002dULL &&
	    buffer.authInfo.programAuthorityId != 0x2800000000000039ULL) {
		result = 0x80800009;
		goto out;
	}
	if (titleId == NULL || appPath == NULL || info == NULL ||
	    strnlen(appPath, sizeof(path)) == sizeof(path)) {
		result = 0x80800001;
		goto out;
	}
	result = sceKernelLockMutex(g_appmgrMountMutex, 1, NULL);
	if (result < 0)
		goto out;

	opt.size = sizeof(opt);
	opt.mmapKind = 0;
	opt.align = 32;
	opt.mmapBase = NULL;
	opt.mmapSize = 0;
	workPath = sceKernelAllocHeapMemoryWithOption(g_appmgrMountHeap, 292, &opt);
	if (workPath == NULL)
		goto noMemory;
	memset(workPath, 0, 292);
	opt.size = sizeof(opt);
	opt.mmapKind = 0;
	opt.mmapBase = NULL;
	opt.mmapSize = 0;
	opt.align = 32;
	content = sceKernelAllocHeapMemoryWithOption(g_appmgrMountHeap, sizeof(*content), &opt);
	if (content == NULL)
		goto noMemory;
	memset(content, 0, sizeof(*content));
	__builtin_strcpy(content->category, "gd");
	memset(info, 0, sizeof(*info));
	result = appmgrGetSaveDataPath(appPath, titleId, attributes, content->category,
				       path, sizeof(path));
	if (result < 0)
		goto freeContent;

	/* The save data may not be an application's content mount. */
	opt.mmapBase = NULL;
	opt.mmapSize = 0;
	opt.mmapKind = 0;
	opt.align = 32;
	opt.size = sizeof(opt);
	cursor = g_appmgrPfsMounts;
	normalized = sceKernelAllocHeapMemoryWithOption(g_appmgrMountHeap, 292, &opt);
	if (normalized == NULL)
		goto noMemoryContent;
	result = appmgrNormalizePath(path, normalized);
	if (result < 0) {
		sceKernelFreeHeapMemory(g_appmgrMountHeap, normalized);
		if (result != 0x80800002)
			goto freeContent;
	} else {
		for (; cursor != NULL; cursor = cursor->next) {
			if (cursor->mountId == 901 &&
			    strncmp(cursor->appPath, normalized, 292) == 0) {
				result = 0x80800017;
				sceKernelFreeHeapMemory(g_appmgrMountHeap, normalized);
				goto freeContent;
			}
		}
		sceKernelFreeHeapMemory(g_appmgrMountHeap, normalized);
	}

	if (strncmp(path, "ux0:", 4) == 0) {
		info->mediaType = 4;
		if (appmgrGetFakeNoMemoryCard() == 2) {
			result = 0x80010013;
			goto freeContent;
		}
	} else if (strncmp(path, "grw0:", 5) == 0 || strncmp(path, "gro0:", 5) == 0) {
		info->mediaType = 3;
	} else if (strncmp(path, "ur0:", 4) == 0 || strncmp(path, "vs0:", 4) == 0) {
		info->mediaType = 2;
	} else {
		info->mediaType = 1;
	}

	/* Mounted already? */
	opt.align = 32;
	opt.size = sizeof(opt);
	opt.mmapKind = 0;
	opt.mmapBase = NULL;
	opt.mmapSize = 0;
	cursor = g_appmgrPfsMounts;
	normalized = sceKernelAllocHeapMemoryWithOption(g_appmgrMountHeap, 292, &opt);
	if (normalized == NULL)
		goto noMemoryContent;
	result = appmgrNormalizePath(path, normalized);
	if (result < 0) {
		sceKernelFreeHeapMemory(g_appmgrMountHeap, normalized);
		if (result != 0x80800002)
			goto freeContent;
	} else {
		for (; cursor != NULL; cursor = cursor->next) {
			if (strncmp(cursor->appPath, normalized, 292) == 0) {
				mount = cursor;
				sceKernelFreeHeapMemory(g_appmgrMountHeap, normalized);
				info->mounted = 1;
				goto createDirectories;
			}
		}
		sceKernelFreeHeapMemory(g_appmgrMountHeap, normalized);
	}

createDirectories:
	result = appmgrCreateDirectories(path, 0);
	if (result < 0)
		goto freeContent;
	if (info->mounted)
		snprintf(workPath, 292, "%s/sce_sys", path);
	else
		snprintf(workPath, 292, "%s/sce_pfs", path);
	result = appmgrCreateDirectories(workPath, 0);
	if (result < 0)
		goto freeContent;
	sceKernelFreeHeapMemory(g_appmgrMountHeap, workPath);

	app = appmgrFindAppByProcessId(g_appmgrAppStates, sceKernelGetProcessId());
	if (app == NULL) {
		result = 0x80801002;
		workPath = NULL;
		goto freeContent;
	}
	__builtin_memset(appName, 0, sizeof(appName));
	strncpy(appName, app->process.appName, 9);
	if (!info->mounted) {
		result = appmgrMount(sceKernelGetProcessId(), &app->process.mounts, 1005,
				     appName, path, NULL, NULL, &mountPoint);
		if (result < 0)
			goto failed;
		result = appmgrFindPfsMount(g_appmgrPfsMounts, 1005, path, &mount);
		if (result < 0)
			goto unmount;
	}
	result = appmgrValidateSaveDataMount(sceKernelGetProcessId(), path, 1, content,
					     &info->maxSize);
	if (result < 0)
		goto failed;
	info->savedataVersion = content->savedataVersion;
	info->savedataOwnershipFlags = content->savedataOwnershipFlags;
	if (SceSysconForDriver_CBD6D8BC() & 0x80)
		info->savedataOwnershipFlags = 0;
	if (sceSblPmMgrGetProductMode(&productMode) >= 0 && productMode == 1)
		info->savedataOwnershipFlags = 0;
	if (ScePfsMgrForKernel_27FB7618(mount->mountPoint.drive_id, &quota, &info->freeSizeKiB) < 0)
		info->freeSizeKiB = 0;
	else
		info->freeSizeKiB >>= 10;
	if (snprintf(sdslotPath, sizeof(sdslotPath), "%s0:/sce_sys/sdslot.dat",
		     mount->mountPoint.drive_id) > 0 &&
	    sceIoGetstat(sdslotPath, &buffer.stat) < 0) {
		result = 0;
		info->broken = 1;
		goto unmount;
	}
	result = 0;
	goto unmount;

failed:
	if (result == 0x8001002f || result == 0x80010022 ||
	    result == 0x8001008f || result == 0x8014001b)
		info->broken = 1;
	else
		info->broken = 0;
unmount:
	if (mountPoint.drive_id[0] != '\0')
		appmgrUnmountDrive(sceKernelGetProcessId(), &app->process.mounts,
				   mountPoint.drive_id, 0);
	workPath = NULL;
	goto freeContent;

noMemoryContent:
	result = 0x80801006;
freeContent:
	sceKernelFreeHeapMemory(g_appmgrMountHeap, content);
	goto freeWork;
noMemory:
	result = 0x80801006;
freeWork:
	if (workPath != NULL)
		sceKernelFreeHeapMemory(g_appmgrMountHeap, workPath);
	sceKernelUnlockMutex(g_appmgrMountMutex, 1);
out:
	sceSyscallLeave(syscallState);
	return result;
}

/* Reads the save data's sce_sys/param.sfo at path into the caller's buffer; returns its size. */
int sceAppMgrLoadSaveDataSystemFile(const char *path, void *buffer, SceSize size)
{
	SceUInt32 syscallState;
	SceAllocOpt opt;
	ScePfsRndDriveId mountPoint = { "" };
	char appName[16];
	union {
		SceSelfAuthInfo authInfo;
		SceIoStat stat;
	} info;
	char name[64];
	char resolvedPath[292];
	AppmgrPfsMount *cursor;
	AppmgrMountHolder *mounts;
	struct AppState *app;
	char *workPath, *normalized;
	void *readBuffer;
	SceUID pid, fd;
	int permission, result, offset, length;

	syscallState = sceSyscallEnter();
	result = sceKernelGetProcessSelfAuthInfo(0, &info.authInfo);
	if (result < 0)
		goto out;
	if (info.authInfo.programAuthorityId != 0x2800000000000001ULL &&
	    info.authInfo.programAuthorityId != 0x280000000000002eULL) {
		result = 0x80800009;
		goto out;
	}
	if (buffer == NULL || path == NULL) {
		result = 0x80800001;
		goto out;
	}
	pid = sceKernelGetProcessId();
	permission = sceKernelSetThreadAccessLevel(128);
	result = SceFios2KernelForDriver_0F456345(pid, 0, path, resolvedPath, sizeof(resolvedPath));
	sceKernelSetThreadAccessLevel(permission);
	if (result < 0) {
		result = 0x80800001;
		goto out;
	}
	result = sceKernelLockMutex(g_appmgrMountMutex, 1, NULL);
	if (result < 0)
		goto out;

	opt.size = sizeof(opt);
	opt.mmapKind = 0;
	opt.align = 32;
	opt.mmapBase = NULL;
	opt.mmapSize = 0;
	workPath = sceKernelAllocHeapMemoryWithOption(g_appmgrMountHeap, 292, &opt);
	if (workPath == NULL)
		goto noMemory;
	memset(workPath, 0, 292);
	opt.mmapKind = 0;
	opt.mmapBase = NULL;
	opt.mmapSize = 0;
	opt.size = sizeof(opt);
	opt.align = 32;
	readBuffer = sceKernelAllocHeapMemoryWithOption(g_appmgrMountHeap, 1024, &opt);
	if (readBuffer == NULL)
		goto noMemory;
	memset(readBuffer, 0, 1024);

	opt.mmapKind = 0;
	opt.mmapBase = NULL;
	opt.mmapSize = 0;
	opt.align = 32;
	opt.size = sizeof(opt);
	cursor = g_appmgrPfsMounts;
	normalized = sceKernelAllocHeapMemoryWithOption(g_appmgrMountHeap, 292, &opt);
	if (normalized == NULL) {
		result = 0x80801006;
		goto freeReadBuffer;
	}
	result = appmgrNormalizePath(resolvedPath, normalized);
	if (result < 0) {
		sceKernelFreeHeapMemory(g_appmgrMountHeap, normalized);
		if (result != 0x80800002)
			goto freeReadBuffer;
	} else {
		for (; cursor != NULL; cursor = cursor->next) {
			if (strncmp(cursor->appPath, normalized, 292) == 0)
				break;
		}
		sceKernelFreeHeapMemory(g_appmgrMountHeap, normalized);
	}

	result = appmgrCreateDirectories(resolvedPath, 0);
	if (result < 0)
		goto freeReadBuffer;
	snprintf(workPath, 292, "%s/sce_pfs", resolvedPath);
	result = appmgrCreateDirectories(workPath, 0);
	if (result < 0)
		goto freeReadBuffer;

	MOUNT_MAIN_LOCK(13551);
	app = appmgrFindAppByProcessId(g_appmgrAppStates, sceKernelGetProcessId());
	if (app == NULL) {
		result = 0x80801002;
		goto freeReadBuffer;
	}
	mounts = &app->process.mounts;
	__builtin_memset(appName, 0, sizeof(appName));
	strncpy(appName, app->process.appName, 9);
	MOUNT_MAIN_UNLOCK(13561);

	result = appmgrMount(sceKernelGetProcessId(), mounts, 1005, appName, resolvedPath,
			     NULL, NULL, &mountPoint);
	if (result < 0)
		goto unmount;
	result = appmgrDecodeString(encodedParamSfo, name, sizeof(name));
	if (result < 0)
		goto unmount;
	if ((unsigned int)snprintf(workPath, 292, "%s/%s", resolvedPath, name) >= 292) {
		result = 0x80800001;
		goto unmount;
	}
	fd = sceIoOpen(workPath, 0x10001, 0);
	if (fd < 0) {
		result = fd;
		goto unmount;
	}
	result = sceIoGetstatByFd(fd, &info.stat);
	if (result >= 0) {
		if ((SceOff)size < info.stat.st_size) {
			result = 0x8080001f;
		} else {
			for (offset = 0; offset < info.stat.st_size; offset += length) {
				length = sceIoRead(fd, readBuffer, 1024);
				if (length < 0) {
					result = length;
					goto close;
				}
				if (length == 0)
					break;
				result = sceKernelCopyToUser((char *)buffer + offset, readBuffer, length);
				if (result < 0)
					goto close;
			}
			result = offset;
		}
	}
close:
	sceIoClose(fd);
unmount:
	if (mounts != NULL && mountPoint.drive_id[0] != '\0')
		appmgrUnmountDrive(sceKernelGetProcessId(), mounts, mountPoint.drive_id, 0);
freeReadBuffer:
	sceKernelFreeHeapMemory(g_appmgrMountHeap, readBuffer);
	goto freeWork;
noMemory:
	result = 0x80801006;
freeWork:
	if (workPath != NULL)
		sceKernelFreeHeapMemory(g_appmgrMountHeap, workPath);
	sceKernelUnlockMutex(g_appmgrMountMutex, 1);
out:
	sceSyscallLeave(syscallState);
	return result;
}

/*
 * Updates the parameters of the save data at path, or, for "ux0:/" or "grw0:/", of
 * every save data of the account there, mounting each one for the update.
 */
int sceAppMgrUpdateSaveDataParam(const char *path)
{
	SceUInt32 syscallState;
	SceAllocOpt opt;
	char titleId[16] = "NPXS19999";
	ScePfsRndDriveId driveId, mountPoint;
	SceIoDirent entry;
	char savedataPath[64];
	struct AppState *app;
	char *resolvedPath, *name;
	SceUID pid, directory;
	SceSize nameSize;
	int permission, result;

	syscallState = sceSyscallEnter();
	result = sceKernelLockMutex(g_appmgrMountMutex, 1, NULL);
	if (result < 0)
		goto out;
	if (path == NULL || strnlen(path, 292) == 292) {
		result = 0x80800001;
		goto unlock;
	}
	app = appmgrFindAppByProcessId(g_appmgrAppStates, sceKernelGetProcessId());
	if (app == NULL) {
		result = 0x80801002;
		goto unlock;
	}
	opt.size = sizeof(opt);
	opt.mmapKind = 0;
	opt.mmapBase = NULL;
	opt.mmapSize = 0;
	opt.align = 32;
	resolvedPath = sceKernelAllocHeapMemoryWithOption(g_appmgrMountHeap, 292, &opt);
	if (resolvedPath == NULL) {
		result = 0x80801006;
		goto unlock;
	}
	pid = sceKernelGetProcessId();
	permission = sceKernelSetThreadAccessLevel(128);
	result = SceFios2KernelForDriver_0F456345(pid, 0, path, resolvedPath, 292);
	sceKernelSetThreadAccessLevel(permission);
	if (result < 0)
		goto free;

	if (strncmp(resolvedPath, "ux0:/", 6) != 0 && strncmp(resolvedPath, "grw0:/", 7) != 0) {
		/* One save data. */
		if (!(strncmp(resolvedPath, "ux0:/user/", 10) == 0 &&
		      strncmp(resolvedPath + 12, "/savedata", 9) == 0)) {
			result = 0x8080000c;
			if (strncmp(resolvedPath, "grw0:/savedata/", 15) != 0)
				goto free;
		}
		__builtin_memset(&driveId, 0, sizeof(driveId));
		__builtin_memset(&mountPoint, 0, sizeof(mountPoint));
		result = appmgrMakeRandomDriveId("ts", driveId.drive_id);
		if (result < 0)
			goto free;
		result = appmgrMount(sceKernelGetProcessId(), &app->process.mounts, 1005, titleId,
				     path, &driveId, NULL, &mountPoint);
		if (result < 0) {
			if (result == 0x80800003)
				result = appmgrUpdateSaveDataParams(resolvedPath, NULL, 0, NULL);
		} else {
			result = appmgrUpdateSaveDataParams(resolvedPath, NULL, 0, NULL);
			appmgrUnmountDrive(sceKernelGetProcessId(), &app->process.mounts,
					   mountPoint.drive_id, 0);
		}
	} else {
		/* Every save data of the account. */
		char allTitleId[16] = "NPXS19999";

		pid = sceKernelGetProcessId();
		__builtin_memset(&driveId, 0, sizeof(driveId));
		__builtin_memset(&mountPoint, 0, sizeof(mountPoint));
		permission = sceKernelSetThreadAccessLevel(64);
		if (strncmp(resolvedPath, "ux0:/", 6) != 0 &&
		    strncmp(resolvedPath, "grw0:/", 7) != 0) {
			result = 0x80800001;
			goto restore;
		}
		result = appmgrMakeRandomDriveId("ts", driveId.drive_id);
		if (result < 0)
			goto restore;
		if (g_appmgrAccountDirectory[0] == '\0')
			appmgrUpdateAccountDirectory();
		snprintf(savedataPath, sizeof(savedataPath), "%suser/%s/savedata/%s", resolvedPath,
			 g_appmgrAccountDirectory, "");
		nameSize = strnlen(savedataPath, sizeof(savedataPath));
		directory = sceIoDopen(savedataPath);
		if (directory < 0) {
			result = 0;
			goto restore;
		}
		memset(&entry, 0, sizeof(entry));
		name = savedataPath + nameSize;
		nameSize = sizeof(savedataPath) - 1 - nameSize;
		while ((result = sceIoDread(directory, &entry)) > 0) {
			if (entry.d_stat.st_mode & SCE_STM_FDIR) {
				strncpy(name, entry.d_name, nameSize);
				result = appmgrMount(pid, &app->process.mounts, 1005, allTitleId,
						     savedataPath, &driveId, NULL, &mountPoint);
				if (result < 0) {
					if (result != 0x80800003)
						continue;
					appmgrUpdateSaveDataParams(savedataPath, NULL, 0, NULL);
				} else {
					appmgrUpdateSaveDataParams(savedataPath, NULL, 0, NULL);
					appmgrUnmountDrive(pid, &app->process.mounts, mountPoint.drive_id, 0);
				}
			}
			memset(&entry, 0, sizeof(entry));
		}
		sceIoDclose(directory);
restore:
		sceKernelSetThreadAccessLevel(permission);
	}
free:
	sceKernelFreeHeapMemory(g_appmgrMountHeap, resolvedPath);
unlock:
	sceKernelUnlockMutex(g_appmgrMountMutex, 1);
out:
	sceSyscallLeave(syscallState);
	return result;
}

/* The medium of a mounted application path: 4 ux0, 3 grw0/gro0, 2 ur0/vs0, 1 other. */
static inline int appmgrGetMediaType(const char *path)
{
	if (strncmp(path, "ux0:", 4) == 0)
		return 4;
	if (strncmp(path, "grw0:", 5) == 0 || strncmp(path, "gro0:", 5) == 0)
		return 3;
	if (strncmp(path, "ur0:", 4) == 0 || strncmp(path, "vs0:", 4) == 0)
		return 2;
	return 1;
}

int appmgrRemoveContentCache(SceUID pid, const AppmgrProcessContent *content)
{
	char path[64];

	if (!SceSblACMgrForDriver_456DA7AC(pid) || !(content->attribute & 0x800000))
		return 0;
	snprintf(path, sizeof(path), "ux0:cache/%s", content->originalTitleId);
	return appmgrRemoveDirectoryTree(path);
}

int appmgrRemoveAppWorkDirectory(const AppmgrProcessContent *content)
{
	char path[64];

	if (!(content->contentFlags & 0x30))
		return 0;
	snprintf(path, sizeof(path), "ux0:temp/app_work/%s", content->titleId);
	return appmgrRemoveDirectoryTree(path);
}

/* The medium of the calling application's mount at drive. */
int sceAppMgrGetMediaTypeFromDrive(const char *drive, int *mediaType)
{
	SceUInt32 syscallState;
	struct AppState *app;
	AppmgrVirtualMount *mount;
	int result;

	syscallState = sceSyscallEnter();
	if (mediaType == NULL || drive == NULL) {
		result = 0x80800001;
		goto out;
	}
	result = sceKernelLockMutex(g_appmgrMountMutex, 1, NULL);
	if (result < 0)
		goto out;
	app = appmgrFindAppByProcessId(g_appmgrAppStates, sceKernelGetProcessId());
	if (app == NULL) {
		result = 0x80801002;
		goto unlock;
	}
	for (mount = app->process.mounts.firstMount; mount != NULL; mount = mount->next) {
		if (strncmp(mount->driveId.drive_id, drive, 16) == 0)
			break;
	}
	if (mount == NULL) {
		result = 0x80800002;
		goto unlock;
	}
	*mediaType = appmgrGetMediaType(mount->pfsMount->appPath);
	result = 0;
unlock:
	sceKernelUnlockMutex(g_appmgrMountMutex, 1);
out:
	sceSyscallLeave(syscallState);
	return result;
}

/* The medium of process pid's mount at drive; another process's only for system programs. */
int sceAppMgrGetMediaTypeFromDriveByPid(SceUID pid, const char *drive, int *mediaType)
{
	SceUInt32 syscallState;
	struct AppState *app;
	AppmgrVirtualMount *mount;
	int result;

	syscallState = sceSyscallEnter();
	if (mediaType == NULL || drive == NULL) {
		result = 0x80800001;
		goto out;
	}
	result = sceKernelLockMutex(g_appmgrMountMutex, 1, NULL);
	if (result < 0)
		goto out;
	if (pid != 0 && SceSblACMgrForDriver_8A54DF3A(0) != 0 &&
	    pid != sceKernelGetProcessId()) {
		result = 0x80800009;
		goto unlock;
	}
	app = appmgrFindAppByProcessId(g_appmgrAppStates, pid);
	if (app == NULL) {
		result = 0x80801002;
		goto unlock;
	}
	for (mount = app->process.mounts.firstMount; mount != NULL; mount = mount->next) {
		if (strncmp(mount->driveId.drive_id, drive, 16) == 0)
			break;
	}
	if (mount == NULL) {
		result = 0x80800002;
		goto unlock;
	}
	*mediaType = appmgrGetMediaType(mount->pfsMount->appPath);
	result = 0;
unlock:
	sceKernelUnlockMutex(g_appmgrMountMutex, 1);
out:
	sceSyscallLeave(syscallState);
	return result;
}

/*
 * The size and free space of the device of path (its drive up to the colon): ux0:
 * directly, otherwise one of the calling application's mounts it may query.
 */
int sceAppMgrGetDevInfo(const char *path, SceOff *maxSize, SceOff *freeSize)
{
	SceUInt32 syscallState;
	SceIoDevInfo info;
	char drive[16];
	char resolvedPath[64];
	struct AppState *app;
	AppmgrVirtualMount *mount;
	SceUID pid;
	int permission, result;

	syscallState = sceSyscallEnter();
	__builtin_memset(&info, 0, sizeof(info));
	if (path == NULL)
		goto invalid;
	__builtin_memset(drive, 0, sizeof(drive));
#define COPY_DRIVE_CHAR(index) \
	if ((drive[index] = path[index]) == ':') \
		goto lock; \
	if (drive[index] == '\0') \
		goto invalid
	COPY_DRIVE_CHAR(0);
	COPY_DRIVE_CHAR(1);
	COPY_DRIVE_CHAR(2);
	COPY_DRIVE_CHAR(3);
	COPY_DRIVE_CHAR(4);
	COPY_DRIVE_CHAR(5);
	COPY_DRIVE_CHAR(6);
	COPY_DRIVE_CHAR(7);
	COPY_DRIVE_CHAR(8);
	COPY_DRIVE_CHAR(9);
	COPY_DRIVE_CHAR(10);
	COPY_DRIVE_CHAR(11);
	COPY_DRIVE_CHAR(12);
	COPY_DRIVE_CHAR(13);
	if ((drive[14] = path[14]) == ':')
		goto lock;
#undef COPY_DRIVE_CHAR
	goto invalid;

lock:
	result = sceKernelLockMutex(g_appmgrMountMutex, 1, NULL);
	if (result < 0)
		goto out;
	if (strncmp(drive, "ux0:", 5) == 0) {
		permission = sceKernelSetThreadAccessLevel(128);
		result = sceIoDevctl(drive, 0x3001, NULL, 0, &info, sizeof(info));
		sceKernelSetThreadAccessLevel(permission);
		if (result < 0)
			goto unlock;
		goto copy;
	}
	app = appmgrFindAppByProcessId(g_appmgrAppStates, sceKernelGetProcessId());
	if (app == NULL) {
		result = 0x80801002;
		goto unlock;
	}
	for (mount = app->process.mounts.firstMount; mount != NULL; mount = mount->next) {
		if (strncmp(mount->driveId.drive_id, drive, 16) == 0)
			break;
	}
	if (mount == NULL) {
		result = 0x80010013;
		goto unlock;
	}
	switch (mount->pfsMount->mountId) {
	case 108:
		break;
	case 205:
		if (SceSblACMgrForDriver_456DA7AC(0) == 0)
			goto denied;
		break;
	case 112:
	case 600:
		if (SceSblACMgrForDriver_6C5AB07F(0) == 0)
			goto denied;
		break;
	default:
	denied:
		result = 0x80800009;
		goto unlock;
	}
	pid = sceKernelGetProcessId();
	permission = sceKernelSetThreadAccessLevel(128);
	result = SceFios2KernelForDriver_0F456345(pid, 0, drive, resolvedPath, sizeof(resolvedPath));
	sceKernelSetThreadAccessLevel(permission);
	if (result < 0)
		goto unlock;
	result = sceIoDevctl(resolvedPath, 0x3001, NULL, 0, &info, sizeof(info));
	if (result < 0)
		goto unlock;
copy:
	if (maxSize != NULL)
		*maxSize = info.maxSize;
	if (freeSize != NULL)
		*freeSize = info.freeSize;
unlock:
	sceKernelUnlockMutex(g_appmgrMountMutex, 1);
out:
	sceSyscallLeave(syscallState);
	return result;

invalid:
	result = 0x80800001;
	goto out;
}

/* The PFS drive under the calling shell's install mount at drive (mount IDs 500 and 501). */
int sceAppMgrGetPfsDrive(const char *drive, char *pfsDrive)
{
	SceUInt32 syscallState;
	SceSelfAuthInfo authInfo;
	struct AppState *app;
	AppmgrVirtualMount *mount;
	int result;

	syscallState = sceSyscallEnter();
	if (sceKernelGetProcessSelfAuthInfo(0, &authInfo) < 0 ||
	    authInfo.programAuthorityId != 0x2800000000000001ULL) {
		result = 0x80800009;
		goto out;
	}
	if (pfsDrive == NULL || drive == NULL) {
		result = 0x80800001;
		goto out;
	}
	result = sceKernelLockMutex(g_appmgrMountMutex, 1, NULL);
	if (result < 0)
		goto out;
	MOUNT_MAIN_LOCK(14358);
	app = appmgrFindAppByProcessId(g_appmgrAppStates, sceKernelGetProcessId());
	if (app == NULL) {
		result = 0x80801002;
		goto unlock;
	}
	for (mount = app->process.mounts.firstMount; mount != NULL; mount = mount->next) {
		if (strncmp(mount->driveId.drive_id, drive, 16) == 0)
			break;
	}
	if (mount == NULL) {
		result = 0x80800002;
		goto unlock;
	}
	if ((unsigned int)mount->pfsMount->mountId - 500 > 1) {
		result = 0x80800001;
		goto unlock;
	}
	result = 0;
	memset(pfsDrive, 0, 16);
	strncpy(pfsDrive, mount->pfsMount->mountPoint.drive_id, 15);
unlock:
	MOUNT_MAIN_UNLOCK(14386);
	sceKernelUnlockMutex(g_appmgrMountMutex, 1);
out:
	sceSyscallLeave(syscallState);
	return result;
}

int sceAppMgrGetSaveDataInfoForSpecialExport(const char *path, SceUInt64 *accountId)
{
	SceUInt32 syscallState;
	ScePfsRndDriveId klicensee;
	ScePfsRndDriveId mountPoint = { "" };
	char titleId[16] = "";
	ScePfsRndDriveId drive = { "" };
	union {
		SceSelfAuthInfo authInfo;
		ScePfsRndDriveId key;
	} info;
	unsigned char random[8];
	struct AppState *app;
	AppmgrMountHolder *holder;
	AppmgrProcessContent *content;
	int result;

	syscallState = sceSyscallEnter();
	result = sceKernelGetProcessSelfAuthInfo(0, &info.authInfo);
	if (result < 0)
		goto out;
	if (info.authInfo.programAuthorityId != 0x280000000000002dULL) {
		result = 0x80800009;
		goto out;
	}
	if (accountId == NULL || path == NULL) {
		result = 0x80800001;
		goto out;
	}
	__builtin_memset((char *)accountId, 0, sizeof(*accountId));
	if (SceSblPostSsMgrForDriver_1923D80D() == 0) {
		result = 0x80800009;
		goto out;
	}
	result = sceKernelLockMutex(g_appmgrMountMutex, 1, NULL);
	if (result < 0)
		goto out;
	holder = NULL;
	content = NULL;
	__builtin_memset(&info.key, 0, sizeof(info.key));
	__builtin_memset(&klicensee, 0, sizeof(klicensee));
	result = appmgrReadSealedKey(path, &info.key);
	if (result < 0)
		goto unmount;
	klicensee = info.key;
	result = sceSblRngPseudoRandomNumber(random, sizeof(random));
	if (result < 0)
		goto unmount;
	snprintf(drive.drive_id, sizeof(drive.drive_id), "%s%02x%02x%02x%02x%02x%02xd", "ts",
		 random[0], random[1], random[2], random[3], random[4], random[5]);
	drive.drive_id[14] = ':';
	drive.drive_id[15] = '\0';
	MOUNT_MAIN_LOCK(13747);
	app = appmgrFindAppByProcessId(g_appmgrAppStates, sceKernelGetProcessId());
	if (app != NULL) {
		holder = &app->process.mounts;
		/* Sony's: freed below when the mount fails */
		content = &app->process.content;
		strncpy(titleId, app->process.appName, 9);
	}
	MOUNT_MAIN_UNLOCK(13755);
	if (app == NULL) {
		result = 0x80801002;
		goto unmount;
	}
	result = appmgrMount(sceKernelGetProcessId(), holder, 1005, titleId, path, &drive,
			     &klicensee, &mountPoint);
	if (result < 0)
		goto unmount;
	content = appmgrAllocMountHeapMemory(sizeof(*content));
	if (content == NULL) {
		result = 0x80801006;
		goto unmount;
	}
	memset(content, 0, sizeof(*content));
	result = appmgrReadSaveDataMetadata(path, content, NULL, accountId);
unmount:
	if (mountPoint.drive_id[0] != '\0' &&
	    strncmp(drive.drive_id, mountPoint.drive_id, 16) == 0)
		appmgrUnmountDrive(sceKernelGetProcessId(), holder, mountPoint.drive_id, 0);
	if (content != NULL)
		sceKernelFreeHeapMemory(g_appmgrMountHeap, content);
	sceKernelUnlockMutex(g_appmgrMountMutex, 1);
out:
	sceSyscallLeave(syscallState);
	return result;
}

static __attribute__((noinline, noclone)) void *appmgrAllocProcessHeap(SceSize size)
{
	SceAllocOpt opt;

	opt.size = sizeof(opt);
	opt.mmapKind = 0;
	opt.align = 0x1000;
	opt.mmapBase = NULL;
	opt.mmapSize = 0;
	return sceKernelAllocRemoteProcessHeap(sceKernelGetProcessId(), size, &opt);
}

static __attribute__((noinline)) int appmgrFreeProcessHeap(void *pointer)
{
	return sceKernelFreeRemoteProcessHeap(sceKernelGetProcessId(), pointer);
}

/*
 * The loop back images (ux0:umass/<id>.img), one per 32-digit hex ID: create (0, a FAT
 * image of one of the allowed sizes), query (1), delete (2), take (3) and release (5) the
 * owner, shrink a deleted one (4, ".img_", 128 MiB a call).
 */
int sceAppMgrLoopBackFormat(unsigned int mountId, unsigned int command, const char *id,
			    AppmgrLoopBackImageInfo *info)
{
	AppmgrFatFormatArguments arguments;
	SceDateTime time;
	SceUInt64 size;
	union {
		SceSelfAuthInfo authInfo;
		SceIoStat stat;
	} stat;
	char directory[64];
	char imagePath[64];
	char workPath[64];
	void *buffer;
	unsigned int serial;
	SceUID pid, fd;
	SceUInt32 syscallState;
	int permission, result, index;

	syscallState = sceSyscallEnter();
	permission = sceKernelSetThreadAccessLevel(128);
	SceKernelSuspendForDriver_4DF40893(0);
	result = sceKernelLockMutex(g_appmgrMountMutex, 1, NULL);
	if (result < 0)
		goto out;
	if (mountId != 100)
		goto invalid;
	if (command == 1) {
		if (SceSblACMgrForDriver_4DB7F512(0) == 0 || SceSblACMgrForDriver_D0E11C89(0) == 0)
			goto denied;
	} else if (command == 5) {
		if (sceKernelGetProcessSelfAuthInfo(0, &stat.authInfo) < 0 ||
		    stat.authInfo.programAuthorityId != 0x2800000000000001ULL)
			goto denied;
		result = 0;
		g_appmgrLoopBackOwner = -1;
		goto unlock;
	} else {
		if (SceSblACMgrForDriver_D0E11C89(0) == 0)
			goto denied;
		if (command == 3) {
			pid = sceKernelGetProcessId();
			if (g_appmgrLoopBackOwner == -1) {
				g_appmgrLoopBackOwner = pid;
				result = 0;
			} else {
				result = pid != g_appmgrLoopBackOwner ? 0x80800046 : 0;
			}
			goto unlock;
		}
	}
	if (id == NULL)
		goto invalid;
	for (index = 0; index < 32; index++) {
		if ((unsigned int)(id[index] - 'A') > 5 && (unsigned int)(id[index] - '0') > 9)
			goto invalid;
	}
	if (id[32] != '\0')
		goto invalid;
	snprintf(imagePath, sizeof(imagePath), "ux0:umass/%s.img", id);
	snprintf(workPath, sizeof(workPath), "ux0:umass/%s.img_", id);
	if (command == 4) {
		if (info != NULL)
			memset(info, 0, sizeof(*info));
		if (sceIoGetstat(workPath, &stat.stat) < 0) {
			result = sceIoRename(imagePath, workPath);
			if (result < 0)
				goto unlock;
			g_appmgrLoopBackOwner = -1;
		}
		result = sceIoGetstat(workPath, &stat.stat);
		if (result < 0)
			goto unlock;
		if (stat.stat.st_size <= 0x8000000) {
			result = sceIoRemove(workPath);
			if (result < 0)
				goto unlock;
			if (info != NULL) {
				result = 0;
				info->size = 0;
				goto unlock;
			}
		} else {
			stat.stat.st_size -= 0x8000000;
			result = sceIoChstat(workPath, &stat.stat, SCE_CST_SIZE);
			if (result < 0)
				goto unlock;
			if (info != NULL) {
				result = 0;
				info->size = stat.stat.st_size;
				goto unlock;
			}
		}
		result = 0;
		goto unlock;
	}
	appmgrRemoveFileInSteps(workPath, 128);
	switch (command) {
	case 1:
		result = sceIoGetstat(imagePath, &stat.stat);
		if (result >= 0 && info != NULL)
			info->size = stat.stat.st_size;
		goto unlock;
	case 2:
		result = sceIoRename(imagePath, workPath);
		if (result < 0)
			goto unlock;
		g_appmgrLoopBackOwner = -1;
		result = appmgrRemoveFileInSteps(workPath, 128);
		goto unlock;
	case 0:
		break;
	default:
		goto invalid;
	}
	if (info == NULL)
		goto invalid;
	size = info->size;
	if (size != 0x800a0000ULL && size != 0x100000000ULL && size != 0x180000000ULL &&
	    size != 0x300000000ULL && size != 0x600000000ULL && size != 0xe00000000ULL &&
	    size != 0xc00000000ULL)
		goto invalid;
	if (g_appmgrLoopBackOwner != -1 && sceKernelGetProcessId() != g_appmgrLoopBackOwner) {
		result = 0x80800046;
		goto unlock;
	}
	result = appmgrDecodeString(encodedUmass, directory, sizeof(directory));
	if (result < 0)
		goto unlock;
	result = appmgrCreateDirectories(directory, 1);
	if (result < 0)
		goto unlock;
	sceIoRename(imagePath, workPath);
	appmgrRemoveFileInSteps(workPath, 128);
	size = info->size;
	SceKernelSuspendForDriver_4DF40893(0);
	fd = appmgrCheckSaveDataFreeSpace(workPath, size);
	if (fd >= 0)
		fd = sceIoOpen(workPath, 0x10603, 0606);
	if (fd < 0) {
		result = fd;
		SceKernelSuspendForDriver_2BB92967(0);
		goto remove;
	}
	memset(&stat.stat, 0, sizeof(stat.stat));
	stat.stat.st_size = size;
	result = sceIoChstatByFd(fd, &stat.stat, SCE_CST_SIZE);
	sceIoClose(fd);
	SceKernelSuspendForDriver_2BB92967(0);
	if (result < 0)
		goto remove;
	result = sceRtcGetCurrentClockLocalTime(&time);
	if (result < 0)
		goto remove;
	serial = appmgrPackFatDateTime(&time);
	buffer = appmgrAllocProcessHeap(0x40000);
	if (buffer == NULL) {
		result = 0x80801006;
		goto remove;
	}
	arguments.volumeSize = 0;
	arguments.deviceName = workPath;
	arguments.buffer = buffer;
	arguments.bufferSize = 0x40000;
	arguments.clusterSize = 0x8000;
	arguments.initialImageSize = 0;
	arguments.writtenDataSize = 0;
	arguments.serial = serial;
	arguments.volumeLabel = NULL;
	arguments.phase = 0;
	arguments.type = 3;
	result = FatFormat(&arguments);
	if (result >= 0)
		sceIoSync(arguments.deviceName, 0);
	appmgrFreeProcessHeap(buffer);
	if (result >= 0) {
		result = sceIoRename(workPath, imagePath);
		if (result >= 0)
			goto unlock;
	}
remove:
	appmgrRemoveFileInSteps(workPath, 128);
	goto unlock;
denied:
	result = 0x80800009;
	goto unlock;
invalid:
	result = 0x80800001;
unlock:
	sceKernelUnlockMutex(g_appmgrMountMutex, 1);
out:
	SceKernelSuspendForDriver_2BB92967(0);
	sceKernelSetThreadAccessLevel(permission);
	sceSyscallLeave(syscallState);
	return result;
}

/* the hex dump of local-backup and cloud-data buffers: empty in release builds */
void appmgrDebugHexDump(const void *data, SceSize size)
{
}

int appmgrResolvePath(const char *path, SceUID pid, char *output, SceSize size)
{
	int permission, result;

	if (pid == 0)
		pid = sceKernelGetProcessId();
	permission = sceKernelSetThreadAccessLevel(128);
	result = SceFios2KernelForDriver_0F456345(pid, 0, path, output, size);
	sceKernelSetThreadAccessLevel(permission);
	return result;
}

int sceAppMgrGetRawPath(char *path, char *resolvedPath, int capacity)
{
	SceUInt32 syscall = sceSyscallEnter();
	SceUID pid = sceKernelGetProcessId();
	int permission = sceKernelSetThreadAccessLevel(128);
	unsigned int size = (unsigned int)capacity;
	int result;

	if (size >= 292)
		size = 292;
	result = SceFios2KernelForDriver_0F456345(pid, 0, path, resolvedPath, size);
	sceKernelSetThreadAccessLevel(permission);
	sceSyscallLeave(syscall);
	return result;
}

int appmgrExtractSaveDataDeviceName(const char *path, char *device,
				    SceSize capacity)
{
	const signed char *source = (const signed char *)path;
	SceSize index;

	memset(device, 0, capacity);
	if (capacity) {
		device[0] = source[0];
		if (source[0] == ':')
			return 0;
		index = 0;
		while (index != capacity - 1) {
			index++;
			device[index] = source[index];
			if (source[index] == ':')
				return 0;
		}
	}
	device[capacity - 1] = 0;
	return -1;
}

int appmgrResolveRawPath(const char *path, SceUID pid, char *output, SceSize size)
{
	SceAllocOpt option;
	AppmgrPfsMount *mount;
	const char *source;
	char *resolvedPath;
	SceUInt32 syscallState;
	SceSize length;
	int permission;
	int index;
	int result;

	syscallState = sceSyscallEnter();
	result = sceKernelLockMutex(g_appmgrMountMutex, 1, NULL);
	if (result < 0)
		goto leaveSyscall;
	option.size = sizeof(option);
	option.mmapKind = 0;
	option.align = 32;
	option.mmapBase = NULL;
	option.mmapSize = 0;
	resolvedPath = sceKernelAllocHeapMemoryWithOption(g_appmgrMountHeap, 292, &option);
	if (!resolvedPath) {
		result = (int)0x80801006;
		goto unlock;
	}
	if (pid == 0)
		pid = sceKernelGetProcessId();
	permission = sceKernelSetThreadAccessLevel(128);
	result = SceFios2KernelForDriver_0F456345(pid, 0, path, resolvedPath, 292);
	sceKernelSetThreadAccessLevel(permission);
	if (result >= 0) {
		length = size >= 292 ? 292 : size;
		source = resolvedPath;
		/* a PFS drive: the application path it mounts */
		if (strncmp(resolvedPath, "PD", 2) == 0) {
			char drive[33] = {0};

			for (index = 0; ; ) {
				drive[index] = resolvedPath[index];
				if (drive[index] == ':')
					break;
				if (++index == sizeof(drive))
					goto copy;
			}
			drive[15] = 0;
			for (mount = g_appmgrPfsMounts; mount; mount = mount->next) {
				if (strncmp(mount->mountPoint.drive_id, drive, 16) == 0) {
					source = mount->appPath;
					break;
				}
			}
		}
copy:
		strncpy(output, source, length);
		output[length - 1] = 0;
		result = 0;
	}
	sceKernelFreeHeapMemory(g_appmgrMountHeap, resolvedPath);
unlock:
	sceKernelUnlockMutex(g_appmgrMountMutex, 1);
leaveSyscall:
	sceSyscallLeave(syscallState);
	return result;
}

void *appmgrAllocMountHeapMemory(SceSize size)
{
	SceAllocOpt opt;

	opt.size = sizeof(opt);
	opt.mmapKind = 0;
	opt.align = 32;
	opt.mmapBase = NULL;
	opt.mmapSize = 0;
	return sceKernelAllocHeapMemoryWithOption(g_appmgrMountHeap, size, &opt);
}

/* license/app */
static const unsigned char encodedLicenseApplicationDirectory[16] = {
	(unsigned char)~'l', (unsigned char)~'i', (unsigned char)~'c', (unsigned char)~'e',
	(unsigned char)~'n', (unsigned char)~'s', (unsigned char)~'e', (unsigned char)~'/',
	(unsigned char)~'a', (unsigned char)~'p', (unsigned char)~'p', 0
};

static char g_appmgrMapFilePath[256];

#if FW_VERSION >= 0x04000000
#define APPMGR_STRCAT(destination, source, capacity) \
	__strcat_chk(destination, source, capacity)
#define APPMGR_STRNCPY(destination, source, count, capacity) \
	__strncpy_chk(destination, source, count, capacity)
#else
#define APPMGR_STRCAT(destination, source, capacity) \
	__builtin_strcat(destination, source)
#define APPMGR_STRNCPY(destination, source, count, capacity) \
	strncpy(destination, source, count)
#endif

int appmgrPrepareProcessContent(const char *name, const char *path,
				const AppmgrLaunchInfo *info, const AppmgrLaunchOptions *options,
				AppmgrMountHolder *mounts, AppmgrProcessContent *content,
				AppmgrProcessMountCleanup *cleanup)
{
	ScePfsRndDriveId driveId = {{0}};
	SceRtcTick tick;
	AppmgrEbootParam *ebootParam;
	AppmgrMapFile *mapFile;
	AppmgrPfsMount *mount;
	const char *slash, *backslash, *colon, *directoryEnd;
	char titleId[16] = { 0 };
	char *appPath, *normalized, *resolvedPath;
	int result, notPcsi, hasLicense = 1, limitedFiles, permission;
	SceUID pid;

	memset(content, 0, sizeof(*content));
	memset(cleanup, 0, sizeof(*cleanup));
	strncpy(titleId, name, 9);
	result = sceKernelLockMutex(g_appmgrMountMutex, 1, NULL);
	if (result < 0)
		return result;
	if (appmgrExtractSaveDataDeviceName(path, content->devicePrefix,
					    sizeof(content->devicePrefix)) < 0) {
		result = (int)0x80800007;
		appPath = NULL;
		goto error;
	}
	appPath = appmgrAllocMountHeapMemory(292);
	if (appPath == NULL)
		goto noMemory;
	cleanup->resolvedPath = appmgrAllocMountHeapMemory(292);
	if (cleanup->resolvedPath == NULL)
		goto noMemory;

	slash = strrchr(path, '/');
	backslash = strrchr(path, '\\');
	colon = strrchr(path, ':');
	if (slash <= backslash) {
		if (backslash > colon) {
			slash = backslash;
		} else {
			if (colon == NULL) {
				result = (int)0x80800007;
				memset(appPath, 0, 292);
				goto error;
			}
			slash = colon + 1;
		}
	}
	memset(appPath, 0, 292);
	if (slash == NULL) {
		result = (int)0x80800007;
		goto error;
	}
	APPMGR_STRNCPY(appPath, path, slash - path, 292);
	if (*slash == '/' || *slash == '\\')
		slash++;
	if (strncmp(slash, "eboot2.bin", 11) == 0)
		content->contentFlags |= 0x40;
	memset(content->appDirectory, 0, sizeof(content->appDirectory));
	strncpy(content->appDirectory, appPath, sizeof(content->appDirectory) - 1);
	notPcsi = strncmp(content->appDirectory, "ux0:app/PCSI00011", 18);

	mount = g_appmgrPfsMounts;
	normalized = appmgrAllocMountHeapMemory(292);
	if (normalized == NULL)
		goto noMemory;
	result = appmgrNormalizePath(appPath, normalized);
	if (result < 0) {
		appmgrFreeMountHeapMemory(normalized);
		if (result != (int)0x80800002)
			goto error;
	} else {
		for (; mount != NULL; mount = mount->next) {
			if (mount->mountId == 900 &&
			    strncmp(mount->appPath, normalized, 292) == 0) {
				appmgrFreeMountHeapMemory(normalized);
				result = (int)0x80800017;
				goto error;
			}
		}
		appmgrFreeMountHeapMemory(normalized);
	}

	if (appmgrIsPfsContent(appPath) == 0) {
		content->contentFlags |= 1;
		goto checkMapFile;
	}
	if (content->contentFlags & 0x40) {
		APPMGR_STRCAT(appPath, "/sce_sys/param.sfo", 292);
		result = appmgrReadParamSfo(0, appPath, 1, content);
		if (result < 0)
			goto paramError;
		__builtin_memcpy(titleId, content->titleId, sizeof(titleId));
		__builtin_memcpy(content->foregroundLookupName, titleId, sizeof(titleId));
	}
	result = appmgrReadApplicationLicense(appPath, notPcsi ? titleId : "PCSI00011",
					      &content->license);
	if (result < 0)
		goto error;
	result = SceRtcForDriver_401C0954(&tick);
	if (result < 0) {
		if ((content->license.flags & 2) != 0 ||
		    content->license.licExpTime != 0x7fffffffffffffffULL)
			goto error;
	} else {
		if (tick.tick < content->license.licStartTime) {
			result = (int)0x80800018;
			goto error;
		}
		if (content->license.licExpTime < tick.tick) {
			result = (int)0x80800011;
			goto error;
		}
	}

	if (content->license.flags & 2) {
		char device[16] = { 0 };
		char decoded[16] = { 0 };
		char rifName[48];
		char *rifPath;

		permission = sceKernelSetThreadAccessLevel(64);
		SceKernelSuspendForDriver_4DF40893(0);
		rifPath = appmgrAllocMountHeapMemory(128);
		if (rifPath != NULL) {
			memset(rifPath, 0, 128);
			if (appmgrExtractSaveDataDeviceName(appPath, device, sizeof(device)) == 0 &&
			    strncmp(device, "ux0:", 5) == 0 &&
			    SceNpDrmForDriver_DF62F3B8(rifName, 0) >= 0) {
				if (appmgrDecodeString(encodedLicenseApplicationDirectory, decoded,
						       sizeof(decoded)) == 0 &&
				    (SceSize)snprintf(rifPath, 128, "%s/%s/%s/%s", device, decoded,
						      titleId, rifName) < 128) {
					appmgrFreeMountHeapMemory(rifPath);
					SceKernelSuspendForDriver_2BB92967(0);
					sceKernelSetThreadAccessLevel(permission);
					content->license.flags &= ~2;
				}
				goto mountApplication;
			}
			appmgrFreeMountHeapMemory(rifPath);
		}
		SceKernelSuspendForDriver_2BB92967(0);
		sceKernelSetThreadAccessLevel(permission);
	}

mountApplication:
	memset(appPath, 0, 292);
	strncpy(appPath, content->appDirectory, 291);
	result = appmgrCheckPackageLicenseFlag(appPath, &hasLicense);
	if (result < 0)
		goto error;
	if (hasLicense && appmgrGetConsoleType() != 0) {
		memset(content->license.klicense, 0, sizeof(content->license.klicense));
		content->contentFlags |= 4;
	}
	result = appmgrMakeRandomDriveId("eb", driveId.drive_id);
	if (result < 0)
		goto error;
	strcpy(&driveId.drive_id[13], "d:");
	result = appmgrMount(sceKernelGetProcessId(), mounts, 1000, titleId, appPath, &driveId,
			     (const ScePfsRndDriveId *)content->license.klicense,
			     (ScePfsRndDriveId *)cleanup->driveId);
	if (result < 0)
		goto error;

checkMapFile:
	if (appmgrGetConsoleType() == 3 && (content->contentFlags & 1) != 0) {
		mapFile = appmgrAllocMountHeapMemory(sizeof(*mapFile));
		cleanup->mapFile = mapFile;
		if (mapFile == NULL) {
			result = (int)0x80801006;
			goto releaseMounts;
		}
		directoryEnd = strrchr(path, '/');
		backslash = strrchr(path, '\\');
		colon = strrchr(path, ':');
		if (directoryEnd <= backslash) {
			if (backslash > colon) {
				directoryEnd = backslash;
			} else {
				if (colon == NULL) {
					directoryEnd = NULL;
					memset(g_appmgrMapFilePath, 0, sizeof(g_appmgrMapFilePath));
					goto openMapFile;
				}
				directoryEnd = colon + 1;
			}
		}
		memset(g_appmgrMapFilePath, 0, sizeof(g_appmgrMapFilePath));
		if (directoryEnd != NULL)
			APPMGR_STRNCPY(g_appmgrMapFilePath, path, directoryEnd - path,
				       sizeof(g_appmgrMapFilePath));
openMapFile:
		if (options != NULL && options->size == sizeof(*options) &&
		    options->mapfilePath[0] != 0) {
			result = appmgrMapFileValidatePath(options->mapfilePath,
							   mapFile->mapFilePath);
			if (result < 0)
				goto mapFileError;
			result = appmgrMapFileOpen(options->mapfilePath, mapFile);
			if (result < 0)
				goto mapFileError;
		} else {
			APPMGR_STRCAT(g_appmgrMapFilePath, "/configuration.psp2path",
				      sizeof(g_appmgrMapFilePath));
			g_appmgrMapFilePath[sizeof(g_appmgrMapFilePath) - 1] = 0;
			appmgrMapFileOpen(g_appmgrMapFilePath, mapFile);
			if (directoryEnd != NULL) {
				APPMGR_STRNCPY(g_appmgrMapFilePath, path, directoryEnd - path,
					       sizeof(g_appmgrMapFilePath));
				g_appmgrMapFilePath[directoryEnd - path] = 0;
			}
		}
		if (mapFile->file > 0) {
			result = appmgrMapFileRead(g_appmgrMapFilePath, mapFile);
			if (result < 0) {
mapFileError:
				sceAppMgrMapFilePrintParseError((const signed char *)mapFile->mapFilePath);
				appmgrMapFileClose(mapFile);
				goto error;
			}
			sceAppMgrDebugDumpMapFileInfo(mapFile);
		}
		appmgrMapFileClose(mapFile);
		memset(appPath, 0, 292);
		result = appmgrResolveLaunchDirectory(path, options, cleanup->mapFile, appPath, 292);
		if (result != 0)
			goto checkResult;
		APPMGR_STRCAT(appPath, "/sce_sys/param.sfo", 292);
		appmgrReadParamSfo(0, appPath, 1, content);
	}

	appmgrInitializeDebugUpgradable();
	if ((content->contentFlags & 1) == 0) {
		APPMGR_STRCAT(appPath, "/sce_sys/param.sfo", 292);
		result = appmgrReadParamSfo(0, appPath, 1, content);
		if (result >= 0) {
			AppmgrSfoFile file;
			void *data;
			SceSize length, capacity;
			SceUInt32 attribute, value;

			if (appmgrGetPatchPath(path, content->patchTitleId, content->attribute,
					       content->category, appPath) < 0 ||
			    appmgrCreateDirectories(appPath, 0) < 0)
				goto resolvePath;
			result = appmgrMount(sceKernelGetProcessId(), mounts, 1001, titleId, appPath,
					     &driveId,
					     (const ScePfsRndDriveId *)content->license.klicense,
					     (ScePfsRndDriveId *)cleanup->driveId);
			if (result < 0)
				goto error;
			APPMGR_STRCAT(appPath, "/sce_sys/param.sfo", 292);
			file.buffer = NULL;
			file.size = 0;
			file.psf = NULL;
			data = NULL;
			length = 0;
			capacity = 0;
			result = appmgrLoadSfoFile(appPath, &file);
			if (result >= 0) {
				attribute = content->attribute;
				if (appmgrGetSfoEntry(&file, "ATTRIBUTE", &data, &length,
						      &capacity) >= 0 &&
				    length > 3 && capacity == 4)
					value = *(SceUInt32 *)data;
				else
					value = 0;
				if (attribute & 1)
					value |= 1;
				else
					value &= ~1;
				if (attribute & 0x100000)
					value |= 0x100000;
				else
					value &= ~0x100000;
				content->attribute = value;
				value = 0;
				if (appmgrGetSfoEntry(&file, "ATTRIBUTE2", &data, &length,
						      &capacity) >= 0 &&
				    length > 3 && capacity == 4)
					value = *(SceUInt32 *)data;
				content->attribute2 = value;
				if (appmgrGetSfoEntry(&file, "ATTRIBUTE_MINOR", &data, &length,
						      &capacity) >= 0 &&
				    length > 3 && capacity == 4) {
					content->attributeMinorMode = *(SceUInt32 *)data & 3;
					content->attributeMinor = *(SceUInt32 *)data;
				} else {
					content->attributeMinorMode = 0;
					content->attributeMinor = 0;
				}
				appmgrReleaseSfoFile(&file);
				goto resolvePath;
			}
			appmgrReleaseSfoFile(&file);
		}
		goto paramError;
	}

resolvePath:
	if (cleanup->driveId[0] != 0) {
		snprintf(appPath, 292, "%s%s", cleanup->driveId, slash);
	} else {
		strncpy(appPath, path, 291);
		appPath[291] = 0;
	}
	resolvedPath = cleanup->resolvedPath;
	pid = sceKernelGetProcessId();
	permission = sceKernelSetThreadAccessLevel(128);
	result = SceFios2KernelForDriver_0F456345(pid, 0, appPath, resolvedPath, 292);
	sceKernelSetThreadAccessLevel(permission);
	if (result < 0)
		goto error;
	resolvedPath = cleanup->resolvedPath;
	if (strncmp(resolvedPath, "PD", 2) == 0) {
		result = appmgrResolvePfsDevicePath(resolvedPath, appPath, 292);
		if (result < 0)
			goto error;
		strncpy(cleanup->resolvedPath, appPath, 292);
		cleanup->resolvedPath[291] = 0;
		resolvedPath = cleanup->resolvedPath;
	}
	limitedFiles = 0;
	if (info != NULL)
		limitedFiles = (info->processAttr & 1) != 0;
	content->maxFD = limitedFiles ? 8 : 16;
	content->maxDIR = 6;
	ebootParam = appmgrAllocMountHeapMemory(sizeof(*ebootParam));
	if (ebootParam == NULL)
		goto noMemory;
	result = sceAppMgrGetEbootParam(resolvedPath, ebootParam);
	if (result >= 0) {
		content->selfControlFlags = ebootParam->attribute;
		content->selfControlMetadata = ebootParam->phcontMemSize;
		content->selfControlMode = ebootParam->memSize;
		content->maxFD = ebootParam->maxFD;
		content->maxDIR = ebootParam->maxDIR;
		content->primaryResourceQuota = ebootParam->primaryResourceQuota;
		content->secondaryResourceQuota = ebootParam->secondaryResourceQuota;
		if (content->selfControlFlags & 2) {
			if (content->primaryResourceQuota == 0)
				content->primaryResourceQuota = 3;
		} else if (content->selfControlMode != 0) {
			if (content->primaryResourceQuota == 0)
				content->primaryResourceQuota = 0;
			if (content->secondaryResourceQuota == 0)
				content->secondaryResourceQuota = 2;
			goto checkLimits;
		} else if (content->primaryResourceQuota == 0) {
			content->primaryResourceQuota = 6;
		}
		if (content->secondaryResourceQuota == 0)
			content->secondaryResourceQuota = 1;
checkLimits:
		if (content->maxFD == 0)
			content->maxFD = limitedFiles ? 8 : 16;
		if (content->maxDIR == 0)
			content->maxDIR = 6;
	}
	appmgrFreeMountHeapMemory(ebootParam);
checkResult:
	if (result < 0)
		goto error;
	goto done;

paramError:
	if (result != (int)0x80010002)
		goto error;
	result = (int)0x8080000D;
	goto error;
noMemory:
	result = (int)0x80801006;
error:
	if (cleanup->mapFile != NULL) {
		appmgrFreeMountHeapMemory(cleanup->mapFile);
		cleanup->mapFile = NULL;
	}
releaseMounts:
	if (cleanup->driveId[0] != 0)
		appmgrUnmountDrive(sceKernelGetProcessId(), mounts, cleanup->driveId, 0);
	if (cleanup->resolvedPath != NULL)
		appmgrFreeMountHeapMemory(cleanup->resolvedPath);
	if (cleanup->mapFile != NULL)
		appmgrFreeMountHeapMemory(cleanup->mapFile);
	memset(cleanup, 0, sizeof(*cleanup));
	if (appPath == NULL)
		goto unlock;
done:
	appmgrFreeMountHeapMemory(appPath);
unlock:
	content->mapFile = cleanup->mapFile;
	sceKernelUnlockMutex(g_appmgrMountMutex, 1);
	return result;
}

#undef APPMGR_STRCAT
#undef APPMGR_STRNCPY

int appmgrFreeMountHeapMemory(void *memory)
{
	return sceKernelFreeHeapMemory(g_appmgrMountHeap, memory);
}

/*
 * A host's mount of an application (900) or its save data (901) for
 * SceDeci4pRDrfp, under the host's authority ID.
 */
int sceAppMgrRdrfpMount(unsigned int type, const char *path, const SceAppMgrRdrfpPasscode *passcode,
			SceAppMgrRdrfpMountPoint *mountPoint)
{
	unsigned char keystone[96];
	char titleId[16];
	ScePfsRndDriveId key;
	SceSelfAuthInfo authInfo;
	SceAllocOpt opt;
	AppmgrPfsMount *pfsMount, *mount;
	char *resolved, *normalized;
	SceUInt32 state;
	SceUID pid;
	int result, resolveResult, permission, allowed, verified, hasPasscode, index;

	state = sceSyscallEnter();
	result = sceKernelLockMutex(g_appmgrMountMutex, 1, NULL);
	if (result < 0)
		goto out;
	switch (type) {
	case 106:
	case 107:
		if (SceSblACMgrForDriver_C98D82EE(0) == 0 && sceSblACMgrIsSystemProgram(0) == 0)
			goto denied;
		break;
	case 110 ... 112:
	case 302 ... 304:
	case 500 ... 505:
	case 900:
	case 901:
		if (sceKernelGetProcessSelfAuthInfo(0, &authInfo) < 0)
			goto denied;
		if (type == 112)
			allowed = SceSblACMgrForDriver_6C5AB07F(0);
		else if (type > 112)
			allowed = (type - 900) <= 1 ?
				  authInfo.programAuthorityId == 0x2808000000000001ULL :
				  authInfo.programAuthorityId == 0x2800000000000001ULL;
		else if (type == 110)
			allowed = authInfo.programAuthorityId == 0x280000000000002DULL ||
				  authInfo.programAuthorityId == 0x2800000000000001ULL;
		else
			allowed = SceSblACMgrForDriver_F5AE24AC(0) != 0;
		if (allowed == 0)
			goto denied;
		break;
	case 205:
		if (SceSblACMgrForDriver_456DA7AC(0) == 0)
			goto denied;
		break;
	case 600:
		if (SceSblACMgrForDriver_4DB7F512(0) == 0)
			goto denied;
		break;
	}
	if ((type - 900) > 1 || mountPoint == NULL || path == NULL) {
		result = (int)0x80800001;
		goto unlock;
	}

	opt.size = sizeof(opt);
	opt.mmapKind = 0;
	opt.align = 32;
	opt.mmapBase = NULL;
	opt.mmapSize = 0;
	resolved = sceKernelAllocHeapMemoryWithOption(g_appmgrMountHeap, 292, &opt);
	if (resolved == NULL)
		goto noMemory;
	memset(resolved, 0, 292);
	pid = sceKernelGetProcessId();
	permission = sceKernelSetThreadAccessLevel(128);
	resolveResult = SceFios2KernelForDriver_0F456345(pid, 0, path, resolved, 292);
	sceKernelSetThreadAccessLevel(permission);
	if (resolveResult < 0)
		goto badPath;
	if (type == 900) {
		if (strncmp(resolved, "ux0:/app/", 9) != 0 &&
		    strncmp(resolved, "ux0:/patch/", 11) != 0 &&
		    strncmp(resolved, "gro0:/app/", 10) != 0 &&
		    strncmp(resolved, "grw0:/patch/", 12) != 0)
			goto badPath;
	} else if ((strncmp(resolved, "ux0:/user/", 10) != 0 ||
		    strncmp(resolved + 12, "/savedata/", 10) != 0) &&
		   strncmp(resolved, "grw0:/savedata/", 15) != 0 &&
		   strncmp(resolved, "ux0:/data/savedata", 18) != 0) {
		goto badPath;
	}
	sceKernelFreeHeapMemory(g_appmgrMountHeap, resolved);
	if (resolveResult != 0)
		goto noMemory;

	__builtin_memcpy(titleId, (const char [16]){"NPXS10989"}, sizeof(titleId));
	memset(keystone, 0, sizeof(keystone));
	key = (ScePfsRndDriveId){{0}};
	memset(mountPoint, 0, sizeof(*mountPoint));
	mount = g_appmgrPfsMounts;
	normalized = appmgrAllocMountPath();
	if (normalized == NULL) {
		result = (int)0x80801006;
		pfsMount = NULL;
		goto failed;
	}
	result = appmgrNormalizePath(path, normalized);
	if (result >= 0) {
		for (; mount != NULL; mount = mount->next) {
			if (strncmp(mount->appPath, normalized, 292) == 0) {
				result = (int)0x80800003;
				pfsMount = NULL;
				sceKernelFreeHeapMemory(g_appmgrMountHeap, normalized);
				goto failed;
			}
		}
		sceKernelFreeHeapMemory(g_appmgrMountHeap, normalized);
	} else {
		sceKernelFreeHeapMemory(g_appmgrMountHeap, normalized);
		if (result != (int)0x80800002) {
			pfsMount = NULL;
			goto failed;
		}
	}

	pfsMount = appmgrAllocMountHeapMemory(sizeof(*pfsMount));
	if (pfsMount == NULL) {
		result = (int)0x80801006;
		goto failed;
	}
	memset(pfsMount, 0, sizeof(*pfsMount));
	pfsMount->mountId = type;
	strncpy(pfsMount->appPath, path, sizeof(pfsMount->appPath) - 1);
	result = appmgrMakeRandomDriveId("PD", pfsMount->mountPoint.drive_id);
	if (result < 0)
		goto failed;
	pfsMount->mountPoint.drive_id[12] = 'd';
	pfsMount->mountPoint.drive_id[13] = '\0';
	for (index = 0; index < 16; index++)
		if (pfsMount->programAuthorityIds[index] == 0)
			break;
	if (index == 16) {
		result = (int)0x8080001e;
		goto failed;
	}
	pfsMount->programAuthorityIds[index] = 0x2F00000000000001ULL;
	hasPasscode = SceSblPostSsMgrForDriver_C93C0A0D();

	if (type == 900) {
		result = ScePfsMgrForKernel_A772209C(path, pfsMount->mountPoint.drive_id,
						     0x2F00000000000001ULL, NULL, 10);
		if (result < 0)
			goto failed;
		verified = 0;
	} else {
		result = appmgrReadSealedKey(path, &key);
		if (result < 0)
			goto failed;
		for (index = 0; index < 16; index++)
			if (key.drive_id[index] != 0)
				break;
		result = ScePfsMgrForKernel_A772209C(path, pfsMount->mountPoint.drive_id,
						     0x2F00000000000001ULL,
						     index != 16 ? &key : NULL, 9);
		if (result < 0)
			goto failed;
		*mountPoint = *(SceAppMgrRdrfpMountPoint *)&pfsMount->mountPoint;
		mountPoint->path[15] = '\0';
		mountPoint->path[13] = '0';
		mountPoint->path[14] = ':';
		result = appmgrReadKeystone(sceKernelGetProcessId(), mountPoint->path, keystone);
		if (result < 0)
			goto failed;
		for (index = 0; index < 96; index++)
			if (keystone[index] != 0)
				break;
		if (index == 96) {
			verified = 0;
		} else {
			verified = SceSblPostSsMgrForDriver_DDA6FA6D(keystone, 0);
			if (verified != 0 && hasPasscode == 0)
				goto mounted;
		}
		result = ScePfsMgrForKernel_680BC384(pfsMount->mountPoint.drive_id);
		if (result < 0)
			goto failed;
		memset(mountPoint, 0, sizeof(*mountPoint));
		for (index = 0; index < 16; index++)
			if (key.drive_id[index] != 0)
				break;
		result = ScePfsMgrForKernel_A772209C(path, pfsMount->mountPoint.drive_id,
						     0x2F00000000000001ULL,
						     index != 16 ? &key : NULL, 7);
		if (result < 0)
			goto failed;
	}
mounted:
	*mountPoint = *(SceAppMgrRdrfpMountPoint *)&pfsMount->mountPoint;
	mountPoint->path[13] = '0';
	mountPoint->path[14] = ':';
	mountPoint->path[15] = '\0';
	result = appmgrReadKeystone(sceKernelGetProcessId(), mountPoint->path, keystone);
	if (result < 0)
		goto failed;
	if (appmgrIsDevelopmentPath(path) == 0) {
		if (passcode == NULL) {
			result = (int)0x80800001;
			goto failed;
		}
		if (SceSblPostSsMgrForDriver_F86F1452(keystone, (const char *)passcode) < 0) {
			result = (int)0x80800006;
			goto failed;
		}
	}
	if (type == 901 && (verified == 0 || hasPasscode != 0)) {
		result = appmgrUpdateSaveDataParams(path, titleId, 0, NULL);
		if (result < 0)
			goto failed;
	}
	result = ScePfsMgrForKernel_D8D0FEE5(pfsMount->mountPoint.drive_id, 0x2800000000000001ULL);
	if (result < 0)
		goto failed;
	result = ScePfsMgrForKernel_D8D0FEE5(pfsMount->mountPoint.drive_id, 0x2800000000007001ULL);
	if (result != (int)0x80010011 && result < 0)
		goto unlock;
	if (g_appmgrPfsMounts == NULL) {
		g_appmgrPfsMounts = pfsMount;
		pfsMount->prev = NULL;
		pfsMount->next = NULL;
	} else {
		mount = g_appmgrPfsMounts;
		if (mount == pfsMount)
			goto duplicate;
		while (mount->next != NULL) {
			if (mount == pfsMount)
				goto duplicate;
			mount = mount->next;
		}
		mount->next = pfsMount;
		pfsMount->prev = mount;
		pfsMount->next = NULL;
	}
	result = 0;
	goto unlock;

duplicate:
	result = (int)0x80800003;
failed:
	if (mountPoint->path[0] != '\0')
		ScePfsMgrForKernel_680BC384(pfsMount->mountPoint.drive_id);
	memset(mountPoint, 0, sizeof(*mountPoint));
	if (pfsMount != NULL)
		sceKernelFreeHeapMemory(g_appmgrMountHeap, pfsMount);
	goto unlock;
badPath:
	sceKernelFreeHeapMemory(g_appmgrMountHeap, resolved);
noMemory:
	result = (int)0x8080000c;
	goto unlock;
denied:
	result = (int)0x80800009;
unlock:
	sceKernelUnlockMutex(g_appmgrMountMutex, 1);
out:
	sceSyscallLeave(state);
	return result;
}
