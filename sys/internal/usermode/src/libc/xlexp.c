/*
 * _LExp function: y times e^*px, times 2^eoff, into *px; return what kind of
 * value it is. A rational approximation of e^g for g in [-ln2/2, ln2/2],
 * scaled by 2^n where *px = n ln2 + g.
 */
#include <math.h>

#include "xmath.h"

static const long double p[] = {	/* the even terms' coefficients, highest first */
	0.0009917323526335046L, 0.11110779924116565L,
};
static const long double q[] = {	/* the odd terms' */
	3.304120783105597e-05L, 0.01388723295391838L, 0.5L,
};
static const long double c1 = 0.6931471675634384L;	/* ln 2, its high bits */
static const long double c2 = 1.2996506893889889e-08L;	/* and its low ones */
static const long double hugexp = 29490.0L;	/* e^hugexp overflows any finite y */
static const long double invln2 = 1.4426950408889634L;	/* 1 / ln 2 */

short _LExp(long double *px, long double y, long eoff)
{
	short ret;
	const short errx = _LDtest(px);
	const short erry = _LDtest(&y);

	if (errx >= 0 || erry >= 0) {	/* a zero, an infinity or a NaN */
		if (errx == _NANCODE) {
			ret = _NANCODE;
		} else if (erry == _NANCODE) {
			*px = y;
			ret = _NANCODE;
		} else if (erry == 0) {
			if (*px == _LInf._Long_double) {	/* 0 times e^INF */
				_Feraise(_FE_INVALID);
				*px = _LNan._Long_double;
				ret = _NANCODE;
			} else {
				*px = y;
				ret = 0;
			}
		} else if (erry == _INFCODE) {
			if (*px == -_LInf._Long_double) {	/* INF times e^-INF */
				_Feraise(_FE_INVALID);
				*px = _LNan._Long_double;
				ret = _NANCODE;
			} else {
				*px = y;
				ret = _INFCODE;
			}
		} else if (errx == 0) {	/* e^0 */
			*px = y;
			switch (ret = _LDscale(px, eoff)) {
			case 0:
				_Feraise(_FE_UNDERFLOW);
				break;
			case _INFCODE:
				_Feraise(_FE_OVERFLOW);
				break;
			}
		} else if (*px == _LInf._Long_double) {
			*px = *px * y;
			ret = _INFCODE;
		} else {	/* e^-INF */
			*px = y * 0.0L;
			ret = 0;
		}
	} else if (*px < -hugexp) {	/* certain underflow */
		*px = y * 0.0L;
		ret = 0;
	} else if (hugexp < *px) {	/* certain overflow */
		*px = _LInf._Long_double * y;
		ret = _INFCODE;
	} else {	/* the exponent won't overflow */
		long double g = *px * invln2;
		const long xexp = (long)(g + (g < 0.0L ? -0.5L : 0.5L));

		g = xexp;
		g = (*px - g * c1) - g * c2;
		if (-_LEps._Long_double < g && g < _LEps._Long_double) {
			*px = y;
		} else {	/* g*g worth computing */
			const long double z = g * g;
			const long double w = z * (p[0] * z + p[1]);

			g *= (q[0] * z + q[1]) * z + q[2];
			*px = (w + g + 1.0L) / (w - g + 1.0L) * y;
		}
		switch (ret = _LDscale(px, eoff + xexp)) {
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
