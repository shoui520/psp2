/* wprintf_s function */
#include <stdlib.h>
#include <wchar.h>
#include "xstdio.h"

static void *prout(void *str, const wchar_t *buf, size_t n)
{
	for (; 0 < n; --n, ++buf) {
		if (fputwc(*buf, (FILE *)str) == WEOF)
			return NULL;
	}
	return str;
}

int wprintf_s(const wchar_t *__restrict fmt, ...)
{
	int ans;
	va_list ap;

	if (fmt == NULL) {
		_EXT1_FAIL("wprintf_s: bad argument");
		return -1;
	}
	va_start(ap, fmt);
	_Lockfilelock(stdout);
	ans = _WPrintf(&prout, stdout, fmt, ap, 1);
	_Unlockfilelock(stdout);
	va_end(ap);
	return ans;
}
