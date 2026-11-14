/* putwc function */
#include <wchar.h>
#include "xstdio.h"

wint_t (putwc)(wchar_t c, FILE *str)
{
	return fputwc(c, str);
}
