/*
 * ilogbl function: the binary exponent of x as an int in long double.
 */
#include <limits.h>
#include <math.h>

#include "xmath.h"

int (ilogbl)(long double x)
{
	short xexp;

	switch (_LDunscale(&xexp, &x)) {
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
