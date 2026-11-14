/* fopen_s function */
#include <errno.h>
#include <stdlib.h>

#include "xstdio.h"

errno_t fopen_s(FILE *__restrict *__restrict pstr, const char *__restrict filename,
		const char *__restrict mods)
{
	FILE *str;
	int share = 'U';

	if (pstr == NULL)
		return _EXT1_FAIL("fopen_s: bad FILE ** argument");
	*pstr = NULL;
	if (filename == NULL || mods == NULL)
		return _EXT1_FAIL("fopen_s: bad argument");
	if (*mods == 'u') {
		++mods;
		if (*mods == 'a' || *mods == 'w')
			share = 'u';
	}
	_Locksyslock(_LOCK_STREAM);
	str = _Foprep(filename, mods, _Fofind(), -1, share, 0);
	_Unlocksyslock(_LOCK_STREAM);
	*pstr = str;
	return str == NULL ? ERANGE : 0;
}
