/* wctob function */
#include <wchar.h>

int wctob(wint_t wc)
{
	return _Wctob(wc);
}
