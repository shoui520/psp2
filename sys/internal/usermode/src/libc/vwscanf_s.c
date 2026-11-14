/* vwscanf_s function */
#include <stdlib.h>
#include <wchar.h>
#include "xstdio.h"

static unsigned int scin(void *str, unsigned int ch, int getfl)
{
	if (getfl)
		return fgetwc((FILE *)str);
	return ungetwc(ch, (FILE *)str);
}

int vwscanf_s(const wchar_t *__restrict fmt, va_list ap)
{
	int ans;

	if (fmt == NULL) {
		_EXT1_FAIL("vwscanf_s: bad argument");
		return -1;
	}
	_Lockfilelock(stdin);
	ans = _WScanf(&scin, stdin, fmt, ap, 1);
	_Unlockfilelock(stdin);
	return ans;
}
