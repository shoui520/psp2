/*
 * _FDtento function: x times 10^n, with checking, rounded to float.
 */
#include "xmath.h"

float _FDtento(double x, long n, int *perr)
{
	if (n != 0 && x != 0.0) {
		_Xfloat xx = _Xfromd(x);

		xx = _Xtento(xx, n);
		return _Xtof(xx, perr);
	}
	return (float)x;
}
