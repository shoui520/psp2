/* vfscanf_s function */
#include <stdlib.h>
#include "xstdio.h"

static int scin(void *str, int ch, int getfl)
{
	if (getfl)
		return fgetc((FILE *)str);
	return ungetc(ch, (FILE *)str);
}

int vfscanf_s(FILE *__restrict str, const char *__restrict fmt, va_list ap)
{
	int ans;

	if (str == NULL || fmt == NULL) {
		_EXT1_FAIL("vfscanf_s: bad argument");
		return -1;
	}
	_Lockfilelock(str);
	ans = _Scanf(&scin, str, fmt, ap, 1);
	_Unlockfilelock(str);
	return ans;
}
