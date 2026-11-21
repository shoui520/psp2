/*
 * The USB mass storage image on the memory card (SceAppUtilUmass): its loop-back image formatted,
 * mounted by its key and unmounted, and the image sizes the memory card can hold.
 */
#include <stddef.h>
#include <scetypes.h>
#include <sceerror.h>
#include <kernel/libkernel.h>
#include <libkernel/libkernel.h>
#include <appmgr.h>
#include <apputil.h>
#include <appmgr/appmgr.h>
#include <pfsmgr/pfsmgr.h>
#include "apputil_private.h"

/* the image's key: 32 capital hexadecimal digits, then four zero bytes */
#define UMASS_KEY_LENGTH		32
#define UMASS_KEY_PADDING		3

/* the image sizes sceAppMgrLoopBackFormat creates, by the memory card's size; each list ends with 0 */
static const SceUInt64 g_umassSizes4G[] = {0x800a0000ULL, 0};
static const SceUInt64 g_umassSizes8G[] = {0x800a0000ULL, 0x100000000ULL, 0x180000000ULL, 0};
static const SceUInt64 g_umassSizes16G[] = {0x800a0000ULL, 0x100000000ULL, 0x180000000ULL, 0x300000000ULL,
					    0};
static const SceUInt64 g_umassSizes32G[] = {0x800a0000ULL, 0x180000000ULL, 0x300000000ULL, 0x600000000ULL,
					    0};
static const SceUInt64 g_umassSizes64G[] = {0x800a0000ULL, 0x180000000ULL, 0x300000000ULL, 0x600000000ULL,
					    0xc00000000ULL, 0xe00000000ULL, 0};

/* where the image is mounted, empty while it is not */
static ScePfsRndDriveId g_umassMountPoint;

/* 32 capital hexadecimal digits, then zero bytes */
static inline int checkUmassKey(const char *key)
{
	const char *p;
	SceUInt32 j;
	int i, c;

	i = 0;
	c = *key;
	j = 0;
	p = key;
	do {
		if ((c < '0' || c > '9') && (c < 'A' || c > 'F')) {
			paramError(172);
			return 0;
		}
		i++;
		c = *++p;
	} while (i < UMASS_KEY_LENGTH);
	if (c != '\0') {
		paramError(172);
		return 0;
	}
	do {
		if (key[UMASS_KEY_LENGTH + 1 + j] != '\0') {
			paramError(173);
			return 0;
		}
		j++;
	} while (j < UMASS_KEY_PADDING);
	return 1;
}

int umassInit(void)
{
	sceClibMemset(&g_umassMountPoint, 0, sizeof(g_umassMountPoint));
	return 0;
}

/* the image size and reserved bytes each kind takes */
static inline int checkUmassFormatInfo(int type, const AppmgrLoopBackImageInfo *info)
{
	const SceUInt8 *reserved;
	SceUInt64 size;
	SceUInt32 i;

	switch (type) {
	case UMASS_FORMAT_CREATE:
		if (info == NULL) {
			paramError(174);
			return 0;
		}
		size = info->size;
		if (size != 0x800a0000ULL && size != 0x100000000ULL && size != 0x180000000ULL &&
		    size != 0x300000000ULL && size != 0x600000000ULL && size != 0xc00000000ULL &&
		    size != 0xe00000000ULL) {
			paramError(176);
			return 0;
		}
		break;
	case UMASS_FORMAT_QUERY:
	case UMASS_FORMAT_SHRINK:
		if (info == NULL)
			return 1;
		break;
	case UMASS_FORMAT_DELETE:
	case UMASS_FORMAT_OWNER:
		if (info == NULL)
			return 1;
		paramError(175);
		return 0;
	default:
		paramError(170);
		return 0;
	}
	reserved = info->reserved;
	for (i = 0; i < sizeof(info->reserved); i++, reserved++) {
		if (*reserved != 0) {
			paramError(177);
			return 0;
		}
	}
	return 1;
}

int umassFormat(int type, const char *key, AppmgrLoopBackImageInfo *info)
{
	AppmgrLoopBackImageInfo image;
	unsigned int command;
	int ret;

	sceClibMemset(&image, 0, sizeof(image));
	switch (type) {
	case UMASS_FORMAT_CREATE:
	case UMASS_FORMAT_QUERY:
	case UMASS_FORMAT_DELETE:
	case UMASS_FORMAT_SHRINK:
		if (key == NULL) {
			paramError(171);
			ret = APPUTIL_ERROR_NULL_STATE;
			goto out;
		}
		if (checkUmassKey(key) == 0) {
			ret = SCE_APPUTIL_ERROR_PARAMETER;
			goto out;
		}
		break;
	case UMASS_FORMAT_OWNER:
		if (key != NULL) {
			paramError(171);
			ret = APPUTIL_ERROR_NULL_STATE;
			goto out;
		}
		break;
	default:
		paramError(170);
		ret = APPUTIL_ERROR_NULL_STATE;
		goto out;
	}
	if (checkUmassFormatInfo(type, info) == 0) {
		ret = SCE_APPUTIL_ERROR_PARAMETER;
		goto out;
	}
	switch (type) {
	case UMASS_FORMAT_CREATE:
		command = APPMGR_LOOPBACK_CREATE;
		break;
	case UMASS_FORMAT_QUERY:
		command = APPMGR_LOOPBACK_QUERY;
		break;
	case UMASS_FORMAT_DELETE:
		command = APPMGR_LOOPBACK_DELETE;
		break;
	case UMASS_FORMAT_OWNER:
		command = APPMGR_LOOPBACK_OWNER;
		break;
	case UMASS_FORMAT_SHRINK:
		command = APPMGR_LOOPBACK_SHRINK;
		break;
	default:
		ret = APPUTIL_ERROR_NULL_STATE;
		goto out;
	}
	image.size = info != NULL ? info->size : 0;
	ret = sceAppMgrLoopBackFormat(APPUTIL_MOUNT_ID_UMASS_FORMAT, command, key,
				      info != NULL ? &image : NULL);
	if (ret >= 0 && info != NULL)
		info->size = image.size;
	if (ret == 0)
		return ret;
out:
	if (ret == APPMGR_ERROR_LOOPBACK_BUSY)
		ret = SCE_APPUTIL_ERROR_BUSY;
	else if (ret == APPMGR_ERROR_NO_PERMISSION)
		ret = SCE_APPUTIL_ERROR_NO_PERMISSION;
	return ret;
}

