/* wmemset function */
#include <wchar.h>

wchar_t *wmemset(wchar_t *s, wchar_t c, size_t n)
{
	wchar_t *su = s;

	for (; 0 < n; ++su, --n)
		*su = c;
	return s;
}
