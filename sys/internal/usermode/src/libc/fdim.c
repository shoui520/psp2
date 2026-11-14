/*
 * fdim function: the positive difference of x and y.
 */
#include <math.h>

#include "xmath.h"

double (fdim)(double x, double y)
{
	if (_Dtest(&x) == _NANCODE)
		return x;
	else if (_Dtest(&y) == _NANCODE)
		return y;
	else
		return x <= y ? 0.0 : x - y;
}
