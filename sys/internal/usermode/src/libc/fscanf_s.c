/* fscanf_s function */
#include <stdlib.h>
#include "xstdio.h"

static int scin(void *str, int ch, int getfl)
{
	if (getfl)
		return fgetc((FILE *)str);
	return ungetc(ch, (FILE *)str);
}

int fscanf_s(FILE *__restrict str, const char *__restrict fmt, ...)
{
	int ans;
	va_list ap;

	if (str == NULL || fmt == NULL) {
		_EXT1_FAIL("fscanf_s: bad argument");
		return -1;
	}
	va_start(ap, fmt);
	_Lockfilelock(str);
	ans = _Scanf(&scin, str, fmt, ap, 1);
	_Unlockfilelock(str);
	va_end(ap);
	return ans;
}
