/* mbstowcs_s function */
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <wchar.h>

#include <xwchar.h>

errno_t mbstowcs_s(size_t *__restrict retval, wchar_t *__restrict dst, rsize_t dstmax,
		   const char *__restrict src, rsize_t len)
{
	mbstate_t mbst = {0};
	wchar_t *d;
	size_t n;
	int i;

	if (retval == NULL)
		;
	else if (src == NULL || (dst == NULL && dstmax != 0)
		 || (dst != NULL && (dstmax == 0 || RSIZE_MAX < dstmax || RSIZE_MAX < len)))
		*retval = (size_t)-1;
	else {
		for (d = dst, n = 0; len != 0; len -= i, src += i, ++d, ++n) {
			wchar_t wc;

			if ((i = _Mbtowc(&wc, src, len, &mbst)) < 0) {
				*retval = (size_t)-1;
				return EILSEQ;
			}
			if ((dst != NULL && dstmax != 0 && n < dstmax)
			    || (dst == NULL && dstmax == 0)) {
				if (dst != NULL)
					*d = wc;
				if (wc == L'\0') {
					*retval = n;
					return 0;
				}
			} else if (dst != NULL) {
				*dst = L'\0';
				*retval = (size_t)-1;
				return _EXT1_FAIL("mbstowcs_s: buffer overflow");
			}
		}
		if (n < dstmax) {
			*retval = n;
			if (dst != NULL)
				dst[n] = L'\0';
		}
		return 0;
	}
	if (dst != NULL && 0 < dstmax && dstmax <= RSIZE_MAX)
		*dst = L'\0';
	return _EXT1_FAIL("mbstowcs_s: bad argument");
}
