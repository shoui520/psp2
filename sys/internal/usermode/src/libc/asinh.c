/*
 * asinh function: the inverse hyperbolic sine, log1p of |x| + x^2 / (sqrt(x^2 + 1) + 1),
 * or ln(2|x|) once x^2 + 1 is x^2, with the sign of x.
 */
#include <math.h>

#include "xmath.h"

static const double ln2 = 0.69314718055994530942;

double (asinh)(double x)
{
	switch (_Dtest(&x)) {
	case _NANCODE:
	case _INFCODE:
	case 0:
		return x;
	default: {
		int neg = 0;

		if (x < 0.0) {
			x = -x;
			neg = 1;
		}
		if (x < 2.0 / _Eps._Double) {
			const double g = x * x;

			x = log1p(x + g / (sqrt(g + 1.0) + 1.0));
		} else {
			x = _Log(x, 0) + ln2;
		}
		return neg ? -x : x;
	}
	}
}
