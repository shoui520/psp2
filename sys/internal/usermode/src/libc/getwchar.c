/* getwchar function */
#include <wchar.h>
#include "xstdio.h"

wint_t (getwchar)(void)
{
	return fgetwc(stdin);
}
