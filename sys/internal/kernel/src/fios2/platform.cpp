/*
 * The platform layer: file system access for the overlay code, straight
 * onto the kernel's IO manager.
 */

#include <psp2kern/io/fcntl.h>
#include <psp2kern/kernel/sysclib.h>
#include "fios2_internal.h"

int sceFiosKernelPlatformGetstat(const char *path, SceIoStat *stat)
{
	return ksceIoGetstat(path, stat);
}

void *sceFiosKernelPlatformOpen(void)
{
	return NULL;
}

void sceFiosKernelPlatformClose(void *platform)
{
}

int sceFiosKernelPlatformDopen(SceUID *fd, const char *path, void *platform)
{
	SceUID ret = ksceIoDopen(path);

	if (ret >= 0) {
		*fd = ret;
		ret = 0;
	}
	return ret;
}

/* the next entry other than "." and "..": 1, or 0 at the end, or an error */
int sceFiosKernelPlatformDread(SceUID fd, SceIoDirent *dirent, void *platform)
{
	int ret;

	memset(dirent, 0, sizeof(*dirent));
	do {
		ret = ksceIoDread(fd, dirent);
		if (ret <= 0)
			return ret;
	} while (*(short *)dirent->d_name == '.' ||
		 (*(unsigned int *)dirent->d_name & 0xffffff) == ('.' << 8 | '.'));
	return 1;
}

int sceFiosKernelPlatformDclose(SceUID fd)
{
	return ksceIoDclose(fd);
}

/* nanoseconds since the Unix epoch, 0 when time cannot be converted */
SceUInt64 sceFiosKernelPlatformDateTimeToTime(const SceDateTime *time)
{
	SceUInt64 t = 0;

	if (ksceRtcConvertDateTimeToUnixTime(time, &t) != 0)
		return 0;
	return t * 1000000000;
}
