/*
 * llroundf function: x rounded to nearest, halfway away from zero, as a long
 * long; invalid when it does not fit.
 */
#include <math.h>

#include "xmath.h"

long long (llroundf)(float x)
{
	switch (_FDtest(&x)) {
	default:	/* finite */
		x = roundf(x);
		if (-9223372036854775808.0F <= x && x <= 9223372036854775808.0F)
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
