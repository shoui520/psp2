/* fwide function */
#include <wchar.h>
#include "xstdio.h"

int fwide(FILE *str, int mode)
{
	int ans;

	_Lockfilelock(str);
	if (mode < 0 && !(str->_Mode & _MWIDE))
		str->_Mode |= _MBYTE;
	else if (0 < mode && !(str->_Mode & _MBYTE))
		str->_Mode |= _MWIDE;
	ans = (str->_Mode & _MBYTE) ? -1 : (str->_Mode & _MWIDE) ? 1 : 0;
	_Unlockfilelock(str);
	return ans;
}
