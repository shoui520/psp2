/*
 * _FErfc function: erfc(x) for 0 < x in float, e^-x^2 times a rational approximation
 * in x.
 */
#include "xmath.h"

static const float num[] = {	/* the numerator's coefficients, highest first */
	-0.00019043297F, 0.003365143F, 0.3904994F, 0.8272613F,
};
static const float den[] = {	/* the denominator's */
	0.7372748F, 1.2764108F, 0.841951F,
};

float _FErfc(float x)
{
	float y = -x * x;

	_FExp(&y, (((num[0] * x + num[1]) * x + num[2]) * x + num[3])
		  / ((den[0] * x + den[1]) * x + den[2]), 0);
	return y;
}
