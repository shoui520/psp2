/* strnlen_s function */
#include <string.h>

size_t strnlen_s(const char *s, size_t maxsize)
{
	const char *sc;

	if (s == NULL)
		return 0;
	for (sc = s; maxsize != 0 && *sc != '\0'; --maxsize, ++sc)
		;
	return sc - s;
}
