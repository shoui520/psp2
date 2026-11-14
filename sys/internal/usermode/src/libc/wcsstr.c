/* wcsstr function */
#include <wchar.h>

wchar_t *wcsstr(const wchar_t *s1, const wchar_t *s2)
{
	if (*s2 == L'\0')
		return (wchar_t *)s1;
	for (; (s1 = wcschr(s1, *s2)) != NULL; ++s1) {	/* match rest of prefix */
		const wchar_t *sc1, *sc2;

		for (sc1 = s1, sc2 = s2;;)
			if (*++sc2 == L'\0')
				return (wchar_t *)s1;
			else if (*++sc1 != *sc2)
				break;
	}
	return NULL;
}
