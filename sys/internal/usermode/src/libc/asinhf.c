/*
 * asinhf function: the inverse hyperbolic sine in float, log1p of |x| + x^2 / (sqrt(x^2 + 1) + 1),
 * or ln(2|x|) once x^2 + 1 is x^2, with the sign of x.
 */
#include <math.h>

#include "xmath.h"

static const float ln2 = 0.69314718F;

float (asinhf)(float x)
{
	switch (_FDtest(&x)) {
	case _NANCODE:
	case _INFCODE:
	case 0:
		return x;
	default: {
		int neg = 0;

		if (x < 0.0F) {
			x = -x;
			neg = 1;
		}
		if (x < 2.0F / _FEps._Float) {
			const float g = x * x;

			x = log1pf(x + g / (sqrtf(g + 1.0F) + 1.0F));
		} else {
			x = _FLog(x, 0) + ln2;
		}
		return neg ? -x : x;
	}
	}
}
