/* fgetpos function */
#include "xstdio.h"

int fgetpos(FILE *__restrict str, fpos_t *__restrict p)
{
	int ans;

	_Lockfilelock(str);
	ans = _Fgpos(str, p);
	_Unlockfilelock(str);
	return ans;
}
