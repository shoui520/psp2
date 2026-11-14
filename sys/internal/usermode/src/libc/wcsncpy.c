/* wcsncpy function */
#include <wchar.h>

wchar_t *wcsncpy(wchar_t *__restrict s1, const wchar_t *__restrict s2, size_t n)
{
	wchar_t *s;

	for (s = s1; 0 < n && *s2 != L'\0'; --n)
		*s++ = *s2++;	/* copy at most n */
	for (; 0 < n; --n)
		*s++ = L'\0';	/* pad */
	return s1;
}
