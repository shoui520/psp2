/*
 * llround function: x rounded to nearest, halfway away from zero, as a long
 * long; invalid when it does not fit.
 */
#include <math.h>

#include "xmath.h"

long long (llround)(double x)
{
	switch (_Dtest(&x)) {
	default:	/* finite */
		x = round(x);
		if (-9223372036854775808.0 <= x && x <= 9223372036854775808.0)
			return (long long)x;
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
