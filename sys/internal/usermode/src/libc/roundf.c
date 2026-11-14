/*
 * roundf function: x rounded to the nearest integer, halfway cases away
 * from zero.
 */
#include <math.h>

#include "xmath.h"

float (roundf)(float x)
{
	switch (_FDint(&x, 1)) {	/* keep the halves */
	case _NANCODE:
	case _INFCODE:
		return x;
	default:
		if (_FDint(&x, 0) != 0)	/* a half: away from zero */
			x = FSIGN(x) ? x - 1.0F : x + 1.0F;
		return x;
	}
}
