/*
 * atanh function: the inverse hyperbolic tangent, log1p(2|x| / (1 - |x|)) / 2
 * with the sign of x.
 */
#include <math.h>

#include "xmath.h"

double (atanh)(double x)
{
	switch (_Dtest(&x)) {
	case _NANCODE:
	case 0:
		return x;
	default: {
		int neg = 0;

		if (x < 0.0) {
			x = -x;
			neg = 1;
		}
		if (1.0 < x) {	/* a domain error */
			_Feraise(_FE_INVALID);
			return _Nan._Double;
		} else if (x == 1.0) {	/* a pole */
			_Feraise(_FE_DIVBYZERO);
			return neg ? -_Inf._Double : _Inf._Double;
		} else {
			x = 0.5 * log1p(2.0 * x / (1.0 - x));
			return neg ? -x : x;
		}
	}
	}
}
