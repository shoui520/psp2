/*
 * NSKBL: the file calls the module manager makes (iofilemgr_if.c in Sony's debug
 * records), on the SceSfat volumes kbl_main.c mounts: os0: and sd0:, one file
 * open at a time.
 */

#include <stddef.h>
#include <string.h>
#include <sceerror.h>
#include <kernel/iofilemgr.h>
#include <iofilemgr/iofilemgr.h>
#include <sysmem/debug.h>
#include <sysmem/sysclib.h>
#include "sfat.h"

/* the device of the open file */
#define IO_CLOSED	0
#define IO_OPEN_OS0	2
#define IO_OPEN_SD0	3

#define IO_PATH_MAX	256

static int s_openDevice;

static inline int IoIsOpen(void)
{
	return s_openDevice == IO_OPEN_OS0 || s_openDevice == IO_OPEN_SD0;
}

SceUID sceIoOpen(const char *file, int flags, SceMode mode)
{
	char path[IO_PATH_MAX];
	SceUID fd;

	SCE_DBG_ASSERT_LEVEL(0, s_openDevice == IO_CLOSED, 76);
	strncpy(path, file, sizeof(path) - 1);
	path[sizeof(path) - 1] = '\0';
	if (strncmp("os0:", path, 4) == 0) {
		if (strnlen(path, sizeof(path) - 1) <= 6)
			return SCE_ERROR_ERRNO_ENOENT;
		path[3] = '/';
		fd = SfatOpen(&g_os0Volume, &path[3], 0);
		if (fd >= 0)
			s_openDevice = IO_OPEN_OS0;
		return fd;
	}
	if (strncmp("sd0:", path, 4) != 0)
		return SCE_ERROR_ERRNO_EIO;
	if (strnlen(path, sizeof(path) - 1) <= 6)
		return SCE_ERROR_ERRNO_ENOENT;
	path[3] = '/';
	fd = SfatOpen(&g_sd0Volume, &path[3], 0);
	if (fd >= 0)
		s_openDevice = IO_OPEN_SD0;
	return fd;
}

SceOff sceIoLseek(SceUID fd, SceOff offset, int whence)
{
	if (IoIsOpen())
		return SfatLseek(fd, offset, whence);
	SCE_DBG_ASSERT_LEVEL(0, 0, 139);
	return SCE_ERROR_ERRNO_EBADF;
}

int sceIoRead(SceUID fd, void *data, SceSize size)
{
	if (IoIsOpen())
		return SfatRead(fd, data, size);
	SCE_DBG_ASSERT_LEVEL(0, 0, 163);
	return SCE_ERROR_ERRNO_EBADF;
}

int sceIoClose(SceUID fd)
{
	int res;

	if (IoIsOpen()) {
		res = SfatClose(fd);
	} else {
		SCE_DBG_ASSERT_LEVEL(0, 0, 187);
		res = SCE_ERROR_ERRNO_EBADF;
	}
	s_openDevice = IO_CLOSED;
	return res;
}
