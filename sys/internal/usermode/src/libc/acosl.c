/*
 * acosl function: the arccosine in long double, through _LAtan.
 */
#include <math.h>

#include "xmath.h"

static const long double sqrthalf = 0.70710678118654752440L;	/* sqrt(1/2) */

long double (acosl)(long double x)
{
	int idx;

	if (_LDtest(&x) == _NANCODE)
		return x;
	idx = 2;
	if (x < 0.0L) {
		x = -x;
		idx = 4;
	}
	if (x <= sqrthalf) {
		return _LAtan(x / sqrtl((1.0L - x) * (x + 1.0L)), idx);
	} else if (x <= 1.0L) {
		return _LAtan(sqrtl((1.0L - x) * (x + 1.0L)) / x, idx ^ 2);
	} else {	/* |x| > 1: a domain error */
		_Feraise(_FE_INVALID);
		return _LNan._Long_double;
	}
}
