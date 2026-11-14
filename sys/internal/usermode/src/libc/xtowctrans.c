/*
 * _Towctrans function: map a wide character through a mapping table.
 */
#include <wctype.h>

#include "xwctype.h"

wint_t _Towctrans(wint_t c, wctrans_t category)
{
	const short *p = (const short *)_Wctrans[0]._Name + _Wctrans[category]._Off;
	const short *pend = (const short *)_Wctrans[0]._Name + _Wctrans[category + 1]._Off;

	for (; p < pend; p += 3)	/* ranges of {first, last, delta} */
		if ((unsigned short)p[0] <= c && c <= (unsigned short)p[1])
			return c + p[2];
	return c;
}
