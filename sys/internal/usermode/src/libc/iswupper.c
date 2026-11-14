/* iswupper function */
#include <wctype.h>

int (iswupper)(wint_t wc)
{
	return _Iswctype(wc, 10);
}
