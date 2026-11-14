/* wcsncpy_s function */
#include <stdint.h>
#include <stdlib.h>
#include <wchar.h>

errno_t wcsncpy_s(wchar_t *__restrict s1, rsize_t s1max, const wchar_t *__restrict s2,
		  rsize_t n)
{
	size_t n2;

	if (s1 == NULL || s1max == 0 || RSIZE_MAX < s1max)
		return _EXT1_FAIL("wcsncpy_s: bad destination argument");
	if (s2 == NULL || RSIZE_MAX < n || s1max <= (n2 = wcsnlen_s(s2, n))
	    || (s1 < s2 + n2 && s2 < s1 + s1max)) {
		*s1 = L'\0';
		return _EXT1_FAIL("wcsncpy_s: bad argument");
	}
	for (; n2 != 0; --n2)
		*s1++ = *s2++;
	*s1 = L'\0';
	return 0;
}
