/* scanf_s function */
#include <stdlib.h>
#include "xstdio.h"

static int scin(void *str, int ch, int getfl)
{
	if (getfl)
		return fgetc((FILE *)str);
	return ungetc(ch, (FILE *)str);
}

int scanf_s(const char *__restrict fmt, ...)
{
	int ans;
	va_list ap;

	if (fmt == NULL) {
		_EXT1_FAIL("scanf_s: bad argument");
		return -1;
	}
	va_start(ap, fmt);
	_Lockfilelock(stdin);
	ans = _Scanf(&scin, stdin, fmt, ap, 1);
	_Unlockfilelock(stdin);
	va_end(ap);
	return ans;
}
