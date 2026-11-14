/* wcsrtombs function */
#include <string.h>
#include <wchar.h>
#include <xwchar.h>

#include "libc_internal.h"

size_t wcsrtombs(char *__restrict s, const wchar_t **__restrict pwcs, size_t n,
		 mbstate_t *__restrict pst)
{
	char buf[_MBMAX];
	int i;
	size_t nc = 0;
	const wchar_t *wcs = *pwcs;

	if (pst == NULL)
		pst = _Getwcsrtombsstate();
	if (s == NULL)
		for (;; nc += i, ++wcs) {	/* translate but don't store */
			if ((i = _Wctomb(buf, *wcs, pst)) < 0)
				return (size_t)-1;
			if (0 < i && buf[i - 1] == '\0')
				return nc + i - 1;
		}
	for (; 0 < n; nc += i, ++wcs, s += i, n -= i) {	/* translate and store */
		char *t;
		mbstate_t pstsave;

		if (n < _MBMAX) {
			t = buf;
			pstsave = *pst;
		} else {
			t = s;
		}
		if ((i = _Wctomb(t, *wcs, pst)) < 0) {	/* an invalid sequence */
			nc = (size_t)-1;
			break;
		}
		if (s == t) {
			;
		} else if (n < (size_t)i) {	/* won't all fit */
			*pst = pstsave;
			break;
		} else {
			memcpy(s, buf, i);
		}
		if (0 < i && s[i - 1] == '\0') {	/* the terminating null */
			*pwcs = NULL;
			return nc + i - 1;
		}
	}
	*pwcs = wcs;
	return nc;
}
