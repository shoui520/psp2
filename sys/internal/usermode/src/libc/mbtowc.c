/* mbtowc function */
#include <stdlib.h>
#include <wchar.h>

#include "libc_internal.h"
#include <xwchar.h>

int mbtowc(wchar_t *__restrict pwc, const char *__restrict s, size_t n)
{
	mbstate_t *mbst = _Getmbtowcstate();
	int ans = _Mbtowc(pwc, s, _MBMAX < n ? _MBMAX : n, mbst);

	return ans < 0 ? -1 : ans;
}
