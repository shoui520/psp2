/*
 * tanh function: the hyperbolic tangent of x, a rational approximation
 * below ln 2 (of x/2 doubled below ln 2), 1 - 2/(e^2x + 1) up to xbig, 1
 * beyond.
 */
#include <math.h>

#include "xmath.h"

static const double xbig = 19.76;	/* tanh(x) is 1 in double from here */
static const double den[] = {	/* the denominator's coefficients, highest first */
	0.01750072318694302, 0.44846583775714,
};
static const double num[] = {	/* the numerator's */
	-6.38332552609522e-06, 0.0013454439344480805, 0.11513250442380779,
};
static const double ln2by2 = 0.34657359027997265471;	/* ln 2 / 2 */
static const double ln2 = 0.69314718055994530942;

double (tanh)(double x)
{
	switch (_Dtest(&x)) {
	case _NANCODE:
		return x;
	case _INFCODE:
		return DSIGN(x) ? -1.0 : 1.0;
	case 0:
		return x;
	default: {	/* finite */
		double g;
		int neg = 0;

		if (x < 0.0) {
			x = -x;
			neg = 1;
		}
		if (x < _Rteps._Double) {	/* tanh(x) is x */
			;
		} else if (x < ln2by2) {
			g = x * x;
			x = (x + x * g * ((num[0] * g + num[1]) * g + num[2])) / (g * (den[0] * g + den[1]) + 1.0);
		} else if (x < ln2) {	/* tanh(x) = 2 tanh(x/2) / (1 + tanh(x/2)^2) */
			x *= 0.5;
			g = x * x;
			x = (x + x * g * ((num[0] * g + num[1]) * g + num[2])) / (g * (den[0] * g + den[1]) + 1.0);
			x = x * 2.0 / (x * x + 1.0);
		} else if (x < xbig) {
			x = 1.0 - 2.0 / (exp(x + x) + 1.0);
		} else {
			x = 1.0;
		}
		return neg ? -x : x;
	}
	}
}
