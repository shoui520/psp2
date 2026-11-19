/* fsutil's device information: a device's size and free space (devctl 0x3001). */

#include <stddef.h>
#include <scetypes.h>
#include <kernel/iofilemgr.h>
#include <iofilemgr/iofilemgr_common.h>
#include <kernel/libkernel.h>

#include "fs_util.h"

#define FS_UTIL_DEVCTL_GET_DEV_INFO	0x3001

int sceFsUtilGetDevInfo(const char *path, SceUInt64 *maxSize, SceUInt64 *freeSize)
{
	SceIoDevInfo info;
	int ret;

	sceClibMemset(&info, 0, sizeof(info));
	ret = sceIoDevctl(path, FS_UTIL_DEVCTL_GET_DEV_INFO, NULL, 0, &info, sizeof(info));
	if (ret == 0) {
		*maxSize = info.maxSize;
		*freeSize = info.freeSize;
	}
	return ret;
}
