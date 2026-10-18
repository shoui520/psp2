/* Application content mounts. */

#include <scetypes.h>
#include <kernel/types.h>
#include <authmgr/sblauthmgr.h>
#include <sysmem/sysmem.h>
#include <ss_mgr/ss_mgr.h>
#include <psp2kern/kernel/processmgr.h>
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

/* SceSblPostSsMgr (ut_mgr.c): declared here until its exporter's header declares it */

struct AppmgrMapFile;

typedef struct AppmgrProcessMountCleanup {
	char driveId[16];
	char *resolvedPath;
	struct AppmgrMapFile *mapFile;
} AppmgrProcessMountCleanup;

_Static_assert(sizeof(AppmgrProcessMountCleanup) == 24,
	       "process mount cleanup size");
_Static_assert(offsetof(AppmgrProcessMountCleanup, driveId) == 0 &&
	       offsetof(AppmgrProcessMountCleanup, resolvedPath) == 16 &&
	       offsetof(AppmgrProcessMountCleanup, mapFile) == 20,
	       "process mount cleanup fields");

int appmgrUnmountDrive(SceUID pid, struct AppmgrMountHolder *holder,
		      const char *driveId, int force);
int appmgrCleanupProcessMount(struct AppmgrMountHolder *holder,
			      AppmgrProcessMountCleanup *cleanup);

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

extern int appmgrGetMountPointById(int mountId, char *mountPoint);

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

char *appmgrAllocMountPath(void)
{
	SceAllocOpt opt;

	opt.size = sizeof(opt);
	opt.mmapKind = 0;
	opt.align = 32;
	opt.mmapBase = NULL;
	opt.mmapSize = 0;
	return SceSysmemForDriver_49D4DD9B(g_appmgrMountHeap, 292, &opt);
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
		result = ScePfsMgrForKernel_2D67D8CA(&mount->mountPoint,
					  authInfo.programAuthorityId);
		return result == (int)0x80010002 || result >= 0 ? 0 : result;
	}
	if (mount->mountId == 600) {
		permission = sceKernelSetThreadAccessLevel(128);
		result = SceIofilemgrForDriver_20574100(0x10000, flags, 0);
		sceKernelSetThreadAccessLevel(permission);
		if (result < 0)
			goto restore;
	} else if (mount->mountPoint.drive_id[0] != 0) {
		permission = sceKernelSetThreadAccessLevel(128);
		result = ScePfsMgrForKernel_680BC384(&mount->mountPoint);
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
	license = SceSysmemForDriver_49D4DD9B(g_appmgrCoreHeap, 512, &opt);
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
		&info->flags, &info->skuFlags, (SceInt64 *)&info->licStartTime,
		(SceInt64 *)&info->licExpTime, &info->flags2);
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
	path = SceSysmemForDriver_49D4DD9B(g_appmgrMountHeap, 292, &opt);
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
	resolvedPath = SceSysmemForDriver_49D4DD9B(g_appmgrMountHeap, 292,
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

	mount = SceSysmemForDriver_49D4DD9B(g_appmgrMountHeap,
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
		status = ScePfsMgrForKernel_D8D0FEE5(&pfsMount->mountPoint,
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
	path = SceSysmemForDriver_49D4DD9B(g_appmgrMountHeap, 292, &opt);
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
	sfoPath = SceSysmemForDriver_49D4DD9B(g_appmgrMountHeap, 292, &opt);
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
	result = SceSblSsMgrForDriver_A5B5D269(&openPsId);
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
	sfoPath = SceSysmemForDriver_49D4DD9B(g_appmgrMountHeap, 292, &opt);
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
	params = SceSysmemForDriver_49D4DD9B(g_appmgrMountHeap,
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
	result = SceSblSsMgrForDriver_A5B5D269(&openPsId);
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
	sfoPath = SceSysmemForDriver_49D4DD9B(g_appmgrMountHeap, 292, &opt);
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
		     SceDipswForDriver_A98FC2FD(218) == 0) &&
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
	if (SceSblACMgrForDriver_E79C7A8D(0, "vs0:sys/external"))
		return appmgrGetMountPointById(1007, mountPoint);
	return (int)0x80800009;
}

int appmgrGetVs0UserDataDrive(char *mountPoint)
{
	if (SceSblACMgrForDriver_E79C7A8D(0, "vs0:data/external"))
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
	if (!SceSblACMgrForDriver_E79C7A8D(0, path)) {
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

int SceAppMgrForDriver_E8DB7942(const SceAppMgrRdrfpMountPoint *mountPoint)
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
	result = ScePfsMgrForKernel_680BC384(&mount->mountPoint);
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
			ScePfsMgrForKernel_2D67D8CA(&pfsMount->mountPoint,
					  authInfo.programAuthorityId);
			goto freeVirtualMount;
		}
		if (pfsMount->mountId == 600) {
			permission = sceKernelSetThreadAccessLevel(128);
			status = SceIofilemgrForDriver_20574100(0x10000, 1, 0);
			sceKernelSetThreadAccessLevel(permission);
			if (status < 0)
				goto restoreAuthority;
		} else if (pfsMount->mountPoint.drive_id[0] != 0) {
			permission = sceKernelSetThreadAccessLevel(128);
			status = ScePfsMgrForKernel_680BC384(&pfsMount->mountPoint);
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
		result = SceSblSsMgrForDriver_A5B5D269(openPsId);
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

extern int appmgrCreateSaveDataSlotFile(SceUID pid, const char *path,
				      SceUInt32 slotCount);

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
	metadataPath = SceSysmemForDriver_49D4DD9B(g_appmgrMountHeap, 292, &opt);
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
		count = SceIofilemgrForDriver_DF57A75F(fd, &stat, 1);
		if (count < 0)
			goto closeError;
		count = SceIofilemgrForDriver_43170575(fd, 0);
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
		count = SceIofilemgrForDriver_DF57A75F(fd, &stat, 1);
		if (count < 0)
			goto closeError;
		count = SceIofilemgrForDriver_43170575(fd, 0);
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
		result = SceSblSsMgrForDriver_A5B5D269(&openPsId);
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
	params = SceSysmemForDriver_49D4DD9B(g_appmgrMountHeap,
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
				count = SceIofilemgrForDriver_43170575(fd, 0);
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
	resolved = SceSysmemForDriver_49D4DD9B(g_appmgrMountHeap, 292, &opt);
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

void *appmgrAllocMountHeapMemory(SceSize size)
{
	SceAllocOpt opt;

	opt.size = sizeof(opt);
	opt.mmapKind = 0;
	opt.align = 32;
	opt.mmapBase = NULL;
	opt.mmapSize = 0;
	return SceSysmemForDriver_49D4DD9B(g_appmgrMountHeap, size, &opt);
}

int appmgrFreeMountHeapMemory(void *memory)
{
	return sceKernelFreeHeapMemory(g_appmgrMountHeap, memory);
}
