/* wctomb_s function */
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#include <xwchar.h>
#include "libc_internal.h"

errno_t wctomb_s(int *__restrict status, char *__restrict s, rsize_t smax, wchar_t wc)
{
	char buf[MB_LEN_MAX];
	int n;

	if (status == NULL || (s == NULL && smax != 0) || (s != NULL && RSIZE_MAX < smax))
		return _EXT1_FAIL("wctomb_s: bad argument");
	if (s != NULL && smax < MB_LEN_MAX) {	/* convert into a buffer, then check the room */
		n = _Wctomb(buf, wc, _Getwctomb_sstate());
		if (0 < n) {
			if (smax < (size_t)n)
				return _EXT1_FAIL("wctomb_s: buffer too small");
			memcpy(s, buf, n);
		}
	} else {
		n = _Wctomb(s, wc, _Getwctomb_sstate());
	}
	*status = n;
	return 0;
}
