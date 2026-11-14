/*
 * _Erfc function: erfc(x) for 0 < x, e^-x^2 times a rational approximation
 * in x.
 */
#include "xmath.h"

static const double num[] = {	/* the numerator's coefficients, highest first */
	6.179283273584607e-10, 0.011684287172270874, 0.0956378935009531, 0.386764151436229,
	0.9123170622003576, 1.252536256149567, 0.8357138200112095,
};
static const double den[] = {	/* the denominator's */
	0.020709916302643547, 0.16951239053544545, 0.6958969498016204, 1.7015840993444071,
	2.5541015508553357, 2.195506625970465, 0.8357186452568679,
};

double _Erfc(double x)
{
	double y = -x * x;

	_Exp(&y, _Poly(x, num, 6) / _Poly(x, den, 6), 0);
	return y;
}
