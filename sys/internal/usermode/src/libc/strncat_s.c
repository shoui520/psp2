/* strncat_s function */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

errno_t strncat_s(char *__restrict s1, rsize_t s1max, const char *__restrict s2, rsize_t n)
{
	size_t n1, n2;

	if (s1 == NULL || s1max == 0 || RSIZE_MAX < s1max)
		return _EXT1_FAIL("strncat_s: bad destination argument");
	if (s2 == NULL || RSIZE_MAX < n
	    || (n1 = strnlen_s(s1, s1max), n2 = strnlen_s(s2, n), s1max <= n1 + n2)
	    || (s1 < s2 + n2 && s2 < s1 + n1)) {
		*s1 = '\0';
		return _EXT1_FAIL("strncat_s: bad argument");
	}
	for (s1 += n1; n2 != 0; --n2)
		*s1++ = *s2++;
	*s1 = '\0';
	return 0;
}
