#include <psp2kern/io/fcntl.h>
#include "update_internal.h"

int sceSblUsVerifyPupSegment(const char *path, uint64_t segment_index)
{
	SceUID fd;
	int result;

	fd = ksceIoOpen(path, 0x4000001, 0);
	if (fd < 0)
		return (int)0x800f0202;
	result = PupUtilVerifyPupSegment(fd, segment_index, 1, 1, 0);
	ksceIoClose(fd);
	return result;
}
