/* strncmp function */
#include <string.h>

int strncmp(const char *s1, const char *s2, size_t n)
{
	for (; 0 < n; ++s1, ++s2, --n)
		if (*s1 != *s2)
			return *(const unsigned char *)s1 < *(const unsigned char *)s2 ? -1 : +1;
		else if (*s1 == '\0')
			return 0;
	return 0;
}
