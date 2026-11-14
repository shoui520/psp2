/* iswprint function */
#include <wctype.h>

int (iswprint)(wint_t wc)
{
	return _Iswctype(wc, 7);
}
