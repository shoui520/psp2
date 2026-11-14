/*
 * acosh function: the inverse hyperbolic cosine, log1p of x - 1 + sqrt(x^2 - 1),
 * or ln(2x) once x^2 - 1 is x^2.
 */
#include <math.h>

#include "xmath.h"

static const double ln2 = 0.69314718055994530942;

double (acosh)(double x)
{
	switch (_Dtest(&x)) {
	case _NANCODE:
		return x;
	case _INFCODE:
		if (!DSIGN(x))
			return x;
		/* fall through */
	default:
		if (x < 1.0) {	/* a domain error */
			_Feraise(_FE_INVALID);
			return _Nan._Double;
		} else if (x == 1.0) {
			return 0.0;
		} else if (x < 2.0 / _Eps._Double) {
			const double y = x - 1.0;

			return log1p(y + sqrt(y * (x + 1.0)));
		} else {
			return _Log(x, 0) + ln2;
		}
	}
}
