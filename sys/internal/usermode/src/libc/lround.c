/*
 * lround function: x rounded to nearest, halfway away from zero, as a
 * long; invalid when it does not fit.
 */
#include <limits.h>
#include <math.h>

#include "xmath.h"

long (lround)(double x)
{
	switch (_Dtest(&x)) {
	default:	/* finite */
		x = round(x);
		if ((double)LONG_MIN <= x && x <= (double)LONG_MAX)
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
