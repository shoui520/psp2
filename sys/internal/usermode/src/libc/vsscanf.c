/* vsscanf function */
#include "xstdio.h"

static int scin(void *str, int ch, int getfl)
{
	const char *s = *(const char **)str;	/* SCE: plain char, so the value is sign-extended */

	if (!getfl) {
		*(const char **)str = s - 1;
		return ch;
	} else if (*s == '\0') {
		return EOF;
	} else {
		*(const char **)str = s + 1;
		return *s;
	}
}

int vsscanf(const char *__restrict buf, const char *__restrict fmt, va_list ap)
{
	int ans;

	ans = _Scanf(&scin, (void *)&buf, fmt, ap, 0);
	return ans;
}
