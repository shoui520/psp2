/* wcspbrk function */
#include <wchar.h>

wchar_t *wcspbrk(const wchar_t *s1, const wchar_t *s2)
{
	const wchar_t *sc1, *sc2;

	for (sc1 = s1; *sc1 != L'\0'; ++sc1)
		for (sc2 = s2; *sc2 != L'\0'; ++sc2)
			if (*sc1 == *sc2)
				return (wchar_t *)sc1;
	return NULL;
}
