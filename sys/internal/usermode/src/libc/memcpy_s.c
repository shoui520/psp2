/* memcpy_s function */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

errno_t memcpy_s(void *__restrict s1, rsize_t s1max, const void *__restrict s2, rsize_t n)
{
	char *su1 = (char *)s1;
	const char *su2 = (const char *)s2;

	if (s1 == NULL || RSIZE_MAX < s1max)
		return _EXT1_FAIL("memcpy_s: bad destination argument");
	if (s2 == NULL || RSIZE_MAX < n || s1max < n
	    || (su1 < su2 + n && su2 < su1 + s1max)) {
		memset(s1, 0, s1max);
		return _EXT1_FAIL("memcpy_s: bad argument");
	}
	for (; n != 0; ++su1, ++su2, --n)
		*su1 = *su2;
	return 0;
}
