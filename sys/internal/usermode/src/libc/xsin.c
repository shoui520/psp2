/*
 * _Sinx function: the sine of x, or its cosine for an odd qoff, in double;
 * _Sin is it with no quadrants given. x is reduced to [-pi/4, pi/4] by
 * _Quad and qoff adds the quadrants it was reduced by.
 */
#include "xmath.h"

static const double c[] = {	/* cos(x) - 1 is x^2 times this in x^2 */
	2.06374484196e-09, -2.7555365134677e-07, 2.480157946764225e-05,
	-0.0013888888873052597, 0.041666666666519865, -0.49999999999999545,
};
static const double s[] = {	/* sin(x) - x is x^3 times this in x^2 */
	1.5893606014e-10, -2.505069049138e-08, 2.75573131527032e-06,
	-0.00019841269827816117, 0.008333333333319082, -0.16666666666666613,
};

double _Sinx(double x, unsigned int qoff, int quads)
{
	switch (_Dtest(&x)) {
	case _NANCODE:
		return x;
	case 0:	/* sin(0) is 0, cos(0) 1 */
		if (qoff & 0x1)
			x = 1.0;
		return (qoff & 0x2) ? -x : x;
	case _INFCODE:
		_Feraise(_FE_INVALID);
		return _Nan._Double;
	default:	/* finite */
		qoff += _Quad(&x);
		if (-_Rteps._Double < x && x < _Rteps._Double) {	/* tiny: x or 1 */
			if (qoff & 0x1)
				x = 1.0;
		} else {
			const double w = x * x;

			if (qoff & 0x1)
				x = 1.0 + w * (((((c[0] * w + c[1]) * w + c[2]) * w + c[3]) * w + c[4]) * w + c[5]);
			else
				x += x * w * (((((s[0] * w + s[1]) * w + s[2]) * w + s[3]) * w + s[4]) * w + s[5]);
		}
		return (qoff & 0x2) ? -x : x;
	}
}

double _Sin(double x, unsigned int qoff)
{
	return _Sinx(x, qoff, 0);
}
