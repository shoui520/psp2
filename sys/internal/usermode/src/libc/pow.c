/*
 * pow function, and _Pow: x^y as 2^(y log2(x)) by _Exp. x is split into
 * 2^xexp times a mantissa near 0.5 + k / 64, whose log2 the table holds in
 * three parts; the rest of log2(x) comes from the series for
 * ln((1 + z) / (1 - z)). When y log2(x) is beyond +-15 (or |y| beyond 40),
 * the product is formed in extended precision by the _Xp_ functions.
 * _Pow returns the binary exponent in *pex when pex is not null.
 */
#include <math.h>
#include <string.h>

#include "xmath.h"

#define NBUF	4	/* the extended-precision words */

/* the sign bit of a value, from its bits */
#define SIGN(x)	(((union { double _V; unsigned long long _W; }){(x)})._W & 0x8000000000000000ULL)

static double log2tab[][NBUF] = {	/* log2(0.5 + k / 64), in three parts */
	{-1.0, -0.0, -0.0, 0.0},
	{-0.9556058645248413, -1.6116704859570063e-08, -3.941830807715784e-16, 0.0},
	{-0.912537157535553, -1.2141074812177521e-09, -1.3201255313653057e-16, 0.0},
	{-0.8707169592380524, -2.3816980831981027e-08, -3.4454261563606663e-16, 0.0},
	{-0.8300749957561493, -2.8015381126067496e-09, -2.3249358351002566e-16, 0.0},
	{-0.7905466258525848, -8.5184650444603e-09, -3.34814706934647e-16, 0.0},
	{-0.7520724833011627, -3.2552516238126827e-09, -1.6266723136303533e-16, 0.0},
	{-0.714597761631012, -1.950673933492908e-08, -3.603297414384454e-16, 0.0},
	{-0.6780718863010406, -1.881159672478816e-08, -2.7792745803634596e-16, 0.0},
	{-0.6424479782581329, -1.7123783280226235e-08, -9.20374750830192e-17, 0.0},
	{-0.6076825559139252, -2.1307314135299293e-08, -4.0490655758848725e-16, 0.0},
	{-0.573735237121582, -8.176319621355788e-09, -4.0851006735667856e-16, 0.0},
	{-0.5405683517456055, -2.961709721915895e-08, -5.589168078281182e-17, 0.0},
	{-0.5081468820571899, -2.161313528148412e-08, -6.6331831696309e-17, 0.0},
	{-0.4764380156993866, -2.8243600347366282e-08, -1.836598775521042e-16, 0.0},
	{-0.44541114568710327, -2.63525912203022e-09, -2.343145918500591e-16, 0.0},
	{-0.41503748297691345, -1.6301930028106426e-08, -3.382913933713217e-16, 0.0},
	{-0.38529014587402344, -1.0010768214385735e-08, -1.3323032181688623e-16, 0.0},
	{-0.3561438024044037, -7.820871505970217e-09, -1.117657062226293e-16, 0.0},
	{-0.32757464051246643, -1.7516037953413388e-08, -2.6214743275234536e-17, 0.0},
	{-0.2995602786540985, -3.2048093068226535e-09, -2.20383414496788e-17, 0.0},
	{-0.2720795273780823, -1.8058718520563843e-08, -2.4764748600988282e-17, 0.0},
	{-0.24511247873306274, -1.9103468584802386e-08, -1.2669576703128473e-16, 0.0},
	{-0.2186402678489685, -1.8626371556251797e-08, -3.338191388191578e-16, 0.0},
	{-0.19264507293701172, -5.005384107192867e-09, -6.661515759972067e-17, 0.0},
	{-0.16710996627807617, -1.955718209600832e-08, -5.68694215017393e-17, 0.0},
	{-0.14201900362968445, -1.2427432416473039e-09, -1.8939073041965938e-16, 0.0},
	{-0.11735692620277405, -2.4435384382570646e-08, -3.107703797076332e-16, 0.0},
	{-0.09310939908027649, -5.311204809288483e-09, -1.7212964155760503e-16, 0.0},
	{-0.06926265358924866, -8.847864663152905e-09, -4.0314832894822414e-16, 0.0},
	{-0.04580366611480713, -2.3498317514025757e-08, -1.4826186776053455e-16, 0.0},
	{-0.02272006869316101, -7.806922219799617e-09, -2.991087477271912e-16, 0.0},
	{-0.0, -0.0, -0.0, 0.0},
};
static const double invln2[NBUF] = {	/* 1 / ln(2), in four parts */
	1.4426950216293335, 1.92596298909109e-08, 2.0355267131149805e-17,
	6.6097812153376124e-24,
};

