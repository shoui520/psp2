/*
 * rintl function: x rounded to an integer in the current rounding mode,
 * inexact when that moves x.
 */
#include <math.h>

#include "xmath.h"

long double (rintl)(long double x)
{
	long double y;

	switch (_LDtest(&x)) {
	case _NANCODE:
	case _INFCODE:
	case 0:
		return x;
	default:	/* finite */
		y = _LRint(x);
		if (x != y)
			_Feraise(_FE_INEXACT);
		return y;
	}
}
