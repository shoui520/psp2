/*
 * _LCosh function: y times the hyperbolic cosine of x, for cosh and the
 * complex functions.
 */
#include <math.h>

#include "xmath.h"

long double _LCosh(long double x, long double y)
{
	const short errx = _LDtest(&x);
	const short erry = _LDtest(&y);

	if (errx >= 0 || erry >= 0) {	/* a zero, an infinity or a NaN */
		if (errx == _NANCODE)
			return x;
		else if (erry == _NANCODE)
			return y;
		else if (errx != _INFCODE)	/* cosh(0) is 1 */
			return y;
		else if (erry == 0) {	/* INF times 0 */
			_Feraise(_FE_INVALID);
			return _LNan._Long_double;
		} else {
			return y < 0.0L ? -_LInf._Long_double : _LInf._Long_double;
		}
	} else {	/* finite */
		if (x < 0.0L)
			x = -x;
		if (x < _LXbig._Long_double) {	/* y * (e^x + e^-x) / 2 */
			_LExp(&x, 1.0L, -1);
			return y * (x + 0.25L / x);
		} else {	/* y * e^x / 2 */
			_LExp(&x, y, -1);
			return x;
		}
	}
}
