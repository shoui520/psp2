/*
 * nearbyintf function: x rounded to an integer in the current rounding
 * mode, without raising inexact.
 */
#include <math.h>

#include "xmath.h"

float (nearbyintf)(float x)
{
	switch (_FDtest(&x)) {
	case _NANCODE:
	case _INFCODE:
	case 0:
		return x;
	default:	/* finite */
		return _FRint(x);
	}
}
