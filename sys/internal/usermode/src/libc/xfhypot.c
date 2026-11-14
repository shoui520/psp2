/*
 * _FHypot function: sqrt(x^2 + y^2) in float, scaled by 2^-*pexp so that it
 * cannot overflow; hypotf scales it back.
 */
#include <math.h>

#include "xmath.h"

static const float sqrt2 = 1.4142135F;
static const float onepsqrt2_hi = 2.4142F;	/* 1 + sqrt(2), its high part */
static const float onepsqrt2_lo = 1.3562373e-05F;	/* and its low part */

float _FHypot(float x, float y, int *pexp)
{
	short xexp;
	short yexp;
	const short errx = _FDunscale(&xexp, &x);
	const short erry = _FDunscale(&yexp, &y);

	*pexp = 0;
	if (errx == _INFCODE || erry == _INFCODE) {
		return _FInf._Float;
	} else if (errx == _NANCODE) {
		return x;
	} else if (erry == _NANCODE) {
		return y;
	} else {	/* finite */
		float z;

		if (x < 0.0F)
			x = -x;
		if (y < 0.0F)
			y = -y;
		if (y == 0.0F) {
			if (x == 0.0F)
				return 0.0F;
		} else if (x == 0.0F || xexp < yexp || (xexp == yexp && x < y)) {	/* make x the larger */
			const float t = x;
			const short texp = xexp;

			x = y;
			xexp = yexp;
			y = t;
			yexp = texp;
		}
		if ((*pexp = xexp / 4 * 2) == 0)	/* scale x near 1 */
			*pexp = 2;
		_FDscale(&x, xexp - *pexp);
		_FDscale(&y, yexp - *pexp);
		z = x - y;
		if (z == x) {	/* y doesn't matter */
			;
		} else if (y < z) {	/* x > 2y */
			const float qv = x / y;

			z = x + y / (qv + sqrtf(1.0F + qv * qv));
		} else {	/* x <= 2y */
			const float qv = z / y;
			const float t = (qv + 2.0F) * qv;

			z = x + y / (t / (sqrtf(t + 2.0F) + sqrt2) + onepsqrt2_lo + qv + onepsqrt2_hi);
		}
		return z;
	}
}
