/*
 * _LHypot function: sqrt(x^2 + y^2) in long double (a double on psp2),
 * scaled by 2^-*pexp so that it cannot overflow; hypotl scales it back.
 */
#include <math.h>

#include "xmath.h"

static const long double sqrt2 = 1.4142135623730951L;
static const long double onepsqrt2_hi = 2.4142L;	/* 1 + sqrt(2), its high part */
static const long double onepsqrt2_lo = 1.3562373095048801e-05L;	/* and its low part */

long double _LHypot(long double x, long double y, int *pexp)
{
	short xexp;
	short yexp;
	const short errx = _LDunscale(&xexp, &x);
	const short erry = _LDunscale(&yexp, &y);

	*pexp = 0;
	if (errx == _INFCODE || erry == _INFCODE) {
		return _LInf._Long_double;
	} else if (errx == _NANCODE) {
		return x;
	} else if (erry == _NANCODE) {
		return y;
	} else {	/* finite */
		long double z;

		if (x < 0.0L)
			x = -x;
		if (y < 0.0L)
			y = -y;
		if (y == 0.0L) {
			if (x == 0.0L)
				return 0.0L;
		} else if (x == 0.0L || xexp < yexp || (xexp == yexp && x < y)) {	/* make x the larger */
			const long double t = x;
			const short texp = xexp;

			x = y;
			xexp = yexp;
			y = t;
			yexp = texp;
		}
		if ((*pexp = xexp / 4 * 2) == 0)	/* scale x near 1 */
			*pexp = 2;
		_LDscale(&x, xexp - *pexp);
		_LDscale(&y, yexp - *pexp);
		z = x - y;
		if (z == x) {	/* y doesn't matter */
			;
		} else if (y < z) {	/* x > 2y */
			const long double qv = x / y;

			z = x + y / (qv + sqrtl(1.0L + qv * qv));
		} else {	/* x <= 2y */
			const long double qv = z / y;
			const long double t = (qv + 2.0L) * qv;

			z = x + y / (t / (sqrtl(t + 2.0L) + sqrt2) + onepsqrt2_lo + qv + onepsqrt2_hi);
		}
		return z;
	}
}
