/*
 * lroundf function: x rounded to nearest, halfway away from zero, as a
 * long; invalid when it does not fit.
 */
#include <limits.h>
#include <math.h>

#include "xmath.h"

long (lroundf)(float x)
{
	switch (_FDtest(&x)) {
	default:	/* finite */
		x = roundf(x);
		if ((float)LONG_MIN <= x && x <= (float)LONG_MAX)
			return (long)x;
		/* fall through */
	case _INFCODE:
		_Feraise(_FE_INVALID);
		return 0;
	case _NANCODE:
		_Feraise(_FE_INVALID);
		/* fall through */
	case 0:
		return 0;
	}
}
