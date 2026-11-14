/* mbsrtowcs_s function */
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <wchar.h>

#include <xwchar.h>

errno_t mbsrtowcs_s(size_t *__restrict retval, wchar_t *__restrict dst, rsize_t dstmax,
		    const char **__restrict src, rsize_t len, mbstate_t *__restrict ps)
{
	const char *s;
	size_t n;
	int i;

	if (retval == NULL || src == NULL || (s = *src) == NULL || ps == NULL
	    || (dst == NULL && dstmax != 0)
	    || (dst != NULL && (dstmax == 0 || RSIZE_MAX < dstmax || RSIZE_MAX < len))) {
		if (dst != NULL && 0 < dstmax && dstmax <= RSIZE_MAX)
			*dst = L'\0';
		if (retval != NULL)
			*retval = (size_t)-1;
		return _EXT1_FAIL("mbsrtowcs_s: bad argument");
	}
	n = 0;
	if (dst == NULL) {
		wchar_t wc;

		for (;; ++n, s += i) {
			if ((i = _Mbtowc(&wc, s, INT_MAX, ps)) < 0) {
				*retval = (size_t)-1;
				return EILSEQ;
			}
			if (i == 0 && wc == L'\0')
				break;
		}
		*retval = n;
		return 0;
	} else {
		wchar_t *d = dst;

		for (; len != 0 && dstmax != 0; --len, ++n, s += i, ++d, --dstmax) {
			if ((i = _Mbtowc(d, s, INT_MAX, ps)) < 0) {
				*src = s;
				*d = L'\0';
				*retval = (size_t)-1;
				return EILSEQ;
			}
			if (i == 0 && *d == L'\0') {
				*src = NULL;
				*retval = n;
				return 0;
			}
		}
		*src = s;
		if (dstmax != 0) {
			*d = L'\0';
			*retval = n;
			return 0;
		}
		*dst = L'\0';
		*retval = (size_t)-1;
		return _EXT1_FAIL("mbsrtowcs_s: buffer too small");
	}
}
