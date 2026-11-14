/*
 * tgammal function: the gamma function of x, from _LTgamma scaled by its
 * exponent; by the reflection formula pi / (x sin(pi x) gamma(-x)) for x
 * below -_LGamma_big.
 */
#include <math.h>

#include "xmath.h"

static const long double pi = 3.14159265358979323846L;

long double (tgammal)(long double x)
{
	long double y = x;

	switch (_LDint(&y, 0)) {
	case _NANCODE:
		return x;
	case _INFCODE:
		if (!LSIGN(x))
			return x;
		_Feraise(_FE_INVALID);
		return _LNan._Long_double;
	case 0:	/* an integer */
		if (x == 0.0L) {	/* a pole */
			_Feraise(_FE_DIVBYZERO);
			return LSIGN(x) ? -_LInf._Long_double : _LInf._Long_double;
		} else if (x < 0.0L) {	/* a domain error */
			_Feraise(_FE_INVALID);
			return _LNan._Long_double;
		}
		/* fall through */
	default:
		if (x <= -174.0L) {	/* underflows */
			_Feraise(_FE_UNDERFLOW);
			return 0.0L;
		} else if (x < -_LGamma_big) {	/* reflect */
			long double z = -x - 1.0L;
			long double t = x;
			short xexp;

			y = x - y;	/* the fraction */
			_LDint(&t, -1);
			if (x - t < -1.0L)	/* an odd integer part */
				y = -y;
			t = pi / (-x * _LSin(y * pi, 0) * (-x - 1.0L));
			t /= _LTgamma(&z, &xexp) + 1.0L;
			t = z * t;
			switch (_LDscale(&t, xexp)) {
			case 0:
				_Feraise(_FE_UNDERFLOW);
				break;
			case _INFCODE:
				_Feraise(_FE_OVERFLOW);
				break;
			}
			return t;
		} else if (x <= 174.0L) {
			short xexp;

			x = (_LTgamma(&x, &xexp) + 1.0L) / x;
			switch (_LDscale(&x, -xexp)) {
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
			return _LInf._Long_double;
		}
	}
}
