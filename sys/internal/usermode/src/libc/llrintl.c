/*
 * llrintl function: x rounded in the current rounding mode, as a long long;
 * inexact when it moves x, invalid when it does not fit.
 */
#include <math.h>

#include "xmath.h"

long long (llrintl)(long double x)
{
	long double y;

	switch (_LDtest(&x)) {
	default:	/* finite */
		y = nearbyintl(x);
		if (x != y)
			_Feraise(_FE_INEXACT);
		if (-9223372036854775808.0L <= y && y <= 9223372036854775808.0L)
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
