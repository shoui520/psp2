/* iswalpha function */
#include <wctype.h>

int (iswalpha)(wint_t wc)
{
	return _Iswctype(wc, 2);
}
