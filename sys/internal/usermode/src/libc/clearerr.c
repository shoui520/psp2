/* clearerr function */
#include "xstdio.h"

void clearerr(FILE *str)
{
	_Lockfilelock(str);
	if (str->_Mode & (_MOPENR | _MOPENW))
		str->_Mode &= ~(_MEOF | _MERR);
	_Unlockfilelock(str);
}
