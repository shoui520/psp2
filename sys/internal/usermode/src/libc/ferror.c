/* ferror function */
#include "xstdio.h"

int ferror(FILE *str)
{
	int mode = 0;

	if (str != NULL) {
		_Lockfilelock(str);
		mode = str->_Mode & _MERR;
		_Unlockfilelock(str);
	}
	return mode;
}
