/* vprintf function */
#include "xstdio.h"

static void *prout(void *str, const char *buf, size_t n)
{
	return fwrite(buf, 1, n, (FILE *)str) == n ? str : NULL;
}

int vprintf(const char *__restrict fmt, va_list ap)
{
	int ans;

	_Lockfilelock(stdout);
	ans = _Printf(&prout, stdout, fmt, ap, 0);
	_Unlockfilelock(stdout);
	return ans;
}
