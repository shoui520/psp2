/*
 * fmax function: the larger of x and y, the number if the other is a NaN, and
 * +0 above -0.
 */
#include <math.h>

#include "xmath.h"

double (fmax)(double x, double y)
{
	if (_Dtest(&x) == _NANCODE)
		return y;
	else if (_Dtest(&y) == _NANCODE)
		return x;
	else
		return x < y || (x == y && DSIGN(x)) ? y : x;
}
