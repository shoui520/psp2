/*
 * acoshl function: the inverse hyperbolic cosine in long double.
 */
#include <math.h>

#include "xmath.h"

static const long double ln2 = 0.69314718055994530942L;

long double (acoshl)(long double x)
{
	switch (_LDtest(&x)) {
	case _NANCODE:
		return x;
	case _INFCODE:
		if (!LSIGN(x))
			return x;
		/* fall through */
	default:
		if (x < 1.0L) {	/* a domain error */
			_Feraise(_FE_INVALID);
			return _LNan._Long_double;
		} else if (x == 1.0L) {
			return 0.0L;
		} else if (x < 2.0L / _LEps._Long_double) {
			const long double y = x - 1.0L;

			return log1pl(y + sqrtl(y * (x + 1.0L)));
		} else {
			return _LLog(x, 0) + ln2;
		}
	}
}
