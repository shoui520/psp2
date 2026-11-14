/* fputws function */
#include <wchar.h>
#include "xstdio.h"

int fputws(const wchar_t *__restrict s, FILE *__restrict str)
{
	_Lockfilelock(str);
	for (; *s != L'\0'; ++s)
		if (fputwc(*s, str) == WEOF) {
			_Unlockfilelock(str);
			return EOF;
		}
	_Unlockfilelock(str);
	return 0;
}
