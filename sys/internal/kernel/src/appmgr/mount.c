/* Application content mounts. */

#include <psp2kern/types.h>
#include <sysmem/sysmem.h>
#include <psp2kern/kernel/iofilemgr.h>
#include <psp2kern/kernel/threadmgr.h>
#include <psp2kern/kernel/ssmgr.h>
#include <psp2kern/kernel/processmgr.h>
#include <psp2kern/kernel/acmgr.h>
#include <psp2kern/kernel/aimgr.h>
#include <psp2kern/npdrm.h>
#include <psp2kern/fios2.h>
#include <acmgr/acmgr.h>
#include <appmgr/appmgr_launch.h>
#include <appmgr/appmgr_rdrfp.h>
#include <regmgr/regmgr.h>
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

extern int sceSblSsDecryptSealedKey(const void *sealedKey, void *key);
extern int SceSblACMgrForDriver_E79C7A8D(SceUID pid, const char *path);
extern int SceSblACMgrForDriver_456DA7AC(SceUID pid);
extern int SceSblACMgrForDriver_F5AE24AC(SceUID pid);
extern int ksceSblACMgrIsAllowedExtendedMemory(SceUID pid);
extern int kscePfsAcidDirSet(const char *name, const char *directory);
extern int appmgrGetMountPointById(int mountId, char *mountPoint);
extern char *__strncpy_chk(char *destination, const char *source,
			   SceSize count, SceSize capacity);

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

int appmgrGetConsoleType(void)
{
	if (ksceSblAimgrIsCEX())
		return 0;
	if (ksceSblAimgrIsDEX())
		return 1;
	if (ksceSblACMgrIsDevelopmentMode())
		return 3;
	return 2;
}

