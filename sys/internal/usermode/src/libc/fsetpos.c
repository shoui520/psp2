/* fsetpos function */
#include "xstdio.h"

int fsetpos(FILE *str, const fpos_t *p)
{
	int ans;

	_Lockfilelock(str);
	ans = _Fspos(str, p, 0, SEEK_SET);
	_Unlockfilelock(str);
	return ans;
}
