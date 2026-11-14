/* wcrtomb_s function */
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#include <xwchar.h>

errno_t wcrtomb_s(size_t *__restrict retval, char *__restrict s, rsize_t smax, wchar_t wc,
		  mbstate_t *__restrict ps)
{
	char buf[MB_LEN_MAX];
	int n;

	if (retval == NULL || ps == NULL || (s == NULL && smax != 0)
	    || (s != NULL && (smax == 0 || RSIZE_MAX < smax))) {
		if (s != NULL && 0 < smax && smax <= RSIZE_MAX)
			*s = '\0';
		if (retval != NULL)
			*retval = (size_t)-1;
		return _EXT1_FAIL("wcrtomb_s: bad argument");
	}
	if (s == NULL) {
		n = _Wctomb(buf, L'\0', ps);
	} else if (smax < MB_LEN_MAX) {	/* convert into a buffer, then check the room */
		n = _Wctomb(buf, wc, ps);
		if (0 < n) {
			if (smax < (size_t)n) {
				*retval = (size_t)-1;
				return _EXT1_FAIL("wcrtomb_s: buffer too small");
			}
			memcpy(s, buf, n);
		}
	} else {
		n = _Wctomb(s, wc, ps);
	}
	if (n < 0) {
		*retval = (size_t)-1;
		return EILSEQ;
	}
	*retval = n;
	return 0;
}
