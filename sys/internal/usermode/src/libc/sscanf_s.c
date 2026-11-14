/* sscanf_s function */
#include <stdlib.h>
#include "xstdio.h"

static int scin(void *str, int ch, int getfl)
{
	const unsigned char *s = *(const unsigned char **)str;

	if (!getfl) {
		*(const unsigned char **)str = s - 1;
		return ch;
	} else if (*s == '\0') {
		return EOF;
	} else {
		*(const unsigned char **)str = s + 1;
		return *s;
	}
}

int sscanf_s(const char *__restrict buf, const char *__restrict fmt, ...)
{
	int ans;
	va_list ap;

	if (buf == NULL || fmt == NULL) {
		_EXT1_FAIL("sscanf_s: bad argument");
		return -1;
	}
	va_start(ap, fmt);
	ans = _Scanf(&scin, (void *)&buf, fmt, ap, 1);
	va_end(ap);
	return ans;
}
