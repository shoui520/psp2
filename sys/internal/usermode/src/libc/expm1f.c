/*
 * expm1f function: e to the x minus 1, a rational approximation times x
 * between -ln 2 and ln 1.5, expf(x) - 1 beyond.
 */
#include <math.h>

#include "xmath.h"

static const float num[] = {	/* the numerator's coefficients, highest first */
	0.0024490478F, 0.030527467F, 0.15380266F, 0.94831777F,
};
static const float den[] = {	/* the denominator's */
	0.03265278F, -0.32035613F, 0.94831777F,
};
static const float xlo = -0.69314718055994530942F;	/* -ln 2 */
static const float xhi = 0.40546510810816438198F;	/* ln 1.5 */

float (expm1f)(float x)
{
	switch (_FDtest(&x)) {
	case _NANCODE:
	case 0:
		return x;
	case _INFCODE:
		return FSIGN(x) ? -1.0F : x;
	default:
		if (xlo < x && x < xhi)
			return (((num[0] * x + num[1]) * x + num[2]) * x + num[3])
			       / ((den[0] * x + den[1]) * x + den[2])
			       * x;
		else
			return expf(x) - 1.0F;
	}
}
