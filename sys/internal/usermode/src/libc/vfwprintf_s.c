/* vfwprintf_s function */
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

int vfwprintf_s(FILE *__restrict str, const wchar_t *__restrict fmt, va_list ap)
{
	int ans;

	if (str == NULL || fmt == NULL) {
		_EXT1_FAIL("vfwprintf_s: bad argument");
		return -1;
	}
	_Lockfilelock(str);
	ans = _WPrintf(&prout, str, fmt, ap, 1);
	_Unlockfilelock(str);
	return ans;
}
