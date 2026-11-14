/*
 * _Rint function: x rounded to an integer in the current rounding mode,
 * ties to even when rounding to nearest.
 */
#include <fenv.h>

#include "xmath.h"

double _Rint(double x)
{
	switch (fegetround()) {
	case FE_TONEAREST:
		if (_Dint(&x, 1) != 0) {	/* not a multiple of 1/2: the nearer integer */
			if (_Dint(&x, 0) != 0)
				x = DSIGN(x) ? x - 1.0 : x + 1.0;
		} else if (_Dint(&x, 0) != 0 && _Dint(&x, -1) != 0) {	/* an odd half */
			x = DSIGN(x) ? x - 2.0 : x + 2.0;
		}
		break;
	case FE_UPWARD:
		if (_Dint(&x, 0) != 0 && !DSIGN(x))
			x += 1.0;
		break;
	case FE_DOWNWARD:
		if (_Dint(&x, 0) != 0 && DSIGN(x))
			x -= 1.0;
		break;
	case FE_TOWARDZERO:
		_Dint(&x, 0);
		break;
	}
	return x;
}
