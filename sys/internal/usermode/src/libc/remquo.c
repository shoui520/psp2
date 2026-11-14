/*
 * remquo function: the remainder of x/y rounded to nearest, and the low
 * bits of that quotient, with its sign, at pquo.
 */
#include <math.h>

#include "xmath.h"

double (remquo)(double x, double y, int *pquo)
{
	const short errx = _Dtest(&x);
	const short erry = _Dtest(&y);

	if (pquo != 0)
		*pquo = 0;
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
		short xchar;
		short ychar;
		short neg = 0;
		int quo = 0;
		int xneg = 0;
		int n;

		if (y < 0.0) {
			y = -y;
			neg = 1;
		}
		if (x < 0.0) {
			x = -x;
			neg = 1 - neg;
			xneg = 1;
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
					if (n < 32)
						quo += 1 << n;
					break;
				}
			}
		}
		if (0.0 < x) {	/* round the quotient to nearest, ties to even */
			t = y * 0.5;
			if ((t < x && 0.0 < t) || (t == x && (quo & 1) != 0)) {
				++quo;
				x -= y;
			}
		}
		if (pquo != 0) {
			quo &= 0x7fffffff;
			*pquo = neg ? -quo : quo;
		}
		return xneg ? -x : x;
	}
}
