/*
 * asinhl function: the inverse hyperbolic sine in long double, log1p of |x| + x^2 / (sqrt(x^2 + 1) + 1),
 * or ln(2|x|) once x^2 + 1 is x^2, with the sign of x.
 */
#include <math.h>

#include "xmath.h"

static const long double ln2 = 0.69314718055994530942L;

long double (asinhl)(long double x)
{
	switch (_LDtest(&x)) {
	case _NANCODE:
	case _INFCODE:
	case 0:
		return x;
	default: {
		int neg = 0;

		if (x < 0.0L) {
			x = -x;
			neg = 1;
		}
		if (x < 2.0L / _LEps._Long_double) {
			const long double g = x * x;

			x = log1pl(x + g / (sqrtl(g + 1.0L) + 1.0L));
		} else {
			x = _LLog(x, 0) + ln2;
		}
		return neg ? -x : x;
	}
	}
}
