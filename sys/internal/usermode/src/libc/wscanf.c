/* wscanf function */
#include <wchar.h>
#include "xstdio.h"

static unsigned int scin(void *str, unsigned int ch, int getfl)
{
	if (getfl)
		return fgetwc((FILE *)str);
	return ungetwc(ch, (FILE *)str);
}

int wscanf(const wchar_t *__restrict fmt, ...)
{
	int ans;
	va_list ap;

	va_start(ap, fmt);
	_Lockfilelock(stdin);
	ans = _WScanf(&scin, stdin, fmt, ap, 0);
	_Unlockfilelock(stdin);
	va_end(ap);
	return ans;
}
