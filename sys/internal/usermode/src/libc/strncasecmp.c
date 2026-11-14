/* strncasecmp function */
#include <ctype.h>
#include <string.h>

int strncasecmp(const char *s1, const char *s2, size_t n)
{
	const unsigned char *su1 = (const unsigned char *)s1;
	const unsigned char *su2 = (const unsigned char *)s2;

	for (; 0 < n; ++su1, ++su2, --n)
		if (_CTolower(*su1) != _CTolower(*su2))
			return (unsigned char)_CTolower(*su1) < (unsigned char)_CTolower(*su2) ? -1 : +1;
		else if (*su1 == '\0')
			return 0;
	return 0;
}
