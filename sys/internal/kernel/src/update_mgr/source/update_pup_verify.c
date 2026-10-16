#include <psp2kern/io/fcntl.h>
#include "update_internal.h"

int sceSblUsVerifyPup(const char *path)
{
	SceUID fd;
	int result;

	fd = ksceIoOpen(path, 0x4000001, 0);
	if (fd < 0)
		return (int)0x800f0202;
	result = PupUtilVerifyPup(fd);
	ksceIoClose(fd);
	return result;
}
