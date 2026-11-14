/*
 * fdiml function: the positive difference of x and y in long double.
 */
#include <math.h>

#include "xmath.h"

long double (fdiml)(long double x, long double y)
{
	if (_LDtest(&x) == _NANCODE)
		return x;
	else if (_LDtest(&y) == _NANCODE)
		return y;
	else
		return x <= y ? 0.0L : x - y;
}
