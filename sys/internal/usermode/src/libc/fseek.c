/* fseek function */
#include "xstdio.h"

int fseek(FILE *str, long off, int smode)
{
	int ans;

	_Lockfilelock(str);
	ans = _Fspos(str, NULL, off, smode);
	_Unlockfilelock(str);
	return ans;
}
