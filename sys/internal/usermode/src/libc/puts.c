/* puts function */
#include "xstdio.h"

int puts(const char *s)
{
	int ans;

	_Lockfilelock(stdout);
	ans = fputs(s, stdout) < 0 || fputc('\n', stdout) < 0 ? EOF : 0;
	_Unlockfilelock(stdout);
	return ans;
}
