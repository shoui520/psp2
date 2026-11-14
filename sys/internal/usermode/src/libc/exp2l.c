/*
 * exp2l function: 2 to the x in long double. The nearest integer n of x goes to the
 * exponent, and e to the remaining fraction times ln 2 is computed by _LExp.
 */
#include <math.h>

#include "xmath.h"

static const long double ln2 = 0.69314718055994530942L;

long double (exp2l)(long double x)
{
	switch (_LDtest(&x)) {
	case _NANCODE:
		return x;
	case _INFCODE:
		return LSIGN(x) ? 0.0L : x;
	case 0:
		return 1.0L;
	default: {
		long n;	/* SCE: (short)y, but the rounding step is not truncated again */

		if (x <= -2147483648.0L || 2147483647.0L <= x) {	/* _Exp overflows or underflows */
			n = 0;
		} else {
			long double y = x;

			_LDint(&y, 0);
			n = (short)y;
			x -= y;
			if (0.5L < x) {
				++n;
				x -= 1.0L;
			} else if (x < -0.5L) {
				--n;
				x += 1.0L;
			}
		}
		x *= ln2;
		_LExp(&x, 1.0L, n);
		return x;
	}
	}
}
