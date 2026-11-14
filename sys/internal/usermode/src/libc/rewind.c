/* rewind function */
#include "xstdio.h"

void rewind(FILE *str)
{
	_Lockfilelock(str);
	_Fspos(str, NULL, 0, SEEK_SET);
	str->_Mode &= ~_MERR;
	_Unlockfilelock(str);
}
