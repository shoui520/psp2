/* ungetwc function */
#include <wchar.h>

#include "xstdio.h"

wint_t ungetwc(wint_t c, FILE *str)
{
	_Lockfilelock(str);
	if (c == WEOF || str->_WRback <= str->_WBack
	    || (str->_Mode & (_MOPENR | _MWRITE | _MBYTE)) != _MOPENR) {
		c = WEOF;
	} else {	/* deliver the pushback */
		str->_Mode = str->_Mode & ~_MEOF | _MREAD | _MWIDE;
		*--str->_WRback = (wchar_t)c;
	}
	_Unlockfilelock(str);
	return c;
}
