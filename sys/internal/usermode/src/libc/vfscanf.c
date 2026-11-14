/* vfscanf function */
#include "xstdio.h"

static int scin(void *str, int ch, int getfl)
{
	if (getfl)
		return fgetc((FILE *)str);
	return ungetc(ch, (FILE *)str);
}

int vfscanf(FILE *__restrict str, const char *__restrict fmt, va_list ap)
{
	int ans;

	_Lockfilelock(str);
	ans = _Scanf(&scin, str, fmt, ap, 0);
	_Unlockfilelock(str);
	return ans;
}
