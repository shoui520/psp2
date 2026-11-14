/* vfprintf_s function */
#include <stdlib.h>
#include "xstdio.h"

static void *prout(void *str, const char *buf, size_t n)
{
	return fwrite(buf, 1, n, (FILE *)str) == n ? str : NULL;
}

int vfprintf_s(FILE *__restrict str, const char *__restrict fmt, va_list ap)
{
	int ans;

	if (str == NULL || fmt == NULL) {
		_EXT1_FAIL("vfprintf_s: bad argument");
		return -1;
	}
	_Lockfilelock(str);
	ans = _Printf(&prout, str, fmt, ap, 1);
	_Unlockfilelock(str);
	return ans;
}
