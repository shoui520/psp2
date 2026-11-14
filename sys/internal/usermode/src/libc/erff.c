/*
 * erff function: the error function in float, a rational approximation in x^2 below
 * _FErf_small, 1 - _FErfc(|x|) up to _FErf_big, and +-1 beyond.
 */
#include <math.h>

#include "xmath.h"

float _FErf_big = 3.82F;	/* erfc(x) is lost in 1 above */
float _FErf_small = 1.523F;	/* the rational approximation's range */

static const float num[] = {	/* the numerator's coefficients, highest first */
	0.0002614094F, 0.03605237F, 0.13132069F, 1.1283792F,
};
static const float den[] = {	/* the denominator's */
	0.0063628103F, 0.081852145F, 0.44971365F, 1.0F,
};

float (erff)(float x)
{
	switch (_FDtest(&x)) {
	case _NANCODE:
	case 0:
		return x;
	case _INFCODE:
		return FSIGN(x) ? -1.0F : 1.0F;
	default: {
		float y = x < 0.0F ? -x : x;

		if (y < _FErf_small) {
			const float g = x * x;

			return (((num[0] * g + num[1]) * g + num[2]) * g + num[3])
			       / (((den[0] * g + den[1]) * g + den[2]) * g + den[3]) * x;
		} else {
			float z = 1.0F;

			if (y < _FErf_big)
				z -= _FErfc(y);
			return x < 0.0F ? -z : z;
		}
	}
	}
}
