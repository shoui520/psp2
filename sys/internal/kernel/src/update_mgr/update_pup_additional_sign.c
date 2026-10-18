#include <iofilemgr/iofilemgr.h>
#include "update_internal.h"

int sceSblUsVerifyPupAdditionalSign(const char *path)
{
	SceUID fd;
	int result;

	fd = sceIoOpen(path, 0x4000001, 0);
	if (fd < 0)
		return (int)0x800f0202;
	result = PupUtilVerifyPupAdditionalSign(fd, 1, 1, 0);
	sceIoClose(fd);
	return result;
}
