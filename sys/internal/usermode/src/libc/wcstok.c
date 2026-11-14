/* wcstok function */
#include <wchar.h>

wchar_t *wcstok(wchar_t *__restrict s1, const wchar_t *__restrict s2, wchar_t **__restrict ptr)
{
	wchar_t *sbegin, *send;

	sbegin = s1 != NULL ? s1 : *ptr;
	sbegin += wcsspn(sbegin, s2);
	if (*sbegin == L'\0') {	/* end of scan */
		*ptr = L"";
		return NULL;
	}
	send = sbegin + wcscspn(sbegin, s2);
	if (*send != L'\0')
		*send++ = L'\0';
	*ptr = send;
	return sbegin;
}
