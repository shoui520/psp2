/*
 * _Sinh function: y times the hyperbolic sine of x, in double, for sinh
 * and the complex functions.
 */
#include "xmath.h"

static const double q[] = {	/* below ln3/2: the denominator, 1 + x^2 times this */
	0.00015900461490295665, -0.02056695099386676,
};
static const double p[] = {	/* the numerator, x + x^3 times this in x^2 */
	5.35222134191626e-05, 0.005064512782295745, 0.14609971567280727,
};
static const double ln3by2 = 0.5493061443340549;

double _Sinh(double x, double y)
{
	const short errx = _Dtest(&x);
	const short erry = _Dtest(&y);

	if (errx >= 0 || erry >= 0) {	/* a zero, an infinity or a NaN */
		if (errx == _NANCODE) {
			return x;
		} else if (erry == _NANCODE) {
			return y;
		} else if (errx == _INFCODE) {
			if (erry != 0)
				return x * y;
			_Feraise(_FE_INVALID);	/* INF times 0 */
			return _Nan._Double;
		} else if (errx == 0) {
			if (erry != _INFCODE)
				return x * y;
			_Feraise(_FE_INVALID);	/* 0 times INF */
			return _Nan._Double;
		} else {
			return x * y;
		}
	} else {	/* finite */
		int neg = 0;

		if (x < 0.0) {
			x = -x;
			neg = 1;
		}
		if (x < _Rteps._Double) {	/* tiny: y x */
			x = x * y;
		} else if (x < ln3by2) {	/* a rational approximation */
			const double w = x * x;

			x = y * ((x + x * w * ((p[0] * w + p[1]) * w + p[2])) / (1.0 + w * (q[0] * w + q[1])));
		} else if (x < _Xbig) {	/* y (e^x - e^-x) / 2 */
			_Exp(&x, 1.0, -1);
			x = y * (x - 0.25 / x);
		} else {	/* y e^x / 2 */
			_Exp(&x, y, -1);
		}
		return neg ? -x : x;
	}
}
