/*
 * _LErfc function: erfc(x) for 0 < x, e^-x^2 times a rational approximation
 * in x.
 */
#include "xmath.h"

static const long double num[] = {	/* the numerator's coefficients, highest first */
	6.179283273584607e-10L, 0.011684287172270874L, 0.0956378935009531L, 0.386764151436229L,
	0.9123170622003576L, 1.252536256149567L, 0.8357138200112095L,
};
static const long double den[] = {	/* the denominator's */
	0.020709916302643547L, 0.16951239053544545L, 0.6958969498016204L, 1.7015840993444071L,
	2.5541015508553357L, 2.195506625970465L, 0.8357186452568679L,
};

long double _LErfc(long double x)
{
	long double y = -x * x;

	_LExp(&y, _LPoly(x, num, 6) / _LPoly(x, den, 6), 0);
	return y;
}
