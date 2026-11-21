/* the music drive (music0:) mounted and unmounted */
#include <stddef.h>
#include <scetypes.h>
#include <sceerror.h>
#include <kernel/libkernel.h>
#include <libkernel/libkernel.h>
#include <apputil.h>
#include <appmgr/appmgr.h>
#include <pfsmgr/pfsmgr.h>
#include "apputil_private.h"

/* where the drive is mounted, empty while it is not */
static ScePfsRndDriveId g_musicMountPoint;

int musicInit(void)
{
	sceClibMemset(&g_musicMountPoint, 0, sizeof(g_musicMountPoint));
	return 0;
}

int musicMount(void)
{
	ScePfsRndDriveId drive;
	int ret;

	sceClibMemset(&drive, 0, sizeof(drive));
	ret = sceAppMgrAppDataMount(APPUTIL_MOUNT_ID_MUSIC, &drive);
	if (ret >= 0)
		sceClibMemcpy_safe(&g_musicMountPoint, &drive, sizeof(g_musicMountPoint));
	if (ret != 0) {
		switch (ret) {
		case APPMGR_ERROR_MOUNT_LIMIT_OVER:
			ret = SCE_APPUTIL_ERROR_MOUNT_LIMIT_OVER;
			break;
		case APPMGR_ERROR_ALREADY_MOUNTED:
			ret = SCE_APPUTIL_ERROR_BUSY;
			break;
		case SCE_ERROR_ERRNO_ENODEV:
			ret = SCE_APPUTIL_ERROR_MUSIC_DEVICE_NOT_FOUND;
			break;
		}
	} else {
		ret = 0;
	}
	return ret;
}

int musicUmount(void)
{
	int ret;

	if (g_musicMountPoint.drive_id[0] == '\0')
		return SCE_APPUTIL_ERROR_NOT_MOUNTED;
	ret = sceAppMgrUmount(g_musicMountPoint.drive_id);
	if (ret >= 0)
		sceClibMemset(&g_musicMountPoint, 0, sizeof(g_musicMountPoint));
	switch (ret) {
	case 0:
		return 0;
	case APPMGR_ERROR_NOT_MOUNTED:
		return SCE_APPUTIL_ERROR_NOT_MOUNTED;
	}
	return ret;
}
