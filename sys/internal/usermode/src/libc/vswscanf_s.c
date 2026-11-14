/* vswscanf_s function */
#include <stdlib.h>
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

int vswscanf_s(const wchar_t *__restrict buf, const wchar_t *__restrict fmt, va_list ap)
{
	int ans;

	if (buf == NULL || fmt == NULL) {
		_EXT1_FAIL("vswscanf_s: bad argument");
		ans = -1;
	} else {
		ans = _WScanf(&scin, (void *)&buf, fmt, ap, 1);
	}
	return ans;
}
