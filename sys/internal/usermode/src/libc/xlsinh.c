/*
 * _LSinh function: y times the hyperbolic sine of x, in long double (a
 * double on psp2), for sinhl and the complex functions.
 */
#include "xmath.h"

static const long double q[] = {	/* below ln3/2: the denominator, 1 + x^2 times this */
	0.00015900461490295665L, -0.02056695099386676L,
};
static const long double p[] = {	/* the numerator, x + x^3 times this in x^2 */
	5.35222134191626e-05L, 0.005064512782295745L, 0.14609971567280727L,
};
static const long double ln3by2 = 0.5493061443340549L;

long double _LSinh(long double x, long double y)
{
	const short errx = _LDtest(&x);
	const short erry = _LDtest(&y);

	if (errx >= 0 || erry >= 0) {	/* a zero, an infinity or a NaN */
		if (errx == _NANCODE) {
			return x;
		} else if (erry == _NANCODE) {
			return y;
		} else if (errx == _INFCODE) {
			if (erry != 0)
				return x * y;
			_Feraise(_FE_INVALID);	/* INF times 0 */
			return _LNan._Long_double;
		} else if (errx == 0) {
			if (erry != _INFCODE)
				return x * y;
			_Feraise(_FE_INVALID);	/* 0 times INF */
			return _LNan._Long_double;
		} else {
			return x * y;
		}
	} else {	/* finite */
		int neg = 0;

		if (x < 0.0L) {
			x = -x;
			neg = 1;
		}
		if (x < _LRteps._Long_double) {	/* tiny: y x */
			x = x * y;
		} else if (x < ln3by2) {	/* a rational approximation */
			const long double w = x * x;

			x = y * ((x + x * w * ((p[0] * w + p[1]) * w + p[2])) / (1.0L + w * (q[0] * w + q[1])));
		} else if (x < _LXbig._Long_double) {	/* y (e^x - e^-x) / 2 */
			_LExp(&x, 1.0L, -1);
			x = y * (x - 0.25L / x);
		} else {	/* y e^x / 2 */
			_LExp(&x, y, -1);
		}
		return neg ? -x : x;
	}
}
