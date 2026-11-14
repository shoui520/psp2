/* wmemcpy_s function */
#include <stdint.h>
#include <stdlib.h>
#include <wchar.h>

errno_t wmemcpy_s(wchar_t *__restrict s1, rsize_t s1max, const wchar_t *__restrict s2,
		  rsize_t n)
{
	wchar_t *su1 = s1;
	const wchar_t *su2 = s2;

	if (s1 == NULL || RSIZE_MAX < s1max)
		return _EXT1_FAIL("wmemcpy_s: bad destination argument");
	if (s2 == NULL || RSIZE_MAX < n || s1max < n
	    || (su1 < su2 + n && su2 < su1 + s1max)) {
		wmemset(s1, 0, s1max);
		return _EXT1_FAIL("wmemcpy_s: bad argument");
	}
	for (; n != 0; ++su1, ++su2, --n)
		*su1 = *su2;
	return 0;
}
