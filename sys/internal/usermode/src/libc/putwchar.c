/* putwchar function */
#include <wchar.h>
#include "xstdio.h"

wint_t (putwchar)(wchar_t c)
{
	return fputwc(c, stdout);
}
