/*
 * log1p function: the natural logarithm of 1 + x, corrected for the
 * rounding error of 1 + x.
 */
#include <math.h>

#include "xmath.h"

double (log1p)(double x)
{
	switch (_Dtest(&x)) {
	case _NANCODE:
	case 0:
		return x;
	case _INFCODE:
		if (!DSIGN(x))
			return x;
		/* fall through */
	default:
		if (x < -1.0) {	/* a domain error */
			_Feraise(_FE_INVALID);
			return _Nan._Double;
		} else if (x == -1.0) {	/* a pole */
			_Feraise(_FE_DIVBYZERO);
			return -_Inf._Double;
		} else {
			const double y = x + 1.0;

			return _Log(y, 0) - ((y - 1.0) - x) / y;
		}
	}
}
