/* ungetc function */
#include "xstdio.h"

int ungetc(int c, FILE *str)
{
	_Lockfilelock(str);
	if (c == EOF || str->_Rback <= str->_Back
	    || (str->_Mode & (_MOPENR | _MWRITE | _MWIDE)) != _MOPENR) {
		c = EOF;
	} else {	/* deliver the pushback */
		str->_Mode = str->_Mode & ~_MEOF | _MREAD | _MBYTE;
		if (str->_Rsave == NULL) {	/* begin a pushback */
			str->_Rsave = str->_Rend;
			str->_Rend = str->_Buf;
		}
		*--str->_Rback = (unsigned char)c;
	}
	_Unlockfilelock(str);
	return c;
}
