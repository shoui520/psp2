/* mbrlen function */
#include <wchar.h>

#include "libc_internal.h"
#include <xwchar.h>

size_t mbrlen(const char *__restrict s, size_t n, mbstate_t *__restrict pst)
{
	mbstate_t *mbst = _Getmbrlenstate();

	return _Mbtowc(NULL, s, n, pst != NULL ? pst : mbst);
}
