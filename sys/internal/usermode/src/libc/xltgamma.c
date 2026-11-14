/*
 * _LTgamma function: for tgammal, gamma(*px) as (1 + the return value)
 * divided by *px 2^*pex. x is moved to [-0.5, 0.5] by the recurrence
 * gamma(x + 1) = x gamma(x), with the factors in *px, and gamma(x + 2) - 1
 * returned from a rational approximation; from 54 up, the reciprocal of
 * Stirling's series is put in *px and 0 returned.
 */
#include <math.h>

#include "xmath.h"

static const long double p[] = {	/* gamma(x + 2) - 1 is x p(x) / q(x) */
	1.192292740826975e-05L, 0.00011794182945081213L, 0.001019594601453559L,
	0.007560606804984384L, 0.01926527211745248L, 0.12554212337999338L, 0.5454433305482579L,
	0.4325194486040339L,
};
static const long double q[] = {
	2.670630520841925e-05L, -0.0014256270073394934L, 0.011843095408138174L,
	-0.00913626688892627L, -0.18643116397504167L, 0.2935773042446101L, 1.023026192546371L,
};
static const long double stirnum[] = {	/* sqrtl(2pi) times Stirling's series, in 1/x - xoff */
	0.016614314518760217L, -0.0013965707272626177L, -0.004982425438353885L,
	0.001058348743260703L, 0.002148945642533754L, -0.0009589644769559295L,
	-0.001445526295980064L, 0.0014401123536802326L, 0.001647409982254744L,
	-0.005187775183999542L, -0.0008078437703700011L, 0.15488179113127576L, 1.9659470092827178L,
	2.5089636430553437L,
};
static const long double stirden[] = {
	0.7002370566012027L, 1.0L,
};
static const struct {	/* the ranges of x above 54, each with its approximation */
	long double xmax;	/* the range ends below xmax */
	long double xoff;	/* the middle of 1/x in the range */
	const long double *num;
	int nnum;	/* the numerator's degree */
	const long double *den;
	int nden;	/* the denominator's */
} range[] = {
	{174.0L, 0.011174968071519796L, stirnum, 13, stirden, 1},
};

long double _LTgamma(long double *px, short *pex)
{
	long double x = *px;
	long double z;

	if (54.0L <= x) {	/* Stirling: x^(x - 1/2) e^-x sqrtl(2pi) times the series */
		long double y;

		z = _LPow(x, x - 0.5L, pex);
		y = 1.0L / x - range[0].xoff;
		*pex = -*pex;
		*px = _LPoly(y, range[0].den, range[0].nden) / _LPoly(y, range[0].num, range[0].nnum);
		*px /= z * expl(-x);
		return 0.0L;
	} else if (x < -0.5L) {	/* multiply up */
		z = 1.0L;
		for (;;) {
			z *= x;
			if (0.5L < x)
				break;
			x += 1.0L;
		}
		x -= 1.0L;
		_LDunscale(pex, &z);
	} else if (x < 0.5L) {
		z = x * (x + 1.0L);
		_LDunscale(pex, &z);
	} else if (x < 1.5L) {
		z = x;
		*pex = 0;
		x -= 1.0L;
	} else if (x < 2.5L) {
		z = 1.0L;
		*pex = 0;
		x -= 2.0L;
	} else {	/* divide down */
		z = 1.0L;
		while (2.5L < x) {
			x -= 1.0L;
			z /= x;
		}
		x -= 2.0L;
		_LDunscale(pex, &z);
	}
	*px = z;
	return _LPoly(x, p, 7) / _LPoly(x, q, 6) * x;
}
