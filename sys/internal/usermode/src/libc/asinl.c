/*
 * asinl function: the arcsine in long double, through _LAtan of x / sqrt(1 - x^2) or its
 * reciprocal.
 */
#include <math.h>

#include "xmath.h"

static const long double sqrthalf = 0.70710678118654752440L;	/* sqrt(1/2) */

long double (asinl)(long double x)
{
	int idx;

	switch (_LDtest(&x)) {
	case _NANCODE:
	case 0:
		return x;
	default:
		idx = 0;
		if (x < 0.0L) {
			x = -x;
			idx = 8;
		}
		if (x <= sqrthalf) {
			return _LAtan(x / sqrtl((1.0L - x) * (x + 1.0L)), idx);
		} else if (x <= 1.0) {
			return _LAtan(sqrtl((1.0L - x) * (x + 1.0L)) / x, idx ^ 2);
		} else {	/* |x| > 1: a domain error */
			_Feraise(_FE_INVALID);
			return _LNan._Long_double;
		}
	}
}
