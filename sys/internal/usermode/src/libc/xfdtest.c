/*
 * _FDtest function: classify *px as NaN, INF, zero, subnormal or finite, in float.
 */
#include "xmath.h"

short _FDtest(float *px)
{
	_Fval *ps = (_Fval *)(char *)px;

	if ((ps->_Sh[_F0] & _FMASK) == _FMAX << _FOFF)
		return (short)((ps->_Sh[_F0] & _FFRAC) != 0 || ps->_Sh[_F1] != 0 ? _NANCODE : _INFCODE);
	else if ((ps->_Sh[_F0] & ~_FSIGN) != 0 || ps->_Sh[_F1] != 0)
		return (ps->_Sh[_F0] & _FMASK) == 0 ? _DENORM : _FINITE;
	else
		return 0;
}
