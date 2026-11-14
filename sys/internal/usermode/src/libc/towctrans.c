/* towctrans function */
#include <wctype.h>

wint_t (towctrans)(wint_t c, wctrans_t category)
{
	return _Towctrans(c, category);
}
