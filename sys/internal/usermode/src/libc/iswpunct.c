/* iswpunct function */
#include <wctype.h>

int (iswpunct)(wint_t wc)
{
	return _Iswctype(wc, 8);
}
