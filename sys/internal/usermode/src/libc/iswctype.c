/* iswctype function */
#include <wctype.h>

int (iswctype)(wint_t wc, wctype_t desc)
{
	return _Iswctype(wc, desc);
}
