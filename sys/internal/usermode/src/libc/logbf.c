/*
 * logbf function: the binary exponent of x as a floating value.
 */
#include <math.h>

#include "xmath.h"

float (logbf)(float x)
{
	short xexp;

	switch (_FDunscale(&xexp, &x)) {
	case _NANCODE:
		return x;
	case 0:
		_Feraise(_FE_DIVBYZERO);
		return -_FInf._Float;
	case _INFCODE:
		return _FInf._Float;
	default:
		return --xexp;
	}
}
