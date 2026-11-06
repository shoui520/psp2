/*
 * SceAppUtilExt's video data drive (Sony's apputil/apputil_ext.h): SceAppMgr mounts it for the
 * caller, and the drive name it gives is kept until the drive is unmounted.
 */
#include <stddef.h>
#include <scetypes.h>
#include <apputil.h>
#include <apputil/apputil_ext.h>
#include <sceerror.h>
#include <kernel/libkernel.h>
#include <libkernel/libkernel.h>
#include <pfsmgr/pfsmgr.h>
#include <appmgr/appmgr.h>
#include "apputil_ext_internal.h"

/* SceAppMgr's application data mount of the video data */
#define APPUTIL_EXT_VIDEO_MOUNT_ID	109

static ScePfsRndDriveId s_videoMountPoint;

int initVideo(void)
{
	sceClibMemset(&s_videoMountPoint, 0, sizeof(s_videoMountPoint));
	return SCE_OK;
}

SceInt32 sceAppUtilExtVideoMount(void)
{
	ScePfsRndDriveId mountPoint;
	int ret;

	sceClibMemset(&mountPoint, 0, sizeof(mountPoint));
	ret = checkStack(APPUTIL_EXT_VIDEO_STACK_SIZE);
	if (ret >= 0) {
		ret = sceAppMgrAppDataMount(APPUTIL_EXT_VIDEO_MOUNT_ID, &mountPoint);
		if (ret >= 0)
			sceClibMemcpy_safe(&s_videoMountPoint, &mountPoint, sizeof(s_videoMountPoint));
	}
	switch (ret) {
	case SCE_OK:
		return SCE_OK;
	case APPMGR_ERROR_MOUNT_LIMIT_OVER:
		return SCE_APPUTIL_ERROR_MOUNT_LIMIT_OVER;
	case APPMGR_ERROR_ALREADY_MOUNTED:
		return SCE_APPUTIL_ERROR_BUSY;
	}
	return ret;
}

SceInt32 sceAppUtilExtVideoUmount(void)
{
	int ret;

	ret = checkStack(APPUTIL_EXT_VIDEO_STACK_SIZE);
	if (ret >= 0) {
		if (s_videoMountPoint.drive_id[0] == '\0')
			return SCE_APPUTIL_ERROR_NOT_MOUNTED;
		ret = sceAppMgrUmount(s_videoMountPoint.drive_id);
		if (ret >= 0)
			sceClibMemset(&s_videoMountPoint, 0, sizeof(s_videoMountPoint));
	}
	switch (ret) {
	case SCE_OK:
		return SCE_OK;
	case APPMGR_ERROR_NOT_MOUNTED:
		return SCE_APPUTIL_ERROR_NOT_MOUNTED;
	}
	return ret;
}
