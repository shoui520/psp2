/*
 * fmal function: x * y + z rounded once, in long double. Where the product and z nearly
 * cancel (opposite signs, exponents within 2), the sum is formed in
 * extended precision; otherwise the plain expression is exact enough.
 */
#include <math.h>

#include "xmath.h"

long double (fmal)(long double x, long double y, long double z)
{
	short xexp;
	short yexp;
	short zexp;
	long double xu = x;
	long double yu = y;
	long double zu = z;
	short errx;
	short erry;
	short errz;

	if ((errx = _LDunscale(&xexp, &xu)) == _NANCODE)
		return x;
	if ((erry = _LDunscale(&yexp, &yu)) == _NANCODE)
		return y;
	if ((errz = _LDunscale(&zexp, &zu)) == _NANCODE) {
		return z;
	} else if (errx == _INFCODE || erry == _INFCODE) {	/* an infinite product */
		if (errx != 0 && erry != 0
		    && (errz != _INFCODE || (LSIGN(x) ^ LSIGN(y)) == LSIGN(z)))
			return x * y + z;
		_Feraise(_FE_INVALID);	/* 0 * inf, or inf - inf */
		return _LNan._Long_double;
	} else if (errz == _INFCODE) {
		return z;
	} else if (errx == 0 || erry == 0 || errz == 0) {
		return x * y + z;
	} else {
		const long xyexp = xexp + yexp;
		const short n = zexp - xyexp;
		long double xp[4];
		long double yp[2];
		long double zp[2];

		if ((LSIGN(x) ^ LSIGN(y) ^ LSIGN(z)) == 0 || n < -2 || 2 < n)
			return x * y + z;
		zexp = n;
		_LDscale(&zu, n);	/* z relative to the product */
		_LXp_setw(xp, 4, xu);
		_LXp_setw(yp, 2, yu);
		_LXp_setw(zp, 2, zu);
		_LXp_mulh(xp, 4, yp[0]);
		if (yp[1] != 0.0L) {
			long double tp[4];

			_LXp_setw(tp, 4, xu);
			_LXp_mulh(tp, 4, yp[1]);
			_LXp_addx(xp, 4, tp, 4);
		}
		_LXp_addx(xp, 4, zp, 2);
		zu = _LXp_getw(xp, 4);
		_LDscale(&zu, xyexp);
		return zu;
	}
}
