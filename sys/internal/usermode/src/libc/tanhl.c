/*
 * tanhl function: the hyperbolic tangent of x, a rational approximation
 * below ln 2 (of x/2 doubled below ln 2), 1 - 2/(e^2x + 1) up to xbig, 1
 * beyond.
 */
#include <math.h>

#include "xmath.h"

static const long double xbig = 19.76L;	/* tanh(x) is 1 in long double from here */
static const long double den[] = {	/* the denominator's coefficients, highest first */
	0.01750072318694302L, 0.44846583775714L,
};
static const long double num[] = {	/* the numerator's */
	-6.38332552609522e-06L, 0.0013454439344480805L, 0.11513250442380779L,
};
static const long double ln2by2 = 0.34657359027997265471L;	/* ln 2 / 2 */
static const long double ln2 = 0.69314718055994530942L;

long double (tanhl)(long double x)
{
	switch (_LDtest(&x)) {
	case _NANCODE:
		return x;
	case _INFCODE:
		return LSIGN(x) ? -1.0L : 1.0L;
	case 0:
		return x;
	default: {	/* finite */
		long double g;
		int neg = 0;

		if (x < 0.0L) {
			x = -x;
			neg = 1;
		}
		if (x < _LRteps._Long_double) {	/* tanh(x) is x */
			;
		} else if (x < ln2by2) {
			g = x * x;
			x = (x + x * g * ((num[0] * g + num[1]) * g + num[2])) / (g * (den[0] * g + den[1]) + 1.0L);
		} else if (x < ln2) {	/* tanh(x) = 2 tanh(x/2) / (1 + tanh(x/2)^2) */
			x *= 0.5L;
			g = x * x;
			x = (x + x * g * ((num[0] * g + num[1]) * g + num[2])) / (g * (den[0] * g + den[1]) + 1.0L);
			x = x * 2.0L / (x * x + 1.0L);
		} else if (x < xbig) {
			x = 1.0L - 2.0L / (expl(x + x) + 1.0L);
		} else {
			x = 1.0L;
		}
		return neg ? -x : x;
	}
	}
}
