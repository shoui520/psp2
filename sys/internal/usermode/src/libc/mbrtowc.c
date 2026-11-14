/* mbrtowc function */
#include <wchar.h>

#include "libc_internal.h"
#include <xwchar.h>

size_t mbrtowc(wchar_t *__restrict pwc, const char *__restrict s, size_t n,
	       mbstate_t *__restrict pst)
{
	if (pst == NULL)
		pst = _Getmbrtowcstate();
	return s != NULL ? _Mbtowc(pwc, s, n, pst) : _Mbtowc(NULL, "", n, pst);
}