void appmgrUpdateAccountDirectory(void)
{
	SceUInt32 account = 0;
	int permission, result;
	char tens, units;

	permission = ksceKernelSetPermission(128);
	result = sceRegMgrGetKeyInt("/CONFIG/NP2", "current_account2", (int *)&account);
	ksceKernelSetPermission(permission);
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
	return ksceKernelAllocHeapMemoryWithOption(g_appmgrMountHeap, 292, &opt);
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

	result = ksceKernelGetProcessSelfAuthInfo(pid, &authInfo);
	if (result < 0)
		return result;
	authorityId = authInfo.program_authority_id;
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
		result = ksceKernelGetProcessSelfAuthInfo(pid, &authInfo);
		if (result < 0)
			return result;
		if (authInfo.program_authority_id == 0x2800000000000001ULL ||
		    authInfo.program_authority_id == 0x2800000000000010ULL ||
		    authInfo.program_authority_id == 0x280000000000002dULL ||
		    (mount->mountId == 1005 &&
		     authInfo.program_authority_id == 0x2800000000000039ULL))
			return 0;
		result = kscePfsDisapprove(&mount->mountPoint,
					  authInfo.program_authority_id);
		return result == (int)0x80010002 || result >= 0 ? 0 : result;
	}
	if (mount->mountId == 600) {
		permission = ksceKernelSetPermission(128);
		result = ksceIoUmount(0x10000, flags, 0, 0);
		ksceKernelSetPermission(permission);
		if (result < 0)
			goto restore;
	} else if (mount->mountPoint.drive_id[0] != 0) {
		permission = ksceKernelSetPermission(128);
		result = kscePfsUnmount(&mount->mountPoint);
		ksceKernelSetPermission(permission);
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
	ksceKernelFreeHeapMemory(g_appmgrMountHeap, mount);
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
	license = ksceKernelAllocHeapMemoryWithOption(g_appmgrCoreHeap, 512, &opt);
	if (license == NULL) {
		result = (int)0x80801006;
		SceKernelSuspendForDriver_2BB92967(0);
		return result;
	}
	SceKernelSuspendForDriver_4DF40893(0);
	fd = ksceIoOpen(path, 0x10001, 0);
	if (fd == (int)0x80010002) {
		result = (int)0x80800004;
		goto out;
	}
	result = (int)0x80800010;
	if (fd < 0)
		goto out;
	length = ksceIoRead(fd, license, 512);
	ksceIoClose(fd);
	if (length != 512)
		goto out;
	result = ksceNpDrmGetRifInfo(license, 512, 1, info->contentId,
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
	} else if (ksceRtcFormatRFC3339(startTime, (SceRtcTick *)&info->licStartTime,
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
	} else if (ksceRtcFormatRFC3339(expiryTime, (SceRtcTick *)&info->licExpTime,
				     540) >= 0) {
		expiryTime[19] = ' ';
		expiryTime[20] = 'J';
		expiryTime[21] = 'S';
		expiryTime[22] = 'T';
		expiryTime[23] = 0;
	}
	result = ksceNpDrmGetRifVitaKey(license, info->klicense,
		&info->flags, &info->skuFlags, &info->licStartTime, &info->licExpTime);
	if (result >= 0) {
		if (info->licStartTime == 0) {
			startTime[0] = '-';
			startTime[1] = 0;
		} else if (ksceRtcFormatRFC3339(startTime,
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
		} else if (ksceRtcFormatRFC3339(expiryTime,
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
	ksceKernelFreeHeapMemory(g_appmgrCoreHeap, license);
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
	path = ksceKernelAllocHeapMemoryWithOption(g_appmgrMountHeap, 292, &opt);
	if (path == NULL) {
		result = (int)0x80801006;
		goto out;
	}
	snprintf(path, 292, "%s/%s/%s", appPath, "sce_sys", "package/head.bin");
	fd = ksceIoOpen(path, 0x10001, 0);
	if (fd < 0) {
		snprintf(path, 292, "%s/%s/%s", appPath, "sce_sys", "clearsign");
		fd = ksceIoOpen(path, 0x10001, 0);
	}
	result = fd;
	if (fd >= 0) {
		result = ksceIoRead(fd, headerBytes, sizeof(headerBytes));
		ksceIoClose(fd);
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
	ksceKernelFreeHeapMemory(g_appmgrMountHeap, path);
out:
	SceKernelSuspendForDriver_2BB92967(0);
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

int appmgrReadSealedKey(const char *appPath, void *key)
{
	SceAllocOpt opt;
	unsigned char sealedKey[80];
	char *path;
	int permission, result, length;

	permission = ksceKernelSetPermission(128);
	SceKernelSuspendForDriver_4DF40893(0);
	opt.size = sizeof(opt);
	opt.mmapKind = 0;
	opt.align = 32;
	opt.mmapBase = NULL;
	opt.mmapSize = 0;
	path = ksceKernelAllocHeapMemoryWithOption(g_appmgrMountHeap, 292, &opt);
	if (path != NULL) {
		snprintf(path, 292, "%s/sce_sys/%s", appPath, "sealedkey");
		result = ksceIoOpen(path, 0x10001, 4);
		if (result >= 0) {
			length = ksceIoRead(result, sealedKey, sizeof(sealedKey));
			ksceIoClose(result);
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
		ksceKernelFreeHeapMemory(g_appmgrMountHeap, path);
	} else {
		result = (int)0x80801006;
	}
	SceKernelSuspendForDriver_2BB92967(0);
	ksceKernelSetPermission(permission);
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

int appmgrResolveLaunchPath(const char *path, char *output, SceSize size)
{
	int result;

	result = ksceKernelLockMutex(g_appmgrMountMutex, 1, NULL);
	if (result >= 0) {
		if (strncmp(path, "PD", 2) != 0) {
			strncpy(output, path, size);
			output[size - 1] = 0;
		} else {
			result = appmgrResolvePfsDevicePath(path, output, size);
		}
		ksceKernelUnlockMutex(g_appmgrMountMutex, 1);
	}
	return result;
}

int ksceAppMgrCheckPfsMounted(const char *path)
{
	SceUID directory;
	int result;
#if FW_VERSION < 0x04000000
	int mounted = 0;
#endif

	if (path == NULL || strncmp(path, "ux0:", 4) != 0 ||
	    strnlen(path, 292) >= 292)
		return (int)0x80800001;
	result = ksceIoDopen(path);
	if (result >= 0) {
		directory = result;
#if FW_VERSION < 0x04000000
		result = ksceIoIoctl(directory, 0x1803, NULL, 0, &mounted, sizeof(mounted));
		if (result == 0)
			result = mounted;
#else
		result = 0;
#endif
		ksceIoDclose(directory);
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
	result = ksceKernelLockMutex(g_appmgrMountMutex, 1, NULL);
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
				    !ksceSblACMgrIsPSMDevAssistant(0))
					ksceSblACMgrIsSystemProgram(0);
				goto count;
			}
		}
		goto checkAuth;
	}
	if (mountId == 600) {
		ksceSblACMgrIsAllowedExtendedMemory(0);
	} else if (mountId > 600) {
		if (mountId - 900 <= 1)
			goto checkAuth;
	} else if (mountId - 500 <= 5) {
		goto checkAuth;
	}
	goto count;
checkAuth:
	if (ksceKernelGetProcessSelfAuthInfo(0, &authInfo) >= 0) {
		if (mountId == 111)
			SceSblACMgrForDriver_F5AE24AC(0);
		else if (mountId == 112)
			ksceSblACMgrIsNonGameProgram(0);
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
	ksceKernelUnlockMutex(g_appmgrMountMutex, 1);
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
	result = ksceKernelLockMutex(g_appmgrMountMutex, 1, NULL);
	if (result < 0)
		goto out;
	if (ksceKernelGetProcessSelfAuthInfo(0, &work.authInfo) < 0 ||
	    work.authInfo.program_authority_id != 0x2808000000000001ULL) {
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
	result = kscePfsUnmount(&mount->mountPoint);
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
	ksceKernelFreeHeapMemory(g_appmgrMountHeap, mount);
	result = 0;
unlock:
	ksceKernelUnlockMutex(g_appmgrMountMutex, 1);
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
	result = ksceKernelLockMutex(g_appmgrMountMutex, 1, NULL);
	if (result < 0)
		goto out;
	result = ksceKernelGetProcessSelfAuthInfo(0, &authInfo);
	if (result < 0)
		goto unlock;
	if (authInfo.program_authority_id != 0x2800000000000001ULL) {
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
	pid = ksceKernelGetProcessId();
	permission = ksceKernelSetPermission(128);
	result = ksceFiosKernelOverlayResolveSync(pid, 0, path, resolvedPath, 292);
	ksceKernelSetPermission(permission);
	if (result >= 0) {
		resolvedPath[15] = 0;
		result = kscePfsAcidDirSet(resolvedPath, directory);
	}
	ksceKernelFreeHeapMemory(g_appmgrMountHeap, resolvedPath);
unlock:
	ksceKernelUnlockMutex(g_appmgrMountMutex, 1);
out:
	sceSyscallLeave(state);
	return result;
}

int appmgrGetAccountIdentity(SceUInt64 *accountId, SceOpenPsId *openPsId,
			    SceUInt32 *currentAccount)
{
	int permission, result = 0;

	permission = ksceKernelSetPermission(64);
	if (accountId != NULL) {
		result = sceRegMgrGetKeyBin("/CONFIG/NP", "account_id", accountId, 8);
		if (result < 0)
			goto out;
	}
	if (openPsId != NULL) {
		result = ksceSblAimgrGetOpenPsId(openPsId);
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
	ksceKernelSetPermission(permission);
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
	result = ksceIoGetstat(path, &stat);
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
		permission = ksceKernelSetPermission(128);
		result = ksceIoMkdir(g_appmgrDirectoryPath, mode);
		ksceKernelSetPermission(permission);
		if (result != (int)0x80010011 && result != 0)
			return result;
	}
}

int appmgrReadDirectoryLicense(const char *directory, AppmgrLicenseInfo *info)
{
	char rifName[48], startTime[36], expiryTime[32], path[128];
	int cardPath, permission, result, attempt;

	cardPath = strncmp(directory, "gro0:", 5);
	permission = ksceKernelSetPermission(64);
	if (appmgrCreateDirectories(directory, 0) < 0) {
		result = (int)0x80800004;
		goto out;
	}
	if (cardPath != 0) {
		for (attempt = 0; attempt != 2; ++attempt) {
			if (attempt == 1) {
				result = ksceNpDrmGetFixedRifName(rifName, 0);
				if (result < 0)
					goto out;
			} else {
				result = ksceNpDrmGetRifName(rifName, 0);
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
				result = ksceNpDrmGetFixedRifName(rifName, 0);
				if (result < 0)
					goto out;
			} else {
				result = ksceNpDrmGetFixedRifName(rifName, 1);
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
		} else if (ksceRtcFormatRFC3339(startTime,
				(SceRtcTick *)&info->licStartTime, 540) >= 0) {
			startTime[10] = ' ';
			__builtin_memcpy(startTime + 19, " JST", 5);
		}
		if (info->licExpTime == 0) {
			expiryTime[0] = '-';
			expiryTime[1] = 0;
		} else if (ksceRtcFormatRFC3339(expiryTime,
				(SceRtcTick *)&info->licExpTime, 540) >= 0) {
			__builtin_memcpy(expiryTime + 19, " JST", 5);
		}
	}
out:
	ksceKernelSetPermission(permission);
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
	result = ksceKernelLockMutex(g_appmgrMountMutex, 1, NULL);
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
		result = ksceRtcGetCurrentSecureTick(&currentTick);
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
	ksceKernelUnlockMutex(g_appmgrMountMutex, 1);
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
	if (ksceSblAimgrIsCEX() || ksceSblAimgrIsDEX() ||
	    !ksceSblACMgrIsDevelopmentMode())
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
	resolved = ksceKernelAllocHeapMemoryWithOption(g_appmgrMountHeap, 292, &opt);
	if (resolved == NULL)
		return (int)0x80801006;
	memset(resolved, 0, 292);
	pid = sceKernelGetProcessId();
	permission = ksceKernelSetPermission(128);
	result = ksceFiosKernelOverlayResolveSync(pid, 0, directory, resolved, 292);
	ksceKernelSetPermission(permission);
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
		ksceKernelFreeHeapMemory(g_appmgrMountHeap, resolved);
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
	ret = ksceKernelLockMutex(g_appmgrMountMutex, 1, NULL);
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
	ksceKernelUnlockMutex(g_appmgrMountMutex, 1);
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
	ret = ksceKernelLockMutex(g_appmgrMountMutex, 1, NULL);
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
	ksceKernelUnlockMutex(g_appmgrMountMutex, 1);
leave:
	sceSyscallLeave(syscall);
	return ret;
}

int appmgrResolvePath(const char *path, SceUID pid, char *output, SceSize size)
{
	int permission, result;

	if (pid == 0)
		pid = sceKernelGetProcessId();
	permission = ksceKernelSetPermission(128);
	result = ksceFiosKernelOverlayResolveSync(pid, 0, path, output, size);
	ksceKernelSetPermission(permission);
	return result;
}

int sceAppMgrGetRawPath(char *path, char *resolvedPath, int capacity)
{
	SceUInt32 syscall = sceSyscallEnter();
	SceUID pid = sceKernelGetProcessId();
	int permission = ksceKernelSetPermission(128);
	unsigned int size = (unsigned int)capacity;
	int result;

	if (size >= 292)
		size = 292;
	result = ksceFiosKernelOverlayResolveSync(pid, 0, path, resolvedPath, size);
	ksceKernelSetPermission(permission);
	sceSyscallLeave(syscall);
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
	return ksceKernelAllocHeapMemoryWithOption(g_appmgrMountHeap, size, &opt);
}

int appmgrFreeMountHeapMemory(void *memory)
{
	return ksceKernelFreeHeapMemory(g_appmgrMountHeap, memory);
}
