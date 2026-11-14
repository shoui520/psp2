/*
 * _FExp function: y times e^*px, times 2^eoff, into *px; return what kind of
 * value it is, in float. A rational approximation of e^g for g in [-ln2/2, ln2/2],
 * scaled by 2^n where *px = n ln2 + g.
 */
#include <math.h>

#include "xmath.h"

static const float p[] = {	/* the even terms' coefficients, highest first */
	0.099974856F,
};
static const float q[] = {	/* the odd terms' */
	0.008320826F, 0.5F,
};
static const float c1 = 0.6928711F;	/* ln 2, its high bits */
static const float c2 = 0.00027608682F;	/* and its low ones */
static const float hugexp = 229.0F;	/* e^hugexp overflows any finite y */
static const float invln2 = 1.442695F;	/* 1 / ln 2 */

short _FExp(float *px, float y, long eoff)
{
	short ret;
	const short errx = _FDtest(px);
	const short erry = _FDtest(&y);

	if (errx >= 0 || erry >= 0) {	/* a zero, an infinity or a NaN */
		if (errx == _NANCODE) {
			ret = _NANCODE;
		} else if (erry == _NANCODE) {
			*px = y;
			ret = _NANCODE;
		} else if (erry == 0) {
			if (*px == _FInf._Float) {	/* 0 times e^INF */
				_Feraise(_FE_INVALID);
				*px = _FNan._Float;
				ret = _NANCODE;
			} else {
				*px = y;
				ret = 0;
			}
		} else if (erry == _INFCODE) {
			if (*px == -_FInf._Float) {	/* INF times e^-INF */
				_Feraise(_FE_INVALID);
				*px = _FNan._Float;
				ret = _NANCODE;
			} else {
				*px = y;
				ret = _INFCODE;
			}
		} else if (errx == 0) {	/* e^0 */
			*px = y;
			switch (ret = _FDscale(px, eoff)) {
			case 0:
				_Feraise(_FE_UNDERFLOW);
				break;
			case _INFCODE:
				_Feraise(_FE_OVERFLOW);
				break;
			}
		} else if (*px == _FInf._Float) {
			*px = *px * y;
			ret = _INFCODE;
		} else {	/* e^-INF */
			*px = y * 0.0F;
			ret = 0;
		}
	} else if (*px < -hugexp) {	/* certain underflow */
		*px = y * 0.0F;
		ret = 0;
	} else if (hugexp < *px) {	/* certain overflow */
		*px = _FInf._Float * y;
		ret = _INFCODE;
	} else {	/* the exponent won't overflow */
		float g = *px * invln2;
		const long xexp = (long)(g + (g < 0.0F ? -0.5F : 0.5F));

		g = xexp;
		g = (*px - g * c1) - g * c2;
		if (-_FEps._Float < g && g < _FEps._Float) {
			*px = y;
		} else {	/* g*g worth computing */
			const float z = g * g;
			const float w = z * p[0];

			g *= q[0] * z + q[1];
			*px = (w + g + 1.0F) / (w - g + 1.0F) * y;
		}
		switch (ret = _FDscale(px, eoff + xexp)) {
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
