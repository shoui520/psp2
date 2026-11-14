/* wcscpy_s function */
#include <stdint.h>
#include <stdlib.h>
#include <wchar.h>

errno_t wcscpy_s(wchar_t *__restrict s1, rsize_t s1max, const wchar_t *__restrict s2)
{
	size_t n;

	if (s1 == NULL || s1max == 0 || RSIZE_MAX < s1max)
		return _EXT1_FAIL("wcscpy_s: bad destination argument");
	if (s2 == NULL || s1max <= (n = wcslen(s2))
	    || (s1 < s2 + n && s2 < s1 + s1max)) {
		*s1 = L'\0';
		return _EXT1_FAIL("wcscpy_s: bad argument");
	}
	for (; n != 0; --n)
		*s1++ = *s2++;
	*s1 = L'\0';
	return 0;
}
