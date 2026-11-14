/* gets_s function */
#include <stdint.h>
#include <stdlib.h>

#include "xstdio.h"

char *gets_s(char *buf, rsize_t n)
{
	char *s = buf;
	int ch;

	if (n == 0) {
		_EXT1_FAIL("gets_s: zero buffer size");
		return NULL;
	}
	if (buf == NULL || RSIZE_MAX < n) {
		*buf = '\0';
		_EXT1_FAIL("gets_s: bad argument");
		return NULL;
	}
	for (--n; (ch = getchar()) != EOF && ch != '\n' && n != 0; --n)
		*s++ = (char)ch;
	if (ch == '\n' || (ch == EOF && s != buf)) {
		*s = '\0';
		return buf;
	}
	if (ch != EOF)
		_EXT1_FAIL("gets_s: short buffer");
	for (; ch != EOF && ch != '\n'; ch = getchar())
		;
	*buf = '\0';
	return NULL;
}
