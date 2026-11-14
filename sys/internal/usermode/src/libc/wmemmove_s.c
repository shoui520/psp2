/* wmemmove_s function */
#include <stdint.h>
#include <stdlib.h>
#include <wchar.h>

errno_t wmemmove_s(wchar_t *s1, rsize_t s1max, const wchar_t *s2, rsize_t n)
{
	wchar_t *sc1 = s1;
	const wchar_t *sc2 = s2;

	if (s1 == NULL || RSIZE_MAX < s1max)
		return _EXT1_FAIL("wmemmove_s: bad destination argument");
	if (s2 == NULL || RSIZE_MAX < n || s1max < n) {
		wmemset(s1, 0, s1max);
		return _EXT1_FAIL("wmemmove_s: bad argument");
	}
	if (sc2 < sc1 && sc1 < sc2 + n)
		for (sc1 += n, sc2 += n; n != 0; --n)
			*--sc1 = *--sc2;	/* copy backwards */
	else
		for (; n != 0; --n)
			*sc1++ = *sc2++;	/* copy forwards */
	return 0;
}
