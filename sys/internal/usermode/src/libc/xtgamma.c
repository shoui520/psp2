/*
 * _Tgamma function: for tgamma, gamma(*px) as (1 + the return value)
 * divided by *px 2^*pex. x is moved to [-0.5, 0.5] by the recurrence
 * gamma(x + 1) = x gamma(x), with the factors in *px, and gamma(x + 2) - 1
 * returned from a rational approximation; from 54 up, the reciprocal of
 * Stirling's series is put in *px and 0 returned.
 */
#include <math.h>

#include "xmath.h"

static const double p[] = {	/* gamma(x + 2) - 1 is x p(x) / q(x) */
	1.192292740826975e-05, 0.00011794182945081213, 0.001019594601453559,
	0.007560606804984384, 0.01926527211745248, 0.12554212337999338, 0.5454433305482579,
	0.4325194486040339,
};
static const double q[] = {
	2.670630520841925e-05, -0.0014256270073394934, 0.011843095408138174,
	-0.00913626688892627, -0.18643116397504167, 0.2935773042446101, 1.023026192546371,
};
static const double stirnum[] = {	/* sqrt(2pi) times Stirling's series, in 1/x - xoff */
	0.016614314518760217, -0.0013965707272626177, -0.004982425438353885,
	0.001058348743260703, 0.002148945642533754, -0.0009589644769559295,
	-0.001445526295980064, 0.0014401123536802326, 0.001647409982254744,
	-0.005187775183999542, -0.0008078437703700011, 0.15488179113127576, 1.9659470092827178,
	2.5089636430553437,
};
static const double stirden[] = {
	0.7002370566012027, 1.0,
};
static const struct {	/* the ranges of x above 54, each with its approximation */
	double xmax;	/* the range ends below xmax */
	double xoff;	/* the middle of 1/x in the range */
	const double *num;
	int nnum;	/* the numerator's degree */
	const double *den;
	int nden;	/* the denominator's */
} range[] = {
	{174.0, 0.011174968071519796, stirnum, 13, stirden, 1},
};

double _Tgamma(double *px, short *pex)
{
	double x = *px;
	double z = 1.0;

	if (54.0 <= x) {	/* Stirling: x^(x - 1/2) e^-x sqrt(2pi) times the series */
		double y;

		z = _Pow(x, x - 0.5, pex);
		y = 1.0 / x - range[0].xoff;
		*pex = -*pex;
		*px = _Poly(y, range[0].den, range[0].nden) / _Poly(y, range[0].num, range[0].nnum);
		*px /= z * exp(-x);
		return 0.0;
	} else if (x < -0.5) {	/* multiply up */
		for (;;) {
			z *= x;
			if (0.5 < x)
				break;
			x += 1.0;
		}
		x -= 1.0;
		_Dunscale(pex, &z);
	} else if (x < 0.5) {
		z = x * (x + 1.0);
		_Dunscale(pex, &z);
	} else if (x < 1.5) {
		z = x;
		*pex = 0;
		x -= 1.0;
	} else if (x < 2.5) {
		z = 1.0;
		*pex = 0;
		x -= 2.0;
	} else {	/* divide down */
		z = 1.0;
		while (2.5 < x) {
			x -= 1.0;
			z /= x;
		}
		x -= 2.0;
		_Dunscale(pex, &z);
	}
	*px = z;
	return _Poly(x, p, 7) / _Poly(x, q, 6) * x;
}
