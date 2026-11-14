/* wcstok_s function */
#include <stdint.h>
#include <stdlib.h>
#include <wchar.h>

wchar_t *wcstok_s(wchar_t *__restrict s1, rsize_t *__restrict s1max,
		  const wchar_t *__restrict s2, wchar_t **__restrict ptr)
{
	rsize_t max = *s1max;	/* SCE: read before s1max is checked */
	wchar_t *sbegin, *send;
	const wchar_t *p;
	rsize_t m;

	if (s1max == NULL || RSIZE_MAX < max || s2 == NULL || ptr == NULL) {
		_EXT1_FAIL("wcstok_s: bad argument");
		return NULL;
	}

	/* skip the leading delimiters */
	sbegin = s1 != NULL ? s1 : *ptr;
	for (send = sbegin, m = max; m != 0 && *send != L'\0'; --m, ++send) {
		for (p = s2; *p != L'\0' && *p != *send; ++p)
			;
		if (*p == L'\0')
			break;
	}
	if (m == 0) {
		_EXT1_FAIL("wcstok_s: missing token");
		return NULL;
	}
	*s1max = max -= send - sbegin;
	if (*send == L'\0')
		return NULL;

	/* find the end of the token */
	for (sbegin = send, m = max; m != 0 && *send != L'\0'; --m, ++send) {
		for (p = s2; *p != L'\0' && *p != *send; ++p)
			;
		if (*p != L'\0')
			break;
	}
	if (m == 0) {
		_EXT1_FAIL("wcstok_s: missing delimiter");
		return NULL;
	}
	*s1max = max - (send - sbegin);
	if (*send != L'\0') {
		*send++ = L'\0';
		if (*s1max != 0)
			--*s1max;
	}
	*ptr = send;
	return sbegin;
}
