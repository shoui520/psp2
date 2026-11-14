/* wmemcpy function */
#include <wchar.h>

wchar_t *wmemcpy(wchar_t *__restrict s1, const wchar_t *__restrict s2, size_t n)
{
	wchar_t *su1 = s1;

	for (; 0 < n; ++su1, ++s2, --n)
		*su1 = *s2;
	return s1;
}
