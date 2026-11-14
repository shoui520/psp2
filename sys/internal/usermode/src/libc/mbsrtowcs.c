/* mbsrtowcs function */
#include <limits.h>
#include <wchar.h>

#include "libc_internal.h"
#include <xwchar.h>

size_t mbsrtowcs(wchar_t *__restrict wcs, const char **__restrict ps, size_t n,
		 mbstate_t *__restrict pst)
{
	const char *s = *ps;
	int i;
	size_t nwc = 0;

	if (pst == NULL)
		pst = _Getmbsrtowcsstate();
	if (wcs == NULL) {	/* count the characters */
		wchar_t wc = 0;

		for (;; ++nwc, s += i) {
			if ((i = _Mbtowc(&wc, s, INT_MAX, pst)) < 0)
				return (size_t)-1;
			if (i == 0 && wc == L'\0')
				return nwc;
		}
	}
	*wcs = L'\0';	/* SCE: an empty result when n is 0 */
	for (; 0 < n; ++nwc, s += i, ++wcs, --n) {
		if ((i = _Mbtowc(wcs, s, INT_MAX, pst)) < 0) {
			nwc = (size_t)-1;
			break;
		}
		if (i == 0 && *wcs == L'\0') {
			s = NULL;
			break;
		}
	}
	*ps = s;
	return nwc;
}
