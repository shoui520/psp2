/* mbstowcs function */
#include <limits.h>
#include <stdlib.h>
#include <wchar.h>

#include <xwchar.h>

size_t mbstowcs(wchar_t *__restrict wcs, const char *__restrict s, size_t n)
{
	int i;
	wchar_t *pwc;
	size_t nwc;
	wchar_t wc = 0;
	_Mbstinit(mbst);

	for (pwc = wcs, nwc = 0; nwc < n || wcs == NULL; ++nwc, ++pwc, s += i) {
		if ((i = _Mbtowc(&wc, s, INT_MAX, &mbst)) < 0)
			return (size_t)-1;
		if (wcs != NULL)
			*pwc = wc;
		if (wc == L'\0')
			break;
	}
	return nwc;
}
