/*
 * fminl function: the smaller of x and y in long double, the number if the other is a NaN, and
 * +0 above -0.
 */
#include <math.h>

#include "xmath.h"

long double (fminl)(long double x, long double y)
{
	if (_LDtest(&x) == _NANCODE)
		return y;
	else if (_LDtest(&y) == _NANCODE)
		return x;
	else
		return x < y || (x == y && LSIGN(x)) ? x : y;
}
