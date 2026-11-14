/*
 * _FRint function: x rounded to an integer in the current rounding mode,
 * ties to even when rounding to nearest.
 */
#include <fenv.h>

#include "xmath.h"

float _FRint(float x)
{
	switch (fegetround()) {
	case FE_TONEAREST:
		if (_FDint(&x, 1) != 0) {	/* not a multiple of 1/2: the nearer integer */
			if (_FDint(&x, 0) != 0)
				x = FSIGN(x) ? x - 1.0F : x + 1.0F;
		} else if (_FDint(&x, 0) != 0 && _FDint(&x, -1) != 0) {	/* an odd half */
			x = FSIGN(x) ? x - 2.0F : x + 2.0F;
		}
		break;
	case FE_UPWARD:
		if (_FDint(&x, 0) != 0 && !FSIGN(x))
			x += 1.0F;
		break;
	case FE_DOWNWARD:
		if (_FDint(&x, 0) != 0 && FSIGN(x))
			x -= 1.0F;
		break;
	case FE_TOWARDZERO:
		_FDint(&x, 0);
		break;
	}
	return x;
}
