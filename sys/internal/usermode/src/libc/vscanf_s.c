/* vscanf_s function */
#include <stdlib.h>
#include "xstdio.h"

static int scin(void *str, int ch, int getfl)
{
	if (getfl)
		return fgetc((FILE *)str);
	return ungetc(ch, (FILE *)str);
}

int vscanf_s(const char *__restrict fmt, va_list ap)
{
	int ans;

	if (fmt == NULL) {
		_EXT1_FAIL("vscanf_s: bad argument");
		return -1;
	}
	_Lockfilelock(stdin);
	ans = _Scanf(&scin, stdin, fmt, ap, 1);
	_Unlockfilelock(stdin);
	return ans;
}
