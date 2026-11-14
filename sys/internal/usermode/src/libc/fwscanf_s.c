/* fwscanf_s function */
#include <stdlib.h>
#include <wchar.h>

#include "xstdio.h"

static unsigned int scin(void *str, unsigned int ch, int getfl)
{
	if (getfl)
		return fgetwc((FILE *)str);
	return ungetwc(ch, (FILE *)str);
}

int fwscanf_s(FILE *__restrict str, const wchar_t *__restrict fmt, ...)
{
	int ans;
	va_list ap;

	if (str == NULL || fmt == NULL) {
		_EXT1_FAIL("fwscanf_s: bad argument");
		return -1;
	}
	va_start(ap, fmt);
	_Lockfilelock(str);
	ans = _WScanf(&scin, str, fmt, ap, 1);
	_Unlockfilelock(str);
	va_end(ap);
	return ans;
}
