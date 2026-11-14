/* wcsncmp function */
#include <wchar.h>

int wcsncmp(const wchar_t *s1, const wchar_t *s2, size_t n)
{
	for (; 0 < n; ++s1, ++s2, --n)
		if (*s1 != *s2)
			return *s1 < *s2 ? -1 : +1;
		else if (*s1 == L'\0')
			return 0;
	return 0;
}
