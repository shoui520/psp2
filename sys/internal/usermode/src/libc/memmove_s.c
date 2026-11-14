/* memmove_s function */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

errno_t memmove_s(void *s1, rsize_t s1max, const void *s2, rsize_t n)
{
	char *sc1 = (char *)s1;
	const char *sc2 = (const char *)s2;

	if (s1 == NULL || RSIZE_MAX < s1max)
		return _EXT1_FAIL("memmove_s: bad destination argument");
	if (s2 == NULL || RSIZE_MAX < n || s1max < n) {
		memset(s1, 0, s1max);
		return _EXT1_FAIL("memmove_s: bad argument");
	}
	if (sc2 < sc1 && sc1 < sc2 + n)
		for (sc1 += n, sc2 += n; n != 0; --n)
			*--sc1 = *--sc2;	/* copy backwards */
	else
		for (; n != 0; --n)
			*sc1++ = *sc2++;	/* copy forwards */
	return 0;
}
