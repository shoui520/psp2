/* vwscanf function */
#include <wchar.h>

#include "xstdio.h"

static unsigned int scin(void *str, unsigned int ch, int getfl)
{
	if (getfl)
		return fgetwc((FILE *)str);
	return ungetwc(ch, (FILE *)str);
}

int vwscanf(const wchar_t *__restrict fmt, va_list ap)
{
	int ans;

	_Lockfilelock(stdin);
	ans = _WScanf(&scin, stdin, fmt, ap, 0);
	_Unlockfilelock(stdin);
	return ans;
}
