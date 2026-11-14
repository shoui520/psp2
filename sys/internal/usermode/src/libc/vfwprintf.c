/* vfwprintf function */
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

int vfwprintf(FILE *__restrict str, const wchar_t *__restrict fmt, va_list ap)
{
	int ans;

	_Lockfilelock(str);
	ans = _WPrintf(&prout, str, fmt, ap, 0);
	_Unlockfilelock(str);
	return ans;
}
