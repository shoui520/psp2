/* fdopen, sceLibcFopenWithFD and sceLibcFopenWithFH functions */
#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <fios2/fios2_api.h>
#include "xstdio.h"

/* open a stream on the FIOS file handle fh; dealloc: release fh on failure */
static FILE *fopenFH(SceFiosFH fh, const char *mods, bool dealloc)
{
	SceFiosOffset off;
	FILE *str;
	int i;

	if (sceFiosIsValidHandle(fh)) {
		_Locksyslock(_LOCK_STREAM);
		for (i = 3; i < FOPEN_MAX; ++i)
			if (fileno(_Files[i]) == fh) {	/* a stream already has it */
				_Unlocksyslock(_LOCK_STREAM);
				if (dealloc)
					sceFiosDeallocatePassthruFH(fh);
				goto fail;
			}
		str = _Foprep(NULL, mods, _Fofind(), fh, 0, 0);
		_Unlocksyslock(_LOCK_STREAM);
		off = sceFiosFHSeek(fh, 0, SCE_FIOS_SEEK_CUR);
		if (off <= 0x7fffffff && fseek(str, (long)off, SEEK_SET) == 0)
			return str;

		_Locksyslock(_LOCK_STREAM);
		for (i = 3; i < FOPEN_MAX; ++i)
			if (_Files[i] == str) {
				free(str);
				_Files[i] = NULL;
				str = NULL;
			}
		_Unlocksyslock(_LOCK_STREAM);
		if (dealloc)
			sceFiosDeallocatePassthruFH(fh);
	} else if (dealloc) {
		sceFiosDeallocatePassthruFH(fh);
	}
fail:
	errno = EINVAL;
	return NULL;
}

FILE *sceLibcFopenWithFD(int fd, const char *mode)
{
	SceFiosFH fh = sceFiosFilenoToFH(fd);

	if (fh == 0)
		return NULL;
	return fopenFH(fh, mode, true);
}

FILE *sceLibcFopenWithFH(SceFiosFH fh, const char *mode)
{
	return fopenFH(fh, mode, false);
}

FILE *fdopen(int fd, const char *mode)
{
	return fopenFH(fd, mode, false);
}
