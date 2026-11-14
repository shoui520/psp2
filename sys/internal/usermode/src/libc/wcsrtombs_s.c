/* wcsrtombs_s function */
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#include <xwchar.h>

errno_t wcsrtombs_s(size_t *__restrict retval, char *__restrict dst, rsize_t dstmax,
		    const wchar_t **__restrict src, rsize_t len, mbstate_t *__restrict ps)
{
	char buf[MB_LEN_MAX];
	const wchar_t *wcs;
	size_t n;
	int i;

	if (retval == NULL || src == NULL || (wcs = *src) == NULL || ps == NULL
	    || (dst == NULL && dstmax != 0)
	    || (dst != NULL && (dstmax == 0 || RSIZE_MAX < dstmax || RSIZE_MAX < len))) {
		if (dst != NULL && 0 < dstmax && dstmax <= RSIZE_MAX)
			*dst = '\0';
		if (retval != NULL)
			*retval = (size_t)-1;
		return _EXT1_FAIL("wcsrtombs_s: bad argument");
	}
	n = 0;
	if (dst == NULL) {
		for (;; ++wcs) {
			if ((i = _Wctomb(buf, *wcs, ps)) < 0) {
				*retval = (size_t)-1;
				return EILSEQ;
			}
			n += i;
			if (0 < i && buf[i - 1] == '\0')
				break;
		}
		*retval = n - 1;
		return 0;
	} else {
		char *s = dst;

		for (; len != 0 && dstmax != 0; len -= i, dstmax -= i, n += i, ++wcs, s += i) {
			mbstate_t mbst = *ps;
			char *t = len < MB_LEN_MAX || dstmax < MB_LEN_MAX ? buf : s;

			if ((i = _Wctomb(t, *wcs, ps)) < 0) {
				*retval = (size_t)-1;
				return EILSEQ;
			}
			if (t != s) {
				if (len < (size_t)i || dstmax < (size_t)i) {
					*ps = mbst;	/* it does not fit: undo it */
					dstmax = 0;
					break;
				}
				memcpy(s, buf, i);
			}
			if (0 < i && s[i - 1] == '\0') {
				*src = NULL;
				*retval = n + i - 1;
				return 0;
			}
		}
		*src = wcs;
		if (dstmax != 0) {
			*dst = '\0';	/* SCE: terminates at dst, not at the end */
			*retval = n;
			return 0;
		}
		*s = '\0';
		*retval = (size_t)-1;
		return _EXT1_FAIL("wcstombs_s: buffer too small");	/* SCE: wcstombs_s's message */
	}
}
