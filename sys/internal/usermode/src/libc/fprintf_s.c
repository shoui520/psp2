/* fprintf_s function */
#include <stdlib.h>
#include "xstdio.h"

static void *prout(void *str, const char *buf, size_t n)
{
	return fwrite(buf, 1, n, (FILE *)str) == n ? str : NULL;
}

int fprintf_s(FILE *__restrict str, const char *__restrict fmt, ...)
{
	int ans;
	va_list ap;

	if (str == NULL || fmt == NULL) {
		_EXT1_FAIL("fprintf_s: bad argument");
		return -1;
	}
	va_start(ap, fmt);
	_Lockfilelock(str);
	ans = _Printf(&prout, str, fmt, ap, 1);
	_Unlockfilelock(str);
	va_end(ap);
	return ans;
}
