/* fscanf function */
#include "xstdio.h"

static int scin(void *str, int ch, int getfl)
{
	if (getfl)
		return fgetc((FILE *)str);
	return ungetc(ch, (FILE *)str);
}

int fscanf(FILE *__restrict str, const char *__restrict fmt, ...)
{
	int ans;
	va_list ap;

	va_start(ap, fmt);
	_Lockfilelock(str);
	ans = _Scanf(&scin, str, fmt, ap, 0);
	_Unlockfilelock(str);
	va_end(ap);
	return ans;
}
