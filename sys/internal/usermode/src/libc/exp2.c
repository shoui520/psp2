/*
 * exp2 function: 2 to the x. The nearest integer n of x goes to the
 * exponent, and e to the remaining fraction times ln 2 is computed by _Exp.
 */
#include <math.h>

#include "xmath.h"

static const double ln2 = 0.69314718055994530942;

double (exp2)(double x)
{
	switch (_Dtest(&x)) {
	case _NANCODE:
		return x;
	case _INFCODE:
		return DSIGN(x) ? 0.0 : x;
	case 0:
		return 1.0;
	default: {
		long n;	/* SCE: (short)y, but the rounding step is not truncated again */

		if (x <= -2147483648.0 || 2147483647.0 <= x) {	/* _Exp overflows or underflows */
			n = 0;
		} else {
			double y = x;

			_Dint(&y, 0);
			n = (short)y;
			x -= y;
			if (0.5 < x) {
				++n;
				x -= 1.0;
			} else if (x < -0.5) {
				--n;
				x += 1.0;
			}
		}
		x *= ln2;
		_Exp(&x, 1.0, n);
		return x;
	}
	}
}
