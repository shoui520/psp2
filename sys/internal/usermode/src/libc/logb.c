/*
 * logb function: the binary exponent of x as a floating value.
 */
#include <math.h>

#include "xmath.h"

double (logb)(double x)
{
	short xexp;

	switch (_Dunscale(&xexp, &x)) {
	case _NANCODE:
		return x;
	case 0:
		_Feraise(_FE_DIVBYZERO);
		return -_Inf._Double;
	case _INFCODE:
		return _Inf._Double;
	default:
		return --xexp;
	}
}
