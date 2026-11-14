/*
 * _Exp function: y times e^*px, times 2^eoff, into *px; return what kind of
 * value it is. A rational approximation of e^g for g in [-ln2/2, ln2/2],
 * scaled by 2^n where *px = n ln2 + g.
 */
#include <math.h>

#include "xmath.h"

static const double p[] = {	/* the even terms' coefficients, highest first */
	0.0009917323526335046, 0.11110779924116565,
};
static const double q[] = {	/* the odd terms' */
	3.304120783105597e-05, 0.01388723295391838, 0.5,
};
static const double c1 = 0.6931471675634384;	/* ln 2, its high bits */
static const double c2 = 1.2996506893889889e-08;	/* and its low ones */
static const double hugexp = 1842.0;	/* e^hugexp overflows any finite y */
static const double invln2 = 1.4426950408889634;	/* 1 / ln 2 */

short _Exp(double *px, double y, long eoff)
{
	short ret;
	const short errx = _Dtest(px);
	const short erry = _Dtest(&y);

	if (errx >= 0 || erry >= 0) {	/* a zero, an infinity or a NaN */
		if (errx == _NANCODE) {
			ret = _NANCODE;
		} else if (erry == _NANCODE) {
			*px = y;
			ret = _NANCODE;
		} else if (erry == 0) {
			if (*px == _Inf._Double) {	/* 0 times e^INF */
				_Feraise(_FE_INVALID);
				*px = _Nan._Double;
				ret = _NANCODE;
			} else {
				*px = y;
				ret = 0;
			}
		} else if (erry == _INFCODE) {
			if (*px == -_Inf._Double) {	/* INF times e^-INF */
				_Feraise(_FE_INVALID);
				*px = _Nan._Double;
				ret = _NANCODE;
			} else {
				*px = y;
				ret = _INFCODE;
			}
		} else if (errx == 0) {	/* e^0 */
			*px = y;
			switch (ret = _Dscale(px, eoff)) {
			case 0:
				_Feraise(_FE_UNDERFLOW);
				break;
			case _INFCODE:
				_Feraise(_FE_OVERFLOW);
				break;
			}
		} else if (*px == _Inf._Double) {
			*px = *px * y;
			ret = _INFCODE;
		} else {	/* e^-INF */
			*px = y * 0.0;
			ret = 0;
		}
	} else if (*px < -hugexp) {	/* certain underflow */
		*px = y * 0.0;
		ret = 0;
	} else if (hugexp < *px) {	/* certain overflow */
		*px = _Inf._Double * y;
		ret = _INFCODE;
	} else {	/* the exponent won't overflow */
		double g = *px * invln2;
		const long xexp = (long)(g + (g < 0.0 ? -0.5 : 0.5));

		g = xexp;
		g = (*px - g * c1) - g * c2;
		if (-_Eps._Double < g && g < _Eps._Double) {
			*px = y;
		} else {	/* g*g worth computing */
			const double z = g * g;
			const double w = z * (p[0] * z + p[1]);

			g *= (q[0] * z + q[1]) * z + q[2];
			*px = (w + g + 1.0) / (w - g + 1.0) * y;
		}
		switch (ret = _Dscale(px, eoff + xexp)) {
		case 0:
			_Feraise(_FE_UNDERFLOW);
			break;
		case _INFCODE:
			_Feraise(_FE_OVERFLOW);
			break;
		}
	}
	return ret;
}
