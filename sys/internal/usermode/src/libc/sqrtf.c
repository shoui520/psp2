/*
 * sqrtf function: the square root of x, from a quadratic first guess on
 * the fraction and two Newton steps, scaled by half the exponent.
 */
#include <math.h>

#include "xmath.h"

float (sqrtf)(float x)
{
	short xexp;

	switch (_FDunscale(&xexp, &x)) {
	case _NANCODE:
	case 0:
		return x;
	case _INFCODE:
		if (!FSIGN(x))
			return x;
		/* fall through */
	default:
		if (FSIGN(x)) {	/* a domain error */
			_Feraise(_FE_INVALID);
			return _FNan._Float;
		} else {
			float y;

			if (xexp & 1) {	/* an even exponent, x in [0.25, 1) */
				x *= 2.0F;
				--xexp;
			}
			y = (-0.09977F * x + 0.71035F) * x + 0.3866F;
			y += x / y;
			y = 0.25F * y + x / y;
			_FDscale(&y, xexp / 2);
			return y;
		}
	}
}
