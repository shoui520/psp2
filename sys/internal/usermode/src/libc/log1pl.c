/*
 * log1pl function: the natural logarithm of 1 + x, corrected for the
 * rounding error of 1 + x.
 */
#include <math.h>

#include "xmath.h"

long double (log1pl)(long double x)
{
	switch (_LDtest(&x)) {
	case _NANCODE:
	case 0:
		return x;
	case _INFCODE:
		if (!LSIGN(x))
			return x;
		/* fall through */
	default:
		if (x < -1.0L) {	/* a domain error */
			_Feraise(_FE_INVALID);
			return _LNan._Long_double;
		} else if (x == -1.0L) {	/* a pole */
			_Feraise(_FE_DIVBYZERO);
			return -_LInf._Long_double;
		} else {
			const long double y = x + 1.0L;

			return _LLog(y, 0) - ((y - 1.0L) - x) / y;
		}
	}
}
