/*
 * fma function: x * y + z rounded once. Where the product and z nearly
 * cancel (opposite signs, exponents within 2), the sum is formed in
 * extended precision; otherwise the plain expression is exact enough.
 */
#include <math.h>

#include "xmath.h"

double (fma)(double x, double y, double z)
{
	short xexp;
	short yexp;
	short zexp;
	double xu = x;
	double yu = y;
	double zu = z;
	const short errx = _Dunscale(&xexp, &xu);
	short erry;
	short errz;

	if (errx == _NANCODE)
		return x;
	if ((erry = _Dunscale(&yexp, &yu)) == _NANCODE)
		return y;
	if ((errz = _Dunscale(&zexp, &zu)) == _NANCODE) {
		return z;
	} else if (errx == _INFCODE || erry == _INFCODE) {	/* an infinite product */
		if (errx != 0 && erry != 0
		    && (errz != _INFCODE || (DSIGN(x) ^ DSIGN(y)) == DSIGN(z)))
			return x * y + z;
		_Feraise(_FE_INVALID);	/* 0 * inf, or inf - inf */
		return _Nan._Double;
	} else if (errz == _INFCODE) {
		return z;
	} else if (errx == 0 || erry == 0 || errz == 0) {
		return x * y + z;
	} else {
		const short xyexp = xexp + yexp;
		const short n = zexp - xyexp;
		double xp[4];
		double yp[2];
		double zp[2];

		if ((DSIGN(x) ^ DSIGN(y) ^ DSIGN(z)) == 0 || n < -2 || 2 < n)
			return x * y + z;
		zexp = n;
		_Dscale(&zu, n);	/* z relative to the product */
		_Xp_setw(xp, 4, xu);
		_Xp_setw(yp, 2, yu);
		_Xp_setw(zp, 2, zu);
		_Xp_mulh(xp, 4, yp[0]);
		if (yp[1] != 0.0) {
			double tp[4];

			_Xp_setw(tp, 4, xu);
			_Xp_mulh(tp, 4, yp[1]);
			_Xp_addx(xp, 4, tp, 4);
		}
		_Xp_addx(xp, 4, zp, 2);
		zu = _Xp_getw(xp, 4);
		_Dscale(&zu, xyexp);
		return zu;
	}
}
