/* towlower function */
#include <wctype.h>

wint_t (towlower)(wint_t c)
{
	return _Towctrans(c, 1);
}
