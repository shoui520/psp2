/*
 * llrint function: x rounded in the current rounding mode, as a long long;
 * inexact when it moves x, invalid when it does not fit.
 */
#include <math.h>

#include "xmath.h"

long long (llrint)(double x)
{
	double y;

	switch (_Dtest(&x)) {
	default:	/* finite */
		y = nearbyint(x);
		if (x != y)
			_Feraise(_FE_INEXACT);
		if (-9223372036854775808.0 <= y && y <= 9223372036854775808.0)
			return (long long)y;
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
