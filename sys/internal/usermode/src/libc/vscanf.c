/* vscanf function */
#include "xstdio.h"

static int scin(void *str, int ch, int getfl)
{
	if (getfl)
		return fgetc((FILE *)str);
	return ungetc(ch, (FILE *)str);
}

int vscanf(const char *__restrict fmt, va_list ap)
{
	int ans;

	_Lockfilelock(stdin);
	ans = _Scanf(&scin, stdin, fmt, ap, 0);
	_Unlockfilelock(stdin);
	return ans;
}
