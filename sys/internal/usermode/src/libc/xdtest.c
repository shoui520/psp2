/*
 * _Dtest function: classify *px as NaN, INF, zero, subnormal or finite.
 */
#include "xmath.h"

short _Dtest(double *px)
{
	_Dval *ps = (_Dval *)(char *)px;

	if ((ps->_Sh[_D0] & _DMASK) == _DMAX << _DOFF)
		return (short)((ps->_Sh[_D0] & _DFRAC) != 0 || ps->_Sh[_D1] != 0
			       || ps->_Sh[_D2] != 0 || ps->_Sh[_D3] != 0 ? _NANCODE : _INFCODE);
	else if ((ps->_Sh[_D0] & ~_DSIGN) != 0 || ps->_Sh[_D1] != 0
		 || ps->_Sh[_D2] != 0 || ps->_Sh[_D3] != 0)
		return (ps->_Sh[_D0] & _DMASK) == 0 ? _DENORM : _FINITE;
	else
		return 0;
}
