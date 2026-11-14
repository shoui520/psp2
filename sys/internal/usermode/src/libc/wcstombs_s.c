/* wcstombs_s function */
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#include <xwchar.h>

errno_t wcstombs_s(size_t *__restrict retval, char *__restrict dst, rsize_t dstmax,
		   const wchar_t *__restrict src, rsize_t len)
{
	mbstate_t mbst = {0};
	char buf[MB_LEN_MAX];
	size_t n;
	int i;

	if (retval == NULL)
		;
	else if (src == NULL || (dst == NULL && dstmax != 0)
		 || (dst != NULL && (dstmax == 0 || RSIZE_MAX < dstmax || RSIZE_MAX < len)))
		*retval = (size_t)-1;
	else {
		for (n = 0; len != 0; --len, ++src) {
			size_t room = dstmax - n;

			if (MB_LEN_MAX <= room && dst != NULL) {	/* convert in place */
				if ((i = _Wctomb(&dst[n], *src, &mbst)) < 0) {
					*retval = (size_t)-1;
					return EILSEQ;
				}
				n += i;
				if (0 < i && dst[n - 1] == '\0') {
					*retval = n - 1;
					return 0;
				}
			} else {	/* convert into a buffer, then check the room */
				if ((i = _Wctomb(buf, *src, &mbst)) < 0) {
					*retval = (size_t)-1;
					return EILSEQ;
				}
				if (room < (size_t)i) {
					if (dst != NULL) {
						*dst = '\0';
						*retval = (size_t)-1;
						return _EXT1_FAIL("wcstombs_s: buffer overflow");
					}
					n += i;
				} else if (dst != NULL) {
					memcpy(&dst[n], buf, i);
					n += i;
					if (0 < i && dst[n - 1] == '\0') {
						*retval = n - 1;
						return 0;
					}
				} else {
					n += i;
					if (0 < i && buf[i - 1] == '\0') {
						*retval = n - 1;
						return 0;
					}
				}
			}
		}
		if (n < dstmax) {
			*retval = n;
			if (dst != NULL)
				dst[n] = '\0';
		}
		return 0;
	}
	if (dst != NULL && 0 < dstmax && dstmax <= RSIZE_MAX)
		*dst = '\0';
	return _EXT1_FAIL("wcstombs_s: bad argument");
}
