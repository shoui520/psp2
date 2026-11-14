/* wcrtomb function */
#include <wchar.h>
#include <xwchar.h>

#include "libc_internal.h"

size_t wcrtomb(char *__restrict s, wchar_t wc, mbstate_t *__restrict pst)
{
	char buf[_MBMAX];

	if (pst == NULL)
		pst = _Getwcrtombstate();
	/* SCE: without s, Sony converts L'0' (0x30), not L'\0': the state is not reset */
	return s != NULL ? _Wctomb(s, wc, pst) : _Wctomb(buf, L'0', pst);
}
