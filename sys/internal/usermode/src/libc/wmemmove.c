/* wmemmove function */
#include <wchar.h>

wchar_t *wmemmove(wchar_t *s1, const wchar_t *s2, size_t n)
{
	wchar_t *su1 = s1;

	if (s2 < su1 && su1 < s2 + n)	/* copy backwards */
		for (su1 += n, s2 += n; 0 < n; --n)
			*--su1 = *--s2;
	else	/* copy forwards */
		for (; 0 < n; --n)
			*su1++ = *s2++;
	return s1;
}
