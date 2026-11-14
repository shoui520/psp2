/*
 * atanhf function: the inverse hyperbolic tangent in float, log1p(2|x| / (1 - |x|)) / 2
 * with the sign of x.
 */
#include <math.h>

#include "xmath.h"

float (atanhf)(float x)
{
	switch (_FDtest(&x)) {
	case _NANCODE:
	case 0:
		return x;
	default: {
		int neg = 0;

		if (x < 0.0F) {
			x = -x;
			neg = 1;
		}
		if (1.0F < x) {	/* a domain error */
			_Feraise(_FE_INVALID);
			return _FNan._Float;
		} else if (x == 1.0F) {	/* a pole */
			_Feraise(_FE_DIVBYZERO);
			return neg ? -_FInf._Float : _FInf._Float;
		} else {
			x = 0.5F * log1pf(2.0F * x / (1.0F - x));
			return neg ? -x : x;
		}
	}
	}
}
