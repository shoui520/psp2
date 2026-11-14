/* strcat_s function */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

errno_t strcat_s(char *__restrict s1, rsize_t s1max, const char *__restrict s2)
{
	size_t n1, n2;

	if (s1 == NULL || s1max == 0 || RSIZE_MAX < s1max)
		return _EXT1_FAIL("strcat_s: bad destination argument");
	if (s2 == NULL || s1max <= (n1 = strnlen_s(s1, s1max))
	    || s1max <= n1 + (n2 = strnlen_s(s2, s1max - n1))
	    || (s1 < s2 + n2 && s2 < s1 + n1)) {
		*s1 = '\0';
		return _EXT1_FAIL("strcat_s: bad argument");
	}
	for (s1 += n1; n2 != 0; --n2)
		*s1++ = *s2++;
	*s1 = '\0';
	return 0;
}