double _Pow(double x, double y, short *pex)
{
	static const double ln2 = 0.6931471805599453;
	short xexp;
	double yi = y;
	double z;
	const short errx = _Dunscale(&xexp, &x);
	const short erry = _Dint(&yi, 0);
	const double *pt;
	double xm;
	double xr;
	int n;
	short neg;

	if (pex != 0)
		*pex = 0;
	if ((erry == 0 && y == 0.0)
	    || (errx < 0 && xexp == 1 && (x == 0.5 || (erry == _INFCODE && x == -0.5))))
		return 1.0;	/* x^0, 1^y, -1^INF */
	if (0 <= errx || 0 < erry) {	/* x is 0, INF or NaN, or y is INF or NaN */
		if (errx == _NANCODE)
			return x;
		else if (erry == _NANCODE)
			return y;
		else if (errx == _INFCODE)	/* x == +-INF */
			return !SIGN(x) ? (!SIGN(y) ? _Inf._Double : 0.0)
				: !SIGN(y) ? (erry == 0 && _Dint(&yi, -1) < 0
					? -_Inf._Double : _Inf._Double)
				: erry == 0 && _Dint(&yi, -1) < 0 ? -_Zero : 0.0;
		else if (erry == _INFCODE)	/* y == +-INF */
			return !SIGN(y) ? (0 < xexp ? _Inf._Double : 0.0)
				: 0 < xexp ? 0.0 : _Inf._Double;
		else if (!SIGN(y))	/* x == 0, y > 0 */
			return erry == 0 && _Dint(&yi, -1) < 0 && SIGN(x) ? -_Zero : 0.0;
		_Feraise(_FE_DIVBYZERO);	/* x == 0, y < 0 */
		return erry == 0 && _Dint(&yi, -1) < 0 && SIGN(x)
			? -_Inf._Double : _Inf._Double;
	}

	if (SIGN(x) && erry < 0) {	/* negative x, nonintegral y */
		_Feraise(_FE_INVALID);
		return _Nan._Double;
	}
	neg = 0;
	if (0.0 < x) {
		;
	} else {	/* odd y makes the result negative */
		x = -x;
		neg = _Dint(&yi, -1);
	}
	if (xexp == 1 && x < 0.7071067811865476) {	/* below sqrt(0.5): use 2x */
		pt = log2tab[0];
		xm = 1.0;
		x *= 2.0;
	} else {	/* the nearest 0.5 + k / 64 */
		const int k = (int)(x * 64.0 + 0.5) - 32;

		pt = log2tab[k];
		xm = k * 0.015625 + 0.5;
	}
	{
		const double w = (x - xm) / (xm + x);
		const double xm1 = 1.0 - xm;
		const double w2 = w * w;
		double series;

		x -= xm;
		series = w * (w2 * _Logpoly(w2) - x + xm1 * 2.0);
		yi = (x + series) / ln2;
		yi = *pt + xexp + yi;
		z = y * yi;
		if (z < -32768.0)
			goto underflow;
		if (-15.0 <= z && z <= 15.0 && -40.0 < y && y < 40.0) {	/* in double */
			xr = z < 0.0 ? z - 0.5 : z + 0.5;
			if (pt == 0) {
				z = y * (x + series) + (y * xexp - (int)xr) * ln2;
			} else {
				z = y * (x + series);
				z += y * (pt[1] + pt[2]) * ln2;
				z += (y * (xexp + pt[0]) - (int)xr) * ln2;
			}
		} else if (32768.0 < z) {
			goto overflow;
		} else {	/* in extended precision */
			double ws[2] = {0.0};
			double wx[NBUF] = {0.0};
			double wl[NBUF] = {0.0};
			double wz[NBUF] = {0.0};
			int i;

			_Xp_setw(wx, NBUF, x);
			_Xp_setw(ws, 2, series);
			_Xp_addx(wx, NBUF, ws, 2);	/* ln(x / xm) */
			if (wx[0] == 0.0) {
				_Xp_setw(wl, NBUF, 0.0);
			} else {	/* times 1 / ln(2) */
				memcpy(wl, invln2, sizeof (wl));
				_Xp_mulh(wl, NBUF, wx[0]);
				for (i = 1; i < NBUF && wx[i] != 0.0; ++i) {
					double wt[NBUF];

					memcpy(wt, invln2, sizeof (wt));
					_Xp_mulh(wt, NBUF, wx[i]);
					_Xp_addx(wl, NBUF, wt, NBUF);
				}
			}
			if (pt != 0)
				_Xp_addx(wl, NBUF, pt, 3);
			_Xp_addh(wl, NBUF, xexp);	/* log2(x) */
			_Xp_setw(wx, 2, y);
			memcpy(wz, wl, sizeof (wz));
			_Xp_mulh(wz, NBUF, wx[0]);
			if (wx[1] != 0.0) {
				double wt[NBUF];

				memcpy(wt, wl, sizeof (wt));
				_Xp_mulh(wt, NBUF, wx[1]);
				_Xp_addx(wz, NBUF, wt, NBUF);
			}
			x = wz[0] + wz[1] + wz[2];	/* the integer part of y log2(x) */
			_Dint(&x, 0);
			_Xp_addh(wz, NBUF, -x);
			z = _Xp_getw(wz, NBUF) * ln2;
			xr = x;
		}
	}
	n = (int)xr;
	if (pex != 0) {
		*pex = n;
		n = 0;
	}
	switch (_Exp(&z, 1.0, n)) {
	case 0:
underflow:
		z = 0.0;
		_Feraise(_FE_UNDERFLOW);
		break;
	case _INFCODE:
overflow:
		z = _Inf._Double;
		_Feraise(_FE_OVERFLOW);
		break;
	}
	return neg ? -z : z;
}

double pow(double x, double y)
{
	return _Pow(x, y, 0);
}
