/*
 * _Dunscale function: separate *px into a fraction in [0.5, 1) and a power of
 * two, *pex; return what kind of value it is.
 */
#include "xmath.h"

short _Dunscale(short *pex, double *px)
{
	_Dval *ps = (_Dval *)(char *)px;
	short xchar = (short)((ps->_Sh[_D0] & _DMASK) >> _DOFF);

	if (xchar == _DMAX) {	/* NaN or INF */
		*pex = 0;
		return (ps->_Sh[_D0] & _DFRAC) != 0 || ps->_Sh[_D1] != 0 || ps->_Sh[_D2] != 0
			|| ps->_Sh[_D3] != 0 ? _NANCODE : _INFCODE;
	} else if (0 < xchar || (xchar = _Dnorm(ps)) <= 0) {	/* finite: reduce to [1/2, 1) */
		ps->_Sh[_D0] = (unsigned short)(ps->_Sh[_D0] & ~_DMASK | _DBIAS << _DOFF);
		*pex = (short)(xchar - _DBIAS);
		return _FINITE;
	} else {	/* zero */
		*pex = 0;
		return 0;
	}
}
