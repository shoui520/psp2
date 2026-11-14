/* printf function */
#include "xstdio.h"

static void *prout(void *str, const char *buf, size_t n)
{
	return fwrite(buf, 1, n, (FILE *)str) == n ? str : NULL;
}

int printf(const char *__restrict fmt, ...)
{
	int ans;
	va_list ap;

	va_start(ap, fmt);
	_Lockfilelock(stdout);
	ans = _Printf(&prout, stdout, fmt, ap, 0);
	_Unlockfilelock(stdout);
	va_end(ap);
	return ans;
}
