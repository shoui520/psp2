/* vfwscanf_s function */
#include <stdlib.h>
#include <wchar.h>
#include "xstdio.h"

static unsigned int scin(void *str, unsigned int ch, int getfl)
{
	if (getfl)
		return fgetwc((FILE *)str);
	return ungetwc(ch, (FILE *)str);
}

int vfwscanf_s(FILE *__restrict str, const wchar_t *__restrict fmt, va_list ap)
{
	int ans;

	if (str == NULL || fmt == NULL) {
		_EXT1_FAIL("vfwscanf_s: bad argument");
		return -1;
	}
	_Lockfilelock(str);
	ans = _WScanf(&scin, str, fmt, ap, 1);
	_Unlockfilelock(str);
	return ans;
}
