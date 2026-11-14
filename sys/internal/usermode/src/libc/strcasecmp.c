/* strcasecmp function */
#include <ctype.h>
#include <string.h>

int strcasecmp(const char *s1, const char *s2)
{
	const unsigned char *su1 = (const unsigned char *)s1;
	const unsigned char *su2 = (const unsigned char *)s2;

	for (; _CTolower(*su1) == _CTolower(*su2); ++su1, ++su2)
		if (*su1 == '\0')
			return 0;
	return (unsigned char)_CTolower(*su1) < (unsigned char)_CTolower(*su2) ? -1 : +1;
}
