/*
 * _FDunscale function: separate *px into a fraction in [0.5, 1) and a power of
 * two, *pex; return what kind of value it is.
 */
#include "xmath.h"

short _FDunscale(short *pex, float *px)
{
	_Fval *ps = (_Fval *)(char *)px;
	short xchar = (short)((ps->_Sh[_F0] & _FMASK) >> _FOFF);

	if (xchar == _FMAX) {	/* NaN or INF */
		*pex = 0;
		return (ps->_Sh[_F0] & _FFRAC) != 0 || ps->_Sh[_F1] != 0 ? _NANCODE : _INFCODE;
	} else if (0 < xchar || (xchar = _FDnorm(ps)) <= 0) {	/* finite: reduce to [1/2, 1) */
		ps->_Sh[_F0] = (unsigned short)(ps->_Sh[_F0] & ~_FMASK | _FBIAS << _FOFF);
		*pex = (short)(xchar - _FBIAS);
		return _FINITE;
	} else {	/* zero */
		*pex = 0;
		return 0;
	}
}
