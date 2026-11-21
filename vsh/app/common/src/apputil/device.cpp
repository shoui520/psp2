/*
 * A storage device's size and free space, and the application's cache drive (cache0:) mounted
 * and unmounted.
 */
#include <stddef.h>
#include <scetypes.h>
#include <sceerror.h>
#include <kernel/libkernel.h>
#include <kernel/threadmgr.h>
#include <libkernel/libkernel.h>
#include <appmgr.h>
#include <apputil.h>
#include <appmgr/appmgr.h>
#include <pfsmgr/pfsmgr.h>
#include "apputil_private.h"

int deviceGetDevInfo(const char *device, SceUInt64 *maxSize, SceUInt64 *freeSize)
{
	int ret;

	if (device == NULL) {
		paramError(160);
		ret = SCE_APPUTIL_ERROR_PARAMETER;
	} else {
		ret = sceAppMgrGetDevInfo(device, maxSize, freeSize);
		if (ret == 0)
			return ret;
	}
	if (ret == APPMGR_ERROR_NO_PERMISSION)
		return SCE_APPUTIL_ERROR_NO_PERMISSION;
	if (ret == APPMGR_ERROR_INVALID_ARGUMENT) {
		paramError(161);
		return SCE_APPUTIL_ERROR_PARAMETER;
	}
	return ret;
}

int cacheMount(void)
{
	char titleId[16];
	ScePfsRndDriveId mountPoint;
	int ret;

	sceClibMemset(titleId, 0, sizeof(titleId));
	sceClibMemset(&mountPoint, 0, sizeof(mountPoint));
	ret = sceAppMgrAppParamGetString(sceKernelGetProcessId(), APPMGR_APP_PARAM_TITLE_ID, titleId,
					 SCE_APPUTIL_TITLE_ID_DATA_SIZE);
	if (ret >= 0)
		ret = sceAppMgrWorkDirMountById(APPUTIL_MOUNT_ID_CACHE, titleId, &mountPoint);
	if (ret != 0) {
		switch (ret) {
		case APPMGR_ERROR_NO_PERMISSION:
			ret = SCE_APPUTIL_ERROR_NO_PERMISSION;
			break;
		case APPMGR_ERROR_ALREADY_MOUNTED:
			ret = SCE_APPUTIL_ERROR_BUSY;
			break;
		case SCE_ERROR_ERRNO_ENODEV:
			ret = APPUTIL_ERROR_CACHE_DEVICE_NOT_FOUND;
			break;
		}
	} else {
		ret = 0;
	}
	return ret;
}

int cacheUmount(void)
{
	char mountPoint[SCE_APPUTIL_MOUNTPOINT_DATA_MAXSIZE];
	int ret;

	sceClibStrlcpy(mountPoint, "cache0:", sizeof(mountPoint));
	ret = sceAppMgrUmount(mountPoint);
	switch (ret) {
	case 0:
		ret = 0;
		break;
	case APPMGR_ERROR_NOT_MOUNTED:
		ret = SCE_APPUTIL_ERROR_NOT_MOUNTED;
		break;
	}
	return ret;
}

int cacheGetDevInfo(SceUInt64 *maxSize, SceUInt64 *freeSize)
{
	int ret;

	ret = sceAppMgrGetDevInfo("cache0:", maxSize, freeSize);
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
