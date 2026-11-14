/* freopen_s function */
#include <errno.h>
#include <stdlib.h>

#include "xstdio.h"

errno_t freopen_s(FILE *__restrict *__restrict pstr, const char *__restrict filename,
		  const char *__restrict mods, FILE *__restrict str)
{
	unsigned short mode;
	FILE *res;
	int share = 'U';

	if (pstr == NULL)
		return _EXT1_FAIL("fopen_s: bad FILE ** argument");
	*pstr = NULL;
	if (filename == NULL || mods == NULL || str == NULL)
		return _EXT1_FAIL("fopen_s: bad argument");
	if (*mods == 'u') {
		++mods;
		if (*mods == 'a' || *mods == 'w')
			share = 'u';
	}
	_Locksyslock(_LOCK_STREAM);
	mode = str->_Mode & _MALFIL;
	str->_Mode &= ~_MALFIL;
	fclose(str);
	str->_Mode = mode;
	_Lockfilelock(str);
	res = _Foprep(filename, mods, str, -1, share, 1);
	_Unlockfilelock(res);
	_Unlocksyslock(_LOCK_STREAM);
	*pstr = res;
	return res == NULL ? ERANGE : 0;
}
