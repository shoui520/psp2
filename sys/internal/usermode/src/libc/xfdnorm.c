/*
 * _FDnorm function: normalize the fraction of a denormalized float.
 */
#include "xmath.h"

short _FDnorm(_Fval *ps)
{
	short xchar = 1;
	unsigned short sign = (unsigned short)(ps->_Sh[_F0] & _FSIGN);
	unsigned short frac = (unsigned short)(ps->_Sh[_F0] & _FFRAC);

	if ((ps->_Sh[_F0] & _FFRAC) != 0 || ps->_Sh[_F1]) {	/* nonzero: scale */
		if (frac == 0) {	/* shift left by 16 */
			frac = ps->_Sh[_F1];
			ps->_Sh[_F1] = 0;
			xchar -= 16;
		}
		for (; frac < 1 << _FOFF; --xchar) {	/* shift left by 1 */
			frac = (unsigned short)(frac << 1 | ps->_Sh[_F1] >> 15);
			ps->_Sh[_F1] <<= 1;
		}
		for (; 1 << (_FOFF + 1) <= frac; ++xchar) {	/* shift right by 1 */
			ps->_Sh[_F1] = (unsigned short)(ps->_Sh[_F1] >> 1 | frac << 15);
			frac >>= 1;
		}
		frac &= _FFRAC;
	}
	ps->_Sh[_F0] = (unsigned short)(frac | sign);
	return xchar;
}
