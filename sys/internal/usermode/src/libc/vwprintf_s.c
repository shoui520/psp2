/* vwprintf_s function */
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

int vwprintf_s(const wchar_t *__restrict fmt, va_list ap)
{
	int ans;

	if (fmt == NULL) {
		_EXT1_FAIL("vwprintf_s: bad argument");
		return -1;
	}
	_Lockfilelock(stdout);
	ans = _WPrintf(&prout, stdout, fmt, ap, 1);
	_Unlockfilelock(stdout);
	return ans;
}
