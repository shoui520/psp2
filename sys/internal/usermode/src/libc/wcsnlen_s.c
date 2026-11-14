/* wcsnlen_s function */
#include <wchar.h>

size_t wcsnlen_s(const wchar_t *s, size_t maxsize)
{
	const wchar_t *sc;

	if (s == NULL)
		return 0;
	for (sc = s; maxsize != 0 && *sc != L'\0'; --maxsize, ++sc)
		;
	return sc - s;
}
