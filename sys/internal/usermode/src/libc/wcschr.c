/* wcschr function */
#include <wchar.h>

wchar_t *wcschr(const wchar_t *s, wchar_t c)
{
	for (; *s != c; ++s)
		if (*s == L'\0')
			return NULL;
	return (wchar_t *)s;
}
