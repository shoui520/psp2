/*
 * powl function, and _LPow: x^y as 2^(y log2(x)) by _LExp. x is split into
 * 2^xexp times a mantissa near 0.5 + k / 64, whose log2 the table holds in
 * three parts; the rest of log2(x) comes from the series for
 * ln((1 + z) / (1 - z)). When y log2(x) is beyond +-15 (or |y| beyond 40),
 * the product is formed in extended precision by the _LXp_ functions.
 * _LPow returns the binary exponent in *pex when pex is not null.
 */
#include <math.h>
#include <string.h>

#include "xmath.h"

#define NBUF	4	/* the extended-precision words */


static long double log2tab[][NBUF] = {	/* log2(0.5 + k / 64), in three parts */
	{-1.0L, -0.0L, -0.0L, 0.0L},
	{-0.9556058645248413L, -1.6116704859570063e-08L, -3.941830807715784e-16L, 0.0L},
	{-0.912537157535553L, -1.2141074812177521e-09L, -1.3201255313653057e-16L, 0.0L},
	{-0.8707169592380524L, -2.3816980831981027e-08L, -3.4454261563606663e-16L, 0.0L},
	{-0.8300749957561493L, -2.8015381126067496e-09L, -2.3249358351002566e-16L, 0.0L},
	{-0.7905466258525848L, -8.5184650444603e-09L, -3.34814706934647e-16L, 0.0L},
	{-0.7520724833011627L, -3.2552516238126827e-09L, -1.6266723136303533e-16L, 0.0L},
	{-0.714597761631012L, -1.950673933492908e-08L, -3.603297414384454e-16L, 0.0L},
	{-0.6780718863010406L, -1.881159672478816e-08L, -2.7792745803634596e-16L, 0.0L},
	{-0.6424479782581329L, -1.7123783280226235e-08L, -9.20374750830192e-17L, 0.0L},
	{-0.6076825559139252L, -2.1307314135299293e-08L, -4.0490655758848725e-16L, 0.0L},
	{-0.573735237121582L, -8.176319621355788e-09L, -4.0851006735667856e-16L, 0.0L},
	{-0.5405683517456055L, -2.961709721915895e-08L, -5.589168078281182e-17L, 0.0L},
	{-0.5081468820571899L, -2.161313528148412e-08L, -6.6331831696309e-17L, 0.0L},
	{-0.4764380156993866L, -2.8243600347366282e-08L, -1.836598775521042e-16L, 0.0L},
	{-0.44541114568710327L, -2.63525912203022e-09L, -2.343145918500591e-16L, 0.0L},
	{-0.41503748297691345L, -1.6301930028106426e-08L, -3.382913933713217e-16L, 0.0L},
	{-0.38529014587402344L, -1.0010768214385735e-08L, -1.3323032181688623e-16L, 0.0L},
	{-0.3561438024044037L, -7.820871505970217e-09L, -1.117657062226293e-16L, 0.0L},
	{-0.32757464051246643L, -1.7516037953413388e-08L, -2.6214743275234536e-17L, 0.0L},
	{-0.2995602786540985L, -3.2048093068226535e-09L, -2.20383414496788e-17L, 0.0L},
	{-0.2720795273780823L, -1.8058718520563843e-08L, -2.4764748600988282e-17L, 0.0L},
	{-0.24511247873306274L, -1.9103468584802386e-08L, -1.2669576703128473e-16L, 0.0L},
	{-0.2186402678489685L, -1.8626371556251797e-08L, -3.338191388191578e-16L, 0.0L},
	{-0.19264507293701172L, -5.005384107192867e-09L, -6.661515759972067e-17L, 0.0L},
	{-0.16710996627807617L, -1.955718209600832e-08L, -5.68694215017393e-17L, 0.0L},
	{-0.14201900362968445L, -1.2427432416473039e-09L, -1.8939073041965938e-16L, 0.0L},
	{-0.11735692620277405L, -2.4435384382570646e-08L, -3.107703797076332e-16L, 0.0L},
	{-0.09310939908027649L, -5.311204809288483e-09L, -1.7212964155760503e-16L, 0.0L},
	{-0.06926265358924866L, -8.847864663152905e-09L, -4.0314832894822414e-16L, 0.0L},
	{-0.04580366611480713L, -2.3498317514025757e-08L, -1.4826186776053455e-16L, 0.0L},
	{-0.02272006869316101L, -7.806922219799617e-09L, -2.991087477271912e-16L, 0.0L},
	{-0.0L, -0.0L, -0.0L, 0.0L},
};
static const long double invln2[NBUF] = {	/* 1 / ln(2), in four parts */
	1.4426950216293335L, 1.92596298909109e-08L, 2.0355267131149805e-17L,
	6.6097812153376124e-24L,
};

