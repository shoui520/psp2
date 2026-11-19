/* fsutil's device information (fs_util.c), which libmtp builds in. */
#pragma once

#include <scetypes.h>

int sceFsUtilGetDevInfo(const char *path, SceUInt64 *maxSize, SceUInt64 *freeSize);
