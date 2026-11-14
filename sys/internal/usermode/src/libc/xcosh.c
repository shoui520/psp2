/*
 * _Cosh function: y times the hyperbolic cosine of x, for cosh and the
 * complex functions.
 */
#include <math.h>

#include "xmath.h"

double _Cosh(double x, double y)
{
	const short errx = _Dtest(&x);
	const short erry = _Dtest(&y);

	if (errx >= 0 || erry >= 0) {	/* a zero, an infinity or a NaN */
		if (errx == _NANCODE)
			return x;
		else if (erry == _NANCODE)
			return y;
		else if (errx != _INFCODE)	/* cosh(0) is 1 */
			return y;
		else if (erry == 0) {	/* INF times 0 */
			_Feraise(_FE_INVALID);
			return _Nan._Double;
		} else {
			return y < 0.0 ? -_Inf._Double : _Inf._Double;
		}
	} else {	/* finite */
		if (x < 0.0)
			x = -x;
		if (x < _Xbig) {	/* y * (e^x + e^-x) / 2 */
			_Exp(&x, 1.0, -1);
			return y * (x + 0.25 / x);
		} else {	/* y * e^x / 2 */
			_Exp(&x, y, -1);
			return x;
		}
	}
}
