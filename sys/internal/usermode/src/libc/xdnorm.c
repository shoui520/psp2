/*
 * _Dnorm function: normalize the fraction of a denormalized double.
 */
#include "xmath.h"

short _Dnorm(_Dval *ps)
{
	short xchar = 1;
	unsigned short sign = (unsigned short)(ps->_Sh[_D0] & _DSIGN);
	unsigned short frac = (unsigned short)(ps->_Sh[_D0] & _DFRAC);

	if ((ps->_Sh[_D0] & _DFRAC) != 0 || ps->_Sh[_D1] || ps->_Sh[_D2] || ps->_Sh[_D3]) {	/* nonzero: scale */
		for (; frac == 0; xchar -= 16) {	/* shift left by 16 */
			frac = ps->_Sh[_D1];
			ps->_Sh[_D1] = ps->_Sh[_D2];
			ps->_Sh[_D2] = ps->_Sh[_D3];
			ps->_Sh[_D3] = 0;
		}
		for (; frac < 1 << _DOFF; --xchar) {	/* shift left by 1 */
			frac = (unsigned short)(frac << 1 | ps->_Sh[_D1] >> 15);
			ps->_Sh[_D1] = (unsigned short)(ps->_Sh[_D1] << 1 | ps->_Sh[_D2] >> 15);
			ps->_Sh[_D2] = (unsigned short)(ps->_Sh[_D2] << 1 | ps->_Sh[_D3] >> 15);
			ps->_Sh[_D3] <<= 1;
		}
		for (; 1 << (_DOFF + 1) <= frac; ++xchar) {	/* shift right by 1 */
			ps->_Sh[_D3] = (unsigned short)(ps->_Sh[_D3] >> 1 | ps->_Sh[_D2] << 15);
			ps->_Sh[_D2] = (unsigned short)(ps->_Sh[_D2] >> 1 | ps->_Sh[_D1] << 15);
			ps->_Sh[_D1] = (unsigned short)(ps->_Sh[_D1] >> 1 | frac << 15);
			frac >>= 1;
		}
		frac &= _DFRAC;
	}
	ps->_Sh[_D0] = (unsigned short)(frac | sign);
	return xchar;
}
