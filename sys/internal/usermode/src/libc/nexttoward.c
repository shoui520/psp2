/*
 * nexttoward function: the next representable value after x in the
 * direction of y, stepped in the bits of x.
 */
#include <math.h>

#include "xmath.h"

double (nexttoward)(double x, long double y)
{
	_Dval *const ps = (_Dval *)&x;
	const short code = _Dtest(&x);

	if (code == _NANCODE) {
		;
	} else if (_LDtest(&y) == _NANCODE || x == y) {
		x = y;
	} else if (code == _INFCODE) {	/* the largest finite value */
		ps->_Sh[_D0] = (ps->_Sh[_D0] & _DSIGN) | ((_DMAX << _DOFF) - 1);
		ps->_Sh[_D1] = 0xffff;
		ps->_Sh[_D2] = 0xffff;
		ps->_Sh[_D3] = 0xffff;
	} else if (code == 0) {	/* the smallest subnormal */
		ps->_Sh[_D0] = x < y ? 0 : _DSIGN;
		ps->_Sh[_D3] = 1;
	} else if ((x < y && 0.0 < x) || (y < x && x < 0.0)) {	/* away from zero */
		if (++ps->_Sh[_D3] == 0 && ++ps->_Sh[_D2] == 0 && ++ps->_Sh[_D1] == 0
		    && (++ps->_Sh[_D0] & ~_DSIGN) == _DMAX << _DOFF)
			_Feraise(_FE_OVERFLOW);
	} else {	/* toward zero */
		if (--ps->_Sh[_D3] == 0xffff && --ps->_Sh[_D2] == 0xffff
		    && --ps->_Sh[_D1] == 0xffff)
			--ps->_Sh[_D0];
	}
	if ((ps->_Sh[_D0] & _DMASK) == 0)
		_Feraise(_FE_UNDERFLOW);
	return x;
}
