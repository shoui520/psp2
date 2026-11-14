/*
 * sqrt function: the square root of x, from a quadratic first guess on
 * the fraction and three Newton steps, scaled by half the exponent.
 */
#include <math.h>

#include "xmath.h"

double (sqrt)(double x)
{
	short xexp;

	switch (_Dunscale(&xexp, &x)) {
	case _NANCODE:
	case 0:
		return x;
	case _INFCODE:
		if (!DSIGN(x))
			return x;
		/* fall through */
	default:
		if (DSIGN(x)) {	/* a domain error */
			_Feraise(_FE_INVALID);
			return _Nan._Double;
		} else {
			double y;

			if (xexp & 1) {	/* an even exponent, x in [0.25, 1) */
				x *= 2.0;
				--xexp;
			}
			y = (-0.09977 * x + 0.71035) * x + 0.3866;
			y += x / y;
			y = 0.25 * y + x / y;
			y = 0.5 * (y + x / y);
			_Dscale(&y, xexp / 2);
			return y;
		}
	}
}
