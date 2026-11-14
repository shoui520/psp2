/* wcslen function */
#include <wchar.h>

size_t wcslen(const wchar_t *s)
{
	const wchar_t *sc;

	for (sc = s; *sc != L'\0'; ++sc)
		;
	return sc - s;
}
