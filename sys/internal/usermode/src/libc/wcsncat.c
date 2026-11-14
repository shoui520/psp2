/* wcsncat function */
#include <wchar.h>

wchar_t *wcsncat(wchar_t *__restrict s1, const wchar_t *__restrict s2, size_t n)
{
	wchar_t *s;

	for (s = s1; *s != L'\0'; ++s)
		;
	for (; 0 < n && *s2 != L'\0'; --n)
		*s++ = *s2++;
	*s = L'\0';
	return s1;
}
