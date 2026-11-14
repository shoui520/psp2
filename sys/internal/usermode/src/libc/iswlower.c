/* iswlower function */
#include <wctype.h>

int (iswlower)(wint_t wc)
{
	return _Iswctype(wc, 6);
}
