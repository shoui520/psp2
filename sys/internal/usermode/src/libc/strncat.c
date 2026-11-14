/* strncat function */
#include <string.h>

char *strncat(char *__restrict s1, const char *__restrict s2, size_t n)
{
	char *s;

	for (s = s1; *s != '\0'; ++s)
		;
	for (; 0 < n && *s2 != '\0'; --n)
		*s++ = *s2++;
	*s = '\0';
	return s1;
}
