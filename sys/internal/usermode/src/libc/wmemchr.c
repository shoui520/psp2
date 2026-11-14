/* wmemchr function */
#include <wchar.h>

wchar_t *wmemchr(const wchar_t *s, wchar_t c, size_t n)
{
	for (; 0 < n; ++s, --n)
		if (*s == c)
			return (wchar_t *)s;
	return NULL;
}
