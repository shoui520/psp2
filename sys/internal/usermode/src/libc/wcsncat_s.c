/* wcsncat_s function */
#include <stdint.h>
#include <stdlib.h>
#include <wchar.h>

errno_t wcsncat_s(wchar_t *__restrict s1, rsize_t s1max, const wchar_t *__restrict s2,
		  rsize_t n)
{
	size_t n1, n2;
	wchar_t *d;

	if (s1 == NULL || s1max == 0 || RSIZE_MAX < s1max)
		return _EXT1_FAIL("wcsncat_s: bad destination argument");
	if (s2 == NULL || RSIZE_MAX < n
	    || (n1 = wcsnlen_s(s1, s1max), n2 = wcsnlen_s(s2, n), s1max <= n1 + n2)
	    || (d = s1 + n1, s1 < s2 + n2 && s2 < d)) {
		*s1 = L'\0';
		return _EXT1_FAIL("wcsncat_s: bad argument");
	}
	for (; n2 != 0; --n2)
		*d++ = *s2++;
	*d = L'\0';
	return 0;
}
