/* printf_s function */
#include <stdlib.h>
#include "xstdio.h"

static void *prout(void *str, const char *buf, size_t n)
{
	return fwrite(buf, 1, n, (FILE *)str) == n ? str : NULL;
}

int printf_s(const char *__restrict fmt, ...)
{
	int ans;
	va_list ap;

	if (fmt == NULL) {
		_EXT1_FAIL("printf_s: bad argument");
		return -1;
	}
	va_start(ap, fmt);
	_Lockfilelock(stdout);
	ans = _Printf(&prout, stdout, fmt, ap, 1);
	_Unlockfilelock(stdout);
	va_end(ap);
	return ans;
}
