/*
 * logbl function: the binary exponent of x as a floating value.
 */
#include <math.h>

#include "xmath.h"

long double (logbl)(long double x)
{
	short xexp;

	switch (_LDunscale(&xexp, &x)) {
	case _NANCODE:
		return x;
	case 0:
		_Feraise(_FE_DIVBYZERO);
		return -_LInf._Long_double;
	case _INFCODE:
		return _LInf._Long_double;
	default:
		return --xexp;
	}
}
