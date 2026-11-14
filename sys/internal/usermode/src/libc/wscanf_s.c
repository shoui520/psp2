/* wscanf_s function */
#include <stdlib.h>
#include <wchar.h>
#include "xstdio.h"

static unsigned int scin(void *str, unsigned int ch, int getfl)
{
	if (getfl)
		return fgetwc((FILE *)str);
	return ungetwc(ch, (FILE *)str);
}

int wscanf_s(const wchar_t *__restrict fmt, ...)
{
	int ans;
	va_list ap;

	if (fmt == NULL) {
		_EXT1_FAIL("wscanf_s: bad argument");
		return -1;
	}
	va_start(ap, fmt);
	_Lockfilelock(stdin);
	ans = _WScanf(&scin, stdin, fmt, ap, 1);
	_Unlockfilelock(stdin);
	va_end(ap);
	return ans;
}
