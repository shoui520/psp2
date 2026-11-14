/* wcscat function */
#include <wchar.h>

wchar_t *wcscat(wchar_t *__restrict s1, const wchar_t *__restrict s2)
{
	wchar_t *s;

	for (s = s1; *s != L'\0'; ++s)
		;
	for (; (*s = *s2) != L'\0'; ++s, ++s2)
		;
	return s1;
}
