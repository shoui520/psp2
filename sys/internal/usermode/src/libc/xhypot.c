/*
 * _Hypot function: sqrt(x^2 + y^2) in double, scaled by 2^-*pexp so that it
 * cannot overflow; hypot scales it back.
 */
#include <math.h>

#include "xmath.h"

static const double sqrt2 = 1.4142135623730951;
static const double onepsqrt2_hi = 2.4142;	/* 1 + sqrt(2), its high part */
static const double onepsqrt2_lo = 1.3562373095048801e-05;	/* and its low part */

double _Hypot(double x, double y, int *pexp)
{
	short xexp;
	short yexp;
	const short errx = _Dunscale(&xexp, &x);
	const short erry = _Dunscale(&yexp, &y);

	*pexp = 0;
	if (errx == _INFCODE || erry == _INFCODE) {
		return _Inf._Double;
	} else if (errx == _NANCODE) {
		return x;
	} else if (erry == _NANCODE) {
		return y;
	} else {	/* finite */
		double z;

		if (x < 0.0)
			x = -x;
		if (y < 0.0)
			y = -y;
		if (y == 0.0) {
			if (x == 0.0)
				return 0.0;
		} else if (x == 0.0 || xexp < yexp || (xexp == yexp && x < y)) {	/* make x the larger */
			const double t = x;
			const short texp = xexp;

			x = y;
			xexp = yexp;
			y = t;
			yexp = texp;
		}
		if ((*pexp = xexp / 4 * 2) == 0)	/* scale x near 1 */
			*pexp = 2;
		_Dscale(&x, xexp - *pexp);
		_Dscale(&y, yexp - *pexp);
		z = x - y;
		if (z == x) {	/* y doesn't matter */
			;
		} else if (y < z) {	/* x > 2y */
			const double qv = x / y;

			z = x + y / (qv + sqrt(1.0 + qv * qv));
		} else {	/* x <= 2y */
			const double qv = z / y;
			const double t = (qv + 2.0) * qv;

			z = x + y / (t / (sqrt(t + 2.0) + sqrt2) + onepsqrt2_lo + qv + onepsqrt2_hi);
		}
		return z;
	}
}
