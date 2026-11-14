/*
 * _Log function: the natural (baseflag 0), base-2 (baseflag < 0) or base-10
 * (0 < baseflag) logarithm of x in double, from the fraction f in
 * [sqrt(1/2), sqrt(2)) and the binary exponent n: ln f by a series in
 * z = (f - 1)/(f + 1), plus n ln 2, each constant split high and low.
 * _Logpoly evaluates that series for _Pow.
 */
#include <math.h>

#include "xmath.h"

static const double p[] = {	/* the series' coefficients, highest first */
	0.14821919759901167, 0.15310988435285788, 0.1818370639390095, 0.22222195289721308,
	0.285714287816195, 0.399999999991905, 0.6666666666666781,
};
static const double ln2hi = 0.6931470632553101;	/* ln 2 */
static const double ln2lo = 1.1730463525082348e-07;
static const double log2hi = 0.30102992057800293;	/* log10 2 */
static const double log2lo = 7.508597826552624e-08;
static const double logehi = 0.4342944622039795;	/* log10 e */
static const double logelo = 1.9699272335463627e-08;
static const double loge = 0.4342944819032518;
static const double lgehi = 1.4426950216293335;	/* log2 e */
static const double lgelo = 1.9259629911266175e-08;
static const double lge = 1.4426950408889634;
static const double rthalf = 0.7071067811865476;	/* sqrt(1/2) */

double _Log(double x, int baseflag)
{
	short xexp;

	switch (_Dunscale(&xexp, &x)) {
	case _NANCODE:
		return x;
	case 0:	/* a pole */
		_Feraise(_FE_DIVBYZERO);
		return -_Inf._Double;
	case _INFCODE:
		if (!DSIGN(x))
			return x;
		/* fall through */
	default:
		if (DSIGN(x)) {	/* a domain error */
			_Feraise(_FE_INVALID);
			return _Nan._Double;
		} else {	/* 0.5 <= x < 1 */
			double x1;
			double xn;
			double z;
			double w;

			if (x < rthalf) {	/* x in [sqrt(1/2), sqrt(2)) */
				x += x;
				--xexp;
			}
			x1 = x - 1.0;
			z = x1 / (x + 1.0);
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

double _Logpoly(double w)
{
	return (((((p[0] * w + p[1]) * w + p[2]) * w + p[3]) * w + p[4]) * w + p[5]) * w + p[6];
}
