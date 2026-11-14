/*
 * acosf function: the arccosine in float, through _FAtan.
 */
#include <math.h>

#include "xmath.h"

static const float sqrthalf = 0.70710678F;	/* sqrt(1/2) */

float (acosf)(float x)
{
	int idx;

	if (_FDtest(&x) == _NANCODE)
		return x;
	idx = 2;
	if (x < 0.0F) {
		x = -x;
		idx = 4;
	}
	if (x <= sqrthalf) {
		return _FAtan(x / sqrtf((1.0F - x) * (x + 1.0F)), idx);
	} else if (x <= 1.0F) {
		return _FAtan(sqrtf((1.0F - x) * (x + 1.0F)) / x, idx ^ 2);
	} else {	/* |x| > 1: a domain error */
		_Feraise(_FE_INVALID);
		return _FNan._Float;
	}
}
