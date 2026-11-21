/*
 * MtpResponderHostFs.cc: the responder that reads the file from the console's own file
 * system (0x81137208-0x811372D8), the "fakehost" protocol a test asks for.
 */
#include <stddef.h>
#include <scetypes.h>
#include <kernel/iofilemgr.h>
#include <kernel/iofilemgr_stat.h>
#include <std/stdlib.h>

#include "mtpr2_internal.h"

using namespace mtpr2;

int MtpResponderHostFs::startDownload(const char *url, long long offset, int *kind,
				      unsigned int *size, unsigned int *time)
{
	SceIoStat stat;
	int ret;
	SceUID fd;

	path = url;
	this->offset = (int)offset;
	ret = sceIoGetstat(url, &stat);
	if (ret < 0) {
		MTPR2_PRINT_ERROR();
		return ret;
	}
	*kind = 1;
	*size = stat.st_size;
	*time = 0;
	MTPR2_PRINT_ERROR();
	fd = sceIoOpen(path, SCE_O_RDONLY, 0606);
	if (fd < 0) {
		MTPR2_PRINT_ERROR();
		return fd;
	}
	this->fd = fd;
	return 0;
}

int MtpResponderHostFs::receiveData(void *buf, unsigned int size, unsigned int *received)
{
	int ret;

	ret = sceIoRead(fd, buf, size);
	if (ret < 0) {
		MTPR2_PRINT_ERROR();
		*received = 0;
		return ret;
	}
	*received = ret;
	return 0;
}

int MtpResponderHostFs::endDownload(void)
{
	int ret;

	ret = sceIoClose(fd);
	if (ret < 0)
		MTPR2_PRINT_ERROR();
	return ret;
}
