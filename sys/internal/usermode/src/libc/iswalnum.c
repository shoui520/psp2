/* iswalnum function */
#include <wctype.h>

int (iswalnum)(wint_t wc)
{
	return _Iswctype(wc, 1);
}
