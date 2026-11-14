/* feof function */
#include "xstdio.h"

int feof(FILE *str)
{
	int mode;

	_Lockfilelock(str);
	mode = str->_Mode & _MEOF;
	_Unlockfilelock(str);
	return mode;
}
