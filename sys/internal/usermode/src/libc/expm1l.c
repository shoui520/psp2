/*
 * expm1l function: e to the x minus 1, a rational approximation times x
 * between -ln 2 and ln 1.5, expl(x) - 1 beyond.
 */
#include <math.h>

#include "xmath.h"

static const long double num[] = {	/* the numerator's coefficients, highest first */
	2.59766943573417e-06L, 0.00011305370687423614L, 0.0010906679297077834L,
	0.027845649093295143L, 0.03926190614812718L, 0.9238986935301859L,
};
static const long double den[] = {	/* the denominator's */
	-1.757061517257875e-05L, 0.0006049978699162782L, -0.009560331104590007L,
	0.08520625381341307L, -0.4226874406169659L, 0.9238986935301859L,
};
static const long double xlo = -0.69314718055994530942L;	/* -ln 2 */
static const long double xhi = 0.40546510810816438198L;	/* ln 1.5 */

long double (expm1l)(long double x)
{
	switch (_LDtest(&x)) {
	case _NANCODE:
	case 0:
		return x;
	case _INFCODE:
		return LSIGN(x) ? -1.0L : x;
	default:
		if (xlo < x && x < xhi)
			return (((((num[0] * x + num[1]) * x + num[2]) * x + num[3]) * x + num[4]) * x
				+ num[5])
			       / (((((den[0] * x + den[1]) * x + den[2]) * x + den[3]) * x + den[4]) * x
				  + den[5])
			       * x;
		else
			return expl(x) - 1.0L;
	}
}
