/*
 * tgamma function: the gamma function of x, from _Tgamma scaled by its
 * exponent; by the reflection formula pi / (x sin(pi x) gamma(-x)) for x
 * below -_Gamma_big.
 */
#include <math.h>

#include "xmath.h"

static const double pi = 3.14159265358979323846;

double (tgamma)(double x)
{
	double y = x;

	switch (_Dint(&y, 0)) {
	case _NANCODE:
		return x;
	case _INFCODE:
		if (!DSIGN(x))
			return x;
		_Feraise(_FE_INVALID);
		return _Nan._Double;
	case 0:	/* an integer */
		if (x == 0.0) {	/* a pole */
			_Feraise(_FE_DIVBYZERO);
			return DSIGN(x) ? -_Inf._Double : _Inf._Double;
		} else if (x < 0.0) {	/* a domain error */
			_Feraise(_FE_INVALID);
			return _Nan._Double;
		}
		/* fall through */
	default:
		if (x <= -174.0) {	/* underflows */
			_Feraise(_FE_UNDERFLOW);
			return 0.0;
		} else if (x < -_Gamma_big) {	/* reflect */
			double z = -x - 1.0;
			double t = x;
			short xexp;

			y = x - y;	/* the fraction */
			_Dint(&t, -1);
			if (x - t < -1.0)	/* an odd integer part */
				y = -y;
			t = pi / (-x * _Sin(y * pi, 0) * (-x - 1.0));
			t /= _Tgamma(&z, &xexp) + 1.0;
			t = z * t;
			switch (_Dscale(&t, xexp)) {
			case 0:
				_Feraise(_FE_UNDERFLOW);
				break;
			case _INFCODE:
				_Feraise(_FE_OVERFLOW);
				break;
			}
			return t;
		} else if (x <= 174.0) {
			short xexp;

			x = (_Tgamma(&x, &xexp) + 1.0) / x;
			switch (_Dscale(&x, -xexp)) {
			case 0:
				_Feraise(_FE_UNDERFLOW);
				break;
			case _INFCODE:
				_Feraise(_FE_OVERFLOW);
				break;
			}
			return x;
		} else {	/* overflows */
			_Feraise(_FE_OVERFLOW);
			return _Inf._Double;
		}
	}
}
