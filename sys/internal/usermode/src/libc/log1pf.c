/*
 * log1pf function: the natural logarithm of 1 + x, corrected for the
 * rounding error of 1 + x.
 */
#include <math.h>

#include "xmath.h"

float (log1pf)(float x)
{
	switch (_FDtest(&x)) {
	case _NANCODE:
	case 0:
		return x;
	case _INFCODE:
		if (!FSIGN(x))
			return x;
		/* fall through */
	default:
		if (x < -1.0F) {	/* a domain error */
			_Feraise(_FE_INVALID);
			return _FNan._Float;
		} else if (x == -1.0F) {	/* a pole */
			_Feraise(_FE_DIVBYZERO);
			return -_FInf._Float;
		} else {
			const float y = x + 1.0F;

			return _FLog(y, 0) - ((y - 1.0F) - x) / y;
		}
	}
}
