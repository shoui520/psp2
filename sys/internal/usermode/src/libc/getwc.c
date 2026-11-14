/* getwc function */
#include <wchar.h>
#include "xstdio.h"

wint_t (getwc)(FILE *str)
{
	return fgetwc(str);
}
