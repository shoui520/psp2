/*
 * acos function: the arccosine, through _Atan of x / sqrt(1 - x^2) or its
 * reciprocal.
 */
#include <math.h>

#include "xmath.h"

static const double sqrthalf = 0.70710678118654752440;	/* sqrt(1/2) */

double (acos)(double x)
{
	int idx;

	if (_Dtest(&x) == _NANCODE)
		return x;
	idx = 2;
	if (x < 0.0) {
		x = -x;
		idx = 4;
	}
	if (x <= sqrthalf) {
		return _Atan(x / sqrt((1.0 - x) * (x + 1.0)), idx);
	} else if (x <= 1.0) {
		return _Atan(sqrt((1.0 - x) * (x + 1.0)) / x, idx ^ 2);
	} else {	/* |x| > 1: a domain error */
		_Feraise(_FE_INVALID);
		return _Nan._Double;
	}
}
