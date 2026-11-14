/*
 * _LRint function: x rounded to an integer in the current rounding mode,
 * ties to even when rounding to nearest.
 */
#include <fenv.h>

#include "xmath.h"

long double _LRint(long double x)
{
	switch (fegetround()) {
	case FE_TONEAREST:
		if (_LDint(&x, 1) != 0) {	/* not a multiple of 1/2: the nearer integer */
			if (_LDint(&x, 0) != 0)
				x = LSIGN(x) ? x - 1.0L : x + 1.0L;
		} else if (_LDint(&x, 0) != 0 && _LDint(&x, -1) != 0) {	/* an odd half */
			x = LSIGN(x) ? x - 2.0L : x + 2.0L;
		}
		break;
	case FE_UPWARD:
		if (_LDint(&x, 0) != 0 && !LSIGN(x))
			x += 1.0L;
		break;
	case FE_DOWNWARD:
		if (_LDint(&x, 0) != 0 && LSIGN(x))
			x -= 1.0L;
		break;
	case FE_TOWARDZERO:
		_LDint(&x, 0);
		break;
	}
	return x;
}
