/*
 * _FPoly function: the polynomial tab[0] x^n + tab[1] x^(n-1) + ... + tab[n]
 * in float, by Horner's rule.
 */
#include "xmath.h"

float _FPoly(float x, const float *tab, int n)
{
	float y = *tab;

	while (0 <= --n)
		y = y * x + *++tab;
	return y;
}
