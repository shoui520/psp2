/*
 * _LPoly function: the polynomial tab[0] x^n + tab[1] x^(n-1) + ... + tab[n]
 * in long double, by Horner's rule.
 */
#include "xmath.h"

long double _LPoly(long double x, const long double *tab, int n)
{
	long double y = *tab;

	while (0 <= --n)
		y = y * x + *++tab;
	return y;
}
