/*
 * _Fopen: open a file for a stream, the stream's mode bits as SceIofilemgr
 * open flags, a new file readable and writable by the owner and others.
 */
#include <kernel/iofilemgr.h>

#include "xstdio.h"
#include "psp2io.h"

int _Fopen(const char *path, unsigned int smode, int share)
{
	static const int rwacc[] = {0, SCE_O_RDONLY, SCE_O_WRONLY, SCE_O_RDWR};
	int acc;

	(void)share;
	acc = rwacc[smode & 3];
	if (smode & _MOPENA)
		acc |= SCE_O_APPEND;
	if (smode & _MTRUNC)
		acc |= SCE_O_TRUNC;
	if (smode & _MCREAT)
		acc |= SCE_O_CREAT;
	return _Open(path, acc, 0606);
}
