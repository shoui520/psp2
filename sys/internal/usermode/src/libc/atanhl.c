/*
 * atanhl function: the inverse hyperbolic tangent in long double, log1p(2|x| / (1 - |x|)) / 2
 * with the sign of x.
 */
#include <math.h>

#include "xmath.h"

long double (atanhl)(long double x)
{
	switch (_LDtest(&x)) {
	case _NANCODE:
	case 0:
		return x;
	default: {
		int neg = 0;

		if (x < 0.0L) {
			x = -x;
			neg = 1;
		}
		if (1.0L < x) {	/* a domain error */
			_Feraise(_FE_INVALID);
			return _LNan._Long_double;
		} else if (x == 1.0L) {	/* a pole */
			_Feraise(_FE_DIVBYZERO);
			return neg ? -_LInf._Long_double : _LInf._Long_double;
		} else {
			x = 0.5L * log1pl(2.0L * x / (1.0L - x));
			return neg ? -x : x;
		}
	}
	}
}
