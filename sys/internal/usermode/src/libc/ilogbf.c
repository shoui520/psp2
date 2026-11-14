/*
 * ilogbf function: the binary exponent of x as an int in float.
 */
#include <limits.h>
#include <math.h>

#include "xmath.h"

int (ilogbf)(float x)
{
	short xexp;

	switch (_FDunscale(&xexp, &x)) {
	case 0:
		return FP_ILOGB0;
	case _INFCODE:
		return INT_MAX;
	case _NANCODE:
		return FP_ILOGBNAN;
	default:
		return --xexp;
	}
}
