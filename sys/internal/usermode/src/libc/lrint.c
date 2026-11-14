/*
 * lrint function: x rounded in the current rounding mode, as a long;
 * inexact when it moves x, invalid when it does not fit.
 */
#include <limits.h>
#include <math.h>

#include "xmath.h"

long (lrint)(double x)
{
	double y;

	switch (_Dtest(&x)) {
	default:	/* finite */
		y = nearbyint(x);
		if (x != y)
			_Feraise(_FE_INEXACT);
		if ((double)LONG_MIN <= y && y <= (double)LONG_MAX)
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
