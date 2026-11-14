/* swscanf function */
#include <wchar.h>

#include "xstdio.h"

static unsigned int scin(void *str, unsigned int ch, int getfl)
{
	const wchar_t *s = *(const wchar_t **)str;

	if (!getfl) {
		*(const wchar_t **)str = s - 1;
		return ch;
	} else if (*s == L'\0') {
		return WEOF;
	} else {
		*(const wchar_t **)str = s + 1;
		return *s;
	}
}

int swscanf(const wchar_t *__restrict buf, const wchar_t *__restrict fmt, ...)
{
	int ans;
	va_list ap;

	va_start(ap, fmt);
	ans = _WScanf(&scin, (void *)&buf, fmt, ap, 0);
	va_end(ap);
	return ans;
}
