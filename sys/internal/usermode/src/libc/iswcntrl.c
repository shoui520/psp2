/* iswcntrl function */
#include <wctype.h>

int (iswcntrl)(wint_t wc)
{
	return _Iswctype(wc, 3);
}
