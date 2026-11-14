/*
 * _LSinx function: the sine of x, or its cosine for an odd qoff, in long
 * double (a double on psp2); _LSin is it with no quadrants given. x is reduced to [-pi/4, pi/4] by
 * _LQuad and qoff adds the quadrants it was reduced by.
 */
#include "xmath.h"

static const long double c[] = {	/* cos(x) - 1 is x^2 times this in x^2 */
	2.06374484196e-09L, -2.7555365134677e-07L, 2.480157946764225e-05L,
	-0.0013888888873052597L, 0.041666666666519865L, -0.49999999999999545L,
};
static const long double s[] = {	/* sin(x) - x is x^3 times this in x^2 */
	1.5893606014e-10L, -2.505069049138e-08L, 2.75573131527032e-06L,
	-0.00019841269827816117L, 0.008333333333319082L, -0.16666666666666613L,
};

long double _LSinx(long double x, unsigned int qoff, int quads)
{
	switch (_LDtest(&x)) {
	case _NANCODE:
		return x;
	case 0:	/* sin(0) is 0, cos(0) 1 */
		if (qoff & 0x1)
			x = 1.0L;
		return (qoff & 0x2) ? -x : x;
	case _INFCODE:
		_Feraise(_FE_INVALID);
		return _LNan._Long_double;
	default:	/* finite */
		qoff += _LQuad(&x);
		if (-_LRteps._Long_double < x && x < _LRteps._Long_double) {	/* tiny: x or 1 */
			if (qoff & 0x1)
				x = 1.0L;
		} else {
			const long double w = x * x;

			if (qoff & 0x1)
				x = 1.0L + w * (((((c[0] * w + c[1]) * w + c[2]) * w + c[3]) * w + c[4]) * w + c[5]);
			else
				x += x * w * (((((s[0] * w + s[1]) * w + s[2]) * w + s[3]) * w + s[4]) * w + s[5]);
		}
		return (qoff & 0x2) ? -x : x;
	}
}

long double _LSin(long double x, unsigned int qoff)
{
	return _LSinx(x, qoff, 0);
}
