/* vsscanf_s function */
#include <stdlib.h>
#include "xstdio.h"

/* SCE: returns the character sign-extended, unlike sscanf_s */
static int scin(void *str, int ch, int getfl)
{
	const char *s = *(const char **)str;

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

int vsscanf_s(const char *__restrict buf, const char *__restrict fmt, va_list ap)
{
	int ans;

	if (buf == NULL || fmt == NULL) {
		_EXT1_FAIL("vsscanf_s: bad argument");
		ans = -1;
	} else {
		ans = _Scanf(&scin, (void *)&buf, fmt, ap, 1);
	}
	return ans;
}
