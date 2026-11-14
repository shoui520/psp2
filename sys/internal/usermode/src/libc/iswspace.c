/* iswspace function */
#include <wctype.h>

int (iswspace)(wint_t wc)
{
	return _Iswctype(wc, 9);
}
