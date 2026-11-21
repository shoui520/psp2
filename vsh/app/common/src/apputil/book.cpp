/* the electronic book data drive mounted and unmounted (sceAppUtilBookMount, sceAppUtilBookUmount) */
#include <stddef.h>
#include <scetypes.h>
#include <sceerror.h>
#include <kernel/libkernel.h>
#include <libkernel/libkernel.h>
#include <apputil.h>
#include <apputil/apputil_internal.h>
#include <appmgr/appmgr.h>
#include <pfsmgr/pfsmgr.h>
#include "apputil_private.h"

/* where the drive is mounted, empty while it is not */
static ScePfsRndDriveId g_bookMountPoint;

int bookInit(void)
{
	sceClibMemset(&g_bookMountPoint, 0, sizeof(g_bookMountPoint));
	return 0;
}

int bookMount(SceAppUtilMountPoint *mountPoint)
{
	ScePfsRndDriveId drive;
	int ret;

	if (mountPoint == NULL) {
		paramError(220);
		return SCE_APPUTIL_ERROR_PARAMETER;
	}
	sceClibMemset(&drive, 0, sizeof(drive));
	ret = sceAppMgrAppDataMount(APPUTIL_MOUNT_ID_BOOK, &drive);
	if (ret >= 0) {
		sceClibMemcpy_safe(&g_bookMountPoint, &drive, sizeof(g_bookMountPoint));
		sceClibMemcpy_safe(mountPoint, &drive, sizeof(*mountPoint));
	}
	if (ret != 0) {
		if (ret == APPMGR_ERROR_ALREADY_MOUNTED) {
			sceClibMemcpy_safe(mountPoint, &g_bookMountPoint, sizeof(*mountPoint));
			ret = SCE_APPUTIL_ERROR_BUSY;
		} else if (ret == SCE_ERROR_ERRNO_ENODEV) {
			ret = SCE_APPUTIL_INTERNAL_ERROR_DEVICE_NOT_FOUND;
		}
	} else {
		ret = 0;
	}
	return ret;
}

int bookUmount(void)
{
	int ret;

	if (g_bookMountPoint.drive_id[0] == '\0')
		return SCE_APPUTIL_ERROR_NOT_MOUNTED;
	ret = sceAppMgrUmount(g_bookMountPoint.drive_id);
	if (ret >= 0)
		sceClibMemset(&g_bookMountPoint, 0, sizeof(g_bookMountPoint));
	switch (ret) {
	case 0:
		return 0;
	case APPMGR_ERROR_NOT_MOUNTED:
		return SCE_APPUTIL_ERROR_NOT_MOUNTED;
	}
	return ret;
}
