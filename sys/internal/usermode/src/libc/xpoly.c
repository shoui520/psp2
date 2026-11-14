/*
 * _Poly function: the polynomial tab[0] x^n + tab[1] x^(n-1) + ... + tab[n]
 * in double, by Horner's rule.
 */
#include "xmath.h"

double _Poly(double x, const double *tab, int n)
{
	double y = *tab;

	while (0 <= --n)
		y = y * x + *++tab;
	return y;
}
