/* vfprintf function */
#include "xstdio.h"

static void *prout(void *str, const char *buf, size_t n)
{
	return fwrite(buf, 1, n, (FILE *)str) == n ? str : NULL;
}

int vfprintf(FILE *__restrict str, const char *__restrict fmt, va_list ap)
{
	int ans;

	_Lockfilelock(str);
	ans = _Printf(&prout, str, fmt, ap, 0);
	_Unlockfilelock(str);
	return ans;
}
