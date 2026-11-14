/*
 * fmodf function: the remainder of x / y with the sign of x in float, by subtracting
 * y scaled to x's exponent, then to each lower one.
 */
#include <math.h>

#include "xmath.h"

float (fmodf)(float x, float y)
{
	const short errx = _FDtest(&x);
	const short erry = _FDtest(&y);

	if (errx >= 0 || erry >= 0) {	/* a zero, an infinity or a NaN */
		if (errx == _NANCODE)
			return x;
		else if (erry == _NANCODE)
			return y;
		else if (errx == _INFCODE || erry == 0) {	/* a domain error */
			_Feraise(_FE_INVALID);
			return _FNan._Float;
		} else {
			return x;
		}
	} else {
		float t;
		short ychar;
		short xchar;
		int n;
		int neg;

		if (y < 0.0F)
			y = -y;
		neg = 0;
		if (x < 0.0F) {
			x = -x;
			neg = 1;
		}
		t = y;
		_FDunscale(&ychar, &t);
		for (n = 0; 0 <= n;) {	/* subtract y * 2^n from x */
			t = x;
			if (_FDunscale(&xchar, &t) == 0 || (n = xchar - ychar) < 0)
				break;
			for (; 0 <= n; --n) {
				t = y;
				_FDscale(&t, n);
				if (t <= x) {
					x -= t;
					break;
				}
			}
		}
		return neg ? -x : x;
	}
}
