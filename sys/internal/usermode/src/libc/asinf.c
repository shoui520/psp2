/*
 * asinf function: the arcsine in float, through _FAtan of x / sqrt(1 - x^2) or its
 * reciprocal.
 */
#include <math.h>

#include "xmath.h"

static const float sqrthalf = 0.70710678F;	/* sqrt(1/2) */

float (asinf)(float x)
{
	int idx;

	switch (_FDtest(&x)) {
	case _NANCODE:
	case 0:
		return x;
	default:
		idx = 0;
		if (x < 0.0F) {
			x = -x;
			idx = 8;
		}
		if (x <= sqrthalf) {
			return _FAtan(x / sqrtf((1.0F - x) * (x + 1.0F)), idx);
		} else if (x <= 1.0) {
			return _FAtan(sqrtf((1.0F - x) * (x + 1.0F)) / x, idx ^ 2);
		} else {	/* |x| > 1: a domain error */
			_Feraise(_FE_INVALID);
			return _FNan._Float;
		}
	}
}
