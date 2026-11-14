/* wcscpy function */
#include <wchar.h>

wchar_t *wcscpy(wchar_t *__restrict s1, const wchar_t *__restrict s2)
{
	wchar_t *s = s1;

	for (; (*s++ = *s2++) != L'\0';)
		;
	return s1;
}
