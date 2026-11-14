/* iswgraph function */
#include <wctype.h>

int (iswgraph)(wint_t wc)
{
	return _Iswctype(wc, 5);
}