long double _LPow(long double x, long double y, short *pex)
{
	static const long double ln2 = 0.6931471805599453L;
	short xexp;
	long double yi = y;
	long double z;
	const short errx = _LDunscale(&xexp, &x);
	const short erry = _LDint(&yi, 0);
	const long double *pt;
	long double xm;
	long double xr;
	int n;
	short neg;

	if (pex != 0)
		*pex = 0;
	if ((erry == 0 && y == 0.0L)
	    || (errx < 0 && xexp == 1 && (x == 0.5L || (erry == _INFCODE && x == -0.5L))))
		return 1.0L;	/* x^0, 1^y, -1^INF */
	if (0 <= errx || 0 < erry) {	/* x is 0, INF or NaN, or y is INF or NaN */
		if (errx == _NANCODE)
			return x;
		else if (erry == _NANCODE)
			return y;
		else if (errx == _INFCODE)	/* x == +-INF */
			return !LSIGN(x) ? (!LSIGN(y) ? _LInf._Long_double : 0.0L)
				: !LSIGN(y) ? (erry == 0 && _LDint(&yi, -1) < 0
					? -_LInf._Long_double : _LInf._Long_double)
				: erry == 0 && _LDint(&yi, -1) < 0 ? -_LZero : 0.0L;
		else if (erry == _INFCODE)	/* y == +-INF */
			return !LSIGN(y) ? (0 < xexp ? _LInf._Long_double : 0.0L)
				: 0 < xexp ? 0.0L : _LInf._Long_double;
		else if (!LSIGN(y))	/* x == 0, y > 0 */
			return erry == 0 && _LDint(&yi, -1) < 0 && LSIGN(x) ? -_LZero : 0.0L;
		_Feraise(_FE_DIVBYZERO);	/* x == 0, y < 0 */
		return erry == 0 && _LDint(&yi, -1) < 0 && LSIGN(x)
			? -_LInf._Long_double : _LInf._Long_double;
	}

	if (LSIGN(x) && erry < 0) {	/* negative x, nonintegral y */
		_Feraise(_FE_INVALID);
		return _LNan._Long_double;
	}
	neg = 0;
	if (0.0L < x) {
		;
	} else {	/* odd y makes the result negative */
		x = -x;
		neg = _LDint(&yi, -1);
	}
	if (xexp == 1 && x < 0.7071067811865476L) {	/* below sqrtl(0.5): use 2x */
		pt = log2tab[0];
		xm = 1.0L;
		x *= 2.0L;
	} else {	/* the nearest 0.5 + k / 64 */
		const int k = (int)(x * 64.0L + 0.5L) - 32;

		pt = log2tab[k];
		xm = k * 0.015625L + 0.5L;
	}
	{
		const long double w = (x - xm) / (xm + x);
		const long double xm1 = 1.0L - xm;
		const long double w2 = w * w;
		long double series;

		x -= xm;
		series = w * (w2 * _LLogpoly(w2) - x + xm1 * 2.0L);
		yi = (x + series) / ln2;
		yi = *pt + xexp + yi;
		z = y * yi;
		if (z < -32768.0L)
			goto underflow;
		if (-15.0L <= z && z <= 15.0L && -40.0L < y && y < 40.0L) {	/* in long double (a double on psp2) */
			xr = z < 0.0L ? z - 0.5L : z + 0.5L;
			if (pt == 0) {
				z = y * (x + series) + (y * xexp - (int)xr) * ln2;
			} else {
				z = y * (x + series);
				z += y * (pt[1] + pt[2]) * ln2;
				z += (y * (xexp + pt[0]) - (int)xr) * ln2;
			}
		} else if (32768.0L < z) {
			goto overflow;
		} else {	/* in extended precision */
			long double ws[2] = {0.0L};
			long double wx[NBUF] = {0.0L};
			long double wl[NBUF] = {0.0L};
			long double wz[NBUF] = {0.0L};
			int i;

			_LXp_setw(wx, NBUF, x);
			_LXp_setw(ws, 2, series);
			_LXp_addx(wx, NBUF, ws, 2);	/* ln(x / xm) */
			if (wx[0] == 0.0L) {
				_LXp_setw(wl, NBUF, 0.0L);
			} else {	/* times 1 / ln(2) */
				memcpy(wl, invln2, sizeof (wl));
				_LXp_mulh(wl, NBUF, wx[0]);
				for (i = 1; i < NBUF && wx[i] != 0.0L; ++i) {
					long double wt[NBUF];

					memcpy(wt, invln2, sizeof (wt));
					_LXp_mulh(wt, NBUF, wx[i]);
					_LXp_addx(wl, NBUF, wt, NBUF);
				}
			}
			if (pt != 0)
				_LXp_addx(wl, NBUF, pt, 3);
			_LXp_addh(wl, NBUF, xexp);	/* log2(x) */
			_LXp_setw(wx, 2, y);
			memcpy(wz, wl, sizeof (wz));
			_LXp_mulh(wz, NBUF, wx[0]);
			if (wx[1] != 0.0L) {
				long double wt[NBUF];

				memcpy(wt, wl, sizeof (wt));
				_LXp_mulh(wt, NBUF, wx[1]);
				_LXp_addx(wz, NBUF, wt, NBUF);
			}
			x = wz[0] + wz[1] + wz[2];	/* the integer part of y log2(x) */
			_LDint(&x, 0);
			_LXp_addh(wz, NBUF, -x);
			z = _LXp_getw(wz, NBUF) * ln2;
			xr = x;
		}
	}
	n = (int)xr;
	if (pex != 0) {
		*pex = n;
		n = 0;
	}
	switch (_LExp(&z, 1.0L, n)) {
	case 0:
underflow:
		z = 0.0L;
		_Feraise(_FE_UNDERFLOW);
		break;
	case _INFCODE:
overflow:
		z = _LInf._Long_double;
		_Feraise(_FE_OVERFLOW);
		break;
	}
	return neg ? -z : z;
}

long double powl(long double x, long double y)
{
	return _LPow(x, y, 0);
}
