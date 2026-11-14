/*
 * lrintl function: x rounded in the current rounding mode, as a long;
 * inexact when it moves x, invalid when it does not fit.
 */
#include <limits.h>
#include <math.h>

#include "xmath.h"

long (lrintl)(long double x)
{
	long double y;

	switch (_LDtest(&x)) {
	default:	/* finite */
		y = nearbyintl(x);
		if (x != y)
			_Feraise(_FE_INEXACT);
		if ((long double)LONG_MIN <= y && y <= (long double)LONG_MAX)
			return (long)y;
		/* fall through */
	case _INFCODE:
		_Feraise(_FE_INVALID);
		return 0;
	case _NANCODE:
		_Feraise(_FE_INVALID);
		/* fall through */
	case 0:
		return 0;
	}
}
