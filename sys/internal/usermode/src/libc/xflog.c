/*
 * _FLog function: the natural (baseflag 0), base-2 (baseflag < 0) or base-10
 * (0 < baseflag) logarithm of x in float, from the fraction f in
 * [sqrt(1/2), sqrt(2)) and the binary exponent n: ln f by a series in
 * z = (f - 1)/(f + 1), plus n ln 2, each constant split high and low.
 * _FLogpoly evaluates that series for _FPow.
 */
#include <math.h>

#include "xmath.h"

static const float p[] = {	/* the series' coefficients, highest first */
	0.298844F, 0.39976552F, 0.66666794F,
};
static const float ln2hi = 0.69314706F;	/* ln 2 */
static const float ln2lo = 1.1730464e-07F;
static const float log2hi = 0.30102992F;	/* log10 2 */
static const float log2lo = 7.508598e-08F;
static const float logehi = 0.43429446F;	/* log10 e */
static const float logelo = 1.9699272e-08F;
static const float loge = 0.4342945F;
static const float lgehi = 1.442695F;	/* log2 e */
static const float lgelo = 1.925963e-08F;
static const float lge = 1.442695F;
static const float rthalf = 0.70710677F;	/* sqrt(1/2) */

float _FLog(float x, int baseflag)
{
	short xexp;

	switch (_FDunscale(&xexp, &x)) {
	case _NANCODE:
		return x;
	case 0:	/* a pole */
		_Feraise(_FE_DIVBYZERO);
		return -_FInf._Float;
	case _INFCODE:
		if (!FSIGN(x))
			return x;
		/* fall through */
	default:
		if (FSIGN(x)) {	/* a domain error */
			_Feraise(_FE_INVALID);
			return _FNan._Float;
		} else {	/* 0.5 <= x < 1 */
			float x1;
			float xn;
			float z;
			float w;

			if (x < rthalf) {	/* x in [sqrt(1/2), sqrt(2)) */
				x += x;
				--xexp;
			}
			x1 = x - 1.0F;
			z = x1 / (x + 1.0F);
			w = z * z;
			xn = xexp;
			z *= x1 - w * ((p[0] * w + p[1]) * w + p[2]);	/* x1 - ln x */
			if (baseflag == 0)
				return (x1 - (z - xn * ln2lo)) + xn * ln2hi;
			else if (baseflag < 0)
				return ((x1 * lgelo - z * lge) + x1 * lgehi) + xn;
			else
				return ((x1 * logelo - (z * loge - xn * log2lo)) + x1 * logehi)
				       + xn * log2hi;
		}
	}
}

float _FLogpoly(float w)
{
	return (p[0] * w + p[1]) * w + p[2];
}
