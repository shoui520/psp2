/*
 * expm1 function: e to the x minus 1, a rational approximation times x
 * between -ln 2 and ln 1.5, exp(x) - 1 beyond.
 */
#include <math.h>

#include "xmath.h"

static const double num[] = {	/* the numerator's coefficients, highest first */
	2.59766943573417e-06, 0.00011305370687423614, 0.0010906679297077834,
	0.027845649093295143, 0.03926190614812718, 0.9238986935301859,
};
static const double den[] = {	/* the denominator's */
	-1.757061517257875e-05, 0.0006049978699162782, -0.009560331104590007,
	0.08520625381341307, -0.4226874406169659, 0.9238986935301859,
};
static const double xlo = -0.69314718055994530942;	/* -ln 2 */
static const double xhi = 0.40546510810816438198;	/* ln 1.5 */

double (expm1)(double x)
{
	switch (_Dtest(&x)) {
	case _NANCODE:
	case 0:
		return x;
	case _INFCODE:
		return DSIGN(x) ? -1.0 : x;
	default:
		if (xlo < x && x < xhi)
			return (((((num[0] * x + num[1]) * x + num[2]) * x + num[3]) * x + num[4]) * x
				+ num[5])
			       / (((((den[0] * x + den[1]) * x + den[2]) * x + den[3]) * x + den[4]) * x
				  + den[5])
			       * x;
		else
			return exp(x) - 1.0;
	}
}
