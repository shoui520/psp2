/* iswdigit function */
#include <wctype.h>

int (iswdigit)(wint_t wc)
{
	return _Iswctype(wc, 4);
}
