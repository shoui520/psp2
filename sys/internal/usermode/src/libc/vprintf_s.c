/* vprintf_s function */
#include <stdlib.h>
#include "xstdio.h"

static void *prout(void *str, const char *buf, size_t n)
{
	return fwrite(buf, 1, n, (FILE *)str) == n ? str : NULL;
}

int vprintf_s(const char *__restrict fmt, va_list ap)
{
	int ans;

	if (fmt == NULL) {
		_EXT1_FAIL("vprintf_s: bad argument");
		return -1;
	}
	_Lockfilelock(stdout);
	ans = _Printf(&prout, stdout, fmt, ap, 1);
	_Unlockfilelock(stdout);
	return ans;
}
