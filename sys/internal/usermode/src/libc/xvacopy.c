/*
 * _Vacopy function: copy a va_list.
 */
#include <stdarg.h>

#include "xstdio.h"

void _Vacopy(va_list *pap, va_list ap)
{
	*pap = ap;
}
