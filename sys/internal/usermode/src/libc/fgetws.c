/* fgetws function */
#include <wchar.h>
#include "xstdio.h"

wchar_t *fgetws(wchar_t *__restrict buf, int n, FILE *__restrict str)
{
	wchar_t *s = buf;

	if (n <= 1)
		return NULL;
	_Lockfilelock(str);
	for (--n; 0 < n; --n) {
		wint_t wc = fgetwc(str);

		if (wc == WEOF)
			break;
		*s++ = wc;
		if (wc == L'\n')
			break;
	}
	if (s == buf)
		buf = NULL;
	else
		*s = L'\0';
	_Unlockfilelock(str);
	return buf;
}
