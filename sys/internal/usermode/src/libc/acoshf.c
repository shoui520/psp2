/*
 * acoshf function: the inverse hyperbolic cosine in float.
 */
#include <math.h>

#include "xmath.h"

static const float ln2 = 0.69314718F;

float (acoshf)(float x)
{
	switch (_FDtest(&x)) {
	case _NANCODE:
		return x;
	case _INFCODE:
		if (!FSIGN(x))
			return x;
		/* fall through */
	default:
		if (x < 1.0F) {	/* a domain error */
			_Feraise(_FE_INVALID);
			return _FNan._Float;
		} else if (x == 1.0F) {
			return 0.0F;
		} else if (x < 2.0F / _FEps._Float) {
			const float y = x - 1.0F;

			return log1pf(y + sqrtf(y * (x + 1.0F)));
		} else {
			return _FLog(x, 0) + ln2;
		}
	}
}
