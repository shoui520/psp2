/* sscanf function */
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

int sscanf(const char *__restrict buf, const char *__restrict fmt, ...)
{
	int ans;
	va_list ap;

	va_start(ap, fmt);
	ans = _Scanf(&scin, (void *)&buf, fmt, ap, 0);
	va_end(ap);
	return ans;
}
