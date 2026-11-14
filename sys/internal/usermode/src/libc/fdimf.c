/*
 * fdimf function: the positive difference of x and y in float.
 */
#include <math.h>

#include "xmath.h"

float (fdimf)(float x, float y)
{
	if (_FDtest(&x) == _NANCODE)
		return x;
	else if (_FDtest(&y) == _NANCODE)
		return y;
	else
		return x <= y ? 0.0F : x - y;
}
