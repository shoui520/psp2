/*
 * exp2f function: 2 to the x in float. The nearest integer n of x goes to the
 * exponent, and e to the remaining fraction times ln 2 is computed by _FExp.
 */
#include <math.h>

#include "xmath.h"

static const float ln2 = 0.69314718F;

float (exp2f)(float x)
{
	switch (_FDtest(&x)) {
	case _NANCODE:
		return x;
	case _INFCODE:
		return FSIGN(x) ? 0.0F : x;
	case 0:
		return 1.0F;
	default: {
		long n;	/* SCE: (short)y, but the rounding step is not truncated again */

		if (x <= -2147483648.0F || 2147483648.0F <= x) {	/* _Exp overflows or underflows */
			n = 0;
		} else {
			float y = x;

			_FDint(&y, 0);
			n = (short)y;
			x -= y;
			if (0.5F < x) {
				++n;
				x -= 1.0F;
			} else if (x < -0.5F) {
				--n;
				x += 1.0F;
			}
		}
		x *= ln2;
		_FExp(&x, 1.0F, n);
		return x;
	}
	}
}
