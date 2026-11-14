/* towupper function */
#include <wctype.h>

wint_t (towupper)(wint_t c)
{
	return _Towctrans(c, 2);
}
