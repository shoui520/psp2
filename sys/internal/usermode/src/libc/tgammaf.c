/*
 * tgammaf function: the gamma function of x, from _FTgamma scaled by its
 * exponent; by the reflection formula pi / (x sin(pi x) gamma(-x)) for x
 * below -_FGamma_big.
 */
#include <math.h>

#include "xmath.h"

static const float pi = 3.14159265358979323846F;

float (tgammaf)(float x)
{
	float y = x;

	switch (_FDint(&y, 0)) {
	case _NANCODE:
		return x;
	case _INFCODE:
		if (!FSIGN(x))
			return x;
		_Feraise(_FE_INVALID);
		return _FNan._Float;
	case 0:	/* an integer */
		if (x == 0.0F) {	/* a pole */
			_Feraise(_FE_DIVBYZERO);
			return FSIGN(x) ? -_FInf._Float : _FInf._Float;
		} else if (x < 0.0F) {	/* a domain error */
			_Feraise(_FE_INVALID);
			return _FNan._Float;
		}
		/* fall through */
	default:
		if (x <= -38.0F) {	/* underflows */
			_Feraise(_FE_UNDERFLOW);
			return 0.0F;
		} else if (x < -_FGamma_big) {	/* reflect */
			float z = -x - 1.0F;
			float t = x;
			short xexp;

			y = x - y;	/* the fraction */
			_FDint(&t, -1);
			if (x - t < -1.0F)	/* an odd integer part */
				y = -y;
			t = pi / (-x * _FSin(y * pi, 0) * (-x - 1.0F));
			t /= _FTgamma(&z, &xexp) + 1.0F;
			t = z * t;
			switch (_FDscale(&t, xexp)) {
			case 0:
				_Feraise(_FE_UNDERFLOW);
				break;
			case _INFCODE:
				_Feraise(_FE_OVERFLOW);
				break;
			}
			return t;
		} else if (x <= 38.0F) {
			short xexp;

			x = (_FTgamma(&x, &xexp) + 1.0F) / x;
			switch (_FDscale(&x, -xexp)) {
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
			return _FInf._Float;
		}
	}
}
