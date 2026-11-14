/* strstr function */
#include <string.h>

char *strstr(const char *s1, const char *s2)
{
	size_t n = strlen(s2);

	if (n == 0)
		return (char *)s1;
	for (; (s1 = strchr(s1, *(const unsigned char *)s2)) != NULL; ++s1)
		if (memcmp(s1 + 1, s2 + 1, n - 1) == 0)
			return (char *)s1;
	return NULL;
}