int umassMount(const char *key, SceAppUtilMountPoint *mountPoint)
{
	SceUInt32 i;
	int ret;

	if (key == NULL) {
		paramError(171);
		ret = SCE_APPUTIL_ERROR_PARAMETER;
		goto out;
	}
	if (checkUmassKey(key) == 0) {
		ret = SCE_APPUTIL_ERROR_PARAMETER;
		goto out;
	}
	if (mountPoint == NULL) {
		paramError(178);
		ret = SCE_APPUTIL_ERROR_PARAMETER;
		goto out;
	}
	if (g_umassMountPoint.drive_id[0] != '\0') {
		ret = SCE_APPUTIL_ERROR_BUSY;
		sceClibMemcpy_safe(mountPoint, &g_umassMountPoint, sizeof(*mountPoint));
		goto out;
	}
	ret = sceAppMgrLoopBackMount(APPUTIL_MOUNT_ID_UMASS, key, &g_umassMountPoint);
	if (ret < 0)
		sceClibMemset(&g_umassMountPoint, 0, sizeof(g_umassMountPoint));
	else
		sceClibMemcpy_safe(mountPoint, &g_umassMountPoint, sizeof(*mountPoint));
	if (ret == 0)
		return ret;
out:
	switch (ret) {
	case APPMGR_ERROR_NO_PERMISSION:
		return SCE_APPUTIL_ERROR_NO_PERMISSION;
	case SCE_ERROR_ERRNO_ENODEV:
		return APPUTIL_ERROR_UMASS_DEVICE_NOT_FOUND;
	case SCE_ERROR_ERRNO_ENOENT:
		return APPUTIL_ERROR_UMASS_IMAGE_NOT_FOUND;
	}
	return ret;
}

int umassUmount(void)
{
	int ret;

	if (g_umassMountPoint.drive_id[0] == '\0') {
		ret = SCE_APPUTIL_ERROR_NOT_MOUNTED;
	} else if (sceClibStrncmp(g_umassMountPoint.drive_id, "lm", 2) != 0) {
		ret = APPUTIL_ERROR_UMASS_MOUNT_POINT;
	} else {
		ret = sceAppMgrUmount(g_umassMountPoint.drive_id);
		if (ret >= 0 || ret == SCE_ERROR_ERRNO_ENODEV || ret == APPMGR_ERROR_NOT_MOUNTED)
			sceClibMemset(&g_umassMountPoint, 0, sizeof(g_umassMountPoint));
		if (ret == 0)
			return ret;
	}
	if (ret == APPMGR_ERROR_NO_PERMISSION)
		return SCE_APPUTIL_ERROR_NO_PERMISSION;
	return ret;
}

int umassForceUmount(void)
{
	int ret;

	ret = sceAppMgrForceUmount(APPUTIL_MOUNT_ID_UMASS);
	if (ret >= 0)
		sceClibMemset(&g_umassMountPoint, 0, sizeof(g_umassMountPoint));
	switch (ret) {
	case 0:
		return 0;
	case APPMGR_ERROR_NOT_MOUNTED:
		return SCE_APPUTIL_ERROR_NOT_MOUNTED;
	case APPMGR_ERROR_NO_PERMISSION:
		return SCE_APPUTIL_ERROR_NO_PERMISSION;
	}
	return ret;
}

int umassGetFormatSizes(SceUInt64 *sizes, SceUInt32 num)
{
	SceUInt64 maxSize = 0;
	const SceUInt64 *list;
	int ret, n;

	if (sizes == NULL) {
		paramError(300);
		ret = SCE_APPUTIL_ERROR_PARAMETER;
		goto out;
	}
	if (num != UMASS_FORMAT_SIZES_MAX) {
		paramError(301);
		ret = SCE_APPUTIL_ERROR_PARAMETER;
		goto out;
	}
	ret = sceAppMgrGetDevInfo("ux0:", &maxSize, NULL);
	if (ret >= 0) {
		if (maxSize <= 0x100000000ULL)
			list = g_umassSizes4G;
		else if (maxSize <= 0x200000000ULL)
			list = g_umassSizes8G;
		else if (maxSize <= 0x400000000ULL)
			list = g_umassSizes16G;
		else if (maxSize <= 0x800000000ULL)
			list = g_umassSizes32G;
		else
			list = g_umassSizes64G;
		sceClibMemset(sizes, 0, sizeof(*sizes) * num);
		for (n = 0; list[n] != 0 && n < UMASS_FORMAT_SIZES_MAX; n++)
			sizes[n] = list[n];
		ret = n;
	}
	if (ret > 0)
		return ret;
out:
	if (ret != 0) {
		if (ret == APPMGR_ERROR_NO_PERMISSION)
			ret = SCE_APPUTIL_ERROR_NO_PERMISSION;
	}
	return ret;
}
