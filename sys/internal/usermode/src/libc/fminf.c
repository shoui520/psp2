/*
 * fminf function: the smaller of x and y in float, the number if the other is a NaN, and
 * +0 above -0.
 */
#include <math.h>

#include "xmath.h"

float (fminf)(float x, float y)
{
	if (_FDtest(&x) == _NANCODE)
		return y;
	else if (_FDtest(&y) == _NANCODE)
		return x;
	else
		return x < y || (x == y && FSIGN(x)) ? x : y;
}
