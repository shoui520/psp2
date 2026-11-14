/*
 * _Dtento function: x times 10^n, with checking.
 */
#include "xmath.h"

double _Dtento(double x, long n, int *perr)
{
	if (n != 0 && x != 0.0) {
		_Xfloat xx = _Xfromd(x);

		xx = _Xtento(xx, n);
		x = _Xtod(xx, perr);
	}
	return x;
}
