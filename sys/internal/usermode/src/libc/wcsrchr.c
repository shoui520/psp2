/* wcsrchr function */
#include <wchar.h>

wchar_t *wcsrchr(const wchar_t *s, wchar_t c)
{
	const wchar_t *sc;

	for (sc = NULL;; ++s) {	/* check another wide character */
		if (*s == c)
			sc = s;
		if (*s == L'\0')
			return (wchar_t *)sc;
	}
}
