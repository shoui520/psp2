/* iswblank function */
#include <wctype.h>

int (iswblank)(wint_t wc)
{
	return _Iswctype(wc, 12);
}
