/* strtok_s function */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

char *strtok_s(char *__restrict s1, rsize_t *__restrict s1max, const char *__restrict s2,
	       char **__restrict ptr)
{
	rsize_t max = *s1max;	/* SCE: read before s1max is checked */
	char *sbegin, *send;
	const char *p;
	rsize_t m;

	if (s1max == NULL || RSIZE_MAX < max || s2 == NULL || ptr == NULL) {
		_EXT1_FAIL("strtok_s: bad argument");
		return NULL;
	}

	/* skip the leading delimiters */
	sbegin = s1 != NULL ? s1 : *ptr;
	for (send = sbegin, m = max; m != 0 && *send != '\0'; --m, ++send) {
		for (p = s2; *p != '\0' && *p != *send; ++p)
			;
		if (*p == '\0')
			break;
	}
	if (m == 0) {
		_EXT1_FAIL("strtok_s: missing token");
		return NULL;
	}
	*s1max = max -= send - sbegin;
	if (*send == '\0')
		return NULL;

	/* find the end of the token */
	for (sbegin = send, m = max; m != 0 && *send != '\0'; --m, ++send) {
		for (p = s2; *p != '\0' && *p != *send; ++p)
			;
		if (*p != '\0')
			break;
	}
	if (m == 0) {
		_EXT1_FAIL("strtok_s: missing delimiter");
		return NULL;
	}
	*s1max = max - (send - sbegin);
	if (*send != '\0') {
		*send++ = '\0';
		if (*s1max != 0)
			--*s1max;
	}
	*ptr = send;
	return sbegin;
}
