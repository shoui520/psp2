/* iswxdigit function */
#include <wctype.h>

int (iswxdigit)(wint_t wc)
{
	return _Iswctype(wc, 11);
}
