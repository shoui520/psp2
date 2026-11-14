/*
 * _FCosh function: y times the hyperbolic cosine of x, for cosh and the
 * complex functions, in float.
 */
#include <math.h>

#include "xmath.h"

float _FCosh(float x, float y)
{
	const short errx = _FDtest(&x);
	const short erry = _FDtest(&y);

	if (errx >= 0 || erry >= 0) {	/* a zero, an infinity or a NaN */
		if (errx == _NANCODE)
			return x;
		else if (erry == _NANCODE)
			return y;
		else if (errx != _INFCODE)	/* cosh(0) is 1 */
			return y;
		else if (erry == 0) {	/* INF times 0 */
			_Feraise(_FE_INVALID);
			return _FNan._Float;
		} else {
			return y < 0.0F ? -_FInf._Float : _FInf._Float;
		}
	} else {	/* finite */
		if (x < 0.0F)
			x = -x;
		if (x < _FXbig._Float) {	/* y * (e^x + e^-x) / 2 */
			_FExp(&x, 1.0F, -1);
			return y * (x + 0.25F / x);
		} else {	/* y * e^x / 2 */
			_FExp(&x, y, -1);
			return x;
		}
	}
}
