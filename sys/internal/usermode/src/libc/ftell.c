/* ftell function */
#include "xstdio.h"

long ftell(FILE *str)
{
	long ans;

	_Lockfilelock(str);
	ans = _Fgpos(str, NULL);
	_Unlockfilelock(str);
	return ans;
}
