/*
 * _LLog function: the natural (baseflag 0), base-2 (baseflag < 0) or base-10
 * (0 < baseflag) logarithm of x in long double (a double on psp2), from
 * the fraction f in [sqrt(1/2), sqrt(2)) and the binary exponent n: ln f by a series in
 * z = (f - 1)/(f + 1), plus n ln 2, each constant split high and low.
 * _LLogpoly evaluates that series for _LPow.
 */
#include <math.h>

#include "xmath.h"

static const long double p[] = {	/* the series' coefficients, highest first */
	0.14821919759901167L, 0.15310988435285788L, 0.1818370639390095L, 0.22222195289721308L,
	0.285714287816195L, 0.399999999991905L, 0.6666666666666781L,
};
static const long double ln2hi = 0.6931470632553101L;	/* ln 2 */
static const long double ln2lo = 1.1730463525082348e-07L;
static const long double log2hi = 0.30102992057800293L;	/* log10 2 */
static const long double log2lo = 7.508597826552624e-08L;
static const long double logehi = 0.4342944622039795L;	/* log10 e */
static const long double logelo = 1.9699272335463627e-08L;
static const long double loge = 0.4342944819032518L;
static const long double lgehi = 1.4426950216293335L;	/* log2 e */
static const long double lgelo = 1.9259629911266175e-08L;
static const long double lge = 1.4426950408889634L;
static const long double rthalf = 0.7071067811865476L;	/* sqrt(1/2) */

long double _LLog(long double x, int baseflag)
{
	short xexp;

	switch (_LDunscale(&xexp, &x)) {
	case _NANCODE:
		return x;
	case 0:	/* a pole */
		_Feraise(_FE_DIVBYZERO);
		return -_LInf._Long_double;
	case _INFCODE:
		if (!LSIGN(x))
			return x;
		/* fall through */
	default:
		if (LSIGN(x)) {	/* a domain error */
			_Feraise(_FE_INVALID);
			return _LNan._Long_double;
		} else {	/* 0.5 <= x < 1 */
			long double x1;
			long double xn;
			long double z;
			long double w;

			if (x < rthalf) {	/* x in [sqrt(1/2), sqrt(2)) */
				x += x;
				--xexp;
			}
			x1 = x - 1.0L;
			z = x1 / (x + 1.0L);
			w = z * z;
			xn = xexp;
			z *= x1 - w * ((((((p[0] * w + p[1]) * w + p[2]) * w + p[3]) * w + p[4]) * w + p[5]) * w + p[6]);	/* x1 - ln x */
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

long double _LLogpoly(long double w)
{
	return (((((p[0] * w + p[1]) * w + p[2]) * w + p[3]) * w + p[4]) * w + p[5]) * w + p[6];
}
