/* fprintf function */
#include "xstdio.h"

static void *prout(void *str, const char *buf, size_t n)
{
	return fwrite(buf, 1, n, (FILE *)str) == n ? str : NULL;
}

int fprintf(FILE *__restrict str, const char *__restrict fmt, ...)
{
	int ans;
	va_list ap;

	va_start(ap, fmt);
	_Lockfilelock(str);
	ans = _Printf(&prout, str, fmt, ap, 0);
	_Unlockfilelock(str);
	va_end(ap);
	return ans;
}
