/* vwprintf function */
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

int vwprintf(const wchar_t *__restrict fmt, va_list ap)
{
	int ans;

	_Lockfilelock(stdout);
	ans = _WPrintf(&prout, stdout, fmt, ap, 0);
	_Unlockfilelock(stdout);
	return ans;
}
