/*
 * _Iswctype function: classify a wide character through a class table.
 */
#include <wctype.h>

#include "xwctype.h"

int _Iswctype(wint_t c, wctype_t category)
{
	const unsigned short *p = (const unsigned short *)_Wctype[0]._Name + _Wctype[category]._Off;
	const unsigned short *pend = (const unsigned short *)_Wctype[0]._Name + _Wctype[category + 1]._Off;

	for (; p < pend; p += 3)	/* ranges of {first, last, class bits} */
		if (p[0] <= c && c <= p[1])
			return p[2];
	return 0;
}
