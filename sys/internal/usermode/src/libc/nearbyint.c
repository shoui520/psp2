/*
 * nearbyint function: x rounded to an integer in the current rounding
 * mode, without raising inexact.
 */
#include <math.h>

#include "xmath.h"

double (nearbyint)(double x)
{
	switch (_Dtest(&x)) {
	case _NANCODE:
	case _INFCODE:
	case 0:
		return x;
	default:	/* finite */
		return _Rint(x);
	}
}
