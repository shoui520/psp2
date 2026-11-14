/*
 * rintf function: x rounded to an integer in the current rounding mode,
 * inexact when that moves x.
 */
#include <math.h>

#include "xmath.h"

float (rintf)(float x)
{
	float y;

	switch (_FDtest(&x)) {
	case _NANCODE:
	case _INFCODE:
	case 0:
		return x;
	default:	/* finite */
		y = _FRint(x);
		if (x != y)
			_Feraise(_FE_INEXACT);
		return y;
	}
}
