/* vfwscanf function */
#include <wchar.h>

#include "xstdio.h"

static unsigned int scin(void *str, unsigned int ch, int getfl)
{
	if (getfl)
		return fgetwc((FILE *)str);
	return ungetwc(ch, (FILE *)str);
}

int vfwscanf(FILE *__restrict str, const wchar_t *__restrict fmt, va_list ap)
{
	int ans;

	_Lockfilelock(str);
	ans = _WScanf(&scin, str, fmt, ap, 0);
	_Unlockfilelock(str);
	return ans;
}
