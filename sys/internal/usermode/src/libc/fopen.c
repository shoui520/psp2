/* fopen function */
#include "xstdio.h"

FILE *fopen(const char *__restrict filename, const char *__restrict mods)
{
	FILE *str;

	_Locksyslock(_LOCK_STREAM);
	str = _Foprep(filename, mods, _Fofind(), _FD_INVALID, 0, 0);
	_Unlocksyslock(_LOCK_STREAM);
	return str;
}
