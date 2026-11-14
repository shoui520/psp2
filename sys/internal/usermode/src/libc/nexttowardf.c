/*
 * nexttowardf function: the next representable value after x in the
 * direction of y, stepped in the bits of x.
 */
#include <math.h>

#include "xmath.h"

float (nexttowardf)(float x, long double y)
{
	_Fval *const ps = (_Fval *)&x;
	const short code = _FDtest(&x);

	if (code == _NANCODE) {
		;
	} else if (_LDtest(&y) == _NANCODE || x == y) {
		x = y;
	} else if (code == _INFCODE) {	/* the largest finite value */
		ps->_Sh[_F0] = (ps->_Sh[_F0] & _FSIGN) | ((_FMAX << _FOFF) - 1);
		ps->_Sh[_F1] = 0xffff;
	} else if (code == 0) {	/* the smallest subnormal */
		ps->_Sh[_F0] = x < y ? 0 : _FSIGN;
		ps->_Sh[_F1] = 1;
	} else if ((x < y && 0.0F < x) || (y < x && x < 0.0F)) {	/* away from zero */
		if (++ps->_Sh[_F1] == 0
		    && (++ps->_Sh[_F0] & ~_FSIGN) == _FMAX << _FOFF)
			_Feraise(_FE_OVERFLOW);
	} else {	/* toward zero */
		if (--ps->_Sh[_F1] == 0xffff)
			--ps->_Sh[_F0];
	}
	if ((ps->_Sh[_F0] & _FMASK) == 0)
		_Feraise(_FE_UNDERFLOW);
	return x;
}
