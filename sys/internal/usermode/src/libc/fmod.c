/*
 * fmod function: the remainder of x / y with the sign of x, by subtracting
 * y scaled to x's exponent, then to each lower one.
 */
#include <math.h>

#include "xmath.h"

double (fmod)(double x, double y)
{
	const short errx = _Dtest(&x);
	const short erry = _Dtest(&y);

	if (errx >= 0 || erry >= 0) {	/* a zero, an infinity or a NaN */
		if (errx == _NANCODE)
			return x;
		else if (erry == _NANCODE)
			return y;
		else if (errx == _INFCODE || erry == 0) {	/* a domain error */
			_Feraise(_FE_INVALID);
			return _Nan._Double;
		} else {
			return x;
		}
	} else {
		double t;
		short ychar;
		short xchar;
		int n;
		int neg;

		if (y < 0.0)
			y = -y;
		neg = 0;
		if (x < 0.0) {
			x = -x;
			neg = 1;
		}
		t = y;
		_Dunscale(&ychar, &t);
		for (n = 0; 0 <= n;) {	/* subtract y * 2^n from x */
			t = x;
			if (_Dunscale(&xchar, &t) == 0 || (n = xchar - ychar) < 0)
				break;
			for (; 0 <= n; --n) {
				t = y;
				_Dscale(&t, n);
				if (t <= x) {
					x -= t;
					break;
				}
			}
		}
		return neg ? -x : x;
	}
}
