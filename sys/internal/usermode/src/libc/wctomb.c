/* wctomb function */
#include <stdlib.h>
#include <wchar.h>
#include <xwchar.h>

#include "libc_internal.h"

int wctomb(char *s, wchar_t wchar)
{
	return _Wctomb(s, wchar, _Getwctombstate());
}
