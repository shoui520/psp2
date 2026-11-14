/*
 * sqrtl function: the square root of x, from a quadratic first guess on
 * the fraction and three Newton steps, scaled by half the exponent.
 */
#include <math.h>

#include "xmath.h"

long double (sqrtl)(long double x)
{
	short xexp;

	switch (_LDunscale(&xexp, &x)) {
	case _NANCODE:
	case 0:
		return x;
	case _INFCODE:
		if (!LSIGN(x))
			return x;
		/* fall through */
	default:
		if (LSIGN(x)) {	/* a domain error */
			_Feraise(_FE_INVALID);
			return _LNan._Long_double;
		} else {
			long double y;

			if (xexp & 1) {	/* an even exponent, x in [0.25, 1) */
				x *= 2.0L;
				--xexp;
			}
			y = (-0.09977L * x + 0.71035L) * x + 0.3866L;
			y += x / y;
			y = 0.25L * y + x / y;
			y = 0.5L * (y + x / y);
			_LDscale(&y, xexp / 2);
			return y;
		}
	}
}
