/* freopen function */
#include <errno.h>
#include "xstdio.h"

FILE *freopen(const char *__restrict filename, const char *__restrict mods,
	      FILE *__restrict str)
{
	unsigned short mode;

	if (str == NULL || filename == NULL) {
		errno = EINVAL;
		return NULL;
	}
	_Locksyslock(_LOCK_STREAM);
	mode = str->_Mode & _MALFIL;	/* keep the stream itself */
	str->_Mode &= ~_MALFIL;
	fclose(str);
	str->_Mode = mode;
	_Lockfilelock(str);
	str = _Foprep(filename, mods, str, _FD_INVALID, 0, 1);
	_Unlockfilelock(str);
	_Unlocksyslock(_LOCK_STREAM);
	return str;
}
