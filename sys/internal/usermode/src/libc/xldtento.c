/*
 * _LDtento function: x times 10^n, with checking (long double is double).
 */
#include "xmath.h"

long double _LDtento(long double x, long n, int *perr)
{
	if (n != 0 && x != 0.0L) {
		_Xfloat xx = _Xfromd(x);

		xx = _Xtento(xx, n);
		x = _Xtold(xx, perr);
	}
	return x;
}
