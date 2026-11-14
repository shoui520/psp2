/* strerror_s function */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "libc_internal.h"

errno_t strerror_s(char *s, rsize_t maxsize, errno_t errnum)
{
	const char *msg;
	size_t n;

	if (s == NULL || maxsize == 0 || RSIZE_MAX < maxsize)
		return _EXT1_FAIL("strerror_s: bad argument");
	msg = _Strerror(errnum, NULL);
	n = strlen(msg) + 1;
	if (maxsize < n) {	/* truncate, ending in "..." */
		memcpy(s, msg, maxsize - 1);
		s[maxsize - 1] = '\0';
		if (3 < maxsize)
			memcpy(&s[maxsize - 4], "...", 3);
	} else {
		strncpy(s, msg, n);
	}
	return 0;
}
