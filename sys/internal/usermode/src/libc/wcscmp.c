/* wcscmp function */
#include <wchar.h>

int wcscmp(const wchar_t *s1, const wchar_t *s2)
{
	for (; *s1 == *s2; ++s1, ++s2)
		if (*s1 == L'\0')
			return 0;
	return *s1 < *s2 ? -1 : +1;
}
